//
// YaPB test host: unit/practice_{storage,update}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for practice.cpp: dense/sparse experience storage,
// damage/value/index routing, bot damage hooks, round-end consolidation
// and disk round-trip. Private node indices go through BotPracticeHook
// (friend, tests only); everything else uses the public API.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct PracticeHook {
  static void SetCurrent (Bot &bot, int index) {
    bot.current_node_index_ = index;
  }
  static void SetChosen (Bot &bot, int index) {
    bot.chosen_goal_index_ = index;
  }
  static void SetPrev (Bot &bot, int index) {
    bot.prev_goal_index_ = index;
  }
};

namespace {

// eight-node chain: the storage loader rejects smaller graphs as damaged
// (shared kMaxNodeLinks gate), dense numbering matches indices
void BuildPracticeGraph () {
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

  auto link = [] (int from, int to) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = 100;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 7; ++i) {
    link (i, i + 1);
    link (i + 1, i);
  }

  graph.PopulateNodes ();
  planner.Init ();
}

void BootPractice (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildPracticeGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

// worker runs inline under YB_SINGLE_THREADED, so load/update are sync here
void RebuildVisForPractice () {
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());
}

} // namespace

TEST_CASE ("unit/practice_storage") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootPractice (engine, cs);

  // cold storage answers defaults and swallows writes
  CHECK (practice.GetValue (Team::Terrorist, 1, 1) == 0);
  CHECK (practice.GetDamage (Team::Terrorist, 0, 1) == 0);
  CHECK (practice.GetIndex (Team::Terrorist, 2, 2) == kInvalidNodeIndex);
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 1); // never below one

  practice.SetValue (Team::Terrorist, 1, 1, 300);
  practice.SetDamage (Team::Terrorist, 0, 1, 120);
  practice.SetIndex (Team::Terrorist, 2, 2, 3);
  CHECK (practice.GetValue (Team::Terrorist, 1, 1) == 0);
  CHECK (practice.GetDamage (Team::Terrorist, 0, 1) == 0);
  CHECK (practice.GetIndex (Team::Terrorist, 2, 2) == kInvalidNodeIndex);

  // invalid teams and ranges never store
  practice.Load ();

  // empty storage drops a stale practice file instead of writing it
  const ystl::String stale_path = bstor.BuildPath (StorageFile::Practice);
  ystl::File::make_path (ystl::String (stale_path.substr (0, stale_path.find_last_of (kPathSeparator))).chars ());
  {
    ystl::File stale (stale_path, "wb");
    stale.put_char ('x');
  }
  HOST_REQUIRE (ystl::plat.file_exists (stale_path.chars ()));
  practice.Save ();
  CHECK (!ystl::plat.file_exists (stale_path.chars ()));

  practice.SetValue (Team::Spectator, 1, 1, 300);
  practice.SetValue (Team::Terrorist, -1, 1, 300);
  practice.SetValue (Team::Terrorist, 1, 40, 300);
  practice.SetDamage (Team::Invalid, 0, 1, 120);
  CHECK (practice.GetValue (Team::Spectator, 1, 1) == 0);
  CHECK (practice.GetValue (Team::Terrorist, -1, 1) == 0);
  CHECK (practice.GetValue (Team::Terrorist, 1, 40) == 0);
  CHECK (practice.GetDamage (Team::Invalid, 0, 1) == 0);
  CHECK (practice.GetIndex (Team::Terrorist, 9, 9) == kInvalidNodeIndex);

  // diagonal cells route straight into the dense array
  practice.SetValue (Team::Terrorist, 1, 1, 300);
  practice.SetDamage (Team::Terrorist, 1, 1, 700);
  practice.SetIndex (Team::Terrorist, 1, 1, 3);
  CHECK (practice.GetValue (Team::Terrorist, 1, 1) == 300);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 1) == 700);
  CHECK (practice.GetIndex (Team::Terrorist, 1, 1) == 3);

  // teams are fully isolated
  CHECK (practice.GetValue (Team::CT, 1, 1) == 0);
  CHECK (practice.GetDamage (Team::CT, 1, 1) == 0);
  CHECK (practice.GetIndex (Team::CT, 1, 1) == kInvalidNodeIndex);

  // off-diagonal cells merge field by field into one entry
  practice.SetValue (Team::Terrorist, 0, 1, 50);
  practice.SetDamage (Team::Terrorist, 0, 1, 120);
  practice.SetIndex (Team::Terrorist, 0, 1, 2);
  CHECK (practice.GetValue (Team::Terrorist, 0, 1) == 50);
  CHECK (practice.GetDamage (Team::Terrorist, 0, 1) == 120);
  CHECK (practice.GetIndex (Team::Terrorist, 0, 1) == 2);

  // (1, 0) is a different cell from (0, 1)
  CHECK (practice.GetValue (Team::Terrorist, 1, 0) == 0);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 0) == 0);

  // fully defaulted cells drop out of the sparse map
  practice.SetValue (Team::Terrorist, 0, 1, 0);
  practice.SetDamage (Team::Terrorist, 0, 1, 0);
  practice.SetIndex (Team::Terrorist, 0, 1, kInvalidNodeIndex);
  CHECK (practice.GetValue (Team::Terrorist, 0, 1) == 0);
  CHECK (practice.GetDamage (Team::Terrorist, 0, 1) == 0);
  CHECK (practice.GetIndex (Team::Terrorist, 0, 1) == kInvalidNodeIndex);

  // sparse cells that fall back to default are erased by the setters
  practice.SetValue (Team::Terrorist, 4, 5, 50);
  practice.SetValue (Team::Terrorist, 4, 5, 0);
  CHECK (practice.GetValue (Team::Terrorist, 4, 5) == 0);

  // team damage tracks the per-team maximum, other teams unaffected
  practice.SetTeamDamage (Team::Terrorist, 900);
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 900);
  CHECK (practice.GetTeamDamage (Team::CT) == 1);

  // extended damage optionally adds the team maximum
  practice.SetDamage (Team::Terrorist, 2, 3, 120);
  CHECK (practice.GetDamageEx (Team::Terrorist, 2, 3, false) == 120.0f);
  CHECK (practice.GetDamageEx (Team::Terrorist, 2, 3, true) == 1020.0f);

  // loading with no graph is a no-op
  graph.Reset ();
  practice.Load ();

  bots.Destroy ();
}

TEST_CASE ("unit/practice_update") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootPractice (engine, cs);
  practice.Load ();

  bots.Addbot ("PracVictim", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("PracAttacker", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *victim = testhost::FindBot ("PracVictim");
  Bot *attacker = testhost::FindBot ("PracAttacker");
  HOST_REQUIRE (victim != nullptr && attacker != nullptr);

  edict_t *human = game.CreateFakeClient ("PracHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::CT;
  clients[human].team2 = Team::CT;

  victim->team_ = Team::Terrorist;
  attacker->team_ = Team::CT;
  victim->health_value_ = 100.0f;

  // lethal damage rates the goal down by health / 20
  PracticeHook::SetChosen (*victim, 2);
  PracticeHook::SetPrev (*victim, 1);
  practice.SetValue (Team::Terrorist, 2, 1, 500);
  practice.UpdateValue (victim, 100);
  CHECK (practice.GetValue (Team::Terrorist, 2, 1) == 495);

  // scratches leave the rating alone...
  practice.UpdateValue (victim, 10);
  CHECK (practice.GetValue (Team::Terrorist, 2, 1) == 495);

  // ...as do missing goal indices
  PracticeHook::SetChosen (*victim, kInvalidNodeIndex);
  practice.UpdateValue (victim, 100);
  CHECK (practice.GetValue (Team::Terrorist, 2, 1) == 495);
  PracticeHook::SetChosen (*victim, 2);
  PracticeHook::SetPrev (*victim, kInvalidNodeIndex);
  practice.UpdateValue (victim, 100);
  CHECK (practice.GetValue (Team::Terrorist, 2, 1) == 495);
  PracticeHook::SetPrev (*victim, 1);

  // human fire stores victim-node damage with the human divisor...
  PracticeHook::SetCurrent (*victim, 1);
  CHECK (graph.GetNearest (human->v.origin) == 3);
  practice.UpdateDamage (victim, human, 100);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 14); // 100 / 7
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 14);
  CHECK (victim->goal_value_ == -100.0f);

  // bot fire uses the bot divisor and swings both goal values; the
  // attacker stands on node 0, so a second damage cell appears there
  practice.UpdateDamage (victim, attacker->Ent (), 50);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 0) == 5); // 50 / 10
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 14); // maximum is kept
  CHECK (victim->goal_value_ == -150.0f);
  CHECK (attacker->goal_value_ == 50.0f);

  // friendly fire is a complete no-op
  clients[human].team = Team::Terrorist;
  practice.UpdateDamage (victim, human, 100);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 14);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 0) == 5);
  CHECK (victim->goal_value_ == -150.0f);
  clients[human].team = Team::CT;

  // non-players are ignored as well
  practice.UpdateDamage (victim, game.EntityOfIndex (0), 100);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 14);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 0) == 5);
  CHECK (victim->goal_value_ == -150.0f);

  // small hits are a complete no-op, goal values included
  practice.UpdateDamage (victim, human, 10);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 14);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 0) == 5);
  CHECK (victim->goal_value_ == -150.0f);

  // damage writes wait out the vistable rebuild instead of clobbering
  // history with fresh-only data (reads lie zero while rebuilding)
  practice.SetDamage (Team::Terrorist, 1, 3, 500);
  vistab.StartRebuild ();
  HOST_REQUIRE (!vistab.IsReady ());
  practice.UpdateDamage (victim, human, 100);
  CHECK (victim->goal_value_ == -250.0f); // runtime ranking still tracks
  RebuildVisForPractice ();
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 500);

  // round-end consolidation needs visibility data: rebuild it first
  RebuildVisForPractice ();

  // damage setters erase cells that fall back to default
  practice.SetDamage (Team::Terrorist, 6, 7, 40);
  practice.SetDamage (Team::Terrorist, 6, 7, 0);
  CHECK (practice.GetDamage (Team::Terrorist, 6, 7) == 0);

  practice.SetDamage (Team::Terrorist, 0, 1, 200);
  practice.SetDamage (Team::Terrorist, 0, 2, 1500);
  practice.SetDamage (Team::Terrorist, 3, 3, 1500);
  practice.SetDamage (Team::CT, 4, 5, 1500);
  practice.SetIndex (Team::Terrorist, 2, 2, 3); // stale slot, no damage behind it
  practice.SetTeamDamage (Team::Terrorist, 1500);
  practice.SetTeamDamage (Team::CT, 100);
  practice.Update ();

  // without overflow experience survives the round: typical hits store
  // single digits, so per-round halving would wipe the map in 1-2 rounds
  CHECK (practice.GetDamage (Team::CT, 4, 5) == 1500);

  // ...most dangerous visible source wins the diagonal slot...
  CHECK (practice.GetIndex (Team::Terrorist, 0, 0) == 2);

  // ...slots without danger are always cleared, never left stale...
  CHECK (practice.GetIndex (Team::Terrorist, 2, 2) == kInvalidNodeIndex);

  // ...dense damage is kept as well, team damage still decays every round
  CHECK (practice.GetDamage (Team::Terrorist, 3, 3) == 1500);
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 480); // 1500 - 1020
  CHECK (practice.GetTeamDamage (Team::CT) == 1); // never below one

  // overflow halves stored experience back into range
  practice.SetDamage (Team::CT, 4, 5, 3000); // > kDamage triggers the decay
  practice.SetTeamDamage (Team::Terrorist, 2000);
  practice.SetTeamDamage (Team::CT, 2000);
  practice.Update ();

  CHECK (practice.GetDamage (Team::CT, 4, 5) == 1980); // 3000 - 1020
  CHECK (practice.GetDamage (Team::Terrorist, 0, 2) == 480); // 1500 - 1020
  CHECK (practice.GetDamage (Team::Terrorist, 0, 1) == 0); // 200 - 1020 clamps, cell erased
  CHECK (practice.GetDamage (Team::Terrorist, 3, 3) == 480); // diagonal decays too
  CHECK (practice.GetIndex (Team::CT, 4, 4) == 5); // published before the decay
  CHECK (practice.GetIndex (Team::Terrorist, 0, 0) == 2);
  CHECK (practice.GetTeamDamage (Team::Terrorist) == 980);
  CHECK (practice.GetTeamDamage (Team::CT) == 980);

  // disk round-trip preserves dense and sparse cells; harness-only mirror
  // (prod engine FS bridges the save/load bases, the harness does not)
  const ystl::String prac_path = bstor.BuildPath (StorageFile::Practice);
  const ystl::String prac_load_path = bstor.BuildPath (StorageFile::Practice, true);
  ystl::File::make_path (ystl::String (prac_path.substr (0, prac_path.find_last_of (kPathSeparator))).chars ());
  ystl::File::make_path (ystl::String (prac_load_path.substr (0, prac_load_path.find_last_of (kPathSeparator))).chars ());

  practice.SetValue (Team::Terrorist, 1, 1, 777);
  practice.SetDamage (Team::CT, 2, 3, 333);
  practice.SetIndex (Team::Terrorist, 0, 3, 1);
  practice.Save ();

  ystl::File src (prac_path, "rb");
  ystl::File dst (prac_load_path, "wb");
  HOST_REQUIRE (src.eof () == false);

  for (int ch = src.get (); ch != EOF; ch = src.get ()) {
    dst.put_char (ch);
  }
  src.close ();
  dst.close ();

  practice.Load ();
  CHECK (practice.GetValue (Team::Terrorist, 1, 1) == 777);
  CHECK (practice.GetDamage (Team::CT, 2, 3) == 333);
  CHECK (practice.GetIndex (Team::Terrorist, 0, 3) == 1);

  bots.Destroy ();
}

} // namespace bot
