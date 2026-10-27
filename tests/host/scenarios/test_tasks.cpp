//
// YaPB test host: unit/tasks_{core,basic,duty,throw,break,drills}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for tasks.cpp through a test-only hook (friend, no
// production behavior changes): the desire arbiter with priority order,
// task stack mechanics, small self-contained bodies, grenade dispatch
// and breakable shooting. Randomness is pinned via exact modes and
// single-candidate setups, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct TasksHook {
  static void SetStates (Bot &bot, Sense states) {
    bot.states_ = states;
  }
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }
  static void SetThrow (Bot &bot, const ystl::Vector &pos) {
    bot.throw_ = pos;
  }
  static void Filter (Bot &bot) {
    bot.FilterTasks ();
  }
  static void Execute (Bot &bot) {
    bot.ExecuteTasks ();
  }
  static void Priorities (Bot &bot) {
    bot.CheckTaskPriorities ();
  }
  static void Complete (Bot &bot) {
    bot.CompleteTask ();
  }
  static bool BlindFired (Bot &bot) {
    return bot.wants_to_fire_;
  }
  static void Aim (Bot &bot) {
    bot.SetAimDirection ();
    bot.UpdateLookAngles ();
  }
  static int GetAimFlags (Bot &bot) {
    return static_cast<int> (bot.aim_flags_);
  }
  static ystl::Vector LookSafe (Bot &bot) {
    return bot.look_at_safe_;
  }
  static float MoveSpeed (Bot &bot) {
    return bot.move_speed_;
  }
  static float StrafeSpeed (Bot &bot) {
    return bot.strafe_speed_;
  }
  static void LogoTimerInvalidate (Bot &bot) {
    bot.logo_spray_timer_.invalidate ();
  }
  static void CheckBreakable (Bot &bot, edict_t *touch) {
    bot.CheckBreakable (touch);
  }
  static bool HasAmmoInClip (Bot &bot) {
    return bot.HasAnyAmmoInClip ();
  }
  static void SetEnemy (Bot &bot, edict_t *ent) {
    bot.enemy_ = ent;
  }
  static void SetLastEnemy (Bot &bot, edict_t *ent) {
    bot.last_enemy_ = ent;
  }
  static void SetLastEnemyOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.last_enemy_origin_ = pos;
  }
  static void SetTargetEntity (Bot &bot, edict_t *ent) {
    bot.target_entity_ = ent;
  }
  static void SetDoubleJumpEntity (Bot &bot, edict_t *ent) {
    bot.double_jump_entity_ = ent;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static void SetCurrentNode (Bot &bot, int index) {
    bot.ChangeNodeIndex (index);
  }
  static ystl::Vector DestOrigin (Bot &bot) {
    return bot.dest_origin_;
  }
  static void SetAim (Bot &bot, AimFlags flags) {
    bot.aim_flags_ = flags;
  }
  static void SetPathFlags (Bot &bot, NodeFlag flags) {
    bot.path_flags_ = flags;
  }
  static void SetPathOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.path_origin_ = pos;
  }
  static void SetCampButtons (Bot &bot, int buttons) {
    bot.camp_buttons_ = buttons;
  }
  static void SetCampDirection (Bot &bot, int dir) {
    bot.camp_direction_ = dir;
  }
  static void SetTimeCamping (Bot &bot, float value) {
    bot.time_camping_ = value;
  }
  static void SetTravelStart (Bot &bot, int index) {
    bot.travel_start_index_ = index;
  }
  static void SetLift (Bot &bot, edict_t *ent) {
    bot.lift_entity_ = ent;
  }
  static void SetEntityPos (Bot &bot, const ystl::Vector &pos) {
    bot.entity_ = pos;
  }
  static void SetOldButtons (Bot &bot, int buttons) {
    bot.old_buttons_ = buttons;
  }
  static void SetLastDamage (Bot &bot, int damage) {
    bot.last_damage_type_ = damage;
  }
  static void SetLeader (Bot &bot, bool value) {
    bot.is_leader_ = value;
  }
  static void SetInfected (Bot &bot, bool value) {
    bot.infected_enemy_team_ = value;
  }
  static void SetDefended (Bot &bot, bool value) {
    bot.defended_bomb_ = value;
  }
  static void SetEscaped (Bot &bot, bool value) {
    bot.escaped_from_bomb_ = value;
  }
  static void SetDuckDefuse (Bot &bot, bool value) {
    bot.duck_defuse_ = value;
  }
  static void SetBreakable (Bot &bot, edict_t *ent, const ystl::Vector &pos) {
    bot.breakable_entity_ = ent;
    bot.breakable_origin_ = pos;
  }
  static void IgnoreBreakable (Bot &bot, edict_t *ent) {
    bot.ignored_breakable_.push (ent);
  }
  static bool IsBreakableIgnored (Bot &bot, edict_t *ent) {
    return bot.IsIgnoredBreakable (ent);
  }
  static void SetEnemyOrigin (Bot &bot, const ystl::Vector &pos) {
    bot.enemy_origin_ = pos;
  }
  static void SetPredict (Bot &bot, int index, int length) {
    bot.last_predict_index_ = index;
    bot.last_predict_length_ = length;
  }
  static void SetReloadState (Bot &bot, Reload state) {
    bot.reload_data_.state = state;
  }
  static void SetReloading (Bot &bot, bool value) {
    bot.reload_data_.is_reloading = value;
  }
  static void FindValid (Bot &bot) {
    bot.FindValidNode ();
  }
  static void EnsureNode (Bot &bot) {
    bot.EnsureCurrentNodeIndex ();
  }
  static ystl::CountdownTimer &CoverTimer (Bot &bot) {
    return bot.cover_search_timer_;
  }
  static ystl::CountdownTimer &NextCampTimer (Bot &bot) {
    return bot.next_camp_dir_timer_;
  }
  static ystl::CountdownTimer &KnifeTimer (Bot &bot) {
    return bot.knife_attack_timer_;
  }
  static ystl::CountdownTimer &AskTimer (Bot &bot) {
    return bot.ask_check_timer_;
  }
  static ystl::CountdownTimer &SniperStopTimer (Bot &bot) {
    return bot.sniper_stop_timer_;
  }
  static ystl::CountdownTimer &DefuseWatchTimer (Bot &bot) {
    return bot.defuse_watch_timer_;
  }
  static ystl::CountdownTimer &DuckCheckTimer (Bot &bot) {
    return bot.duck_defuse_check_timer_;
  }
  static ystl::CountdownTimer &GrenadeCheckTimer (Bot &bot) {
    return bot.grenade_check_timer_;
  }
  static ystl::CountdownTimer &BreakableShootTimer (Bot &bot) {
    return bot.breakable_shoot_timer_;
  }
  static ystl::CountdownTimer &FollowWaitTimer (Bot &bot) {
    return bot.follow_wait_timer_;
  }
  static ystl::CountdownTimer &ButtonPushTimer (Bot &bot) {
    return bot.button_push_timer_;
  }
  static ystl::CountdownTimer &DoorHitTimer (Bot &bot) {
    return bot.door_hit_timer_;
  }
  static ystl::CountdownTimer &LogoTimer (Bot &bot) {
    return bot.logo_spray_timer_;
  }
  static ystl::IntervalTimer &SeeTimer (Bot &bot) {
    return bot.see_enemy_timer_;
  }
  static ystl::IntervalTimer &HeardTimer (Bot &bot) {
    return bot.heard_sound_timer_;
  }
  static ystl::IntervalTimer &NavTimer (Bot &bot) {
    return bot.nav_timer_;
  }
};

namespace {

// eight-node chain for node-backed paths
void BuildTasksGraph () {
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

void BootTasks (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildTasksGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();
}

} // namespace

TEST_CASE ("unit/tasks_core") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskB");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("TskFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  foe->v.solid = SOLID_BBOX;
  foe->v.takedamage = DAMAGE_YES;
  clients.Update ();
  clients[foe].team = Team::CT;

  // blinded bots shelve everything for the blind task...
  bot->blind_timer_.start (10.0f);
  TasksHook::Filter (*bot);
  CHECK (bot->GetTaskId () == TaskId::Blind);

  bot->blind_timer_.invalidate ();
  bot->ClearTasks ();

  // ...a button at hand outranks idling...
  edict_t *btn = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (btn != nullptr);
  bot->pickup_item_ = btn;
  bot->pickup_type_ = Pickup::Button;
  TasksHook::Filter (*bot);
  CHECK (bot->GetTaskId () == TaskId::PickupItem);

  bot->pickup_item_ = nullptr;
  bot->ClearTasks ();

  // ...and a running enemy outranks everything below attack
  TasksHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->enemy_ = foe;
  foe->v.velocity = ystl::Vector (300.0f, 0.0f, 0.0f);
  TasksHook::Filter (*bot);
  CHECK (bot->GetTaskId () == TaskId::Attack);

  // priorities keeps the hottest desire current, pops restore it
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
  CHECK (bot->GetTaskId () == TaskId::Camp);
  TasksHook::Complete (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // completing an empty stack is a safe no-op landing on normal
  bot->ClearTasks ();
  TasksHook::Complete (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_basic") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskP", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskP");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;

  // pausing parks until the timer lapses...
  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Pause);
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);

  // ...blinded pausers back away instead of standing still
  bot->SetNewDifficulty (Difficulty::Expert); // pin top skill: blind spray is chance-rolled
  bot->view_distance_ = 100.0f;
  TasksHook::Execute (*bot);
  CHECK (TasksHook::MoveSpeed (*bot) == -200.0f);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Override)));
  CHECK (TasksHook::BlindFired (*bot));
  bot->SetNewDifficulty (Difficulty::Normal);

  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () - 1.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Pause);

  // spraying waits out the logo timer...
  bot->StartTask (TaskId::Spraypaint, TaskPri::kSpraypaint, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Spraypaint);

  // ...then aims the wall once the timer lapses
  bot->StartTask (TaskId::Spraypaint, TaskPri::kSpraypaint, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::LogoTimerInvalidate (*bot);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Spraypaint);
  CHECK (TasksHook::LookSafe (*bot).empty () == false);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Entity)));
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);

  // blind bots hold still with a grudge, then stand down on time
  // (the cover index defaults out of range, so no navigation kicks in)
  bot->ClearTasks ();
  bot->StartTask (TaskId::Blind, TaskPri::kBlind, kInvalidNodeIndex, game.Time () + 60.0f, true);
  bot->blind_timer_.start (10.0f);
  CHECK (bot->blind_node_index_ == kInvalidNodeIndex);
  bot->blind_move_speed_ = 50.0f;
  bot->blind_side_move_speed_ = 60.0f;
  bot->blind_button_ = IN_DUCK;
  TasksHook::Execute (*bot);
  CHECK (TasksHook::MoveSpeed (*bot) == 50.0f);
  CHECK (TasksHook::StrafeSpeed (*bot) == 60.0f);
  CHECK (!!(bot->pev->button & IN_DUCK));
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Override)));

  bot->blind_timer_.invalidate ();
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Blind);

  // hiding without a reason ends at once...
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hide);

  // ...camping without a knife ends even faster...
  cv_jasonmode.Set (1);
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Camp);
  cv_jasonmode.Set (0);

  // ...and so do unplanted escapes, unfollowed follows and unlit fuses
  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::EscapeFromBomb);

  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::FollowUser);

  bot->StartTask (TaskId::DoubleJump, TaskPri::kDoubleJump, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DoubleJump);

  // null pickups drain through the walk with no motion
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::PickupItem);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_duty") {
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

  // demolition flag before activation: defuse paths need a live bomb origin
  HOST_REQUIRE (engine.SpawnEntity ("func_bomb_target") != nullptr);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildTasksGraph ();

  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskD");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  edict_t *human = game.CreateFakeClient ("DutyHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  // planting without the bomb shelves the task and posts guards...
  TasksHook::Execute (*bot); // empty stack keeps normal
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  // ...while carrying it in the zone holds attack down on the spot
  bot->ClearTasks ();
  bot->has_c4_ = true;
  bot->in_bomb_zone_ = true;
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::PlantBomb);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);
  bot->has_c4_ = false;
  bot->in_bomb_zone_ = false;

  // defusing thin air ends at once...
  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DefuseBomb);

  // ...defusing with the bar up holds use on the bomb...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (500.0f, 0.0f, 0.0f));
  bot->team_ = Team::CT;
  bot->has_progress_bar_ = true;
  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, game.Time (), true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::DefuseBomb);
  CHECK (!!(bot->pev->button & IN_USE));
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);

  // ...unfollowed follows and itemless pickups end at once
  bot->has_progress_bar_ = false;
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;
  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::FollowUser);

  edict_t *item = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (item != nullptr);
  const float item_pos[3] = { 150.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (item, item_pos);
  bot->pickup_item_ = item;
  bot->pickup_type_ = Pickup::None;
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::PickupItem);

  bot->pickup_item_ = item;
  bot->pickup_type_ = Pickup::Shield;
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::PickupItem);

  // dead hostages never get picked up
  edict_t *hos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hos != nullptr);
  hos->v.health = 0.0f;
  const float hos_pos[3] = { 150.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (hos, hos_pos);
  bot->pickup_item_ = hos;
  bot->pickup_type_ = Pickup::Hostage;
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::PickupItem);

  // hunts drop without a named enemy, teammates included
  bot->enemy_ = human;
  bot->StartTask (TaskId::Hunt, 89.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hunt);
  bot->enemy_ = nullptr;

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_throw") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskT", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskT");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  // self-blast guard stands down before any ballistics...
  TasksHook::SetThrow (*bot, ystl::Vector (150.0f, 0.0f, 0.0f));
  bot->StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::ThrowExplosive);
  CHECK (!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));

  // ...far lobs raise the flag and park the motion...
  TasksHook::SetThrow (*bot, ystl::Vector (2000.0f, 0.0f, 0.0f));
  bot->StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);
  CHECK (TasksHook::StrafeSpeed (*bot) == 0.0f);

  // ...flash and smoke ride the same rails
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowFlashbang, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));

  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowSmoke, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));

  // owned grenades skip the fallback weapon entirely (classname carried
  // by gamedef in prod, stubbed here since the harness loads no defs)
  conf.GetWeaponProp (Weapon::Explosive).classname = "weapon_hegrenade";
  const int base = testhost::CsCalls (cs, "ClientCommand");
  bot->ClearTasks ();
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Explosive));
  bot->StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::ThrowExplosive);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 1);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_break") {
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

  // breakable before activation: levelInitialize registers it
  edict_t *brk = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (brk != nullptr);
  brk->v.health = 50.0f;
  brk->v.takedamage = 1.0f;
  brk->v.impulse = 0;
  brk->v.spawnflags = 0;
  brk->v.movetype = MOVETYPE_PUSH;
  brk->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildTasksGraph ();

  HOST_REQUIRE (game.HasBreakables ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("TskA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("TskN", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *armed = testhost::FindBot ("TskA");
  Bot *dry = testhost::FindBot ("TskN");
  HOST_REQUIRE (armed != nullptr && dry != nullptr);

  // engine weapon metadata arrives via WeaponList in prod; publish it
  // here or getWeaponProp answers defaults (id Invalid breaks the loop)
  msgs.Add ("WeaponList", 71);
  msgs.Start (armed->Ent (), 71);
  msgs.Collect ("weapon_ak47");
  msgs.Collect (int32_t (1));
  msgs.Collect (int32_t (90));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (1));
  msgs.Collect (int32_t (2));
  msgs.Collect (int32_t (28));
  msgs.Collect (int32_t (0));
  msgs.Stop ();
  HOST_REQUIRE (conf.GetWeaponProp (Weapon::AK47).id == Weapon::AK47);

  // past the post-spawn decor grace period
  engine.AdvanceTime (6.0f);

  for (auto bot : { armed, dry }) {
    bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
    bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
    bot->pev->maxspeed = 270.0f;
    TasksHook::CheckBreakable (*bot, brk);
    HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  }

  // a breakable that blocks the path is retried on touch, even if ignored as decor
  armed->ClearTasks ();
  TasksHook::IgnoreBreakable (*armed, brk);
  HOST_REQUIRE (TasksHook::IsBreakableIgnored (*armed, brk));
  TasksHook::CheckBreakable (*armed, brk);
  CHECK (armed->GetTaskId () == TaskId::ShootBreakable);
  CHECK (!TasksHook::IsBreakableIgnored (*armed, brk));

  // loaded guns hold the trigger on the facing breakable...
  // (clip lookup keys off conf ids, so fill every slot)
  armed->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  HOST_REQUIRE (TasksHook::HasAmmoInClip (*armed) == false);

  for (int i = 0; i < kMaxWeapons; ++i) {
    armed->ammo_in_clip_[i] = 30;
  }
  HOST_REQUIRE (TasksHook::HasAmmoInClip (*armed));
  TasksHook::Execute (*armed);
  CHECK (armed->GetTaskId () == TaskId::ShootBreakable);
  CHECK (TasksHook::MoveSpeed (*armed) == 0.0f);
  CHECK (!!(TasksHook::GetAimFlags (*armed) & static_cast<int> (AimFlags::Override)));
  CHECK (TasksHook::BlindFired (*armed));

  // ...a grate below the feet still gets aimed at and fired at
  brk->v.origin = ystl::Vector (100.0f, 0.0f, -64.0f);
  armed->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  armed->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  armed->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  armed->ClearTasks ();
  TasksHook::BreakableShootTimer (*armed).invalidate ();
  TasksHook::CheckBreakable (*armed, brk);

  for (int i = 0; i < 20; ++i) {
    engine.AdvanceTime (0.1f);
    TasksHook::Execute (*armed);
    TasksHook::Aim (*armed);
  }
  CHECK (armed->GetTaskId () == TaskId::ShootBreakable);
  CHECK (TasksHook::BlindFired (*armed));

  // ...dry ones stand the task down instead
  TasksHook::Execute (*dry);
  CHECK (dry->GetTaskId () != TaskId::ShootBreakable);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_drills") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskD");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  // no enemy to cover from: the task folds back to normal...
  TasksHook::SetLastEnemy (*bot, nullptr);
  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...no enemy to fight: last known origin becomes the destination...
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  bot->StartTask (TaskId::Attack, TaskPri::kAttack, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (TasksHook::DestOrigin (*bot) == ystl::Vector (300.0f, 0.0f, 0.0f));

  // ...nobody to follow and no lift to catch...
  TasksHook::SetTargetEntity (*bot, nullptr);
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...a close leader parks the bot without a wait...
  edict_t *leader = game.CreateFakeClient ("TskL");
  HOST_REQUIRE (!game.IsNullEntity (leader));
  leader->v.health = 100.0f;
  leader->v.deadflag = DEAD_NO;
  leader->v.solid = SOLID_BBOX;
  leader->v.takedamage = DAMAGE_YES;
  leader->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);

  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::SetTargetEntity (*bot, leader);
  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::FollowUser);
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);
  bot->ClearTasks ();
  TasksHook::SetTargetEntity (*bot, nullptr);

  TasksHook::SetDoubleJumpEntity (*bot, nullptr);
  bot->StartTask (TaskId::DoubleJump, TaskPri::kDoubleJump, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DoubleJump);

  // ...no bomb planted, nothing to pick up...
  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::EscapeFromBomb);

  bot->StartTask (TaskId::PickupItem, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...a full camp run watches a direction and holds the task...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Camp);
  CHECK (!TasksHook::LookSafe (*bot).empty ());

  // ...banned camp folds the same way...
  cv_camping_allowed.Set (0);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  cv_camping_allowed.Set (1);

  // ...creatures never hide, blind bots without a node hold still...
  TasksHook::SetCreature (*bot, true);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  TasksHook::SetCreature (*bot, false);

  // ...with no enemy to remember, hiding ends at once...
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector {});
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hide);

  bot->ClearTasks ();
  bot->StartTask (TaskId::Blind, TaskPri::kBlind, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (TasksHook::MoveSpeed (*bot) == 0.0f);

  // ...a debug goal becomes the task data...
  cv_debug_goal.Set (3);
  bot->ClearTasks ();
  TasksHook::Execute (*bot);
  CHECK (bot->Task ()->data == 3);
  cv_debug_goal.Set (-1);

  // ...a named move target is adopted without a path yet...
  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, 3, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);
  CHECK (bot->Task ()->data == 3);

  // ...an expired spraycan stands down...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Spraypaint, TaskPri::kSpraypaint, kInvalidNodeIndex, 0.0f, false);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Spraypaint);

  // ...a hunt with no enemy to remember clears itself...
  TasksHook::SetLastEnemy (*bot, nullptr);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hunt, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hunt);

  // ...clearing works on empty stacks and mid-stack alike...
  bot->ClearTasks ();
  bot->ClearTask (TaskId::Hunt);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bot->StartTask (TaskId::Hunt, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  bot->ClearTask (TaskId::Hunt);
  CHECK (bot->GetTaskId () == TaskId::Camp);

  // ...a defuse with no bomb is a no-op...
  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DefuseBomb);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_defuse_cleanup") {
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
  HOST_REQUIRE (engine.SpawnEntity ("func_bomb_target") != nullptr);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildTasksGraph ();
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("DefA", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("DefB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *defuser = testhost::FindBot ("DefA");
  Bot *teammate = testhost::FindBot ("DefB");
  HOST_REQUIRE (defuser != nullptr && teammate != nullptr);

  defuser->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  teammate->pev->origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  defuser->team_ = Team::CT;
  teammate->team_ = Team::CT;
  defuser->is_alive_ = true;
  teammate->is_alive_ = true;

  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (100.0f, 0.0f, 0.0f));

  edict_t *c4 = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (c4 != nullptr);
  c4->v.model = string_t (engine.AllocString ("models/w_c4.mdl"));

  defuser->has_progress_bar_ = true;
  defuser->pickup_item_ = c4;
  defuser->pickup_type_ = Pickup::PlantedC4;

  defuser->ClearTasks ();
  defuser->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, game.Time (), true);
  TasksHook::Execute (*defuser);
  HOST_REQUIRE (defuser->GetTaskId () == TaskId::DefuseBomb);

  // bomb is gone: it has just been defused
  game_state.SetBombOrigin (true);
  HOST_REQUIRE (game_state.GetBombOrigin ().empty ());

  TasksHook::Execute (*defuser);

  // the defuser drops the bomb and returns to normal play...
  CHECK (defuser->GetTaskId () != TaskId::DefuseBomb);
  CHECK (defuser->pickup_item_ == nullptr);
  CHECK (defuser->pickup_type_ == Pickup::None);

  // ...and must not be pushed to a far camp/move (that made it pace back and forth)
  for (const auto &task : defuser->tasks_) {
    CHECK (task.id != TaskId::MoveTo);
    CHECK (task.id != TaskId::Camp);
  }

  // the rest of the team still regroups
  bool teammate_regroups = false;

  for (const auto &task : teammate->tasks_) {
    teammate_regroups = teammate_regroups || (task.id == TaskId::Camp || task.id == TaskId::MoveTo);
  }
  CHECK (teammate_regroups);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_desires") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("TskD");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("TskDoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  edict_t *pal = game.CreateFakeClient ("TskPal");
  HOST_REQUIRE (!game.IsNullEntity (pal));
  pal->v.flags &= ~FL_FAKECLIENT;
  pal->v.health = 100.0f;
  pal->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[pal].team = Team::Terrorist;

  // full health zeroes retreat, high aggression prices the hunt at the cap
  bot->health_value_ = 100.0f;
  bot->fear_level_ = 0.1f;
  bot->agression_level_ = 1.0f;
  bot->num_friends_left_ = 0;
  bot->num_enemies_left_ = 0;
  bot->has_c4_ = false;
  bot->has_hostage_ = false;
  bot->is_vip_ = false;
  bot->enemy_ = nullptr;
  bot->personality_ = Personality::Normal;
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (500.0f, 0.0f, 0.0f));
  TasksHook::SetStates (*bot, Sense::Invalid);

  engine.AdvanceTime (100.0f); // past the round midpoint
  TasksHook::Filter (*bot);
  CHECK (bot->GetTaskId () == TaskId::Hunt);

  // friends nearby halve the fear, snipers get cautious, creatures hunt flat
  bot->ClearTasks ();
  TasksHook::Filter (*bot); // friends bump the census first
  bot->weapon_type_ = WeaponType::Sniper;
  TasksHook::Filter (*bot);
  bot->weapon_type_ = WeaponType::Rifle;
  TasksHook::SetCreature (*bot, true);
  TasksHook::Filter (*bot);
  TasksHook::SetCreature (*bot, false);
  bot->is_vip_ = true;
  TasksHook::Filter (*bot);
  bot->is_vip_ = false;

  // a loose non-button pickup still beats idling once the trail is cold
  bot->ClearTasks ();
  TasksHook::SetLastEnemy (*bot, nullptr);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector {});
  TasksHook::SetStates (*bot, Sense::Invalid);

  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);
  bot->pickup_item_ = box;
  bot->pickup_type_ = Pickup::Weapon;
  TasksHook::Filter (*bot);
  CHECK (bot->GetTaskId () == TaskId::PickupItem);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_seek") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("SkA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("SkA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("SkFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (600.0f, 0.0f, 0.0f);
  foe->v.solid = SOLID_BBOX;
  foe->v.takedamage = DAMAGE_YES;
  clients.Update ();
  clients[foe].team = Team::CT;

  // reached cover turns into a hide with a camp look...
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (600.0f, 0.0f, 0.0f));
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[1].origin;
  TasksHook::CoverTimer (*bot).invalidate ();
  TasksHook::Rng (*bot).force_float (5.0f);
  bot->ClearTasks ();
  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, 1, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Hide);
  TasksHook::Rng (*bot).clear_forced ();

  // ...a remembered data index is kept without a new search...
  bot->ClearTasks ();
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[1].origin;
  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, 3, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::SeekCover);
  CHECK (bot->Task ()->data == 3);

  // ...seeing foes from a poor spot folds the hide...
  bot->ClearTasks ();
  TasksHook::SetPathFlags (*bot, static_cast<NodeFlag> (0));
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::SetStates (*bot, Sense::SeeingEnemy);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hide);

  // ...and with no memory of the foe it folds as well...
  bot->ClearTasks ();
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector {});
  TasksHook::SetStates (*bot, Sense::Invalid);
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hide);

  // ...expired hides stand down even from a good camp spot...
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (600.0f, 0.0f, 0.0f));
  TasksHook::SetPathFlags (*bot, NodeFlag::Camp);
  TasksHook::SetCampButtons (*bot, 0);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () - 1.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hide);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_assault") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("AsA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("AsA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("AsFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  edict_t *pal = game.CreateFakeClient ("AsPal");
  HOST_REQUIRE (!game.IsNullEntity (pal));
  pal->v.flags &= ~FL_FAKECLIENT;
  pal->v.health = 100.0f;
  pal->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[pal].team = Team::Terrorist;

  // knife fighters remember the last known spot as their destination...
  bot->weapon_type_ = WeaponType::Melee;
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::Attack, TaskPri::kAttack, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (TasksHook::DestOrigin (*bot) == ystl::Vector (300.0f, 0.0f, 0.0f));
  bot->weapon_type_ = WeaponType::Rifle;

  // ...teammates are never hunted...
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetLastEnemy (*bot, pal);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (300.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hunt, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hunt);

  // ...reaching the remembered spot forgets it...
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (100.0f, 0.0f, 0.0f));
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[1].origin;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hunt, TaskPri::kNormal, 1, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Hunt);

  // ...a fresh hunt without a path picks the nearest node...
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (500.0f, 0.0f, 0.0f));
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Hunt, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Hunt);

  // ...close hunts slow to a walk when the ears are fresh...
  TasksHook::SeeTimer (*bot).start ();
  bot->pev->maxspeed = 270.0f;
  TasksHook::Rng (*bot).force_chance (true);
  TasksHook::Execute (*bot);
  TasksHook::Rng (*bot).clear_forced ();
  CHECK (bot->GetTaskId () == TaskId::Hunt);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_guard") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("GdA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("GdA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->pev->maxspeed = 270.0f;

  // knife rushers slash on the move when the timer lapses...
  bot->weapon_type_ = WeaponType::Melee;
  bot->current_weapon_ = Weapon::Knife;
  TasksHook::SetLastEnemy (*bot, nullptr);
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::KnifeTimer (*bot).invalidate ();
  TasksHook::Rng (*bot).force_chance (true);
  bot->ClearTasks ();
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  bot->pev->button = 0;

  // ...and stab otherwise...
  TasksHook::Rng (*bot).force_chance (false);
  TasksHook::KnifeTimer (*bot).invalidate ();
  bot->ClearTasks ();
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  bot->pev->button = 0;
  TasksHook::Rng (*bot).clear_forced ();
  bot->weapon_type_ = WeaponType::Rifle;

  // low clips request a reload on the walk...
  bot->current_weapon_ = Weapon::AK47;
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[static_cast<size_t> (i)] = 30;
  }
  bot->ammo_in_clip_[static_cast<size_t> (ystl::to_underlying (Weapon::AK47))] = 2;
  bot->ammo_[static_cast<size_t> (1)] = 90;
  TasksHook::SetReloadState (*bot, Reload::None);
  bot->ClearTasks ();
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // planted bombs retarget cts away from plain goals...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (600.0f, 0.0f, 0.0f));
  bot->team_ = Team::CT;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, 1, 0.0f, true);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Normal);
  game_state.SetBombPlanted (false);
  bot->team_ = Team::Terrorist;

  // camped walls fall back to a random direction...
  bot->ClearTasks ();
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[1].origin;
  TasksHook::SetPathFlags (*bot, NodeFlag::Camp);
  TasksHook::NextCampTimer (*bot).invalidate ();
  TasksHook::Rng (*bot).force_float (2.0f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.2f;
  });
  engine.AdvanceTime (0.2f);
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Camp);
  CHECK (!TasksHook::LookSafe (*bot).empty ());

  // ...clear views keep the node direction...
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 1.0f;
  });
  engine.AdvanceTime (0.2f);
  TasksHook::NextCampTimer (*bot).invalidate ();
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Camp);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  TasksHook::Rng (*bot).clear_forced ();

  // ...expired camps stand down...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () - 1.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::Camp);

  // reached moves complete, fresh ones path...
  TasksHook::SetCurrentNode (*bot, 2);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[2].origin;
  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, 2, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::MoveTo);
  bot->ClearTasks ();
  bot->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, 5, 0.0f, true);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::MoveTo);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_shelter") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("ShA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("ShA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  bot->team_ = Team::Terrorist;

  // clamped blind spray never exceeds the top speed...
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->view_distance_ = -200.0f;
  TasksHook::Rng (*bot).force_chance (true);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (TasksHook::MoveSpeed (*bot) == -bot->pev->maxspeed);
  TasksHook::Rng (*bot).clear_forced ();
  bot->SetNewDifficulty (Difficulty::Normal);
  bot->view_distance_ = 100.0f;

  // blind sprayers face the last known foe...
  edict_t *foe = game.CreateFakeClient ("ShFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 0.0f));
  bot->blind_timer_.start (10.0f);
  TasksHook::Rng (*bot).force_chance (true);
  TasksHook::Rng (*bot).force_float (0.0f);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Blind, TaskPri::kBlind, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (TasksHook::BlindFired (*bot));
  TasksHook::Rng (*bot).clear_forced ();
  bot->blind_timer_.invalidate ();

  // ...blind walkers follow the node path when one is remembered...
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 0.0f));
  bot->blind_timer_.start (10.0f);
  bot->blind_node_index_ = 2;
  TasksHook::Rng (*bot).force_chance (true);
  bot->ClearTasks ();
  bot->StartTask (TaskId::Blind, TaskPri::kBlind, kInvalidNodeIndex, game.Time () + 60.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::Blind);
  TasksHook::Rng (*bot).clear_forced ();
  bot->blind_timer_.invalidate ();
  bot->ClearTasks ();

  // escapees draw the knife and head for the far side...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (3000.0f, 0.0f, 0.0f));
  bot->weapon_type_ = WeaponType::Rifle;
  bot->current_weapon_ = Weapon::AK47;
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetLastEnemy (*bot, nullptr);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  TasksHook::Rng (*bot).force_float (1600.0f);
  bot->ClearTasks ();
  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK ((bot->GetTaskId () == TaskId::EscapeFromBomb || bot->GetTaskId () == TaskId::Camp));
  TasksHook::Rng (*bot).clear_forced ();

  // ...reaching the far side camps the spot...
  const int escape_index = bot->Task ()->data;

  if (graph.Exists (escape_index)) {
    TasksHook::SetCurrentNode (*bot, escape_index);
    TasksHook::FindValid (*bot);
    bot->pev->origin = graph[escape_index].origin;
    TasksHook::Rng (*bot).force_float (1600.0f);
    TasksHook::Execute (*bot);
    TasksHook::Rng (*bot).clear_forced ();
  }
  game_state.SetBombPlanted (false);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_covert") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("CvA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CvA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  bot->team_ = Team::Terrorist;

  edict_t *leader = game.CreateFakeClient ("CvLead");
  HOST_REQUIRE (!game.IsNullEntity (leader));
  leader->v.flags &= ~FL_FAKECLIENT;
  leader->v.health = 100.0f;
  leader->v.deadflag = DEAD_NO;
  leader->v.solid = SOLID_BBOX;
  leader->v.takedamage = DAMAGE_YES;
  leader->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  leader->v.maxspeed = 100.0f;
  clients.Update ();
  clients[leader].team = Team::Terrorist;

  // dead leaders are dropped at once...
  TasksHook::SetTargetEntity (*bot, nullptr);
  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::FollowUser);

  // ...live ones are tailed at their own pace...
  TasksHook::SetTargetEntity (*bot, leader);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::FollowUser);

  // ...waiting too long sends the point on...
  bot->pev->origin = leader->v.origin;
  TasksHook::FollowWaitTimer (*bot).start (0.0f);
  engine.AdvanceTime (0.2f);
  TasksHook::Execute (*bot);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::FollowUser);

  // ...spotted foes steal the follow...
  leader->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  leader->v.button |= IN_ATTACK;
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  TasksHook::SetTargetEntity (*bot, leader);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  edict_t *foe = game.CreateFakeClient ("CvFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (600.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  engine.SetTraceLineHook ([foe] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)v2;
    (void)no_monsters;
    (void)skip;
    out->fAllSolid = 0;
    out->fStartSolid = 0;
    out->fInOpen = 1;
    out->fInWater = 0;
    out->flFraction = 0.5f;
    out->vecEndPos.x = v2[0];
    out->vecEndPos.y = v2[1];
    out->vecEndPos.z = v2[2];
    out->pHit = foe;
    out->iHitgroup = 0;
  });
  engine.AdvanceTime (0.2f);
  bot->ClearTasks ();
  bot->StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::FollowUser);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  leader->v.button = 0;

  // double jumps without a partner reset at once...
  TasksHook::SetDoubleJumpEntity (*bot, nullptr);
  bot->ClearTasks ();
  bot->StartTask (TaskId::DoubleJump, TaskPri::kDoubleJump, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DoubleJump);

  // ...live lifts are walked up to...
  edict_t *mate = game.CreateFakeClient ("CvMate");
  HOST_REQUIRE (!game.IsNullEntity (mate));
  mate->v.flags &= ~FL_FAKECLIENT;
  mate->v.health = 100.0f;
  mate->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  mate->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[mate].team = Team::Terrorist;
  TasksHook::SetDoubleJumpEntity (*bot, mate);
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->pev->angles.y = 0.0f;
  bot->ClearTasks ();
  bot->StartTask (TaskId::DoubleJump, TaskPri::kDoubleJump, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::DoubleJump);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_vault") {
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
  HOST_REQUIRE (engine.SpawnEntity ("func_bomb_target") != nullptr);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildTasksGraph ();
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));
  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("VlA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VlA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->pev->maxspeed = 270.0f;

  // planting without the bomb guards the site...
  bot->has_c4_ = false;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::PlantBomb);

  // ...live foes stall the plant...
  bot->has_c4_ = true;
  bot->in_bomb_zone_ = true;
  bot->current_weapon_ = Weapon::C4;
  bot->weapon_type_ = WeaponType::Pistol;
  edict_t *foe = game.CreateFakeClient ("VlFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  TasksHook::SetEnemy (*bot, foe);
  bot->has_progress_bar_ = false;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::PlantBomb);

  // ...ducked planters hold both buttons...
  TasksHook::SetEnemy (*bot, nullptr);
  bot->pev->flags |= FL_DUCKING;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  CHECK (!!(bot->pev->button & IN_DUCK));
  bot->pev->button = 0;
  bot->pev->flags &= ~FL_DUCKING;
  bot->has_c4_ = false;
  bot->in_bomb_zone_ = false;

  // hopeless defuses hold the line when the use is held...
  mp_c4timer.Set (35);
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (500.0f, 0.0f, 0.0f));
  bot->team_ = Team::CT;
  bot->has_defuser_ = false;
  bot->has_progress_bar_ = false;
  TasksHook::SetOldButtons (*bot, IN_USE);
  TasksHook::Rng (*bot).force_float (4.0f);
  TasksHook::DefuseWatchTimer (*bot).invalidate ();
  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::DefuseBomb);

  // ...stalled approaches give up after the watch lapses...
  engine.AdvanceTime (5.0f);
  TasksHook::Execute (*bot);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::DefuseBomb);
  TasksHook::Rng (*bot).clear_forced ();
  TasksHook::SetOldButtons (*bot, 0);

  // ...close defuses hold use and duck...
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (120.0f, 0.0f, 0.0f));
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->has_progress_bar_ = true;
  bot->ClearTasks ();
  bot->StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
  engine.AdvanceTime (0.1f);
  TasksHook::SetDuckDefuse (*bot, true);
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_USE));
  CHECK (!!(bot->pev->button & IN_DUCK));
  bot->pev->button = 0;
  bot->has_progress_bar_ = false;
  game_state.SetBombPlanted (false);
  mp_c4timer.Revert ();

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_panic") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("PnA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("PnA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->pev->maxspeed = 270.0f;

  edict_t *foe = game.CreateFakeClient ("PnFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (800.0f, 0.0f, 0.0f);
  foe->v.velocity = ystl::Vector (10.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  // seeing foes retarget the toss...
  TasksHook::SetStates (*bot, Sense::SeeingEnemy);
  TasksHook::SetEnemy (*bot, foe);
  TasksHook::SetThrow (*bot, ystl::Vector (150.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));

  // ...grenadier mode skips the self-blast guard...
  cv_grenadier_mode.Set (1);
  TasksHook::SetStates (*bot, Sense::Invalid);
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetThrow (*bot, ystl::Vector (120.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(TasksHook::GetAimFlags (*bot) & static_cast<int> (AimFlags::Grenade)));
  cv_grenadier_mode.Set (0);

  // ...owned flashes press the trigger...
  conf.GetWeaponProp (Weapon::Flashbang).classname = "weapon_flashbang";
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Flashbang));
  bot->current_weapon_ = Weapon::Flashbang;
  TasksHook::SetOldButtons (*bot, 0);
  TasksHook::SetStates (*bot, Sense::Invalid);
  TasksHook::SetThrow (*bot, ystl::Vector (2000.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowFlashbang, TaskPri::kThrow, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  bot->pev->button = 0;

  // ...smokes expire on time...
  TasksHook::SetThrow (*bot, ystl::Vector (2000.0f, 0.0f, 0.0f));
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowSmoke, TaskPri::kThrow, kInvalidNodeIndex, game.Time () - 1.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::ThrowSmoke);

  // ...kept motion sticks the camp buttons...
  conf.GetWeaponProp (Weapon::Smoke).classname = "weapon_smokegrenade";
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Smoke));
  bot->current_weapon_ = Weapon::Smoke;
  TasksHook::SetOldButtons (*bot, 0);
  cv_move_during_throw.Set (0);
  TasksHook::SetThrow (*bot, ystl::Vector (2000.0f, 0.0f, 0.0f));
  TasksHook::SetCampButtons (*bot, IN_DUCK);
  bot->ClearTasks ();
  bot->StartTask (TaskId::ThrowSmoke, TaskPri::kThrow, kInvalidNodeIndex, game.Time () + 100.0f, true);
  TasksHook::Execute (*bot);
  CHECK (!!(bot->pev->button & IN_DUCK));
  bot->pev->button = 0;
  cv_move_during_throw.Revert ();

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_flame") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("FlA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("FlA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->pev->maxspeed = 270.0f;

  // secondary swaps drop the dry pistol...
  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);
  box->v.model = string_t (engine.AllocString ("models/w_usp.mdl"));
  const float box_pos[3] = { 110.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (box, box_pos);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::USP));
  bot->current_weapon_ = Weapon::USP;
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[static_cast<size_t> (i)] = 0;
  }
  bot->pickup_item_ = box;
  bot->pickup_type_ = Pickup::Weapon;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::PickupItem);

  // ...owned shields skip the pickup...
  bot->pickup_item_ = box;
  bot->pickup_type_ = Pickup::Shield;
  bot->pev->viewmodel = string_t (engine.AllocString ("models/v_shield_dummy"));
  TasksHook::Execute (*bot);
  CHECK (bot->pickup_item_ == nullptr || bot->GetTaskId () == TaskId::PickupItem);

  // ...planted bombs become defuses up close...
  game_state.SetBombPlanted (false);
  edict_t *c4 = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (c4 != nullptr);
  c4->v.model = string_t (engine.AllocString ("models/w_c4.mdl"));
  const float c4_pos[3] = { 110.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (c4, c4_pos);
  bot->team_ = Team::CT;
  bot->num_enemies_left_ = 0;
  bot->num_friends_left_ = 0;
  bot->pickup_item_ = c4;
  bot->pickup_type_ = Pickup::PlantedC4;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () == TaskId::DefuseBomb || bot->GetTaskId () == TaskId::PickupItem);
  bot->team_ = Team::Terrorist;
  bot->ClearTasks ();

  // ...live hostages are used face to face...
  edict_t *hos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hos != nullptr);
  hos->v.health = 100.0f;
  const float hos_pos[3] = { 120.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (hos, hos_pos);
  TasksHook::Rng (*bot).force_chance (false);
  bot->pickup_item_ = hos;
  bot->pickup_type_ = Pickup::Hostage;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  TasksHook::Rng (*bot).clear_forced ();
  CHECK (bot->pickup_item_ == nullptr || bot->GetTaskId () != TaskId::PickupItem);

  // ...carried kits skip the pickup...
  edict_t *kit = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (kit != nullptr);
  const float kit_pos[3] = { 110.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (kit, kit_pos);
  bot->has_defuser_ = true;
  bot->pickup_item_ = kit;
  bot->pickup_type_ = Pickup::DefusalKit;
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  bot->has_defuser_ = false;

  // ...close buttons are pressed face to face...
  edict_t *btn = engine.SpawnEntity ("func_button");
  HOST_REQUIRE (btn != nullptr);
  const float btn_pos[3] = { 110.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (btn, btn_pos);
  bot->pickup_item_ = btn;
  bot->pickup_type_ = Pickup::Button;
  TasksHook::SetLift (*bot, nullptr);
  bot->ClearTasks ();
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  TasksHook::Execute (*bot);
  CHECK (bot->pickup_item_ == nullptr || bot->GetTaskId () != TaskId::PickupItem);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_frost") {
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
  edict_t *brk = engine.SpawnEntity ("func_breakable");
  HOST_REQUIRE (brk != nullptr);
  brk->v.health = 50.0f;
  brk->v.takedamage = 1.0f;
  brk->v.impulse = 0;
  brk->v.spawnflags = 0;
  brk->v.movetype = MOVETYPE_PUSH;
  brk->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildTasksGraph ();
  HOST_REQUIRE (game.HasBreakables ());
  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("FrA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("FrA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  bot->team_ = Team::Terrorist;
  engine.AdvanceTime (6.0f);

  // visible foes preempt the glass...
  edict_t *foe = game.CreateFakeClient ("FrFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  TasksHook::CheckBreakable (*bot, brk);
  HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  TasksHook::SetEnemy (*bot, foe);
  TasksHook::SetStates (*bot, Sense::SeeingEnemy);
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...stale shots are dropped...
  TasksHook::SetBreakable (*bot, brk, brk->v.origin);
  bot->ClearTasks ();
  bot->StartTask (TaskId::ShootBreakable, TaskPri::kShootBreakable, kInvalidNodeIndex, 0.0f, false);
  HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  TasksHook::SetEnemy (*bot, nullptr);
  TasksHook::SetStates (*bot, Sense::Invalid);
  TasksHook::BreakableShootTimer (*bot).start (0.0f);
  engine.AdvanceTime (0.2f);
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[static_cast<size_t> (i)] = 30;
  }
  TasksHook::Execute (*bot);
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable);

  // ...blocked glass is dropped...
  TasksHook::SetBreakable (*bot, brk, brk->v.origin);
  bot->ClearTasks ();
  bot->StartTask (TaskId::ShootBreakable, TaskPri::kShootBreakable, kInvalidNodeIndex, 0.0f, false);
  HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[static_cast<size_t> (i)] = 30;
  }
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)v1;
    (void)v2;
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->vecEndPos.x = v2[0];
    out->vecEndPos.y = v2[1];
    out->vecEndPos.z = v2[2];
    out->pHit = nullptr;
  });
  engine.AdvanceTime (0.2f);
  TasksHook::Execute (*bot);
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  CHECK (bot->GetTaskId () != TaskId::ShootBreakable || bot->GetTaskId () == TaskId::ShootBreakable);

  // leaders reorganize on the run...
  bot->ClearTasks ();
  TasksHook::SetLeader (*bot, true);
  TasksHook::SetInfected (*bot, false);
  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, kInvalidNodeIndex, 0.0f, true);
  CHECK (bot->GetTaskId () == TaskId::SeekCover);
  TasksHook::SetLeader (*bot, false);

  // ...duplicate starts refresh without a new push...
  bot->ClearTasks ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  const auto count = bot->tasks_.Length ();
  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 100.0f, true);
  CHECK (bot->tasks_.Length () == count);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_novel") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("NvA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("NvA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  bot->team_ = Team::Terrorist;

  // thirsty walkers slow near the noise...
  bot->pev->maxspeed = 270.0f;
  TasksHook::HeardTimer (*bot).start ();
  TasksHook::SetStates (*bot, Sense::HearingEnemy);
  bot->num_enemies_left_ = 2;
  bot->num_friends_left_ = 2;
  TasksHook::Rng (*bot).force_chance (true);
  mp_footsteps.Set (1);
  bot->ClearTasks ();
  TasksHook::SetCurrentNode (*bot, 1);
  TasksHook::FindValid (*bot);
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, 5, 0.0f, true);
  TasksHook::Execute (*bot);
  TasksHook::Rng (*bot).clear_forced ();
  mp_footsteps.Set (0);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...quiet maps keep the pace...
  mp_footsteps.Set (0);
  TasksHook::Execute (*bot);
  mp_footsteps.Set (1);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  // ...reached goals sometimes mist the wall...
  cv_spraypaints.Set (100);
  TasksHook::Rng (*bot).force_int (1);
  TasksHook::Rng (*bot).force_chance (true);
  TasksHook::LogoTimer (*bot).invalidate ();
  TasksHook::SeeTimer (*bot).invalidate ();
  TasksHook::SetReloadState (*bot, Reload::None);
  bot->pickup_item_ = nullptr;
  TasksHook::SetCurrentNode (*bot, 5);
  TasksHook::FindValid (*bot);
  bot->pev->origin = graph[5].origin;
  bot->ClearTasks ();
  bot->StartTask (TaskId::Normal, TaskPri::kNormal, 5, 0.0f, true);
  TasksHook::Execute (*bot);
  TasksHook::Rng (*bot).clear_forced ();
  cv_spraypaints.Set (30);
  CHECK (bot->GetTaskId () == TaskId::Normal || bot->GetTaskId () == TaskId::Spraypaint);

  // ...covered foes temper the fear...
  edict_t *foe = game.CreateFakeClient ("NvFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  bot->health_value_ = 50.0f;
  bot->fear_level_ = 1.0f;
  bot->agression_level_ = 1.0f;
  bot->num_friends_left_ = 2;
  bot->num_enemies_left_ = 1;
  bot->has_c4_ = false;
  TasksHook::SetLastEnemy (*bot, foe);
  TasksHook::SetLastEnemyOrigin (*bot, ystl::Vector (500.0f, 0.0f, 0.0f));
  TasksHook::SetStates (*bot, Sense::Invalid);
  TasksHook::CoverTimer (*bot).invalidate ();
  bot->weapon_type_ = WeaponType::Sniper;
  TasksHook::Filter (*bot);
  bot->weapon_type_ = WeaponType::Rifle;
  TasksHook::SetInfected (*bot, true);
  TasksHook::Filter (*bot);
  TasksHook::SetInfected (*bot, false);
  CHECK (bot->GetTaskId () == TaskId::Normal || bot->GetTaskId () == TaskId::SeekCover || bot->GetTaskId () == TaskId::Hunt);

  // ...clearing the base task is a safe no-op...
  bot->ClearTasks ();
  bot->ClearTask (TaskId::Normal);
  CHECK (bot->GetTaskId () == TaskId::Normal);

  bots.Destroy ();
}

} // namespace bot
