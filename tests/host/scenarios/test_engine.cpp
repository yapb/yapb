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

} // namespace bot
