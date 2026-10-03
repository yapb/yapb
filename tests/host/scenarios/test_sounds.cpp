//
// YaPB test host: unit/sounds_{classify,sim}.
//
// SPDX-License-Identifier: Unlicense
//
// Coverage for sounds.cpp through the public API only (no production-code
// changes): sample classification with exact radii, player/world
// attribution, noise refresh/expire rules, simulated client noises and
// the audibility fade math.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BuildSoundGraph () {
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
  planner.Init ();
}

void BootSounds (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildSoundGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/sounds_classify") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootSounds (engine, cs);

  bots.Addbot ("SndB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("SndB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("SndHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f); // zero origins resolve nowhere
  clients.Update ();

  Client &bclient = clients[bot->Ent ()];
  bclient.noise = ClientNoise {};

  // unknown, empty and null samples never touch the client
  sounds.Acquire (bot->Ent (), "foo/bar.wav", 1.0f, 1.0f);
  sounds.Acquire (bot->Ent (), "", 1.0f, 1.0f);
  sounds.Acquire (nullptr, "weapons/m4a1-1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.last == 0.0f);

  // zero origin resolves to no position at all
  edict_t *flat = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (flat != nullptr);
  sounds.Acquire (flat, "weapons/m4a1-1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.last == 0.0f);

  // weapon fire catch-all with standard attenuation math
  const float zero_pos[3] = { 0.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (flat, zero_pos);
  sounds.Acquire (bot->Ent (), "weapons/m4a1-1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.type == Noise::WeaponFire);
  CHECK (bclient.noise.dist == 2048.0f * 0.8f);
  CHECK (bclient.noise.pos == ystl::Vector (100.0f, 0.0f, 0.0f));
  CHECK (bclient.noise.last == game.Time () + 2.0f);

  // ordering beats the catch-all: planted c4 is Defuse, not fire
  sounds.Acquire (bot->Ent (), "weapons/c4_explode1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.type == Noise::Defuse);
  CHECK (bclient.noise.dist == 4096.0f * 0.8f);

  // flashbangs count as explosions, beeps keep their own radius
  // (active louder noise wins, so each probe starts isolated)
  bclient.noise = ClientNoise {};
  sounds.Acquire (bot->Ent (), "weapons/flashbang-1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.type == Noise::Explosion);

  bclient.noise = ClientNoise {};
  sounds.Acquire (bot->Ent (), "weapons/c4_beep1.wav", 1.0f, 1.0f);
  CHECK (bclient.noise.type == Noise::Defuse);
  CHECK (bclient.noise.dist == kBombHearDistance * 0.8f);

  // signature samples across the whole database
  struct Expect {
    const char *sample;
    Noise type;
    float radius;
  };
  const Expect table[] = {
    { "weapons/explode3.wav",   Noise::Explosion,  2048.0f },
    { "weapons/sg_explode.wav", Noise::SGDetonate, 1024.0f },
    { "weapons/ric2.wav",       Noise::Ricochet,   1024.0f },
    { "weapons/zoo.wav",        Noise::Zoom,       512.0f  },
    { "weapons/knife_hit.wav",  Noise::Misc,       512.0f  },
    { "player/bhit_flesh.wav",  Noise::HitFall,    768.0f  },
    { "player/pl_step1.wav",    Noise::Footstep,   1280.0f },
    { "items/gunpickup2.wav",   Noise::Pickup,     768.0f  },
    { "items/9mmclip1.wav",     Noise::Ammo,       512.0f  },
    { "hostage/hos1.wav",       Noise::Hostage,    1024.0f },
    { "doors/doormove1.wav",    Noise::Door,       1024.0f },
    { "debris/bust1.wav",       Noise::Broke,      1024.0f },
  };

  for (const auto &row : table) {
    bclient.noise = ClientNoise {};
    sounds.Acquire (bot->Ent (), row.sample, 1.0f, 1.0f);
    CHECK (bclient.noise.type == row.type);
    CHECK (bclient.noise.dist == row.radius * 0.8f);
  }

  // zero attenuation quadruples, volume scales linearly
  bclient.noise = ClientNoise {};
  sounds.Acquire (bot->Ent (), "weapons/m4a1-1.wav", 1.0f, 0.0f);
  CHECK (bclient.noise.dist == 2048.0f * 4.0f);
  bclient.noise = ClientNoise {};
  sounds.Acquire (bot->Ent (), "weapons/m4a1-1.wav", 0.5f, 1.0f);
  CHECK (bclient.noise.dist == 2048.0f * 0.8f * 0.5f);

  // world sounds attribute to the nearest living client
  Client &hclient = clients[human];
  hclient.noise = ClientNoise {};
  bclient.noise = ClientNoise {};

  static const float near_human[3] = { 490.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (flat, near_human);
  sounds.Acquire (flat, "player/pl_step1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::Footstep);
  CHECK (bclient.noise.last == 0.0f);

  // nobody alive, nobody hears it
  human->v.health = 0.0f;
  human->v.deadflag = DEAD_DEAD;
  bot->pev->health = 0.0f;
  bot->pev->deadflag = DEAD_DEAD;
  clients.Update ();
  hclient.noise = ClientNoise {};
  sounds.Acquire (flat, "player/pl_step1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.last == 0.0f);

  // loud active noise wins, same type refreshes, expiry replaces
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  clients.Update ();
  hclient.noise = ClientNoise {};

  sounds.Acquire (human, "weapons/explode3.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::Explosion);

  engine.AdvanceTime (0.5f);
  sounds.Acquire (human, "player/pl_step1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::Explosion); // dimmer rival ignored
  CHECK (hclient.noise.dist == 2048.0f * 0.8f); // still sustained

  const float before = hclient.noise.last;
  sounds.Acquire (human, "weapons/explode4.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.last > before); // same type refreshes the timer

  engine.AdvanceTime (5.0f);
  sounds.Acquire (human, "player/pl_step1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::Footstep); // expired, replaced

  // template swap replaces the whole database at once
  ystl::Array<SoundTemplate> custom {};
  custom.push (SoundTemplate { "test/gun", Noise::WeaponFire, 100.0f, 1.0f });
  sounds.ApplyTemplates (ystl::move (custom));

  hclient.noise = ClientNoise {};
  sounds.Acquire (human, "test/gun1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::WeaponFire);
  CHECK (hclient.noise.dist == 100.0f * 0.8f);
  sounds.Acquire (human, "weapons/m4a1-1.wav", 1.0f, 1.0f);
  CHECK (hclient.noise.type == Noise::WeaponFire); // unknown sample ignored, previous noise kept
  CHECK (hclient.noise.dist == 100.0f * 0.8f);

  bots.Destroy ();
}

TEST_CASE ("unit/sounds_sim") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootSounds (engine, cs);

  bots.Addbot ("SndS", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("SndS");
  HOST_REQUIRE (bot != nullptr);

  bot->pev->health = 100.0f;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  Client &client = clients[bot->Ent ()];

  // out-of-range clients are ignored
  sounds.SimulateNoise (-1);
  sounds.SimulateNoise (game.MaxClients ());

  // trigger pulls beat everything else on the same tick
  client.noise = ClientNoise {};
  bot->pev->button = IN_ATTACK | IN_USE;
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::WeaponFire);
  CHECK (client.noise.dist == 2048.0f);
  CHECK (client.noise.last == game.Time () + 0.3f);
  CHECK (client.noise.pos == bot->pev->origin);

  // use and reload fall back to misc
  client.noise = ClientNoise {};
  bot->pev->button = IN_USE;
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Misc);
  CHECK (client.noise.dist == 512.0f);

  client.noise = ClientNoise {};
  bot->pev->button = IN_RELOAD;
  bot->pev->oldbuttons = 0;
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Misc);

  // ladders rattle only when actually moving
  client.noise = ClientNoise {};
  bot->pev->button = 0;
  bot->pev->movetype = MOVETYPE_FLY;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 100.0f);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Misc);
  CHECK (client.noise.dist == 1024.0f);

  client.noise = ClientNoise {};
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 10.0f);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.last == 0.0f);

  // footsteps scale linearly with planar speed
  client.noise = ClientNoise {};
  bot->pev->movetype = MOVETYPE_WALK;
  bot->pev->velocity = ystl::Vector (260.0f, 0.0f, 0.0f);
  mp_footsteps.Set (1);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Footstep);
  CHECK (client.noise.dist == 1280.0f);

  client.noise = ClientNoise {};
  bot->pev->velocity = ystl::Vector (130.0f, 0.0f, 0.0f);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.dist == 640.0f);

  // disabled footsteps stay silent, active loud noise is kept
  client.noise = ClientNoise {};
  mp_footsteps.Set (0);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.last == 0.0f);
  mp_footsteps.Set (1);

  // a quieter active noise is overridden by a louder one on the same tick
  client.noise = ClientNoise {};
  bot->pev->movetype = MOVETYPE_WALK;
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->button = IN_USE;
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.dist == 512.0f);

  bot->pev->button = 0;
  bot->pev->velocity = ystl::Vector (260.0f, 0.0f, 0.0f);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Footstep);
  CHECK (client.noise.dist == 1280.0f);

  sounds.Acquire (bot->Ent (), "weapons/explode3.wav", 1.0f, 1.0f);
  engine.AdvanceTime (0.1f);
  bot->pev->velocity = ystl::Vector (130.0f, 0.0f, 0.0f);
  sounds.SimulateNoise (game.IndexOfPlayer (bot->Ent ()));
  CHECK (client.noise.type == Noise::Explosion);

  // audibility fades linearly after a 40% sustain
  const float now = game.Time ();
  ClientNoise fade {};
  fade.dist = 1000.0f;
  fade.start = now;
  fade.last = now + 10.0f;
  CHECK (fade.CurrentRange () == 1000.0f);

  engine.AdvanceTime (3.0f);
  CHECK (fade.CurrentRange () == 1000.0f);
  engine.AdvanceTime (4.0f);
  CHECK (fade.CurrentRange () == Approx (500.0).margin (0.01));
  engine.AdvanceTime (3.0f);
  CHECK (fade.CurrentRange () == 0.0f);
  engine.AdvanceTime (1.0f);
  CHECK (fade.CurrentRange () == 0.0f);

  ClientNoise instant {};
  instant.dist = 777.0f;
  instant.start = now;
  instant.last = now;
  CHECK (instant.CurrentRange () == 777.0f);

  bots.Destroy ();
}

} // namespace bot
