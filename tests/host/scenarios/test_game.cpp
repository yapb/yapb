//
// YaPB test host: unit/engine_{game,cvars,state}.
//
// SPDX-License-Identifier: Unlicense
//
// Remaining Game coverage without skips: flags, entities, commands,
// messages, cvars, game-state leftovers, grenade/interesting tracking,
// hitbox enumeration. Modern staging (flag asserts need a Modern game).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("unit/engine_game [modern]") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "-modern/cstrike");

  // breakable seeded before activation for the list-path checks below
  edict_t *seed = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (seed != nullptr);
  seed->v.health = 50.0f;
  seed->v.takedamage = DAMAGE_YES;
  seed->v.movetype = MOVETYPE_PUSH;

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  // identities that need no world at all
  CHECK (strcmp (game.GetMapName (), "de_test") == 0);
  CHECK (strcmp (game.GetRunningModName (), "cstrike") == 0);
  CHECK (game.IsDedicatedServer ());

  // the game library is really loaded; on POSIX the engine self-locate
  // (dladdr into a shared engine) fails for the PIE test executable, while
  // Windows VirtualQuery always resolves the executable module. elib only
  // feeds the query hook, disabled by default
  CHECK (game.Lib ().handle () != nullptr);
#if defined(YSTL_WINDOWS)
  CHECK (game.Elib ().handle () != nullptr);
#else
  CHECK (game.Elib ().handle () == nullptr);
#endif

  // edict index roundtrips and null semantics
  HOST_REQUIRE (game.GetStartEntity () == engine.EdictList ());
  CHECK (strcmp (game.GetStartEntity ()->v.classname.chars (), "worldspawn") == 0);
  CHECK (game.EntityOfIndex (0) == game.GetStartEntity ());
  CHECK (game.EntityOfIndex (3) == engine.EdictList () + 3);
  CHECK (game.PlayerOfIndex (0) == engine.EdictList () + 1);
  CHECK (game.IndexOfEntity (engine.EdictList () + 5) == 5);
  CHECK (game.IndexOfPlayer (engine.EdictList () + 5) == 4);
  CHECK (game.IsNullEntity (nullptr));
  CHECK (game.IsNullEntity (game.GetStartEntity ())); // index 0 reads as null
  CHECK (!game.IsNullEntity (seed));

  edict_t *freed = engine.SpawnEntity ("info_target");
  HOST_REQUIRE (freed != nullptr);
  engine.Funcs ().pfnRemoveEntity (freed);
  CHECK (game.IsNullEntity (freed));

  // flag add/clear roundtrip (restored immediately: flags drive behavior)
  CHECK (!game.Is (GameFlags::ZombieMod));
  game.AddGameFlag (GameFlags::ZombieMod);
  CHECK (game.Is (GameFlags::ZombieMod));
  game.ClearGameFlag (GameFlags::ZombieMod);
  CHECK (!game.Is (GameFlags::ZombieMod));

  // local entity roundtrip
  CHECK (game.GetLocalEntity () == nullptr);
  edict_t *local = engine.SpawnClient ("Local");
  HOST_REQUIRE (local != nullptr);
  game.SetLocalEntity (local);
  CHECK (game.GetLocalEntity () == local);
  game.SetLocalEntity (nullptr);
  CHECK (game.GetLocalEntity () == nullptr);

  // spawn counting goes through the spawn hook
  CHECK (game.GetSpawnCount (Team::CT) == 0);
  CHECK (game.GetSpawnCount (Team::Terrorist) == 0);

  edict_t *ct_spawn = engine.SpawnEntity ("info_player_start");
  edict_t *t_spawn = engine.SpawnEntity ("info_player_deathmatch");
  HOST_REQUIRE (ct_spawn != nullptr && t_spawn != nullptr);
  table.pfnSpawn (ct_spawn);
  table.pfnSpawn (t_spawn);
  CHECK (game.GetSpawnCount (Team::CT) == 1);
  CHECK (game.GetSpawnCount (Team::Terrorist) == 1);

  // player-start draw models resolve per spawn class
  edict_t *vip_spawn = engine.SpawnEntity ("info_vip_start");
  HOST_REQUIRE (vip_spawn != nullptr);
  game.SetPlayerStartDrawModels ();
  CHECK (strstr (engine.StringText (ct_spawn->v.model), "urban.mdl") != nullptr);
  CHECK (strstr (engine.StringText (t_spawn->v.model), "terror.mdl") != nullptr);
  CHECK (strstr (engine.StringText (vip_spawn->v.model), "vip.mdl") != nullptr);

  // unprecaching allows a second precache pass to append again
  const size_t models_before = engine.PrecachedModels ().size ();
  game.SetUnprecached ();
  game.Precache ();
  CHECK (engine.PrecachedModels ().size () > models_before);

  // breakable registry content and invalidation
  engine.AdvanceTime (0.6f); // interesting-entity refresh runs every 0.5s
  game_state.UpdateInterestingEntities ();
  CHECK (game.HasBreakables ());

  bool saw_seed = false;

  for (const auto &entry : game_state.GetInterestingEntities ()) {
    saw_seed = saw_seed || (entry.kind == EntityKind::Breakable && entry.ent == seed);
  }
  CHECK (saw_seed);
  CHECK (game.IsBreakableValid (seed));
  game.MarkBreakableAsInvalid (seed);
  CHECK (!game.IsBreakableValid (seed));

  // fake client creation resets the edict but keeps name and flags
  edict_t *fake = game.CreateFakeClient ("CreatedBot");
  HOST_REQUIRE (fake != nullptr);
  CHECK ((fake->v.flags & FL_FAKECLIENT) != 0);
  CHECK ((fake->v.flags & FL_CLIENT) != 0);
  CHECK (strcmp (fake->v.netname.chars (), "CreatedBot") == 0);
  CHECK (fake->v.health == 0.0f);

  bool saw_create = false;
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "CreateFakeClient") {
      saw_create = true;
      break;
    }
  }
  CHECK (saw_create);

  // idle command interface reads empty...
  CHECK (!game.IsBotCmd ());
  CHECK (game.BotArgc () == 0);
  CHECK (game.BotArgs ()[0] == '\0');
  CHECK (game.BotArgv (0)[0] == '\0');
  CHECK (game.BotArgv (99)[0] == '\0');

  // ...while a raw BotCommand struct starts the same way
  Command fresh {};
  CHECK (!fresh.Active ());
  CHECK (fresh.arg_count == 0);
  CHECK (fresh.Argv (0).empty ());
  CHECK (fresh.Argv (fresh.kMaxArgs + 10).empty ());

  edict_t *cmder = engine.SpawnClient ("Commander");
  HOST_REQUIRE (cmder != nullptr);
  game.Command (cmder, "say %s", "hi");
  game.PrepareBotArgs (cmder);
  CHECK (cs.HasCall ("ClientCommand"));
  CHECK (!game.IsBotCmd ());

  // engine command registration lands in the engine table
  game.RegisterEngineCommand ("testcmd_xyz", [] () {});

  bool found_cmd = false;
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    const auto &call = engine.Calls ()[i];

    if (call.name == "AddServerCommand" && call.detail == "testcmd_xyz") {
      found_cmd = true;
      break;
    }
  }
  CHECK (found_cmd);

  // sounds go through the emit hook and are heard
  const size_t sounds_before = engine.HeardSounds ().size ();
  game.PlaySound (cmder, "weapons/fire.wav");
  HOST_REQUIRE (engine.HeardSounds ().size () == sounds_before + 1);
  CHECK (engine.HeardSounds ()[sounds_before].sample == "weapons/fire.wav");
  CHECK (engine.HeardSounds ()[sounds_before].ent_index == engine.Funcs ().pfnIndexOfEdict (cmder));

  game.PlaySound (nullptr, "weapons/fire.wav");
  CHECK (engine.HeardSounds ().size () == sounds_before + 1);

  // client prints: null falls back to the server console...
  const size_t calls_before = engine.Calls ().size ();
  game.ClientPrint (nullptr, "null fallback");
  CHECK (engine.Calls ().size () > calls_before);

  // ...a listen entity also takes the server path...
  const size_t listen_before = engine.Calls ().size ();
  game.SetLocalEntity (local);
  game.ClientPrint (local, "listen");
  game.SetLocalEntity (nullptr);
  CHECK (engine.Calls ().size () > listen_before);

  // ...and a real client goes through the reliable channel
  const size_t messages_before = engine.Messages ().size ();
  game.ClientPrint (cmder, "hello console");
  HOST_REQUIRE (engine.Messages ().size () >= messages_before + 3);
  CHECK (engine.Messages ()[messages_before + 1] == "byte=2"); // HUD_PRINTCONSOLE

  game.CenterPrint (cmder, "hello center");
  CHECK (engine.Messages ()[engine.Messages ().size () - 1] == "end");

  // fake clients never get messages
  const size_t muted_before = engine.Messages ().size ();
  game.ClientPrint (fake, "muted");
  CHECK (engine.Messages ().size () == muted_before);

  // hud message to null is a silent no-op
  hudtextparms_t htp {};
  game.SendHudMessage (nullptr, htp, "nowhere");
  CHECK (engine.Messages ().size () == muted_before);

  // direct message writer roundtrip
  const size_t writer_before = engine.Messages ().size ();
  {
    MessageWriter writer {};
    writer.Start (5, 23, nullptr, nullptr).WriteByte (9).WriteString ("hi");
    writer.end ();
  }
  HOST_REQUIRE (engine.Messages ().size () == writer_before + 4);
  CHECK (engine.Messages ()[writer_before + 1] == "byte=9");
  CHECK (engine.Messages ()[writer_before + 2] == "string=hi");
  CHECK (engine.Messages ()[writer_before + 3] == "end");

  // fixed-point helpers
  CHECK (MessageWriter::Fu16 (0.0f, 0) == 0);
  CHECK (MessageWriter::Fu16 (1.0f, 0) == 1);
  CHECK (MessageWriter::Fs16 (0.0f, 0) == 0);
  CHECK (MessageWriter::Fs16 (1.0f, 0) == 1);

  // line drawing emits a beam points temp entity
  const size_t line_before = engine.Messages ().size ();
  game.DrawLine (cmder, ystl::Vector (1.0f, 2.0f, 3.0f), ystl::Vector (7.0f, 8.0f, 9.0f), 10, 0, ystl::Color (255, 0, 0), 200, 5, 10);
  HOST_REQUIRE (engine.Messages ().size () > line_before + 4);
  CHECK (engine.Messages ()[line_before + 1] == "byte=0"); // TE_BEAMPOINTS
  CHECK (engine.Messages ()[line_before + 2] == "coord=7.000000");

  const size_t mute_line_before = engine.Messages ().size ();
  game.DrawLine (nullptr, ystl::Vector (1.0f, 2.0f, 3.0f), ystl::Vector (7.0f, 8.0f, 9.0f), 10, 0, ystl::Color (255, 0, 0), 200, 5, 10);
  CHECK (engine.Messages ().size () == mute_line_before);

  // cvar reads go through the engine string table
  CHECK (strcmp (game.FindCvar ("mp_timelimit").chars (), "0") == 0);
  CHECK (game.FindCvar ("no_such_cvar").empty ());

  // server command and print go to the server channel
  const size_t server_before = engine.ServerCommands ().size ();
  game.ServerCommand ("mp_timelimit 30");
  CHECK (engine.ServerCommands ().size () == server_before + 1);
  CHECK (engine.ServerCommands ()[server_before] == "mp_timelimit 30\n");

  const size_t print_before = engine.Calls ().size ();
  game.Print ("hello server");
  CHECK (engine.Calls ().size () > print_before);

  // gamedll team index is the real team shifted by CT
  clients.Update ();
  CHECK (game.GetPlayerTeamGame (cmder) == game.GetRealPlayerTeam (cmder) + Team::CT);

  // developer mode follows the developer cvar
  CHECK (!game.IsDeveloperMode ());
  engine.SetCvar ("developer", "1");
  CHECK (game.IsDeveloperMode ());
  engine.SetCvar ("developer", "0");
  CHECK (!game.IsDeveloperMode ());

  // healthy environment reverts fake features on modern dedicated servers
  cv_show_latency.Set (2);
  cv_show_avatars.Set (1);
  cv_enable_query_hook.Set (1);
  game.EnsureHealthyGameEnvironment ();
  CHECK (cv_show_latency.As<int> () == 0);
  CHECK (!cv_show_avatars.As<bool> ());
  CHECK (!cv_enable_query_hook.As<bool> ());

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/engine_cvars") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // registered name carries the bot prefix
  CHECK (strstr (cv_display_welcome_text.Name ().chars (), "display_welcome_text") != nullptr);

  // typed accessors over a registered bot cvar
  CHECK (cv_display_welcome_text.As<bool> ());
  CHECK (cv_display_welcome_text.As<int> () == 1);
  CHECK (strcmp (cv_display_welcome_text.As<ystl::StringRef> ().chars (), "1") == 0);
  CHECK (static_cast<float> (cv_display_welcome_text) == 1.0f);

  cv_display_welcome_text.Set (0.0f);
  CHECK (!cv_display_welcome_text.As<bool> ());

  cv_display_welcome_text.Revert ();
  CHECK (cv_display_welcome_text.As<bool> ());

  cv_display_welcome_text.Set (2);
  CHECK (cv_display_welcome_text.As<int> () == 2);
  cv_display_welcome_text.Revert ();

  // references resolve game cvars on every access
  ConVarRef timelimit ("mp_timelimit");
  CHECK (timelimit.Exists ());
  CHECK (timelimit.Value<float> () == 0.0f);

  timelimit.Set ("99");
  CHECK (timelimit.Value<float> () == 99.0f);
  CHECK (timelimit.Value<int> () == 99);

  ConVarRef missing ("no_such_cvar_ever");
  CHECK (!missing.Exists ());
  CHECK (missing.Value<float> () == 0.0f);

  // bounds checker reverts out-of-range values to the initial one
  cv_breakable_health_limit.Set (99999.0f);
  game.CheckCvarsBounds ();
  CHECK (cv_breakable_health_limit.As<float> () == 500.0f);

  // cvar descriptions are stored on the registration stack
  game.SetCvarDescription (cv_display_welcome_text, "test description");

  bool found_desc = false;
  for (const auto &var : game.GetCvars ()) {
    if (strstr (var.name.chars (), "display_welcome_text")) {
      found_desc = true;
      CHECK (strstr (var.info.chars (), "test description") != nullptr);
    }
  }
  CHECK (found_desc);

  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/engine_state [modern]") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "-modern/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  // round flags start clear
  CHECK (!game_state.IsRoundOver ());
  CHECK (!game_state.IsResetHud ());

  game_state.SetRoundOver (true);
  game_state.SetResetHud (true);
  CHECK (game_state.IsRoundOver ());
  CHECK (game_state.IsResetHud ());
  game_state.SetRoundOver (false);
  game_state.SetResetHud (false);

  // tracked lists start empty
  CHECK (!game_state.HasActiveGrenades ());
  CHECK (!game_state.HasInterestingEntities ());
  CHECK (game_state.GetActiveGrenades ().empty ());
  CHECK (game_state.GetInterestingEntities ().empty ());

  // live grenade shows up after the throttle window
  edict_t *nade = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (nade != nullptr);
  nade->v.model = string_t (engine.AllocString ("models/p_hegrenade.mdl"));

  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);

  edict_t *item = engine.SpawnEntity ("item_healthkit");
  HOST_REQUIRE (item != nullptr);

  edict_t *monster = engine.SpawnEntity ("monster_zombie");
  HOST_REQUIRE (monster != nullptr);
  monster->v.flags = FL_MONSTER;

  edict_t *vip = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (vip != nullptr);

  edict_t *button = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (button != nullptr);

  engine.AdvanceTime (1.0f);
  game_state.UpdateActiveGrenade ();
  game_state.UpdateInterestingEntities ();

  CHECK (game_state.HasActiveGrenades ());
  CHECK (game_state.GetActiveGrenades ().size () == 1);
  CHECK (game_state.GetActiveGrenades ()[0].ent == nade);

  CHECK (game_state.HasInterestingEntities ());

  bool saw_nade = false, saw_box = false, saw_item = false, saw_monster = false;
  bool saw_hostage = false, saw_button = false;
  for (const auto &entry : game_state.GetInterestingEntities ()) {
    const auto ent = entry.ent;
    saw_nade = saw_nade || ent == nade;
    saw_box = saw_box || ent == box;
    saw_item = saw_item || ent == item;
    saw_monster = saw_monster || ent == monster;
    saw_hostage = saw_hostage || ent == vip;
    saw_button = saw_button || ent == button;
  }
  CHECK (saw_nade && saw_box && saw_item);
  CHECK (!saw_monster); // attack_monsters defaults to off
  CHECK (!saw_hostage); // no hostage-rescue map flag here
  CHECK (!saw_button); // no button map flag here

  // opt-in monsters appear on the next sweep
  cv_attack_monsters.Set (1.0f);
  engine.AdvanceTime (1.0f);
  game_state.UpdateInterestingEntities ();

  saw_monster = false;
  for (const auto &entry : game_state.GetInterestingEntities ()) {
    saw_monster = saw_monster || entry.ent == monster;
  }
  CHECK (saw_monster);
  cv_attack_monsters.Set (0.0f);

  // detonation tracker is a plain entity map
  CHECK (!sgtrack.Has (nade));
  sgtrack.Acquire (nade, ystl::Vector (1.0f, 2.0f, 3.0f));
  CHECK (sgtrack.Has (nade));

  const ystl::Vector &pos = sgtrack.Find (nade);
  CHECK (pos.x == 1.0f && pos.y == 2.0f && pos.z == 3.0f);

  sgtrack.Clear ();
  CHECK (!sgtrack.Has (nade));

  // hitbox enumeration without studio data falls back to origin math
  // and clears the studio-models flag (single failure disables it)
  CHECK (game.Is (GameFlags::HasStudioModels));

  edict_t *player = engine.SpawnClient ("Hitbox");
  HOST_REQUIRE (player != nullptr);
  player->v.origin = ystl::Vector (100.0f, 200.0f, 300.0f);
  player->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  clients.Update ();

  PlayerHitboxEnumerator hitbox {};
  const ystl::Vector head = hitbox.Get (player, PlayerPart::Head, 10.0f);
  CHECK (head.x == 100.0f && head.y == 200.0f && head.z == 328.0f);
  CHECK (!game.Is (GameFlags::HasStudioModels));

  const ystl::Vector stomach = hitbox.Get (player, PlayerPart::Stomach, 10.0f);
  CHECK (stomach.x == 100.0f && stomach.y == 200.0f && stomach.z == 300.0f);

  const ystl::Vector arm = hitbox.Get (player, PlayerPart::LeftArm, 10.0f);
  CHECK (arm.x == head.x && arm.y == head.y && arm.z == head.z);

  const ystl::Vector leg = hitbox.Get (player, PlayerPart::RightLeg, 10.0f);
  CHECK (leg.x == head.x && leg.y == head.y && leg.z == 266.0f);

  const ystl::Vector feet = hitbox.Get (player, PlayerPart::Feet, 10.0f);
  CHECK (feet.z == 266.0f); // standing: origin - 34

  // cached parts survive entity moves until the timestamp lapses
  player->v.origin = ystl::Vector (500.0f, 600.0f, 700.0f);
  const ystl::Vector stale = hitbox.Get (player, PlayerPart::Head, 10.0f);
  CHECK (stale.x == 100.0f);

  engine.AdvanceTime (11.0f);
  const ystl::Vector fresh = hitbox.Get (player, PlayerPart::Head, 10.0f);
  CHECK (fresh.x == 500.0f && fresh.z == 728.0f);

  // ducking lowers the feet spot
  player->v.flags |= FL_DUCKING;
  engine.AdvanceTime (11.0f);
  const ystl::Vector duck_feet = hitbox.Get (player, PlayerPart::Feet, 0.0f);
  CHECK (duck_feet.z == 686.0f); // 700 - 14

  hitbox.Reset ();
  const ystl::Vector after_reset = hitbox.Get (player, PlayerPart::Head, 0.0f);
  CHECK (after_reset.x == 500.0f);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

} // namespace bot
