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
  static edict_t *LastEnemy (Bot &bot) {
    return bot.last_enemy_;
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
  static void HostageEscort (Bot &bot) {
    bot.UpdateHostageEscort ();
  }
  static void GiveUpHostageWait (Bot &bot) {
    bot.hostage_wait_giveup_.start (-1.0f);
  }
  static bool Targeting (Bot &bot, int node) {
    return bot.IsTeammateTargetingNode (node);
  }
  static void TeamAwareness (Bot &bot) {
    bot.UpdateTeamAwareness ();
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
  static void SetLastEnemy (Bot &bot, edict_t *ent) {
    bot.last_enemy_ = ent;
    bot.last_enemy_origin_ = ent->v.origin;
  }
  static void ClearLastEnemy (Bot &bot) {
    bot.last_enemy_ = nullptr;
    bot.last_enemy_origin_.clear ();
  }
  static void Predict (Bot &bot) {
    bot.UpdatePredictedIndex ();
  }
  static int PredictIndex (Bot &bot) {
    return bot.last_predict_index_;
  }
  static bool PredictValid (Bot &bot) {
    return bot.predict_cache_.is_valid;
  }
  static void Slow (Bot &bot) {
    bot.SlowFrame ();
  }
  static bool Teleport (Bot &bot) {
    return bot.TryTeleportToSealedBomb ();
  }
  static int NeedAvoid (Bot &bot) {
    return bot.need_avoid_grenade_;
  }
  static void SetRadioOrder (Bot &bot, RadioChat order) {
    bot.radio_order_ = order;
  }
  static void SetRadioEntity (Bot &bot, edict_t *ent) {
    bot.radio_entity_ = ent;
  }
  static void SetPrevOrigin (Bot &bot, const ystl::Vector &origin) {
    bot.prev_origin_ = origin;
    bot.prev_timer_.start (0.0f);
  }
  static void StartSniperStop (Bot &bot, float time) {
    bot.sniper_stop_timer_.start (time);
  }
  static void SetReload (Bot &bot, bool reloading) {
    bot.reload_data_.state = reloading ? Reload::Primary : Reload::None;
    bot.reload_data_.is_reloading = reloading;
  }
  static void SetKnifeCheck (Bot &bot, bool value) {
    bot.check_knife_switch_ = value;
  }
  static void SetWeaponCheck (Bot &bot, bool value) {
    bot.check_weapon_switch_ = value;
  }
  static void DebugTimer (Bot &bot, float time) {
    bot.debug_update_timer_.start (time);
  }
  static void BuyTimer (Bot &bot, float time) {
    bot.next_buy_timer_.start (time);
  }
  static void SetBuyState (Bot &bot, BuyState state) {
    bot.buy_state_ = state;
  }
  static void SetTracking (Bot &bot, edict_t *ent) {
    bot.tracking_edict_ = ent;
    bot.next_tracking_timer_.start (0.0f);
  }
  static void SetPickupType (Bot &bot, Pickup type) {
    bot.pickup_type_ = type;
  }
  static void SetChat (Bot &bot, const char *text) {
    bot.chat_buffer_.assign (text);
  }
  static void CheckSpawn (Bot &bot) {
    bot.CheckSpawnConditions ();
  }
  static bool HasTask (Bot &bot, TaskId id) {
    return bot.tasks_.Find (id) != nullptr;
  }
  static bool Audible (Bot &bot) {
    return !bot.IsBombAudible ().empty ();
  }
  static void Victim (Bot &bot, edict_t *ent) {
    bot.SetLastVictim (ent);
  }
  static void SetWelcome (Bot &bot, bool value) {
    bot.need_to_send_welcome_chat_ = value;
  }
  static void ExpireView (Bot &bot) {
    bot.change_view_timer_.invalidate ();
  }
  static void SetWeaponType (Bot &bot, WeaponType type) {
    bot.weapon_type_ = type;
  }
  static void RefreshPredict (Bot &bot) {
    bot.RefreshEnemyPredict ();
  }
  static void SetShootTime (Bot &bot, float time) {
    bot.shoot_time_ = time;
  }
  static float ViewDist (Bot &bot) {
    return bot.view_distance_;
  }
  static float Move (Bot &bot) {
    return bot.move_speed_;
  }
  static float Strafe (Bot &bot) {
    return bot.strafe_speed_;
  }
  static void SetMoney (Bot &bot, int money) {
    bot.money_amount_ = money;
  }
  static void SetDefuser (Bot &bot, bool value) {
    bot.has_defuser_ = value;
  }
  static void ExpireStay (Bot &bot) {
    bot.stay_timer_.start (0.0f);
  }
  static edict_t *LastVictim (Bot &bot) {
    return bot.last_victim_;
  }
  static void Freezetime (Bot &bot) {
    bot.LogicDuringFreezetime ();
  }
  static void ExpireChangeView (Bot &bot) {
    bot.change_view_timer_.invalidate ();
  }
  static void SetClip (Bot &bot, Weapon weapon, int clip) {
    bot.ammo_in_clip_[weapon] = clip;
  }
  static void Sync (Bot &bot, int src, int dst) {
    bot.path_enqueue_timer_.invalidate ();
    bot.FindPath (src, dst, FindPathType::Fast);
  }
  static void PinPenetration (Bot &bot, edict_t *ent) {
    bot.penetration_check_origin_ = ent->v.origin;
    bot.penetration_result_ = true;
    bot.penetration_check_timer_.start (0.25f);
  }
  static void SetJump (Bot &bot, float time) {
    bot.jump_time_ = time;
  }
  static void CheckAround (Bot &bot) {
    bot.CheckBreakablesAround ();
  }
  static edict_t *Lookup (Bot &bot) {
    return bot.LookupBreakable ();
  }
  static void IgnoreBreakable (Bot &bot, edict_t *ent) {
    bot.ignored_breakable_.push (ent);
  }
  static void ClearIgnoredBreakable (Bot &bot) {
    bot.ignored_breakable_.clear ();
  }
  static edict_t *Breakable (Bot &bot) {
    return bot.breakable_entity_;
  }
  static void ClearEnemy (Bot &bot) {
    bot.enemy_ = nullptr;
  }
  static void SetAim (Bot &bot, AimFlags flags) {
    bot.aim_flags_ = flags;
  }
  static AimFlags AimState (Bot &bot) {
    return bot.aim_flags_;
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

  bots.Addbot ("MsgA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("MsgB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("SenseB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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
  bots.Addbot ("CondB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // ...fresh kills cap the aggression spike...
  bot->agression_level_ = 0.95f;
  bot->last_victim_ = human;
  clients[human].team = Team::CT;
  BehaviorHook::Conditions (*bot);
  CHECK (bot->agression_level_ == 1.0f);
  CHECK (bot->last_victim_ == nullptr);

  // ...stale trails clear once the world goes quiet...
  clients[human].noise.last = game.Time () - 1.0f;
  clients[human2].noise.last = game.Time () - 1.0f;
  engine.AdvanceTime (10.5f); // age out the last heard contact
  clients[human2].team = Team::CT;
  human2->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::SetLastEnemy (*bot, human2);
  BehaviorHook::Conditions (*bot); // runs hearing, arms the sound timer
  engine.AdvanceTime (0.01f);
  BehaviorHook::Conditions (*bot); // quiet tick clears the dead trail
  CHECK (BehaviorHook::LastEnemy (*bot) == nullptr);

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

  bots.Addbot ("MapB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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
  // (past the post-spawn decor grace period, moving at full speed)
  bot->pev->maxspeed = 270.0f;
  engine.AdvanceTime (6.0f);
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

  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::EscapeFromBomb);

  // once the bot has escaped, the ticking bomb must not pull it back in
  BehaviorHook::SetEscapedFromBomb (*bot, true);
  CHECK (!BehaviorHook::BombTimer (*bot));

  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_frame") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("FrameB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("DmgB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
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

  // ...hot streaks cap out on both ends...
  bot->health_value_ = 100.0f;
  bot->agression_level_ = 0.95f;
  BehaviorHook::Damage (*bot, foe, 10, 0, 4);
  CHECK (bot->agression_level_ == 1.0f);

  bot->health_value_ = 40.0f;
  bot->fear_level_ = 0.99f;
  BehaviorHook::Damage (*bot, foe, 10, 0, 4);
  CHECK (bot->fear_level_ == 1.0f);
  bot->health_value_ = 100.0f;

  // team kills mark the killer as the enemy on the spot
  cv_tkpunish.Set (1);
  clients[foe].team = Team::Terrorist;
  bot->enemy_ = nullptr;
  BehaviorHook::Damage (*bot, foe, 20, 0, 4);
  CHECK (bot->enemy_ == foe);
  CHECK (bot->last_enemy_ == foe);
  CHECK (bot->actual_reaction_time_ == 0.0f);
  clients[foe].team = Team::CT;

  // ...stray damage far from the graph re-seats the search...
  bot->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  BehaviorHook::Damage (*bot, nullptr, 10, 0, 0);
  CHECK (bot->last_damage_type_ == 0);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_logic") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("LogB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("LogD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // ...a synced walk paints the path arrows too...
  BehaviorHook::Sync (*bot, 0, 7);
  BehaviorHook::ShowOverlay (*bot);
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
  other->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
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

  bots.Addbot ("RadA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RadB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("FlashB", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // ...a live hold re-affirms the flash aim while it lasts...
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::AvoidFlashFlagged (*bot));

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

  bots.Addbot ("C4B", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

TEST_CASE ("unit/behavior_predict") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("PredB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("PredB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  BehaviorHook::SetCurrent (*bot, 1);

  edict_t *foe = game.CreateFakeClient ("PredFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  // visibility must be ready or prediction stays cold
  vistab.StartRebuild ();

  for (int i = 0; i < 40 && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  HOST_REQUIRE (vistab.IsReady ());

  // ...no last enemy, nothing to predict...
  BehaviorHook::ClearLastEnemy (*bot);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == kInvalidNodeIndex);
  CHECK (!BehaviorHook::PredictValid (*bot));

  // ...a suspect on the bot's own node never resolves...
  foe->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  BehaviorHook::SetLastEnemy (*bot, foe);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (0.3f);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == kInvalidNodeIndex);
  CHECK (!BehaviorHook::PredictValid (*bot));

  // ...a far-off trail exhausts the search with nothing...
  foe->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::SetLastEnemy (*bot, foe);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (0.3f);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == kInvalidNodeIndex);
  CHECK (!BehaviorHook::PredictValid (*bot));

  // ...a detached search skips everything out of reach...
  bot->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (0.3f);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == kInvalidNodeIndex);
  CHECK (!BehaviorHook::PredictValid (*bot));
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  // ...a home-node trail finds nothing in reach...
  // (exact origin zero reads as empty, stay a unit off it)
  foe->v.origin = ystl::Vector (1.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::SetLastEnemy (*bot, foe);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (0.3f);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == kInvalidNodeIndex);
  CHECK (!BehaviorHook::PredictValid (*bot));

  // ...live suspect with an origin computes and caches a node...
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::SetLastEnemy (*bot, foe);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (5.0f); // old trail, wide search band
  BehaviorHook::Predict (*bot);
  const int computed = BehaviorHook::PredictIndex (*bot);
  CHECK (computed != kInvalidNodeIndex);
  CHECK (BehaviorHook::PredictValid (*bot));

  // ...fresh cache answers without recomputing...
  engine.AdvanceTime (0.3f);
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == computed);
  CHECK (BehaviorHook::PredictValid (*bot));

  // ...throttled ticks skip the work...
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == computed);
  CHECK (BehaviorHook::PredictValid (*bot));

  // ...dead suspects keep the stale prediction, the guard returns early...
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  BehaviorHook::Predict (*bot);
  CHECK (BehaviorHook::PredictIndex (*bot) == computed);

  // ...a near suspect trail raises the predict flag (last-enemy look needs range)...
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::SetLastEnemy (*bot, foe);
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::SetAim (*bot, AimFlags::Invalid);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  BehaviorHook::SetShootTime (*bot, 0.0f);
  BehaviorHook::SetStates (*bot, Sense::SuspectEnemy);
  BehaviorHook::RefreshPredict (*bot);
  CHECK (has_flag (BehaviorHook::AimState (*bot), AimFlags::PredictPath));

  // ...a moving bot on top of a fresh trail denies the last-enemy look...
  BehaviorHook::SetAim (*bot, AimFlags::Invalid);
  bot->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  foe->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  BehaviorHook::SetLastEnemy (*bot, foe);
  BehaviorHook::SetShootTime (*bot, game.Time ());
  BehaviorHook::RefreshPredict (*bot);
  CHECK (has_flag (BehaviorHook::AimState (*bot), AimFlags::PredictPath));
  CHECK (!has_flag (BehaviorHook::AimState (*bot), AimFlags::LastEnemy));

  // ...a far visible trail with no predict pressure lands the last-enemy look...
  BehaviorHook::SetAim (*bot, AimFlags::Invalid);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  foe->v.origin = ystl::Vector (2200.0f, 0.0f, 0.0f);
  BehaviorHook::SetLastEnemy (*bot, foe);
  BehaviorHook::SetShootTime (*bot, 0.0f);
  BehaviorHook::RefreshPredict (*bot);
  CHECK (!has_flag (BehaviorHook::AimState (*bot), AimFlags::PredictPath));
  CHECK (has_flag (BehaviorHook::AimState (*bot), AimFlags::LastEnemy));

  // ...creatures skip the whole watch...
  BehaviorHook::SetCreature (*bot, true);
  BehaviorHook::RefreshPredict (*bot);
  BehaviorHook::SetCreature (*bot, false);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_siege") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  // demolition + rescue flags need their entities before activate
  edict_t *target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (target != nullptr);

  edict_t *hos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hos != nullptr);
  hos->v.health = 100.0f;
  hos->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);

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

  bots.Addbot ("SiegeT", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("SiegeC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *ter = testhost::FindBot ("SiegeT");
  Bot *ct = testhost::FindBot ("SiegeC");
  HOST_REQUIRE (ter != nullptr && ct != nullptr);
  ter->team_ = Team::Terrorist;
  ct->team_ = Team::CT;
  ter->is_alive_ = true;
  ct->is_alive_ = true;
  ter->pev->health = 100.0f;
  ct->pev->health = 100.0f;
  ter->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  ct->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  ct->pev->maxspeed = 250.0f;
  clients.Update ();
  clients[ter->Ent ()].team = Team::Terrorist;
  clients[ct->Ent ()].team = Team::CT;

  // ...quiet bomb, nobody is out of time...
  CHECK (!BehaviorHook::BombTimer (*ct));
  CHECK (!BehaviorHook::Audible (*ct));

  // ...planted bomb near a CT starts the defuse approach...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (2010.0f, 0.0f, 0.0f));
  ct->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);

  // ...holders far from the plant reset the approach timer...
  ct->pev->origin = ystl::Vector (1500.0f, 0.0f, 0.0f);
  BehaviorHook::Slow (*ct);
  CHECK (ct->GetTaskId () != TaskId::Camp);
  ct->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f);

  BehaviorHook::Slow (*ct);
  CHECK (ct->GetTaskId () != TaskId::Camp);

  // ...lingering by the bomb turns into a defuse task...
  engine.AdvanceTime (5.0f);
  BehaviorHook::Slow (*ct);
  CHECK (ct->GetTaskId () == TaskId::DefuseBomb);

  // ...defusing bots invalidate the approach timer...
  BehaviorHook::Slow (*ct);
  CHECK (ct->GetTaskId () == TaskId::DefuseBomb);

  // ...fresh timer with plenty left keeps holders calm...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (10.0f, 0.0f, 0.0f));
  ter->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  BehaviorHook::SetCurrent (*ter, 1);
  BehaviorHook::SetEscapedFromBomb (*ter, false);
  CHECK (!BehaviorHook::BombTimer (*ter));

  // ...blown timer sends nearby terrorists running...
  engine.AdvanceTime (25.0f);
  CHECK (BehaviorHook::BombTimer (*ter));

  // ...escaped bots stay out...
  BehaviorHook::SetEscapedFromBomb (*ter, true);
  CHECK (!BehaviorHook::BombTimer (*ter));
  BehaviorHook::SetEscapedFromBomb (*ter, false);

  // ...counter-terrorists with a kit and a short walk go too...
  engine.AdvanceTime (10.0f); // run the clock out so reach math always fits
  ct->pev->origin = ystl::Vector (1500.0f, 0.0f, 0.0f);
  ct->has_defuser_ = true;
  BehaviorHook::SetCurrent (*ct, 5);
  CHECK (BehaviorHook::BombTimer (*ct));

  // ...but a covered teammate with a kit waves the others off...
  ct->has_defuser_ = false;
  ter->team_ = Team::CT;
  ter->has_defuser_ = true;
  ter->pev->origin = ystl::Vector (50.0f, 0.0f, 0.0f);
  clients.Update ();
  ct->pev->origin = ystl::Vector (1500.0f, 0.0f, 0.0f);
  CHECK (!BehaviorHook::BombTimer (*ct));
  ter->team_ = Team::Terrorist;
  ter->has_defuser_ = false;

  // ...defusing bots sit the timer out (the early guard owns them)...
  ct->has_progress_bar_ = true;
  ct->pev->flags |= FL_ONGROUND;
  ct->pev->origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  CHECK (!BehaviorHook::BombTimer (*ct));
  ct->has_progress_bar_ = false;

  // ...teammate vectors route through every defuse witness...
  ter->team_ = Team::CT;
  ct->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  ct->pev->origin = ystl::Vector (2010.0f, 0.0f, 0.0f);
  CHECK (BehaviorHook::IsDefusing (*ter, ystl::Vector (2010.0f, 0.0f, 0.0f)));

  ct->ClearTasks ();
  ct->StartTask (TaskId::MoveTo, TaskPri::kHide, 7, 0.0f, true);
  ct->pev->origin = ystl::Vector (700.0f, 0.0f, 0.0f);
  game_state.SetBombOrigin (false, ystl::Vector (710.0f, 0.0f, 0.0f));
  clients.Update ();
  CHECK (BehaviorHook::IsDefusing (*ter, ystl::Vector (710.0f, 0.0f, 0.0f)));

  ct->ClearTasks ();
  BehaviorHook::SetPickupType (*ct, Pickup::PlantedC4);
  CHECK (BehaviorHook::IsDefusing (*ter, ystl::Vector (710.0f, 0.0f, 0.0f)));
  BehaviorHook::SetPickupType (*ct, Pickup::None);

  edict_t *human = game.CreateFakeClient ("SiegeHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (700.0f, 0.0f, 0.0f);
  human->v.button |= IN_USE;
  clients.Update ();
  clients[human].team = Team::CT;
  CHECK (BehaviorHook::IsDefusing (*ter, ystl::Vector (710.0f, 0.0f, 0.0f)));
  human->v.button &= ~IN_USE;

  // ...c4 beeps in memory short-circuit the audible fallback...
  ter->team_ = Team::Terrorist;
  Client &hclient = clients[human];
  hclient.noise.dist = 5000.0f;
  hclient.noise.last = game.Time () + 10.0f;
  hclient.noise.start = game.Time ();
  hclient.noise.pos = ystl::Vector (710.0f, 0.0f, 0.0f);
  hclient.noise.type = Noise::Defuse;
  BehaviorHook::Hearing (*ter);
  CHECK (BehaviorHook::Audible (*ter));

  // ...stale beeps fall back to the blast radius...
  engine.AdvanceTime (6.0f);
  ter->pev->origin = ystl::Vector (900.0f, 0.0f, 0.0f);
  CHECK (BehaviorHook::Audible (*ter));
  ter->pev->origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  CHECK (!BehaviorHook::Audible (*ter));
  ter->pev->origin = ystl::Vector (900.0f, 0.0f, 0.0f);

  // ...spawn checks hand out spray work, grenades and followers...
  // (...live enemies skip the spawn chores entirely...)
  BehaviorHook::SetEnemy (*ter, human);
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::ClearEnemy (*ter);
  BehaviorHook::SetKnifeCheck (*ter, true);
  ter->buying_finished_ = true;
  engine.AdvanceTime (8.0f);
  ter->pev->groundentity = game.GetStartEntity ();
  cv_spraypaints.Set (100);
  BehaviorHook::Rng (*ter).force_int (50);
  BehaviorHook::Rng (*ter).force_chance (true);
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::Rng (*ter).clear_forced ();
  CHECK (BehaviorHook::HasTask (*ter, TaskId::Spraypaint));
  cv_spraypaints.Set (0);

  ter->ClearTasks ();
  ter->pev->weapons |= ystl::bit (Weapon::Explosive);
  BehaviorHook::SetKnifeCheck (*ter, true);
  BehaviorHook::Rng (*ter).force_chance (true);
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::Rng (*ter).clear_forced ();
  CHECK (ter->GetTaskId () == TaskId::Normal);

  clients[human].team = Team::Terrorist;
  human->v.origin = ystl::Vector (950.0f, 0.0f, 0.0f);
  ter->pev->origin = ystl::Vector (900.0f, 0.0f, 0.0f);
  clients.Update ();
  cv_user_follow_percent.Set (100);
  BehaviorHook::SetKnifeCheck (*ter, true);
  BehaviorHook::Rng (*ter).force_chance (true);
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::Rng (*ter).clear_forced ();
  CHECK (ter->GetTaskId () == TaskId::FollowUser);
  cv_user_follow_percent.Set (0);

  // ...weapon switches rack silenced pistols and burst toggles...
  engine.AdvanceTime (5.0f);
  ter->ClearTasks ();
  BehaviorHook::SetWeaponCheck (*ter, true);
  ter->current_weapon_ = Weapon::M4A1;
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::SetWeaponCheck (*ter, true);
  ter->current_weapon_ = Weapon::Famas;
  ter->pev->button = 0;
  BehaviorHook::Rng (*ter).force_chance (true);
  BehaviorHook::CheckSpawn (*ter);
  BehaviorHook::Rng (*ter).clear_forced ();
  CHECK (!!(ter->pev->button & IN_ATTACK2));

  // ...knifers drop the rifle without a second thought...
  ter->ClearTasks ();
  BehaviorHook::SetKnifeCheck (*ter, true);
  cv_jasonmode.Set (1);
  BehaviorHook::CheckSpawn (*ter);
  cv_jasonmode.Set (0);

  // ...carriers hand the pack over instead of shopping...
  ter->pev->weapons |= ystl::bit (Weapon::C4);
  cv_ignore_objectives.Set (1);
  BehaviorHook::Update (*ter);
  cv_ignore_objectives.Set (0);
  CHECK (!ter->has_c4_);

  ter->pev->weapons |= ystl::bit (Weapon::C4);
  cv_jasonmode.Set (1);
  BehaviorHook::Update (*ter);
  cv_jasonmode.Set (0);
  CHECK (!ter->has_c4_);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_ordnance") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("OrdB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("OrdB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("OrdFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (800.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  // ...priming the think interval lets approach math see motion...
  engine.AdvanceTime (0.3f);
  BehaviorHook::Update (*bot);

  // ...incoming enemy explosives pick a strafe side by geometry...
  edict_t *he = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (he != nullptr);
  he->v.model = string_t (engine.AllocString ("models/w_hegrenade.mdl"));
  he->v.origin = ystl::Vector (300.0f, 100.0f, 28.0f);
  he->v.velocity = ystl::Vector (-300.0f, -100.0f, 0.0f);
  he->v.owner = foe;

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  HOST_REQUIRE (game_state.HasActiveGrenades ());

  BehaviorHook::AvoidGrenades (*bot);
  const int first_side = BehaviorHook::NeedAvoid (*bot);
  CHECK (first_side != 0);

  // ...mirrored geometry commits to the other side...
  he->v.flags |= FL_ONGROUND;
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);

  he->v.flags &= ~FL_ONGROUND;
  he->v.origin = ystl::Vector (300.0f, -100.0f, 28.0f);
  he->v.velocity = ystl::Vector (-300.0f, 100.0f, 0.0f);

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) != 0);
  CHECK (BehaviorHook::NeedAvoid (*bot) != first_side);

  // ...a landed shell stops being a threat...
  he->v.effects |= EF_NODRAW;
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);
  he->v.effects &= ~EF_NODRAW;

  // ...outgoing shells never turn the bot...
  he->v.origin = ystl::Vector (300.0f, 0.0f, 28.0f);
  he->v.velocity = ystl::Vector (300.0f, 0.0f, 0.0f);
  he->v.owner = foe;

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);

  // ...distant pops stay out of the dodge radius...
  he->v.origin = ystl::Vector (600.0f, 0.0f, 28.0f);
  he->v.velocity = ystl::Vector (-300.0f, 0.0f, 0.0f);

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);

  // ...friendly fire never triggers a dodge...
  Bot *mate = nullptr;
  bots.Addbot ("OrdM", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  mate = testhost::FindBot ("OrdM");
  HOST_REQUIRE (mate != nullptr);
  mate->team_ = Team::Terrorist;
  clients.Update ();
  clients[mate->Ent ()].team = Team::Terrorist;
  clients[mate->Ent ()].team2 = Team::Terrorist;

  he->v.origin = ystl::Vector (300.0f, 0.0f, 28.0f);
  he->v.velocity = ystl::Vector (-300.0f, 0.0f, 0.0f);
  he->v.owner = mate->Ent ();

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);

  he->v.owner = bot->Ent ();
  BehaviorHook::AvoidGrenades (*bot);
  CHECK (BehaviorHook::NeedAvoid (*bot) == 0);

  // ...the dodge side drives the legs through a live frame...
  he->v.owner = foe;
  he->v.origin = ystl::Vector (300.0f, 100.0f, 28.0f);
  he->v.velocity = ystl::Vector (-300.0f, -100.0f, 0.0f);

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  HOST_REQUIRE (BehaviorHook::NeedAvoid (*bot) != 0);

  bot->pev->maxspeed = 250.0f;
  BehaviorHook::Logic (*bot);
  CHECK (BehaviorHook::Strafe (*bot) != 0.0f);
  CHECK (BehaviorHook::Move (*bot) == -bot->pev->maxspeed);

  // ...settled smoke on the view axis is evaluated, off-center math holds...
  he->v.model = string_t (engine.AllocString ("models/w_smokegrenade.mdl"));
  he->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  he->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  he->v.flags |= FL_ONGROUND;

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();

  const int smoke_before = cv_smoke_grenade_checks.As<int> ();
  cv_smoke_grenade_checks.Set (1);
  const float view_before = BehaviorHook::ViewDist (*bot);
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::AvoidGrenades (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  cv_smoke_grenade_checks.Revert ();
  CHECK (smoke_before == 2);
  CHECK (BehaviorHook::ViewDist (*bot) == view_before);

  // ...unseen flashes and blind-side shells never register...
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // nothing is visible past halfway
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL

  edict_t *unseen = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (unseen != nullptr);
  unseen->v.model = string_t (engine.AllocString ("models/w_flashbang.mdl"));
  unseen->v.origin = ystl::Vector (500.0f, 0.0f, 28.0f);
  unseen->v.dmgtime = game.Time () + 5.0f;

  edict_t *behind_he = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (behind_he != nullptr);
  behind_he->v.model = string_t (engine.AllocString ("models/w_hegrenade.mdl"));
  behind_he->v.origin = ystl::Vector (-300.0f, 0.0f, 28.0f);
  behind_he->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  behind_he->v.flags |= FL_ONGROUND;
  behind_he->v.owner = foe;

  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  BehaviorHook::AvoidGrenades (*bot);
  engine.SetTraceLineHook (nullptr);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_dispatch") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("DisA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DisB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DisJ", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DisV", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DisD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DisN", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 6));

  Bot *a = testhost::FindBot ("DisA");
  Bot *b = testhost::FindBot ("DisB");
  HOST_REQUIRE (a != nullptr && b != nullptr);
  a->team_ = Team::Terrorist;
  b->team_ = Team::Terrorist;
  a->is_alive_ = true;
  b->is_alive_ = true;
  BehaviorHook::Queue (*a).clear ();
  BehaviorHook::Queue (*b).clear ();

  // ...reporting-in answers on the radio lanes and clears the log...
  cv_radio_mode.Set (1);
  engine.AdvanceTime (10.0f);
  bots.SetLastRadioTimestamp (Team::Terrorist, 0.0f);
  bots.SetLastRadio (Team::Terrorist, RadioChat::Invalid);

  const int radio_base = testhost::CsCalls (cs, "ClientCommand");
  BehaviorHook::Queue (*a).clear ();
  BehaviorHook::SetRadioSelect (*a, RadioChat::ReportingIn);
  BehaviorHook::SetForceRadio (*a, true);
  BehaviorHook::Rng (*a).force_float (1.0f);
  a->PushMsgQueue (Msg::Radio);
  BehaviorHook::Rng (*a).clear_forced ();
  BehaviorHook::CheckQueue (*a);
  CHECK (testhost::CsCalls (cs, "ClientCommand") - radio_base == 2);
  CHECK (bots.GetLastRadio (Team::Terrorist) == RadioChat::Invalid);
  CHECK (BehaviorHook::RadioOrder (*b) == RadioChat::ReportingIn);

  // ...team chat goes out on its own channel...
  BehaviorHook::SetChat (*a, "go go");
  a->PushMsgQueue (Msg::SayTeam);
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).empty ());

  // ...a stocked buy zone runs the weapon flow...
  mp_freezetime.Set (6.0f);
  a->in_buy_zone_ = true;
  BehaviorHook::BuyTimer (*a, 0.0f);
  engine.AdvanceTime (0.1f); // let the rearmed timer lapse
  BehaviorHook::SetBuyState (*a, BuyState::PrimaryWeapon);
  BehaviorHook::Queue (*a).clear ();
  a->PushMsgQueue (Msg::Buy);
  BehaviorHook::CheckQueue (*a);
  CHECK (!a->buy_pending_);
  CHECK (!a->buying_finished_);

  // ...low freezetime skips the buy delay...
  BehaviorHook::SetBuyState (*a, BuyState::PrimaryWeapon);
  BehaviorHook::BuyTimer (*a, 0.0f);
  engine.AdvanceTime (0.1f); // let the rearmed timer lapse
  mp_freezetime.Set (0.0f);
  BehaviorHook::Queue (*a).clear ();
  a->PushMsgQueue (Msg::Buy);
  BehaviorHook::CheckQueue (*a);
  CHECK (!a->buy_pending_);
  mp_freezetime.Set (6.0f);

  // ...disabled buying, fun mode, vip status and done states all finish dry...
  auto dry_buy = [&] (const char *name) {
    Bot *bot = testhost::FindBot (name);
    HOST_REQUIRE (bot != nullptr);
    bot->in_buy_zone_ = true;
    bot->buying_finished_ = false;
    BehaviorHook::BuyTimer (*bot, 0.0f);
    engine.AdvanceTime (0.1f); // let the rearmed timer lapse
    BehaviorHook::SetBuyState (*bot, BuyState::PrimaryWeapon);
    BehaviorHook::Queue (*bot).clear ();
    bot->PushMsgQueue (Msg::Buy);
    BehaviorHook::CheckQueue (*bot);
    CHECK (bot->buying_finished_);
  };

  cv_botbuy.Set (0);
  dry_buy ("DisN");
  cv_botbuy.Set (1);

  Bot *jason = testhost::FindBot ("DisJ");
  HOST_REQUIRE (jason != nullptr);
  cv_jasonmode.Set (1);
  dry_buy ("DisJ");
  cv_jasonmode.Set (0);

  Bot *vip = testhost::FindBot ("DisV");
  HOST_REQUIRE (vip != nullptr);
  vip->is_vip_ = true;
  dry_buy ("DisV");
  vip->is_vip_ = false;

  Bot *done = testhost::FindBot ("DisD");
  HOST_REQUIRE (done != nullptr);
  done->in_buy_zone_ = true;
  done->buying_finished_ = false;
  BehaviorHook::BuyTimer (*done, 0.0f);
  engine.AdvanceTime (0.1f); // let the rearmed timer lapse
  BehaviorHook::SetBuyState (*done, BuyState::Done);
  BehaviorHook::Queue (*done).clear ();
  done->PushMsgQueue (Msg::Buy);
  BehaviorHook::CheckQueue (*done);
  CHECK (done->buying_finished_);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_trauma") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("TrmB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TrmB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->health_value_ = 100.0f;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("TrmFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  edict_t *user = game.CreateFakeClient ("TrmUser");
  HOST_REQUIRE (!game.IsNullEntity (user));
  user->v.flags &= ~FL_FAKECLIENT;
  user->v.health = 100.0f;
  user->v.deadflag = DEAD_NO;
  user->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[user].team = Team::Terrorist;

  // ...monsters anger through the same enemy-fire path...
  edict_t *monster = engine.SpawnEntity ("monster_mortar");
  HOST_REQUIRE (monster != nullptr);
  monster->v.health = 100.0f;
  monster->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  monster->v.flags |= FL_MONSTER;
  bot->agression_level_ = 0.0f;
  cv_attack_monsters.Set (1);
  BehaviorHook::Damage (*bot, monster, 10, 0, 4);
  cv_attack_monsters.Set (0);
  CHECK (bot->agression_level_ == 0.1f);

  // ...unusual damage only stamps the intake...
  BehaviorHook::Damage (*bot, nullptr, 10, 0, 0);
  CHECK (bot->last_damage_type_ == 0);
  CHECK (BehaviorHook::DamageTimestamp (*bot) == game.Time ());

  // ...creature bites track visible attackers at once...
  BehaviorHook::SetCreature (*bot, true);
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::Damage (*bot, foe, 10, 0, 4);
  CHECK (BehaviorHook::Enemy (*bot) == foe);
  BehaviorHook::SetCreature (*bot, false);

  // ...unseen bites only leave a last-known trail...
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // nothing is visible past halfway
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  BehaviorHook::SetCreature (*bot, true);
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::ClearLastEnemy (*bot);
  BehaviorHook::Damage (*bot, foe, 10, 0, 4);
  CHECK (BehaviorHook::Enemy (*bot) == nullptr);
  CHECK (BehaviorHook::LastEnemy (*bot) == foe);
  BehaviorHook::SetCreature (*bot, false);
  engine.SetTraceLineHook (nullptr);

  // ...blind experts run for cover picked off the graph...
  bot->SetNewDifficulty (Difficulty::Hard);
  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::Blind (*bot, 300);
  CHECK (BehaviorHook::Enemy (*bot) == nullptr);

  // ...hard blinds commit to a side, the careful duck instead...
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::Blind (*bot, 300);
  BehaviorHook::Rng (*bot).clear_forced ();
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::Blind (*bot, 300);
  BehaviorHook::Rng (*bot).clear_forced ();

  bot->pev->health = 50.0f;
  BehaviorHook::Blind (*bot, 300);

  bot->pev->health = 100.0f;
  bot->personality_ = Personality::Careful;
  BehaviorHook::Blind (*bot, 300);
  bot->personality_ = Personality::Normal;

  // ...hurt experts back away instead...
  bot->health_value_ = 50.0f;
  BehaviorHook::Blind (*bot, 300);
  bot->health_value_ = 100.0f;

  // ...spent flashes with no hold return at once...
  BehaviorHook::Blind (*bot, 100);

  // ...weapon drops arm the buyer when handed over in the zone...
  bot->pev->weapons |= ystl::bit (Weapon::AK47);
  bot->current_weapon_ = Weapon::AK47;
  BehaviorHook::SetMoney (*bot, 2500);
  BehaviorHook::Queue (*bot).clear ();
  BehaviorHook::DropWeapon (*bot, user, false);
  CHECK (BehaviorHook::ItemTimerStarted (*bot));

  bot->has_c4_ = true;
  bot->in_buy_zone_ = true;
  BehaviorHook::DropWeapon (*bot, user, true);
  CHECK (BehaviorHook::ItemTimerStarted (*bot));
  CHECK (!bot->buying_finished_);
  CHECK (BehaviorHook::Queue (*bot).size () == 1);
  bot->has_c4_ = false;
  bot->in_buy_zone_ = false;

  bot->ClearTasks ();
  user->v.deadflag = DEAD_DEAD;
  user->v.health = 0.0f;
  clients.Update ();
  BehaviorHook::DropWeapon (*bot, user, false);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  user->v.deadflag = DEAD_NO;
  user->v.health = 100.0f;
  clients.Update ();

  // ...c4 finds a human carrier and the dropped pack changes hands...
  edict_t *pack = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (pack != nullptr);
  pack->v.model = string_t (engine.AllocString ("models/w_backpack.mdl"));
  pack->v.origin = ystl::Vector (120.0f, 0.0f, 0.0f);

  user->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  bot->has_c4_ = true;
  BehaviorHook::Donate (*bot);
  CHECK (BehaviorHook::ItemTimerStarted (*bot));
  bot->has_c4_ = false;

  // ...far victims arm the forget timer...
  foe->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  BehaviorHook::Victim (*bot, foe);
  CHECK (BehaviorHook::LastVictim (*bot) == foe);

  // ...stale damage stamps reset on the next frame...
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::Damage (*bot, foe, 5, 0, 4);
  engine.AdvanceTime (0.5f);
  BehaviorHook::Update (*bot);
  CHECK (BehaviorHook::DamageTimestamp (*bot) == 0.0f);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_donate2") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("Don2", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DonM", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("Don2");
  Bot *mate = testhost::FindBot ("DonM");
  HOST_REQUIRE (bot != nullptr && mate != nullptr);
  bot->team_ = Team::Terrorist;
  mate->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  mate->is_alive_ = true;
  mate->pev->health = 100.0f;
  mate->pev->deadflag = DEAD_NO;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  clients[mate->Ent ()].team = Team::Terrorist;

  edict_t *human = game.CreateFakeClient ("DonHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  // ...with nobody around the pack stays on the back...
  bot->has_c4_ = true;
  human->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  BehaviorHook::Donate (*bot);
  CHECK (!BehaviorHook::BuyTimerStarted (*bot));
  bot->has_c4_ = false;

  // ...bot mates sit the handoff out, decoys fall through...
  edict_t *decoy = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (decoy != nullptr);
  decoy->v.model = string_t (engine.AllocString ("models/w_ak47.mdl"));
  decoy->v.origin = ystl::Vector (120.0f, 0.0f, 0.0f);

  edict_t *pack = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (pack != nullptr);
  pack->v.model = string_t (engine.AllocString ("models/w_backpack.mdl"));
  pack->v.origin = ystl::Vector (130.0f, 0.0f, 0.0f);

  human->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  bot->has_c4_ = true;
  bot->current_weapon_ = Weapon::AK47;
  BehaviorHook::Donate (*bot);
  CHECK (BehaviorHook::BuyTimerStarted (*bot));
  bot->has_c4_ = false;

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_lifecycle") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("LifeB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("LifeB");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 250.0f;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;
  clients[bot->Ent ()].team2 = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("LifeFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  clients[foe].team2 = Team::CT;

  edict_t *mate = game.CreateFakeClient ("LifeMate");
  HOST_REQUIRE (!game.IsNullEntity (mate));
  mate->v.flags &= ~FL_FAKECLIENT;
  mate->v.health = 100.0f;
  mate->v.deadflag = DEAD_NO;
  mate->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[mate].team = Team::Terrorist;
  clients[mate].team2 = Team::Terrorist;

  // ...a fresh frame joins the team through the menu flow...
  BehaviorHook::Update (*bot);
  BehaviorHook::ForceStarted (*bot);

  // ...naughty origins hold still instead of poisoning traces...
  union {
    uint32_t words;
    float number;
  } nan = { 0x7FC00000u };
  const ystl::Vector saved_origin = bot->pev->origin;
  bot->pev->origin.x = nan.number;
  BehaviorHook::Logic (*bot);
  bot->pev->origin = saved_origin;

  // ...yanked bots drop stale nav state and re-seat on the graph...
  BehaviorHook::SetPrevOrigin (*bot, ystl::Vector (5000.0f, 0.0f, 0.0f));
  engine.AdvanceTime (0.3f);
  BehaviorHook::Logic (*bot);

  // ...queued radio orders run through the frame...
  BehaviorHook::SetRadioOrder (*bot, RadioChat::CoverMe);
  BehaviorHook::SetRadioEntity (*bot, foe);
  BehaviorHook::Logic (*bot);

  // ...consecutive frames split heavy sensing from enemy tracking...
  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::Logic (*bot);
  engine.AdvanceTime (1.0f);
  BehaviorHook::Logic (*bot);

  // ...idle legs clear stale pickups once the walk overstays...
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::SetStates (*bot, Sense::Invalid);
  engine.AdvanceTime (5.0f);
  BehaviorHook::Logic (*bot);

  // ...hearing feeds the overlay sound arrows...
  Client &fclient = clients[foe];
  fclient.noise.dist = 5000.0f;
  fclient.noise.last = game.Time () + 10.0f;
  fclient.noise.start = game.Time ();
  fclient.noise.pos = foe->v.origin;
  fclient.noise.type = Noise::WeaponFire;
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::SoundMemoryCount (*bot) == 1);

  // ...without an editor the overlay stays dark...
  BehaviorHook::ShowOverlay (*bot);

  // ...a watching editor renders text, lines and boxes...
  edict_t *editor = game.CreateFakeClient ("LifeEd");
  HOST_REQUIRE (!game.IsNullEntity (editor));
  editor->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  editor->v.iuser2 = bot->Entindex ();
  graph.SetEditor (editor);

  const int debug_keep = cv_debug.As<int> ();
  cv_debug.Set (4);
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);

  const size_t overlay_before = engine.Messages ().size ();
  BehaviorHook::ShowOverlay (*bot);
  CHECK (engine.Messages ().size () > overlay_before);

  // ...a quiet level-two editor picks the nearest bot instead...
  editor->v.iuser2 = 0;
  cv_debug.Set (2);
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  clients.Update ();
  mate->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  foe->v.origin = ystl::Vector (2500.0f, 0.0f, 0.0f);
  clients.Update ();

  const size_t level2_before = engine.Messages ().size ();
  BehaviorHook::ShowOverlay (*bot);
  CHECK (engine.Messages ().size () > level2_before);

  // ...held text and empty tasks skip the text pass...
  editor->v.iuser2 = bot->Entindex ();
  cv_debug.Set (4);
  BehaviorHook::DebugTimer (*bot, 100.0f);
  BehaviorHook::ShowOverlay (*bot);

  bot->ClearTasks ();
  BehaviorHook::ShowOverlay (*bot);

  // ...unwatched editors stay dark even at level one...
  editor->v.iuser2 = 0;
  cv_debug.Set (1);
  const size_t dark_before = engine.Messages ().size ();
  BehaviorHook::ShowOverlay (*bot);
  CHECK (engine.Messages ().size () == dark_before);

  // ...debug chatter at level three needs a spectator...
  editor->v.iuser2 = bot->Entindex ();
  game.SetLocalEntity (editor);
  cv_debug.Set (3);
  int prints_before = 0;

  for (int i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint") {
      ++prints_before;
    }
  }
  BehaviorHook::DebugMsg (*bot, "probe3");

  int prints_after = 0;

  for (int i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint") {
      ++prints_after;
    }
  }
  CHECK (prints_after > prints_before);

  // ...level three without a spectator stays quiet...
  editor->v.iuser2 = 0;
  BehaviorHook::DebugMsg (*bot, "lonely");

  int prints_quiet = 0;

  for (int i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint") {
      ++prints_quiet;
    }
  }
  CHECK (prints_quiet == prints_after);
  editor->v.iuser2 = bot->Entindex ();

  cv_debug.Set (0);
  BehaviorHook::DebugMsg (*bot, "quiet");

  graph.SetEditor (nullptr);
  cv_debug.Set (debug_keep);

  // ...slow boots run the parade rest with a jump and a stare...
  bot->pev->maxspeed = 5.0f;
  BehaviorHook::ExpireView (*bot);
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::Update (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (!!(bot->pev->button & IN_JUMP));
  CHECK (BehaviorHook::Enemy (*bot) == foe);

  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  BehaviorHook::SetWelcome (*bot, true);
  BehaviorHook::Update (*bot);

  // ...frozen boots reset movement instead...
  bot->SetMoveAllowed (false);
  bot->pev->maxspeed = 250.0f;
  BehaviorHook::Update (*bot);
  CHECK (BehaviorHook::Move (*bot) == 0.0f);
  bot->SetMoveAllowed (true);

  // ...dead boots vote team killers away...
  bot->pev->health = 0.0f;
  bot->pev->deadflag = DEAD_DEAD;
  clients.Update ();
  bot->vote_kick_index_ = 3;
  bot->last_vote_kick_ = 0;
  const int tk_keep = cv_tkpunish.As<int> ();
  cv_tkpunish.Set (1);

  const int vote_base = testhost::CsCalls (cs, "ClientCommand");
  BehaviorHook::Update (*bot);
  CHECK (bot->last_vote_kick_ == 3);
  CHECK (testhost::CsCalls (cs, "ClientCommand") - vote_base == 1);

  // ...proven killers eat a frag and a slay...
  const int human_idx = game.IndexOfEntity (mate);
  const float frags_before = mate->v.frags;
  bot->vote_kick_index_ = human_idx;
  bot->last_vote_kick_ = 0;
  cv_tkpunish.Set (2);
  BehaviorHook::Update (*bot);
  CHECK (mate->v.frags == frags_before + 1.0f);
  cv_tkpunish.Set (tk_keep);

  // ...map votes go out on their own channel...
  bot->vote_map_ = 7;
  const int map_base = testhost::CsCalls (cs, "ClientCommand");
  BehaviorHook::Update (*bot);
  CHECK (bot->vote_map_ == 0);
  CHECK (testhost::CsCalls (cs, "ClientCommand") - map_base == 1);

  // ...parade rest jumps, stares and greets...
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  bot->pev->maxspeed = 5.0f;
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  BehaviorHook::ExpireChangeView (*bot);
  BehaviorHook::SetWelcome (*bot, true);
  BehaviorHook::SetJump (*bot, 0.0f);
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::Freezetime (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (!!(bot->pev->button & IN_JUMP));

  // ...blinded logic shrinks the world...
  BehaviorHook::StartBlind (*bot, 2.0f);
  BehaviorHook::Logic (*bot);
  CHECK (bot->max_view_distance_ == 4096.0f);

  // ...watched logic paints the overlay...
  editor->v.iuser2 = bot->Entindex ();
  graph.SetEditor (editor);
  cv_debug.Set (1);
  BehaviorHook::Logic (*bot);
  cv_debug.Set (0);
  graph.SetEditor (nullptr);

  // ...blocked sight lines resolve through the graph...
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  clients.Update ();
  BehaviorHook::SetCurrent (*bot, 1);
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (700.0f, 0.0f, 0.0f)).empty ());

  engine.SetTraceLineHook ([&] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // every sight line stops halfway
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (700.0f, 0.0f, 0.0f)) == ystl::Vector (200.0f, 0.0f, 0.0f));
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (3000.0f, 0.0f, 0.0f)).empty ());

  // ...off-graph campers fall back to nothing...
  graph.Reset ();
  CHECK (BehaviorHook::CampDirection (*bot, ystl::Vector (700.0f, 0.0f, 0.0f)).empty ());
  engine.SetTraceLineHook (nullptr);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_override") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("OvrB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("OvrB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 250.0f;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("OvrFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  BehaviorHook::SetCurrent (*bot, 1);

  // ...knifers walk the graph when the prey is out of reach...
  BehaviorHook::SetWeaponType (*bot, WeaponType::Melee);
  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (bot->Task ()->data == 5);

  // ...and holster the detour once in stabbing range...
  foe->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...stale detours re-route instead of stacking...
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  BehaviorHook::SetEnemy (*bot, foe);
  bot->StartTask (TaskId::MoveTo, TaskPri::kHide, 3, 0.0f, true);
  BehaviorHook::Override (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (bot->Task ()->data == 5);

  // ...snipers freeze on a settled bead...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  BehaviorHook::SetWeaponType (*bot, WeaponType::Sniper);
  BehaviorHook::SetReload (*bot, false);
  BehaviorHook::SetStates (*bot, Sense::SeeingEnemy | Sense::SuspectEnemy);
  BehaviorHook::SetShootTime (*bot, game.Time ());
  BehaviorHook::StartSniperStop (*bot, 10.0f);
  BehaviorHook::Override (*bot);
  CHECK (BehaviorHook::Move (*bot) == 0.0f);

  // ...a wandering muzzle keeps the feet moving...
  bot->pev->v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  BehaviorHook::Override (*bot);

  // ...mid-reload legs drop or keep the pace by nerve...
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->current_weapon_ = Weapon::AK47;
  BehaviorHook::SetWeaponType (*bot, WeaponType::Rifle);
  BehaviorHook::SetReload (*bot, true);
  BehaviorHook::SetStates (*bot, Sense::SuspectEnemy);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (3.0f);
  bot->fear_level_ = 0.9f;
  bot->agression_level_ = 0.1f;
  BehaviorHook::Override (*bot);
  CHECK (BehaviorHook::Move (*bot) == 0.0f);

  // ...full magazines stand the reload down...
  BehaviorHook::SetClip (*bot, Weapon::AK47, 30);
  BehaviorHook::Override (*bot);
  CHECK (BehaviorHook::Move (*bot) == 0.0f);
  BehaviorHook::SetReload (*bot, true);

  bot->fear_level_ = 0.1f;
  bot->agression_level_ = 0.9f;
  bot->num_enemies_left_ = 5;
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::Override (*bot);
  CHECK (BehaviorHook::Move (*bot) == BehaviorHook::ShiftSpeed (*bot));
  CHECK (BehaviorHook::Move (*bot) != 0.0f);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_breakables") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // escape flag + breakables need their entities before activate
  // (pre-activate spawns seed the validity map, so the interest
  // scan lists them later)
  edict_t *esc = engine.SpawnEntity ("func_escapezone");
  HOST_REQUIRE (esc != nullptr);

  auto make_brk = [&] (float x) -> edict_t * {
    edict_t *ent = engine.SpawnEntity ("func_breakable");

    if (ent == nullptr) {
      return nullptr;
    }
    ent->v.health = 50.0f;
    ent->v.takedamage = 1.0f;
    ent->v.impulse = 0;
    ent->v.spawnflags = 0;
    ent->v.movetype = MOVETYPE_PUSH;
    ent->v.origin = ystl::Vector (x, 0.0f, 0.0f);
    return ent;
  };

  edict_t *brk = make_brk (400.0f);
  edict_t *far_brk = make_brk (2000.0f);
  edict_t *near_ign = make_brk (150.0f);
  edict_t *near_close = make_brk (170.0f);
  HOST_REQUIRE (brk != nullptr && far_brk != nullptr && near_ign != nullptr && near_close != nullptr);

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
  HOST_REQUIRE (game.MapIs (MapFlags::Escape));
  HOST_REQUIRE (game.HasBreakables ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("BrkB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("BrkB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->maxspeed = 270.0f;
  bot->pev->weapons |= ystl::bit (Weapon::AK47);
  bot->current_weapon_ = Weapon::AK47;
  bot->buying_finished_ = true;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  // ...past the spawn decor grace, interested and looking...
  engine.AdvanceTime (6.0f);
  engine.AdvanceTime (0.6f);
  game_state.UpdateInterestingEntities ();

  // ...the 25% random skip stays out...
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () == TaskId::ShootBreakable);

  // ...the random skip keeps the feet moving instead...
  bot->ClearTasks ();
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...an aimed trace finds the breakable straight ahead...
  engine.SetTraceLineHook ([brk] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = brk;
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  BehaviorHook::SetBreakable (*bot, nullptr);
  CHECK (BehaviorHook::Lookup (*bot) == brk);

  // ...and the touch path adopts it through the same lookup...
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, nullptr);
  CHECK (BehaviorHook::Breakable (*bot) == brk);

  // ...cached breakables revalidate team and ignore list...
  brk->v.team = 99;
  CHECK (BehaviorHook::Lookup (*bot) == nullptr);
  brk->v.team = 0;

  BehaviorHook::SetBreakable (*bot, brk);
  BehaviorHook::IgnoreBreakable (*bot, brk);
  CHECK (BehaviorHook::Lookup (*bot) == nullptr);
  BehaviorHook::ClearIgnoredBreakable (*bot);

  BehaviorHook::SetBreakable (*bot, brk);
  CHECK (BehaviorHook::Lookup (*bot) == brk);

  // ...the eye-level trace rescues what the feet miss...
  engine.SetTraceLineHook ([brk] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (v1[2] > 20.0f) {
      out->flFraction = 0.5f;
      out->pHit = brk;
    }
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  BehaviorHook::SetBreakable (*bot, nullptr);
  CHECK (BehaviorHook::Lookup (*bot) == brk);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL

  // ...ignored breakables never resolve...
  BehaviorHook::IgnoreBreakable (*bot, brk);
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, brk);
  CHECK (BehaviorHook::Breakable (*bot) == nullptr);
  BehaviorHook::ClearIgnoredBreakable (*bot);

  // ...enemies and frozen legs stop the tracking cold...
  edict_t *foe = game.CreateFakeClient ("BrkFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  clients.Update ();
  clients[foe].team = Team::CT;

  BehaviorHook::SetEnemy (*bot, foe);
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, brk);
  CHECK (BehaviorHook::Breakable (*bot) == nullptr);
  BehaviorHook::ClearEnemy (*bot);

  bot->pev->maxspeed = 5.0f;
  BehaviorHook::CheckBreakable (*bot, brk);
  CHECK (BehaviorHook::Breakable (*bot) == nullptr);
  bot->pev->maxspeed = 270.0f;

  // ...far touch entities track by pointer...
  edict_t *far = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (far != nullptr);
  far->v.health = 50.0f;
  far->v.takedamage = 1.0f;
  far->v.impulse = 0;
  far->v.spawnflags = 0;
  far->v.movetype = MOVETYPE_PUSH;
  far->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::CheckBreakable (*bot, far);
  CHECK (bot->GetTaskId () == TaskId::ShootBreakable);
  bot->ClearTasks ();

  // ...a stale quarry gets blacklisted after crying wolf...
  BehaviorHook::SetBreakable (*bot, nullptr);
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  bot->ClearTasks ();

  engine.AdvanceTime (2.0f); // past the 1.5s give-up horizon
  game_state.UpdateInterestingEntities ();
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...pickups, the ignored, the dead, the far and the near all filter out...
  // (no re-scan: the dead brk must stay listed to trip the damage check)
  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);
  box->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);

  brk->v.health = 0.0f; // dead decor stops being a target, stays listed
  BehaviorHook::ClearIgnoredBreakable (*bot);
  BehaviorHook::IgnoreBreakable (*bot, near_ign);
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...fresh pickups filter out on their own...
  engine.AdvanceTime (0.6f);
  game_state.UpdateInterestingEntities ();
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::CheckAround (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...terrorists skip the buy line on escape maps...
  bot->ClearTasks ();
  bot->in_buy_zone_ = false;
  BehaviorHook::BuyTimer (*bot, 0.0f);
  engine.AdvanceTime (0.1f); // let the rearmed timer lapse
  BehaviorHook::SetBuyState (*bot, BuyState::PrimaryWeapon);
  BehaviorHook::Queue (*bot).clear ();
  bot->PushMsgQueue (Msg::Buy);
  BehaviorHook::CheckQueue (*bot);
  CHECK (bot->buying_finished_);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_teleport") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // demolition + rescue flags need their entities before activate
  edict_t *target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (target != nullptr);

  edict_t *telhos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (telhos != nullptr);
  telhos->v.health = 100.0f;
  telhos->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();

  // worldspawn grounds the null-entity checks behind the wall scan
  edict_t *world = engine.SpawnEntity ("worldspawn");
  HOST_REQUIRE (world != nullptr);
  game.OnSpawnEntity (world);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildBehaviorGraph ();
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("TelB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TelB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::CT;
  bot->is_alive_ = true;
  bot->pev->health = 100.0f;
  bot->pev->maxspeed = 250.0f;
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[bot->Ent ()].team = Team::CT;

  // ...no walls, no cheat...
  game_state.SetBombPlanted (true); // planted with no origin lookup yet
  CHECK (!BehaviorHook::Teleport (*bot));

  game_state.SetBombOrigin (false, ystl::Vector (600.0f, 0.0f, 0.0f));
  mode_walls.Reset ();
  CHECK (!BehaviorHook::Teleport (*bot));

  // ...a walled partition between bot and plant pulls the bot inside...
  // (the wall must not cover the plant itself, or no free spot exists)
  edict_t *wall = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (wall != nullptr);
  wall->v.solid = SOLID_BBOX;
  wall->v.absmin = ystl::Vector (250.0f, -100.0f, -100.0f);
  wall->v.absmax = ystl::Vector (350.0f, 100.0f, 100.0f);

  // thin slab between the node row and the second plant: graph nodes stay
  // free but every node-to-plant segment crosses it, forcing radial search
  edict_t *slab = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (slab != nullptr);
  slab->v.solid = SOLID_BBOX;
  slab->v.absmin = ystl::Vector (550.0f, 20.0f, -100.0f);
  slab->v.absmax = ystl::Vector (650.0f, 130.0f, 100.0f);

  // low cover over a neighboring node: the graph scan skips it,
  // the plant stays reachable through the other nodes
  edict_t *cover = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (cover != nullptr);
  cover->v.solid = SOLID_BBOX;
  cover->v.absmin = ystl::Vector (480.0f, -30.0f, -100.0f);
  cover->v.absmax = ystl::Vector (520.0f, 30.0f, 100.0f);

  mode_walls.OnRoundStart ();
  HOST_REQUIRE (mode_walls.HasWalls ());

  // ...dead, unplanted, escaping, stray-team and origin-less states stay out...
  bot->is_alive_ = false;
  CHECK (!BehaviorHook::Teleport (*bot));
  bot->is_alive_ = true;

  game_state.SetBombPlanted (false);
  CHECK (!BehaviorHook::Teleport (*bot));
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (600.0f, 0.0f, 0.0f));

  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  CHECK (!BehaviorHook::Teleport (*bot));
  bot->ClearTasks ();

  bot->team_ = Team::Spectator;
  CHECK (!BehaviorHook::Teleport (*bot));
  bot->team_ = Team::CT;

  game_state.SetBombOrigin (true); // reset drops the plant too
  game_state.SetBombPlanted (true); // planted with no origin lookup yet
  CHECK (!BehaviorHook::Teleport (*bot));
  game_state.SetBombOrigin (false, ystl::Vector (600.0f, 0.0f, 0.0f));

  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  CHECK (BehaviorHook::Teleport (*bot));
  CHECK (bot->pev->origin.distance_sq (ystl::Vector (600.0f, 0.0f, 0.0f)) < ystl::sqrf (300.0f));

  // ...standing on the plant needs no cheat...
  bot->pev->origin = ystl::Vector (600.0f, 0.0f, 0.0f);
  CHECK (!BehaviorHook::Teleport (*bot));

  // ...sitters within arm's reach stay put too...
  bot->pev->origin = ystl::Vector (600.0f, 40.0f, 0.0f);
  CHECK (!BehaviorHook::Teleport (*bot));

  // ...escaping bots walk out on their own...
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  CHECK (!BehaviorHook::Teleport (*bot));

  // ...a plant behind the slab misses every graph node and lands radial...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (600.0f, 150.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  CHECK (BehaviorHook::Teleport (*bot));
  CHECK (bot->pev->origin.distance_sq (ystl::Vector (600.0f, 150.0f, 0.0f)) < ystl::sqrf (150.0f));

  // ...a plant sealed on all sides waves the cheat off...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (500.0f, 0.0f, 0.0f));
  bot->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  CHECK (!BehaviorHook::Teleport (*bot));

  // ...rescuers count their hostages on the way out...
  BehaviorHook::PushHostage (*bot, telhos);
  BehaviorHook::Update (*bot);
  CHECK (bot->has_hostage_);

  // ...a trailed hostage makes the rescuer halt so it can catch up...
  telhos->v.health = 100.0f;
  telhos->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->origin = ystl::Vector (1100.0f, 0.0f, 0.0f); // inside straggler range, past follow range
  bot->has_hostage_ = false;
  bot->enemy_ = nullptr;
  bot->agression_level_ = 0.0f; // keep the endgame-rush path out of the way
  BehaviorHook::SetStates (*bot, Sense::Invalid);
  bot->ClearTasks ();
  BehaviorHook::HostageEscort (*bot);
  CHECK (bot->GetTaskId () == TaskId::Pause);

  // ...a hostage that truly fell behind is left alone...
  bot->pev->origin = ystl::Vector (4200.0f, 0.0f, 0.0f);
  bot->ClearTasks ();
  BehaviorHook::HostageEscort (*bot);
  CHECK (bot->GetTaskId () != TaskId::Pause);

  // ...and halting while an enemy is in sight is never an option...
  bot->pev->origin = ystl::Vector (1100.0f, 0.0f, 0.0f);
  bot->enemy_ = game.CreateFakeClient ("EscortFoe");
  bot->ClearTasks ();
  BehaviorHook::HostageEscort (*bot);
  CHECK (bot->GetTaskId () != TaskId::Pause);
  bot->enemy_ = nullptr;

  // ...a hostage that never catches up is eventually abandoned...
  bot->ClearTasks ();
  BehaviorHook::GiveUpHostageWait (*bot);
  BehaviorHook::HostageEscort (*bot);
  CHECK (bot->GetTaskId () != TaskId::Pause);

  // ...overstayed bots rotate out through the slow frame...
  cv_rotate_bots.Set (1);
  BehaviorHook::ExpireStay (*bot);
  engine.AdvanceTime (0.1f); // let the stay timer lapse
  BehaviorHook::Slow (*bot);
  cv_rotate_bots.Set (0);
  CHECK (bot->kicked_by_rotation_);

  mode_walls.Reset ();
  game_state.SetBombPlanted (false);
  game_state.SetBombOrigin (true);

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_comms") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("ComA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("ComB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *a = testhost::FindBot ("ComA");
  Bot *b = testhost::FindBot ("ComB");
  HOST_REQUIRE (a != nullptr && b != nullptr);
  a->team_ = Team::Terrorist;
  b->team_ = Team::Terrorist;
  a->is_alive_ = true;
  b->is_alive_ = true;
  BehaviorHook::Queue (*a).clear ();
  BehaviorHook::Queue (*b).clear ();

  // ...plain say goes out on the public channel...
  BehaviorHook::SetChat (*a, "hello all");
  a->PushMsgQueue (Msg::Say);
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).empty ());

  // ...repeat chatter only re-queues (the same-message squash needs a
  // stamp both older than d and newer than d/2, impossible in one sample)...
  cv_radio_mode.Set (1);
  engine.AdvanceTime (10.0f);
  bots.SetLastRadioTimestamp (Team::Terrorist, 0.0f);
  bots.SetLastRadio (Team::Terrorist, RadioChat::Invalid);

  BehaviorHook::Queue (*a).clear ();
  bots.SetLastRadioTimestamp (Team::Terrorist, game.Time ());
  BehaviorHook::SetRadioSelect (*a, RadioChat::CoverMe);
  BehaviorHook::SetForceRadio (*a, true);
  BehaviorHook::Rng (*a).force_float (1.0f);
  a->PushMsgQueue (Msg::Radio);
  BehaviorHook::Rng (*a).clear_forced ();
  BehaviorHook::CheckQueue (*a);
  CHECK (BehaviorHook::Queue (*a).size () == 1);

  // ...model names feed the creature mask...
  engine.SetInfoKey ("model", "zombie");
  BehaviorHook::RefreshModel (*a, nullptr);
  CHECK (BehaviorHook::IsCreatureBot (*a));

  engine.SetInfoKey ("model", "ab");
  BehaviorHook::RefreshModel (*a, nullptr);
  CHECK (!BehaviorHook::IsCreatureBot (*a));

  bots.Destroy ();
}

TEST_CASE ("unit/behavior_hearing2") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBehavior (engine, cs);

  bots.Addbot ("HearB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("HearB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->current_weapon_ = Weapon::AK47;
  BehaviorHook::SetWeaponType (*bot, WeaponType::Rifle);
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  auto make_noise = [&] (const char *name, const ystl::Vector &origin, Noise type, float dist) -> edict_t * {
    edict_t *human = game.CreateFakeClient (name);

    if (game.IsNullEntity (human)) {
      return nullptr;
    }
    human->v.flags &= ~FL_FAKECLIENT;
    human->v.health = 100.0f;
    human->v.deadflag = DEAD_NO;
    human->v.origin = origin;
    clients.Update ();
    clients[human].team = Team::CT;

    Client &client = clients[human];
    client.noise.dist = dist;
    client.noise.last = game.Time () + 10.0f;
    client.noise.start = game.Time ();
    client.noise.pos = origin;
    client.noise.type = type;
    return human;
  };

  edict_t *h1 = make_noise ("Hear1", ystl::Vector (300.0f, 0.0f, 0.0f), Noise::WeaponFire, 5000.0f);
  edict_t *h2 = make_noise ("Hear2", ystl::Vector (210.0f, 0.0f, 0.0f), Noise::Defuse, 5000.0f);
  edict_t *h3 = make_noise ("Hear3", ystl::Vector (2000.0f, 0.0f, 0.0f), Noise::WeaponFire, 5000.0f);
  HOST_REQUIRE (h1 != nullptr && h2 != nullptr && h3 != nullptr);

  auto silence = [&] (edict_t *ent) {
    clients[ent].noise.last = game.Time () - 1.0f;
  };
  auto refresh = [&] (edict_t *ent) {
    clients[ent].noise.last = game.Time () + 10.0f;
    clients[ent].noise.start = game.Time ();
  };

  // ...tracked suspects answer without a search...
  BehaviorHook::SetLastEnemy (*bot, h2);
  BehaviorHook::SetTracking (*bot, h2);
  engine.AdvanceTime (0.3f);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == h2);
  CHECK (has_flag (BehaviorHook::States (*bot), Sense::HearingEnemy));

  // ...godmode suspects stay unheard...
  silence (h1);
  silence (h2);
  refresh (h3);
  BehaviorHook::ClearLastEnemy (*bot);
  BehaviorHook::SetTracking (*bot, nullptr);
  h3->v.flags |= FL_GODMODE;
  cv_check_enemy_invincibility.Set (1);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == nullptr);
  cv_check_enemy_invincibility.Revert ();
  h3->v.flags &= ~FL_GODMODE;

  // ...out-of-range pops stay unheard too...
  clients[h3].noise.dist = 50.0f;
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == nullptr);
  clients[h3].noise.dist = 5000.0f;

  // ...blast and impact scoring still resolves...
  refresh (h1);
  refresh (h2);
  silence (h3);
  clients[h1].noise.type = Noise::Ricochet;
  clients[h2].noise.type = Noise::Explosion;
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::ClearLastEnemy (*bot);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) != nullptr);
  clients[h1].noise.type = Noise::WeaponFire;
  clients[h2].noise.type = Noise::Defuse;

  // ...seen suspects short-circuit the refresh...
  BehaviorHook::SetStates (*bot, Sense::SeeingEnemy);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == h2);
  BehaviorHook::SetStates (*bot, Sense::Invalid);

  // ...nearer pops steal the trail...
  BehaviorHook::SetLastEnemy (*bot, h1);
  engine.AdvanceTime (1.5f);
  refresh (h1);
  refresh (h2);
  BehaviorHook::Hearing (*bot);
  CHECK (BehaviorHook::HeardEnemy (*bot) == h2);
  CHECK (BehaviorHook::LastEnemy (*bot) == h2);

  // ...walled-in trails engage and roll through the wall...
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f; // every sight line stops halfway
    out->vecEndPos.x = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos.y = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos.z = v1[2] + (v2[2] - v1[2]) * 0.5f;
  });
  engine.AdvanceTime (0.2f); // past the 0.1s trace cache TTL
  silence (h1);
  silence (h3);
  refresh (h2);
  BehaviorHook::SetStates (*bot, Sense::Invalid);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (1.5f); // old enough to compare, fresh enough to engage
  refresh (h2);
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::Hearing (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (has_flag (BehaviorHook::States (*bot), Sense::HearingEnemy));
  CHECK (!has_flag (BehaviorHook::AimState (*bot), AimFlags::Enemy));
  CHECK (has_flag (BehaviorHook::States (*bot), Sense::SuspectEnemy));
  CHECK (has_flag (BehaviorHook::AimState (*bot), AimFlags::LastEnemy));

  // ...failed rolls log and fall back...
  engine.AdvanceTime (2.0f); // past the thru-wall hold
  bot->see_enemy_timer_.start ();
  refresh (h2);
  BehaviorHook::PinPenetration (*bot, h2);
  BehaviorHook::Rng (*bot).force_chance (false);
  BehaviorHook::Hearing (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  engine.SetTraceLineHook (nullptr);

  // ...fresh contacts radio the team...
  BehaviorHook::ClearEnemy (*bot);
  BehaviorHook::ClearLastEnemy (*bot);
  engine.AdvanceTime (8.0f);
  refresh (h1);
  silence (h2);
  silence (h3);
  BehaviorHook::Rng (*bot).force_chance (true);
  BehaviorHook::Hearing (*bot);
  BehaviorHook::Rng (*bot).clear_forced ();
  CHECK (BehaviorHook::HeardEnemy (*bot) == h1);

  bots.Destroy ();
}

} // namespace bot
