//
// YaPB test host: unit/support_{pure,conf,engine}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file unit coverage for support.cpp, three fixture levels:
// pure (no engine at all), conf (postload prefix only), engine
// (full prefix up to ServerActivate for traces/entities/messages).
//

#include <cstdio>

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("unit/support_pure") {
  // team conversions, both directions plus invalid
  CHECK (util.ConvertFromCsTeam (CSTeam::CT) == Team::CT);
  CHECK (util.ConvertFromCsTeam (CSTeam::Terrorist) == Team::Terrorist);
  CHECK (util.ConvertFromCsTeam (CSTeam::Invalid) == Team::Invalid);
  CHECK (util.ConvertFromCsTeam (Team::CT) == CSTeam::CT);
  CHECK (util.ConvertFromCsTeam (Team::Terrorist) == CSTeam::Terrorist);
  CHECK (util.ConvertFromCsTeam (Team::Invalid) == CSTeam::Invalid);

  // handmade eye: facing +X, eyes at z 28, 90-degree fov
  edict_t eye {};
  eye.v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  eye.v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  eye.v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  eye.v.fov = 90.0f;

  // straight ahead: deviation is exactly 1
  CHECK (util.ViewDot (&eye, ystl::Vector (100.0f, 0.0f, 28.0f)) == 1.0f);
  CHECK (util.IsInViewCone (ystl::Vector (100.0f, 0.0f, 28.0f), &eye));

  // behind and to the side: outside the cone
  CHECK (util.ViewDot (&eye, ystl::Vector (-100.0f, 0.0f, 28.0f)) == -1.0f);
  CHECK (!util.IsInViewCone (ystl::Vector (-100.0f, 0.0f, 28.0f), &eye));
  CHECK (!util.IsInViewCone (ystl::Vector (0.0f, 100.0f, 28.0f), &eye));

  // well inside (26 degrees) and well outside (63 degrees) the 45-degree half-cone
  CHECK (util.IsInViewCone (ystl::Vector (100.0f, 50.0f, 28.0f), &eye));
  CHECK (!util.IsInViewCone (ystl::Vector (100.0f, 200.0f, 28.0f), &eye));

  // zero fov falls back to 90 degrees
  eye.v.fov = 0.0f;
  CHECK (util.IsInViewCone (ystl::Vector (100.0f, 0.0f, 28.0f), &eye));
  CHECK (!util.IsInViewCone (ystl::Vector (0.0f, 100.0f, 28.0f), &eye));

  // date formatting: "dd-mm-yyyy hh:mm:ss", always 19 chars
  const ystl::String now = util.GetCurrentDateTime ();
  CHECK (now.size () == 19);
  CHECK (now.chars ()[2] == '-');
  CHECK (now.chars ()[5] == '-');
  CHECK (now.chars ()[10] == ' ');
  CHECK (now.chars ()[13] == ':');
  CHECK (now.chars ()[16] == ':');
}

TEST_CASE ("unit/support_conf") {
  // light prefix only: postload (logger, cvars, cs load), no server lifecycle
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  conf.InitWeapons ();

  // table aliases first, equipment aliases second, "none" fallback last
  CHECK (util.WeaponIdToAlias (Weapon::USP) == "usp");
  CHECK (util.WeaponIdToAlias (Weapon::AK47) == "ak47");
  CHECK (util.WeaponIdToAlias (Weapon::Knife) == "knife");
  CHECK (util.WeaponIdToAlias (Weapon::Flashbang) == "flash");
  CHECK (util.WeaponIdToAlias (Weapon::Explosive) == "hegren");
  CHECK (util.WeaponIdToAlias (Weapon::Smoke) == "sgren");
  CHECK (util.WeaponIdToAlias (Weapon::Armor) == "vest");
  CHECK (util.WeaponIdToAlias (Weapon::ArmorHelm) == "vesthelm");
  CHECK (util.WeaponIdToAlias (Weapon::Defuser) == "defuser");
  CHECK (util.WeaponIdToAlias (Weapon::Invalid) == "none");

  // fake steam ids are off by default: everything is a plain BOT
  CHECK (util.GetFakeSteamId (nullptr) == "BOT");

  // null sentence root keeps built-in defaults, no crash
  util.ApplySentenceDefs (nullptr);

  // a real sentences section replaces the built-in defaults...
  ystl::ConfNode sentences_root {};
  sentences_root.add_item ("custom welcome one");
  sentences_root.add_item ("custom welcome two");
  util.ApplySentenceDefs (&sentences_root);

  // ...but a block-only (no scalar items) section keeps them
  ystl::ConfNode empty_sentences_root {};
  empty_sentences_root.add_block ("ignored");
  util.ApplySentenceDefs (&empty_sentences_root);

  // custom cvar descriptions render without a crash and mention equipment
  util.SetCustomCvarDescriptions ();

  bool found_restricted = false;

  for (const auto &var : game.GetCvars ()) {
    if (strstr (var.name.chars (), "restricted_weapons")) {
      found_restricted = true;
      CHECK (strstr (var.info.chars (), "flash") != nullptr);
      CHECK (strstr (var.info.chars (), "usp") != nullptr);
    }
  }
  CHECK (found_restricted);

  // memory files go through pfnLoadFileForMe like on a real server
  conf.SetupMemoryFiles ();

  // wave durations: missing file is silence, a real file reports its length
  CHECK (util.GetWaveFileDuration ("no_such_chatter_line") == 0.0f);

  // minimal wav fixture: 8000 hz mono 16-bit, 800 samples -> 0.1 s
  ystl::File::make_path ("testdata-wave");

  const char *wav_name = "testdata-wave/tone.wav";
  {
    ystl::File wav {};

    HOST_REQUIRE (wav.open (wav_name, "wb"));

    struct WavHead {
      char riff[4] = { 'R', 'I', 'F', 'F' };
      uint32_t chunk_size = 1636;
      char wave[4] = { 'W', 'A', 'V', 'E' };
      char fmt[4] = { 'f', 'm', 't', ' ' };
      uint32_t subchunk1_size = 16;
      uint16_t audio_format = 1;
      uint16_t num_channels = 1;
      uint32_t sample_rate = 8000;
      uint32_t byte_rate = 16000;
      uint16_t block_align = 2;
      uint16_t bits_per_sample = 16;
      char data_id[4] = { 'd', 'a', 't', 'a' };
      uint32_t data_length = 1600;
    } head {};

    wav.write (&head, sizeof (head));
    wav.close ();
  }
  cv_chatter_path.Set ("testdata-wave");
  const float duration = util.GetWaveFileDuration ("tone");
  CHECK (duration > 0.09f && duration < 0.11f);
  ::remove (wav_name);

  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/support_engine") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  newgamefuncs_t newtable {};
  int new_version = 0;
  HOST_REQUIRE (GetNewDLLFunctions (&newtable, &new_version) != 0);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  // precache resolves the GameRef cvars (mp_footsteps et al.); on a live
  // server the pfnSpawn hook does this before any client connects
  game.Precache ();

  // null entity is never visible
  CHECK (!util.IsVisible (ystl::Vector (100.0f, 0.0f, 0.0f), nullptr));

  // default trace is a clean miss: eye sees the target
  edict_t *eye = engine.SpawnClient ("Eye");
  HOST_REQUIRE (eye != nullptr);
  eye->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);

  const float eye_pos[3] = { 0.0f, 0.0f, 28.0f };
  eye->v.origin.x = eye_pos[0];
  eye->v.origin.y = eye_pos[1];
  eye->v.origin.z = eye_pos[2];

  CHECK (util.IsVisible (ystl::Vector (500.0f, 0.0f, 28.0f), eye));

  // a wall halfway (distinct spot: trace results are cached per position)
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    out->fAllSolid = 0;
    out->fStartSolid = 0;
    out->fInOpen = 1;
    out->fInWater = 0;
    out->flFraction = 0.5f;
    out->vecEndPos.x = (v1[0] + v2[0]) * 0.5f;
    out->vecEndPos.y = (v1[1] + v2[1]) * 0.5f;
    out->vecEndPos.z = (v1[2] + v2[2]) * 0.5f;
    out->pHit = nullptr;
    out->iHitgroup = 0;
  });
  CHECK (!util.IsVisible (ystl::Vector (0.0f, 500.0f, 64.0f), eye));
  engine.SetTraceLineHook (nullptr);

  // fake steam ids: deterministic per name once enabled
  edict_t *bot = engine.SpawnClient ("TestBot");
  HOST_REQUIRE (bot != nullptr);

  // park it far away: later proximity tests must not trip over it
  bot->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);

  cv_enable_fake_steamids.Set (1.0f);

  ystl::String expected {};
  expected.assignf ("STEAM_0:1:%d", ystl::abs (static_cast<int32_t> (ystl::StringRef::fnv1a32 ("TestBot")) & 0xffff00));
  CHECK (util.GetFakeSteamId (bot) == expected.chars ());
  CHECK (util.GetFakeSteamId (bot) == util.GetFakeSteamId (bot));

  cv_enable_fake_steamids.Set (0.0f);
  CHECK (util.GetFakeSteamId (bot) == "BOT");

  // nearest player: two humans, the closer one wins
  const float near_pos[3] = { 100.0f, 0.0f, 0.0f };
  const float far_pos[3] = { 500.0f, 0.0f, 0.0f };
  edict_t *near = engine.SpawnClient ("Near");
  edict_t *far = engine.SpawnClient ("Far");
  HOST_REQUIRE (near != nullptr && far != nullptr);
  near->v.origin.x = near_pos[0];
  far->v.origin.x = far_pos[0];
  clients.Update ();

  CHECK (util.FindNearestPlayer ({ .origin = eye }));
  CHECK (*util.FindNearestPlayer ({ .origin = eye }) == near);

  // cutoff too short: nobody home
  CHECK (!util.FindNearestPlayer ({ .origin = eye, .distance = 50.0f }));

  // the dead are skipped when aliveness is required
  near->v.health = 0.0f;
  near->v.deadflag = DEAD_DEAD;
  clients.Update ();

  CHECK (util.FindNearestPlayer ({ .origin = eye, .alive = true }));
  CHECK (*util.FindNearestPlayer ({ .origin = eye, .alive = true }) == far);

  // bots need real Bot objects behind the edict: fakes never qualify
  CHECK (!util.FindNearestBot ({ .origin = eye }));

  // spray decals: world hit writes TE_WORLDDECAL, no entity index
  const size_t messages_before = engine.Messages ().size ();

  Trace::Result hit {};
  hit.fraction = 0.5f;
  hit.end_pos = ystl::Vector (10.0f, 20.0f, 30.0f);
  hit.hit = nullptr;
  util.DecalTrace (&hit, 5);

  HOST_REQUIRE (engine.Messages ().size () >= messages_before + 6);
  CHECK (engine.Messages ()[messages_before + 1] == "byte=116");

  // solid entity hit with a high decal index: TE_DECALHIGH + entity index
  const float wall_pos[3] = { 50.0f, 0.0f, 0.0f };
  edict_t *wall = engine.SpawnEntity ("func_wall", wall_pos);
  HOST_REQUIRE (wall != nullptr);
  wall->v.solid = SOLID_BSP;

  const size_t decals_before = engine.Messages ().size ();
  hit.hit = wall;
  util.DecalTrace (&hit, 300);

  HOST_REQUIRE (engine.Messages ().size () >= decals_before + 8);
  CHECK (engine.Messages ()[decals_before + 1] == "byte=118");
  CHECK (engine.Messages ()[decals_before + 2] == "coord=10.000000");
  CHECK (engine.Messages ()[decals_before + 5] == "byte=44");

  // dedicated server never sends welcomes
  const size_t commands_before = engine.ServerCommands ().size ();
  util.CheckWelcome ();
  CHECK (engine.ServerCommands ().size () == commands_before);

  // clean fraction and non-solid hits never paint a decal
  const size_t decal_skip_before = engine.Messages ().size ();

  Trace::Result clean {};
  clean.fraction = 1.0f;
  util.DecalTrace (&clean, 5);
  CHECK (engine.Messages ().size () == decal_skip_before);

  edict_t *soft = engine.SpawnEntity ("info_target");
  HOST_REQUIRE (soft != nullptr);

  clean.fraction = 0.5f;
  clean.hit = soft; // not SOLID_BSP / MOVETYPE_PUSHSTEP
  util.DecalTrace (&clean, 5);
  CHECK (engine.Messages ().size () == decal_skip_before);

  // world hit with a high decal index: TE_WORLDDECALHIGH (117)
  const size_t world_high_before = engine.Messages ().size ();
  hit.hit = nullptr;
  util.DecalTrace (&hit, 300);

  HOST_REQUIRE (engine.Messages ().size () >= world_high_before + 6);
  CHECK (engine.Messages ()[world_high_before + 1] == "byte=117");

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/support_welcome") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // listen server: unlocks the welcome path (dedicated servers skip it)
  engine.SetDedicatedServer (false);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  game.Precache ();

  CHECK (!game.IsDedicatedServer ());

  // requested but no local host entity: nothing goes out
  util.SetNeedForWelcome (true);
  const size_t no_host_before = engine.ServerCommands ().size ();
  util.CheckWelcome ();
  CHECK (engine.ServerCommands ().size () == no_host_before);

  // local listen-server host gets the welcome once the timer elapses
  edict_t *local = engine.SpawnClient ("ListenHost");
  HOST_REQUIRE (local != nullptr);
  local->v.health = 100.0f;
  local->v.deadflag = DEAD_NO;
  local->v.movetype = MOVETYPE_WALK;
  game.SetLocalEntity (local);
  clients.Update ();

  util.SetNeedForWelcome (true);
  util.CheckWelcome (); // starts the welcome countdown

  engine.AdvanceTime (mp_freezetime.As<float> () + 3.0f);
  const size_t send_before = engine.ServerCommands ().size ();
  util.CheckWelcome (); // countdown elapsed: speak + chat + hud

  HOST_REQUIRE (engine.ServerCommands ().size () > send_before);
  CHECK (engine.ServerCommands ()[send_before].find ("speak") != ystl::String::InvalidIndex);

  // one-shot: the request is cleared after a successful send
  const size_t after_send = engine.ServerCommands ().size ();
  util.CheckWelcome ();
  CHECK (engine.ServerCommands ().size () == after_send);

  // non-empty graph also reports (needToSendMsg follows the request flag)
  graph.Reset ();

  for (int i = 0; i < 4; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.PopulateNodes ();

  // a foreign author plus a modifier exercises the attribution branch
  graph.info_.author = "Test Author";
  graph.info_.modified = "Test Modder";

  util.SetNeedForWelcome (true);
  util.CheckWelcome ();
  engine.AdvanceTime (mp_freezetime.As<float> () + 3.0f);

  const size_t graph_before = engine.ServerCommands ().size ();
  util.CheckWelcome ();
  CHECK (engine.ServerCommands ().size () > graph_before);

  testhost::CloseFakeCs (cs);
}

} // namespace bot
