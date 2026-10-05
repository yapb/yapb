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
    bot.current_node_index_ = index;
  }
  static ystl::Vector DestOrigin (Bot &bot) {
    return bot.dest_origin_;
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

  bots.Addbot ("TskB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  bots.Addbot ("TskP", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  bots.Addbot ("TskT", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  bots.Addbot ("TskA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("TskN", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  for (auto bot : { armed, dry }) {
    bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
    bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
    bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
    TasksHook::CheckBreakable (*bot, brk);
    HOST_REQUIRE (bot->GetTaskId () == TaskId::ShootBreakable);
  }

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

  // ...dry ones stand the task down instead
  TasksHook::Execute (*dry);
  CHECK (dry->GetTaskId () != TaskId::ShootBreakable);

  bots.Destroy ();
}

TEST_CASE ("unit/tasks_drills") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootTasks (engine, cs);

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

  bots.Addbot ("DefA", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  bots.Addbot ("DefB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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

  bots.Addbot ("TskD", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
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

} // namespace bot
