//
// YaPB test host: unit/fakeping_calc.
//
// SPDX-License-Identifier: Unlicense
//
// Coverage for fakeping.cpp through the public manager API only (no
// production-code changes): feature gating, base ranges, ping math,
// update throttling and scoreboard emission.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct TestHook {
  static void SetPingBase (Bot &bot, int value) {
    bot.ping_base_ = value;
  }
};

namespace {

void BuildPingGraph () {
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

void BootPing (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "-modern/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, cs));
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
  BuildPingGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/fakeping_calc [modern]") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootPing (engine, cs);

  // feature needs the capability flag plus full latency display
  cv_show_latency.Set (0);
  CHECK (!fakeping.HasFeature ());
  cv_show_latency.Set (1);
  CHECK (!fakeping.HasFeature ());
  cv_show_latency.Set (2);
  CHECK (fakeping.HasFeature ());

  // base rolls stay inside the configured bounds
  const int lo = cv_ping_base_min.As<int> ();
  const int hi = cv_ping_base_max.As<int> ();

  for (int i = 0; i < 10; ++i) {
    const int base = fakeping.RandomBase ();
    CHECK (base >= lo);
    CHECK (base <= hi);
  }

  bots.Addbot ("PingB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("PingB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("PingHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  clients.Update ();

  // averaged math keeps every bot inside the displayable band
  cv_ping_count_real_players.Set (0);
  bot->ping_ = 9999;
  fakeping.SyncCalculate ();
  CHECK (bot->ping_.load () >= 1);
  CHECK (bot->ping_.load () <= 40);

  // human averaging takes the same path when enabled
  cv_ping_count_real_players.Set (1);
  bot->ping_ = 9999;
  fakeping.SyncCalculate ();
  CHECK (bot->ping_.load () >= 1);
  CHECK (bot->ping_.load () <= 40);
  cv_ping_count_real_players.Set (0);

  // throttled path stays quiet without the feature...
  cv_show_latency.Set (0);
  bot->ping_ = 9999;
  fakeping.Calculate ();
  CHECK (bot->ping_.load () == 9999);

  // ...and recomputes once enabled (timers start elapsed)
  cv_show_latency.Set (2);
  fakeping.Calculate ();
  CHECK (bot->ping_.load () != 9999);

  // reset and emit drive the scoreboard bit-packer without crashing
  clients.Update ();
  fakeping.Reset (human);
  fakeping.Emit (bot->Ent ());
  fakeping.Emit (human);
  CHECK (bot->ping_.load () >= 1);

  // timer restart arm the throttle interval exactly
  fakeping.RestartTimer ();
  bot->ping_ = 9999;
  fakeping.Calculate (); // too early, nothing happens
  CHECK (bot->ping_.load () == 9999);
  engine.AdvanceTime (cv_ping_updater_interval.As<float> () + 0.1f);
  fakeping.Calculate ();
  CHECK (bot->ping_.load () != 9999);

  cv_show_latency.Set (0);

  // emit and reset are no-ops without the feature
  fakeping.Emit (bot->Ent ());
  fakeping.Reset (human);

  // out-of-band pings are re-rolled back into range
  cv_show_latency.Set (2);
  TestHook::SetPingBase (*bot, -1000);
  fakeping.SyncCalculate ();
  CHECK (bot->ping_.load () >= 1);

  TestHook::SetPingBase (*bot, 1000);
  fakeping.SyncCalculate ();
  CHECK (bot->ping_.load () >= 1);

  // averaging with no real players falls back to a random base
  human->v.flags |= FL_FAKECLIENT;
  clients.Update ();
  cv_ping_count_real_players.Set (1);
  fakeping.SyncCalculate ();
  CHECK (bot->ping_.load () >= 1);
  cv_ping_count_real_players.Set (0);

  cv_show_latency.Set (0);

  bots.Destroy ();
}

} // namespace bot
