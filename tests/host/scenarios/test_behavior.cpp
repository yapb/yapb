//
// YaPB test host: unit/behavior_{queue,senses,damage,map,frame,logic}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for behavior.cpp through a test-only hook (friend,
// no production behavior changes): message queue fan-out and dispatch,
// senses setup, hearing, damage intake and round helpers. Randomness is
// pinned via exact modes and single-candidate setups, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct BehaviorHook {
  static ystl::Deque<Msg> &Queue (Bot &bot) {
    return bot.msg_queue_;
  }
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }
  static void AvoidGrenades (Bot &bot) {
    bot.AvoidGrenades ();
  }
  static void AimDirection (Bot &bot) {
    bot.SetAimDirection ();
  }
  static bool AvoidFlashActive (Bot &bot) {
    return !bot.avoid_flash_timer_.elapsed ();
  }
  static void Update (Bot &bot) {
    bot.Update ();
  }
  static ystl::Vector MoveAngles (Bot &bot) {
    return bot.move_angles_;
  }
  static void IntegrateWalk (Bot &bot) {
    // the fake engine drops move commands, integrate them manually (open field)
    ystl::Vector fwd {}, right {};
    bot.move_angles_.angle_vectors (&fwd, &right, nullptr);
    bot.pev->velocity = fwd * bot.move_speed_ + right * bot.strafe_speed_;
    bot.pev->velocity.z = 0.0f;
  }
  static void ForceStarted (Bot &bot) {
    bot.not_started_ = false; // skip the menu join flow, teams are pinned above
  }
  static void StartBlind (Bot &bot, float duration) {
    bot.blind_timer_.start (duration);
  }
  static bool AvoidFlashFlagged (Bot &bot) {
    return has_flag (bot.aim_flags_, AimFlags::Flash);
  }
  static void SetStates (Bot &bot, Sense states) {
    bot.states_ = states;
  }
  static void SetRadioSelect (Bot &bot, RadioChat value) {
    bot.radio_select_ = value;
  }
  static RadioChat RadioSelect (Bot &bot) {
    return bot.radio_select_;
  }
  static void SetForceRadio (Bot &bot, bool value) {
    bot.force_radio_ = value;
  }
  static RadioChat RadioOrder (Bot &bot) {
    return bot.radio_order_;
  }
  static edict_t *RadioEntity (Bot &bot) {
    return bot.radio_entity_;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static void SetCurrent (Bot &bot, int index) {
    bot.current_node_index_ = index;
  }
  static Sense States (Bot &bot) {
    return bot.states_;
  }
  static float DamageTimestamp (Bot &bot) {
    return bot.last_damage_timestamp_;
  }
  static edict_t *HeardEnemy (Bot &bot) {
    return bot.heard_enemy_;
  }
  static int SoundMemoryCount (Bot &bot) {
    int count = 0;

    for (const auto &entry : bot.sound_memory_) {
      if (!game.IsNullEntity (entry.source)) {
        ++count;
      }
    }
    return count;
  }
  static bool BuyTimerStarted (Bot &bot) {
    return bot.item_check_timer_.started ();
  }
  static edict_t *TargetEntity (Bot &bot) {
    return bot.target_entity_;
  }
  static void CheckQueue (Bot &bot) {
    bot.CheckMsgQueue ();
  }
  static void ReactionTimers (Bot &bot, bool actual) {
    bot.SetIdealReactionTimers (actual);
  }
  static bool IgnoredItem (Bot &bot, edict_t *ent) {
    return bot.IsIgnoredItem (ent);
  }
  static ystl::Vector CampDirection (Bot &bot, const ystl::Vector &dest) {
    return bot.GetCampDirection (dest);
  }
  static int CampButtons (Bot &bot) {
    return bot.camp_buttons_;
  }
  static void SetCampButtons (Bot &bot, int buttons) {
    bot.camp_buttons_ = buttons;
  }
  static void SelectCamp (Bot &bot, int index) {
    bot.SelectCampButtons (index);
  }
  static float ShiftSpeed (Bot &bot) {
    return bot.GetShiftSpeed ();
  }
  static bool HeavyWeight (Bot &bot) {
    return bot.CanRunHeavyWeight ();
  }
  static void Hearing (Bot &bot) {
    bot.UpdateHearing ();
  }
  static bool Hostage (Bot &bot) {
    return bot.HasHostage ();
  }
  static bool BombTimer (Bot &bot) {
    return bot.IsOutOfBombTimer ();
  }
  static void SetEscapedFromBomb (Bot &bot, bool value) {
    bot.escaped_from_bomb_ = value;
  }
  static void Donate (Bot &bot) {
    bot.DonateC4ToHuman ();
  }
  static void Follow (Bot &bot) {
    bot.DecideFollowUser ();
  }
  static void Damage (Bot &bot, edict_t *inflictor, int damage, int armor, int bits) {
    bot.TakeDamage (inflictor, damage, armor, bits);
  }
  static void Emotions (Bot &bot) {
    bot.UpdateEmotions ();
  }
  static void Override (Bot &bot) {
    bot.OverrideConditions ();
  }
  static void Conditions (Bot &bot) {
    bot.SetConditions ();
  }
  static void PushHostage (Bot &bot, edict_t *ent) {
    bot.hostages_.push (ent);
  }
  static void CheckBreakable (Bot &bot, edict_t *touch) {
    bot.CheckBreakable (touch);
  }
  static void SetBreakable (Bot &bot, edict_t *ent) {
    bot.breakable_entity_ = ent;
  }
  static void Logic (Bot &bot) {
    bot.Logic ();
  }
  static bool ItemTimerStarted (Bot &bot) {
    return bot.item_check_timer_.started ();
  }
  static void DebugMsg (Bot &bot, const char *text) {
    bot.DebugMsgInternal (text);
  }
  static ystl::Vector CampDir (Bot &bot, const ystl::Vector &dest) {
    return bot.GetCampDirection (dest);
  }
  static void ShowOverlay (Bot &bot) {
    bot.ShowDebugOverlay ();
  }
  static void CheckParachute (Bot &bot) {
    bot.CheckParachute ();
  }
  static void DropWeapon (Bot &bot, edict_t *user, bool discard) {
    bot.DropWeaponForUser (user, discard);
  }
  static void DoubleJump (Bot &bot, edict_t *ent) {
    bot.StartDoubleJump (ent);
  }
  static void Blind (Bot &bot, int alpha) {
    bot.TakeBlind (alpha);
  }
  static void SetEnemy (Bot &bot, edict_t *ent) {
    bot.enemy_ = ent;
    bot.enemy_origin_ = ent->v.origin;
    bot.aim_flags_ |= AimFlags::Enemy;
  }
  static edict_t *Enemy (Bot &bot) {
    return bot.enemy_;
  }
  static void RefreshModel (Bot &bot, char *model) {
    bot.RefreshCreatureStatus (model);
  }
  static void SetInfectedTeam (Bot &bot, bool value) {
    bot.is_on_infected_team_ = value;
  }
  static bool IsCreatureBot (Bot &bot) {
    return bot.IsCreature ();
  }
  static bool IsDefusing (Bot &bot, const ystl::Vector &pos) {
    return bot.IsBombDefusing (pos);
  }
  static bool MoveToGoalState (Bot &bot) {
    return bot.move_to_goal_;
  }
  static bool WantsFire (Bot &bot) {
    return bot.wants_to_fire_;
  }
  static bool UsingGrenade (Bot &bot) {
    return bot.is_using_grenade_;
  }
};

namespace {

// eight-node chain for practice-backed paths (loader needs >= 8)
void BuildBehaviorGraph () {
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

void BootBehavior (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildBehaviorGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();
}

} // namespace

TEST_CASE ("unit/behavior_queue") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("MsgA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("MsgB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *a = testhost::FindBot ("MsgA");
  Bot *b = testhost::FindBot ("MsgB");
  HOST_REQUIRE (a != nullptr && b != nullptr);

  // say fans out to same-state listeners with a fresh chat timer...
  // (ctor leaves a Buy message queued, drain first for exact counts)
  BehaviorHook::Queue (*a).clear ();
  BehaviorHook::Queue (*b).clear ();
  a->is_alive_ = true;
  b->is_alive_ = true;
  b->say_text_buffer_.entity_index = -1;
  a->PushMsgQueue (Msg::Say);
  CHECK (b->say_text_buffer_.entity_index == a->Index ());
  CHECK (b->say_text_buffer_.time_next_chat > game.Time ());
  CHECK (BehaviorHook::Queue (*a).size () == 1);

  // ...but never crosses the alive boundary
  b->is_alive_ = false;
  b->say_text_buffer_.entity_index = -1;
  a->PushMsgQueue (Msg::Say);
  CHECK (b->say_text_buffer_.entity_index == -1);

  // other messages queue up silently
  BehaviorHook::Queue (*a).clear ();
  a->PushMsgQueue (Msg::Buy);
  CHECK (BehaviorHook::Queue (*a).size () == 1);

  // dry buys resolve to pending without touching the wallet flow
  // (the spawn buy timer always outlives pump, let it lapse first)
  engine.AdvanceTime (3.0f);
  a->in_buy_zone_ = false;
  BehaviorHook::CheckQueue (*a);
  CHECK (a->buy_pending_);
  CHECK (a->buying_finished_);
  CHECK (BehaviorHook::Queue (*a).empty ());

  // none and creature-radio drain with no effects
  a->PushMsgQueue (Msg::None);
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).empty ());

  BehaviorHook::SetCreature (*a, true);
  a->PushMsgQueue (Msg::Radio);
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).empty ());
  BehaviorHook::SetCreature (*a, false);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_senses") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("SenseB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("SenseB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("SenseHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[human].team = Team::CT;
  clients[human].team2 = Team::CT;
  bot->team_ = Team::Terrorist;

  // reaction timers: daddy zeroes, actual copies, roll stays in range
  bot->SetNewDifficulty (Difficulty::Hard);
  const auto *tweaks = conf.GetDifficultyTweaks (Difficulty::Hard);
  HOST_REQUIRE (tweaks != nullptr);
  BehaviorHook::ReactionTimers (*bot, true);
  CHECK (bot->ideal_reaction_time_ == tweaks->reaction[0]);
  CHECK (bot->actual_reaction_time_ == tweaks->reaction[0]);

  cv_whose_your_daddy.Set (1);
  BehaviorHook::ReactionTimers (*bot, false);
  CHECK (bot->ideal_reaction_time_ == 0.05f);
  CHECK (bot->actual_reaction_time_ == 0.095f);
  cv_whose_your_daddy.Set (0);

  BehaviorHook::ReactionTimers (*bot, false);
  CHECK (bot->ideal_reaction_time_ >= tweaks->reaction[0]);
  CHECK (bot->ideal_reaction_time_ <= tweaks->reaction[1]);

  // ignore list is a plain membership check
  CHECK (!BehaviorHook::IgnoredItem (*bot, human));
  bot->ignored_items_.push (human);
  CHECK (BehaviorHook::IgnoredItem (*bot, human));
  CHECK (!BehaviorHook::IgnoredItem (*bot, bot->Ent ()));
  bot->ignored_items_.clear ();
  CHECK (!BehaviorHook::IgnoredItem (*bot, human));

  // clear world falls back to nothing without practice data...
  BehaviorHook::SetCurrent (*bot, kInvalidNodeIndex);
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (700.0f, 0.0f, 0.0f)).empty ());

  // ...and to the danger index once experience exists
  BehaviorHook::SetCurrent (*bot, 1);
  practice.SetIndex (Team::Terrorist, 1, 1, 2);
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (700.0f, 0.0f, 0.0f)) == ystl::Vector (200.0f, 0.0f, 0.0f));

  // camp buttons follow fear against the visibility table
  graph.paths_[0].vis.stand = 5;
  graph.paths_[0].vis.crouch = 2;
  bot->personality_ = Personality::Rusher;
  bot->fear_level_ = 0.9f;
  bot->agression_level_ = 0.1f;
  bot->pev->health = 100.0f;
  BehaviorHook::SetCampButtons (*bot, 0);
  BehaviorHook::SelectCamp (*bot, 0);
  CHECK (!!(BehaviorHook::CampButtons (*bot) & IN_DUCK));

  bot->fear_level_ = 0.1f;
  bot->agression_level_ = 0.9f;
  BehaviorHook::SelectCamp (*bot, 0);
  CHECK (!(BehaviorHook::CampButtons (*bot) & IN_DUCK));

  bot->personality_ = Personality::Normal;
  bot->pev->health = 50.0f;
  BehaviorHook::SelectCamp (*bot, 0);
  CHECK (!!(BehaviorHook::CampButtons (*bot) & IN_DUCK));

  graph.paths_[0].vis.crouch = 7;
  BehaviorHook::SelectCamp (*bot, 0);
  CHECK (!(BehaviorHook::CampButtons (*bot) & IN_DUCK));

  // shift speed drops only when everything is quiet
  bot->num_enemies_left_ = 0;
  CHECK (BehaviorHook::ShiftSpeed (*bot) == bot->pev->maxspeed);
  bot->num_enemies_left_ = 5;
  CHECK (BehaviorHook::ShiftSpeed (*bot) == bot->pev->maxspeed * 0.4f);

  // heavy weight gates on its own timer (let the spawn roll elapse first)
  engine.AdvanceTime (0.2f);
  CHECK (BehaviorHook::HeavyWeight (*bot));
  CHECK (!BehaviorHook::HeavyWeight (*bot));

  // hearing: muted globally, silent world, single fire, defuse preference
  cv_ignore_enemies.Set (1);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == nullptr);
  cv_ignore_enemies.Set (0);

  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == nullptr);
  CHECK (BehaviorHook::SoundMemoryCount (*bot) == 0);

  Client &hclient = clients[human];
  hclient.noise.dist = 5000.0f;
  hclient.noise.last = game.Time () + 10.0f;
  hclient.noise.start = game.Time ();
  hclient.noise.pos = human->v.origin;
  hclient.noise.type = Noise::WeaponFire;

  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == human);
  CHECK (BehaviorHook::SoundMemoryCount (*bot) == 1);

  edict_t *human2 = game.CreateFakeClient ("SenseHuman2");
  HOST_REQUIRE (!game.IsNullEntity (human2));
  human2->v.flags &= ~FL_FAKECLIENT;
  human2->v.health = 100.0f;
  human2->v.origin = ystl::Vector (210.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human2].team = Team::CT;

  Client &hclient2 = clients[human2];
  hclient2.noise.dist = 5000.0f;
  hclient2.noise.last = game.Time () + 10.0f;
  hclient2.noise.start = game.Time ();
  hclient2.noise.pos = human2->v.origin;
  hclient2.noise.type = Noise::Defuse;

  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == human2); // 12100 * 0.5 < 40000 * 0.8

  // round helpers stay quiet off their maps
  cv_ignore_objectives.Set (1);
  CHECK (!BehaviorHook::Hostage (*bot));
  cv_ignore_objectives.Set (0);
  CHECK (!BehaviorHook::Hostage (*bot));
  CHECK (!BehaviorHook::BombTimer (*bot));

  // c4 goes to a nearby human teammate when carried
  clients[human].team = Team::Terrorist;
  bot->has_c4_ = false;
  CHECK (!BehaviorHook::BuyTimerStarted (*bot));
  BehaviorHook::Donate (*bot);
  CHECK (!BehaviorHook::BuyTimerStarted (*bot));

  bot->has_c4_ = true;
  BehaviorHook::Donate (*bot);
  CHECK (BehaviorHook::BuyTimerStarted (*bot));
  bot->has_c4_ = false;

  // follow needs a visible human teammate, otherwise nothing sticks
  clients[human].team = Team::CT;
  BehaviorHook::Follow (*bot);
  CHECK (BehaviorHook::TargetEntity (*bot) == nullptr);
  clients[human].team = Team::Terrorist; // same team now, still seen
  BehaviorHook::Follow (*bot);
  CHECK (BehaviorHook::TargetEntity (*bot) == human);
  CHECK (bot->GetTaskId () == TaskId::FollowUser);

  // conditions aggregate a frame: kill credit, dead-enemy forget, census
  // (a dedicated fresh bot keeps stale orders, timers and enemies out;
  // the leftover live edict counts as a teammate so lookup stays blind)
  bots.Addbot ("CondB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *cond = testhost::FindBot ("CondB");
  HOST_REQUIRE (cond != nullptr);
  cond->team_ = Team::Terrorist;

  clients[human].team = Team::CT;
  clients[bot->Ent ()].team = Team::Terrorist;
  bot->pev->health = 0.0f;
  bot->pev->deadflag = DEAD_DEAD;
  cond->last_victim_ = human;
  cond->agression_level_ = 0.0f;
  cond->last_enemy_ = human;
  human->v.health = 0.0f;
  human->v.deadflag = DEAD_DEAD;
  human2->v.health = 0.0f;
  human2->v.deadflag = DEAD_DEAD;
  clients.Update ();
  BehaviorHook::Conditions (*cond);
  CHECK (cond->agression_level_ == 0.1f); // enemy-team victim credited
  CHECK (cond->last_victim_ == nullptr);
  CHECK (cond->last_enemy_ == nullptr); // dead and cold, forgotten
  CHECK (cond->num_enemies_left_ == 0);
  CHECK (cond->num_friends_left_ == 0);

  // emotions decay toward the base when calm, spike on fresh contact
  bot->base_agression_level_ = 0.4f;
  bot->base_fear_level_ = 0.6f;
  bot->agression_level_ = 0.4f;
  bot->fear_level_ = 0.6f;
  BehaviorHook::Emotions (*bot);
  CHECK (bot->agression_level_ == 0.4f);
  CHECK (bot->fear_level_ == 0.6f);

  bot->see_enemy_timer_.start ();
  bot->agression_level_ = 0.4f;
  bot->emotion_update_timer_.invalidate ();
  BehaviorHook::Emotions (*bot);
  CHECK (bot->agression_level_ == Approx (0.45).margin (0.001));

  engine.AdvanceTime (6.0f);
  bot->agression_level_ = 0.9f;
  bot->fear_level_ = 0.9f;
  bot->emotion_update_timer_.invalidate ();
  BehaviorHook::Emotions (*bot);
  CHECK (bot->agression_level_ == Approx (0.85).margin (0.001));
  CHECK (bot->fear_level_ == Approx (0.85).margin (0.001));

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_map") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();

  // map entities before activation: levelInitialize scans them for
  // map flags and breakables on the same path as every boot
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);

  edict_t *brk = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (brk != nullptr);
  brk->v.health = 50.0f;
  brk->v.takedamage = 1.0f;
  brk->v.impulse = 0;
  brk->v.spawnflags = 0;
  brk->v.movetype = MOVETYPE_PUSH;
  brk->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);

  edict_t *hos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hos != nullptr);
  hos->v.health = 100.0f;
  hos->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildBehaviorGraph ();

  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));
  HOST_REQUIRE (game.MapIs (MapFlags::HostageRescue));
  HOST_REQUIRE (game.HasBreakables ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("MapB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("MapB");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  // live hostages on a rescue map count
  BehaviorHook::PushHostage (*bot, hos);
  CHECK (BehaviorHook::Hostage (*bot));

  hos->v.health = 0.0f;
  bot->pev->origin = ystl::Vector (500.0f, 0.0f, 0.0f); // close enough to care
  CHECK (BehaviorHook::Hostage (*bot));

  bot->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f); // too far from the corpse
  CHECK (!BehaviorHook::Hostage (*bot));

  // explicit touch tracks a breakable straight into the shooting task
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, brk);
  CHECK (bot->GetTaskId () == TaskId::ShootBreakable);

  // clear world finds nothing to shoot
  bot->ClearTasks ();
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, nullptr);
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // planted bomb with a blown timer sends holders running (non-empty
  // origin, or the lookup misses the bomb entity and unplants it)
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (10.0f, 0.0f, 0.0f));
  engine.AdvanceTime (25.0f);
  BehaviorHook::SetCurrent (*bot, 1);
  bot->is_alive_ = true;
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (BehaviorHook::BombTimer (*bot));

  bot->StartTask (TaskId::Normal, TaskPri::normal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::EscapeFromBomb);

  // once the bot has escaped, the ticking bomb must not pull it back in
  BehaviorHook::SetEscapedFromBomb (*bot, true);
  CHECK (!BehaviorHook::BombTimer (*bot));

  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::normal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_frame") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("FrameB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("FrameB");
  HOST_REQUIRE (bot != nullptr);

  // a live bot runs the whole update/logic/task chain without crashing
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  for (int i = 0; i < 10; ++i) {
    engine.AdvanceTime (0.2f);
    bots.Frame ();
  }
  CHECK (bot->is_alive_);
  CHECK (bots.GetBotCount () == 1);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_damage") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("DmgB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("DmgB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = game.CreateFakeClient ("DmgFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[foe].team = Team::CT;
  clients[foe].team2 = Team::CT;
  bot->team_ = Team::Terrorist;
  bot->health_value_ = 100.0f;

  // every hit is stamped, creatures stop right there on junk inflictors
  BehaviorHook::SetCreature (*bot, true);
  BehaviorHook::Damage (*bot, nullptr, 25, 0, 8);
  CHECK (bot->last_damage_type_ == 8);
  CHECK (BehaviorHook::DamageTimestamp (*bot) == game.Time ());
  CHECK (bot->enemy_ == nullptr);
  BehaviorHook::SetCreature (*bot, false);

  // enemy fire feeds aggression, clears camp and remembers the shooter...
  bot->agression_level_ = 0.0f;
  bot->fear_level_ = 0.5f;
  BehaviorHook::SetCurrent (*bot, 1);
  bot->StartTask (TaskId::Camp, TaskPri::camp, kInvalidNodeIndex, 0.0f, true);
  HOST_REQUIRE (bot->GetTaskId () == TaskId::Camp);
  BehaviorHook::Damage (*bot, foe, 50, 0, 4);
  CHECK (bot->agression_level_ == 0.1f);
  CHECK (bot->fear_level_ == 0.5f);
  CHECK (bot->GetTaskId () != TaskId::Camp);
  CHECK (bot->last_enemy_ == foe);
  CHECK (bot->last_enemy_origin_ == foe->v.origin);
  CHECK (bot->see_enemy_timer_.started ());

  // ...and lands in the practice storage through the node map
  CHECK (bot->goal_value_ == -50.0f);
  CHECK (practice.GetDamage (Team::Terrorist, 1, 3) == 7); // 50 / 7, human divisor

  // low health scares instead of angering
  bot->health_value_ = 40.0f;
  bot->agression_level_ = 0.5f;
  bot->fear_level_ = 0.0f;
  BehaviorHook::Damage (*bot, foe, 10, 0, 4);
  CHECK (bot->agression_level_ == 0.5f);
  CHECK (bot->fear_level_ == 0.03f);

  // team kills mark the killer as the enemy on the spot
  cv_tkpunish.Set (1);
  clients[foe].team = Team::Terrorist;
  bot->enemy_ = nullptr;
  BehaviorHook::Damage (*bot, foe, 20, 0, 4);
  CHECK (bot->enemy_ == foe);
  CHECK (bot->last_enemy_ == foe);
  CHECK (bot->actual_reaction_time_ == 0.0f);
  clients[foe].team = Team::CT;

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_logic") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("LogB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("LogD", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("LogB");
  Bot *other = testhost::FindBot ("LogD");
  HOST_REQUIRE (bot != nullptr && other != nullptr);
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  // ...c4 goes nowhere without the flag, and somewhere with a human...
  bot->has_c4_ = false;
  BehaviorHook::Donate (*bot);
  CHECK (!BehaviorHook::ItemTimerStarted (*bot));

  edict_t *human = game.CreateFakeClient ("LogHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.solid = SOLID_BBOX;
  human->v.takedamage = DAMAGE_YES;
  human->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  bot->has_c4_ = true;
  BehaviorHook::Donate (*bot);
  CHECK (BehaviorHook::ItemTimerStarted (*bot));

  // a single frame sets the movement contract and reaction clamps...
  BehaviorHook::Logic (*bot);
  CHECK (BehaviorHook::MoveToGoalState (*bot));
  CHECK (!BehaviorHook::WantsFire (*bot));
  CHECK (!BehaviorHook::UsingGrenade (*bot));
  CHECK (bot->actual_reaction_time_ <= bot->ideal_reaction_time_);
  CHECK (bot->view_distance_ <= bot->max_view_distance_);

  // ...parachutes stay folded without the cvar...
  bot->pev->button = 0;
  BehaviorHook::CheckParachute (*bot);
  CHECK (!(bot->pev->button & IN_USE));

  // ...null drops and blind lifts are no-ops...
  BehaviorHook::DropWeapon (*bot, nullptr, false);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  edict_t *foe = game.CreateFakeClient ("LogFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.health = 100.0f;
  foe->v.solid = SOLID_BBOX;
  foe->v.takedamage = DAMAGE_YES;

  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::Blind (*bot, 200);
  CHECK (BehaviorHook::Enemy (*bot) == nullptr);

  // ...double jumps name their partner and task...
  BehaviorHook::DoubleJump (*bot, foe);
  CHECK (bot->GetTaskId () == TaskId::DoubleJump);

  // ...empty model info clears the mask, infected team flag rules...
  CHECK (!BehaviorHook::IsCreatureBot (*bot));

  char *info = engfuncs.pfnGetInfoKeyBuffer (bot->Ent ());
  BehaviorHook::RefreshModel (*bot, info);
  CHECK (!BehaviorHook::IsCreatureBot (*bot));

  BehaviorHook::SetInfectedTeam (*bot, true);
  CHECK (BehaviorHook::IsCreatureBot (*bot));
  BehaviorHook::SetInfectedTeam (*bot, false);

  // ...debug overlay needs an editor watching this bot...
  edict_t *editor = game.CreateFakeClient ("LogEd");
  HOST_REQUIRE (!game.IsNullEntity (editor));
  editor->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  editor->v.iuser2 = bot->Entindex ();
  graph.SetEditor (editor);

  const int messages_before = engine.Messages ().size ();
  BehaviorHook::ShowOverlay (*bot);
  CHECK (engine.Messages ().size () > messages_before);
  graph.SetEditor (nullptr);

  // ...no bomb planted, nobody is defusing...
  CHECK (!BehaviorHook::IsDefusing (*bot, ystl::Vector (10.0f, 0.0f, 0.0f)));

  // ...a planted bomb with a teammate on it reads defusing...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (10.0f, 0.0f, 0.0f));

  other->is_alive_ = true;
  other->team_ = Team::Terrorist; // pumped bots join as spectators
  other->pev->health = 100.0f;
  other->pev->deadflag = DEAD_NO;
  other->pev->movetype = MOVETYPE_WALK;
  other->pev->origin = ystl::Vector (50.0f, 0.0f, 0.0f);
  other->StartTask (TaskId::DefuseBomb, TaskPri::defuse_bomb, kInvalidNodeIndex, 0.0f, true);
  clients.Update ();

  CHECK (BehaviorHook::IsDefusing (*bot, ystl::Vector (10.0f, 0.0f, 0.0f)));

  game_state.SetBombPlanted (false);
  game_state.SetBombOrigin (true);

  // ...clear sight falls back to an empty camp direction...
  CHECK (BehaviorHook::CampDir (*bot, ystl::Vector (500.0f, 0.0f, 0.0f)).empty ());

  // ...debug chatter at level four reaches the server log...
  const int debug_before = cv_debug.As<int> ();
  cv_debug.Set (4);

  int prints_before = 0;

  for (int i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint") {
      ++prints_before;
    }
  }
  BehaviorHook::DebugMsg (*bot, "probe");

  int prints_after = 0;

  for (int i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint") {
      ++prints_after;
    }
  }
  CHECK (prints_after > prints_before);
  cv_debug.Set (debug_before);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_radio_dispatch") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("RadA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("RadB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *a = testhost::FindBot ("RadA");
  Bot *b = testhost::FindBot ("RadB");
  HOST_REQUIRE (a != nullptr && b != nullptr);

  a->team_ = Team::Terrorist;
  b->team_ = Team::Terrorist;
  a->is_alive_ = true;
  b->is_alive_ = true;
  BehaviorHook::Queue (*a).clear ();
  BehaviorHook::Queue (*b).clear ();

  // old global timestamp lets the local roll answer at once
  cv_radio_mode.Set (1);
  engine.AdvanceTime (10.0f);
  bots.SetLastRadioTimestamp (Team::Terrorist, 0.0f);
  bots.SetLastRadio (Team::Terrorist, RadioChat::Invalid);

  auto run_radio = [&] (RadioChat select) {
    BehaviorHook::Queue (*a).clear ();
    BehaviorHook::SetRadioSelect (*a, select);
    BehaviorHook::SetForceRadio (*a, true);
    BehaviorHook::Rng (*a).force_float (1.0f);
    a->PushMsgQueue (Msg::Radio);
    BehaviorHook::Rng (*a).clear_forced ();

    const int base = testhost::CsCalls (cs, "ClientCommand");
    BehaviorHook::CheckQueue (*a);

    return testhost::CsCalls (cs, "ClientCommand") - base;
  };

  // radio1 / radio2 / radio3 lanes, including menuselect
  CHECK (run_radio (RadioChat::CoverMe) == 2);
  CHECK (BehaviorHook::RadioOrder (*b) == RadioChat::CoverMe);
  CHECK (BehaviorHook::RadioEntity (*b) == a->Ent ());

  bots.SetLastRadioTimestamp (Team::Terrorist, 0.0f);
  bots.SetLastRadio (Team::Terrorist, RadioChat::Invalid);
  CHECK (run_radio (RadioChat::GoGoGo) == 2);
  CHECK (BehaviorHook::RadioOrder (*b) == RadioChat::GoGoGo);

  bots.SetLastRadioTimestamp (Team::Terrorist, 0.0f);
  bots.SetLastRadio (Team::Terrorist, RadioChat::Invalid);
  CHECK (run_radio (RadioChat::RogerThat) == 2);
  // acks are never fanned out, the mate keeps the previous order
  CHECK (BehaviorHook::RadioOrder (*b) == RadioChat::GoGoGo);

  // same-message squash is unreachable with a single delay sample
  // (outer needs stamp < time - d, inner needs stamp > time - d/2),
  // so a fresh stamp simply re-queues instead of answering
  BehaviorHook::Queue (*a).clear ();
  bots.SetLastRadioTimestamp (Team::Terrorist, game.Time ());
  BehaviorHook::SetRadioSelect (*a, RadioChat::CoverMe);
  BehaviorHook::SetForceRadio (*a, true);
  BehaviorHook::Rng (*a).force_float (1.0f);
  a->PushMsgQueue (Msg::Radio);
  BehaviorHook::Rng (*a).clear_forced ();
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).size () == 1);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_flash_avoid") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("FlashB", Difficulty::Expert, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("FlashB");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;

  // flying flashbang ahead, pops in 0.4s (inside the expert window)
  edict_t *flash = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (flash != nullptr);
  flash->v.model = string_t (engine.AllocString ("models/w_flashbang.mdl"));
  flash->v.origin = ystl::Vector (500.0f, 0.0f, 28.0f);
  flash->v.dmgtime = game.Time () + 0.4f;

  engine.AdvanceTime (0.3f); // active-grenade refresh runs every 0.25s
  game_state.UpdateActiveGrenade ();
  HOST_REQUIRE (game_state.HasActiveGrenades ());

  // pinned reaction, hold covers the pop (live experts roll up to 90%)
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::AvoidGrenades (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (BehaviorHook::AvoidFlashActive (*bot));

  // aim turns away from the grenade, level pitch, yaw follows the threat bearing
  BehaviorHook::AimDirection (*bot);
  const auto eyes = bot->pev->origin + bot->pev->view_ofs;
  const auto &look = bot->LookAtVector ();
  CHECK (look.x - eyes.x < 0.0f);
  CHECK (ystl::abs (look.z - eyes.z) < 100.0f);

  // fighting bots hold their aim, the enemy outranks the flash
  BehaviorHook::SetStates (*bot, Sense::SeeingEnemy);
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (!BehaviorHook::AvoidFlashFlagged (*bot));
  BehaviorHook::SetStates (*bot, Sense::Invalid);

  // blinded bots drop the floor hold at once, staring down blind looks broken
  edict_t *foe = game.CreateFakeClient ("FlashFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.origin = ystl::Vector (800.0f, 0.0f, 28.0f);
  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::StartBlind (*bot, 2.0f);
  BehaviorHook::AimDirection (*bot);
  const auto &released = bot->LookAtVector ();
  CHECK (eyes.z - released.z < 100.0f);
  CHECK (released.x - eyes.x > 0.0f);

  // behind-the-back flashbang is out of sight, no hold is rolled for it
  engine.AdvanceTime (1.0f);
  edict_t *behind = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (behind != nullptr);
  behind->v.model = string_t (engine.AllocString ("models/w_flashbang.mdl"));
  behind->v.origin = ystl::Vector (-500.0f, 0.0f, 28.0f);
  behind->v.dmgtime = game.Time () + 0.2f;

  engine.AdvanceTime (0.3f); // active-grenade refresh runs every 0.25s
  game_state.UpdateActiveGrenade ();

  BehaviorHook::AvoidGrenades (*bot);
  CHECK (!BehaviorHook::AvoidFlashActive (*bot));

  // hold lapses after the pop, aim is free again
  engine.AdvanceTime (1.0f);
  CHECK (!BehaviorHook::AvoidFlashActive (*bot));

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_c4_approach") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  // demolition flag needs the target present before activate
  edict_t *target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (target != nullptr);

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildBehaviorGraph ();
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("C4B", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  cv_quota.Set (1);

  Bot *bot = testhost::FindBot ("C4B");
  HOST_REQUIRE (bot != nullptr);
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->maxspeed = 250.0f; // the gamedll owns this on live servers
  bot->pev->movetype = MOVETYPE_WALK; // the fake engine only integrates walk/step/fly
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  clients[bot->Ent ()].team2 = Team::Terrorist;
  tickmgr.OnBotRound (bot); // start think/command timers like a round start does
  BehaviorHook::ForceStarted (*bot);

  edict_t *bomb = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (bomb != nullptr);
  bomb->v.model = string_t (engine.AllocString ("models/w_backpack.mdl"));
  const float bomb_pos[3] = { 175.0f, 0.0f, 18.0f };
  engine.Funcs ().pfnSetOrigin (bomb, bomb_pos);

  // past freezetime
  for (int i = 0; i < 40; ++i) {
    engine.AdvanceTime (0.2f);
    game_state.UpdateInterestingEntities ();
    bots.Frame ();
  }

  // dropped bomb straight ahead on open ground: the bot must walk onto the
  // spot instead of stalling with a vertical move pitch once it gets there
  float min_dist_2d = 1e9f;
  float max_move_pitch = 0.0f;

  for (int i = 0; i < 100; ++i) {
    engine.AdvanceTime (0.1f);
    game_state.UpdateInterestingEntities ();
    bots.Frame ();
    BehaviorHook::Update (*bot);
    BehaviorHook::IntegrateWalk (*bot);

    const float dist_2d = bot->pev->origin.distance2d (game.GetEntityOrigin (bomb));
    min_dist_2d = ystl::min (min_dist_2d, dist_2d);

    if (bot->GetTaskId () == TaskId::PickupItem) {
      max_move_pitch = ystl::max (max_move_pitch, ystl::abs (BehaviorHook::MoveAngles (*bot).x));
    }
  }
  CHECK (min_dist_2d < 30.0f); // walks onto the bomb spot, drive survives the vertical goal
  CHECK (max_move_pitch <= 45.01f); // movement pitch never goes vertical on the ground

  bots.Destroy ();
}

} // namespace bot
