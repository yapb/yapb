//
// YaPB test host: unit/radio_{orders,chatter}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for radio.cpp through a test-only hook (friend, no
// production behavior changes): order handling with exact state effects,
// and the chatter/frame helpers. Radio chances pin at 100% via the hook,
// literal-chance branches stay out on flake risk.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct RadioHook {
  static RadioChat RadioSelect (Bot &bot) {
    return bot.radio_select_;
  }
  static bool ForceRadio (Bot &bot) {
    return bot.force_radio_;
  }
  static void SetRadioPercent (Bot &bot, int value) {
    bot.radio_percent_ = value;
  }
  static int RadioPercent (Bot &bot) {
    return bot.radio_percent_;
  }
  static edict_t *TargetEntity (Bot &bot) {
    return bot.target_entity_;
  }
  static void SetTargetEntity (Bot &bot, edict_t *ent) {
    bot.target_entity_ = ent;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static size_t QueueLength (Bot &bot) {
    return bot.msg_queue_.size ();
  }
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }

  static void CheckQueue (Bot &bot) {
    bot.CheckRadioQueue ();
  }
  static void PushChat (Bot &bot, RadioChat message) {
    bot.PushRadioChat (message);
  }
  static void ShowIcon (Bot &bot, bool show, bool disconnect) {
    bot.ShowChatterIcon (show, disconnect);
  }
  static void Instant (Bot &bot, RadioChat message) {
    bot.InstantChatter (message);
  }
  static void PlayerKill (Bot &bot, bool team_kill) {
    bot.HandleChatterOnPlayerKill (team_kill);
  }
  static void EnemyDown (Bot &bot) {
    bot.HandleChatterEnemyDown ();
  }
  static void FrameEvents (Bot &bot) {
    bot.ExecuteChatterFrameEvents ();
  }
  static void TaskChange (Bot &bot, TaskId tid) {
    bot.HandleChatterTaskChange (tid);
  }
  static void HeadToward (Bot &bot) {
    bot.TryHeadTowardRadioMessage ();
  }
  static void StartHeadedTimer (Bot &bot) {
    bot.headed_timer_.start ();
  }
  static void DrainQueue (Bot &bot) {
    bot.msg_queue_.clear ();
  }
};

namespace {

// four-node chain, dense numbering matches indices
void BuildRadioGraph () {
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

void BootRadio (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildRadioGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
  cv_radio_mode.Set (2); // no voice bank in the harness, radio leg anyway
}

} // namespace

TEST_CASE ("unit/radio_orders") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("RadB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("RadF1", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("RadF2", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));

  Bot *bot = testhost::FindBot ("RadB");
  Bot *f1 = testhost::FindBot ("RadF1");
  Bot *f2 = testhost::FindBot ("RadF2");
  HOST_REQUIRE (bot != nullptr && f1 != nullptr && f2 != nullptr);

  edict_t *human = game.CreateFakeClient ("RadHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  clients.Update ();

  clients[human].team = Team::Terrorist;
  bot->team_ = Team::Terrorist;
  bot->num_friends_left_ = 1;
  RadioHook::SetRadioPercent (*bot, 100); // every percent roll passes
  RadioHook::DrainQueue (*bot); // ctor leaves a Buy queued

  // busy states refuse orders outright
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->radio_order_ == RadioChat::InvalidSelect);
  CHECK (RadioHook::QueueLength (*bot) == 0);
  bot->ClearTasks ();

  RadioHook::SetCreature (*bot, true);
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->radio_order_ == RadioChat::InvalidSelect);
  RadioHook::SetCreature (*bot, false);

  // cover and stick-together recruit a follower with a task
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->radio_order_ == RadioChat::InvalidSelect);
  CHECK (RadioHook::TargetEntity (*bot) == human);
  CHECK (bot->GetTaskId () == TaskId::FollowUser);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);

  bot->ClearTasks ();
  RadioHook::SetTargetEntity (*bot, nullptr);
  bot->radio_order_ = RadioChat::StickTogetherTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*bot) == human);
  CHECK (bot->GetTaskId () == TaskId::FollowUser);

  // overfull follow prunes back down to the allowed headcount
  RadioHook::SetTargetEntity (*f1, human);
  RadioHook::SetTargetEntity (*f2, human);
  f1->is_alive_ = true;
  f2->is_alive_ = true;
  RadioHook::SetTargetEntity (*bot, nullptr);
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*f1) == nullptr);
  CHECK (RadioHook::TargetEntity (*f2) == human);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);

  // hold position stands the follower down into a pause
  RadioHook::SetTargetEntity (*bot, human);
  bot->radio_order_ = RadioChat::HoldThisPosition;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);
  CHECK (bot->GetTaskId () == TaskId::Pause);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);

  // new round is always acknowledged
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::NewRound;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::YouHeardTheMan);

  // taking fire steadies fear and calls for help...
  bot->ClearTasks ();
  bot->fear_level_ = 0.5f;
  bot->radio_order_ = RadioChat::TakingFireNeedAssistance;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->fear_level_ == Approx (0.3).margin (0.001));
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::OnMyWay);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...unless an enemy is already on, then it's a plain negative
  bot->enemy_ = human;
  bot->radio_order_ = RadioChat::TakingFireNeedAssistance;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Negative);
  bot->enemy_ = nullptr;

  // spotted enemies trim fear the same way
  bot->fear_level_ = 0.5f;
  bot->radio_order_ = RadioChat::EnemySpotted;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->fear_level_ == Approx (0.4).margin (0.001));
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::OnMyWay);

  // matched go-orders stand down with a roger, paused ones move out
  RadioHook::SetTargetEntity (*bot, human);
  bot->fear_level_ = 0.5f;
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  const size_t go_base = RadioHook::QueueLength (*bot);
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);
  CHECK (bot->fear_level_ == Approx (0.3).margin (0.001));
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);
  CHECK (RadioHook::QueueLength (*bot) == go_base + 1);

  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);

  const float moved = bot->position_.distance (human->v.origin);
  CHECK (moved >= 1024.0f);
  CHECK (moved <= 2048.0f);

  // blown bombs send holders packing with an escape task
  bot->ClearTasks ();
  game_state.SetBombPlanted (true);
  bot->radio_order_ = RadioChat::ShesGonnaBlow;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::EscapeFromBomb);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);

  game_state.SetBombPlanted (false);
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::ShesGonnaBlow;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Negative);

  // regrouping knife-rushes the site on a planted bomb...
  game_state.SetBombPlanted (true);
  bot->team_ = Team::CT;
  bot->num_enemies_left_ = 0;
  bot->radio_order_ = RadioChat::RegroupTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  // ...storming emboldens, fallback frightens, both deterministically
  bot->ClearTasks ();
  bot->fear_level_ = 0.5f;
  bot->agression_level_ = 0.4f;
  bot->radio_order_ = RadioChat::StormTheFront;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->fear_level_ == Approx (0.2).margin (0.001));
  CHECK (bot->agression_level_ == Approx (0.7).margin (0.001));
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  bot->ClearTasks ();
  bot->fear_level_ = 0.4f;
  bot->agression_level_ = 0.4f;
  bot->radio_order_ = RadioChat::TeamFallback;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->fear_level_ == Approx (0.9).margin (0.001));
  CHECK (bot->agression_level_ == 0.0f);

  // reports route by current business, move alongs stay quiet
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::ReportingIn);

  bot->ClearTasks ();
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::PlantingBomb);

  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::DefusingBomb);

  bot->ClearTasks ();
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SeekingEnemies);

  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Nothing);

  // sector calls rest without a live bomb, then arm the search cooldown
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::SectorClear;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->radio_order_ == RadioChat::InvalidSelect);

  game_state.SetBombPlanted (true);
  bot->team_ = Team::CT;
  clients[human].team = Team::CT;
  bot->radio_order_ = RadioChat::SectorClear;
  bot->radio_entity_ = f1->Ent ();
  clients.Update ();
  clients[f1->Ent ()].team = Team::CT;
  RadioHook::CheckQueue (*bot);
  CHECK (bots.HasPlantedBombSearchCooldown ());
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  bots.Destroy ();
}

TEST_CASE ("unit/radio_chatter") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("RadC", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("RadC");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("RadEarner");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[human].team = Team::Terrorist;
  bot->team_ = Team::Terrorist;
  bot->num_friends_left_ = 1;
  RadioHook::SetRadioPercent (*bot, 100);

  // voiceless games stay silent through both chatter doors...
  RadioHook::ShowIcon (*bot, true, false);
  RadioHook::ShowIcon (*bot, false, false);
  CHECK (!has_flag (clients[human].icon[bot->Index ()].flags, ClientIcon::Visible));

  RadioHook::Instant (*bot, RadioChat::StormTheFront);
  CHECK (!has_flag (clients[human].icon[bot->Index ()].flags, ClientIcon::Visible));

  // raw pushes respect creatures, loners and mutes on a fresh slate...
  RadioHook::SetCreature (*bot, true);
  RadioHook::PushChat (*bot, RadioChat::StormTheFront);
  CHECK (!RadioHook::ForceRadio (*bot));
  RadioHook::SetCreature (*bot, false);

  bot->num_friends_left_ = 0;
  RadioHook::PushChat (*bot, RadioChat::StormTheFront);
  CHECK (!RadioHook::ForceRadio (*bot));
  bot->num_friends_left_ = 1;

  cv_radio_mode.Set (0);
  RadioHook::PushChat (*bot, RadioChat::StormTheFront);
  CHECK (!RadioHook::ForceRadio (*bot));
  cv_radio_mode.Set (2);

  // ...and queue through the radio leg otherwise
  const size_t radio_base = RadioHook::QueueLength (*bot);
  RadioHook::PushChat (*bot, RadioChat::StormTheFront);
  CHECK (RadioHook::ForceRadio (*bot));
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::StormTheFront);
  CHECK (RadioHook::QueueLength (*bot) == radio_base + 1);

  // ...team kills always report, living kills only tune lightning
  RadioHook::PlayerKill (*bot, true);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::FriendlyFire);

  // enemy counts map straight onto spotted calls
  edict_t *foe1 = game.CreateFakeClient ("RadFoe1");
  edict_t *foe2 = game.CreateFakeClient ("RadFoe2");
  edict_t *foe3 = game.CreateFakeClient ("RadFoe3");
  HOST_REQUIRE (!game.IsNullEntity (foe1) && !game.IsNullEntity (foe2) && !game.IsNullEntity (foe3));

  for (auto foe : { foe1, foe2, foe3 }) {
    foe->v.flags &= ~FL_FAKECLIENT;
    foe->v.health = 0.0f;
    foe->v.deadflag = DEAD_DEAD;
  }
  clients.Update ();
  clients[foe1].team = Team::CT;
  clients[foe2].team = Team::CT;
  clients[foe3].team = Team::CT;

  // victim in hand keeps the tune-up crash-free and clamps to fifteen
  bot->last_victim_ = foe1;
  RadioHook::PlayerKill (*bot, false);
  CHECK (RadioHook::RadioPercent (*bot) == 15);

  // missing victim used to deref through the sniper check on the 72%
  // path (regression: guard falls back to the headcount switch instead)
  bot->last_victim_ = nullptr;

  for (int i = 0; i < 3; ++i) {
    RadioHook::PlayerKill (*bot, false);
  }
  CHECK (RadioHook::RadioPercent (*bot) >= 0);
  CHECK (RadioHook::RadioPercent (*bot) <= 14);

  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::TooManyEnemies);

  foe1->v.health = 100.0f;
  foe1->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[foe1].team = Team::CT;
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedOneEnemy);

  foe2->v.health = 100.0f;
  foe2->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[foe2].team = Team::CT;
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedTwoEnemies);

  foe3->v.health = 100.0f;
  foe3->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[foe3].team = Team::CT;
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedThreeEnemies);

  // frame events rest with nothing to report, then warn the planted site
  const RadioChat idle_select = RadioHook::RadioSelect (*bot);
  RadioHook::FrameEvents (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == idle_select);

  game_state.SetBombPlanted (true);
  bot->team_ = Team::CT;
  RadioHook::FrameEvents (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GottaFindC4);
  CHECK (!bots.HasBombSay (BombPlantedSay::Chatter));
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  // heading needs a live human order-giver, movers never divert
  bot->radio_entity_ = human;
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  const ystl::Vector stayed = bot->position_;
  RadioHook::HeadToward (*bot);
  CHECK (bot->position_ == stayed);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  bot->ClearTasks ();
  RadioHook::StartHeadedTimer (*bot);
  RadioHook::HeadToward (*bot);
  CHECK (bot->position_ == human->v.origin);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  // task-change chatter rolls are pinned per-bot (YSTL_TESTS)
  ystl::RWrand &rng = RadioHook::Rng (*bot);

  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Blind);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Blind);

  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::PlantBomb);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::PlantingBomb);

  // camp rolls route by zone and team
  bot->in_escape_zone_ = false;
  bot->in_rescue_zone_ = false;
  bot->in_vip_zone_ = false;

  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToCamp);

  bot->team_ = Team::CT;
  bot->in_escape_zone_ = true;
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardEscapeZone);
  bot->in_escape_zone_ = false;

  bot->team_ = Team::Terrorist;
  bot->in_rescue_zone_ = true;
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardRescueZone);
  bot->in_rescue_zone_ = false;

  bot->in_vip_zone_ = true;
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardVIPSafety);
  bot->in_vip_zone_ = false;

  // steer player-kill rolls: kill/enemy-down fail, the headcount passes
  rng.set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });

  // a sniper victim takes priority over the headcount
  foe1->v.health = 100.0f;
  foe1->v.deadflag = DEAD_NO;
  foe1->v.weapons = ystl::to_underlying (kSniperWeaponMask);
  bot->last_victim_ = foe1;
  clients.Update ();
  clients[foe1].team = Team::CT;
  RadioHook::PlayerKill (*bot, false);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SniperKilled);

  // then the headcount switch, one case at a time
  bot->last_victim_ = nullptr;

  for (int alive = 0; alive <= 3; ++alive) {
    foe1->v.weapons = 0;
    foe1->v.health = alive >= 1 ? 100.0f : 0.0f;
    foe2->v.health = alive >= 2 ? 100.0f : 0.0f;
    foe3->v.health = alive >= 3 ? 100.0f : 0.0f;
    foe1->v.deadflag = alive >= 1 ? DEAD_NO : DEAD_DEAD;
    foe2->v.deadflag = alive >= 2 ? DEAD_NO : DEAD_DEAD;
    foe3->v.deadflag = alive >= 3 ? DEAD_NO : DEAD_DEAD;
    clients.Update ();
    clients[foe1].team = Team::CT;
    clients[foe2].team = Team::CT;
    clients[foe3].team = Team::CT;

    RadioHook::PlayerKill (*bot, false);

    const RadioChat expected = alive == 1   ? RadioChat::OneEnemyLeft
                               : alive == 2 ? RadioChat::TwoEnemiesLeft
                               : alive == 3 ? RadioChat::ThreeEnemiesLeft
                                            : RadioChat::EnemyDown;
    CHECK (RadioHook::RadioSelect (*bot) == expected);
  }
  rng.clear_forced ();

  // chance branches are deterministic under a fixed seed (100% pinning
  // elsewhere is determinism, not a prod workaround)
  ystl::rg.seed (1234);
  const bool first = ystl::rg.chance (50);
  const int roll = ystl::rg (0, 99);
  ystl::rg.seed (1234);
  CHECK (ystl::rg.chance (50) == first);
  CHECK (ystl::rg (0, 99) == roll);
  ystl::rg.seed ();

  bots.Destroy ();
}

TEST_CASE ("unit/radio_chance") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("RadChance", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("RadChance");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("RadChanceHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[human].team = Team::Terrorist;
  bot->team_ = Team::Terrorist;
  bot->radio_entity_ = human;
  bot->num_friends_left_ = 1;
  RadioHook::SetRadioPercent (*bot, 100);
  RadioHook::DrainQueue (*bot);

  // literal-chance branches are driven deterministically by pinning the
  // per-bot rng (YSTL_TESTS), no seed sweeping. Note: Bot methods use the
  // bot's own ystl::RWrand member, not the global rg.
  ystl::RWrand &rng = RadioHook::Rng (*bot);

  // pass: combat call
  bot->ClearTasks ();
  bot->StartTask (TaskId::Attack, TaskPri::kAttack, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  RadioHook::DrainQueue (*bot);
  rng.force_chance (true);
  RadioHook::CheckQueue (*bot);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::InCombat);

  // fail (mode 2): headcount default with no enemies near
  bot->ClearTasks ();
  bot->StartTask (TaskId::Attack, TaskPri::kAttack, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  RadioHook::DrainQueue (*bot);
  rng.force_chance (false);
  RadioHook::CheckQueue (*bot);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::TooManyEnemies);

  // mode 1 reports a generic enemy-spotted on a failed roll
  bot->ClearTasks ();
  bot->StartTask (TaskId::Attack, TaskPri::kAttack, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  RadioHook::DrainQueue (*bot);

  cv_radio_mode.Set (1);
  rng.force_chance (false);
  RadioHook::CheckQueue (*bot);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::EnemySpotted);

  bots.Destroy ();
}

} // namespace bot
