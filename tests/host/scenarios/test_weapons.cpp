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
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
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
  static void CheckReload (Bot &bot) {
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
  static void Handle (Bot &bot, float distance, Weapon id, int choosen) {
    bot.HandleWeapons (distance, 0, id, choosen);
  }
  static bool BadAtDistance (Bot &bot, int index, float distance) {
    return bot.IsWeaponBadAtDistance (index, distance);
  }
  static bool HasPrimary (Bot &bot) {
    return bot.HasPrimaryWeapon ();
  }
  static bool HasSecondary (Bot &bot) {
    return bot.HasSecondaryWeapon ();
  }
  static bool HasShield (Bot &bot) {
    return bot.HasShield ();
  }
  static bool ShieldDrawn (Bot &bot) {
    return bot.IsShieldDrawn ();
  }
  static bool EnemyShield (Bot &bot, edict_t *enemy) {
    return bot.IsEnemyBehindShield (enemy);
  }
  static int BestSecondary (Bot &bot) {
    return bot.GetBestSecondaryCarriedIndex ();
  }
  static Weapon BestNade (Bot &bot) {
    return bot.GetBestGrenadeCarriedId ();
  }
  static bool RateGround (Bot &bot, edict_t *ent) {
    return bot.RateGroundWeapon (ent);
  }
  static bool HasAnyWeapons (Bot &bot) {
    return bot.HasAnyWeapons ();
  }
  static bool HasAnyAmmoInClip (Bot &bot) {
    return bot.HasAnyAmmoInClip ();
  }
  static void Silencer (Bot &bot) {
    bot.CheckSilencer ();
  }
  static Pickup CheckAmmo (Bot &bot, ystl::StringRef model) {
    return bot.CheckAmmoAndKitsPickup (model);
  }
  static bool HandleTeam (Bot &bot, edict_t *ent, const ystl::Vector &origin, Pickup type) {
    return bot.HandleTeamSpecificPickups (ent, origin, type);
  }
  static bool TerrorHostage (Bot &bot, edict_t *ent, const ystl::Vector &origin) {
    return bot.HandleTerroristHostagePickup (ent, origin);
  }
  static bool TerrorBomb (Bot &bot, const ystl::Vector &origin) {
    return bot.HandleTerroristBombPickup (origin);
  }
  static bool CtHostage (Bot &bot, edict_t *ent) {
    return bot.HandleCtHostagePickup (ent);
  }
  static bool CtBomb (Bot &bot, const ystl::Vector &origin) {
    return bot.HandleCtBombPickup (origin);
  }
  static bool CtDropped (Bot &bot, edict_t *ent, const ystl::Vector &origin) {
    return bot.HandleCtDroppedC4 (ent, origin);
  }
  static bool ValidateExisting (Bot &bot, float radius_sq) {
    return bot.ValidateExistingPickup (game_state.GetInterestingEntities (), radius_sq);
  }
  static void SetLook (Bot &bot, const ystl::Vector &value) {
    bot.look_at_ = value;
  }
  static void SetEnemy (Bot &bot, edict_t *value) {
    bot.enemy_ = value;
  }
  static void SetEnemyOrigin (Bot &bot, const ystl::Vector &value) {
    bot.enemy_origin_ = value;
  }
  static void SetAimFlags (Bot &bot, AimFlags value) {
    bot.aim_flags_ = value;
  }
  static void SetOldButtons (Bot &bot, int value) {
    bot.old_buttons_ = value;
  }
  static void SetFirePause (Bot &bot, float value) {
    bot.fire_pause_ = value;
  }
  static void SetHealth (Bot &bot, float value) {
    bot.health_value_ = value;
  }
  static void SetDefended (Bot &bot, bool value) {
    bot.defended_bomb_ = value;
  }
  static void SetDefendHostage (Bot &bot, bool value) {
    bot.defend_hostage_ = value;
  }
  static void SetProgress (Bot &bot, bool value) {
    bot.has_progress_bar_ = value;
  }
  static void SetVip (Bot &bot, bool value) {
    bot.is_vip_ = value;
  }
  static void SetDefuser (Bot &bot, bool value) {
    bot.has_defuser_ = value;
  }
  static void SetStabUnaware (Bot &bot, bool value) {
    bot.stab_unaware_ = value;
  }
  static void SetStabEnemy (Bot &bot, edict_t *value) {
    bot.stab_unaware_enemy_ = value;
  }
  static void StartBlind (Bot &bot, float duration) {
    bot.blind_timer_.start (duration);
  }
  static void ClearBlind (Bot &bot) {
    bot.blind_timer_.invalidate ();
  }
  static void StartShootDead (Bot &bot, float duration) {
    bot.shoot_at_dead_timer_.start (duration);
  }
  static void ClearShootDead (Bot &bot) {
    bot.shoot_at_dead_timer_.invalidate ();
  }
  static void StartStabTimer (Bot &bot, float duration) {
    bot.stab_unaware_timer_.start (duration);
  }
  static void ClearNoAmmo (Bot &bot) {
    bot.no_ammo_pickup_timer_.invalidate ();
  }
  static void StartNoAmmo (Bot &bot, float duration) {
    bot.no_ammo_pickup_timer_.start (duration);
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

  // material types fixture must exist before level init loads it; entries mirror
  // the real cstrike file: comments, prefixed and lowercase names
  ystl::File::make_path ("sound");
  {
    ystl::File mat {};

    HOST_REQUIRE (mat.open ("sound/materials.txt", "wb"));

    constexpr char kMaterials[] = "// real-file sample\nC CONCRETE\nW WOOD\nM -0cstrike_ME1Pl\nN znow1\n";
    mat.write (kMaterials, sizeof (kMaterials) - 1);
    mat.close ();
  }
  BootWeapons (engine, cs);

  bots.Addbot ("WpnF", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  // ...method four mirrors FireBullets3 penetration: counts per wall over
  // the current power, power shrinks per surface material...
  cv_shoots_thru_walls.Set (4);
  HOST_REQUIRE (conf.HasMaterialTypes ());
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->flFraction = 1.0f;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...a thin wooden slab punches through...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    if (v1[0] < 240.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 240.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 240.0f && v[0] <= 256.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "{WOOD";
  });
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...a thick concrete slab exhausts an ak...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    if (v1[0] < 150.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 150.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 150.0f && v[0] <= 350.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "-0CONCRETE";
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...the same slab in wood still doesn't (wood punches at full power, not bonus)...
  engine.AdvanceTime (0.3f);
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "WOOD";
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...a thin concrete one does, power covers it...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    if (v1[0] < 240.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 240.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 240.0f && v[0] <= 248.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "CONCRETE";
  });
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...engines without TraceTexture count everything as concrete, a wooden
  // slab this thick stops punching through then...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    if (v1[0] < 240.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 240.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 240.0f && v[0] <= 264.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "WOOD";
  });
  const auto saved_trace_texture = engfuncs.pfnTraceTexture;
  engfuncs.pfnTraceTexture = nullptr;
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  engfuncs.pfnTraceTexture = saved_trace_texture;

  // ...and walls past the weapon's penetration range stop the bullet
  bot->current_weapon_ = Weapon::USP;
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    if (v1[0] < 610.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 610.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 610.0f && v[0] <= 618.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "WOOD";
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (900.0f, 0.0f, 28.0f)));
  bot->current_weapon_ = Weapon::AK47;

  engine.SetTraceLineHook (nullptr);
  engine.SetPointContentsHook (nullptr);
  engine.SetTraceTextureHook (nullptr);
  ::remove ("sound/materials.txt");
  cv_shoots_thru_walls.Set (2);

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

TEST_CASE ("unit/weapons_materials") {
  ::remove ("sound/materials.txt");

  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("MatA", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("MatA");
  HOST_REQUIRE (bot != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->current_weapon_ = Weapon::AK47;

  // no materials list: everything counts as concrete and method four delegates
  // to the cheapest check, so the staged entry/exit wall still punches through
  CHECK (!conf.HasMaterialTypes ());
  CHECK (conf.GetMaterialType ("WOOD") == 'C');

  int calls = 0;
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([&calls] (const float *, const float *, int, edict_t *, TraceResult *out) {
    ++calls;

    out->flFraction = 0.5f;
    out->vecEndPos[0] = calls == 1 ? 250.0f : 325.0f;
    out->vecEndPos[1] = 0.0f;
    out->vecEndPos[2] = 28.0f;
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 250.0f && v[0] <= 325.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  cv_shoots_thru_walls.Set (4);
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  CHECK (calls == 2);

  engine.SetTraceLineHook (nullptr);
  engine.SetPointContentsHook (nullptr);
  cv_shoots_thru_walls.Set (2);

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_arms") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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
  bot->StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::CheckReload (*bot);
  CHECK (WeaponsHook::ReloadState (*bot) == Reload::None);
  bot->ClearTasks ();

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_in_clip_[ak_clip] = 0;
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::CheckReload (*bot);
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

  // ...a failed roll keeps the knife out instead of swapping back like clockwork
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::SetReloading (*bot, false);
  bot->pev->weapons |= ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Melee;
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::Rng (*bot).force_chance (false);

  const int swap_base = testhost::CsCalls (cs, "ClientCommand");
  WeaponsHook::RestoreJump (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == swap_base);

  WeaponsHook::Rng (*bot).clear_forced ();
  CHECK (!WeaponsHook::JumpDrawn (*bot));
  CHECK (bot->weapon_type_ == WeaponType::Melee);

  // ...while a passed roll orders the gun back (hands follow via engine ack)
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Melee;
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::RestoreJump (*bot);
  WeaponsHook::Rng (*bot).clear_forced ();
  CHECK (!WeaponsHook::JumpDrawn (*bot));
  CHECK (testhost::CsCalls (cs, "ClientCommand") == swap_base + 1);

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

  bots.Addbot ("WpnP", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("WpnQ", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
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

  bot->StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
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

TEST_CASE ("unit/weapons_vault") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  // full material roster for the method-four multiplier table
  ystl::File::make_path ("sound");
  {
    ystl::File mat {};

    HOST_REQUIRE (mat.open ("sound/materials.txt", "wb"));

    constexpr char kMats[] = "M METAL\nC CONCRETE\nG GLASS\nV VENT\nT TILE\nP PLASTIC\nW WOOD\n";
    mat.write (kMats, sizeof (kMats) - 1);
    mat.close ();
  }
  BootWeapons (engine, cs);

  bots.Addbot ("WpnV", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnV");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->current_weapon_ = Weapon::AK47;

  // ...method one startsolid refuses, second-trace miss refuses...
  cv_shoots_thru_walls.Set (1);
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 1;
    out->fAllSolid = 0;
    out->pHit = nullptr;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = 250.0f;
    out->vecEndPos[1] = 0.0f;
    out->vecEndPos[2] = 28.0f;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 200.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 250.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = v2[0];
      out->vecEndPos[1] = v2[1];
      out->vecEndPos[2] = v2[2];
    }
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...far exit and high exit both refuse...
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;
    out->flFraction = 0.5f;
    out->vecEndPos[0] = 1125.0f;
    out->vecEndPos[1] = 0.0f;
    out->vecEndPos[2] = 28.0f;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (2000.0f, 0.0f, 28.0f)));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;
    out->flFraction = 0.5f;

    if (v1[0] < 200.0f) {
      out->vecEndPos[0] = 250.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->vecEndPos[0] = 325.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 400.0f;
    }
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...method two thin wall near the foe punches, thick wall stops...
  cv_shoots_thru_walls.Set (2);
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 340.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 350.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = v2[0];
      out->vecEndPos[1] = v2[1];
      out->vecEndPos[2] = v2[2];
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 350.0f && v[0] <= 358.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 140.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 150.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = v2[0];
      out->vecEndPos[1] = v2[1];
      out->vecEndPos[2] = v2[2];
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 150.0f && v[0] <= 350.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...method three solid starts refuse, grinding refuses...
  cv_shoots_thru_walls.Set (3);
  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 1;
    out->fAllSolid = 1;
    out->pHit = nullptr;
    out->flFraction = 0.0f;
    out->vecEndPos[0] = v2[0];
    out->vecEndPos[1] = v2[1];
    out->vecEndPos[2] = v2[2];
  });
  engine.SetPointContentsHook (nullptr);
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.AdvanceTime (0.2f);
  engine.SetTraceLineHook ([] (const float *, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 1;
    out->fAllSolid = 0;
    out->pHit = nullptr;
    out->flFraction = 0.0f;
    out->vecEndPos[0] = v2[0];
    out->vecEndPos[1] = v2[1];
    out->vecEndPos[2] = v2[2];
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  // ...method four each surface eats power, thin slabs punch...
  cv_shoots_thru_walls.Set (4);
  const char *kTextures[] = { "METAL", "GLASS", "VENT", "TILE", "PLASTIC" };

  for (int i = 0; i < 5; ++i) {
    const char *tex = kTextures[i];

    engine.AdvanceTime (0.3f);
    engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
      out->fStartSolid = 0;
      out->fAllSolid = 0;
      out->pHit = nullptr;

      if (v1[0] < 240.0f) {
        out->flFraction = 0.5f;
        out->vecEndPos[0] = 240.0f;
        out->vecEndPos[1] = 0.0f;
        out->vecEndPos[2] = 28.0f;
      }
      else {
        out->flFraction = 1.0f;
        out->vecEndPos[0] = 400.0f;
        out->vecEndPos[1] = 0.0f;
        out->vecEndPos[2] = 28.0f;
      }
    });
    engine.SetPointContentsHook ([] (const float *v) {
      return (v[0] >= 240.0f && v[0] <= 248.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
    });
    engine.SetTraceTextureHook ([tex] (edict_t *, const float *, const float *) -> const char * {
      return tex;
    });
    CHECK (WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  }

  // ...first-trace solid refuses, far walls and solid-to-foe refuse...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *, const float *v2, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 1;
    out->fAllSolid = 0;
    out->pHit = nullptr;
    out->flFraction = 0.0f;
    out->vecEndPos[0] = v2[0];
    out->vecEndPos[1] = v2[1];
    out->vecEndPos[2] = v2[2];
  });
  engine.SetPointContentsHook ([] (const float *) {
    return CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "CONCRETE";
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 610.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 610.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = 900.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 610.0f && v[0] <= 618.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  bot->current_weapon_ = Weapon::USP;
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (900.0f, 0.0f, 28.0f)));
  bot->current_weapon_ = Weapon::AK47;

  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 200.0f) {
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 200.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = 400.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *) {
    return CONTENTS_SOLID;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));

  engine.SetTraceLineHook (nullptr);
  engine.SetPointContentsHook (nullptr);
  engine.SetTraceTextureHook (nullptr);
  ::remove ("sound/materials.txt");
  cv_shoots_thru_walls.Set (2);

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_quartz") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnZ", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnZ");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  WeaponsHook::ClearShootDead (*bot);
  WeaponsHook::SetFirePause (*bot, 0.0f);

  // ...dead-timer and pending pause short-circuit...
  WeaponsHook::StartShootDead (*bot, 5.0f);
  CHECK (!WeaponsHook::PauseFiring (*bot, 500.0f));
  WeaponsHook::ClearShootDead (*bot);

  WeaponsHook::SetFirePause (*bot, game.Time () + 1.0f);
  CHECK (WeaponsHook::PauseFiring (*bot, 500.0f));
  WeaponsHook::SetFirePause (*bot, 0.0f);

  // ...shielded foe forces a brief hold...
  edict_t *foe = game.CreateFakeClient ("WpnZoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  foe->v.weaponanim = 6;
  foe->v.viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  bot->pev->weaponanim = 0;
  WeaponsHook::SetAimFlags (*bot, AimFlags::Enemy);
  WeaponsHook::SetEnemy (*bot, foe);
  WeaponsHook::SetEnemyOrigin (*bot, ystl::Vector (400.0f, 0.0f, 28.0f));
  CHECK (WeaponsHook::PauseFiring (*bot, 500.0f));
  WeaponsHook::SetFirePause (*bot, 0.0f);
  WeaponsHook::SetAimFlags (*bot, AimFlags::Invalid);
  WeaponsHook::SetEnemy (*bot, nullptr);

  // ...suspect mid band sprays, plain mid band re-arms...
  WeaponsHook::SetStates (*bot, Sense::SuspectEnemy);
  CHECK (!WeaponsHook::PauseFiring (*bot, 450.0f));
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  CHECK (!WeaponsHook::PauseFiring (*bot, 450.0f));

  // ...pistols use the short pause table on both sides...
  bot->weapon_type_ = WeaponType::Pistol;
  CHECK (!WeaponsHook::PauseFiring (*bot, 600.0f));
  CHECK (!WeaponsHook::PauseFiring (*bot, 400.0f));
  bot->weapon_type_ = WeaponType::Rifle;

  // ...zoomable rifles zoom past 800, unzoom at rest with fov feedback...
  bot->SetNewDifficulty (Difficulty::Easy);
  bot->weapon_type_ = WeaponType::ZoomRifle;
  bot->pev->fov = 90.0f;
  bot->pev->button = 0;
  CHECK (WeaponsHook::Zoom (*bot, 900.0f));
  bot->pev->fov = 90.0f;
  bot->pev->button = 0;
  engine.AdvanceTime (0.6f);
  CHECK (!WeaponsHook::Zoom (*bot, 500.0f));
  bot->pev->fov = 40.0f;
  bot->pev->button = 0;
  engine.AdvanceTime (0.6f);
  CHECK (WeaponsHook::Zoom (*bot, 100.0f));
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->fov = 90.0f;
  bot->pev->button = 0;

  // ...silencers attach and detach, shields and foes gate...
  bot->current_weapon_ = Weapon::USP;
  bot->pev->viewmodel = engine.AllocString ("models/w_usp.mdl");
  WeaponsHook::SetEnemy (*bot, nullptr);
  bot->pev->weaponanim = 7;
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Silencer (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();

  bot->pev->weaponanim = 6;
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (false);
  WeaponsHook::Silencer (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();

  bot->current_weapon_ = Weapon::M4A1;
  bot->pev->weaponanim = 7;
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Silencer (*bot);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();

  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Silencer (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();
  bot->pev->viewmodel = engine.AllocString ("models/w_m4a1.mdl");

  WeaponsHook::SetEnemy (*bot, foe);
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Silencer (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();
  WeaponsHook::SetEnemy (*bot, nullptr);

  bot->current_weapon_ = Weapon::AK47;
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Silencer (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();

  // ...burst ignores shielded hands...
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_glock.mdl");
  bot->current_weapon_ = Weapon::Glock18;
  bot->weapon_burst_mode_ = BurstMode::Off;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 100.0f);
  CHECK (!(bot->pev->button & IN_ATTACK2));
  bot->pev->viewmodel = engine.AllocString ("models/w_glock.mdl");
  bot->current_weapon_ = Weapon::AK47;

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_beacon") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnB");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->ClearTasks ();

  // ...weapon metadata for reload and underwater paths...
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
  msgs.Start (bot->Ent (), 71);
  msgs.Collect ("weapon_usp");
  msgs.Collect (int32_t (6));
  msgs.Collect (int32_t (100));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (1));
  msgs.Collect (int32_t (16));
  msgs.Collect (int32_t (0));
  msgs.Stop ();
  HOST_REQUIRE (conf.GetWeaponProp (Weapon::AK47).id == Weapon::AK47);
  HOST_REQUIRE (conf.GetWeaponProp (Weapon::USP).id == Weapon::USP);

  // ...primary and secondary presence...
  bot->pev->weapons = 0;
  CHECK (!WeaponsHook::HasPrimary (*bot));
  CHECK (!WeaponsHook::HasSecondary (*bot));
  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask);
  CHECK (WeaponsHook::HasPrimary (*bot));
  bot->pev->weapons = ystl::to_underlying (kSecondaryWeaponMask);
  CHECK (WeaponsHook::HasSecondary (*bot));
  bot->pev->weapons = 0;

  // ...shield predicates...
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  bot->pev->weaponanim = 0;
  CHECK (!WeaponsHook::HasShield (*bot));
  CHECK (!WeaponsHook::ShieldDrawn (*bot));
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  CHECK (WeaponsHook::HasShield (*bot));
  CHECK (!WeaponsHook::ShieldDrawn (*bot));
  bot->pev->weaponanim = 6;
  CHECK (WeaponsHook::ShieldDrawn (*bot));
  bot->pev->weaponanim = 0;

  edict_t *foe = game.CreateFakeClient ("WpnBoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  foe->v.weaponanim = 6;
  foe->v.viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;
  CHECK (!WeaponsHook::EnemyShield (*bot, nullptr));
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  bot->pev->weaponanim = 6;
  CHECK (!WeaponsHook::EnemyShield (*bot, foe));
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  bot->pev->weaponanim = 0;
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (WeaponsHook::EnemyShield (*bot, foe));
  foe->v.viewmodel = engine.AllocString ("models/w_ak47.mdl");
  foe->v.weaponanim = 0;
  CHECK (!WeaponsHook::EnemyShield (*bot, foe));
  foe->v.viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  foe->v.weaponanim = 6;
  foe->v.origin = ystl::Vector (-500.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (!WeaponsHook::EnemyShield (*bot, foe));
  foe->v.origin = ystl::Vector (400.0f, 0.0f, 0.0f);
  clients.Update ();

  // ...bad-at-distance matrix...
  const int ak_idx = ConfIndexOf (Weapon::AK47);
  const int awp_idx = ConfIndexOf (Weapon::AWP);
  const int m3_idx = ConfIndexOf (Weapon::M3);
  HOST_REQUIRE (ak_idx >= 0 && awp_idx >= 0 && m3_idx >= 0);
  bot->SetNewDifficulty (Difficulty::Noob);
  CHECK (!WeaponsHook::BadAtDistance (*bot, m3_idx, 800.0f));
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  CHECK (!WeaponsHook::BadAtDistance (*bot, ak_idx, 800.0f));
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47) | ystl::bit (Weapon::USP));
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 0;
  CHECK (!WeaponsHook::BadAtDistance (*bot, awp_idx, 300.0f));
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 12;
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  bot->current_weapon_ = Weapon::Scout;
  CHECK (!WeaponsHook::BadAtDistance (*bot, awp_idx, 300.0f));
  bot->current_weapon_ = Weapon::AK47;
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  WeaponsHook::SetOldButtons (*bot, IN_ATTACK);
  CHECK (!WeaponsHook::BadAtDistance (*bot, awp_idx, 500.0f));
  WeaponsHook::SetOldButtons (*bot, 0);
  CHECK (!WeaponsHook::BadAtDistance (*bot, awp_idx, 100.0f));
  bot->last_fired_timer_.invalidate ();
  engine.AdvanceTime (0.5f);
  CHECK (WeaponsHook::BadAtDistance (*bot, awp_idx, 300.0f));
  CHECK (!WeaponsHook::BadAtDistance (*bot, awp_idx, 500.0f));
  CHECK (WeaponsHook::BadAtDistance (*bot, m3_idx, 800.0f));
  CHECK (!WeaponsHook::BadAtDistance (*bot, m3_idx, 500.0f));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;

  // ...handle reload stale clears, active returns...
  conf.GetWeaponProp (Weapon::Knife).classname = "weapon_knife";
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 90;
  }
  WeaponsHook::SetReloading (*bot, true);
  WeaponsHook::SetReloadState (*bot, Reload::None);
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AK47, ak_idx);
  WeaponsHook::SetReloading (*bot, true);
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 5;
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AK47, ak_idx);
  CHECK (!(bot->pev->button & IN_ATTACK));
  WeaponsHook::SetReloading (*bot, false);
  WeaponsHook::SetReloadState (*bot, Reload::None);

  // ...handle switch and choosen repair...
  bot->current_weapon_ = Weapon::AK47;
  const int cmd_base = testhost::CsCalls (cs, "ClientCommand");
  WeaponsHook::Handle (*bot, 500.0f, Weapon::Knife, 0);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == cmd_base + 1);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  WeaponsHook::ClearBlind (*bot);
  WeaponsHook::ClearShootDead (*bot);
  WeaponsHook::SetFirePause (*bot, 0.0f);
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::Handle (*bot, 100.0f, Weapon::AK47, 0);
  bot->pev->button = 0;
  CHECK (conf.GetWeapons ()[ak_idx].primary_fire_hold);
  WeaponsHook::Handle (*bot, 100.0f, Weapon::AK47, ak_idx);
  // ...repair path covered above, button follows in later legs...

  // ...handle shield draws...
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_m4a1.mdl");
  bot->pev->weaponanim = 0;
  bot->current_weapon_ = Weapon::AK47;
  engine.AdvanceTime (1.2f);
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 800.0f, Weapon::AK47, ak_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  engine.AdvanceTime (1.2f);
  bot->pev->weaponanim = 6;
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AK47, ak_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  bot->pev->weaponanim = 0;
  engine.AdvanceTime (1.2f);

  // ...handle zoom and sniper stops...
  bot->current_weapon_ = Weapon::AWP;
  bot->weapon_type_ = WeaponType::Sniper;
  bot->pev->fov = 90.0f;
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 2000.0f, Weapon::AWP, awp_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  bot->pev->fov = 40.0f;
  bot->pev->button = 0;
  engine.AdvanceTime (0.6f);
  WeaponsHook::SetAimFlags (*bot, static_cast<AimFlags> (ystl::to_underlying (AimFlags::Enemy) | ystl::to_underlying (AimFlags::LastEnemy)));
  bot->pev->velocity = ystl::Vector (10.0f, 0.0f, 0.0f);
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AWP, awp_idx);
  bot->pev->velocity = ystl::Vector (1.0f, 0.0f, 0.0f);
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AWP, awp_idx);
  bot->pev->velocity = ystl::Vector (0.0f, 0.0f, 0.0f);
  WeaponsHook::SetAimFlags (*bot, AimFlags::Invalid);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->fov = 90.0f;

  // ...handle knife close and autofire...
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Melee;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Handle (*bot, 50.0f, Weapon::Knife, ConfIndexOf (Weapon::Knife));
  CHECK (!!(bot->pev->button & IN_ATTACK));
  WeaponsHook::Rng (*bot).clear_forced ();
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (false);
  WeaponsHook::Handle (*bot, 50.0f, Weapon::Knife, ConfIndexOf (Weapon::Knife));
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::Rng (*bot).clear_forced ();
  WeaponsHook::SetCreature (*bot, true);
  bot->pev->button = 0;
  WeaponsHook::Rng (*bot).force_chance (true);
  WeaponsHook::Handle (*bot, 75.0f, Weapon::Knife, ConfIndexOf (Weapon::Knife));
  CHECK (!!(bot->pev->button & IN_ATTACK));
  WeaponsHook::Rng (*bot).clear_forced ();
  WeaponsHook::SetCreature (*bot, false);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  WeaponsHook::Handle (*bot, 100.0f, Weapon::AK47, ak_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  const int usp_idx = ConfIndexOf (Weapon::USP);
  HOST_REQUIRE (usp_idx >= 0);
  bot->current_weapon_ = Weapon::USP;
  bot->weapon_type_ = WeaponType::Pistol;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::Handle (*bot, 100.0f, Weapon::USP, usp_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));

  // ...handle knife far, pause, hold and delays...
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::Knife, ConfIndexOf (Weapon::Knife));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->punchangle = ystl::Vector (-10.0f, 0.0f, 0.0f);
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 2000.0f, Weapon::AK47, ak_idx);
  CHECK (!(bot->pev->button & IN_ATTACK));
  bot->pev->punchangle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->pev->button = 0;
  WeaponsHook::SetFirePause (*bot, 0.0f);
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AK47, ak_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  bot->current_weapon_ = Weapon::USP;
  bot->weapon_type_ = WeaponType::Pistol;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::SetShootTime (*bot, 0.0f);
  WeaponsHook::Handle (*bot, 500.0f, Weapon::USP, usp_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  CHECK (WeaponsHook::ShootTime (*bot) > 0.0f);
  const int deagle_idx = ConfIndexOf (Weapon::Deagle);
  HOST_REQUIRE (deagle_idx >= 0);
  bot->current_weapon_ = Weapon::Deagle;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::SetShootTime (*bot, 0.0f);
  WeaponsHook::Handle (*bot, 500.0f, Weapon::Deagle, deagle_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));
  const int m3full_idx = ConfIndexOf (Weapon::M3);
  HOST_REQUIRE (m3full_idx >= 0);
  bot->current_weapon_ = Weapon::M3;
  bot->weapon_type_ = WeaponType::Shotgun;
  bot->pev->button = 0;
  WeaponsHook::SetOldButtons (*bot, 0);
  WeaponsHook::SetShootTime (*bot, 0.0f);
  WeaponsHook::Handle (*bot, 500.0f, Weapon::M3, m3full_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK));

  // ...fire grenade, friend, knife mode and stab...
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetUsingGrenade (*bot, true);
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  CHECK (!(bot->pev->button & IN_ATTACK));
  WeaponsHook::SetUsingGrenade (*bot, false);

  mp_friendlyfire.Set (1);
  edict_t *pal = game.CreateFakeClient ("WpnBal");
  HOST_REQUIRE (!game.IsNullEntity (pal));
  pal->v.flags &= ~FL_FAKECLIENT;
  pal->v.health = 100.0f;
  clients.Update ();
  clients[pal].team = Team::Terrorist;
  pal->v.origin = bot->pev->origin + bot->pev->view_ofs + ystl::Vector (100.0f, 0.0f, 0.0f);
  clients.Update ();
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  WeaponsHook::SetLook (*bot, ystl::Vector (500.0f, 0.0f, 28.0f));
  WeaponsHook::SetWantsFire (*bot, true);
  WeaponsHook::SetShootTime (*bot, 0.0f);
  WeaponsHook::Fire (*bot);
  mp_friendlyfire.Set (0);

  cv_jasonmode.Set (1);
  bot->pev->button = 0;
  WeaponsHook::SetLook (*bot, bot->pev->origin + bot->pev->view_ofs + ystl::Vector (50.0f, 0.0f, 0.0f));
  WeaponsHook::Fire (*bot);
  cv_jasonmode.Set (0);

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  WeaponsHook::SetHealth (*bot, 100.0f);
  foe->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  clients.Update ();
  WeaponsHook::SetEnemy (*bot, foe);
  WeaponsHook::SetLook (*bot, ystl::Vector (150.0f, 0.0f, 28.0f));
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  WeaponsHook::SetStabUnaware (*bot, false);
  WeaponsHook::SetStabEnemy (*bot, foe);
  WeaponsHook::StartStabTimer (*bot, 5.0f);
  WeaponsHook::Rng (*bot).force_chance (true);
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  WeaponsHook::Rng (*bot).clear_forced ();
  WeaponsHook::SetEnemy (*bot, nullptr);

  // ...fire underwater skip, dry refill, reload arm and knife fall...
  bot->pev->waterlevel = 3;
  conf.GetWeaponProp (Weapon::AK47).flags = ITEM_FLAG_NOFIREUNDERWATER;
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  bot->current_weapon_ = Weapon::AK47;
  WeaponsHook::SetLook (*bot, ystl::Vector (500.0f, 0.0f, 28.0f));
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  bot->pev->waterlevel = 0;
  conf.GetWeaponProp (Weapon::AK47).flags = 0;

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  bot->ammo_[1] = 90;
  bot->current_weapon_ = Weapon::USP;
  bot->weapon_type_ = WeaponType::Pistol;
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::SetLook (*bot, ystl::Vector (700.0f, 0.0f, 28.0f));
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);

  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::Rng (*bot).force_chance (false);
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  WeaponsHook::Rng (*bot).clear_forced ();

  bot->pev->weapons = 0;
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 0;
  }
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Melee;
  WeaponsHook::SetLook (*bot, ystl::Vector (500.0f, 0.0f, 28.0f));
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);

  mp_friendlyfire.Revert ();
  cv_jasonmode.Set (0);
  cv_shoots_thru_walls.Set (2);
  engine.SetTraceLineHook (nullptr);

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_ledger") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnL", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnL");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->ClearTasks ();

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
  msgs.Start (bot->Ent (), 71);
  msgs.Collect ("weapon_usp");
  msgs.Collect (int32_t (6));
  msgs.Collect (int32_t (100));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (1));
  msgs.Collect (int32_t (16));
  msgs.Collect (int32_t (0));
  msgs.Stop ();
  HOST_REQUIRE (conf.GetWeaponProp (Weapon::AK47).id == Weapon::AK47);

  // ...secondary index respects shields and pistols...
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::USP));
  CHECK (WeaponsHook::BestSecondary (*bot) >= 0);
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  CHECK (WeaponsHook::BestSecondary (*bot) >= 0);
  CHECK (WeaponsHook::BestPrimary (*bot) >= 0);
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");

  // ...grenade ladder explosive, smoke, flash, none...
  bot->pev->weapons = 0;
  CHECK (WeaponsHook::BestNade (*bot) == Weapon::Invalid);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Flashbang));
  CHECK (WeaponsHook::BestNade (*bot) == Weapon::Flashbang);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Smoke));
  CHECK (WeaponsHook::BestNade (*bot) == Weapon::Smoke);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Explosive));
  CHECK (WeaponsHook::BestNade (*bot) == Weapon::Explosive);
  bot->pev->weapons = 0;

  // ...ground rating prefers better prefs, dry drops and empty pools...
  edict_t *ground = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (ground != nullptr);
  ground->v.model = engine.AllocString ("models/w_ak47.mdl");
  ground->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::USP));
  WeaponsHook::ClearNoAmmo (*bot);
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 0;
  }
  CHECK (WeaponsHook::RateGround (*bot, ground));
  bot->dropped_dry_weapons_mask_ = ystl::to_underlying (ystl::bit (Weapon::AK47));
  CHECK (!WeaponsHook::RateGround (*bot, ground));
  bot->dropped_dry_weapons_mask_ = 0;
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 12;
  bot->ammo_[6] = 100;
  ground->v.model = engine.AllocString ("models/w_usp.mdl");
  CHECK (!WeaponsHook::RateGround (*bot, ground));

  // ...clip presence needs owned guns and loaded mags...
  bot->pev->weapons = 0;
  CHECK (!WeaponsHook::HasAnyAmmoInClip (*bot));
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  CHECK (!WeaponsHook::HasAnyAmmoInClip (*bot));
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 0;
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47) | ystl::bit (Weapon::USP));
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 12;
  CHECK (WeaponsHook::HasAnyAmmoInClip (*bot));
  CHECK (WeaponsHook::HasAnyWeapons (*bot));
  bot->pev->weapons = 0;
  CHECK (!WeaponsHook::HasAnyWeapons (*bot));

  // ...best keeps the rifle, swaps dry mags near foes...
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47) | ystl::bit (Weapon::USP));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 90;
  }
  WeaponsHook::SetReloading (*bot, false);
  WeaponsHook::SelectBest (*bot);
  CHECK (bot->current_weapon_ == Weapon::AK47);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 0;
  WeaponsHook::SetStates (*bot, Sense::SeeingEnemy);
  WeaponsHook::SelectBest (*bot);
  WeaponsHook::SetStates (*bot, Sense::Invalid);

  // ...jump knife early outs and restores...
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::DrawKnife (*bot, 0.0f, 0.0f);
  CHECK (WeaponsHook::JumpDrawn (*bot));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetJumpDrawn (*bot, false);
  WeaponsHook::SetStates (*bot, Sense::SeeingEnemy);
  WeaponsHook::DrawKnife (*bot, ystl::sqrf (200.0f), 0.0f);
  CHECK (!WeaponsHook::JumpDrawn (*bot));
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::SetJumpDrawn (*bot, false);
  bot->current_weapon_ = Weapon::Knife;
  bot->weapon_type_ = WeaponType::Melee;
  WeaponsHook::RestoreJump (*bot);
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->flags &= ~FL_ONGROUND;
  WeaponsHook::SetJumpDrawn (*bot, true);
  WeaponsHook::RestoreJump (*bot);
  CHECK (WeaponsHook::JumpDrawn (*bot));
  bot->pev->flags |= FL_ONGROUND;
  WeaponsHook::SetJumpDrawn (*bot, false);

  // ...reload clears full mags, arms dry primaries and walks stages...
  bot->current_weapon_ = Weapon::AK47;
  WeaponsHook::SetReloading (*bot, true);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  bot->ammo_[1] = 90;
  WeaponsHook::CheckReload (*bot);
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 0;
  bot->ammo_[1] = 90;
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::SetReloading (*bot, false);
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::CheckReload (*bot);
  CHECK (WeaponsHook::ReloadState (*bot) != Reload::None);
  WeaponsHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 12;
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47) | ystl::bit (Weapon::USP));
  WeaponsHook::CheckReload (*bot);
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  bot->pev->weapons = 0;
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::CheckReload (*bot);
  CHECK (WeaponsHook::ReloadState (*bot) == Reload::None);
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_[1] = 0;
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::CheckReload (*bot);
  bot->ammo_[1] = 90;
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  WeaponsHook::SetReloadState (*bot, Reload::Primary);
  WeaponsHook::SetStates (*bot, Sense::SeeingEnemy);
  bot->see_enemy_timer_.start ();
  engine.AdvanceTime (0.1f);
  WeaponsHook::CheckReload (*bot);
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::SetReloading (*bot, false);

  // ...ammo edges read -1 past the table...
  CHECK (WeaponsHook::GetAmmo (*bot, Weapon::AK47) == 90);
  conf.GetWeaponProp (Weapon::AK47).ammo1 = -1;
  CHECK (WeaponsHook::GetAmmo (*bot, Weapon::AK47) == -1);
  conf.GetWeaponProp (Weapon::AK47).ammo1 = 99;
  CHECK (WeaponsHook::GetAmmo (*bot, Weapon::AK47) == -1);
  conf.GetWeaponProp (Weapon::AK47).ammo1 = 1;

  // ...famas burst engages close...
  bot->pev->viewmodel = engine.AllocString ("models/w_famas.mdl");
  bot->current_weapon_ = Weapon::Famas;
  bot->weapon_burst_mode_ = BurstMode::On;
  bot->pev->button = 0;
  WeaponsHook::Burst (*bot, 300.0f);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  bot->current_weapon_ = Weapon::AK47;
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");

  // ...blocked pickups clear, existing ones hold...
  WeaponsHook::SetCreature (*bot, true);
  WeaponsHook::Pickups (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  WeaponsHook::SetCreature (*bot, false);

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_onyx") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootWeapons (engine, cs);

  bots.Addbot ("WpnO", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("WpnO");
  HOST_REQUIRE (bot != nullptr);
  bot->SetNewDifficulty (Difficulty::Expert);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  bot->ClearTasks ();

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

  const int ak_idx = ConfIndexOf (Weapon::AK47);
  const int awp_idx = ConfIndexOf (Weapon::AWP);
  HOST_REQUIRE (ak_idx >= 0 && awp_idx >= 0);

  // ...shield redraw via reload flag...
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_m4a1.mdl");
  bot->pev->weaponanim = 0;
  engine.AdvanceTime (1.2f);
  WeaponsHook::SetReloading (*bot, true);
  bot->pev->button = 0;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::AK47, ak_idx);
  CHECK (!!(bot->pev->button & IN_ATTACK2));
  WeaponsHook::SetReloading (*bot, false);
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  engine.AdvanceTime (1.2f);

  // ...rifle never bad with a loaded sidearm...
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47) | ystl::bit (Weapon::USP));
  bot->ammo_in_clip_[static_cast<int> (Weapon::USP)] = 12;
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  bot->current_weapon_ = Weapon::AK47;
  bot->last_fired_timer_.invalidate ();
  engine.AdvanceTime (0.5f);
  WeaponsHook::SetOldButtons (*bot, 0);
  CHECK (!WeaponsHook::BadAtDistance (*bot, ak_idx, 500.0f));

  // ...recent switch pins the held gun...
  bot->current_weapon_ = Weapon::AK47;
  WeaponsHook::Handle (*bot, 500.0f, Weapon::Knife, 0);
  CHECK (WeaponsHook::BadAtDistance (*bot, awp_idx, 500.0f));
  engine.AdvanceTime (3.5f);

  // ...far walls outrun the ak range...
  ystl::File::make_path ("sound");
  {
    ystl::File mat {};

    HOST_REQUIRE (mat.open ("sound/materials.txt", "wb"));

    constexpr char kMats[] = "C CONCRETE\nW WOOD\n";
    mat.write (kMats, sizeof (kMats) - 1);
    mat.close ();
  }
  bot->current_weapon_ = Weapon::AK47;
  cv_shoots_thru_walls.Set (4);
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *, int, edict_t *, TraceResult *out) {
    out->fStartSolid = 0;
    out->fAllSolid = 0;
    out->pHit = nullptr;

    if (v1[0] < 1000.0f) {
      out->flFraction = 0.9f;
      out->vecEndPos[0] = 5500.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->flFraction = 1.0f;
      out->vecEndPos[0] = 6000.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 5500.0f && v[0] <= 5508.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  engine.SetTraceTextureHook ([] (edict_t *, const float *, const float *) -> const char * {
    return "CONCRETE";
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (6000.0f, 0.0f, 28.0f)));

  // ...second-trace solid grinds power forward...
  engine.AdvanceTime (0.3f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int, edict_t *, TraceResult *out) {
    out->pHit = nullptr;

    if (v1[0] < 200.0f) {
      out->fStartSolid = 0;
      out->fAllSolid = 0;
      out->flFraction = 0.5f;
      out->vecEndPos[0] = 240.0f;
      out->vecEndPos[1] = 0.0f;
      out->vecEndPos[2] = 28.0f;
    }
    else {
      out->fStartSolid = 1;
      out->fAllSolid = 0;
      out->flFraction = 0.0f;
      out->vecEndPos[0] = v2[0];
      out->vecEndPos[1] = v2[1];
      out->vecEndPos[2] = v2[2];
    }
  });
  engine.SetPointContentsHook ([] (const float *v) {
    return (v[0] >= 240.0f && v[0] <= 248.0f) ? CONTENTS_SOLID : CONTENTS_EMPTY;
  });
  CHECK (!WeaponsHook::Penetrable (*bot, ystl::Vector (400.0f, 0.0f, 28.0f)));
  engine.SetTraceLineHook (nullptr);
  engine.SetPointContentsHook (nullptr);
  engine.SetTraceTextureHook (nullptr);
  ::remove ("sound/materials.txt");
  cv_shoots_thru_walls.Set (2);

  // ...unaware stab re-rolls, dry guns call for help, empty hands take knife...
  edict_t *foe = game.CreateFakeClient ("WpnOoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (150.0f, 0.0f, 0.0f);
  foe->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  bot->ammo_in_clip_[static_cast<int> (Weapon::AK47)] = 30;
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 90;
  }
  WeaponsHook::SetHealth (*bot, 100.0f);
  WeaponsHook::SetEnemy (*bot, foe);
  WeaponsHook::SetLook (*bot, ystl::Vector (150.0f, 0.0f, 28.0f));
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  WeaponsHook::SetStabEnemy (*bot, nullptr);
  WeaponsHook::Rng (*bot).force_chance (true);
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  WeaponsHook::Rng (*bot).clear_forced ();
  WeaponsHook::SetEnemy (*bot, nullptr);

  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::AK47));
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  bot->ammo_[1] = 90;
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetReloadState (*bot, Reload::None);
  WeaponsHook::SetStates (*bot, Sense::Invalid);
  WeaponsHook::SetLook (*bot, ystl::Vector (700.0f, 0.0f, 28.0f));
  WeaponsHook::Rng (*bot).force_chance (true);
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);
  WeaponsHook::Rng (*bot).clear_forced ();

  bot->pev->weapons = 0;
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 0;
  }
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  WeaponsHook::SetLook (*bot, ystl::Vector (500.0f, 0.0f, 28.0f));
  bot->pev->button = 0;
  WeaponsHook::Fire (*bot);

  bots.Destroy ();
}

TEST_CASE ("unit/weapons_cobalt") {
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
  edict_t *bomb_target = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb_target != nullptr);
  edict_t *hostage_map = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hostage_map != nullptr);
  hostage_map->v.health = 100.0f;
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildWeaponsGraph ();
  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));
  HOST_REQUIRE (game.MapIs (MapFlags::HostageRescue));
  bots.InitQuota ();
  cv_quota.Set (10);
  practice.Load ();

  bots.Addbot ("WpnC", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("WpnD", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("WpnC");
  Bot *ct = testhost::FindBot ("WpnD");
  HOST_REQUIRE (bot != nullptr && ct != nullptr);
  bot->pev->origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  bot->pev->v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  bot->team_ = Team::Terrorist;
  ct->pev->origin = ystl::Vector (120.0f, 0.0f, 0.0f);
  ct->pev->view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  ct->team_ = Team::CT;
  bot->ClearTasks ();
  ct->ClearTasks ();

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

  // ...classify every family...
  edict_t *hos = engine.SpawnEntity ("hostage_entity");
  HOST_REQUIRE (hos != nullptr);
  hos->v.health = 100.0f;
  engine.Funcs ().pfnSetOrigin (hos, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, hos).second == Pickup::Hostage);

  edict_t *dropped = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (dropped != nullptr);
  dropped->v.model = engine.AllocString ("models/w_backpack.mdl");
  engine.Funcs ().pfnSetOrigin (dropped, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, dropped).second == Pickup::DroppedC4);
  cv_ignore_objectives.Set (1);
  CHECK (WeaponsHook::Classify (*bot, dropped).second != Pickup::DroppedC4);
  cv_ignore_objectives.Set (0);

  edict_t *box = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (box != nullptr);
  box->v.model = engine.AllocString ("models/w_ak47.mdl");
  engine.Funcs ().pfnSetOrigin (box, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, box).second == Pickup::Weapon);
  cv_pickup_best.Set (0);
  CHECK (!WeaponsHook::Classify (*bot, box).first);
  cv_pickup_best.Set (1);

  edict_t *armoury = engine.SpawnEntity ("armoury_entity");
  HOST_REQUIRE (armoury != nullptr);
  armoury->v.model = engine.AllocString ("models/w_ak47.mdl");
  engine.Funcs ().pfnSetOrigin (armoury, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, armoury).first);

  edict_t *shield = engine.SpawnEntity ("weapon_shield");
  HOST_REQUIRE (shield != nullptr);
  engine.Funcs ().pfnSetOrigin (shield, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, shield).second == Pickup::Shield);

  edict_t *kit = engine.SpawnEntity ("item_thighpack");
  HOST_REQUIRE (kit != nullptr);
  engine.Funcs ().pfnSetOrigin (kit, ct->pev->origin);
  CHECK (WeaponsHook::Classify (*ct, kit).second == Pickup::DefusalKit);
  ct->has_defuser_ = true;
  CHECK (WeaponsHook::Classify (*ct, kit).second != Pickup::DefusalKit);
  ct->has_defuser_ = false;

  edict_t *planted = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (planted != nullptr);
  planted->v.model = engine.AllocString ("models/w_c4.mdl");
  engine.Funcs ().pfnSetOrigin (planted, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, planted).second == Pickup::PlantedC4);

  cv_pickup_custom_items.Set (1);
  edict_t *custom = engine.SpawnEntity ("item_healthkit");
  HOST_REQUIRE (custom != nullptr);
  custom->v.model = engine.AllocString ("models/w_medkit.mdl");
  engine.Funcs ().pfnSetOrigin (custom, bot->pev->origin);
  CHECK (WeaponsHook::Classify (*bot, custom).second == Pickup::Items);
  cv_pickup_custom_items.Set (0);
  CHECK (!WeaponsHook::Classify (*bot, custom).first);

  edict_t *plain = engine.SpawnEntity ("info_target");
  HOST_REQUIRE (plain != nullptr);
  engine.Funcs ().pfnSetOrigin (plain, bot->pev->origin);
  CHECK (!WeaponsHook::Classify (*bot, plain).first);

  // ...ammo and kits gate on loadout...
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::USP) | ystl::bit (Weapon::AK47));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 90;
  }
  bot->health_value_ = 100.0f;
  bot->pev->armorvalue = 100.0f;
  CHECK (WeaponsHook::CheckAmmo (*bot, "357ammobox.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "9mmarclip.mdl") == Pickup::AmmoAndKits);
  CHECK (WeaponsHook::CheckAmmo (*bot, "shotbox.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "9mmclip.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "crossbow_clip.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "chainammo.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "medkit.mdl") == Pickup::Weapon);
  CHECK (WeaponsHook::CheckAmmo (*bot, "kevlar.mdl") == Pickup::Weapon);
  bot->pev->weapons |= ystl::to_underlying (ystl::bit (Weapon::Flashbang));
  CHECK (WeaponsHook::CheckAmmo (*bot, "flashbang.mdl") == Pickup::Weapon);
  bot->pev->weapons |= ystl::to_underlying (ystl::bit (Weapon::Explosive));
  CHECK (WeaponsHook::CheckAmmo (*bot, "hegrenade.mdl") == Pickup::Weapon);
  bot->pev->weapons |= ystl::to_underlying (ystl::bit (Weapon::Smoke));
  CHECK (WeaponsHook::CheckAmmo (*bot, "smokegrenade.mdl") == Pickup::Weapon);
  bot->health_value_ = 50.0f;
  bot->pev->armorvalue = 0.0f;
  bot->pev->weapons = 0;
  CHECK (WeaponsHook::CheckAmmo (*bot, "battery.mdl") == Pickup::AmmoAndKits);

  // ...type validation: vips, cooldowns, models and shields...
  WeaponsHook::SetVip (*bot, true);
  CHECK (!WeaponsHook::ValidateType (*bot, box, Pickup::Weapon));
  WeaponsHook::SetVip (*bot, false);
  WeaponsHook::StartNoAmmo (*bot, 5.0f);
  CHECK (!WeaponsHook::ValidateType (*bot, box, Pickup::Weapon));
  WeaponsHook::ClearNoAmmo (*bot);
  bot->pev->weapons = 0;
  for (int i = 0; i < kMaxWeapons; ++i) {
    bot->ammo_in_clip_[i] = 0;
  }
  for (int i = 0; i < MAX_AMMO_SLOTS; ++i) {
    bot->ammo_[i] = 0;
  }
  CHECK (WeaponsHook::ValidateType (*bot, box, Pickup::Weapon));
  WeaponsHook::ClearNoAmmo (*bot);
  edict_t *ammo_ent = engine.SpawnEntity ("weaponbox");
  HOST_REQUIRE (ammo_ent != nullptr);
  ammo_ent->v.model = engine.AllocString ("models/battery.mdl");
  engine.Funcs ().pfnSetOrigin (ammo_ent, bot->pev->origin);
  CHECK (WeaponsHook::ValidateType (*bot, ammo_ent, Pickup::AmmoAndKits));
  ammo_ent->v.model = engine.AllocString ("models/w_ak47.mdl");
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::USP));
  CHECK (!WeaponsHook::ValidateType (*bot, ammo_ent, Pickup::AmmoAndKits));
  bot->pev->weapons = ystl::to_underlying (ystl::bit (Weapon::Elite));
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  bot->pev->weapons = 0;
  bot->pev->viewmodel = engine.AllocString ("models/shield/v_shield_usp.mdl");
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  bot->pev->viewmodel = engine.AllocString ("models/w_ak47.mdl");
  WeaponsHook::SetVip (*bot, true);
  CHECK (!WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  WeaponsHook::SetVip (*bot, false);
  CHECK (WeaponsHook::ValidateType (*bot, shield, Pickup::Shield));
  CHECK (WeaponsHook::ValidateType (*bot, hos, Pickup::Hostage));

  // ...team routing and direct handlers...
  CHECK (WeaponsHook::HandleTeam (*bot, dropped, dropped->v.origin, Pickup::DroppedC4));
  CHECK (!WeaponsHook::HandleTeam (*bot, hos, hos->v.origin, Pickup::Hostage));
  CHECK (!WeaponsHook::HandleTeam (*bot, planted, planted->v.origin, Pickup::PlantedC4));
  CHECK (WeaponsHook::HandleTeam (*ct, hos, hos->v.origin, Pickup::Hostage));
  CHECK (WeaponsHook::HandleTeam (*bot, box, box->v.origin, Pickup::Weapon));
  WeaponsHook::SetDefended (*bot, false);
  WeaponsHook::TerrorBomb (*bot, planted->v.origin);
  WeaponsHook::SetDefended (*bot, false);
  WeaponsHook::SetDefendHostage (*bot, false);
  WeaponsHook::Rng (*bot).force_chance (false);
  WeaponsHook::TerrorHostage (*bot, hos, hos->v.origin);
  WeaponsHook::Rng (*bot).clear_forced ();
  hos->v.health = 0.0f;
  CHECK (!WeaponsHook::CtHostage (*ct, hos));
  hos->v.health = 100.0f;
  edict_t *human = game.CreateFakeClient ("WpnHum");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.origin = hos->v.origin;
  clients.Update ();
  clients[human].team = Team::CT;
  CHECK (!WeaponsHook::CtHostage (*ct, human));
  human->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  clients.Update ();
  CHECK (WeaponsHook::CtHostage (*ct, hos));
  WeaponsHook::SetEnemy (*ct, nullptr);
  WeaponsHook::SetProgress (*ct, false);
  WeaponsHook::SetDefended (*ct, true);
  CHECK (WeaponsHook::CtBomb (*ct, planted->v.origin) || !WeaponsHook::CtBomb (*ct, planted->v.origin));
  WeaponsHook::SetDefended (*ct, false);
  WeaponsHook::Rng (*ct).force_chance (false);
  WeaponsHook::CtDropped (*ct, dropped, dropped->v.origin);
  WeaponsHook::Rng (*ct).clear_forced ();

  // ...finalize height gates per family...
  bot->pickup_item_ = hos;
  bot->pickup_type_ = Pickup::Hostage;
  hos->v.origin = ystl::Vector (100.0f, 0.0f, 500.0f);
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  hos->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pickup_item_ = planted;
  bot->pickup_type_ = Pickup::PlantedC4;
  planted->v.origin = ystl::Vector (100.0f, 0.0f, 500.0f);
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  planted->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  bot->pickup_item_ = box;
  bot->pickup_type_ = Pickup::Weapon;
  box->v.origin = ystl::Vector (100.0f, 0.0f, 500.0f);
  WeaponsHook::Finalize (*bot);
  CHECK (bot->pickup_item_ == nullptr);
  box->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);

  // ...stuck guard bounds the ignore list...
  bot->ignored_items_.clear ();
  for (int i = 0; i < 70; ++i) {
    bot->ignored_items_.push (box);
  }
  bot->pickup_item_ = box;
  bot->pickup_type_ = Pickup::Weapon;
  bot->StartTask (TaskId::PickupItem, 50.0f, kInvalidNodeIndex, 0.0f, true);
  WeaponsHook::EnsureClear (*bot);
  CHECK (bot->ignored_items_.size () == 70);

  // ...smoke geometry: empty, nodraw, grounded and flash kinds...
  edict_t *flash = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (flash != nullptr);
  flash->v.model = engine.AllocString ("models/w_flashbang.mdl");
  flash->v.flags |= FL_ONGROUND;
  engine.Funcs ().pfnSetOrigin (flash, bot->pev->origin);
  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 0.0f)));
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));

  edict_t *smoke = engine.SpawnEntity ("grenade");
  HOST_REQUIRE (smoke != nullptr);
  smoke->v.model = engine.AllocString ("models/w_smokegrenade.mdl");
  smoke->v.flags |= FL_ONGROUND;
  engine.Funcs ().pfnSetOrigin (smoke, bot->pev->origin);
  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));
  sounds.Acquire (smoke, "weapons/sg_explode.wav", 1.0f, 1.0f);
  engine.AdvanceTime (0.3f);
  game_state.UpdateActiveGrenade ();
  smoke->v.effects |= EF_NODRAW;
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));
  smoke->v.effects &= ~EF_NODRAW;
  smoke->v.flags &= ~FL_ONGROUND;
  CHECK (!WeaponsHook::SmokeBlocked (*bot, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (400.0f, 0.0f, 0.0f)));

  cv_ignore_objectives.Set (0);
  cv_pickup_best.Set (1);
  cv_pickup_custom_items.Set (0);
  engine.SetTraceLineHook (nullptr);

  bots.Destroy ();
}

} // namespace bot
