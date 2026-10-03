//
// YaPB test host: unit/control_{cmds,graph,debug}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for control.cpp through the real entries
// (handleEngineCommands, collectArgs + executeCommands/executeMenus):
// dispatcher legs, bot management commands, graph editing commands,
// debug commands, menus and admin rights. Randomness is pinned via
// exact modes and extreme chances, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

// eight-node flagged chain: T-only, CT-only and goal keep checkNodes happy
void BuildControlGraph () {
  graph.Reset ();

  for (int i = 0; i < 8; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.paths_[1].flags |= NodeFlag::TerroristOnly;
  graph.paths_[2].flags |= NodeFlag::CTOnly;
  graph.paths_[3].flags |= NodeFlag::Goal;

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 7; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();
}

void BootControl (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  graph.SetMessageSilence (false); // autostart silences graph chatter, suspend keeps it
  BuildControlGraph ();

  bots.InitQuota ();
  cv_quota.Set (10);
  ctrl.SetDenyCommands (false);
  ctrl.SetIssuer (nullptr);
}

void CleanupControlFiles () {
  // drop leftovers from aborted runs, every leg below is hermetic either way
  ystl::plat.remove_file (bstor.BuildPath (StorageFile::Graph).chars ());
  ystl::plat.remove_file (bstor.BuildPath (StorageFile::Graph, true).chars ());

  const ystl::String text_fs = ystl::strings.join_path (bstor.GetRunningPath (), folders.data, folders.graph,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
  const ystl::String text_vfs = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.data, folders.graph,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
  ystl::plat.remove_file (text_fs.chars ());
  ystl::plat.remove_file (text_vfs.chars ());
}

void MirrorControlFile (const ystl::String &from, const ystl::String &to) {
  ystl::File::make_path (ystl::String (from.substr (0, from.find_last_of (kPathSeparator))).chars ());
  ystl::File::make_path (ystl::String (to.substr (0, to.find_last_of (kPathSeparator))).chars ());

  ystl::File src (from, "rb");
  HOST_REQUIRE (!!src);
  ystl::File dst (to, "wb");
  HOST_REQUIRE (!!dst);

  for (int ch = src.get (); ch != EOF; ch = src.get ()) {
    dst.put_char (ch);
  }
}

void RunCmd (testhost::FakeEngine &engine, std::initializer_list<const char *> words) {
  ystl::Array<ystl::String> args {};

  for (const auto word : words) {
    args.push (word);
  }
  engine.SetCmdArgs (args);
  ctrl.SetDenyCommands (false);
  ctrl.HandleEngineCommands ();
}

// issuer-preserving variant: handleEngineCommands always re-issues as
// console, this one drives collect + execute directly instead
void RunCmdAs (testhost::FakeEngine &engine, edict_t *issuer, std::initializer_list<const char *> words) {
  ystl::Array<ystl::String> args {};

  for (const auto word : words) {
    args.push (word);
  }
  engine.SetCmdArgs (args);
  ctrl.CollectArgs ();
  ctrl.SetDenyCommands (false);
  ctrl.SetIssuer (issuer);
  ctrl.ExecuteCommands ();
}

void DrainPrints (testhost::FakeEngine &engine, int rounds = 30) {
  for (int i = 0; i < rounds; ++i) {
    engine.AdvanceTime (0.06f);
    ctrl.FlushPrintQueue ();
  }
}

int PrintMark (testhost::FakeEngine &engine) {
  return engine.Calls ().size ();
}

bool PrintedSince (testhost::FakeEngine &engine, int mark, const char *needle) {
  for (int i = mark; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), needle) != nullptr) {
      return true;
    }
  }
  return false;
}

void PrepControlBot (Bot *bot, const ystl::Vector &pos) {
  bot->pev->origin = pos;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  bot->pev->takedamage = DAMAGE_YES;
  bot->pev->solid = SOLID_BBOX;
  bot->pev->movetype = MOVETYPE_WALK;
  bot->is_alive_ = true;
  bot->team_ = Team::CT;
}

} // namespace

TEST_CASE ("unit/control_cmds") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);

  // foreign prefixes never reach us...
  {
    ystl::Array<ystl::String> args {};
    args.push ("say");
    args.push ("hi");
    engine.SetCmdArgs (args);
    ctrl.SetDenyCommands (false);
    ctrl.HandleEngineCommands (); // foreign prefix, silently ignored
  }

  // ...bare prefix lists, help pages, unknown names...
  int mark = PrintMark (engine);
  RunCmd (engine, { "yb" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "valid commands"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Command:"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help", "kick" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "kick"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help", "nosuchcmd" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "No help found"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "frobnicate" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Unknown command"));

  // ...unprivileged clients are turned away...
  edict_t *stranger = game.CreateFakeClient ("Stranger");
  HOST_REQUIRE (!game.IsNullEntity (stranger));

  // handleEngineCommands always re-issues as console, drive the gate directly...
  {
    ystl::Array<ystl::String> args {};
    args.push ("yb");
    args.push ("version");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (stranger);
    CHECK (ctrl.ExecuteCommands ());
    CHECK (ctrl.GetIssuer () == nullptr); // gate resets the issuer, version never runs
  }
  mark = PrintMark (engine);
  DrainPrints (engine);
  CHECK (!PrintedSince (engine, mark, "YaPB v"));
  ctrl.SetIssuer (nullptr);

  // ...version and list always answer...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "version" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "YaPB v"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yapb", "list" });
  DrainPrints (engine);

  // ...bad formats are reported, not executed...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "fill" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "vote" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "weaponmode" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "weaponmode", "railgun" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "fun" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "fun", "nosuchmode" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  // ...bots join through the command and answer roll calls...
  RunCmd (engine, { "yb", "addbot" });
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  CHECK (bots.GetBotCount () == 1);

  RunCmd (engine, { "yb", "fill", "1" });
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "vote", "3" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "vote for map #3"));

  bool all_voted = true;

  bots.ForEach ([&] (Bot *bot) {
    all_voted = all_voted && bot->vote_map_ == 3;
    return false;
  });
  CHECK (all_voted);

  // ...fun and weapon modes apply...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "fun", "tronisback" });
  DrainPrints (engine);
  CHECK (cv_fun_mode.As<ystl::StringRef> () == "tronisback"); // applied on the next frame

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "weaponmode", "knife" });
  DrainPrints (engine);
  CHECK (!PrintedSince (engine, mark, "Incorrect usage"));

  // ...kicks shrink the roster, kills drop everyone...
  bots.ForEach ([&] (Bot *bot) {
    PrepControlBot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
    return false;
  });
  cv_quota.Set (2); // pin quota so the kicked bot is not re-added
  const int sc_base = engine.ServerCommands ().size ();
  RunCmd (engine, { "yb", "kick" });
  DrainPrints (engine);

  // harness never processes the disconnect, the contract is the
  // issued kick plus the decremented quota...
  bool kick_issued = false;

  for (int i = sc_base; i < engine.ServerCommands ().size (); ++i) {
    kick_issued = kick_issued || ystl::StringRef (engine.ServerCommands ()[i]).contains ("kick");
  }
  CHECK (kick_issued);
  CHECK (cv_quota.As<int> () == 1);

  RunCmd (engine, { "yb", "kickbots", "instant" });
  DrainPrints (engine);
  CHECK (cv_quota.As<int> () == 0);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "killbots" });
  DrainPrints (engine);

  // killing runs through the damage chain (async), the order is the contract...
  CHECK (PrintedSince (engine, mark, "All bots died"));

  // ...cvars list, filter, revert and persist...
  cv_quota.Set (3);
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "cvars", "quota" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "quota"));

  RunCmd (engine, { "yb", "cvars", "defaults" });
  DrainPrints (engine);
  CHECK (cv_quota.As<int> () == 9);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "cvars", "save" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "written to file"));

  {
    const ystl::String cfg_path =
      ystl::strings.join_path (bstor.GetRunningPath (), folders.config, ystl::strings.format ("%s.%s", product.name_lower, kConfigExtension));
    ystl::MemFile probe (cfg_path);
    CHECK (!!probe);
    probe.close ();
    ystl::plat.remove_file (cfg_path.chars ());
  }

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "show_custom" });
  DrainPrints (engine);

  bots.Destroy ();
}

TEST_CASE ("unit/control_graph") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);
  CleanupControlFiles ();

  // graph help via the full name (must not be shadowed by the earlier
  // "graphmenu" entry) and via the "g" alias...
  int mark = PrintMark (engine);
  RunCmd (engine, { "yb", "graph", "help" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "save"));
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "help" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "save"));

  // dedicated servers without an editor only allow the safe subset...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Unable to use graph edit"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "frobnicate" });
  DrainPrints (engine);

  // ...health checks and stats read the live graph...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "check" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Graph seems to be OK"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "stats" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Nodes:"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "fileinfo" });
  DrainPrints (engine);

  // ...editor rights need a live player, console is refused...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "acquire_editor" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "HLDS console"));

  edict_t *editor = game.CreateFakeClient ("Editor");
  HOST_REQUIRE (!game.IsNullEntity (editor));
  clients.Update ();
  clients[editor].flags |= ClientFlags::Admin;
  RunCmdAs (engine, editor, { "yb", "g", "acquire_editor" });
  DrainPrints (engine);
  CHECK (graph.HasEditor ());
  CHECK (graph.GetEditor () == editor);

  // ...with an editor aboard, console drives the rest...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Graph Status"));

  // ...radius and cache pin nodes...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "setradius" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  RunCmd (engine, { "yb", "g", "setradius", "48", "2" });
  DrainPrints (engine);
  CHECK (graph.paths_[2].radius == 48.0f);

  RunCmd (engine, { "yb", "g", "cache", "3" });
  DrainPrints (engine);

  // ...editor display modes store and restore game cvars...
  const float roundtime_before = mp_roundtime.As<float> ();
  RunCmd (engine, { "yb", "g", "on" });
  DrainPrints (engine);
  CHECK (graph.HasEditFlag (GraphEdit::On));
  CHECK (mp_roundtime.As<float> () == 0.0f);

  RunCmd (engine, { "yb", "g", "off" });
  DrainPrints (engine);
  CHECK (!graph.HasEditFlag (GraphEdit::On));
  CHECK (mp_roundtime.As<float> () == roundtime_before);

  RunCmdAs (engine, editor, { "yb", "g", "on", "noclip" });
  DrainPrints (engine);
  CHECK (editor->v.movetype == MOVETYPE_NOCLIP);

  RunCmdAs (engine, editor, { "yb", "g", "off", "noclip" });
  DrainPrints (engine);
  CHECK (editor->v.movetype == MOVETYPE_WALK);

  // ...teleport validates before moving the editor...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "teleport" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "teleport", "99" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Could not teleport"));

  RunCmdAs (engine, editor, { "yb", "g", "teleport", "5" });
  DrainPrints (engine);
  CHECK (editor->v.origin == graph[5].origin);

  // ...height shifts, saves, exports, deletes and imports round-trip...
  RunCmd (engine, { "yb", "g", "adjust_height", "10" });
  DrainPrints (engine);
  CHECK (graph[4].origin.z == 10.0f);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "save" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "written to disk"));

  {
    ystl::MemFile probe (bstor.BuildPath (StorageFile::Graph));
    CHECK (!!probe);
    probe.close ();
  }

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "export" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Graph text exported"));

  RunCmd (engine, { "yb", "g", "delete", "7" });
  DrainPrints (engine);
  CHECK (graph.Length () == 7);

  {
    const ystl::String text_fs = ystl::strings.join_path (bstor.GetRunningPath (), folders.data, folders.graph,
      ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
    const ystl::String text_vfs = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.data, folders.graph,
      ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
    MirrorControlFile (text_fs, text_vfs);
  }

  RunCmd (engine, { "yb", "g", "import" });
  DrainPrints (engine);
  CHECK (graph.Length () == 8);
  CHECK (graph[4].origin.z == 10.0f);

  MirrorControlFile (bstor.BuildPath (StorageFile::Graph), bstor.BuildPath (StorageFile::Graph, true));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "load" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "successfully loaded"));

  // ...cleaning runs before erase (erase wipes the live graph too)...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "clean", "all" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Done. Processed 8 nodes."));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "clean", "2" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Processed node 2"));

  // ...erase nags first, then wipes...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "erase" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "iamsure"));

  RunCmd (engine, { "yb", "g", "erase", "iamsure" });
  DrainPrints (engine);

  {
    ystl::plat.remove_file (bstor.BuildPath (StorageFile::Graph, true).chars ());
    ystl::MemFile probe (bstor.BuildPath (StorageFile::Graph));
    CHECK (!probe);
  }

  // ...offline refresh degrades to a message, menus open for editors...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "refresh" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "append"));

  RunCmd (engine, { "yb", "g", "addbasic" });
  DrainPrints (engine);

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  DrainPrints (engine);
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage1);

  // ...release frees, then the gate takes over again...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "release_editor" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "freed"));
  CHECK (!graph.HasEditor ());

  CleanupControlFiles ();
  bots.Destroy ();
}

TEST_CASE ("unit/control_debug") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);

  // secureCompare is exact and length-checked...
  CHECK (ctrl.SecureCompare ("secret", "secret"));
  CHECK (!ctrl.SecureCompare ("secret", "other!"));
  CHECK (!ctrl.SecureCompare ("short", "longer"));

  // translate starts empty and resets cleanly...
  int mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "translate" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "No missing translations"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "translate", "reset" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "cleared"));

  // ...targets resolve by name, indices and miss cleanly...
  edict_t *one = game.CreateFakeClient ("DbgOne");
  edict_t *two = game.CreateFakeClient ("DbgTwo");
  HOST_REQUIRE (!game.IsNullEntity (one) && !game.IsNullEntity (two));

  for (auto target : { one, two }) {
    target->v.health = 100.0f;
    target->v.deadflag = DEAD_NO;
    target->v.takedamage = DAMAGE_YES;
    target->v.solid = SOLID_BBOX;
    target->v.movetype = MOVETYPE_WALK;
  }
  // ...admin rights are inert without a grant...
  ctrl.MaintainAdminRights ();
  ctrl.AssignAdminRights (one, nullptr);
  CHECK (!has_flag (clients[one].flags, ClientFlags::Admin));
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "nosuchplayer" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "not found"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "Dbg" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Multiple players"));

  const int kill_base = testhost::CsCalls (cs, "ClientKill");
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "DbgOne" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Slaying player"));
  CHECK (testhost::CsCalls (cs, "ClientKill") == kill_base + 1); // stub only records

  one->v.health = 0.0f; // fake_cs cannot kill, help it along
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "DbgOne" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "not alive"));

  // ...slaps push up, lethal slaps kill...
  const float vel_before = two->v.velocity.z;
  RunCmd (engine, { "yb", "debug", "slap", "DbgTwo" });
  DrainPrints (engine);
  CHECK (two->v.velocity.z == vel_before + 256.0f);

  RunCmd (engine, { "yb", "debug", "slap", "DbgTwo", "1000" });
  DrainPrints (engine);
  CHECK (!game.IsAliveEntity (two));

  // ...god and notarget toggle twice...
  one->v.health = 100.0f;
  one->v.deadflag = DEAD_NO;

  RunCmd (engine, { "yb", "debug", "god", "DbgOne" });
  DrainPrints (engine);
  CHECK (!!(one->v.flags & FL_GODMODE));

  RunCmd (engine, { "yb", "debug", "god", "DbgOne" });
  DrainPrints (engine);
  CHECK (!(one->v.flags & FL_GODMODE));

  RunCmd (engine, { "yb", "debug", "notarget", "DbgOne" });
  DrainPrints (engine);
  CHECK (!!(one->v.flags & FL_NOTARGET));

  RunCmd (engine, { "yb", "debug", "notarget", "DbgOne" });
  DrainPrints (engine);
  CHECK (!(one->v.flags & FL_NOTARGET));

  // ...exec reaches bots and raw clients, memory always answers...
  // (menus below need the grant, take it before they run)
  clients.Update ();
  clients[one].flags |= ClientFlags::Admin;
  RunCmd (engine, { "yb", "addbot" });
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = nullptr;

  bots.ForEach ([&] (Bot *candidate) {
    bot = candidate;
    return true;
  });
  HOST_REQUIRE (bot != nullptr);

  const ystl::String bot_name = bot->pev->netname.chars ();
  const int cs_base = cs.call_count ();
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "exec", bot_name.chars (), "say hi" });
  DrainPrints (engine);
  CHECK (cs.call_count () > cs_base);
  CHECK (PrintedSince (engine, mark, "Executed command"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "exec" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "memory" });
  DrainPrints (engine);

  // ...god without a target needs the listenserver host...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "god" });
  DrainPrints (engine);

  // ...menus open, select and close through the client channel...
  RunCmdAs (engine, one, { "yb", "menu" });
  DrainPrints (engine);
  CHECK (clients[one].MenuId == MenuId::Main);

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("1");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (one);
    CHECK (ctrl.ExecuteMenus ());
    DrainPrints (engine);
    CHECK (clients[one].MenuId == MenuId::Control);
  }

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("10");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (one);
    CHECK (ctrl.ExecuteMenus ());
    DrainPrints (engine);
    CHECK (clients[one].MenuId == MenuId::None);
  }

  // ...features menu flips debug, control menu manages the roster...
  RunCmdAs (engine, one, { "yb", "menu" });
  DrainPrints (engine);

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("2");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (one);
    CHECK (ctrl.ExecuteMenus ());
    DrainPrints (engine);
    CHECK (clients[one].MenuId == MenuId::Features);
  }
  const int debug_before = cv_debug.As<int> ();

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("4");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (one);
    CHECK (ctrl.ExecuteMenus ());
    DrainPrints (engine);
    CHECK (cv_debug.As<int> () == (debug_before ^ 1));
    CHECK (clients[one].MenuId == MenuId::Features);
  }

  // ...stale menus and foreign input never dispatch...
  clients[one].MenuId = MenuId::None;

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("1");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (one);
    CHECK (!ctrl.ExecuteMenus ());
  }

  clients[one].flags |= ClientFlags::Admin;
  mark = PrintMark (engine);
  RunCmdAs (engine, one, { "yb", "version" });
  DrainPrints (engine);
  CHECK (ctrl.GetIssuer () == one); // gate passed, issuer kept (prints go client-side)

  bots.Destroy ();
}

} // namespace bot
