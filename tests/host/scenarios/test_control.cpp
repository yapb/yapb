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

size_t PrintMark (testhost::FakeEngine &engine) {
  return engine.Calls ().size ();
}

bool PrintedSince (testhost::FakeEngine &engine, size_t mark, const char *needle) {
  for (size_t i = mark; i < engine.Calls ().size (); ++i) {
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
  size_t mark = PrintMark (engine);
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
  const size_t sc_base = engine.ServerCommands ().size ();
  RunCmd (engine, { "yb", "kick" });
  DrainPrints (engine);

  // harness never processes the disconnect, the contract is the
  // issued kick plus the decremented quota...
  bool kick_issued = false;

  for (size_t i = sc_base; i < engine.ServerCommands ().size (); ++i) {
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
  size_t mark = PrintMark (engine);
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
  size_t mark = PrintMark (engine);
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

  // digit-leading names must resolve by name, not as player indexes
  bot->pev->netname = string_t (engine.AllocString ("4TestBot"));

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

  // ...http diagnostics always answer with resolver/tls/cacert/url lines...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "http" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "resolver:"));
  CHECK (PrintedSince (engine, mark, "tls:"));
  CHECK (PrintedSince (engine, mark, "cacert:"));
  CHECK (PrintedSince (engine, mark, "download:"));
  CHECK (PrintedSince (engine, mark, "upload:"));
  CHECK (PrintedSince (engine, mark, "last http status:"));

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

TEST_CASE ("unit/control_botops") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);

  const int quota_before = cv_quota.As<int> ();
  const ystl::String fun_before = cv_fun_mode.As<ystl::StringRef> ();
  const ystl::String pw_before = cv_password.As<ystl::StringRef> ();
  const ystl::String pwkey_before = cv_password_key.As<ystl::StringRef> ();

  // ...upper-case prefix still resolves, first two args fold case...
  size_t mark = PrintMark (engine);
  RunCmd (engine, { "YB", "VERSION" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "YaPB v"));

  // ...help legs: single-name entry skips the alias line, multi-name lists it...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help", "cvars" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Command:"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help", "weapons" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Aliases:"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "help", "nosuchcmd" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "No help found"));

  // ...node dispatcher help: known subcommand prints, unknown lists all...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "help", "slay" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Command:"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "help", "nosuchsub" });
  DrainPrints (engine);

  // ...addbot alias legs: _ct pins the team, addhs maxes the skill...
  RunCmd (engine, { "yb", "add_ct" });
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  RunCmd (engine, { "yb", "add_t" });
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  RunCmd (engine, { "yb", "addhs" });
  HOST_REQUIRE (testhost::PumpBots (engine, 3));
  CHECK (bots.GetBotCount () == 3);

  // ...pin teams by hand for the team-scoped legs below...
  clients.Update ();

  {
    int seen = 0;

    bots.ForEach ([&] (Bot *bot) {
      const Team want = (seen == 0) ? Team::CT : Team::Terrorist;

      clients[bot->Ent ()].team = want;
      clients[bot->Ent ()].team2 = want;
      ++seen;
      return false;
    });
    CHECK (seen == 3);
  }

  // ...kick legs: team aliases, numeric and name teams, then the plain one...
  const size_t sc_base = engine.ServerCommands ().size ();
  RunCmd (engine, { "yb", "kick_ct" });
  RunCmd (engine, { "yb", "kick_t" });
  RunCmd (engine, { "yb", "kick", "2" });
  RunCmd (engine, { "yb", "kick", "ct" });
  RunCmd (engine, { "yb", "kick", "1" });
  RunCmd (engine, { "yb", "kick", "t" });
  DrainPrints (engine);
  CHECK (bots.GetBotCount () == 3); // kicks are record-only, the roster stays

  cv_quota.Set (10);
  RunCmd (engine, { "yb", "kick" });
  DrainPrints (engine);
  CHECK (cv_quota.As<int> () == 9);

  bool kick_seen = false;

  for (size_t i = sc_base; i < engine.ServerCommands ().size (); ++i) {
    kick_seen = kick_seen || ystl::StringRef (engine.ServerCommands ()[i]).contains ("kick");
  }
  CHECK (kick_seen);

  // ...kickbots legs: instant wipes the quota, team aliases trim by team...
  RunCmd (engine, { "yb", "kickall_ct" });
  RunCmd (engine, { "yb", "kickall_t" });
  RunCmd (engine, { "yb", "removebots", "instant", "2" });
  DrainPrints (engine);

  cv_quota.Set (10);
  RunCmd (engine, { "yb", "removebots", "instant" });
  DrainPrints (engine);
  CHECK (cv_quota.As<int> () == 0);
  cv_quota.Set (10);

  // ...kill legs: loud team kills announce, silent numeric ones stay quiet...
  bots.ForEach ([&] (Bot *bot) {
    PrepControlBot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
    return false;
  });

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "kill_ct" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "All bots died"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "kill_t" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "All bots died"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "kill", "1", "silent" });
  RunCmd (engine, { "yb", "kill", "2", "si" });
  DrainPrints (engine);
  CHECK (!PrintedSince (engine, mark, "All bots died"));

  // ...fill with every arg covers the full-arg leg...
  RunCmd (engine, { "yb", "fill", "1", "1", "2", "1" });
  HOST_REQUIRE (testhost::PumpBots (engine, 4));

  // ...every weapon mode applies without a format complaint...
  static constexpr const char *kModes[7] = { "knife", "pistol", "shotgun", "smg", "rifle", "sniper", "standard" };

  for (int i = 0; i < 7; ++i) {
    mark = PrintMark (engine);
    RunCmd (engine, { "yb", "weaponmode", kModes[i] });
    DrainPrints (engine);
    CHECK (!PrintedSince (engine, mark, "Incorrect usage"));
  }

  // ...every fun mode applies, spot-check the last keyword...
  static constexpr const char *kFun[8] = { "imsober", "off", "tronisback", "itsnewyear", "imhaunted", "itstoodark", "stonedagain", "imonmars" };

  for (int i = 0; i < 8; ++i) {
    mark = PrintMark (engine);
    RunCmd (engine, { "yb", "fun", kFun[i] });
    DrainPrints (engine);
    CHECK (!PrintedSince (engine, mark, "Incorrect usage"));
  }
  CHECK (cv_fun_mode.As<ystl::StringRef> () == "imonmars");

  // ...menu cmd variant follows liveness: alive opens commands, dead falls back...
  edict_t *cmder = game.CreateFakeClient ("Cmder");
  HOST_REQUIRE (!game.IsNullEntity (cmder));
  clients.Update ();
  clients[cmder].flags |= ClientFlags::Admin;
  cmder->v.health = 100.0f;
  cmder->v.deadflag = DEAD_NO;
  cmder->v.takedamage = DAMAGE_YES;
  cmder->v.solid = SOLID_BBOX;
  cmder->v.movetype = MOVETYPE_WALK;

  RunCmdAs (engine, cmder, { "yb", "menu", "cmd" });
  CHECK (clients[cmder].MenuId == MenuId::Commands);

  cmder->v.health = 0.0f;
  RunCmdAs (engine, cmder, { "yb", "menu", "cmd" });
  CHECK (clients[cmder].MenuId == MenuId::Main);
  cmder->v.health = 100.0f;

  // ...node menu needs an editor: refused first, opens once granted...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "graphmenu" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "without setting the editor"));

  RunCmdAs (engine, cmder, { "yb", "g", "acquire_editor" });
  CHECK (graph.HasEditor ());
  RunCmdAs (engine, cmder, { "yb", "graphmenu" });
  CHECK (clients[cmder].MenuId == MenuId::NodeMainPage1);

  // ...rapid client output queues through the client channel...
  RunCmdAs (engine, cmder, { "yb", "cvars", "quota" });
  DrainPrints (engine);

  // ...cvars legs: bare lists all, nomatch lists none, save_map persists...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "cvars" });
  DrainPrints (engine);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "cvars", "zzz_nomatch" });
  DrainPrints (engine);

  {
    const ystl::String maps_dir = ystl::strings.join_path (bstor.GetRunningPath (), folders.config, "maps");
    ystl::File::make_path (maps_dir.chars ());
  }

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "cvars", "save_map" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "written to file"));

  {
    const ystl::String map_cfg = ystl::strings.join_path (
      bstor.GetRunningPath (), folders.config, "maps", ystl::strings.format ("%s.%s", game.GetMapName (), kConfigExtension));
    ystl::MemFile probe (map_cfg);
    CHECK (!!probe);
    probe.close ();
    ystl::plat.remove_file (map_cfg.chars ());
  }

  // ...resolve legs: host is listenserver-only, indexes hit slots, names hit...
  edict_t *slayee = game.CreateFakeClient ("Slayee");
  HOST_REQUIRE (!game.IsNullEntity (slayee));
  slayee->v.health = 100.0f;
  slayee->v.deadflag = DEAD_NO;
  slayee->v.takedamage = DAMAGE_YES;
  slayee->v.solid = SOLID_BBOX;
  slayee->v.movetype = MOVETYPE_WALK;
  clients.Update ();

  const int kills_before = testhost::CsCalls (cs, "ClientKill");
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "host" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "listenserver"));
  CHECK (testhost::CsCalls (cs, "ClientKill") == kills_before);

  RunCmd (engine, { "yb", "debug", "slay", "1" });
  DrainPrints (engine);
  CHECK (testhost::CsCalls (cs, "ClientKill") == kills_before + 1);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slay", "99" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "not found"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "notarget" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slap" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slap", "nosuchplayer" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "not found"));

  const float slap_before = slayee->v.velocity.z;
  RunCmd (engine, { "yb", "debug", "slap", "Slayee" });
  DrainPrints (engine);
  CHECK (slayee->v.velocity.z == slap_before + 256.0f);

  RunCmd (engine, { "yb", "debug", "slap", "Slayee", "5000" });
  DrainPrints (engine);
  CHECK (!game.IsAliveEntity (slayee));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "slap", "Slayee" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "not alive"));

  // ...exec legs: raw clients go direct, misses stay handled, write is empty...
  const size_t cc_base = engine.ClientCommands ().size ();
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "exec", "Slayee", "say hi" });
  DrainPrints (engine);
  CHECK (engine.ClientCommands ().size () > cc_base);
  CHECK (PrintedSince (engine, mark, "Executed command"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "exec", "nosuchplayer", "say hi" });
  DrainPrints (engine);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "translate", "write" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "No missing translations"));

  // ...client wrappers: denied and bot issuers never dispatch...
  ctrl.SetDenyCommands (true);
  CHECK (!ctrl.HandleClientCommands (cmder));
  CHECK (!ctrl.HandleMenuCommands (cmder));

  {
    ystl::Array<ystl::String> args {};
    args.push ("yb");
    args.push ("version");
    engine.SetCmdArgs (args);
    ctrl.HandleEngineCommands (); // denied at the door, silently ignored
  }
  ctrl.SetDenyCommands (false);

  edict_t *human = game.CreateFakeClient ("Human");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  clients.Update ();
  clients[human].flags |= ClientFlags::Admin;

  {
    ystl::Array<ystl::String> args {};
    args.push ("yb");
    args.push ("version");
    engine.SetCmdArgs (args);
    CHECK (ctrl.HandleClientCommands (human));
  }

  {
    ystl::Array<ystl::String> args {}; // empty input never dispatches
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (human);
    CHECK (!ctrl.ExecuteCommands ());
    ctrl.SetIssuer (nullptr);
  }

  {
    ystl::Array<ystl::String> args {}; // no issuer never opens menus
    args.push ("menuselect");
    args.push ("1");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (nullptr);
    CHECK (!ctrl.ExecuteMenus ());
  }

  Bot *any_bot = nullptr;

  bots.ForEach ([&] (Bot *bot) {
    any_bot = bot;
    return true;
  });
  HOST_REQUIRE (any_bot != nullptr);
  CHECK (!ctrl.HandleClientCommands (any_bot->Ent ()));
  CHECK (!ctrl.HandleMenuCommands (any_bot->Ent ()));

  {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push ("1");
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (any_bot->Ent ());
    CHECK (!ctrl.ExecuteMenus ());
    ctrl.SetIssuer (nullptr);
  }

  // ...admin rights last: gain with the key, loss without, buffers follow...
  cv_password_key.Set ("_ybpw");
  cv_password.Set ("secret");
  engine.SetInfoKey ("_ybpw", "secret");
  ctrl.MaintainAdminRights ();
  CHECK (has_flag (clients[human].flags, ClientFlags::Admin));

  engine.SetInfoKey ("_ybpw", "wrong");
  ctrl.MaintainAdminRights ();
  CHECK (!has_flag (clients[human].flags, ClientFlags::Admin));

  engine.SetInfoKey ("_ybpw", "secret"); // gain path announces itself
  mark = PrintMark (engine);
  ctrl.MaintainAdminRights ();
  DrainPrints (engine);
  CHECK (has_flag (clients[human].flags, ClientFlags::Admin));
  CHECK (PrintedSince (engine, mark, "gained full remote access"));

  cv_password.Set (""); // empty key drops everyone quietly
  ctrl.MaintainAdminRights ();
  CHECK (!has_flag (clients[human].flags, ClientFlags::Admin));
  cv_password.Set ("secret");

  {
    char buf[1024] = {};
    char key[16] = "_ybpw";
    char val[16] = "secret";
    char bad[16] = "nope";

    engine.SetInfoKey ("_ybpw", "secret");
    engfuncs.pfnSetKeyValue (buf, key, val);
    ctrl.AssignAdminRights (human, buf);
    CHECK (has_flag (clients[human].flags, ClientFlags::Admin));

    engine.SetInfoKey ("_ybpw", "nope");
    engfuncs.pfnSetKeyValue (buf, key, bad);
    ctrl.AssignAdminRights (human, buf);
    CHECK (!has_flag (clients[human].flags, ClientFlags::Admin));

    cv_password.Set ("");
    ctrl.AssignAdminRights (human, buf); // empty password, silent no-op
    cv_password.Set ("secret");

    ctrl.AssignAdminRights (cmder, buf); // fake issuers never gain rights
  }

  // ...listen flavor stays dedicated here (game caches it), god still needs a target...
  engine.SetDedicatedServer (false);
  ctrl.MaintainAdminRights ();
  ctrl.AssignAdminRights (human, nullptr);

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "debug", "god" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));
  engine.SetDedicatedServer (true);

  cv_quota.Set (quota_before);
  cv_fun_mode.Set (fun_before.chars ());
  cv_password.Set (pw_before.chars ());
  cv_password_key.Set (pwkey_before.chars ());
  ctrl.SetDenyCommands (false);
  ctrl.SetIssuer (nullptr);
  bots.Destroy ();
}

TEST_CASE ("unit/control_nodeops") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);
  CleanupControlFiles ();

  HOST_REQUIRE (engine.SpawnEntity ("info_player_start") != nullptr);
  HOST_REQUIRE (engine.SpawnEntity ("info_player_deathmatch") != nullptr);
  HOST_REQUIRE (engine.SpawnEntity ("info_vip_start") != nullptr);

  const int quota_before = cv_quota.As<int> ();
  const float roundtime_before = mp_roundtime.As<float> ();
  const float freezetime_before = mp_freezetime.As<float> ();
  const float timelimit_before = mp_timelimit.As<float> ();

  edict_t *editor = game.CreateFakeClient ("NodeEditor");
  HOST_REQUIRE (!game.IsNullEntity (editor));
  clients.Update ();
  clients[editor].flags |= ClientFlags::Admin;

  edict_t *rival = game.CreateFakeClient ("NodeRival");
  HOST_REQUIRE (!game.IsNullEntity (rival));
  clients.Update ();
  clients[rival].flags |= ClientFlags::Admin;

  // ...second editor loses the race while rights are held...
  RunCmdAs (engine, editor, { "yb", "g", "acquire_editor" });
  CHECK (graph.HasEditor ());
  CHECK (graph.GetEditor () == editor);
  RunCmdAs (engine, rival, { "yb", "g", "acquire_editor" });
  CHECK (graph.GetEditor () == editor);

  // ...save legs run first on the pristine graph, then the matrix...
  RunCmd (engine, { "yb", "g", "on" });
  CHECK (graph.HasEditFlag (GraphEdit::On));
  CHECK (mp_roundtime.As<float> () == 0.0f);

  size_t mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "save" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "written to disk"));

  MirrorControlFile (bstor.BuildPath (StorageFile::Graph), bstor.BuildPath (StorageFile::Graph, true));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "load" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "successfully loaded"));

  RunCmd (engine, { "yb", "g", "save", "nocheck" });
  RunCmd (engine, { "yb", "g", "save", "old" });
  RunCmd (engine, { "yb", "g", "save", "oldformat" });

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "apply" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Graph applied"));

  // ...on/off matrix: one restore pair, then every flag branch...
  RunCmd (engine, { "yb", "g", "off" });
  CHECK (!graph.HasEditFlag (GraphEdit::On));
  CHECK (mp_roundtime.As<float> () == roundtime_before);
  CHECK (mp_freezetime.As<float> () == freezetime_before);
  CHECK (mp_timelimit.As<float> () == timelimit_before);

  RunCmd (engine, { "yb", "g", "on", "display" });
  CHECK (graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on", "models" });
  CHECK (graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "off" });
  CHECK (!graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on", "noclip" }); // cold: enables both at once
  CHECK (graph.HasEditFlag (GraphEdit::On));
  CHECK (graph.HasEditFlag (GraphEdit::Noclip));

  RunCmd (engine, { "yb", "g", "off", "noclip" });
  CHECK (!graph.HasEditFlag (GraphEdit::Noclip));
  CHECK (graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on", "noclip" }); // warm: editor flag already on
  CHECK (graph.HasEditFlag (GraphEdit::Noclip));

  RunCmd (engine, { "yb", "g", "off" });
  CHECK (!graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on", "auto" }); // cold: enables both at once
  CHECK (graph.HasEditFlag (GraphEdit::On));
  CHECK (graph.HasEditFlag (GraphEdit::Auto));

  RunCmd (engine, { "yb", "g", "off" });
  CHECK (!graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on" });
  RunCmd (engine, { "yb", "g", "on", "auto" }); // warm: only the auto flag
  CHECK (graph.HasEditFlag (GraphEdit::Auto));

  RunCmd (engine, { "yb", "g", "off", "auto" });
  CHECK (!graph.HasEditFlag (GraphEdit::Auto));

  RunCmd (engine, { "yb", "g", "off", "models" });

  RunCmd (engine, { "yb", "g", "off" });
  CHECK (!graph.HasEditFlag (GraphEdit::On));

  RunCmd (engine, { "yb", "g", "on" }); // back on for the editing legs

  // ...add, flags and autopath entries open their menus for the editor...
  RunCmdAs (engine, editor, { "yb", "g", "add" });
  CHECK (clients[editor].MenuId == MenuId::NodeType);

  RunCmdAs (engine, editor, { "yb", "g", "flags" });
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);

  RunCmdAs (engine, editor, { "yb", "g", "path_set_autopath" });
  CHECK (clients[editor].MenuId == MenuId::NodeAutoPath);

  RunCmd (engine, { "yb", "g", "path_create" }); // console has no menu target

  // ...park on node 0 and cache node 1 for the directed legs...
  RunCmd (engine, { "yb", "g", "on" });
  editor->v.origin = graph[0].origin;
  RunCmd (engine, { "yb", "g", "cache", "1" });

  RunCmd (engine, { "yb", "g", "path_create_out" });
  RunCmd (engine, { "yb", "g", "path_create_in" });
  RunCmd (engine, { "yb", "g", "path_create_both" });
  RunCmd (engine, { "yb", "g", "path_create_jump" });
  RunCmd (engine, { "yb", "g", "path_delete" });
  RunCmd (engine, { "yb", "g", "path_clean" });
  RunCmd (engine, { "yb", "g", "path_clean", "1" });

  // ...missing options fall back to the nearest node...
  RunCmd (engine, { "yb", "g", "cache" });
  RunCmd (engine, { "yb", "g", "setradius", "32" });
  CHECK (graph.paths_[0].radius == 32.0f);
  RunCmd (engine, { "yb", "g", "clean" });

  // ...bad indexes are reported, never applied...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "delete", "99" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Could not delete"));

  RunCmd (engine, { "yb", "g", "cache", "99" });
  RunCmd (engine, { "yb", "g", "clean", "99" });

  // ...no nearest and no destination legs need a cleared cache far away...
  editor->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  RunCmd (engine, { "yb", "g", "cache" }); // clears: nothing in 50 units

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "path_create_out" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Unable to find nearest"));

  editor->v.origin = graph[0].origin; // nearest back, cache still cleared
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "path_create_out" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Unable to find destination"));

  RunCmd (engine, { "yb", "g", "path_delete" }); // same missing destination leg

  // ...camp iteration walks begin, next, double-begin, drain and end...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "iterate_camp", "begin" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Unable to begin"));

  graph.paths_[0].flags |= NodeFlag::Camp;
  graph.paths_[0].flags |= NodeFlag::Crouch;
  graph.paths_[0].end = ystl::Vector (100.0f, 0.0f, 0.0f); // checks need a camp end

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "iterate_camp", "begin" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Ready for iteration"));

  RunCmd (engine, { "yb", "g", "iterate_camp", "begin" }); // already begun
  RunCmdAs (engine, editor, { "yb", "g", "iterate_camp", "next" }); // crouch adds 23 up
  CHECK (editor->v.origin.z == graph[0].origin.z + 23.0f);

  editor->v.origin = graph[0].origin;
  RunCmd (engine, { "yb", "g", "iterate_camp", "next" }); // drained after the single spot
  RunCmd (engine, { "yb", "g", "iterate_camp", "end" }); // empty needs a begin first

  RunCmd (engine, { "yb", "g", "iterate_camp", "begin" });
  RunCmd (engine, { "yb", "g", "iterate_camp", "end" }); // clears silently

  // ...height, delete and training legs run before the final wipe...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "adjust_height" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  RunCmd (engine, { "yb", "g", "adjust_height", "-5" });

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "delete", "7" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "has been deleted"));
  CHECK (graph.Length () == 7);

  RunCmd (engine, { "yb", "g", "erase_training" });

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "erase" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "iamsure"));

  RunCmd (engine, { "yb", "g", "erase", "iamsure" });

  {
    ystl::MemFile probe (bstor.BuildPath (StorageFile::Graph));
    CHECK (!probe);
  }

  // ...help legs: known subcommand prints, unknown lists everything...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "help", "save" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Command:"));

  RunCmd (engine, { "yb", "g", "help", "nosuchsub" });

  // ...offline refresh degrades to the append hint...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "refresh" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "append"));

  // ...bad iterate legs stay format errors (editor still held)...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "iterate_camp", "frobnicate" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "iterate_camp" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Incorrect usage"));

  // ...release frees, the second one finds nobody...
  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "release_editor" });
  DrainPrints (engine);
  CHECK (!graph.HasEditor ());
  CHECK (PrintedSince (engine, mark, "freed"));

  RunCmd (engine, { "yb", "g", "release_editor" });

  mp_roundtime.Set (roundtime_before);
  mp_freezetime.Set (freezetime_before);
  mp_timelimit.Set (timelimit_before);
  cv_quota.Set (quota_before);
  ctrl.SetIssuer (nullptr);
  CleanupControlFiles ();
  bots.Destroy ();
}

TEST_CASE ("unit/control_menuflow") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);

  const int quota_before = cv_quota.As<int> ();
  const int debug_before = cv_debug.As<int> ();
  const ystl::String fun_before = cv_fun_mode.As<ystl::StringRef> ();
  const float limitteams_before = mp_limitteams.As<float> ();
  const float autobal_before = mp_autoteambalance.As<float> ();

  auto pick = [&] (edict_t *who, const char *item) -> bool {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push (item);
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (who);
    return ctrl.ExecuteMenus ();
  };

  auto open = [&] (edict_t *who) {
    RunCmdAs (engine, who, { "yb", "menu" });
  };

  edict_t *one = game.CreateFakeClient ("MenuOne");
  HOST_REQUIRE (!game.IsNullEntity (one));

  edict_t *two = game.CreateFakeClient ("MenuTwo");
  HOST_REQUIRE (!game.IsNullEntity (two));

  clients.Update ();
  clients[one].flags |= ClientFlags::Admin;
  clients[two].flags |= ClientFlags::Admin;

  for (auto target : { one, two }) {
    target->v.health = 100.0f;
    target->v.deadflag = DEAD_NO;
    target->v.takedamage = DAMAGE_YES;
    target->v.solid = SOLID_BBOX;
    target->v.movetype = MOVETYPE_WALK;
  }

  RunCmd (engine, { "yb", "addbot" });
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  // ...main matrix: control, features, fill, kill, exit and default...
  open (one);
  CHECK (clients[one].MenuId == MenuId::Main);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Control);

  open (one);
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Features);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (clients[one].MenuId == MenuId::TeamSelect);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Difficulty);
  CHECK (pick (one, "3"));
  CHECK (clients[one].MenuId == MenuId::Personality);
  CHECK (pick (one, "2")); // full fill queued, menu closes
  CHECK (clients[one].MenuId == MenuId::None);

  // ...wait for a workable crowd, stop early once paged...
  for (int i = 0; i < 60 && bots.GetBotCount () < 8; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
  }
  HOST_REQUIRE (bots.GetBotCount () >= 8);

  // ...fill variants: teams, difficulties, personalities and exits...
  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Difficulty);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "5"));
  CHECK (pick (one, "5"));
  CHECK (pick (one, "4"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "2"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "4"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::Personality);
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "3"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "1"));
  CHECK (pick (one, "3"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...features weapon tour flips through every mode...
  open (one);
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Features);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::WeaponMode);

  for (const char *item : { "1", "2", "3", "4", "5", "6", "7" }) {
    CHECK (pick (one, item));
    CHECK (clients[one].MenuId == MenuId::WeaponMode);
  }
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...features graph tour: first call auto-acquires, foreign holder refuses...
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::NodeMainPage1);
  CHECK (graph.GetEditor () == one);

  graph.SetEditor (two);
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Features);
  graph.SetEditor (nullptr);

  // ...features personality tour needs fill off, control reopens it...
  open (one);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Control);

  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (clients[one].MenuId == MenuId::Personality);
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::TeamSelect);
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...features debug tour toggles twice...
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "4"));
  CHECK (cv_debug.As<int> () == (debug_before ^ 1));
  CHECK (clients[one].MenuId == MenuId::Features);
  CHECK (pick (one, "4"));
  CHECK (cv_debug.As<int> () == debug_before);
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...teamselect and classselect tours add single bots...
  open (one);
  CHECK (pick (one, "1"));
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::TeamSelect);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::TerroristSelect);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::CTSelect);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (pick (one, "5"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "3"));
  CHECK (pick (one, "2"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...commands tour needs a close same-team neighbor...
  clients.Update ();

  Bot *cmd_bot = nullptr;

  bots.ForEach ([&] (Bot *bot) {
    PrepControlBot (bot, ystl::Vector (0.0f, 0.0f, 0.0f));
    clients[bot->Ent ()].team = Team::CT;
    clients[bot->Ent ()].team2 = Team::CT;
    cmd_bot = bot;
    return false;
  });
  HOST_REQUIRE (cmd_bot != nullptr);
  clients.Update (); // resync Alive after the manual prep above

  clients[one].team = Team::CT;
  clients[one].team2 = Team::CT;
  one->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  RunCmdAs (engine, one, { "yb", "menu", "cmd" });
  CHECK (clients[one].MenuId == MenuId::Commands);

  cmd_bot->has_c4_ = true;
  CHECK (pick (one, "1")); // c4 carrier skips the double jump
  CHECK (clients[one].MenuId == MenuId::Commands);
  cmd_bot->has_c4_ = false;

  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Commands);
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Commands);
  CHECK (pick (one, "3"));
  CHECK (clients[one].MenuId == MenuId::Commands);
  CHECK (pick (one, "4"));
  CHECK (clients[one].MenuId == MenuId::Commands);

  one->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  CHECK (pick (one, "1")); // nobody in range, menu just reopens
  CHECK (clients[one].MenuId == MenuId::Commands);
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);
  one->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  // ...features dead tour is refused, then health is restored...
  one->v.health = 0.0f;
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "5"));
  CHECK (clients[one].MenuId == MenuId::None);
  one->v.health = 100.0f;

  // ...features fun tour flips every mode, default steps back...
  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "6"));
  CHECK (clients[one].MenuId == MenuId::FunModeId);

  for (const char *item : { "1", "2", "3", "4", "5", "6", "7" }) {
    CHECK (pick (one, item));
    CHECK (clients[one].MenuId == MenuId::FunModeId);
  }
  CHECK (pick (one, "9"));
  CHECK (clients[one].MenuId == MenuId::Features);

  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "6"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "2"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  // ...control matrix: add, difficulty, random kick, removal and exits...
  open (one);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Control);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Control);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "2"));
  CHECK (clients[one].MenuId == MenuId::Difficulty);

  // ...kick pager: two pages, clamp, shrink, walk and exit...
  ctrl.KickBotByMenu (99); // clamps to the last page
  CHECK (clients[one].MenuId == MenuId::Kick);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "5"));
  CHECK (clients[one].MenuId == MenuId::Kick);
  CHECK (pick (one, "8"));
  CHECK (clients[one].MenuId == MenuId::Kick);
  CHECK (pick (one, "9"));
  CHECK (clients[one].MenuId == MenuId::Kick);

  ctrl.kick_pages_[game.IndexOfEntity (one)] = 5; // stale page clamps on select
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Kick);
  CHECK (pick (one, "1"));
  CHECK (clients[one].MenuId == MenuId::Kick);
  CHECK (pick (one, "8")); // single page now, nothing to advance to
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "5"));
  CHECK (clients[one].MenuId == MenuId::Kick);
  CHECK (pick (one, "9")); // single page now, nothing to go back to
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "5"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::Control);

  bots.Destroy ();
  ctrl.KickBotByMenu (1); // nobody left, back to control
  CHECK (clients[one].MenuId == MenuId::Control);

  // ...control removal zeroes the quota, exits close...
  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "4"));
  CHECK (clients[one].MenuId == MenuId::None);
  CHECK (cv_quota.As<int> () == 0);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "1"));
  CHECK (pick (one, "3")); // random kick over the dormant roster
  CHECK (clients[one].MenuId == MenuId::Control);

  // ...main kill, exit and default legs close the matrix...
  open (one);
  CHECK (pick (one, "4"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "10"));
  CHECK (clients[one].MenuId == MenuId::None);

  open (one);
  CHECK (pick (one, "9"));
  CHECK (clients[one].MenuId == MenuId::Main);

  cv_quota.Set (quota_before);
  cv_debug.Set (debug_before);
  cv_fun_mode.Set (fun_before.chars ());
  mp_limitteams.Set (limitteams_before);
  mp_autoteambalance.Set (autobal_before);
  ctrl.SetIssuer (nullptr);
  bots.Destroy ();
}

TEST_CASE ("unit/control_pageflow") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootControl (engine, cs);
  CleanupControlFiles ();

  const int goal_before = cv_debug_goal.As<int> ();

  auto pick = [&] (edict_t *who, const char *item) -> bool {
    ystl::Array<ystl::String> args {};
    args.push ("menuselect");
    args.push (item);
    engine.SetCmdArgs (args);
    ctrl.CollectArgs ();
    ctrl.SetIssuer (who);
    return ctrl.ExecuteMenus ();
  };

  edict_t *editor = game.CreateFakeClient ("PageEditor");
  HOST_REQUIRE (!game.IsNullEntity (editor));
  clients.Update ();
  clients[editor].flags |= ClientFlags::Admin;

  RunCmdAs (engine, editor, { "yb", "g", "acquire_editor" });
  HOST_REQUIRE (graph.HasEditor ());

  RunCmd (engine, { "yb", "g", "on" });
  editor->v.origin = graph[0].origin;
  RunCmd (engine, { "yb", "g", "cache", "1" });

  // ...page one toggles, caches, opens every submenu and pages over...
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage1);

  CHECK (pick (editor, "1"));
  CHECK (!graph.HasEditFlag (GraphEdit::On));
  CHECK (pick (editor, "1"));
  CHECK (graph.HasEditFlag (GraphEdit::On));

  CHECK (pick (editor, "2"));
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage1);

  CHECK (pick (editor, "3"));
  CHECK (clients[editor].MenuId == MenuId::NodePath);

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "5"));
  CHECK (clients[editor].MenuId == MenuId::NodeType);

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "7"));
  CHECK (clients[editor].MenuId == MenuId::NodeAutoPath);

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "8"));
  CHECK (clients[editor].MenuId == MenuId::NodeRadius);

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "9"));
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage2);

  // ...page two stores, loads, checks and flips auto and noclip...
  RunCmd (engine, { "yb", "g", "save" });

  {
    MirrorControlFile (bstor.BuildPath (StorageFile::Graph), bstor.BuildPath (StorageFile::Graph, true));
  }

  CHECK (pick (editor, "6"));
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage2);
  CHECK (pick (editor, "5"));
  CHECK (pick (editor, "4"));
  CHECK (pick (editor, "7"));
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage2);

  CHECK (pick (editor, "2"));
  CHECK (graph.HasEditFlag (GraphEdit::Auto));
  CHECK (pick (editor, "2"));
  CHECK (!graph.HasEditFlag (GraphEdit::Auto));

  CHECK (pick (editor, "8"));
  CHECK (graph.HasEditFlag (GraphEdit::Noclip));
  CHECK (editor->v.movetype == MOVETYPE_NOCLIP);
  CHECK (pick (editor, "8"));
  CHECK (!graph.HasEditFlag (GraphEdit::Noclip));
  CHECK (editor->v.movetype == MOVETYPE_WALK);

  // ...debug tour: nearest pins a goal, facing misses, stop clears...
  CHECK (pick (editor, "1"));
  CHECK (clients[editor].MenuId == MenuId::NodeDebug);
  CHECK (pick (editor, "1"));
  CHECK (cv_debug_goal.As<int> () == graph.GetEditorNearest ());
  CHECK (pick (editor, "2"));
  CHECK (pick (editor, "3"));
  CHECK (cv_debug_goal.As<int> () == kInvalidNodeIndex);
  CHECK (pick (editor, "10"));

  // ...flag tour toggles every flag, camp opens directions...
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "9"));
  CHECK (pick (editor, "3"));
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);

  const int flag_node = graph.GetEditorNearest ();
  const float z_before = graph[flag_node].origin.z;

  CHECK (pick (editor, "1"));
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);

  graph.paths_[static_cast<size_t> (flag_node)].flags |= NodeFlag::CTOnly;
  CHECK (pick (editor, "2")); // CT holder swaps to the Terrorists
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);
  CHECK (pick (editor, "3")); // Terrorist holder swaps back to CT
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);

  for (const char *item : { "4", "5", "6", "7" }) {
    CHECK (pick (editor, item));
    CHECK (clients[editor].MenuId == MenuId::NodeFlag);
  }
  CHECK (pick (editor, "8")); // crouch dips the node
  CHECK (graph[graph.GetEditorNearest ()].origin.z == z_before - 18.0f);
  CHECK (pick (editor, "9")); // camp opens directions
  CHECK (clients[editor].MenuId == MenuId::CampDirections);

  CHECK (pick (editor, "1"));
  CHECK (pick (editor, "2"));
  CHECK (clients[editor].MenuId == MenuId::CampDirections);
  CHECK (pick (editor, "10"));
  CHECK (clients[editor].MenuId == MenuId::None);

  RunCmdAs (engine, editor, { "yb", "g", "flags" });
  CHECK (pick (editor, "9")); // camp off, back to flags
  CHECK (clients[editor].MenuId == MenuId::NodeFlag);
  CHECK (pick (editor, "8")); // crouch back up
  CHECK (graph[graph.GetEditorNearest ()].origin.z == z_before);

  // ...radius tour pins one value, exit closes...
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "8"));
  CHECK (pick (editor, "5"));
  CHECK (graph[graph.GetEditorNearest ()].radius == 48.0f);
  CHECK (clients[editor].MenuId == MenuId::NodeRadius);
  CHECK (pick (editor, "10"));

  // ...type tour adds normal and goal, learns jump, exits...
  editor->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f); // adds refuse spots near nodes
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "5"));
  CHECK (pick (editor, "1"));
  CHECK (clients[editor].MenuId == MenuId::NodeType);
  CHECK (graph.Length () == 9);

  editor->v.origin = ystl::Vector (5020.0f, 0.0f, 0.0f);
  CHECK (pick (editor, "8"));
  CHECK (graph.Length () == 10);
  CHECK (pick (editor, "9"));
  CHECK (pick (editor, "10"));
  editor->v.origin = graph[0].origin;

  // ...autopath tour sets, defaults and exits...
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "7"));
  CHECK (pick (editor, "2"));
  CHECK (clients[editor].MenuId == MenuId::NodeAutoPath);
  CHECK (pick (editor, "8"));
  CHECK (pick (editor, "10"));

  // ...path tour creates every direction and exits...
  RunCmd (engine, { "yb", "g", "on" });
  editor->v.origin = graph[0].origin;
  RunCmd (engine, { "yb", "g", "cache", "1" });

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "3"));

  for (const char *item : { "1", "2", "3", "4" }) {
    CHECK (pick (editor, item));
    CHECK (clients[editor].MenuId == MenuId::NodePath);
  }
  CHECK (pick (editor, "10"));

  // ...page one destructive legs erase a path and the nearest node...
  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "4"));
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage1);
  CHECK (pick (editor, "6"));
  CHECK (graph.Length () == 9);
  editor->v.origin = graph[0].origin;
  CHECK (pick (editor, "10"));

  // ...empty graph legs: apply, save, check, clean and cache degrade...
  graph.Reset ();

  size_t mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "apply" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "no graph to apply"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "addbot" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "There is no graph"));

  mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "save" });
  DrainPrints (engine);
  CHECK (PrintedSince (engine, mark, "Could not save nodes"));

  RunCmd (engine, { "yb", "g", "check" });
  RunCmd (engine, { "yb", "g", "path_clean" });
  RunCmd (engine, { "yb", "g", "path_clean", "0" });
  RunCmd (engine, { "yb", "g", "cache" });
  RunCmd (engine, { "yb", "g", "cache", "0" });
  RunCmd (engine, { "yb", "g", "delete", "0" });
  RunCmd (engine, { "yb", "g", "delete" });
  RunCmd (engine, { "yb", "g", "clean", "0" });
  RunCmd (engine, { "yb", "g", "clean", "all" });
  RunCmd (engine, { "yb", "g", "setradius", "10" });
  RunCmd (engine, { "yb", "g", "teleport", "0" });

  RunCmdAs (engine, editor, { "yb", "g", "menu" });
  CHECK (pick (editor, "9"));
  CHECK (pick (editor, "4")); // save with check fails on the empty graph
  CHECK (clients[editor].MenuId == MenuId::NodeMainPage2);

  ystl::plat.remove_file (bstor.BuildPath (StorageFile::Graph).chars ());
  ystl::plat.remove_file (bstor.BuildPath (StorageFile::Graph, true).chars ());
  CHECK (pick (editor, "6")); // load fails with no file anywhere
  CHECK (pick (editor, "7")); // check fails on the empty graph

  // ...close guards and editor release end the tour...
  CHECK (pick (editor, "10"));
  ctrl.CloseMenu ();
  ctrl.SetIssuer (nullptr);
  ctrl.CloseMenu (); // no issuer, early return
  ctrl.SetIssuer (editor);
  ctrl.CloseMenu (); // already none, early return

  size_t free_mark = PrintMark (engine);
  RunCmd (engine, { "yb", "g", "release_editor" });
  DrainPrints (engine);
  CHECK (!graph.HasEditor ());
  CHECK (PrintedSince (engine, free_mark, "freed"));

  RunCmd (engine, { "yb", "g", "release_editor" });

  cv_debug_goal.Set (goal_before);
  ctrl.SetIssuer (nullptr);
  CleanupControlFiles ();
  bots.Destroy ();
}

} // namespace bot
