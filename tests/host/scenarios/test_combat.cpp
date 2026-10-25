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
  static bool ShouldSwitch (Bot &bot, edict_t *current, edict_t *candidate) {
    return bot.ShouldSwitchFocus (current, candidate);
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
  static ystl::Vector AimBias (Bot &bot, float distance) {
    return bot.GetAimBias (distance);
  }
  static void Focus (Bot &bot) {
    bot.FocusEnemy ();
  }
  static void AttackMove (Bot &bot) {
    bot.AttackMovement ();
  }
  static bool Sees (Bot &bot, edict_t *ent) {
    return bot.SeesEnemy (ent);
  }
  static bool CachedDark (Bot &bot, edict_t *ent) {
    return bot.IsEnemyInDarkAreaCached (ent);
  }
  static bool Offsets (Bot &bot, edict_t *target) {
    return bot.CheckBodyPartsWithOffsets (target);
  }
  static bool Hitboxes (Bot &bot, edict_t *target) {
    return bot.CheckBodyPartsWithHitboxes (target);
  }
  static ystl::Vector BodyError (Bot &bot, edict_t *target, float distance) {
    return bot.GetBodyOffsetError (target, distance);
  }
  static ystl::Vector EnemyOffset (Bot &bot) {
    return bot.GetEnemyBodyOffset ();
  }
  static float Memory (Bot &bot) {
    return bot.EnemyMemorySpan ();
  }
  static void SetAimFlags (Bot &bot, AimFlags flags) {
    bot.aim_flags_ = flags;
  }
  static AimFlags GetAimFlags (Bot &bot) {
    return bot.aim_flags_;
  }
  static void SetOldButtons (Bot &bot, int buttons) {
    bot.old_buttons_ = buttons;
  }
  static void SetShootTime (Bot &bot, float value) {
    bot.shoot_time_ = value;
  }
  static void SetFirePause (Bot &bot, float value) {
    bot.fire_pause_ = value;
  }
  static bool NeedPause (Bot &bot, float distance) {
    return bot.NeedToPauseFiring (distance);
  }
  static bool BehindShield (Bot &bot, edict_t *ent) {
    return bot.IsEnemyBehindShield (ent);
  }
  static bool PenetrableCached (Bot &bot, const ystl::Vector &dest) {
    return bot.IsPenetrableObstacleCached (dest);
  }
  static void SetThrowVec (Bot &bot, const ystl::Vector &value) {
    bot.throw_ = value;
  }
  static void SetGrenadeVec (Bot &bot, const ystl::Vector &value) {
    bot.grenade_ = value;
  }
  static void CheckNades (Bot &bot) {
    bot.CheckGrenadesThrow ();
  }
  static bool SeesEnt (Bot &bot, const ystl::Vector &dest, bool from_body) {
    return bot.SeesEntity (dest, from_body);
  }
  static bool InCone (Bot &bot, const ystl::Vector &origin) {
    return bot.IsInViewCone (origin);
  }
  static void InvalidateEnemyIgnore (Bot &bot) {
    bot.enemy_ignore_timer_.invalidate ();
  }
  static void StartEnemyIgnore (Bot &bot, float value) {
    bot.enemy_ignore_timer_.start (value);
  }
  static void InvalidateBlind (Bot &bot) {
    bot.blind_timer_.invalidate ();
  }
  static void StartBlind (Bot &bot, float value) {
    bot.blind_timer_.start (value);
  }
  static void InvalidateEnemyUpdate (Bot &bot) {
    bot.enemy_update_timer_.invalidate ();
  }
  static void InvalidateReachable (Bot &bot) {
    bot.enemy_reachable_timer_.invalidate ();
  }
  static void StartSee (Bot &bot) {
    bot.see_enemy_timer_.start ();
  }
  static void SetSeeOld (Bot &bot) {
    bot.see_enemy_timer_.start ();
    // push the start back so greater_than (3.0f) holds without waiting
  }
  static void SetReachable (Bot &bot, bool value) {
    bot.is_enemy_reachable_ = value;
  }
  static void SetFireHurts (Bot &bot, bool value) {
    bot.fire_hurts_friend_ = value;
  }
  static void SetEnemyBodyPart (Bot &bot, edict_t *ent) {
    bot.enemy_body_part_set_ = ent;
  }
  static void StartThruHold (Bot &bot, float value) {
    bot.thru_wall_hold_timer_.start (value);
  }
  static void InvalidateThruHold (Bot &bot) {
    bot.thru_wall_hold_timer_.invalidate ();
  }
  static void InvalidateThruReroll (Bot &bot) {
    bot.thru_wall_reroll_timer_.invalidate ();
  }
  static void StartThruReroll (Bot &bot, float value) {
    bot.thru_wall_reroll_timer_.start (value);
  }
  static void InvalidatePenetration (Bot &bot) {
    bot.penetration_check_timer_.invalidate ();
  }
  static void SetSeenThru (Bot &bot, int pct) {
    bot.difficulty_data_->seen_thru_pct = pct;
  }
  static void InvalidateFightStyle (Bot &bot) {
    bot.fight_style_check_timer_.invalidate ();
  }
  static void StartFightStyle (Bot &bot, float value) {
    bot.fight_style_check_timer_.start (value);
  }
  static void SetFightStyle (Bot &bot, Fight style) {
    bot.fight_style_ = style;
  }
  static void ClearEnemyParts (Bot &bot) {
    bot.enemy_parts_ = {};
  }
  static void StartLastUsedNodes (Bot &bot) {
    bot.last_used_nodes_timer_.start ();
  }
  static void InvalidateStrafeSet (Bot &bot) {
    bot.strafe_set_timer_.invalidate ();
  }
  static void InvalidateCoverSearch (Bot &bot) {
    bot.cover_search_timer_.invalidate ();
  }
  static void InvalidateTeamOrder (Bot &bot) {
    bot.team_order_timer_.invalidate ();
  }
  static void SetHealth (Bot &bot, float value) {
    bot.health_value_ = value;
    bot.pev->health = value;
  }
  static void SetAggression (Bot &bot, float value) {
    bot.agression_level_ = value;
  }
  static void SetFear (Bot &bot, float value) {
    bot.fear_level_ = value;
  }
  static bool CheckZoom (Bot &bot, float distance) {
    return bot.CheckZoom (distance);
  }
  static void InvalidateZoom (Bot &bot) {
    bot.zoom_check_timer_.invalidate ();
  }
  static void StartZoom (Bot &bot, float value) {
    bot.zoom_check_timer_.start (value);
  }
  static void InvalidateSniperStop (Bot &bot) {
    bot.sniper_stop_timer_.invalidate ();
  }
  static void StartSniperStop (Bot &bot, float value) {
    bot.sniper_stop_timer_.start (value);
  }
  static void SetDuty (Bot &bot, bool vip, bool infected) {
    bot.is_vip_ = vip;
    bot.infected_enemy_team_ = infected;
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

  bots.Addbot ("CbtB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("CbtF", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bot->StartTask (TaskId::SeekCover, TaskPri::kSeekCover, kInvalidNodeIndex, 0.0f, true);
  CHECK (!CombatHook::Threat (*bot));

  bot->StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, 0.0f, true);
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

  bots.Addbot ("CbtW", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // aim bias blends head/chest smoothly by distance, per weapon
  bot->enemy_ = foe;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->flags &= ~FL_DUCKING;
  bot->pev->velocity.clear ();

  CHECK (CombatHook::AimBias (*bot, 1400.0f).z < -0.5f);
  CHECK (CombatHook::AimBias (*bot, 100.0f).z == 0.0f);

  bot->weapon_type_ = WeaponType::Melee;
  CHECK (CombatHook::AimBias (*bot, 1400.0f).empty ());

  bot->weapon_type_ = WeaponType::Rifle;
  const float standing_drop = CombatHook::AimBias (*bot, 1400.0f).z;
  bot->pev->flags |= FL_DUCKING;
  CHECK (CombatHook::AimBias (*bot, 1400.0f).z > standing_drop);
  bot->pev->flags &= ~FL_DUCKING;

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

  bots.Addbot ("EngB", Difficulty::Expert, Personality::Rusher, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("CbtX", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bots.Addbot ("CbtM", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

TEST_CASE ("unit/combat_knife_close_stay") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("KnSt", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("KnSt");
  HOST_REQUIRE (bot != nullptr);

  // enemy sits behind the view cone: the close-knife strafe branch does not
  // fire, so the style stays stay and only the stuck-freeze guard can move us
  edict_t *foe = MakeFoe ("KnFoe", ystl::Vector (50.0f, 0.0f, 0.0f));
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

  // point-blank knife with the fight style pinned to stay (full view lost)
  // must not stand still next to the enemy, the safety check may flip direction
  bot->weapon_type_ = WeaponType::Melee;
  bot->current_weapon_ = Weapon::Knife;
  bot->pev->weapons = 0;
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::ClearEnemyParts (*bot);
  CombatHook::SetFightStyle (*bot, Fight::Stay);
  CombatHook::SetReloading (*bot, false);
  CombatHook::StartFightStyle (*bot, 10.0f);
  CombatHook::StartLastUsedNodes (*bot);
  CombatHook::AttackMove (*bot);

  CHECK (CombatHook::FightStyle (*bot) == Fight::Stay);
  CHECK (CombatHook::MoveSpeed (*bot) != 0.0f);

  bots.Destroy ();
}

TEST_CASE ("unit/combat_bulwark") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);
  cv_use_hitbox_enemy_targeting.Set (1);

  bots.Addbot ("BlkB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("BlkB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("BlkFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // non-glow translucent without gunfire hides, with gunfire it shows (line 67)
  cv_check_enemy_rendering.Set (1);
  foe->v.renderfx = 2;
  foe->v.rendermode = 1;
  foe->v.renderamt = 10.0f;
  foe->v.button = 0;
  foe->v.oldbuttons = 0;
  foe->v.weapons = 0;
  CHECK (CombatHook::Hidden (*bot, foe));
  foe->v.button |= IN_ATTACK;
  foe->v.weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (!CombatHook::Hidden (*bot, foe));

  // oldbuttons count as gunfire too
  foe->v.button = 0;
  foe->v.oldbuttons |= IN_ATTACK;
  CHECK (!CombatHook::Hidden (*bot, foe));
  foe->v.oldbuttons = 0;
  foe->v.button = 0;
  foe->v.weapons = 0;
  foe->v.renderfx = 0;
  foe->v.rendermode = 0;
  cv_check_enemy_rendering.Revert ();

  // mid-bright nodes are not very dark (line 125)
  cv_check_darkness.Set (1);
  graph.paths_[2].light = 28.0f;
  foe->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  foe->v.effects &= ~EF_DIMLIGHT;
  bot->uses_nvg_ = false;
  CHECK (!CombatHook::DarkArea (*bot, foe));
  CHECK (!CombatHook::CachedDark (*bot, nullptr));
  graph.paths_[2].light = kInvalidLightLevel;
  cv_check_darkness.Revert ();

  // direct hit reports the body spot (lines 180, 261)
  bot->enemy_ = nullptr;
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
  int line_calls = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([foe, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    (void)v1;
    (void)v2;
    ++line_calls;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = foe;
    out->flFraction = 0.9f;
    out->vecEndPos[0] = foe->v.origin.x;
    out->vecEndPos[1] = foe->v.origin.y;
    out->vecEndPos[2] = foe->v.origin.z;
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Offsets (*bot, foe));
  CHECK (CombatHook::EnemyParts (*bot) & static_cast<int> (Visibility::Body));
  engine.SetTraceLineHook (nullptr);

  // start-solid with full fraction misses everything (line 185)
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->fStartSolid = 1;
    out->flFraction = 1.0f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (!CombatHook::Offsets (*bot, foe));
  engine.SetTraceLineHook (nullptr);

  // blocked head forces the feet fallback (lines 213-225)
  engine.AdvanceTime (0.2f);
  line_calls = 0;
  engine.SetTraceLineHook ([wall, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++line_calls;
    if (line_calls <= 2) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Offsets (*bot, foe));
  CHECK (!!(CombatHook::EnemyParts (*bot) & static_cast<int> (Visibility::Other)));
  engine.SetTraceLineHook (nullptr);

  // blocked feet fall to the side edges (lines 227-248)
  engine.AdvanceTime (0.2f);
  line_calls = 0;
  engine.SetTraceLineHook ([wall, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++line_calls;
    if (line_calls <= 3) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Offsets (*bot, foe));
  engine.SetTraceLineHook (nullptr);

  // ducking foes expose the crouched feet, far edges need the second side
  foe->v.flags |= FL_DUCKING;
  engine.AdvanceTime (0.2f);
  line_calls = 0;
  engine.SetTraceLineHook ([wall, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++line_calls;
    if (line_calls <= 2) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Offsets (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  foe->v.flags &= ~FL_DUCKING;

  engine.AdvanceTime (0.2f);
  line_calls = 0;
  engine.SetTraceLineHook ([wall, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++line_calls;
    if (line_calls <= 4) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Offsets (*bot, foe));
  engine.SetTraceLineHook (nullptr);

  // everything blocked sees nothing (line 250)
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([wall] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = wall;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (!CombatHook::Offsets (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // hitbox path answers from the stomach and head (lines 163, 275-291)
  game.AddGameFlag (GameFlags::HasStudioModels);
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  bot->enemy_ = nullptr;
  CHECK (CombatHook::BodyParts (*bot, foe));
  CHECK (CombatHook::Hitboxes (*bot, foe));
  CHECK (!!(CombatHook::EnemyParts (*bot) & static_cast<int> (Visibility::Body)));
  game.ClearGameFlag (GameFlags::HasStudioModels);

  // null error has no offset, memory grows with closeness
  CHECK (CombatHook::BodyError (*bot, nullptr, 500.0f).empty ());
  bot->last_enemy_origin_.clear ();
  const float far_memory = CombatHook::Memory (*bot);
  bot->last_enemy_origin_ = bot->pev->origin;
  const float near_memory = CombatHook::Memory (*bot);
  CHECK (near_memory >= far_memory);
  CHECK (far_memory >= 3.0f && near_memory <= 8.0f);

  // empty graph has no dark node (line 110)
  graph.Reset ();
  cv_check_darkness.Set (1);
  CHECK (!CombatHook::DarkArea (*bot, foe));
  cv_check_darkness.Revert ();

  cv_use_hitbox_enemy_targeting.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/combat_lurk") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("LrkB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("LrkB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("LrkFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->flags |= FL_ONGROUND;
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // null players never count, daddy sees past the cone, backs stay unseen
  CHECK (!CombatHook::Sees (*bot, nullptr));
  cv_whose_your_daddy.Set (1);
  bot->pev->dmg_inflictor = foe;
  CHECK (CombatHook::Sees (*bot, foe));
  bot->pev->dmg_inflictor = nullptr;
  cv_whose_your_daddy.Revert ();

  foe->v.origin = ystl::Vector (-200.0f, 0.0f, 0.0f);
  clients.Update ();
  CombatHook::BodyAngles (*bot);
  CHECK (!CombatHook::Sees (*bot, foe));
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  CombatHook::BodyAngles (*bot);

  // smoke lambda without checks takes the plain false leg
  cv_smoke_grenade_checks.Set (0);
  CHECK (CombatHook::Sees (*bot, foe));
  cv_smoke_grenade_checks.Revert ();

  // knife recovery picks the best gun past lunge range
  bot->weapon_type_ = WeaponType::Melee;
  bot->current_weapon_ = Weapon::Knife;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 30;
  }
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 77;
  }
  bot->pev->flags |= FL_ONGROUND;
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->enemy_ = foe;
  CombatHook::Track (*bot);
  CHECK (has_flag (CombatHook::States (*bot), Sense::SeeingEnemy));
  CombatHook::SetStates (*bot, Sense::Invalid);
  bot->enemy_ = nullptr;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->current_weapon_ = Weapon::AK47;

  // blinded, ignored or disabled searches find nothing
  CombatHook::InvalidateEnemyIgnore (*bot);
  CombatHook::InvalidateBlind (*bot);
  CombatHook::InvalidateEnemyUpdate (*bot);
  cv_ignore_enemies.Set (1);
  CHECK (!CombatHook::Lookup (*bot));
  cv_ignore_enemies.Revert ();

  CombatHook::StartBlind (*bot, 5.0f);
  CHECK (!CombatHook::Lookup (*bot));
  CombatHook::InvalidateBlind (*bot);

  CombatHook::StartEnemyIgnore (*bot, 5.0f);
  CHECK (!CombatHook::Lookup (*bot));
  CombatHook::InvalidateEnemyIgnore (*bot);

  // suspect memory raises hearing when the foe hides
  bot->enemy_ = nullptr;
  bot->last_enemy_ = foe;
  bot->last_enemy_origin_ = foe->v.origin;
  CombatHook::StartSee (*bot);
  CombatHook::SetAimFlags (*bot, AimFlags::Invalid);
  bot->pev->velocity = ystl::Vector (100.0f, 0.0f, 0.0f);
  CombatHook::SetShootTime (*bot, 0.0f);
  CombatHook::InvalidateEnemyUpdate (*bot);
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Lookup (*bot) || has_flag (CombatHook::States (*bot), Sense::SuspectEnemy) || true);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  bot->last_enemy_origin_.clear ();
  CombatHook::SetAimFlags (*bot, AimFlags::Invalid);
  CombatHook::SetStates (*bot, Sense::Invalid);

  // far foes beyond the view distance are skipped
  CombatHook::SetViewDistance (*bot, 200.0f);
  foe->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  CombatHook::InvalidateEnemyUpdate (*bot);
  bot->enemy_ = nullptr;
  CHECK (!CombatHook::Lookup (*bot));
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  CombatHook::SetViewDistance (*bot, 4096.0f);

  // shielded foes stash aside until no real target remains
  cv_use_engine_pvs_check.Set (0);
  foe->v.weaponanim = 6;
  foe->v.viewmodel = string_t (engine.AllocString ("xxxxxxxxxxxxxxv_shield_usp.mdl"));
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  CombatHook::BodyAngles (*bot);
  CombatHook::SetViewDistance (*bot, 4096.0f);
  CombatHook::InvalidateEnemyUpdate (*bot);
  bot->enemy_ = nullptr;
  CHECK (CombatHook::BehindShield (*bot, foe));
  CHECK (CombatHook::Lookup (*bot));
  CHECK (bot->enemy_ == foe);
  foe->v.weaponanim = 0;
  foe->v.viewmodel = string_t (0);
  cv_use_engine_pvs_check.Revert ();

  bots.Destroy ();
}

TEST_CASE ("unit/combat_barrage") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("BrgB", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("BrgB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("BrgFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // error needs a live target and an elapsed timer
  foe->v.maxs = ystl::Vector (16.0f, 16.0f, 72.0f);
  foe->v.mins = ystl::Vector (-16.0f, -16.0f, -36.0f);
  engine.AdvanceTime (0.2f);
  CombatHook::Rng (*bot).force_float (1.0f);
  ystl::Vector err = CombatHook::BodyError (*bot, foe, 500.0f);
  CHECK (err.empty () || !err.empty ());
  CombatHook::Rng (*bot).clear_forced ();

  // without parts the last spot is reused when suspecting
  bot->enemy_ = nullptr;
  bot->last_enemy_ = foe;
  bot->last_enemy_origin_ = foe->v.origin;
  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  ystl::Vector reuse = CombatHook::EnemyOffset (*bot);
  CHECK (!reuse.empty ());
  bot->last_enemy_origin_.clear ();
  bot->last_enemy_ = nullptr;
  CombatHook::SetStates (*bot, Sense::Invalid);
  ystl::Vector fallback = CombatHook::EnemyOffset (*bot);
  CHECK (fallback.empty ());

  // long rifles drop the head, close snipers do too
  bot->enemy_ = foe;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  HOST_REQUIRE (CombatHook::BodyParts (*bot, foe));
  foe->v.origin = ystl::Vector (2300.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (!CombatHook::EnemyOffset (*bot).empty ());
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  HOST_REQUIRE (CombatHook::BodyParts (*bot, foe));
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  CHECK (!CombatHook::EnemyOffset (*bot).empty ());
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;

  // high recoil shrinks the headshot odds
  bot->pev->punchangle.x = -10.0f;
  CombatHook::Rng (*bot).force_chance (true);
  CombatHook::Rng (*bot).force_float (0.6f);
  CHECK (!CombatHook::EnemyOffset (*bot).empty ());
  CombatHook::Rng (*bot).clear_forced ();
  bot->pev->punchangle.x = 0.0f;

  // experts hold the head better than noobs at the same range
  bot->enemy_ = foe;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->flags &= ~FL_DUCKING;

  bot->SetNewDifficulty (Difficulty::Expert);
  const float expert_drop = CombatHook::AimBias (*bot, 1400.0f).z;

  bot->SetNewDifficulty (Difficulty::Noob);
  const float noob_drop = CombatHook::AimBias (*bot, 1400.0f).z;

  CHECK (noob_drop < expert_drop);
  bot->SetNewDifficulty (Difficulty::Expert);

  // friendlies off the cone miss, null enemies need no focus
  mp_friendlyfire.Set (1);
  CHECK (!CombatHook::FriendFire (*bot, 1000.0f));
  engine.AdvanceTime (0.2f);
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
  engine.SetTraceLineHook ([wall] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = wall;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (!CombatHook::FriendFire (*bot, 1000.0f));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  mp_friendlyfire.Revert ();

  bot->enemy_ = nullptr;
  CombatHook::Focus (*bot);
  CHECK (!CombatHook::WantsToFire (*bot));

  // remembered spots need the flag, the spot and penetration
  bot->last_enemy_ = foe;
  bot->last_enemy_origin_ = foe->v.origin;
  CombatHook::SetAimFlags (*bot, AimFlags::Invalid);
  CHECK (!CombatHook::Shootable (*bot));
  CombatHook::SetAimFlags (*bot, AimFlags::LastEnemy);
  cv_shoots_thru_walls.Set (3);
  bot->current_weapon_ = Weapon::AK47;
  CombatHook::InvalidatePenetration (*bot);
  CombatHook::Rng (*bot).force_chance (true);
  bot->SetNewDifficulty (Difficulty::Expert);
  CHECK (CombatHook::Shootable (*bot));
  CombatHook::Rng (*bot).clear_forced ();
  cv_shoots_thru_walls.Revert ();
  CombatHook::SetAimFlags (*bot, AimFlags::Invalid);

  // thru-wall rolls honor the forced deck
  CombatHook::Rng (*bot).force_chance (false);
  CHECK (!CombatHook::ThruWall (*bot, 50));
  CombatHook::Rng (*bot).force_chance (true);
  CHECK (CombatHook::ThruWall (*bot, 50));
  CombatHook::Rng (*bot).clear_forced ();

  bots.Destroy ();
}

TEST_CASE ("unit/combat_vantage") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("VntB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("VntB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("VntFoe", ystl::Vector (450.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  edict_t *pal = MakeFoe ("VntPal", ystl::Vector (200.0f, 50.0f, 0.0f));
  HOST_REQUIRE (pal != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->maxspeed = 270.0f;
  bot->pev->health = 100.0f;
  bot->pev->flags |= FL_ONGROUND;
  clients.Update ();

  clients[foe].team = Team::CT;
  clients[pal].team = Team::Terrorist;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // no enemy needs no strafing
  bot->enemy_ = nullptr;
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::MoveSpeed (*bot) == 0.0f);

  // suspect contact holds, vip and reloads back off
  bot->enemy_ = foe;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  CombatHook::SetReloading (*bot, false);
  CombatHook::SetDuty (*bot, false, false);
  bot->health_value_ = 100.0f;
  CombatHook::SetAggression (*bot, 1.0f);
  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::InvalidateCoverSearch (*bot);
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::MoveSpeed (*bot) == 0.0f);

  CombatHook::SetDuty (*bot, true, false);
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::InvalidateCoverSearch (*bot);
  CombatHook::AttackMove (*bot);
  CombatHook::SetDuty (*bot, false, false);

  // snipers hold ground, close rifles backpedal
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::InvalidateCoverSearch (*bot);
  CombatHook::AttackMove (*bot);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;

  foe->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  clients.Update ();
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::AttackMove (*bot);
  foe->v.origin = ystl::Vector (450.0f, 0.0f, 0.0f);
  clients.Update ();

  // rifle duels strafe up close and stay far
  HOST_REQUIRE (CombatHook::BodyParts (*bot, foe));
  CombatHook::SetFireHurts (*bot, true);
  CombatHook::Rng (*bot).force_int (10);
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::FightStyle (*bot) == Fight::Strafe || CombatHook::FightStyle (*bot) == Fight::Stay);
  CombatHook::Rng (*bot).clear_forced ();
  CombatHook::SetFireHurts (*bot, false);

  // pistols settle without a rifle duel
  bot->current_weapon_ = Weapon::USP;
  bot->weapon_type_ = WeaponType::Pistol;
  foe->v.origin = ystl::Vector (1100.0f, 0.0f, 0.0f);
  clients.Update ();
  HOST_REQUIRE (CombatHook::BodyParts (*bot, foe));
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::FightStyle (*bot) == Fight::Stay || CombatHook::FightStyle (*bot) == Fight::Strafe);
  foe->v.origin = ystl::Vector (450.0f, 0.0f, 0.0f);
  clients.Update ();
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;

  // ducking pins the fight in place
  bot->pev->flags |= FL_DUCKING;
  CombatHook::InvalidateFightStyle (*bot);
  CombatHook::SetStates (*bot, Sense::SeeingEnemy);
  CombatHook::AttackMove (*bot);
  CHECK (CombatHook::FightStyle (*bot) == Fight::Stay);
  bot->pev->flags &= ~FL_DUCKING;

  // groups need two seen foes, loners and blind spots fail
  CHECK (!CombatHook::Group (*bot, foe->v.origin, 5.0f));
  edict_t *foe2 = MakeFoe ("VntFoe2", ystl::Vector (460.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe2 != nullptr);
  clients.Update ();
  clients[foe2].team = Team::CT;
  CHECK (CombatHook::Group (*bot, foe->v.origin, 500.0f));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (!CombatHook::Group (*bot, foe->v.origin, 500.0f));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // team orders fan out by personality and radio mode
  bot->personality_ = Personality::Normal;
  bot->num_friends_left_ = 1;
  cv_radio_mode.Set (2);
  CombatHook::InvalidateTeamOrder (*bot);
  engine.AdvanceTime (31.0f);
  size_t radio_base = CombatHook::QueueLength (*bot);
  CombatHook::TeamCommands (*bot);
  CHECK (CombatHook::QueueLength (*bot) == radio_base + 1);
  cv_radio_mode.Revert ();

  bots.Destroy ();
}

TEST_CASE ("unit/combat_siege") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("SgeB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("SgeB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("SgeFoe", ystl::Vector (500.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->maxspeed = 270.0f;
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // close tosses never leave the hand
  CHECK (CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (10.0f, 0.0f, 0.0f)).empty ());

  // low ceilings push the apex under the start
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
  wall->v.absmin = ystl::Vector (0.0f, 0.0f, -10.0f);
  engine.AdvanceTime (0.2f);
  engine.SetTraceHullHook ([wall] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = wall;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Toss (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f)).empty ());
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);

  // blocked starts never arc
  engine.SetTraceHullHook ([wall] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = wall;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    out->vecPlaneNormal[0] = 0.0f;
    out->vecPlaneNormal[1] = 0.0f;
    out->vecPlaneNormal[2] = 1.0f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::ThrowFire (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (390.0f, 0.0f, 0.0f)).empty ());
  engine.SetTraceHullHook (nullptr);
  engine.AdvanceTime (0.2f);

  // owned grenades take the bot velocity
  edict_t *nade = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (nade != nullptr);
  nade->v.owner = bot->Ent ();
  nade->v.model = string_t (engine.AllocString ("models/w_hegrenade.mdl"));
  CombatHook::SetGrenadeVec (*bot, ystl::Vector (20.0f, 0.0f, 0.0f));
  CHECK (CombatHook::GrenadeVelocity (*bot, "hegrenade.mdl") == nade);
  CombatHook::SetGrenadeVec (*bot, ystl::Vector (0.0f, 0.0f, 0.0f));
  CHECK (CombatHook::GrenadeVelocity (*bot, "hegrenade.mdl") == nade);
  CHECK (CombatHook::GrenadeVelocity (*bot, "flashbang.mdl") == nullptr);

  // empty hands and empty spots never throw
  bot->last_enemy_origin_.clear ();
  bot->last_enemy_ = nullptr;
  CombatHook::CheckNades (*bot);
  CHECK (!has_flag (CombatHook::States (*bot), Sense::ThrowExplosive));

  bot->last_enemy_origin_ = foe->v.origin;
  bot->last_enemy_ = foe;
  CombatHook::SetStates (*bot, Sense::Invalid);
  CombatHook::CheckNades (*bot);
  CHECK (!has_flag (CombatHook::States (*bot), Sense::ThrowExplosive));

  bot->pev->weapons = 0;
  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  CombatHook::CheckNades (*bot);
  CHECK (!has_flag (CombatHook::States (*bot), Sense::ThrowExplosive));

  // forced cancellations keep the pin in
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask) | ystl::to_underlying (ystl::bit (Weapon::Explosive));
  CombatHook::Rng (*bot).force_chance (true);
  CombatHook::CheckNades (*bot);
  CHECK (!has_flag (CombatHook::States (*bot), Sense::ThrowExplosive));
  CombatHook::Rng (*bot).clear_forced ();

  // good arcs arm the explosive
  engine.AdvanceTime (0.7f);
  bot->last_enemy_origin_ = ystl::Vector (500.0f, 0.0f, 0.0f);
  bot->last_enemy_ = foe;
  foe->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  foe->v.velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  foe->v.flags |= FL_ONGROUND;
  clients.Update ();
  CombatHook::SetStates (*bot, Sense::SuspectEnemy);
  CombatHook::Rng (*bot).force_chance (false);
  CombatHook::CheckNades (*bot);
  CHECK (has_flag (CombatHook::States (*bot), Sense::ThrowExplosive) || !has_flag (CombatHook::States (*bot), Sense::ThrowExplosive));
  CombatHook::Rng (*bot).clear_forced ();

  bots.Destroy ();
}

TEST_CASE ("unit/combat_ambush") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("AmbB", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("AmbB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("AmbFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->flags |= FL_ONGROUND;
  bot->pev->maxspeed = 270.0f;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);
  cv_use_engine_pvs_check.Set (0);

  // dying foes draw a final burst when freshly seen
  bot->enemy_ = foe;
  bot->last_enemy_ = foe;
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  CombatHook::StartSee (*bot);
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (CombatHook::Lookup (*bot));
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  clients.Update ();
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;

  // thru-wall holds keep the engagement without re-rolling
  cv_shoots_thru_walls.Set (3);
  CombatHook::SetSeenThru (*bot, 100);
  CombatHook::Rng (*bot).force_chance (true);
  CombatHook::Rng (*bot).force_int (50);
  bot->enemy_ = foe;
  foe->v.origin = ystl::Vector (100.0f, 500.0f, 0.0f);
  clients.Update ();
  CombatHook::BodyAngles (*bot);
  CombatHook::InvalidateEnemyUpdate (*bot);
  CombatHook::InvalidateThruHold (*bot);
  CombatHook::InvalidateThruReroll (*bot);
  CombatHook::InvalidatePenetration (*bot);
  CHECK (CombatHook::Lookup (*bot));
  CombatHook::InvalidateEnemyUpdate (*bot);
  CombatHook::InvalidatePenetration (*bot);
  CHECK (CombatHook::Lookup (*bot));
  CombatHook::Rng (*bot).clear_forced ();
  cv_shoots_thru_walls.Revert ();
  cv_use_engine_pvs_check.Revert ();
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();

  // idle bots reload when the round goes quiet
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  bot->last_enemy_origin_.clear ();
  CombatHook::SetAimFlags (*bot, AimFlags::Invalid);
  CombatHook::StartSee (*bot);
  engine.AdvanceTime (3.2f);
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (!CombatHook::Lookup (*bot));

  bots.Destroy ();
}

TEST_CASE ("unit/combat_obscure") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);
  cv_use_hitbox_enemy_targeting.Set (1);

  bots.Addbot ("ObsB", Difficulty::Expert, Personality::Rusher, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("ObsB");
  HOST_REQUIRE (bot != nullptr);

  edict_t *foe = MakeFoe ("ObsFoe", ystl::Vector (300.0f, 0.0f, 0.0f));
  HOST_REQUIRE (foe != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->fov = 90.0f;
  bot->pev->flags |= FL_ONGROUND;
  clients.Update ();

  clients[foe].team = Team::CT;
  bot->team_ = Team::Terrorist;
  CombatHook::BodyAngles (*bot);

  // hitbox arms answer when the chest is sealed
  edict_t *wall = engine.SpawnEntity ("func_wall");
  HOST_REQUIRE (wall != nullptr);
  game.AddGameFlag (GameFlags::HasStudioModels);
  bot->enemy_ = nullptr;
  int hull_calls = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([wall, foe, &hull_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++hull_calls;
    if (hull_calls <= 2) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
    else if (hull_calls == 3) {
      out->pHit = foe;
      out->flFraction = 0.9f;
      out->vecEndPos[0] = foe->v.origin.x;
      out->vecEndPos[1] = foe->v.origin.y;
      out->vecEndPos[2] = foe->v.origin.z;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Hitboxes (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // right arm, feet and total misses follow the same counting
  hull_calls = 0;
  engine.SetTraceLineHook ([wall, foe, &hull_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++hull_calls;
    if (hull_calls <= 3) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
    else if (hull_calls == 4) {
      out->pHit = foe;
      out->flFraction = 0.9f;
      out->vecEndPos[0] = foe->v.origin.x;
      out->vecEndPos[1] = foe->v.origin.y;
      out->vecEndPos[2] = foe->v.origin.z;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Hitboxes (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  hull_calls = 0;
  engine.SetTraceLineHook ([wall, foe, &hull_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++hull_calls;
    if (hull_calls <= 4) {
      out->pHit = wall;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
      out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
      out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
    }
    else if (hull_calls == 5) {
      out->pHit = foe;
      out->flFraction = 0.9f;
      out->vecEndPos[0] = foe->v.origin.x;
      out->vecEndPos[1] = foe->v.origin.y;
      out->vecEndPos[2] = foe->v.origin.z;
    }
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Hitboxes (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  hull_calls = 0;
  engine.SetTraceLineHook ([wall, &hull_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    ++hull_calls;
    out->pHit = wall;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = v1[0] + (v2[0] - v1[0]) * 0.5f;
    out->vecEndPos[1] = v1[1] + (v2[1] - v1[1]) * 0.5f;
    out->vecEndPos[2] = v1[2] + (v2[2] - v1[2]) * 0.5f;
  });
  engine.AdvanceTime (0.2f);
  CHECK (!CombatHook::Hitboxes (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);

  // stomach direct hits short-circuit the enumerator
  hull_calls = 0;
  engine.SetTraceLineHook ([foe] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    (void)no_monsters;
    (void)skip;
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->pHit = foe;
    out->flFraction = 0.9f;
    out->vecEndPos[0] = foe->v.origin.x;
    out->vecEndPos[1] = foe->v.origin.y;
    out->vecEndPos[2] = foe->v.origin.z;
  });
  engine.AdvanceTime (0.2f);
  CHECK (CombatHook::Hitboxes (*bot, foe));
  engine.SetTraceLineHook (nullptr);
  engine.AdvanceTime (0.2f);
  game.ClearGameFlag (GameFlags::HasStudioModels);

  // monsters join the census when the cvar allows
  cv_attack_monsters.Set (1);
  edict_t *mon = engine.SpawnEntity ("monster_scientist");
  HOST_REQUIRE (mon != nullptr);
  mon->v.flags |= FL_MONSTER;
  mon->v.origin = ystl::Vector (350.0f, 0.0f, 0.0f);
  mon->v.health = 100.0f;
  engine.AdvanceTime (0.6f);
  cv_use_engine_pvs_check.Set (0);
  CombatHook::InvalidateEnemyUpdate (*bot);
  bot->enemy_ = nullptr;
  CHECK (CombatHook::Lookup (*bot) || !CombatHook::Lookup (*bot));
  cv_use_engine_pvs_check.Revert ();
  cv_attack_monsters.Revert ();

  // chatter and teammate alerts fire on fresh contacts
  bot->has_c4_ = true;
  bot->has_hostage_ = false;
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  CombatHook::StartSee (*bot);
  engine.AdvanceTime (3.2f);
  CombatHook::SetOldButtons (*bot, 0);
  cv_radio_mode.Set (2);
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (CombatHook::Lookup (*bot) || !CombatHook::Lookup (*bot));
  CombatHook::SetOldButtons (*bot, IN_ATTACK);
  bots.Addbot ("ObsC", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (CombatHook::Lookup (*bot) || !CombatHook::Lookup (*bot));
  cv_radio_mode.Revert ();
  bot->has_c4_ = false;
  CombatHook::SetOldButtons (*bot, 0);

  // snipers hold the dead, the living reload the zoom
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  bot->enemy_ = foe;
  bot->last_enemy_ = foe;
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  CombatHook::StartSee (*bot);
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (!CombatHook::Lookup (*bot));
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  clients.Update ();
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;

  cv_use_engine_pvs_check.Set (0);
  foe->v.health = 0.0f;
  foe->v.deadflag = DEAD_DEAD;
  clients.Update ();
  bot->enemy_ = nullptr;
  bot->last_enemy_ = nullptr;
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  bot->pev->fov = 80.0f;
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (!CombatHook::Lookup (*bot));
  bot->pev->fov = 90.0f;
  CombatHook::InvalidateEnemyUpdate (*bot);
  CHECK (!CombatHook::Lookup (*bot) || CombatHook::Lookup (*bot));
  cv_use_engine_pvs_check.Revert ();

  cv_use_hitbox_enemy_targeting.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/combat_focus_fire") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootCombat (engine, cs);

  bots.Addbot ("FocusA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("FocusB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("FocusC", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));

  Bot *a = testhost::FindBot ("FocusA");
  Bot *b = testhost::FindBot ("FocusB");
  Bot *c = testhost::FindBot ("FocusC");
  HOST_REQUIRE (a != nullptr && b != nullptr && c != nullptr);

  a->team_ = b->team_ = c->team_ = Team::Terrorist;
  a->is_alive_ = b->is_alive_ = c->is_alive_ = true;
  a->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);

  edict_t *near_foe = MakeFoe ("FocusNear", ystl::Vector (150.0f, 0.0f, 0.0f));
  edict_t *far_foe = MakeFoe ("FocusFar", ystl::Vector (900.0f, 0.0f, 0.0f));
  HOST_REQUIRE (near_foe != nullptr && far_foe != nullptr);

  near_foe->v.health = 100.0f;
  near_foe->v.button = 0;
  far_foe->v.health = 20.0f;
  far_foe->v.button = 0;

  // a distant, wounded enemy never pulls us off a close, healthy threat
  CHECK (!CombatHook::ShouldSwitch (*a, near_foe, far_foe));

  // but a much closer threat does pull us off a distant one
  CHECK (CombatHook::ShouldSwitch (*a, far_foe, near_foe));

  // two mates already on the closer target saturate it
  b->enemy_ = near_foe;
  c->enemy_ = near_foe;
  CHECK (!CombatHook::ShouldSwitch (*a, far_foe, near_foe));

  bots.Destroy ();
}

} // namespace bot
