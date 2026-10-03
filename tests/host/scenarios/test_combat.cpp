//
// YaPB test host: unit/combat_{senses,fire,watch}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for combat.cpp through a test-only hook (friend, no
// production behavior changes): friend/foe census, enemy state predicates,
// darkness rules, group/knife/grenade modes, grenade ballistics, body
// visibility and threat/react chains. Randomness is pinned via exact
// modes and extreme inputs, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct CombatHook {
  static void SetStates (Bot &bot, Sense states) {
    bot.states_ = states;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static int FriendsNear (Bot &bot, const ystl::Vector &origin, float radius) {
    return bot.NumFriendsNear (origin, radius);
  }
  static int EnemiesNear (Bot &bot, const ystl::Vector &origin, float radius) {
    return bot.NumEnemiesNear (origin, radius);
  }
  static bool Hidden (Bot &bot, edict_t *ent) {
    return bot.IsEnemyHidden (ent);
  }
  static bool Invincible (Bot &bot, edict_t *ent) {
    return bot.IsEnemyInvincible (ent);
  }
  static bool NoTarget (Bot &bot, edict_t *ent) {
    return bot.IsEnemyNoTarget (ent);
  }
  static bool DarkArea (Bot &bot, edict_t *ent) {
    return bot.IsEnemyInDarkArea (ent);
  }
  static bool Group (Bot &bot, const ystl::Vector &location, float radius) {
    return bot.IsGroupOfEnemies (location, radius);
  }
  static bool Knife (Bot &bot) {
    return bot.IsKnifeMode ();
  }
  static bool GrenadeWar (Bot &bot) {
    return bot.IsGrenadeWar ();
  }
  static bool ThruWall (Bot &bot, int pct) {
    return bot.GetThruWallChance (pct);
  }
  static ystl::Vector Toss (Bot &bot, const ystl::Vector &start, const ystl::Vector &stop) {
    return bot.CalcToss (start, stop);
  }
  static ystl::Vector ThrowFire (Bot &bot, const ystl::Vector &start, const ystl::Vector &stop) {
    return bot.CalcThrow (start, stop);
  }
  static float ScaleFactor (Bot &bot, edict_t *ent) {
    return bot.CalculateScaleFactor (ent);
  }
  static bool BodyParts (Bot &bot, edict_t *target) {
    return bot.CheckBodyParts (target);
  }
  static int EnemyParts (Bot &bot) {
    return static_cast<int> (bot.enemy_parts_);
  }
  static bool EnemyOriginSet (Bot &bot) {
    return !bot.enemy_origin_.empty ();
  }
  static bool InSight (Bot &bot, ystl::Vector &end_pos) {
    return bot.IsEnemyInSight (end_pos);
  }
  static bool Noticeable (Bot &bot, float range) {
    return bot.IsEnemyNoticeable (range);
  }
  static bool Threat (Bot &bot) {
    return bot.IsEnemyThreat ();
  }
  static bool React (Bot &bot) {
    return bot.ReactOnEnemy ();
  }
  static bool Shootable (Bot &bot) {
    return bot.LastEnemyShootable ();
  }
  static bool FriendFire (Bot &bot, float distance) {
    return bot.IsFriendInLineOfFire (distance);
  }
  static bool NavStarted (Bot &bot) {
    return bot.nav_timer_.started ();
  }
  static void BodyAngles (Bot &bot) {
    bot.UpdateBodyAngles ();
  }
  static bool Lookup (Bot &bot) {
    return bot.LookupEnemies ();
  }
  static void Track (Bot &bot) {
    bot.TrackEnemies ();
  }
  static void TeamCommands (Bot &bot) {
    bot.UpdateTeamCommands ();
  }
  static bool TeamOrderStarted (Bot &bot) {
    return bot.team_order_timer_.started ();
  }
  static edict_t *GrenadeVelocity (Bot &bot, ystl::StringRef model) {
    return bot.SetCorrectGrenadeVelocity (model);
  }
  static ystl::Vector CustomHeight (Bot &bot, float distance) {
    return bot.GetCustomHeight (distance);
  }
  static void Focus (Bot &bot) {
    bot.FocusEnemy ();
  }
  static void AttackMove (Bot &bot) {
    bot.AttackMovement ();
  }
  static Fight FightStyle (Bot &bot) {
    return bot.fight_style_;
  }
  static ystl::Vector DestOrigin (Bot &bot) {
    return bot.dest_origin_;
  }
  static float MoveSpeed (Bot &bot) {
    return bot.move_speed_;
  }
  static void SetReloading (Bot &bot, bool value) {
    bot.reload_data_.is_reloading = value;
  }
  static bool Reloading (Bot &bot) {
    return bot.reload_data_.is_reloading;
  }
  static bool InfectedEnemyTeam (Bot &bot) {
    return bot.infected_enemy_team_;
  }
  static bool WantsToFire (Bot &bot) {
    return bot.wants_to_fire_;
  }
  static Sense States (Bot &bot) {
    return bot.states_;
  }
  static void SetBuying (Bot &bot, bool value) {
    bot.buying_finished_ = value;
  }
  static void SetViewDistance (Bot &bot, float value) {
    bot.view_distance_ = value;
    bot.max_view_distance_ = value;
  }
  static size_t QueueLength (Bot &bot) {
    return bot.msg_queue_.size ();
  }
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }
};

namespace {

// eight-node chain for node-backed paths, invalid light skips darkness
void BuildCombatGraph () {
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

void BootCombat (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildCombatGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();
}

edict_t *MakeFoe (ystl::StringRef name, const ystl::Vector &pos) {
  edict_t *ent = game.CreateFakeClient (name);

  if (game.IsNullEntity (ent)) {
    return nullptr;
  }
  ent->v.flags &= ~FL_FAKECLIENT;
  ent->v.health = 100.0f;
  ent->v.origin = pos;
  ent->v.solid = SOLID_BBOX; // fresh clients start zeroed, bring up liveness
  ent->v.takedamage = DAMAGE_YES;
  return ent;
}

} // namespace

TEST_CASE ("unit/combat_senses") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("CbtB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CbtB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *pal_near = MakeFoe ("PalNear", ystl::Vector (200.0f, 0.0f, 0.0f));
  edict_t *pal_far = MakeFoe ("PalFar", ystl::Vector (5000.0f, 0.0f, 0.0f));
  edict_t *pal_dead = MakeFoe ("PalDead", ystl::Vector (150.0f, 0.0f, 0.0f));
  edict_t *foe_near = MakeFoe ("FoeNear", ystl::Vector (250.0f, 0.0f, 0.0f));
  HOST_REQUIRE (pal_near != nullptr && pal_far != nullptr && pal_dead != nullptr && foe_near != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  pal_dead->v.health = 0.0f;
  pal_dead->v.deadflag = DEAD_DEAD;
  clients.Update ();

  clients[pal_near].team = Team::Terrorist;
  clients[pal_far].team = Team::Terrorist;
  clients[pal_dead].team = Team::Terrorist;
  clients[foe_near].team = Team::CT;
  bot->team_ = Team::Terrorist;

  // census skips the self, the far, the dead and the other side
  CHECK (CombatHook::FriendsNear (*bot, bot->pev->origin, 200.0f) == 1);
  CHECK (CombatHook::FriendsNear (*bot, bot->pev->origin, 10000.0f) == 2);
  CHECK (CombatHook::EnemiesNear (*bot, bot->pev->origin, 200.0f) == 1);
  CHECK (CombatHook::EnemiesNear (*bot, bot->pev->origin, 10.0f) == 0);

  // rendering predicates follow the flag matrix exactly
  cv_check_enemy_rendering.Set (1);
  CHECK (!CombatHook::Hidden (*bot, nullptr));
  CHECK (!CombatHook::Hidden (*bot, foe_near));

  foe_near->v.effects |= EF_NODRAW;
  CHECK (CombatHook::Hidden (*bot, foe_near));

  foe_near->v.button |= IN_ATTACK;
  foe_near->v.weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (!CombatHook::Hidden (*bot, foe_near));
  foe_near->v.button = 0;
  foe_near->v.weapons = 0;
  foe_near->v.effects &= ~EF_NODRAW;

  foe_near->v.renderfx = kRenderFxGlowShell;
  foe_near->v.rendermode = 1;
  foe_near->v.renderamt = 10.0f;
  foe_near->v.rendercolor = ystl::Vector (10.0f, 10.0f, 10.0f);
  CHECK (CombatHook::Hidden (*bot, foe_near));

  foe_near->v.button |= IN_ATTACK;
  foe_near->v.weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (!CombatHook::Hidden (*bot, foe_near));
  foe_near->v.button = 0;
  foe_near->v.weapons = 0;

  foe_near->v.renderfx = kRenderFxHologram + 1;
  foe_near->v.renderamt = 10.0f;
  CHECK (CombatHook::Hidden (*bot, foe_near));
  foe_near->v.renderamt = 30.0f;
  CHECK (CombatHook::Hidden (*bot, foe_near));

  foe_near->v.button |= IN_ATTACK;
  CHECK (!CombatHook::Hidden (*bot, foe_near));
  foe_near->v.button = 0;
  foe_near->v.renderfx = 0;
  foe_near->v.rendermode = 0;
  cv_check_enemy_rendering.Set (0);

  // invincibility is solid, godmode or no-damage
  cv_check_enemy_invincibility.Set (1);
  CHECK (!CombatHook::Invincible (*bot, nullptr));
  CHECK (!CombatHook::Invincible (*bot, foe_near));

  foe_near->v.solid = SOLID_NOT;
  CHECK (CombatHook::Invincible (*bot, foe_near));
  foe_near->v.solid = SOLID_BBOX;

  foe_near->v.flags |= FL_GODMODE;
  CHECK (CombatHook::Invincible (*bot, foe_near));
  foe_near->v.flags &= ~FL_GODMODE;

  foe_near->v.takedamage = DAMAGE_NO;
  CHECK (CombatHook::Invincible (*bot, foe_near));
  foe_near->v.takedamage = DAMAGE_YES;
  cv_check_enemy_invincibility.Set (0);

  // no-target is a single bit
  CHECK (!CombatHook::NoTarget (*bot, nullptr));
  CHECK (!CombatHook::NoTarget (*bot, foe_near));
  foe_near->v.flags |= FL_NOTARGET;
  CHECK (CombatHook::NoTarget (*bot, foe_near));
  foe_near->v.flags &= ~FL_NOTARGET;

  // darkness needs a dark node, then threat capability decides
  cv_check_darkness.Set (1);
  CHECK (!CombatHook::DarkArea (*bot, nullptr));
  CHECK (!CombatHook::DarkArea (*bot, foe_near)); // invalid light skips

  graph.paths_[2].light = 0.0f;
  foe_near->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  CHECK (CombatHook::DarkArea (*bot, foe_near));

  foe_near->v.weapons = ystl::to_underlying (kPrimaryWeaponMask);
  foe_near->v.button |= IN_ATTACK;
  CHECK (!CombatHook::DarkArea (*bot, foe_near));
  foe_near->v.button = 0;
  foe_near->v.weapons = 0;

  bot->uses_nvg_ = true;
  CHECK (!CombatHook::DarkArea (*bot, foe_near));
  bot->uses_nvg_ = false;

  // darkness blocks acquiring a new target but never drops a tracked one
  bot->enemy_ = nullptr;
  CHECK (!CombatHook::BodyParts (*bot, foe_near));
  bot->enemy_ = foe_near;
  CHECK (CombatHook::BodyParts (*bot, foe_near));
  bot->enemy_ = nullptr;

  foe_near->v.origin = ystl::Vector (250.0f, 0.0f, 0.0f);

  // groups need two seen enemies, loners never qualify
  CHECK (!CombatHook::Group (*bot, foe_near->v.origin, 500.0f));

  edict_t *foe_near2 = MakeFoe ("FoeNear2", ystl::Vector (260.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe_near2 != nullptr);
  foe_near2->v.health = 100.0f;
  clients.Update ();
  clients[foe_near2].team = Team::CT;
  CHECK (CombatHook::Group (*bot, foe_near->v.origin, 500.0f));
  CHECK (!CombatHook::Group (*bot, foe_near->v.origin, 5.0f));

  // knife mode is jason, bare melee, creature or dry seeing...
  cv_jasonmode.Set (1);
  CHECK (CombatHook::Knife (*bot));
  cv_jasonmode.Set (0);

  bot->weapon_type_ = WeaponType::Melee;
  bot->pev->weapons = 0;
  CHECK (CombatHook::Knife (*bot));

  CombatHook::SetCreature (*bot, true);
  CHECK (CombatHook::Knife (*bot));
  CombatHook::SetCreature (*bot, false);

  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CHECK (CombatHook::Knife (*bot));
  CombatHook::SetStates (*bot, Sense::Invalid);

  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (!CombatHook::Knife (*bot));

  // ...grenade war is bare grenades, orders or map flags
  cv_grenadier_mode.Set (1);
  CHECK (CombatHook::GrenadeWar (*bot));
  cv_grenadier_mode.Set (0);

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Explosive));
  CHECK (CombatHook::GrenadeWar (*bot));
  bot->pev->weapons = 0;
  CHECK (!CombatHook::GrenadeWar (*bot));

  // zeroed thru-wall chance never rolls
  CHECK (!CombatHook::ThruWall (*bot, 0));

  bots.Destroy ();
}

TEST_CASE ("unit/combat_fire") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("CbtF", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CbtF");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("FireFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->maxspeed = 270.0f;
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;

  // flat throws solve ballistics exactly (gravity 800 in the harness)
  const ystl::Vector toss = CombatHook::ThrowFire (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f));
  CHECK (toss.x == Approx (195.0 * 0.7793).margin (0.05));
  CHECK (toss.y == Approx (0.0).margin (0.001));
  CHECK (toss.z == Approx (440.0 * 0.7793).margin (0.05));

  // zero distance never flies, marathon throws clamp the arc
  CHECK (CombatHook::ThrowFire (*bot, ystl::Vector (100.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)).empty ());
  const ystl::Vector far = CombatHook::ThrowFire (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (3900.0f, 0.0f, 0.0f));
  CHECK (far.x == Approx (3900.0 / 1.2 * 0.7793).margin (0.5));
  CHECK (far.z == Approx (440.0 * 0.6 * 0.7793).margin (0.05));

  // tossed arcs reject absurd height gaps up front
  CHECK (CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 600.0f)).empty ());
  CHECK (CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 600.0f), ystl::Vector (100.0f, 0.0f, 0.0f)).empty ());

  // open sky arcs over the chord instead of dying on the midpoint check
  // (regression: the raw midpoint sits on the chord and failed validation)
  const ystl::Vector lob = CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f));
  CHECK (!lob.empty ());
  CHECK (lob.x == Approx (151.5).margin (1.0));
  CHECK (lob.z == Approx (336.0).margin (1.0));
  CHECK (!CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 100.0f)).empty ());
  CHECK (!CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 100.0f), ystl::Vector (390.0f, 0.0f, 0.0f)).empty ());

  // no gravity, no ballistics on either arc
  sv_gravity.Set (0);
  CHECK (CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f)).empty ());
  CHECK (CombatHook::ThrowFire (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f)).empty ());
  sv_gravity.Set (800);

  // scale follows the surface ratio exactly
  bot->pev->maxs = ystl::Vector (16.0f, 16.0f, 72.0f);
  bot->pev->mins = ystl::Vector (-16.0f, -16.0f, -36.0f);
  foe->v.maxs = ystl::Vector (16.0f, 16.0f, 72.0f);
  foe->v.mins = ystl::Vector (-16.0f, -16.0f, -36.0f);
  CHECK (CombatHook::ScaleFactor (*bot, foe) == 1.0f);

  foe->v.maxs = ystl::Vector (8.0f, 8.0f, 36.0f);
  foe->v.mins = ystl::Vector (-8.0f, -8.0f, -18.0f);
  CHECK (CombatHook::ScaleFactor (*bot, foe) == 0.25f);

  // clear world exposes every body part at once
  CHECK (CombatHook::BodyParts (*bot, foe));
  const int parts = CombatHook::EnemyParts (*bot);
  CHECK (!!(parts & static_cast<int> (Visibility::Body)));
  CHECK (!!(parts & static_cast<int> (Visibility::Head)));
  CHECK (CombatHook::EnemyOriginSet (*bot));

  // hidden enemies expose nothing
  foe->v.effects |= EF_NODRAW;
  cv_check_enemy_rendering.Set (1);
  CHECK (!CombatHook::BodyParts (*bot, foe));
  CHECK (CombatHook::EnemyParts (*bot) == static_cast<int> (Visibility::None));
  foe->v.effects &= ~EF_NODRAW;
  cv_check_enemy_rendering.Set (0);

  // model traces never connect in an empty world (no callers in prod)
  ystl::Vector end_pos {};
  bot->enemy_ = foe;
  CHECK (!CombatHook::InSight (*bot, end_pos));

  // remembered shots need the flag, the spot and the foe
  CHECK (!CombatHook::Shootable (*bot));
  bot->last_enemy_ = foe;
  bot->last_enemy_origin_ = foe->v.origin;
  CHECK (!CombatHook::Shootable (*bot));

  // friendlies are safe with the cvar off...
  CHECK (!CombatHook::FriendFire (*bot, 1000.0f));

  // ...and catchable down the exact firing ray once enabled
  mp_friendlyfire.Set (1);
  edict_t *pal = MakeFoe ("FirePal", ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (pal != nullptr);
  pal->v.health = 100.0f;
  clients.Update ();
  clients[pal].team = Team::Terrorist;

  const ystl::Vector eye = bot->pev->origin + bot->pev->view_ofs;
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  pal->v.origin = eye + ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (CombatHook::FriendFire (*bot, 1000.0f));
  mp_friendlyfire.Set (0);

  // runners are always noticeable, threats need proximity or facing
  foe->v.velocity = ystl::Vector (300.0f, 0.0f, 0.0f);
  CHECK (CombatHook::Noticeable (*bot, 1500.0f));

  bot->enemy_ = nullptr;
  CHECK (!CombatHook::Threat (*bot));

  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  bot->enemy_ = foe;
  CHECK (!CombatHook::Threat (*bot));
  CombatHook::SetStates (*bot, Sense::Invalid);

  bot->StartTask (TaskId::SeekCover, TaskPri::seek_cover, kInvalidNodeIndex, 0.0f, true);
  CHECK (!CombatHook::Threat (*bot));

  bot->StartTask (TaskId::Camp, TaskPri::camp, kInvalidNodeIndex, 0.0f, true);
  CHECK (!CombatHook::Threat (*bot));

  bot->ClearTasks ();
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f); // 100 away, inside 256
  CHECK (CombatHook::Threat (*bot));

  // reactions: nothing without an enemy...
  bot->enemy_ = nullptr;
  CHECK (!CombatHook::React (*bot));

  // ...creatures lunge up close...
  CombatHook::SetCreature (*bot, true);
  bot->enemy_ = foe;
  CHECK (CombatHook::React (*bot));
  CHECK (CombatHook::NavStarted (*bot));
  CombatHook::SetCreature (*bot, false);

  // ...and noticed runners trigger at range too (foe still sprinting)
  CHECK (CombatHook::React (*bot));

  bots.Destroy ();
}

TEST_CASE ("unit/combat_watch") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("CbtW", Difficulty::Expert, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CbtW");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("WatchFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  edict_t *pal = MakeFoe ("WatchPal", ystl::Vector (200.0f, 50.0f, 0.0f));
  HOST_REQUIRE (pal != nullptr);

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  clients.Update ();

  clients[foe].team = Team::CT;
  clients[foe].team2 = Team::CT;
  clients[pal].team = Team::Terrorist;
  clients[pal].team2 = Team::Terrorist;
  bot->team_ = Team::Terrorist;

  // facing calculations need a fresh frustum first
  CombatHook::BodyAngles (*bot);

  // a preset, live, faced enemy sticks through the lookup...
  bot->enemy_ = foe;
  CHECK (CombatHook::Lookup (*bot));
  CHECK (bot->enemy_ == foe);

  // ...and tracking raises the seeing flag for it
  CombatHook::Track (*bot);
  CHECK (CombatHook::States (*bot) == Sense::SeeingEnemy);

  // dead world tracks nothing and clears the flag
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  CombatHook::Track (*bot);
  CHECK (CombatHook::States (*bot) == Sense::Invalid);
  CHECK (bot->enemy_ == nullptr);
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  clients.Update ();

  // seen teammates trigger the rusher storm call...
  bot->personality_ = Personality::Rusher;
  bot->num_friends_left_ = 1;
  cv_radio_mode.Set (2);
  const size_t radio_base = CombatHook::QueueLength (*bot);
  CombatHook::TeamCommands (*bot);
  CHECK (CombatHook::QueueLength (*bot) == radio_base + 1);
  CHECK (CombatHook::TeamOrderStarted (*bot));

  // ...muted radios stay out of it
  cv_radio_mode.Set (0);
  CombatHook::TeamCommands (*bot);
  CHECK (CombatHook::QueueLength (*bot) == radio_base + 1);
  cv_radio_mode.Set (2);

  // no grenades in the world resolves to nothing
  CHECK (CombatHook::GrenadeVelocity (*bot, "models/w_hegrenade.mdl") == nullptr);

  // custom heights follow the weapon table by distance band
  bot->enemy_ = foe;
  bot->weapon_type_ = WeaponType::Rifle;
  CHECK (CombatHook::CustomHeight (*bot, 400.0f) == ystl::Vector (0.0f, 0.0f, -7.5f));
  CHECK (CombatHook::CustomHeight (*bot, 100.0f) == ystl::Vector (0.0f, 0.0f, -9.5f));

  bot->weapon_type_ = WeaponType::Melee;
  CHECK (CombatHook::CustomHeight (*bot, 400.0f) == ystl::Vector (0.0f, 0.0f, 0.0f));

  foe->v.flags |= FL_DUCKING;
  bot->weapon_type_ = WeaponType::Rifle;
  CHECK (CombatHook::CustomHeight (*bot, 400.0f) == ystl::Vector (0.0f, 0.0f, 0.0f));
  foe->v.flags &= ~FL_DUCKING;

  // focused close enemies draw fire when faced (aim point first)
  HOST_REQUIRE (CombatHook::BodyParts (*bot, foe));
  CombatHook::Focus (*bot);
  CHECK (CombatHook::WantsToFire (*bot));

  bots.Destroy ();
}

// full engagement: bot spots an enemy, keeps it, and opens fire on a clear lane
TEST_CASE ("scenario/combat_engagement") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("EngB", Difficulty::Expert, Personality::Rusher, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("EngB");
  HOST_REQUIRE (bot != nullptr);

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f); // looking down +x
  bot->pev->fov = 90.0f;
  bot->pev->maxspeed = 270.0f;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  bot->pev->takedamage = DAMAGE_YES;
  bot->pev->solid = SOLID_BBOX;
  bot->pev->movetype = MOVETYPE_WALK;
  bot->team_ = Team::Terrorist;
  bot->is_alive_ = true;
  CombatHook::SetBuying (*bot, true);
  bot->not_started_ = false; // pretend the initial join already happened
  CombatHook::SetViewDistance (*bot, 4096.0f);

  // the fake engine has no real leaf/PVS data: decide visibility by trace
  cv_use_engine_pvs_check.Set (0);

  // hand the bot a loaded ak47
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;

  edict_t *foe = MakeFoe ("EngFoe", ystl::Vector (450.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f); // facing the bot
  clients.Update ();
  clients[foe].team = Team::CT;

  bool spotted = false;
  bool attacked = false;
  bool fired = false;
  bool wanted = false;

  for (int i = 0; i < 60; ++i) {
    // keep the bot facing the enemy so the view cone/frustum include it
    const ystl::Vector aim = (foe->v.origin + ystl::Vector (0.0f, 0.0f, 36.0f) - bot->GetEyesPos ()).angles ();
    bot->pev->v_angle = ystl::Vector (-aim.x, aim.y, 0.0f);

    engine.AdvanceTime (0.1f);
    bots.Frame ();

    spotted = spotted || (bot->enemy_ == foe);
    attacked = attacked || (bot->GetTaskId () == TaskId::Attack);
    fired = fired || (bot->pev->button & IN_ATTACK) != 0;
    wanted = wanted || CombatHook::WantsToFire (*bot);
  }

  CHECK (spotted);
  CHECK (has_flag (CombatHook::States (*bot), Sense::SeeingEnemy));
  CHECK (attacked);
  CHECK (wanted);
  CHECK (fired);

  bots.Destroy ();
}

TEST_CASE ("unit/combat_predicates_extra") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("CbtX", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CbtX");
  HOST_REQUIRE (bot != nullptr);

  edict_t *pal = MakeFoe ("PalX", ystl::Vector (200.0f, 0.0f, 0.0f));
  edict_t *foe = MakeFoe ("FoeX", ystl::Vector (250.0f, 0.0f, 0.0f));
  HOST_REQUIRE (pal != nullptr && foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[pal].team = Team::Terrorist;
  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;

  // free-for-all: no friends, every other player counts as hostile
  game.AddGameFlag (GameFlags::FreeForAll);
  CHECK (CombatHook::FriendsNear (*bot, bot->pev->origin, 10000.0f) == 0);
  CHECK (CombatHook::EnemiesNear (*bot, bot->pev->origin, 10000.0f) == 2);
  game.ClearGameFlag (GameFlags::FreeForAll);
  CHECK (CombatHook::FriendsNear (*bot, bot->pev->origin, 10000.0f) == 1);
  CHECK (CombatHook::EnemiesNear (*bot, bot->pev->origin, 10000.0f) == 1);

  // rendering matrix: explode flags and mid glow without gunfire hide
  cv_check_enemy_rendering.Set (1);
  foe->v.renderfx = kRenderFxExplode;
  foe->v.effects &= ~EF_NODRAW;
  foe->v.button = 0;
  foe->v.weapons = 0;
  CHECK (CombatHook::Hidden (*bot, foe));

  foe->v.renderfx = kRenderFxGlowShell;
  foe->v.rendermode = 1;
  foe->v.renderamt = 30.0f;
  foe->v.rendercolor = ystl::Vector (30.0f, 30.0f, 30.0f);
  foe->v.button = 0;
  foe->v.weapons = 0;
  CHECK (CombatHook::Hidden (*bot, foe));

  foe->v.renderfx = kRenderFxHologram + 1;
  foe->v.renderamt = 30.0f;
  CHECK (CombatHook::Hidden (*bot, foe));
  foe->v.renderfx = 0;
  foe->v.rendermode = 0;
  cv_check_enemy_rendering.Set (0);
  CHECK (!CombatHook::Hidden (*bot, foe));

  // invincibility checks stay off when the cvar is off
  cv_check_enemy_invincibility.Set (0);
  foe->v.solid = SOLID_NOT;
  CHECK (!CombatHook::Invincible (*bot, foe));
  foe->v.solid = SOLID_BBOX;
  cv_check_enemy_invincibility.Set (1);
  CHECK (!CombatHook::Invincible (*bot, foe));

  // darkness: bright nodes and flashlights never hide
  cv_check_darkness.Set (1);
  graph.paths_[2].light = 100.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  CHECK (!CombatHook::DarkArea (*bot, foe));

  graph.paths_[2].light = 0.0f;
  foe->v.effects |= EF_DIMLIGHT;
  CHECK (!CombatHook::DarkArea (*bot, foe));
  foe->v.effects &= ~EF_DIMLIGHT;
  cv_check_darkness.Set (0);

  bots.Destroy ();
}

TEST_CASE ("unit/combat_attack_move") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("CbtM", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("CbtM");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("MoveFoe", ystl::Vector (450.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);

  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  bot->pev->health = 100.0f;
  bot->team_ = Team::Terrorist;
  bot->enemy_ = foe;
  clients.Update ();
  clients[foe].team = Team::CT;
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);

  // knife hands steer the destination straight onto the enemy
  bot->weapon_type_ = WeaponType::Melee;
  bot->pev->weapons = 0;
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::DestOrigin (*bot) == foe->v.origin);

  // rifles at mid range run forward and settle out of ducking cover
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  CombatHook::SetReloading (*bot, false);
  bot->is_vip_ = false;
  bot->health_value_ = 100.0f;
  bot->agression_level_ = 1.0f;
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CHECK (CombatHook::States (*bot) == Sense::SeeingEnemy);
  CombatHook::AttackMove (*bot);
  // the fight style settles deterministically out of ducking cover
  CHECK (CombatHook::FightStyle (*bot) == Fight::Stay);

  // suspect-only contact holds position instead
  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::MoveSpeed (*bot) == 0.0f);

  bots.Destroy ();
}

} // namespace bot
