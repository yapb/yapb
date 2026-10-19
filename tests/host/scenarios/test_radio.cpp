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
  static void SetStates (Bot &bot, Sense value) {
    bot.states_ = value;
  }
  static void SetLeader (Bot &bot, bool value) {
    bot.is_leader_ = value;
  }
  static void SetMoveToC4 (Bot &bot, bool value) {
    bot.move_to_c4_ = value;
  }
  static void SetBombKnown (Bot &bot, bool value) {
    bot.bomb_known_ = value;
  }
  static bool BombKnown (Bot &bot) {
    return bot.bomb_known_;
  }
  static void SetChatterTime (Bot &bot, RadioChat message, float value) {
    bot.chatter_times_[message] = value;
  }
  static void SetLastEnemy (Bot &bot, edict_t *ent) {
    bot.last_enemy_ = ent;
    bot.last_enemy_origin_ = ent->v.origin;
  }
  static void ClearLastEnemy (Bot &bot) {
    bot.last_enemy_ = nullptr;
    bot.last_enemy_origin_.clear ();
  }
  static edict_t *LastEnemy (Bot &bot) {
    return bot.last_enemy_;
  }
  static void SetDoubleJumpEntity (Bot &bot, edict_t *ent) {
    bot.double_jump_entity_ = ent;
  }
  static edict_t *DoubleJumpEntity (Bot &bot) {
    return bot.double_jump_entity_;
  }
  static void SetKills (Bot &bot, int count) {
    bot.kills_count_ = count;
  }
  static void TouchVictimTimer (Bot &bot) {
    bot.last_victim_timer_.start ();
  }
  static int FoesNear (Bot &bot) {
    return bot.NumEnemiesNear (bot.pev->origin, kInfiniteDistance);
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

  bots.Addbot ("RadB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RadF1", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RadF2", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("RadC", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // an unaware ct stays quiet even though the team wants a planted bomb callout
  RadioHook::SetBombKnown (*bot, false);
  RadioHook::FrameEvents (*bot);
  CHECK (RadioHook::RadioSelect (*bot) != RadioChat::GottaFindC4);
  CHECK (bots.HasBombSay (BombPlantedSay::Chatter));

  // but a ct that learned about the plant warns the team and shares the knowledge
  RadioHook::SetBombKnown (*bot, true);
  RadioHook::FrameEvents (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GottaFindC4);
  CHECK (!bots.HasBombSay (BombPlantedSay::Chatter));
  CHECK (bots.IsBombKnown (Team::CT));
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
  engine.AdvanceTime (2.0f); // let the team-speech floor clear between callouts
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardEscapeZone);
  bot->in_escape_zone_ = false;

  bot->team_ = Team::Terrorist;
  bot->in_rescue_zone_ = true;
  engine.AdvanceTime (2.0f);
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardRescueZone);
  bot->in_rescue_zone_ = false;

  bot->in_vip_zone_ = true;
  engine.AdvanceTime (2.0f);
  rng.force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  rng.clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardVIPSafety);
  bot->in_vip_zone_ = false;

  // steer player-kill rolls: kill/enemy-down fail, the headcount passes
  // (headcount chance is teamwork-scaled now, pin max teamwork so it clears the hook)
  bot->teamwork_level_ = 1.0f;
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

  bots.Addbot ("RadChance", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

TEST_CASE ("unit/radio_odds") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("OddB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("OddF", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("OddB");
  Bot *mate = testhost::FindBot ("OddF");
  HOST_REQUIRE (bot != nullptr && mate != nullptr);

  auto make_foe = [&] (const char *name, const ystl::Vector &origin) -> edict_t * {
    edict_t *foe = game.CreateFakeClient (name);

    if (game.IsNullEntity (foe)) {
      return nullptr;
    }
    foe->v.flags &= ~FL_FAKECLIENT;
    foe->v.health = 100.0f;
    foe->v.deadflag = DEAD_NO;
    foe->v.solid = SOLID_BBOX;
    foe->v.takedamage = DAMAGE_YES;
    foe->v.origin = origin;
    clients.Update ();
    clients[foe].team = Team::CT;
    return foe;
  };

  edict_t *human = game.CreateFakeClient ("OddHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  edict_t *f1 = make_foe ("OddF1", ystl::Vector (300.0f, 0.0f, 0.0f));
  edict_t *f2 = make_foe ("OddF2", ystl::Vector (250.0f, 0.0f, 0.0f));
  edict_t *f3 = make_foe ("OddF3", ystl::Vector (350.0f, 0.0f, 0.0f));
  edict_t *f4 = make_foe ("OddF4", ystl::Vector (400.0f, 0.0f, 0.0f));
  HOST_REQUIRE (f1 != nullptr && f2 != nullptr && f3 != nullptr && f4 != nullptr);

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->team_ = Team::Terrorist;
  mate->team_ = Team::Terrorist;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  clients[mate->Ent ()].team = Team::Terrorist;
  bot->num_friends_left_ = 1;
  RadioHook::DrainQueue (*bot);

  // ...zeroed percent answers nothing on an open recruit...
  RadioHook::SetRadioPercent (*bot, 0);
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->radio_order_ == RadioChat::InvalidSelect);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  CHECK (RadioHook::QueueLength (*bot) == 0);

  // ...an exact headcount answers negative instead of recruiting...
  mate->is_alive_ = true;
  RadioHook::SetTargetEntity (*mate, human);
  RadioHook::SetTargetEntity (*bot, nullptr);
  bot->ClearTasks ();
  RadioHook::SetRadioPercent (*bot, 100);
  bot->radio_order_ = RadioChat::CoverMe;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Negative);
  RadioHook::SetTargetEntity (*mate, nullptr);
  RadioHook::DrainQueue (*bot);

  // ...taking fire with an enemy on stays silent on zeroed percent...
  bot->enemy_ = f1;
  bot->radio_order_ = RadioChat::TakingFireNeedAssistance;
  bot->radio_entity_ = human;
  RadioHook::SetRadioPercent (*bot, 0);
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::QueueLength (*bot) == 0);
  bot->enemy_ = nullptr;

  // ...a stranded backup call answers negative...
  human->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  bot->enemy_ = f1;
  RadioHook::SetMoveToC4 (*bot, true);
  RadioHook::SetRadioPercent (*bot, 100);
  bot->radio_order_ = RadioChat::NeedBackup;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Negative);
  RadioHook::SetMoveToC4 (*bot, false);
  bot->enemy_ = nullptr;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);

  // ...matched go-orders stand down with a roger, paused ones move out...
  RadioHook::SetTargetEntity (*bot, human);
  bot->fear_level_ = 0.5f;
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::TargetEntity (*bot) == nullptr);
  CHECK (bot->fear_level_ == Approx (0.3).margin (0.001));

  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  // ...unseen go-orders route through the double jump...
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // nothing is visible past halfway
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  human->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  RadioHook::SetDoubleJumpEntity (*bot, human);
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);
  CHECK (RadioHook::DoubleJumpEntity (*bot) == nullptr);

  // ...and go nowhere without one on full percent...
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::GoGoGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Negative);
  engine.SetTraceLineHook (nullptr);
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);

  // ...campers on a blown bomb clear out first...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  game_state.SetBombPlanted (true);
  bot->radio_order_ = RadioChat::ShesGonnaBlow;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::EscapeFromBomb);
  game_state.SetBombPlanted (false);
  bot->ClearTasks ();

  // ...storming from a pause banks extra time...
  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::StormTheFront;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  bot->ClearTasks ();

  // ...fallback from a camp banks time, from the field scans enemies...
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  bot->fear_level_ = 0.1f;
  bot->agression_level_ = 0.9f;
  bot->radio_order_ = RadioChat::TeamFallback;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->fear_level_ == Approx (0.6).margin (0.001));
  CHECK (bot->agression_level_ == Approx (0.4).margin (0.001));

  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::TeamFallback;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::LastEnemy (*bot) == f2); // nearest to the ordering human

  bots.Destroy ();
}

TEST_CASE ("unit/radio_report") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("RepB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("RepB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("RepHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  auto make_foe = [&] (const char *name, const ystl::Vector &origin) -> edict_t * {
    edict_t *foe = game.CreateFakeClient (name);

    if (game.IsNullEntity (foe)) {
      return nullptr;
    }
    foe->v.flags &= ~FL_FAKECLIENT;
    foe->v.health = 100.0f;
    foe->v.deadflag = DEAD_NO;
    foe->v.solid = SOLID_BBOX;
    foe->v.takedamage = DAMAGE_YES;
    foe->v.origin = origin;
    clients.Update ();
    clients[foe].team = Team::CT;
    return foe;
  };

  edict_t *f1 = make_foe ("RepF1", ystl::Vector (300.0f, 0.0f, 0.0f));
  edict_t *f2 = make_foe ("RepF2", ystl::Vector (250.0f, 0.0f, 0.0f));
  HOST_REQUIRE (f1 != nullptr && f2 != nullptr);

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->team_ = Team::Terrorist;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  bot->num_friends_left_ = 1;
  RadioHook::SetRadioPercent (*bot, 100);
  RadioHook::DrainQueue (*bot);

  // ...normal reports route by the current goal flags...
  graph.paths_[2].flags = ystl::to_underlying (NodeFlag::Goal);
  bot->has_c4_ = true;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, 2, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToPlantBomb);

  bot->has_c4_ = false;
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Nothing);

  graph.paths_[2].flags = ystl::to_underlying (NodeFlag::Camp);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToCamp);

  graph.paths_[2].flags = 0; // no goal flags at all
  RadioHook::SetStates (*bot, Sense::HearingEnemy);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::HeardTheEnemy);
  RadioHook::SetStates (*bot, Sense::Invalid);

  // ...goalless normals just report in...
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::ReportingIn);

  // ...movers answer on a pinned roll, campers name their watch...
  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::CheckQueue (*bot);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToCamp);

  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::Rng (*bot).force_chance (false);
  RadioHook::DrainQueue (*bot);
  RadioHook::CheckQueue (*bot);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::QueueLength (*bot) == 0);

  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  game_state.SetBombPlanted (true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GuardingPlantedC4);
  game_state.SetBombPlanted (false);

  bot->in_escape_zone_ = true;
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  bot->team_ = Team::CT;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GuardingEscapeZone);
  bot->in_escape_zone_ = false;
  bot->team_ = Team::Terrorist;

  bot->in_vip_zone_ = true;
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GuardingVIPSafety);
  bot->in_vip_zone_ = false;

  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Camping);
  game_state.SetBombPlanted (false);
  bot->ClearTasks ();

  // ...planters and defusers name their business...
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

  // ...hiders seek, idlers stay quiet...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SeekingEnemies);

  bot->ClearTasks ();
  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SeekingEnemies);

  bot->ClearTasks ();
  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, 0.0f, true);
  bot->radio_order_ = RadioChat::ReportInTeam;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Nothing);

  // ...sector calls adopt the reported site...
  graph.paths_[2].flags = ystl::to_underlying (NodeFlag::Goal);
  graph.PopulateNodes ();
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, 2, 0.0f, true);
  game_state.SetBombPlanted (true);
  bot->team_ = Team::CT;
  clients[human].team = Team::CT;
  clients.Update ();
  bot->radio_order_ = RadioChat::SectorClear;
  bot->radio_entity_ = human;
  human->v.flags |= FL_FAKECLIENT; // sector calls come from radio-carrying bots
  RadioHook::CheckQueue (*bot);
  CHECK (bots.HasPlantedBombSearchCooldown ());
  human->v.flags &= ~FL_FAKECLIENT;
  clients[human].team = Team::Terrorist;
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  // ...take-position orders camp and move out...
  bot->ClearTasks ();
  bot->radio_order_ = RadioChat::GetInPositionAndWaitForGo;
  bot->radio_entity_ = human;
  RadioHook::CheckQueue (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::RogerThat);

  bots.Destroy ();
}

TEST_CASE ("unit/radio_kill") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // demolition flag for the camping callouts, before activate
  edict_t *target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (target != nullptr);

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

  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.InitQuota ();
  cv_quota.Set (10);
  cv_radio_mode.Set (2);

  bots.Addbot ("KilB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("KilB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("KilHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  auto make_foe = [&] (const char *name, const ystl::Vector &origin) -> edict_t * {
    edict_t *foe = game.CreateFakeClient (name);

    if (game.IsNullEntity (foe)) {
      return nullptr;
    }
    foe->v.flags &= ~FL_FAKECLIENT;
    foe->v.health = 100.0f;
    foe->v.deadflag = DEAD_NO;
    foe->v.solid = SOLID_BBOX;
    foe->v.takedamage = DAMAGE_YES;
    foe->v.origin = origin;
    clients.Update ();
    clients[foe].team = Team::CT;
    return foe;
  };

  edict_t *f1 = make_foe ("KilF1", ystl::Vector (300.0f, 0.0f, 0.0f));
  edict_t *f2 = make_foe ("KilF2", ystl::Vector (250.0f, 0.0f, 0.0f));
  edict_t *f3 = make_foe ("KilF3", ystl::Vector (350.0f, 0.0f, 0.0f));
  edict_t *f4 = make_foe ("KilF4", ystl::Vector (400.0f, 0.0f, 0.0f));
  HOST_REQUIRE (f1 != nullptr && f2 != nullptr && f3 != nullptr && f4 != nullptr);

  auto kill_foes = [&] (int live_count) {
    edict_t *foes[] = { f1, f2, f3, f4 };

    for (int i = 0; i < 4; ++i) {
      const bool live = i < live_count;
      foes[i]->v.health = live ? 100.0f : 0.0f;
      foes[i]->v.deadflag = live ? DEAD_NO : DEAD_DEAD;
    }
    clients.Update ();
  };

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->team_ = Team::Terrorist;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  bot->num_friends_left_ = 1;
  RadioHook::DrainQueue (*bot);

  // ...task changes announce blind and plant work...
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Blind);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Blind);

  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::PlantBomb);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::PlantingBomb);

  // ...camping callouts name the watch...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  game_state.SetBombPlanted (true);
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GuardingPlantedC4);

  game_state.SetBombPlanted (false);
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToCamp);

  bot->in_escape_zone_ = true;
  bot->team_ = Team::CT;
  engine.AdvanceTime (2.0f); // let the team-speech floor clear between callouts
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardEscapeZone);
  bot->in_escape_zone_ = false;
  bot->team_ = Team::Terrorist;

  bot->in_rescue_zone_ = true;
  engine.AdvanceTime (2.0f);
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardRescueZone);
  bot->in_rescue_zone_ = false;

  bot->in_vip_zone_ = true;
  engine.AdvanceTime (2.0f);
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::TaskChange (*bot, TaskId::Camp);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GoingToGuardVIPSafety);
  bot->in_vip_zone_ = false;
  bot->ClearTasks ();

  // ...team kills apologize, clean kills tune the radio...
  RadioHook::PlayerKill (*bot, true);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::FriendlyFire);

  RadioHook::SetRadioPercent (*bot, 10);
  RadioHook::Rng (*bot).force_int (3);
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioPercent (*bot) == 7);

  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();

  // ...sniped victims call out the sniper...
  // headcount chance is teamwork-scaled now, pin max teamwork so it clears the hook
  bot->teamwork_level_ = 1.0f;
  bot->last_victim_ = f1;
  f1->v.weapons = ystl::to_underlying (kSniperWeaponMask);
  CHECK (bot->last_victim_ == f1);
  CHECK (!game.IsNullEntity (bot->last_victim_));
  RadioHook::Rng (*bot).set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SniperKilled);
  f1->v.weapons = 0;
  bot->last_victim_ = nullptr;

  // ...headcounts scale the callout...
  kill_foes (1);
  RadioHook::Rng (*bot).set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::OneEnemyLeft);

  kill_foes (2);
  RadioHook::Rng (*bot).set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::TwoEnemiesLeft);

  kill_foes (3);
  RadioHook::Rng (*bot).set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::ThreeEnemiesLeft);

  kill_foes (4); // full headcount hits the default leg
  CHECK (RadioHook::FoesNear (*bot) == 4);
  RadioHook::Rng (*bot).set_chance_hook ([] (int32_t percent) {
    return percent >= 80;
  });
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::EnemyDown);

  // ...quiet streaks reset, hot streaks roll on...
  kill_foes (0);
  RadioHook::Rng (*bot).force_chance (false);
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();

  RadioHook::SetKills (*bot, 3);
  RadioHook::TouchVictimTimer (*bot);
  RadioHook::Rng (*bot).force_chance (false);
  RadioHook::PlayerKill (*bot, false);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::OnARoll);
  kill_foes (4);

  // ...ct carriers knife up for a planted bomb...
  bot->team_ = Team::CT;
  bot->num_enemies_left_ = 0;
  bot->current_weapon_ = Weapon::AK47;
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (100.0f, 0.0f, 0.0f));
  kill_foes (0);
  bot->IssueCommand ("say hello");
  RadioHook::PlayerKill (*bot, false);
  CHECK (game.BotArgc () == 0); // knife select clears the command buffer
  game_state.SetBombPlanted (false);
  game_state.SetBombOrigin (true);
  bot->team_ = Team::Terrorist;
  kill_foes (4);

  // ...enemy-down counts heads...
  kill_foes (1);
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedOneEnemy);

  kill_foes (2);
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedTwoEnemies);

  kill_foes (3);
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpottedThreeEnemies);

  kill_foes (4);
  RadioHook::EnemyDown (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::TooManyEnemies);
  kill_foes (0);

  // ...framed chatter spots the bomber, the pack and the planted bomb...
  kill_foes (4);
  bot->team_ = Team::Terrorist;
  human->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f); // out of friend census
  clients.Update ();
  RadioHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->enemy_ = f1;
  f1->v.health = 100.0f;
  f1->v.deadflag = DEAD_NO;
  clients.Update ();
  f1->v.weapons |= ystl::bit (Weapon::C4);
  engine.AdvanceTime (2.0f); // clear the speech floor left by earlier callouts
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::FrameEvents (*bot);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SpotTheBomber);
  f1->v.weapons &= ~ystl::bit (Weapon::C4);

  // team-wide shouts are serialized now, let the floor go before the next callout
  engine.AdvanceTime (2.0f);
  RadioHook::Rng (*bot).force_chance (true);
  RadioHook::FrameEvents (*bot);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::ScaredEmotion);

  // a lone sniper (not a group) to isolate the sniper callout
  kill_foes (1);
  engine.AdvanceTime (2.0f);
  RadioHook::Rng (*bot).force_chance (true);
  f1->v.weapons |= ystl::bit (Weapon::AWP);
  RadioHook::FrameEvents (*bot);
  RadioHook::Rng (*bot).clear_forced ();
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::SniperWarning);
  f1->v.weapons &= ~ystl::bit (Weapon::AWP);
  bot->enemy_ = nullptr;
  RadioHook::SetStates (*bot, Sense::Invalid);

  game_state.SetBombPlanted (true);
  bot->team_ = Team::CT;
  RadioHook::SetBombKnown (*bot, true);
  RadioHook::FrameEvents (*bot);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::GottaFindC4);
  CHECK (!bots.HasBombSay (BombPlantedSay::Chatter));
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  // ...head-toward orders walk the queue states...
  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  RadioHook::HeadToward (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  bot->ClearTasks ();
  bot->has_c4_ = true;
  RadioHook::HeadToward (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  bot->has_c4_ = false;

  bots.Addbot ("KilM", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *mate = testhost::FindBot ("KilM");
  HOST_REQUIRE (mate != nullptr);
  mate->team_ = Team::Terrorist;
  mate->is_alive_ = true;
  mate->pev->health = 100.0f;
  mate->pev->deadflag = DEAD_NO;
  clients.Update ();
  clients[mate->Ent ()].team = Team::Terrorist;

  bot->ClearTasks ();
  RadioHook::StartHeadedTimer (*bot);
  bot->radio_entity_ = mate->Ent (); // bots keep their fake-client flag
  RadioHook::SetRadioPercent (*bot, 100);
  RadioHook::HeadToward (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  bot->ClearTasks ();
  RadioHook::StartHeadedTimer (*bot);
  bot->radio_entity_ = human;
  RadioHook::HeadToward (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  // ...creatures and loners never push...
  RadioHook::SetCreature (*bot, true);
  RadioHook::DrainQueue (*bot);
  RadioHook::PushChat (*bot, RadioChat::CoverMe);
  CHECK (RadioHook::QueueLength (*bot) == 0);
  RadioHook::SetCreature (*bot, false);

  bot->num_friends_left_ = 0;
  RadioHook::DrainQueue (*bot);
  RadioHook::PushChat (*bot, RadioChat::CoverMe);
  CHECK (RadioHook::QueueLength (*bot) == 0);
  bot->num_friends_left_ = 1;

  bots.Destroy ();
}

TEST_CASE ("unit/radio_voice") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootRadio (engine, cs);

  bots.Addbot ("VoxB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VoxB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("VoxHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  edict_t *stranger = game.CreateFakeClient ("VoxStranger");
  HOST_REQUIRE (!game.IsNullEntity (stranger));
  stranger->v.flags &= ~FL_FAKECLIENT;
  stranger->v.health = 100.0f;
  stranger->v.deadflag = DEAD_NO;
  stranger->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[stranger].team = Team::CT;

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  bot->num_friends_left_ = 1;

  // voice banks need a real wave plus a chatter file on the vfs
  game.AddGameFlag (GameFlags::HasBotVoice);

  const ystl::String chatter_file = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, ystl::String ("chatter.cfg"));
  testhost::MakeDirs (chatter_file.substr (0, chatter_file.find_last_of ("/\\")));

  auto wave_path = [&] (const char *name) {
    return ystl::strings.join_path (cv_chatter_path.As<ystl::StringRef> (), ystl::String (name) + ".wav");
  };

  auto write_wave = [&] (const char *name) {
    const ystl::String path = wave_path (name);
    testhost::MakeDirs (path.substr (0, path.find_last_of ("/\\")));

    ystl::String wav {};
    wav += "RIFF";

    auto push_le32 = [&] (uint32_t value) {
      wav += static_cast<char> (value & 0xffu);
      wav += static_cast<char> ((value >> 8) & 0xffu);
      wav += static_cast<char> ((value >> 16) & 0xffu);
      wav += static_cast<char> ((value >> 24) & 0xffu);
    };

    auto push_le16 = [&] (uint32_t value) {
      wav += static_cast<char> (value & 0xffu);
      wav += static_cast<char> ((value >> 8) & 0xffu);
    };

    push_le32 (8036u);
    wav += "WAVE";
    wav += "fmt ";
    push_le32 (16u);
    push_le16 (1u);
    push_le16 (1u);
    push_le32 (8000u);
    push_le32 (8000u);
    push_le16 (1u);
    push_le16 (8u);
    wav += "data";
    push_le32 (8000u);

    for (int i = 0; i < 8000; ++i) {
      wav += static_cast<char> (128);
    }
    ystl::File file (path, "wb");
    HOST_REQUIRE (!!file);
    file.write (wav.chars (), wav.size (), 1);
    file.close ();
  };

  write_wave ("voxok1");
  write_wave ("voxok2");

  ystl::String chatter_text = "RewritePath = sound/radio/bot\nCoverMe {\n  voxok1\n}\nDiePain {\n  voxok2\n}\n";
  {
    ystl::File file (chatter_file, "wb");
    HOST_REQUIRE (!!file);
    file.write (chatter_text.chars (), chatter_text.size (), 1);
    file.close ();
  }
  conf.LoadChatterConfig ();
  HOST_REQUIRE (conf.HasChatterBank (RadioChat::CoverMe));
  HOST_REQUIRE (conf.HasChatterBank (RadioChat::DiePain));

  const int own = bot->Index ();

  // ...teammates get voice packets, strangers and the dormant get none...
  stranger->v.flags |= FL_DORMANT;
  clients.Update ();

  size_t voice_base = engine.Messages ().size ();
  RadioHook::ShowIcon (*bot, true, false);
  CHECK (engine.Messages ().size () > voice_base);

  // (...strangers and the dormant never do... proven by icon flags staying
  // clear where InstantChatter would set them; ShowIcon itself only sends...)
  CHECK (!has_flag (clients[stranger].icon[own].flags, ClientIcon::Visible));
  stranger->v.flags &= ~FL_DORMANT;
  clients.Update ();

  // ...spectators watching our team get them too...
  stranger->v.iuser2 = game.IndexOfEntity (bot->Ent ());
  clients.Update ();
  voice_base = engine.Messages ().size ();
  RadioHook::ShowIcon (*bot, true, false);
  CHECK (engine.Messages ().size () > voice_base);
  stranger->v.iuser2 = 0;
  clients.Update ();

  // ...hiding clears it once the timer lapses...
  clients[human].icon[own].flags |= ClientIcon::Visible;
  clients[human].icon[own].timestamp = game.Time () - 1.0f;
  RadioHook::ShowIcon (*bot, false, false);
  CHECK (!has_flag (clients[human].icon[own].flags, ClientIcon::Visible));

  // ...disconnects clear unconditionally...
  clients[human].icon[own].flags |= ClientIcon::Visible;
  clients[human].icon[own].timestamp = game.Time () + 100.0f;
  RadioHook::ShowIcon (*bot, false, true);
  CHECK (!has_flag (clients[human].icon[own].flags, ClientIcon::Visible));

  // ...instant chatter speaks alive and dying...
  RadioHook::Instant (*bot, RadioChat::CoverMe);
  CHECK (has_flag (clients[human].icon[own].flags, ClientIcon::Visible));

  bot->pev->deadflag = DEAD_DYING;
  RadioHook::Instant (*bot, RadioChat::CoverMe);
  bot->pev->deadflag = DEAD_NO;

  // ...voice pushes arm the repeat timer, spam goes invalid...
  RadioHook::DrainQueue (*bot);
  RadioHook::PushChat (*bot, RadioChat::CoverMe);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::CoverMe);
  CHECK (RadioHook::QueueLength (*bot) == 1);

  RadioHook::PushChat (*bot, RadioChat::CoverMe);
  CHECK (RadioHook::RadioSelect (*bot) == RadioChat::Invalid);
  CHECK (RadioHook::QueueLength (*bot) == 1);

  game.ClearGameFlag (GameFlags::HasBotVoice);
  ystl::plat.remove_file (chatter_file.chars ());
  ystl::plat.remove_file (wave_path ("voxok1").chars ());
  ystl::plat.remove_file (wave_path ("voxok2").chars ());

  bots.Destroy ();
}

} // namespace bot
