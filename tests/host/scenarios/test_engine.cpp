//
// YaPB test host: unit/engine_{tables,entities,round}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file unit coverage for engine.cpp: string interop + trace cache
// (tables only), entity classification + search + teams (full prefix),
// game-state timing (postload prefix).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("unit/engine_tables") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  // engine string interop: offsets roundtrip through the string table
  const auto hello = engine.AllocString ("hello");
  CHECK (strcmp (string_t::from (hello), "hello") == 0);
  CHECK (strcmp (string_t::from (0), "") == 0);

  // HLString from a foreign pointer allocates through pfnAllocString
  string_t foreign ("world-outside");
  CHECK (strcmp (foreign.chars (), "world-outside") == 0);
  CHECK (foreign.str () == "world-outside");

  // globals are live: time, client limits and renderer detection
  CHECK (game.Time () == 1.0f);
  CHECK (game.MaxClients () == 16);
  CHECK (game.IsSoftwareRenderer ()); // dedicated servers use sw structures
  CHECK (!game.Is25thAnniversaryUpdate ());
  CHECK (!game.IsGoldClientListenServer ());

  // cs binary names follow the platform rules (x86-64 linux here)
  ystl::SmallArray<ystl::String> libs {};
  game.ConstructCsBinaryName (libs);
  HOST_REQUIRE (libs.size () == 2);
  CHECK (libs[0] == "cs_amd64");
  CHECK (libs[1] == "mp_amd64");

  // android x86 has neither arm nor x64 flags, still resolves its own lib
  const bool saved_android = ystl::plat.android;
  const bool saved_arm = ystl::plat.arm;
  const bool saved_x64 = ystl::plat.x64;

  ystl::plat.android = true;
  ystl::plat.arm = false;
  ystl::plat.x64 = false;

  ystl::SmallArray<ystl::String> android_libs {};
  game.ConstructCsBinaryName (android_libs);
  HOST_REQUIRE (android_libs.size () == 1);
  CHECK (android_libs[0] == "libcs_android_i386");

  // android x86-64 is 64-bit but not arm, arm64 needs both flags
  ystl::plat.arm = false;
  ystl::plat.x64 = true;

  ystl::SmallArray<ystl::String> android64_libs {};
  game.ConstructCsBinaryName (android64_libs);
  HOST_REQUIRE (android64_libs.size () == 1);
  CHECK (android64_libs[0] == "libcs_android_amd64");

  ystl::plat.arm = true;

  ystl::SmallArray<ystl::String> android_arm64_libs {};
  game.ConstructCsBinaryName (android_arm64_libs);
  HOST_REQUIRE (android_arm64_libs.size () == 1);
  CHECK (android_arm64_libs[0] == "libcs_android_arm64");

  ystl::plat.android = saved_android;
  ystl::plat.arm = saved_arm;
  ystl::plat.x64 = saved_x64;

  // trace cache: first call misses, second hits, TTL expiry misses again
  int line_calls = 0;
  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    ++line_calls;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
  });

  Trace::Result hit {};
  trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &hit);
  trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &hit);

  CHECK (line_calls == 1);
  CHECK (trace.GetHits () == 1);
  CHECK (trace.GetMisses () == 1);

  engine.AdvanceTime (0.2f); // past the 0.1 s cache TTL
  trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &hit);
  CHECK (line_calls == 2);

  trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (900.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &hit);
  CHECK (line_calls == 3);

  const float rate = trace.GetHitRate ();
  CHECK (rate > 24.9f && rate < 25.1f); // 1 hit out of 4
  engine.SetTraceLineHook (nullptr);

  // exact keys: two endpoints inside the old 8-unit quantization cell must not
  // share a result. a target that moved a couple units at point-blank used to
  // reuse a stale blocked trace, so bots intermittently failed to see it.
  {
    int calls = 0;
    engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
      ++calls;
      testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
      out->flFraction = v2[0] < 12.0f ? 0.5f : 1.0f; // 10 blocked, 14 clear
    });

    Trace::Result a {}, b {};
    trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &a);
    trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (14.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &b);

    CHECK (calls == 2); // 10/8 and 14/8 are the same cell, but the lines differ
    CHECK (a.fraction == 0.5f);
    CHECK (b.fraction == 1.0f);
    engine.SetTraceLineHook (nullptr);
  }

  // entity hits must never be reused from the cache: a door opens or a teammate
  // steps out of the line without the endpoints changing, so a stale blocked
  // result would keep the bot blind
  {
    int calls = 0;
    edict_t *door = engine.SpawnEntity ("func_door");
    HOST_REQUIRE (door != nullptr);
    engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
      ++calls;
      testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
      if (calls == 1) {
        out->flFraction = 0.5f;
        out->pHit = door; // first pass blocked by the door entity
      }
    });

    Trace::Result a {}, b {};
    trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (60.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &a);
    trace.Line (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (60.0f, 0.0f, 0.0f), TraceIgnore::Everything, nullptr, &b);

    CHECK (calls == 2); // door hit was not cached
    CHECK (a.fraction == 0.5f);
    CHECK (b.fraction == 1.0f); // door gone: fresh trace sees the clear line
    engine.SetTraceLineHook (nullptr);
  }

  // hull traces go through their own cache key
  int hull_calls = 0;
  engine.SetTraceHullHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    ++hull_calls;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
  });
  trace.Hull (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (50.0f, 0.0f, 0.0f), TraceIgnore::Everything, 1, nullptr, &hit);
  CHECK (hull_calls == 1);
  CHECK (hit.fraction == 1.0f);
  engine.SetTraceHullHook (nullptr);

  // endpoint openness: a completed, non-solid trace is clear; startSolid/allSolid or an
  // incomplete trace are not. inOpen/inWater must not be consulted (their goldsrc meaning is
  // "trace start was in open space/water", not "endpoint is clear")
  Trace::Result open {};
  open.fraction = 1.0f;
  open.in_open = 0; // deliberately left unset, must still count as clear
  CHECK (trace.IsEndpointClear (open));

  Trace::Result solid {};
  solid.fraction = 1.0f;
  solid.start_solid = 1;
  CHECK (!trace.IsEndpointClear (solid));

  Trace::Result partial {};
  partial.fraction = 0.5f;
  CHECK (!trace.IsEndpointClear (partial));

  // visibility sets: null set is visible, leaf bits decide otherwise
  edict_t eye {};
  eye.headnode = -1;
  eye.num_leafs = 2;
  eye.leafnums[0] = 3;
  eye.leafnums[1] = 65;

  CHECK (game.CheckVisibility (&eye, nullptr));

  uint8_t pvs[16] = {};
  CHECK (!game.CheckVisibility (&eye, pvs));

  pvs[0] = 8; // leaf 3
  CHECK (game.CheckVisibility (&eye, pvs));
}

TEST_CASE ("unit/engine_entities") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // map entities before activation: the level scan derives map flags
  // and the breakable seed list from them
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);
  edict_t *vip_zone = engine.SpawnEntity ("func_vip_safetyzone");
  HOST_REQUIRE (vip_zone != nullptr);

  edict_t *seeded = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (seeded != nullptr);
  seeded->v.health = 50.0f;
  seeded->v.takedamage = DAMAGE_YES;
  seeded->v.movetype = MOVETYPE_PUSH;

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  CHECK (game.MapIs (MapFlags::Demolition));
  CHECK (game.MapIs (MapFlags::Assassination));
  CHECK (game.HasBreakables ());
  CHECK (game.IsBreakableValid (seeded));

  // aliveness: deadflag, health and noclip all disqualify
  edict_t *player = engine.SpawnClient ("Alive");
  HOST_REQUIRE (player != nullptr);
  CHECK (game.IsAliveEntity (player));

  player->v.deadflag = DEAD_DEAD;
  CHECK (!game.IsAliveEntity (player));
  player->v.deadflag = DEAD_NO;

  player->v.health = 0.0f;
  CHECK (!game.IsAliveEntity (player));
  player->v.health = 100.0f;

  player->v.movetype = MOVETYPE_NOCLIP;
  CHECK (!game.IsAliveEntity (player));
  player->v.movetype = MOVETYPE_WALK;

  // player / fakeclient identity
  CHECK (game.IsPlayerEntity (player));
  CHECK (!game.IsFakeClientEntity (player));

  edict_t *bot = engine.SpawnClient ("Fake");
  HOST_REQUIRE (bot != nullptr);
  bot->v.flags |= FL_FAKECLIENT;
  CHECK (game.IsPlayerEntity (bot));
  CHECK (game.IsFakeClientEntity (bot));

  edict_t *proxy = engine.SpawnClient ("Proxy");
  HOST_REQUIRE (proxy != nullptr);
  proxy->v.flags |= FL_PROXY;
  CHECK (!game.IsPlayerEntity (proxy));

  CHECK (!game.IsPlayerEntity (nullptr));

  // monster flag, with the hostage exemption
  edict_t *zombie = engine.SpawnEntity ("monster_zombie");
  HOST_REQUIRE (zombie != nullptr);
  zombie->v.flags = FL_MONSTER;
  CHECK (game.IsMonsterEntity (zombie));
  CHECK (!game.IsPlayerEntity (zombie));

  edict_t *scientist = engine.SpawnEntity ("monster_scientist");
  HOST_REQUIRE (scientist != nullptr);
  scientist->v.flags = FL_MONSTER;
  CHECK (!game.IsMonsterEntity (scientist)); // hostages are exempt
  CHECK (game.IsHostageEntity (scientist));

  edict_t *hostage = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage != nullptr);
  CHECK (game.IsHostageEntity (hostage));
  CHECK (!game.IsHostageEntity (zombie));

  // plain substring match, null-safe
  edict_t *kit = engine.SpawnEntity ("item_healthkit");
  HOST_REQUIRE (kit != nullptr);
  CHECK (game.IsItemEntity (kit));
  CHECK (!game.IsItemEntity (zombie));
  CHECK (!game.IsItemEntity (nullptr));

  // doors match by class hash
  edict_t *door = engine.SpawnEntity ("func_door_rotating");
  HOST_REQUIRE (door != nullptr);
  CHECK (game.IsDoorEntity (door));
  CHECK (!game.IsDoorEntity (seeded));

  // model match skips the 9-char "models/p_" / "models/w_" prefix
  edict_t *gun = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (gun != nullptr);
  gun->v.model = string_t (engine.AllocString ("models/p_ak47.mdl"));
  CHECK (game.IsEntityModelMatches (gun, "ak47.mdl"));
  CHECK (!game.IsEntityModelMatches (gun, "m4a1.mdl"));

  // origins: explicit origin wins, otherwise the bbox center
  const float gun_pos[3] = { 10.0f, 20.0f, 30.0f };
  gun->v.origin.x = gun_pos[0];
  gun->v.origin.y = gun_pos[1];
  gun->v.origin.z = gun_pos[2];

  const ystl::Vector origin = game.GetEntityOrigin (gun);
  CHECK (origin.x == 10.0f && origin.y == 20.0f && origin.z == 30.0f);

  edict_t *centered = engine.SpawnEntity ("info_origin");
  HOST_REQUIRE (centered != nullptr);
  centered->v.absmin = ystl::Vector (0.0f, 0.0f, 0.0f);
  centered->v.size = ystl::Vector (32.0f, 32.0f, 32.0f);

  const ystl::Vector center = game.GetEntityOrigin (centered);
  CHECK (center.x == 16.0f && center.y == 16.0f && center.z == 16.0f);

  // breakables: seeded entry passes, unknown entries do not (list path)
  CHECK (game.IsBreakableEntity (seeded, true));
  CHECK (game.IsBreakableEntity (seeded, false));

  edict_t *unseeded = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (unseeded != nullptr);
  unseeded->v.health = 50.0f;
  unseeded->v.takedamage = DAMAGE_YES;
  unseeded->v.movetype = MOVETYPE_PUSH;
  CHECK (game.IsBreakableEntity (unseeded, true));
  CHECK (!game.IsBreakableEntity (unseeded, false));
  CHECK (!game.IsBreakableValid (unseeded));

  // ... and the attribute matrix on the seeding path
  edict_t *dead = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (dead != nullptr);
  dead->v.health = 0.0f;
  dead->v.takedamage = DAMAGE_YES;
  dead->v.movetype = MOVETYPE_PUSH;
  CHECK (!game.IsBreakableEntity (dead, true));

  edict_t *tanky = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (tanky != nullptr);
  tanky->v.health = 9999.0f;
  tanky->v.takedamage = DAMAGE_YES;
  tanky->v.movetype = MOVETYPE_PUSH;
  CHECK (!game.IsBreakableEntity (tanky, true));

  edict_t *brushed = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (brushed != nullptr);
  brushed->v.health = 50.0f;
  brushed->v.takedamage = DAMAGE_YES;
  brushed->v.movetype = MOVETYPE_PUSH;
  brushed->v.flags = FL_WORLDBRUSH;
  CHECK (!game.IsBreakableEntity (brushed, true));

  edict_t *walker = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (walker != nullptr);
  walker->v.health = 50.0f;
  walker->v.takedamage = DAMAGE_YES;
  walker->v.movetype = MOVETYPE_WALK;
  CHECK (!game.IsBreakableEntity (walker, true));

  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
  wall->v.health = 50.0f;
  wall->v.takedamage = DAMAGE_YES;
  wall->v.movetype = MOVETYPE_PUSHSTEP;
  CHECK (game.IsBreakableEntity (wall, true));

  edict_t *pushable = engine.SpawnEntity ("func_pushable");
  HOST_REQUIRE (pushable != nullptr);
  pushable->v.health = 50.0f;
  pushable->v.takedamage = DAMAGE_YES;
  pushable->v.movetype = MOVETYPE_PUSH;
  CHECK (!game.IsBreakableEntity (pushable, true));

  pushable->v.spawnflags = SF_PUSH_BREAKABLE;
  CHECK (game.IsBreakableEntity (pushable, true));

  pushable->v.spawnflags = SF_BREAK_TRIGGER_ONLY;
  CHECK (!game.IsBreakableEntity (pushable, true));

  // planted bomb: weaponbox branch is config-independent
  edict_t *c4box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (c4box != nullptr);
  c4box->v.model = string_t (engine.AllocString ("models/p_backpack.mdl"));
  CHECK (game.IsBombEntity (c4box));

  // ... while the grenade branch needs the C4ModelName custom, absent here
  edict_t *nade = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (nade != nullptr);
  nade->v.model = string_t (engine.AllocString ("models/p_backpack.mdl"));
  CHECK (!game.IsBombEntity (nade));
  CHECK (!game.IsBombEntity (zombie));

  // vip status comes from the player model infokey
  engine.SetInfoKey ("model", "vip");
  CHECK (game.IsPlayerVip (player));

  engine.SetInfoKey ("model", "terror");
  CHECK (!game.IsPlayerVip (player));
  CHECK (!game.IsPlayerVip (zombie));

  // teams: null is unassigned, fresh clients default to spectator
  CHECK (game.GetPlayerTeam (nullptr) == Team::Unassigned);
  CHECK (game.GetRealPlayerTeam (nullptr) == Team::Unassigned);

  clients.Update ();
  CHECK (game.GetPlayerTeam (player) == Team::Spectator);
  CHECK (game.GetRealPlayerTeam (player) == Team::Spectator);

  // entity search by class, break-early and sphere variants
  const float t1pos[3] = { 100.0f, 0.0f, 0.0f };
  const float t2pos[3] = { 500.0f, 0.0f, 0.0f };
  edict_t *target1 = engine.SpawnEntity ("info_target", t1pos);
  edict_t *target2 = engine.SpawnEntity ("info_target", t2pos);
  HOST_REQUIRE (target1 != nullptr && target2 != nullptr);

  int found = 0;
  game.SearchEntities ("classname", "info_target", [&] (edict_t *) {
    ++found;
    return EntitySearchResult::Continue;
  });
  CHECK (found == 2);

  found = 0;
  game.SearchEntities ("classname", "info_target", [&] (edict_t *) {
    ++found;
    return EntitySearchResult::Break;
  });
  CHECK (found == 1);

  CHECK (game.HasEntityInGame ("info_target"));
  CHECK (!game.HasEntityInGame ("no_such_class"));

  int near = 0;
  game.SearchEntities (ystl::Vector (100.0f, 0.0f, 0.0f), 50.0f, [&] (edict_t *ent) {
    ++near;
    CHECK (ent == target1);
    return EntitySearchResult::Continue;
  });
  CHECK (near == 1);

  bool saw_both = false;
  int wide = 0;
  game.SearchEntities (ystl::Vector (100.0f, 0.0f, 0.0f), 10000.0f, [&] (edict_t *ent) {
    ++wide;
    saw_both = saw_both || ent == target1 || ent == target2;
    return EntitySearchResult::Continue;
  });
  CHECK (wide > 2 && saw_both);

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/engine_round") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // GameRef timing cvars resolve through the precache pass
  game.Precache ();

  // freezetime 6 s, roundtime 2.5 min on top of the frozen clock
  game_state.RoundStart ();
  CHECK (game_state.GetRoundStartTime () == 7.0f);
  CHECK (game_state.GetRoundMidTime () == 82.0f);
  CHECK (game_state.GetRoundEndTime () == 157.0f);
  CHECK (game_state.GetRoundTimeLeft () == 156.0f);
  CHECK (!game_state.IsRoundOver ());

  CHECK (game_state.IsEarlyRound (0.0f));
  CHECK (game_state.IsEarlyRound (5.0f));
  CHECK (!game_state.IsEarlyRound (-100.0f));

  // no bomb, no time left
  CHECK (!game_state.IsBombPlanted ());
  CHECK (game_state.GetBombTimeLeft () == 0.0f);
  CHECK (game_state.GetBombOrigin ().empty ());

  game_state.SetBombPlanted (true);
  CHECK (game_state.IsBombPlanted ());
  CHECK (game_state.GetTimeBombPlanted () == 1.0f);
  engine.AdvanceTime (10.0f);
  CHECK (game_state.GetBombTimeLeft () == 25.0f); // planted at t=1, c4timer 35

  // round end is far on a fresh round: low only against wide thresholds
  CHECK (game_state.IsRoundTimeLow (1000.0f));
  CHECK (!game_state.IsRoundTimeLow (10.0f));

  game_state.SetBombPlanted (false);
  CHECK (game_state.GetBombTimeLeft () == 0.0f);

  // no demolition map here: origin tracking stays a no-op
  CHECK (game_state.GetBombEntity () == nullptr);
  game_state.SetBombOrigin (true);
  CHECK (game_state.GetBombEntity () == nullptr);

   const ystl::Vector some_pos (50.0f, 60.0f, 70.0f);
   game_state.SetBombOrigin (false, some_pos);
   CHECK (game_state.GetBombEntity () == nullptr);

   testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/engine_visibility") {
   // hermetic visibility + software renderer + trace model, no cs load
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   testhost::InstallEngineTables (engine);

   // xash always uses hw structures
   game.AddGameFlag (GameFlags::Xash3D);
   CHECK (!game.IsSoftwareRenderer ());
   game.ClearGameFlag (GameFlags::Xash3D);

   // dedicated off with no world model
   engine.SetDedicatedServer (false);
   illum.ResetWorldModel ();
   CHECK (!game.IsSoftwareRenderer ());

   // world model branches for software check
   mnode_t nodes[2] {};
   model_t model {};
   model.nodes = nodes;
   model.numnodes = 2;

   // parent set fails
   nodes[0].parent = &nodes[0];
   illum.SetWorldModel (&model);
   CHECK (!game.IsSoftwareRenderer ());
   illum.ResetWorldModel ();
   nodes[0].parent = nullptr;

   // child out of range fails
   nodes[0].children[0] = nullptr;
   nodes[0].children[1] = nullptr;
   illum.SetWorldModel (&model);
   CHECK (!game.IsSoftwareRenderer ());
   illum.ResetWorldModel ();

   // child parent mismatch fails
   nodes[0].children[0] = &nodes[1];
   nodes[1].parent = nullptr;
   illum.SetWorldModel (&model);
   CHECK (!game.IsSoftwareRenderer ());
   illum.ResetWorldModel ();

   // valid chain reaches the platform tail
   nodes[0].children[0] = &nodes[1];
   nodes[1].parent = &nodes[0];
   const bool saved_win = ystl::plat.win;
   ystl::plat.win = true;
   illum.SetWorldModel (&model);
   CHECK (!game.IsSoftwareRenderer ());
   illum.ResetWorldModel ();
   ystl::plat.win = false;
   illum.SetWorldModel (&model);
   CHECK (!game.IsSoftwareRenderer ());
   illum.ResetWorldModel ();
   ystl::plat.win = saved_win;
   engine.SetDedicatedServer (true);

   // second visibility loop: break on -1 then engine fallback
   edict_t eye2 {};
   eye2.headnode = 0;
   eye2.leafnums[0] = -1;
   uint8_t pvs2[16] = {};
   CHECK (game.CheckVisibility (&eye2, pvs2));
   CHECK (game.CheckVisibility (&eye2, nullptr));

   // second loop leaf hit
   eye2.leafnums[0] = 3;
   pvs2[0] = 8;
   CHECK (game.CheckVisibility (&eye2, pvs2));
   pvs2[0] = 0;
   CHECK (game.CheckVisibility (&eye2, pvs2));

   // xash legacy has no visibility set
   game.AddGameFlag (GameFlags::Xash3DLegacy);
   CHECK (game.GetVisibilitySet (nullptr, false) == nullptr);
   CHECK (game.GetVisibilitySet (nullptr, true) == nullptr);
   game.ClearGameFlag (GameFlags::Xash3DLegacy);

   // trace model goes straight to the engine
   Trace::Result mr {};
   trace.Model (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f), 0, nullptr, &mr);
   CHECK (mr.fraction == 1.0f);

   bots.Destroy ();
}

TEST_CASE ("unit/engine_spawn") {
   // level scan branches + spawn hooks, one activation
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   edict_t *strip = engine.SpawnEntity ("player_weaponstrip");
   HOST_REQUIRE (strip != nullptr);
   edict_t *ct_spawn = engine.SpawnEntity ("info_player_start");
   HOST_REQUIRE (ct_spawn != nullptr);
   edict_t *t_spawn = engine.SpawnEntity ("info_player_deathmatch");
   HOST_REQUIRE (t_spawn != nullptr);
   edict_t *vip_safe = engine.SpawnEntity ("info_vip_safetyzone");
   HOST_REQUIRE (vip_safe != nullptr);
   edict_t *hostage = engine.SpawnEntity ("hostage_entity");
   HOST_REQUIRE (hostage != nullptr);
   edict_t *escape = engine.SpawnEntity ("func_escapezone");
   HOST_REQUIRE (escape != nullptr);
   edict_t *door = engine.SpawnEntity ("func_door");
   HOST_REQUIRE (door != nullptr);
   edict_t *button = engine.SpawnEntity ("func_button");
   HOST_REQUIRE (button != nullptr);
   edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
   HOST_REQUIRE (bomb_target != nullptr);
   edict_t *brk = engine.SpawnEntity ("func_breakable");
   HOST_REQUIRE (brk != nullptr);
   brk->v.health = 50.0f;
   brk->v.takedamage = DAMAGE_YES;
   brk->v.movetype = MOVETYPE_PUSH;

   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   game.Precache ();
   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

   // legacy weaponstrip gets a fake target
   CHECK (strcmp (engine.StringText (strip->v.target), "fake") == 0);
   // spawn draw models
   CHECK (ct_spawn->v.rendermode == kRenderTransAlpha);
   CHECK (ct_spawn->v.renderamt == 127);
   CHECK ((ct_spawn->v.effects & EF_NODRAW) != 0);
   CHECK (t_spawn->v.rendermode == kRenderTransAlpha);
   // map flags from the scan
   CHECK (game.MapIs (MapFlags::Assassination));
   CHECK (game.MapIs (MapFlags::Escape));
   CHECK (!game.MapIs (MapFlags::HostageRescue));
   CHECK (game.MapIs (MapFlags::HasDoors));
   CHECK (game.MapIs (MapFlags::HasButtons));
   CHECK (game.MapIs (MapFlags::Demolition));
   CHECK (game.HasBreakables ());

   // prefix maps via direct level init
   engine.Globals ().mapname = string_t (engine.AllocString ("fy_test"));
   game.LevelInitialize (engine.EdictList (), engine.EdictCount ());
   CHECK (game.MapIs (MapFlags::FightYard));
   engine.Globals ().mapname = string_t (engine.AllocString ("ka_test"));
   game.LevelInitialize (engine.EdictList (), engine.EdictCount ());
   CHECK (game.MapIs (MapFlags::KnifeArena));
   engine.Globals ().mapname = string_t (engine.AllocString ("he_test"));
   game.LevelInitialize (engine.EdictList (), engine.EdictCount ());
   CHECK (game.MapIs (MapFlags::GrenadeWar));
   engine.Globals ().mapname = string_t (engine.AllocString ("de_test"));

   // non-legacy strip is removed
   game.ClearGameFlag (GameFlags::Legacy);
   game.ClearGameFlag (GameFlags::ReGameDLL);
   edict_t *strip2 = engine.SpawnEntity ("player_weaponstrip");
   HOST_REQUIRE (strip2 != nullptr);
   game.LevelInitialize (engine.EdictList (), engine.EdictCount ());
   CHECK (strip2->free == 1);
   game.AddGameFlag (GameFlags::Legacy);

   // spawn entity hook edges
   game.OnSpawnEntity (nullptr);
   edict_t *free_ent = engine.SpawnEntity ("info_target");
   HOST_REQUIRE (free_ent != nullptr);
   free_ent->free = 1;
   game.OnSpawnEntity (free_ent);
   edict_t *blank = engine.Funcs ().pfnCreateEntity ();
   HOST_REQUIRE (blank != nullptr);
   game.OnSpawnEntity (blank);
   game.OnSpawnEntity (engine.EdictList ());
   CHECK (game.GetStartEntity () == engine.EdictList ());

   // spawn counting through the dll hook
   CHECK (game.GetSpawnCount (Team::CT) == 0);
   CHECK (game.GetSpawnCount (Team::Terrorist) == 0);
   table.pfnSpawn (ct_spawn);
   table.pfnSpawn (t_spawn);
   CHECK (game.GetSpawnCount (Team::CT) == 1);
   CHECK (game.GetSpawnCount (Team::Terrorist) == 1);

   table.pfnServerDeactivate ();
   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_bomb") {
   // demolition search + grenade kinds, full prefix
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
   HOST_REQUIRE (bomb_target != nullptr);
   // early false before demolition is known
   edict_t *early_box = engine.SpawnEntity ("weaponbox");
   HOST_REQUIRE (early_box != nullptr);
   early_box->v.model = string_t (engine.AllocString ("models/p_backpack.mdl"));
   testhost::InstallEngineTables (engine);
   CHECK (!game.IsBombEntity (early_box));
   CHECK (!game.IsBombEntity (nullptr));

   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   game.Precache ();
   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
   CHECK (game.MapIs (MapFlags::Demolition));

   // weaponbox backpack still counts
   CHECK (game.IsBombEntity (early_box));
   // grenade needs the c4 model name, non-zero origin avoids empty vector
   const float c4pos[3] = { 10.0f, 20.0f, 30.0f };
   edict_t *c4 = engine.SpawnEntity ("grenade", c4pos);
   HOST_REQUIRE (c4 != nullptr);
   c4->v.model = string_t (engine.AllocString ("models/p_c4.mdl"));
   CHECK (game.IsBombEntity (c4));
   edict_t *he = engine.SpawnEntity ("grenade");
   HOST_REQUIRE (he != nullptr);
   he->v.model = string_t (engine.AllocString ("models/p_hegrenade.mdl"));
   CHECK (!game.IsBombEntity (he));

   // bomb origin search finds the c4
   game_state.RoundStart ();
   game_state.SetBombPlanted (true);
   game_state.SetBombOrigin (false);
   CHECK (game_state.GetBombEntity () == c4);
   CHECK (!game_state.GetBombOrigin ().empty ());

   // explicit pos pins without search
   const ystl::Vector pinned (11.0f, 22.0f, 33.0f);
   game_state.SetBombOrigin (false, pinned);
   CHECK (game_state.GetBombEntity () == nullptr);
   CHECK (game_state.GetBombOrigin ().x == 11.0f);

   // losing the grenade clears the plant
   engine.Funcs ().pfnRemoveEntity (c4);
   game_state.SetBombPlanted (true);
   game_state.SetBombOrigin (false);
   CHECK (!game_state.IsBombPlanted ());
   CHECK (game_state.GetBombEntity () == nullptr);
   CHECK (game_state.GetBombOrigin ().empty ());

   // reset clears too
   game_state.SetBombPlanted (true);
   game_state.SetBombOrigin (true);
   CHECK (!game_state.IsBombPlanted ());

   // active grenade kinds, freed bomb stays out
   edict_t *flash = engine.SpawnEntity ("grenade");
   HOST_REQUIRE (flash != nullptr);
   flash->v.model = string_t (engine.AllocString ("models/p_flashbang.mdl"));
   edict_t *smoke = engine.SpawnEntity ("grenade");
   HOST_REQUIRE (smoke != nullptr);
   smoke->v.model = string_t (engine.AllocString ("models/p_smokegrenade.mdl"));
   edict_t *other = engine.SpawnEntity ("grenade");
   HOST_REQUIRE (other != nullptr);
   other->v.model = string_t (engine.AllocString ("models/p_unknown.mdl"));
   game_state.RoundStart ();
   engine.AdvanceTime (0.6f);
   game_state.UpdateActiveGrenade ();
   CHECK (game_state.HasActiveGrenades ());
   bool saw_flash = false;
   bool saw_he = false;
   bool saw_smoke = false;
   bool saw_other = false;
   for (const auto &g : game_state.GetActiveGrenades ()) {
      if (g.ent == flash && g.kind == GrenadeKind::Flash) {
         saw_flash = true;
      }
      if (g.ent == he && g.kind == GrenadeKind::Explosive) {
         saw_he = true;
      }
      if (g.ent == smoke && g.kind == GrenadeKind::Smoke) {
         saw_smoke = true;
      }
      if (g.ent == other && g.kind == GrenadeKind::Other) {
         saw_other = true;
      }
      CHECK (!game.IsBombEntity (g.ent));
   }
   CHECK (saw_flash && saw_he && saw_smoke && saw_other);

   table.pfnServerDeactivate ();
   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_hud") {
   // long splits + hud + box, full prefix
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   game.Precache ();
   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
   edict_t *real = engine.SpawnClient ("HudReal");
   HOST_REQUIRE (real != nullptr);
   clients.Update ();
   edict_t *fake = game.CreateFakeClient ("HudFake");
   HOST_REQUIRE (fake != nullptr);

   // null origin reads empty
   CHECK (game.GetEntityOrigin (nullptr).empty ());

   // long client message splits into utf8 chunks
   ystl::String long_msg {};
   for (size_t i = 0; i < 200; ++i) {
      long_msg += 'x';
   }
   const size_t msgs_before = engine.Messages ().size ();
   game.SendClientMessage (true, real, long_msg.chars ());
   CHECK (engine.Messages ().size () > msgs_before + 3);
   // fake clients never get messages
   const size_t muted_before = engine.Messages ().size ();
   game.SendClientMessage (true, fake, long_msg.chars ());
   CHECK (engine.Messages ().size () == muted_before);

   // long server message splits too
   const size_t calls_before = engine.Calls ().size ();
   game.SendServerMessage (long_msg.chars ());
   CHECK (engine.Calls ().size () > calls_before);

   // hud with effect 2 writes fx time, long truncates
   hudtextparms_t htp {};
   htp.effect = 2;
   const size_t hud_before = engine.Messages ().size ();
   game.SendHudMessage (real, htp, "hi hud");
   CHECK (engine.Messages ().size () > hud_before);
   ystl::String huge {};
   for (size_t i = 0; i < 600; ++i) {
      huge += 'y';
   }
   game.SendHudMessage (real, htp, huge.chars ());
   CHECK (engine.Messages ().size () > hud_before);
   game.SendHudMessage (nullptr, htp, "nowhere");
   // box null is a no-op, valid draws twelve edges
   const size_t box_mute = engine.Messages ().size ();
   game.DrawBox (nullptr, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (1.0f, 1.0f, 1.0f), 10, 0, ystl::Color (255, 0, 0), 200, 5, 10);
   CHECK (engine.Messages ().size () == box_mute);
   const size_t box_before = engine.Messages ().size ();
   game.DrawBox (real, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (8.0f, 8.0f, 8.0f), 10, 0, ystl::Color (0, 255, 0), 200, 5, 10);
   CHECK (engine.Messages ().size () > box_before + 10);

   table.pfnServerDeactivate ();
   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_command") {
   // bot command parsing edges, full prefix without level
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
   edict_t *cmder = engine.SpawnClient ("Cmder");
   HOST_REQUIRE (cmder != nullptr);

   // quoted say keeps the inner text
   game.Command (cmder, "say \"hi there\"");
   CHECK (cs.HasCall ("ClientCommand"));
   CHECK (!game.IsBotCmd ());
   // quoted non-say joins command and value
   game.Command (cmder, "menuselect \"1\"");
   CHECK (cs.HasCall ("ClientCommand"));
   // multi part with separators
   game.Command (cmder, "say hi; say bye");
   CHECK (cs.HasCall ("ClientCommand"));
   // empty parts are skipped
   game.Command (cmder, "; say hi;;");
   CHECK (cs.HasCall ("ClientCommand"));
   // whitespace say trims, plain token runs alone
   game.Command (cmder, "say hi there");
   CHECK (cs.HasCall ("ClientCommand"));
   game.Command (cmder, "jointeam 1");
   CHECK (cs.HasCall ("ClientCommand"));
   game.Command (cmder, "jointeam");
   CHECK (cs.HasCall ("ClientCommand"));

   // engine command lands in the table
   game.RegisterEngineCommand ("testcmd_xyz2", [] () {});
   bool saw_cmd = false;
   for (size_t i = 0; i < engine.Calls ().size (); ++i) {
      if (engine.Calls ()[i].name == "AddServerCommand" && engine.Calls ()[i].detail == "testcmd_xyz2") {
         saw_cmd = true;
         break;
      }
   }
   CHECK (saw_cmd);

   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_flag [modern]") {
   // version + healthy env, modern prefix without level
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "-modern/cstrike");
   engine.SetDedicatedServer (false);
   engine.SetCvar ("host_hl25_extended_structs", "1");
   engine.SetCvar ("sv_userinfo_transmitted_fields", "1");
   engine.SetCvar ("sv_rcon_whitelist_address", "1");
   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);

   // postload picked up the flags
   CHECK (game.Is (GameFlags::AnniversaryHL25));
   CHECK (game.Is (GameFlags::GoldClient));
   game.ClearGameFlag (GameFlags::AnniversaryHL25);
   game.ClearGameFlag (GameFlags::GoldClient);

   // version strings for each game generation
   game.AddGameFlag (GameFlags::Legacy);
   game.PrintBotVersion ();
   game.ClearGameFlag (GameFlags::Legacy);
   game.AddGameFlag (GameFlags::ConditionZero);
   game.PrintBotVersion ();
   game.ClearGameFlag (GameFlags::ConditionZero);
   game.AddGameFlag (GameFlags::Modern);
   game.PrintBotVersion ();
   game.AddGameFlag (GameFlags::Xash3D);
   game.AddGameFlag (GameFlags::Xash3DLegacy);
   game.AddGameFlag (GameFlags::Mobility);
   game.AddGameFlag (GameFlags::HasBotVoice);
   game.AddGameFlag (GameFlags::ReGameDLL);
   game.AddGameFlag (GameFlags::ReHLDS);
   game.AddGameFlag (GameFlags::HasFakePings);
   game.AddGameFlag (GameFlags::Metamod);
   game.PrintBotVersion ();
   game.ClearGameFlag (GameFlags::Xash3D);
   game.ClearGameFlag (GameFlags::Xash3DLegacy);
   game.ClearGameFlag (GameFlags::Mobility);
   game.ClearGameFlag (GameFlags::HasBotVoice);
   game.ClearGameFlag (GameFlags::ReGameDLL);
   game.ClearGameFlag (GameFlags::ReHLDS);
   game.ClearGameFlag (GameFlags::HasFakePings);
   game.ClearGameFlag (GameFlags::Metamod);
   game.ClearGameFlag (GameFlags::Modern);

   // listen server enables pings when modern and off
   game.AddGameFlag (GameFlags::Modern);
   cv_show_latency.Set (0);
   game.EnsureHealthyGameEnvironment ();
   CHECK (cv_show_latency.As<int> () == 2);
   cv_show_latency.Revert ();
   game.ClearGameFlag (GameFlags::Modern);
   engine.SetDedicatedServer (true);

   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_bounds") {
   // cvar stack + platform names, full prefix without level
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   // anniversary + gold client must exist before postload caches them
   engine.SetCvar ("host_hl25_extended_structs", "1");
   engine.SetDedicatedServer (false);
   engine.SetCvar ("sv_userinfo_transmitted_fields", "1");
   engine.SetCvar ("sv_rcon_whitelist_address", "1");
   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   engine.SetDedicatedServer (true);

   // normal type hits the server flag, revert with no pointer is a no-op
   ConVar tmp_normal ("tmp_normal_xyz", "1", Var::Normal);
   ConVar tmp_revert ("tmp_revert_xyz", "1");
   tmp_revert.Revert ();

   // readonly reverts to init
   engine.SetCvar (cv_version.Name ().chars (), "hacked");
   game.CheckCvarsBounds ();
   CHECK (strcmp (cv_version.As<ystl::StringRef> ().chars (), product.version.chars ()) == 0);

   // min above max swaps
   cv_difficulty_min.Set (3.0f);
   cv_difficulty_max.Set (1.0f);
   game.CheckCvarsBounds ();
   CHECK (cv_difficulty_min.As<float> () == 1.0f);
   CHECK (cv_difficulty_max.As<float> () == 3.0f);
   cv_difficulty_min.Revert ();
   cv_difficulty_max.Revert ();

   // xash legacy forces simulating
   game.AddGameFlag (GameFlags::Xash3DLegacy);
   engine.SetCvar ("sv_forcesimulating", "0");
   game.CheckCvarsBounds ();
   ConVarRef sim ("sv_forcesimulating");
   CHECK (sim.Exists ());
   CHECK (sim.Value<float> () == 1.0f);
   engine.SetCvar ("sv_forcesimulating", "0");
   game.ClearGameFlag (GameFlags::Xash3DLegacy);

   // xash sentinel + metamod + missing + error
   game.AddGameFlag (GameFlags::Xash3D);
   ConVar tmp_xash ("tmp_xash_xyz", "1", "desc", true, 0.0f, 1.0f, Var::NoServer);
   game.RegisterCvars ();
   game.ClearGameFlag (GameFlags::Xash3D);
   game.AddGameFlag (GameFlags::Metamod);
   ConVar tmp_meta ("tmp_meta_xyz", "1");
   game.RegisterCvars ();
   game.ClearGameFlag (GameFlags::Metamod);
   ConVar tmp_missing ("tmp_missing_xyz", nullptr, Var::GameRef, true, "fallback");
   game.RegisterCvars (true);
   CHECK (strcmp (game.FindCvar ("tmp_missing_xyz").chars (), "fallback") == 0);
   ConVar tmp_err ("tmp_err_nonexist_xyz", nullptr, Var::GameRef, false);
   game.RegisterCvars (true);
   CHECK (game.FindCvar ("tmp_err_nonexist_xyz").empty ());

   // platform names
   const bool saved_android = ystl::plat.android;
   const bool saved_arm = ystl::plat.arm;
   const bool saved_x64 = ystl::plat.x64;
   const bool saved_psvita = ystl::plat.psvita;
   const bool saved_ppc = ystl::plat.ppc;
   const bool saved_riscv = ystl::plat.riscv;
   const bool saved_nix = ystl::plat.nix;
   const bool saved_win = ystl::plat.win;
   const bool saved_macos = ystl::plat.macos;
   ystl::plat.android = false;
   ystl::plat.psvita = false;
   ystl::plat.x64 = false;
   ystl::plat.arm = true;
   ystl::plat.ppc = false;
   ystl::plat.riscv = false;
   ystl::SmallArray<ystl::String> arm_libs {};
   game.ConstructCsBinaryName (arm_libs);
   CHECK (arm_libs.size () == 2);
   CHECK (arm_libs[0] == "cs_armv7hf");
   ystl::plat.arm = false;
   ystl::plat.nix = false;
   ystl::plat.win = false;
   ystl::plat.macos = false;
   ystl::SmallArray<ystl::String> i386_libs {};
   game.ConstructCsBinaryName (i386_libs);
   CHECK (i386_libs.size () == 2);
   CHECK (i386_libs[0] == "cs_i386");
   ystl::plat.android = saved_android;
   ystl::plat.arm = saved_arm;
   ystl::plat.x64 = saved_x64;
   ystl::plat.psvita = saved_psvita;
   ystl::plat.ppc = saved_ppc;
   ystl::plat.riscv = saved_riscv;
   ystl::plat.nix = saved_nix;
   ystl::plat.win = saved_win;
   ystl::plat.macos = saved_macos;

   // xash detect + anniversary + gold client, last two pre-seeded before postload
   engine.SetCvar ("build", "1");
   game.DetectXashEngine ();
   CHECK (game.Is (GameFlags::Xash3DLegacy));
   game.ClearGameFlag (GameFlags::Xash3DLegacy);
   game.ClearGameFlag (GameFlags::Xash3D);
   engine.SetCvar ("host_ver", "1");
   game.DetectXashEngine ();
   CHECK (game.Is (GameFlags::Xash3D));
   game.ClearGameFlag (GameFlags::Xash3D);
   CHECK (game.Is25thAnniversaryUpdate ());
   engine.SetCvar ("host_hl25_extended_structs", "0");
   CHECK (!game.Is25thAnniversaryUpdate ());
   CHECK (game.IsGoldClientListenServer ());

   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_probe") {
   // modes + search + frame + shutdown, full prefix
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   // sphere early out before start entity exists
   bool sphere_called = false;
   game.SearchEntities (ystl::Vector (), 100.0f, [&] (edict_t *) {
      sphere_called = true;
      return EntitySearchResult::Continue;
   });
   CHECK (!sphere_called);
   CHECK (!game.IsBombEntity (nullptr));
   CHECK (!game.IsMonsterEntity (nullptr));
   CHECK (!game.IsItemEntity (nullptr));
   CHECK (!game.IsHostageEntity (nullptr));
   CHECK (!game.IsDoorEntity (nullptr));
   edict_t *seedless = engine.SpawnEntity ("func_breakable");
   HOST_REQUIRE (seedless != nullptr);
   seedless->v.health = 50.0f;
   seedless->v.takedamage = DAMAGE_YES;
   seedless->v.movetype = MOVETYPE_PUSH;
   CHECK (!game.IsBreakableEntity (seedless, false));

   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);
   game.Precache ();
   gamefuncs_t table {};
   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
   edict_t *t1 = engine.SpawnEntity ("info_target");
   HOST_REQUIRE (t1 != nullptr);
   edict_t *t2 = engine.SpawnEntity ("info_target");
   HOST_REQUIRE (t2 != nullptr);
   edict_t *cli = engine.SpawnClient ("ProbeCli");
   HOST_REQUIRE (cli != nullptr);
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

   // field skips clients
   int skipped = 0;
   game.SearchEntities ("classname", "player", [&] (edict_t *) {
      ++skipped;
      return EntitySearchResult::Continue;
   });
   CHECK (skipped == 0);
   // sphere break after first
   int wide = 0;
   game.SearchEntities (ystl::Vector (0.0f, 0.0f, 0.0f), 100000.0f, [&] (edict_t *) {
      ++wide;
      return EntitySearchResult::Break;
   });
   CHECK (wide == 1);
   CHECK (game.HasEntityInGame ("info_target"));
   CHECK (!game.HasEntityInGame ("no_such_xyz"));

   // csdm switch
   game.AddGameFlag (GameFlags::Metamod);
   game.AddGameFlag (GameFlags::ReGameDLL);
   cv_csdm_mode.Set (1);
   game.ApplyGameModes ();
   CHECK (game.Is (GameFlags::CSDM));
   CHECK (!game.Is (GameFlags::FreeForAll));
   cv_csdm_mode.Set (2);
   game.ApplyGameModes ();
   CHECK (game.Is (GameFlags::CSDM | GameFlags::FreeForAll));
   cv_csdm_mode.Set (3);
   game.ApplyGameModes ();
   CHECK (!game.Is (GameFlags::CSDM));
   CHECK (!game.Is (GameFlags::FreeForAll));
   cv_csdm_mode.Set (0);
   engine.SetCvar ("csdm_active", "1");
   engine.SetCvar ("mp_freeforall", "1");
   game_state.RoundStart ();
   game.ApplyGameModes ();
   CHECK (game.Is (GameFlags::CSDM));
   CHECK (game.Is (GameFlags::FreeForAll));
   engine.SetCvar ("csdm_active", "0");
   engine.SetCvar ("mp_freeforall", "0");
   // zombie scan with and without delay cvar
   engine.SetCvar ("zp_delay", "0");
   game_state.RoundStart ();
   game.ApplyGameModes ();
   CHECK (!game.Is (GameFlags::ZombieMod));
   engine.SetCvar ("zp_delay", "5");
   engine.AdvanceTime (10.0f);
   game_state.RoundStart ();
   game.ApplyGameModes ();
   CHECK (game.Is (GameFlags::ZombieMod));
   game.ClearGameFlag (GameFlags::ZombieMod);
   game.ClearGameFlag (GameFlags::CSDM);
   game.ClearGameFlag (GameFlags::FreeForAll);
   cv_csdm_mode.Revert ();
   cv_ignore_enemies_after_spawn_time.Revert ();
   game.ClearGameFlag (GameFlags::Metamod);
   game.ClearGameFlag (GameFlags::ReGameDLL);

   // frame without metamod runs ai
   game.Frame ();

   // xash shutdown kicks
   game.AddGameFlag (GameFlags::Xash3D);
   table.pfnServerDeactivate ();
   game.ClearGameFlag (GameFlags::Xash3D);
   testhost::CloseFakeCs (cs);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_postload") {
   // metamod unload path, dedicated prefix
   testhost::FakeEngine engine;
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   game.AddGameFlag (GameFlags::Metamod);
   testhost::FakeCSApi cs {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   // under metamod the gamedll is not entered
   CHECK (cs.entered () == 0);
   CHECK (game.Lib ().handle () == nullptr);
   game.ClearGameFlag (GameFlags::Metamod);
   // android mobility also stops before the gamedll
   game.AddGameFlag (GameFlags::Metamod);
   const bool saved_android = ystl::plat.android;
   ystl::plat.android = true;
   testhost::FakeCSApi cs2 {};
   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs2));
   cs2.reset ();
   // second give uses the same engine tables, still no enter
   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   CHECK (game.Is (GameFlags::Xash3D | GameFlags::Mobility));
   ystl::plat.android = saved_android;
   game.ClearGameFlag (GameFlags::Metamod);
   game.ClearGameFlag (GameFlags::Xash3D);
   game.ClearGameFlag (GameFlags::Mobility);
   game.ClearGameFlag (GameFlags::HasBotVoice);
   game.ClearGameFlag (GameFlags::ReGameDLL);
   testhost::CloseFakeCs (cs);
   testhost::CloseFakeCs (cs2);
   bots.Destroy ();
}

TEST_CASE ("unit/engine_empty") {
   // empty mod name fails the cs load, lightest prefix
   testhost::FakeEngine engine;
   engine.Initialise ("");
   testhost::InstallEngineTables (engine);
   CHECK (!game.LoadCsBinary ());
   bots.Destroy ();
}

} // namespace bot
