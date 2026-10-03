//
// YaPB test host: unit/weapons_{fire,arms,pickup}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for weapons.cpp through a test-only hook (friend, no
// production behavior changes): penetration methods, firing pauses, zoom
// and burst selectors, weapon bookkeeping, the reload machine and the
// pickup pipeline. Randomness is pinned via exact modes and extreme
// chances, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct WeaponsHook {
  static void SetStates (Bot &bot, Sense states) {
    bot.states_ = states;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static void SetReloading (Bot &bot, bool value) {
    bot.reload_data_.is_reloading = value;
  }
  static void SetReloadState (Bot &bot, Reload state) {
    bot.reload_data_.state = state;
  }
  static Reload ReloadState (Bot &bot) {
    return bot.reload_data_.state;
  }
  static float ShootTime (Bot &bot) {
    return bot.shoot_time_;
  }
  static void SetShootTime (Bot &bot, float value) {
    bot.shoot_time_ = value;
  }
  static void SetWantsFire (Bot &bot, bool value) {
    bot.wants_to_fire_ = value;
  }
  static void SetUsingGrenade (Bot &bot, bool value) {
    bot.is_using_grenade_ = value;
  }
  static void SetJumpDrawn (Bot &bot, bool value) {
    bot.jump_knife_drawn_ = value;
  }
  static bool JumpDrawn (Bot &bot) {
    return bot.jump_knife_drawn_;
  }
  static bool Penetrable (Bot &bot, const ystl::Vector &dest) {
    return bot.IsPenetrableObstacle (dest);
  }
  static bool PenetrableCached (Bot &bot, const ystl::Vector &dest) {
    return bot.IsPenetrableObstacleCached (dest);
  }
  static bool PauseFiring (Bot &bot, float distance) {
    return bot.NeedToPauseFiring (distance);
  }
  static bool Zoom (Bot &bot, float distance) {
    return bot.CheckZoom (distance);
  }
  static void DoFire (Bot &bot) {
    bot.DoFireWeapons ();
  }
  static void Fire (Bot &bot) {
    bot.FireWeapons ();
  }
  static int BestPrimary (Bot &bot) {
    return bot.GetBestPrimaryCarriedIndex ();
  }
  static int BestOwned (Bot &bot) {
    return bot.GetBestOwnedWeaponIndex ();
  }
  static int BestPistol (Bot &bot) {
    return bot.GetBestOwnedPistolIndex ();
  }
  static bool LowAmmo (Bot &bot, Weapon id, float factor) {
    return bot.IsLowOnAmmo (id, factor);
  }
  static bool AnotherWithAmmo (Bot &bot, Weapon except) {
    return bot.HasAnotherWeaponWithAmmoInClip (except);
  }
  static bool AnyAmmo (Bot &bot) {
    return bot.HasAnyAmmo ();
  }
  static void SelectBest (Bot &bot) {
    bot.SelectBestWeapon ();
  }
  static void DrawKnife (Bot &bot, float dist_sq, float height) {
    bot.DrawKnifeForJump (dist_sq, height);
  }
  static void RestoreJump (Bot &bot) {
    bot.RestoreAfterJump ();
  }
  static void SelectSec (Bot &bot) {
    bot.SelectSecondary ();
  }
  static int GetAmmo (Bot &bot, Weapon id) {
    return bot.GetAmmo (id);
  }
  static void SelectByIndex (Bot &bot, int index) {
    bot.SelectWeaponByIndex (index);
  }
  static void SelectById (Bot &bot, Weapon id) {
    bot.SelectWeaponById (id);
  }
  static void Burst (Bot &bot, float distance) {
    bot.CheckBurstMode (distance);
  }
  static void Reload (Bot &bot) {
    bot.CheckReload ();
  }
  static void Pickups (Bot &bot) {
    bot.UpdatePickups ();
  }
  static bool PickupBlocked (Bot &bot) {
    return bot.IsPickupBlocked ();
  }
  static bool ValidateType (Bot &bot, edict_t *ent, Pickup type) {
    return bot.ValidatePickupByType (ent, type);
  }
  static void Finalize (Bot &bot) {
    bot.FinalizePickup ();
  }
  static void EnsureClear (Bot &bot) {
    bot.EnsurePickupEntitiesClear ();
  }
  static bool SmokeBlocked (Bot &bot, const ystl::Vector &from, const ystl::Vector &to) {
    return bot.IsLineBlockedBySmoke (from, to);
  }
  static ystl::Twin<bool, Pickup> Classify (Bot &bot, edict_t *ent) {
    return bot.ClassifyPickupType (ent);
  }
};

namespace {

// eight-node chain for node-backed paths
void BuildWeaponsGraph () {
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

void BootWeapons (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildWeaponsGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();
}

// config index of a weapon id, -1 when absent
int ConfIndexOf (Weapon id) {
  const auto &tab = conf.GetWeapons ();

  for (int i = 0; i < tab.size<int32_t> (); ++i) {
    if (tab[i].id == id) {
      return i;
    }
  }
  return -1;
}

} // namespace

TEST_CASE ("unit/weapons_fire") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnF", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnF");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert); // pin top skill: penetration is chance-rolled
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;

  // clear world never penetrates on methods one and two...
  bot->current_weapon_ = Weapon::AK47;
  cv_shoots_thru_walls.Set (1);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  cv_shoots_thru_walls.Set (2);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...method three passes through, unknown methods refuse
  cv_shoots_thru_walls.Set (3);
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  cv_shoots_thru_walls.Set (0);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // grenade duty and junior difficulties never penetrate either
  cv_shoots_thru_walls.Set (3);
  WeaponsHook::SetUsingGrenade (*bot, true);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  WeaponsHook::SetUsingGrenade (*bot, false);
  bot->SetNewDifficulty (Difficulty::Noob);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  bot->SetNewDifficulty (Difficulty::Expert);

  // staged wall: entry, exit and thickness math check out...
  // (fresh time first: the clear-world traces above are still cached)
  int calls = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&calls] (const float *, const float *, int, edict_t *, TraceResult *out) {
    ++calls;

    out->flFraction = 0.5f;
    out->vecEndPos[0] = calls == 1 ? 250.0f : 325.0f;
    out->vecEndPos[1] = 0.0f;
    out->vecEndPos[2] = 28.0f;
  });
  cv_shoots_thru_walls.Set (1);
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  CHECK (calls == 2);

  // ...too thick is too thick
  calls = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&calls] (const float *, const float *, int, edict_t *, TraceResult *out) {
    ++calls;

    out->flFraction = 0.5f;
    out->vecEndPos[0] = calls == 1 ? 150.0f : 395.0f;
    out->vecEndPos[1] = 0.0f;
    out->vecEndPos[2] = 28.0f;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // cached verdicts survive fresh traces until the window lapses
  cv_shoots_thru_walls.Set (3);
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 1.0f;
  });
  CHECK (WeaponsHook::PenetrableCached (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 0.0f;
    out->pHit = nullptr;
  });
  engine.AdvanceTime (0.1f);
  CHECK (WeaponsHook::PenetrableCached (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  engine.AdvanceTime (0.3f);
  CHECK (!WeaponsHook::PenetrableCached (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // firing pauses only for real recoil at range...
  CHECK (!WeaponsHook::PauseFiring (*bot, 0.0f));
  CHECK (!WeaponsHook::PauseFiring (*bot, 100.0f));
  CHECK (!WeaponsHook::PauseFiring (*bot, 2000.0f));

  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->punchangle = ystl::Vector (-10.0f, 0.0f, 0.0f);
  CHECK (WeaponsHook::PauseFiring (*bot, 2000.0f));
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->SetNewDifficulty (Difficulty::Normal);

  WeaponsHook::SetStates (*bot, Sense::SuspectEnemy);
  CHECK (!WeaponsHook::PauseFiring (*bot, 300.0f));
  WeaponsHook::SetStates (*bot, Sense::Invalid);

  // ...zooms follow distance bands with fov feedback...
  bot->weapon_type_ = WeaponType::Sniper;
  bot->pev->fov = 90.0f;
  CHECK (WeaponsHook::Zoom (*bot, 2000.0f));
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  CHECK (WeaponsHook::Zoom (*bot, 500.0f));
  bot->pev->button = 0;
  bot->pev->fov = 90.0f;
  CHECK (!WeaponsHook::Zoom (*bot, 100.0f));

  bot->weapon_type_ = WeaponType::Rifle;
  CHECK (!WeaponsHook::Zoom (*bot, 2000.0f));

  // ...burst flips on glock and famas lines...
  bot->current_weapon_ = Weapon::Glock18;
  bot->weapon_burst_mode_ = BurstMode::Off;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 100.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  bot->weapon_burst_mode_ = BurstMode::On;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 500.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  bot->current_weapon_ = Weapon::Famas;
  bot->weapon_burst_mode_ = BurstMode::Off;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 500.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  // ...and firing picks the owned rifle up on a clear lane
  edict_t *foe = game.CreateFakeClient ("WpnFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (180.0f, 0.0f, 0.0f);
  foe->v.solid = SOLID_BBOX;
  foe->v.takedamage = DAMAGE_YES;
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 1.0f;
  });
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  bot->enemy_ = foe;
  bot->pev->button = 0;
  WeaponsHook::SetShootTime (*bot, 0.0f);
  WeaponsHook::SetWantsFire (*bot, true);
  WeaponsHook::DoFire (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  CHECK (WeaponsHook::ShootTime (*bot) > 0.0f);

  // grenades and quiet timers hold fire instead
  bot->pev->button = 0;
  WeaponsHook::SetWantsFire (*bot, true);
  WeaponsHook::SetUsingGrenade (*bot, true);
  WeaponsHook::DoFire (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK));
  WeaponsHook::SetUsingGrenade (*bot, false);

  WeaponsHook::SetWantsFire (*bot, false);
  WeaponsHook::DoFire (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK));

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_arms") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnA");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;

  // engine weapon metadata arrives via WeaponList in prod; publish it
  // here or getWeaponProp answers defaults (id Invalid breaks reloads)
  msgs.Add ("WeaponList", 71);
  msgs.Start (bot->Ent (), 71);
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

  // owned indexes resolve through the conf table...
  CHECK (WeaponsHook::BestOwned (*bot) == 0);
  CHECK (WeaponsHook::BestPistol (*bot) == 0);

  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (WeaponsHook::BestOwned (*bot) > 0);
  CHECK (WeaponsHook::BestPrimary (*bot) > 0);

  bot->pev->weapons = ystl::to_underlying (kSecondaryWeaponMask);
  CHECK (WeaponsHook::BestPistol (*bot) > 0);
  bot->pev->weapons = 0;

  // ...low ammo compares the clip against the conf ceiling...
  const int ak_clip = static_cast<int> (Weapon::AK47);
  bot->ammo_in_clip_[ak_clip] = 30;
  CHECK (!WeaponsHook::LowAmmo (*bot, Weapon::AK47, 0.18f));
  bot->ammo_in_clip_[ak_clip] = 3;
  CHECK (WeaponsHook::LowAmmo (*bot, Weapon::AK47, 0.18f));
  bot->ammo_in_clip_[ak_clip] = 0;

  // ...spare guns count only with ammo behind them...
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);

  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  CHECK (!WeaponsHook::AnotherWithAmmo (*bot, Weapon::AK47));
  CHECK (!WeaponsHook::AnyAmmo (*bot));

  bot->ammo_in_clip_[ak_clip] = 30;
  CHECK (WeaponsHook::AnotherWithAmmo (*bot, Weapon::Glock18));
  CHECK (!WeaponsHook::AnotherWithAmmo (*bot, Weapon::AK47));

  const int ak_ammo = conf.GetWeaponProp (Weapon::AK47).ammo1;
  HOST_REQUIRE (ak_ammo >= 0 && ak_ammo < MAX_AMMO_SLOTS);
  CHECK (WeaponsHook::GetAmmo (*bot, Weapon::AK47) == 0);

  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 77;
  }
  CHECK (WeaponsHook::GetAmmo (*bot, Weapon::AK47) == 77);
  CHECK (WeaponsHook::AnyAmmo (*bot));

  // ...best selection keeps the held rifle and clears reloads...
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::SelectBest (*bot);
  CHECK (bot->current_weapon_ == Weapon::AK47);
  CHECK (WeaponsHook::ReloadState (*bot) == Reload::None);

  // ...knife mode forces the blade, secondaries mask primaries...
  cv_jasonmode.Set (1);
  WeaponsHook::SelectBest (*bot);
  CHECK (bot->current_weapon_ == Weapon::AK47); // commands only, hands stay
  cv_jasonmode.Set (0);

  bot->pev->weapons = ystl::to_underlying (kSecondaryWeaponMask);
  WeaponsHook::SelectSec (*bot);
  CHECK (bot->pev->weapons == ystl::to_underlying (kSecondaryWeaponMask));

  // ...indexed and named selects emit exact menu strings
  const int ak_index = ConfIndexOf (Weapon::AK47);
  HOST_REQUIRE (ak_index >= 0);
  const int base = testhost::CsCalls (cs, "ClientCommand");
  WeaponsHook::SelectByIndex (*bot, ak_index);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 1);

  conf.GetWeaponProp (Weapon::AK47).classname = "weapon_ak47";
  WeaponsHook::SelectById (*bot, Weapon::AK47);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 2);

  // ...burst flips on the glock and famas lines...
  bot->current_weapon_ = Weapon::Glock18;
  bot->weapon_burst_mode_ = BurstMode::Off;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 100.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  bot->weapon_burst_mode_ = BurstMode::On;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 500.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  bot->current_weapon_ = Weapon::Famas;
  bot->weapon_burst_mode_ = BurstMode::Off;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 500.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));

  // ...reloads refuse uninterruptible business, then refill dry guns...
  bot->StartTask (TaskId::PlantBomb, TaskPri::plant_bomb, kInvalidNodeIndex, 0.0f, true);
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::Reload (*bot);
  CHECK (WeaponsHook::ReloadState (*bot) == Reload::None);
  bot->ClearTasks ();

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_in_clip_[ak_clip] = 0;
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::Reload (*bot);
  CHECK (WeaponsHook::ReloadState (*bot) == Reload::Primary);

  // ...jump knives draw past range and come back on landing
  // (drawing parks an infinite run-up guard, re-armed to 0.5-1.0s by
  // the jump execute in navigate.cpp, so a bare drawn flag without
  // the jump correctly refuses to restore)
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::DrawKnife (*bot, 0.0f, 0.0f);
  CHECK (WeaponsHook::JumpDrawn (*bot));

  bot->weapon_type_ = WeaponType::Melee;
  WeaponsHook::RestoreJump (*bot);
  CHECK (WeaponsHook::JumpDrawn (*bot));

  bot->pev->flags |= FL_ONGROUND;
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::RestoreJump (*bot);
  CHECK (!WeaponsHook::JumpDrawn (*bot));

  WeaponsHook::SetJumpDrawn (*bot, false);
  WeaponsHook::SetReloading (*bot, false);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::DrawKnife (*bot, ystl::sqrf (200.0f), 0.0f);
  CHECK (WeaponsHook::JumpDrawn (*bot));

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_pickup") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnP", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("WpnQ", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("WpnP");
  Bot *other = testhost::FindBot ("WpnQ");
  HOST_REQUIRE (bot != nullptr && other != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->team_ = Team::Terrorist;

  // pickup blocking follows creature, senses, ladder, task and mode...
  WeaponsHook::SetCreature (*bot, true);
  CHECK (WeaponsHook::PickupBlocked (*bot));
  WeaponsHook::SetCreature (*bot, false);

  WeaponsHook::SetStates (*bot, Sense::SeeingEnemy);
  CHECK (WeaponsHook::PickupBlocked (*bot));
  WeaponsHook::SetStates (*bot, Sense::Invalid);

  bot->pev->movetype = MOVETYPE_FLY;
  CHECK (WeaponsHook::PickupBlocked (*bot));
  bot->pev->movetype = MOVETYPE_WALK;

  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::escape_from_bomb, kInvalidNodeIndex, 0.0f, true);
  CHECK (WeaponsHook::PickupBlocked (*bot));
  bot->ClearTasks ();

  cv_jasonmode.Set (1);
  CHECK (WeaponsHook::PickupBlocked (*bot));
  cv_jasonmode.Set (0);

  CHECK (WeaponsHook::PickupBlocked (*bot)); // nothing interesting yet

  // ...a custom item on the ground walks the whole pipeline, and the
  // shield on the ground is collected into the interesting list too...
  // (kit spawns first: pickup takes the first valid entity in list order)
  cv_pickup_custom_items.Set (1);
  const float shield_pos[3] = { 200.0f, 0.0f, 0.0f };

  edict_t *kit = engine.SpawnEntity ("item_healthkit");
  HOST_REQUIRE (kit != nullptr);
  kit->v.model = engine.AllocString ("models/w_medkit.mdl");
  engine.Funcs ().pfnSetOrigin (kit, shield_pos);

  edict_t *shield = engine.SpawnEntity ("weapon_shield");
  HOST_REQUIRE (shield != nullptr);
  engine.Funcs ().pfnSetOrigin (shield, shield_pos);

  engine.AdvanceTime (0.6f); // interesting-entity refresh runs every 0.5s
  game_state.UpdateInterestingEntities ();
  HOST_REQUIRE (game_state.HasInterestingEntities ());

  auto contains_ent = [] (edict_t *ent) {
    for (const auto &entry : game_state.GetInterestingEntities ()) {
      if (entry.ent == ent) {
        return true;
      }
    }
    return false;
  };

  CHECK (contains_ent (shield));
  CHECK (contains_ent (kit));
  CHECK (!WeaponsHook::PickupBlocked (*bot));

  WeaponsHook::Pickups (*bot);
  CHECK (bot->pickup_item_ == kit);
  CHECK (bot->pickup_type_ == Pickup::Items);

  // ...shield classification answers directly, gated by pickup_best...
  const auto shield_class = WeaponsHook::Classify (*bot, shield);
  CHECK (shield_class.first);
  CHECK (shield_class.second == Pickup::Shield);

  cv_pickup_best.Set (0);
  const auto shield_denied = WeaponsHook::Classify (*bot, shield);
  CHECK (!shield_denied.first);
  cv_pickup_best.Set (1);

  // ...type validation rejects elites, vips and owned primaries...
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Elite));
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  bot->pev->weapons = 0;

  bot->is_vip_ = true;
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Weapon));
  bot->is_vip_ = false;
  CHECK (WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));

  edict_t *ammo = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (ammo != nullptr);
  engine.Funcs ().pfnSetOrigin (ammo, shield_pos);
  ammo->v.model = engine.AllocString ("medkit.mdl");
  CHECK (WeaponsHook::ValidateType (*bot, ammo, Pickup::AmmoAndKits));

  ammo->v.model = engine.AllocString ("models/ak47.mdl");
  CHECK (!WeaponsHook::ValidateType (*bot, ammo, Pickup::AmmoAndKits));

  // ...finalizing keeps, conflicts and drops by height...
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == kit);

  other->is_alive_ = true;
  other->pickup_item_ = kit;
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  CHECK (bot->pickup_type_ == Pickup::None);
  other->pickup_item_ = nullptr;
  other->is_alive_ = false;
  bot->pickup_item_ = kit;
  bot->pickup_type_ = Pickup::Items;

  bot->pickup_item_ = shield;
  bot->pickup_type_ = Pickup::Shield;
  shield->v.origin = ystl::Vector (200.0f, 0.0f, 500.0f);
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  CHECK (bot->ignored_items_.size () == 1);
  shield->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);

  // ...stuck bots shed the pursuit, idle ones pass through...
  bot->pickup_item_ = nullptr;
  WeaponsHook::EnsureClear (*bot);
  CHECK (bot->pickup_item_ == nullptr);

  bot->pickup_item_ = shield;
  bot->pickup_type_ = Pickup::Shield;
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  WeaponsHook::EnsureClear (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  CHECK (bot->pickup_type_ == Pickup::None);
  CHECK (bot->GetTaskId () != TaskId::PickupItem);

  // ...smoke on the sightline reads blocked, offset reads clear
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));

  edict_t *smoke = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (smoke != nullptr);
  smoke->v.model = engine.AllocString ("models/w_smokegrenade.mdl");
  smoke->v.flags |= FL_ONGROUND;
  const float smoke_pos[3] = { 200.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (smoke, smoke_pos);
  sounds.Acquire (smoke, "weapons/sg_explode.wav", 1.0f, 1.0f);
  engine.AdvanceTime (0.3f); // active-grenade refresh runs every 0.25s
  game_state.UpdateActiveGrenade ();
  HOST_REQUIRE (game_state.HasActiveGrenades ());
  CHECK (WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 500.0f, 0.0f), ystl::Vector (400.0f, 500.0f, 0.0f)));

  bots.Destroy ();
}

} // namespace bot
