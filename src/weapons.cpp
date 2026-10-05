//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

bool Bot::IsPenetrableObstacle (const ystl::Vector &dest) {
  // this function returns true if enemy can be shoot through some obstacle, false otherwise

  if (is_using_grenade_ || !rg.chance (Skill ())) {
    return false;
  }
  auto penetrate_power = conf.FindWeaponById (current_weapon_).penetrate_power;

  if (penetrate_power == 0) {
    return false;
  }
  const auto method = cv_shoots_thru_walls.As<int> ();

  // switch methods
  switch (method) {
  case 1:
    return IsPenetrableObstacle1 (dest, penetrate_power);

  case 2:
    return IsPenetrableObstacle2 (dest, penetrate_power);

  case 3:
    return IsPenetrableObstacle3 (dest, penetrate_power);
  };
  return false;
}

bool Bot::IsPenetrableObstacleCached (const ystl::Vector &dest) {
  // cache wall traces until the timer lapses or the target moves

  if (penetration_check_timer_.elapsed () || penetration_check_origin_.distance_sq (dest) > ystl::sqrf (64.0f)) {
    penetration_result_ = IsPenetrableObstacle (dest);
    penetration_check_origin_ = dest;
    penetration_check_timer_.start (0.25f);
  }
  return penetration_result_;
}

bool Bot::IsPenetrableObstacle1 (const ystl::Vector &dest, int penetrate_power) const {
  Trace::Result tr {};

  trace.Line (GetEyesPos (), dest, TraceIgnore::Everything, Ent (), &tr);

  // vecendpos is unreliable when fstartsolid, skip this case
  if (tr.start_solid) {
    return false;
  }

  // trace didn't hit anything, no obstacle
  if (ystl::fequal (tr.fraction, 1.0f)) {
    return false;
  }

  // hit a wall, trace back from dest to find obstacle exit point
  const ystl::Vector wall_entry = tr.end_pos;

  trace.Line (dest, wall_entry, TraceIgnore::Everything, Ent (), &tr);

  if (ystl::fequal (tr.fraction, 1.0f)) {
    return false;
  }
  const ystl::Vector &wall_exit = tr.end_pos;

  if (wall_exit.distance_sq (dest) > ystl::sqrf (800.0f)) {
    return false;
  }

  if (wall_exit.z >= dest.z + 200.0f) {
    return false;
  }
  float obstacle_distance_sq = wall_exit.distance_sq (wall_entry);

  if (obstacle_distance_sq > 0.0f) {
    constexpr float kMaxDistanceSq = ystl::sqrf (75.0f);

    while (penetrate_power > 0) {
      if (obstacle_distance_sq > kMaxDistanceSq) {
        obstacle_distance_sq -= kMaxDistanceSq;
        penetrate_power--;

        continue;
      }
      return true;
    }
  }
  return false;
}

bool Bot::IsPenetrableObstacle2 (const ystl::Vector &dest, int penetrate_power) const {
  // this function returns if enemy can be shoot through some obstacle

  const ystl::Vector source = GetEyesPos ();
  const ystl::Vector direction = (dest - source).normalize (); // 1 unit long

  int thickness = 0;
  int num_hits = 0;

  ystl::Vector point = dest;
  Trace::Result tr {};

  const int max_thickness = penetrate_power * 24;

  trace.Line (source, dest, TraceIgnore::Everything, Ent (), &tr);

  while (!ystl::fequal (tr.fraction, 1.0f) && num_hits < penetrate_power) {
    num_hits++;
    thickness++;

    point = tr.end_pos + direction;

    while (thickness < max_thickness && engfuncs.pfnPointContents (point) == CONTENTS_SOLID) {
      point = point + direction;
      thickness++;
    }
    trace.Line (point, dest, TraceIgnore::Everything, Ent (), &tr);
  }

  if (num_hits == 0) {
    return false;
  }

  if (num_hits < penetrate_power && thickness < max_thickness) {
    if (dest.distance_sq (point) < ystl::sqrf (112.0f)) {
      return true;
    }
  }
  return false;
}

bool Bot::IsPenetrableObstacle3 (const ystl::Vector &dest, int penetrate_power) const {
  // this function returns if enemy can be shoot through some obstacle

  Trace::Result tr {};

  ystl::Vector source = GetEyesPos ();
  const ystl::Vector dir = (dest - source).normalize () * 8.0f;

  float max_thickness = static_cast<float> (penetrate_power) * 75.0f;

  for (int iter = 0; iter < 64; ++iter) {
    trace.Line (source, dest, TraceIgnore::Everything, Ent (), &tr);

    if (tr.start_solid) {
      if (tr.all_solid) {
        return false;
      }
      source += dir;
      max_thickness -= 8.0f;

      if (max_thickness <= 0.0f) {
        return false;
      }
    }
    else {
      // check if line hit anything
      if (ystl::fequal (tr.fraction, 1.0f)) {
        return true;
      }

      if (--penetrate_power == 0) {
        return false;
      }
      source = tr.end_pos + dir;
    }
  }
  return false;
}

bool Bot::NeedToPauseFiring (float distance) {
  // returns true if bot needs to pause between firing to compensate for punchangle & weapon spread

  // validate inputs
  if (distance <= 0.0f || !pev) {
    return false;
  }

  // special cases where we never pause firing
  if (UsesSniper () || is_using_grenade_ || (has_flag (states_, Sense::SuspectEnemy) && distance < 400.0f)) {
    return false;
  }

  // currently simulating humans firing few bullets at dead corpses
  if (!shoot_at_dead_timer_.elapsed ()) {
    return false;
  }

  if (fire_pause_ > game.Time ()) {
    return true;
  }

  // prevent negative or zero pausing
  auto normalize_fire_pause = [] (const float calc, const float delay) -> float {
    return calc <= game.Time () ? game.Time () + delay : calc;
  };

  // check for shielded enemy, pause briefly to avoid wasting ammo
  if (has_flag (aim_flags_, AimFlags::Enemy) && !enemy_origin_.empty ()) {
    if (util.ViewDot (Ent (), enemy_origin_) > 0.92f && IsEnemyBehindShield (enemy_)) {

      float shield_pause_duration = rg (0.3f, 0.7f);
      fire_pause_ = normalize_fire_pause (game.Time () + shield_pause_duration - frame_interval_, 0.3f);

      return true;
    }
  }

  // close range, never pause, just spray
  if (distance < kSprayDistance) {
    return false;
  }

  // calculate base offset and pause time based on distance
  float base_offset = 3.0f;
  float base_pause_time = 0.55f;

  if (distance < kSprayDistanceX2) {

    // suspect enemies keep spraying without pausing
    if (has_flag (states_, Sense::SuspectEnemy)) {
      return false;
    }
    base_offset = 2.0f;
    base_pause_time = 0.38f;
  }

  // pistols recover faster, so use shorter pauses for them
  if (UsesPistol ()) {
    base_pause_time = distance < kSprayDistanceX2 ? 0.18f : 0.25f;
  }
  constexpr float kToleranceExpert = 0.0f;
  constexpr float kToleranceNoob = 1.0f;

  const float vertical_rad = ystl::deg2rad (pev->punchangle.x);
  const float horizontal_rad = ystl::deg2rad (pev->punchangle.y);

  const float total_angle_rad = ystl::sqrtf (ystl::sqrf (vertical_rad) + ystl::sqrf (horizontal_rad));
  const float recoil_deviation = ystl::tanf (total_angle_rad) * distance;

  const auto clamped_difficulty = static_cast<float> (ystl::clamp (ystl::to_underlying (difficulty_), 0, 4));
  const auto max_allowed_recoil = static_cast<float> (difficulty_data_->max_recoil);

  const float t = static_cast<float> (clamped_difficulty) / 4.0f;
  const float tolerance = kToleranceNoob * (1.0f - t) + kToleranceExpert * t;

  // threshold = base allowance + recoil tolerance + difficulty adjustment
  const float threshold = base_offset + max_allowed_recoil + tolerance;

  // check if recoil has exceeded threshold
  if (recoil_deviation > threshold) {
    const float additional_pause = max_allowed_recoil * 0.01f * tolerance;
    const float pause_duration = rg (base_pause_time, base_pause_time + additional_pause);

    // safety check ensure we don't schedule a zero or negative pause
    fire_pause_ = normalize_fire_pause (game.Time () + pause_duration - frame_interval_, base_pause_time);
    return true;
  }
  return false;
}

bool Bot::CheckZoom (float distance) {
  enum class ZoomLevel : int {
    None = 0,
    Low = 1,
    High = 2
  };

  ZoomLevel zoom_magnification = ZoomLevel::None;
  bool zoom_change = false;

  // is the bot holding a sniper rifle?
  if (UsesSniper ()) {

    // should the bot switch to the long-range zoom?
    if (distance > 1500.0f) {
      zoom_magnification = ZoomLevel::High;
    }

    // else should the bot switch to the close-range zoom ?
    else if (distance > 150.0f) {
      zoom_magnification = ZoomLevel::Low;
    }

    // else should the bot restore the normal view ?
    else if (distance <= 150.0f) {
      zoom_magnification = ZoomLevel::None;
    }
  }

  // else is the bot holding a zoomable rifle?
  else if (difficulty_ < Difficulty::Normal && UsesZoomableRifle ()) {
    // should the bot switch to zoomed mode?
    if (distance > 800.0f) {
      zoom_magnification = ZoomLevel::Low;
    }

    // else should the bot restore the normal view?
    else if (distance <= 800.0f) {
      zoom_magnification = ZoomLevel::None;
    }
  }

  switch (zoom_magnification) {
  case ZoomLevel::None:
    if (pev->fov < 90.0f) {
      zoom_change = true;
    }
    break;

  case ZoomLevel::Low:
    if (pev->fov >= 90.0f) {
      zoom_change = true;
    }
    break;

  case ZoomLevel::High:
    if (pev->fov >= 40.0f) {
      zoom_change = true;
    }
    break;
  }

  if (zoom_change && zoom_check_timer_.elapsed ()) {
    pev->button |= IN_ATTACK2;
    shoot_time_ = game.Time () + 0.15f;

    zoom_check_timer_.start (0.5f);
  }
  return zoom_change;
}

void Bot::HandleWeapons (float distance, int, Weapon id, int choosen) {
  const auto &tab = conf.GetWeapons ();

  // never switch weapons mid-reload, wait until it finishes
  if (reload_data_.is_reloading) {
    if (reload_data_.state == Reload::None || GetAmmoInClip () >= conf.FindWeaponById (current_weapon_).max_clip || GetAmmo () <= 0) {
      reload_data_.is_reloading = false; // stale flag, no active reload sequence
    }
    else {
      return;
    }
  }

  // select this weapon if it isn't already selected
  if (current_weapon_ != id) {
    SelectWeaponById (id);

    // reset burst fire variables
    fire_pause_ = 0.0f;
    last_fired_timer_.invalidate ();

    // record switch time to prevent rapid switching
    last_weapon_switch_timer_.start ();

    return;
  }

  if (tab[choosen].id != id) {
    choosen = 0;

    // find the weapon entry for this id, clamp to last entry if not found
    while (choosen < tab.size<int32_t> () && tab[choosen].id != id) {
      choosen++;
    }
    choosen = ystl::min (choosen, tab.size<int32_t> () - 1);
  }

  // if we're have a glock or famas vary burst fire mode
  CheckBurstMode (distance);

  // better shield gun usage
  if (HasShield () && shield_check_timer_.elapsed () && GetTaskId () != TaskId::Camp) {
    const bool has_enemy = !game.IsNullEntity (enemy_);

    if (distance >= 750.0f && !IsShieldDrawn ()) {
      pev->button |= IN_ATTACK2; // draw the shield
    }
    else if (IsShieldDrawn () || reload_data_.is_reloading || (has_enemy && (enemy_->v.button & IN_RELOAD)) ||
             (has_enemy && !SeesEntity (enemy_->v.origin))) {

      pev->button |= IN_ATTACK2; // draw out the shield
    }
    shield_check_timer_.start (1.0f);
  }

  if (CheckZoom (distance)) {
    return;
  }

  // we're should stand still before firing sniper weapons, else sniping is useless
  if (UsesSniper () && has_flag (aim_flags_, AimFlags::Enemy | AimFlags::LastEnemy) && !reload_data_.is_reloading &&
      pev->velocity.length_sq () > 0.0f) {

    if (!ystl::fzero (pev->velocity.x) || !ystl::fzero (pev->velocity.y) || !ystl::fzero (pev->velocity.z)) {
      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;
      nav_timer_.start ();

      if (ystl::abs (pev->velocity.x) > 5.0f || ystl::abs (pev->velocity.y) > 5.0f || ystl::abs (pev->velocity.z) > 5.0f) {
        sniper_stop_timer_.start (2.0f);
        return;
      }
    }
  }
  const float time_delta = game.Time () - frame_interval_;

  // need to care for burst fire?
  if ((distance < kSprayDistance && !IsRecoilHigh ()) || !blind_timer_.elapsed () || UsesKnife ()) {
    if (id == Weapon::Knife) {
      const float min_attack_distance = is_creature_ ? 80.0f : 72.0f;

      if (distance < min_attack_distance) {
        const auto primary_attack_chance = (old_buttons_ & IN_ATTACK2) ? 80 : 40;

        if (rg.chance (primary_attack_chance) || HasShield ()) {
          pev->button |= IN_ATTACK; // use primary attack
        }
        else {
          pev->button |= IN_ATTACK2; // use secondary attack
        }
      }
    }
    else {
      // if automatic weapon press attack
      if (tab[choosen].primary_fire_hold) {
        pev->button |= IN_ATTACK;
      }

      // if not, toggle
      else {
        if ((old_buttons_ & IN_ATTACK) == 0) {
          pev->button |= IN_ATTACK;
        }
      }
    }

    if (pev->button & IN_ATTACK) {
      shoot_time_ = time_delta;
    }
  }
  else {
    // don't attack with knife over long distance
    if (id == Weapon::Knife) {
      shoot_time_ = time_delta;
      return;
    }

    if (NeedToPauseFiring (distance)) {
      return;
    }

    if (tab[choosen].primary_fire_hold) {
      shoot_time_ = time_delta;
      zoom_check_timer_.start (0.0f);

      pev->button |= IN_ATTACK; // use primary attack
    }
    else {
      if ((old_buttons_ & IN_ATTACK) == 0) {
        pev->button |= IN_ATTACK;
      }

      // values here never overclock shotguns/snipers, they only unthrottle pistols
      const auto &fire_delay = conf.GetFireDelay ();
      const int offset = ystl::abs<int> (Skill () / 20 - 5);

      if (tab[choosen].type == WeaponType::Pistol) {
        // deagle has a noticeably heavier cycle than the rest of the pistols
        const float deagle_extra = id == Weapon::Deagle ? fire_delay.deagle_extra : 0.0f;

        shoot_time_ = time_delta + fire_delay.pistol_base + deagle_extra + rg (fire_delay.pistol_min[offset], fire_delay.pistol_max[offset]);
      }
      else {
        shoot_time_ = time_delta + fire_delay.other_base + rg (fire_delay.other_min[offset], fire_delay.other_max[offset]);
      }
      zoom_check_timer_.start (0.0f);
    }
  }
}

void Bot::DoFireWeapons () {
  // the bots wants to fire at something?

  if (!shoot_at_dead_timer_.elapsed () || (wants_to_fire_ && !is_using_grenade_ && shoot_time_ <= game.Time ())) {
    FireWeapons (); // if bot didn't fire a bullet try again next frame
  }
}

void Bot::FireWeapons () {
  // this function will return true if weapon was fired, false otherwise

  // do not handle this if with grenade, as it's done it throw grenade task
  if (is_using_grenade_) {
    return;
  }
  const float distance = look_at_.distance (GetEyesPos ()); // how far away is the enemy?

  // or if friend in line of fire, stop this too but do not update shoot time
  if (IsFriendInLineOfFire (distance)) {
    fire_hurts_friend_ = true;
    return;
  }
  else {
    fire_hurts_friend_ = false;
  }
  auto select_id = Weapon::Knife;
  auto select_index = 0, choosen_weapon = 0;

  const auto &tab = conf.GetWeapons ();

  // if knife mode use knife only
  if (IsKnifeMode ()) {
    HandleWeapons (distance, select_index, select_id, choosen_weapon);
    return;
  }

  // use knife if near and good difficulty (l33t dude!)
  if (!game.Is (GameFlags::ZombieMod) && cv_stab_close_enemies && rg.chance (ystl::max (25, Skill ())) && health_value_ > 80.0f &&
      !game.IsNullEntity (enemy_) && distance < 100.0f && !IsGroupOfEnemies (pev->origin) && GetTaskId () != TaskId::Camp) {

    // the less the enemy is looking at us, the more likely we go for the knife (re-roll on enemy change or after a while, enemy may turn)
    if (stab_unaware_enemy_ != enemy_ || stab_unaware_timer_.elapsed ()) {
      const float enemy_dot = util.ViewDot (enemy_, pev->origin);

      stab_unaware_ = rg.chance (static_cast<int> (ystl::clamp (45.0f * (1.0f - enemy_dot), 0.0f, 90.0f)));
      stab_unaware_enemy_ = enemy_;
      stab_unaware_timer_.start (rg (1.0f, 2.0f));
    }

    if (stab_unaware_) {
      HandleWeapons (distance, select_index, select_id, choosen_weapon);
      return;
    }
  }

  // loop through all the weapons
  for (select_index = 0; select_index < tab.size<int32_t> (); ++select_index) {
    const auto wid = tab[select_index].id;

    // is the bot carrying this weapon?
    if (has_flag (pev->weapons, ystl::bit (wid))) {
      // keep this weapon if it has ammo and suits the distance
      if (ammo_in_clip_[wid] > 0 && !IsWeaponBadAtDistance (select_index, distance)) {
        const auto &prop = conf.GetWeaponProp (wid);

        // skip the weapons that cannot be used underwater (regamedll addition)
        if (!(pev->waterlevel == 3 && (prop.flags & ITEM_FLAG_NOFIREUNDERWATER))) {
          choosen_weapon = select_index;
        }
      }
    }
  }
  select_id = tab[choosen_weapon].id;

  // if no available weapon
  if (choosen_weapon == 0) {
    // all mags are dry, fall back to a refillable primary if safe
    const int primary_index = GetBestOwnedWeaponIndex ();

    if (primary_index >= kPrimaryWeaponMinIndex && tab[primary_index].id != current_weapon_ && GetAmmo (tab[primary_index].id) > 0 &&
        !(has_flag (states_, Sense::SeeingEnemy) && distance < 500.0f)) {
      select_id = tab[primary_index].id;
      choosen_weapon = primary_index;

      HandleWeapons (distance, select_index, select_id, choosen_weapon); // draw the primary, refill comes from checkreload
      return;
    }

    for (select_index = 0; select_index < tab.size<int32_t> (); ++select_index) {
      const auto wid = tab[select_index].id;

      // is the bot carrying this weapon?
      if (has_flag (pev->weapons, ystl::bit (wid))) {
        if (GetAmmo (wid) >= tab[select_index].min_primary_ammo && wid == current_weapon_) {

          // available ammo found, arm the reload stage for the weapon we're holding
          if (reload_data_.state == Reload::None || !reload_data_.check_timer.elapsed ()) {
            reload_data_.state = has_flag (kSecondaryWeaponMask, ystl::bit (current_weapon_)) ? Reload::Secondary : Reload::Primary;
            reload_data_.check_timer.invalidate ();

            if (rg.chance (ystl::abs (Skill () - 100)) && rg.chance (25)) {
              PushRadioChat (RadioChat::NeedBackup);
            }
          }
          return;
        }
      }
    }
    select_id = Weapon::Knife; // no available ammo, use knife!
  }
  HandleWeapons (distance, select_index, select_id, choosen_weapon);
}

bool Bot::IsWeaponBadAtDistance (int weapon_index, float distance) {
  // check if the pistol beats the primary weapon at this distance

  // do not switch weapons when crossing the distance line
  const auto &tab = conf.GetWeapons ();

  if (difficulty_ < Difficulty::Easy || !HasSecondaryWeapon ()) {
    return false;
  }
  const auto weapon_type = tab[weapon_index].type;

  if (weapon_type == WeaponType::Melee || !(weapon_type == WeaponType::Shotgun || weapon_type == WeaponType::Sniper)) {
    return false;
  }

  // check is ammo available for secondary weapon (uses index, convert to id for array access)
  if (ammo_in_clip_[tab[GetBestOwnedPistolIndex ()].id] <= 0) {
    return false;
  }

  // scout sniper can shoot at any distance, do not switch
  if (current_weapon_ == Weapon::Scout) {
    return false;
  }

  // prevent rapid weapon switching (to-and-back) - add delay after switch
  const float k_weapon_switch_delay = 3.0f; // seconds to wait before allowing another switch

  if (last_weapon_switch_timer_.less_than (k_weapon_switch_delay)) {
    // keep whatever the bot holds right now until cooldown expires
    return tab[weapon_index].id != current_weapon_;
  }

  // if already shooting or very close to enemy, don't switch sniper (causes delay, bot dies while switching)
  if (weapon_type == WeaponType::Sniper) {

    // if bot is actively firing or enemy is very close, just shoot instead of switching
    const bool is_actively_firing = (old_buttons_ & IN_ATTACK) || last_fired_timer_.less_than (0.3f);
    const bool is_enemy_very_close = distance < 150.0f; // too close to safely switch

    if (is_actively_firing || is_enemy_very_close) {
      return false;
    }
  }

  // better use pistol in short range distances, when using sniper weapons
  if (weapon_type == WeaponType::Sniper && distance < 400.0f) {
    return true;
  }

  // shotguns is too inaccurate at long distances, so weapon is bad
  if (weapon_type == WeaponType::Shotgun && distance > 750.0f) {
    return true;
  }
  return false;
}

bool Bot::HasPrimaryWeapon () const {
  // this function returns returns true, if bot has a primary weapon

  return (pev->weapons & kPrimaryWeaponMask) != 0;
}

bool Bot::HasSecondaryWeapon () const {
  // this function returns returns true, if bot has a secondary weapon

  return (pev->weapons & kSecondaryWeaponMask) != 0;
}

bool Bot::HasShield () {
  // this function returns true, if bot has a tactical shield

  return pev->viewmodel.str (14).starts_with ("v_shield_");
}

bool Bot::IsShieldDrawn () {
  // this function returns true, is the tactical shield is drawn

  if (!HasShield ()) {
    return false;
  }
  return pev->weaponanim == 6 || pev->weaponanim == 7;
}

bool Bot::IsEnemyBehindShield (edict_t *enemy) {
  // this function returns true, if enemy protected by the shield

  if (game.IsNullEntity (enemy) || IsShieldDrawn ()) {
    return false;
  }

  // check if enemy has shield and this shield is drawn
  if ((enemy->v.weaponanim == 6 || enemy->v.weaponanim == 7) && enemy->v.viewmodel.str (14).starts_with ("v_shield_")) {
    if (util.IsInViewCone (pev->origin, enemy)) {
      return true;
    }
  }
  return false;
}

int Bot::GetBestPrimaryCarriedIndex () {
  // return the config index of the best carried primary weapon

  const auto &pref = conf.GetWeaponPrefs (personality_);

  int weapon_index = 0;
  int weapons = pev->weapons;

  const auto &tab = conf.GetWeapons ();

  // take the shield in account
  if (HasShield ()) {
    weapons |= ystl::bit (Weapon::Shield);
  }

  for (int i = 0; i < kNumWeapons; ++i) {
    if (has_flag (weapons, ystl::bit (tab[pref[i]].id))) {
      weapon_index = i;
    }
  }
  return weapon_index;
}

int Bot::GetBestSecondaryCarriedIndex () {
  // return the config index of the best carried secondary weapon

  const auto &pref = conf.GetWeaponPrefs (personality_);

  int weapon_index = 0;
  int weapons = pev->weapons;

  // take the shield in account
  if (HasShield ()) {
    weapons |= ystl::bit (Weapon::Shield);
  }
  const auto &tab = conf.GetWeapons ();

  for (int i = 0; i < kNumWeapons; ++i) {
    const auto id = tab[pref[i]].id;

    if (has_flag (weapons, ystl::bit (id)) && conf.GetWeaponType (id) == WeaponType::Pistol) {
      weapon_index = i;
      break;
    }
  }
  return weapon_index;
}

Weapon Bot::GetBestGrenadeCarriedId () const {
  if (has_flag (pev->weapons, ystl::bit (Weapon::Explosive))) {
    return Weapon::Explosive;
  }
  else if (has_flag (pev->weapons, ystl::bit (Weapon::Smoke))) {
    return Weapon::Smoke;
  }
  else if (has_flag (pev->weapons, ystl::bit (Weapon::Flashbang))) {
    return Weapon::Flashbang;
  }
  return Weapon::Invalid;
}

bool Bot::RateGroundWeapon (edict_t *ent) {
  // this function compares weapons on the ground to the one the bot is using

  int ground_index = 0;
  int ground_table_index = 0;

  const auto &pref = conf.GetWeaponPrefs (personality_);
  const auto &tab = conf.GetWeapons ();

  for (int i = 0; i < kNumWeapons; ++i) {
    if (ent->v.model.str (9) == tab[pref[i]].model) {
      ground_index = i;
      ground_table_index = pref[i];
      break;
    }
  }
  int has_weapon = 0;

  if (ground_table_index < kPrimaryWeaponMinIndex) {
    has_weapon = GetBestSecondaryCarriedIndex ();
  }
  else {
    has_weapon = GetBestPrimaryCarriedIndex ();
  }

  if (ground_index > has_weapon) {
    return true;
  }

  // pick up anything if fully out of ammo and off cooldown
  if (no_ammo_pickup_timer_.elapsed ()) {
    bool completely_out_of_ammo = true;

    for (int i = 1; i < kMaxWeapons; ++i) {
      if (pev->weapons & ystl::bit (i)) {
        if (ammo_in_clip_[i] > 0 || GetAmmo (static_cast<Weapon> (i)) > 0) {
          completely_out_of_ammo = false;
          break;
        }
      }
    }

    // if completely out of ammo, pick up any weapon regardless of rating
    if (completely_out_of_ammo) {
      return true;
    }
  }
  return false;
}

bool Bot::HasAnyWeapons () const {
  return has_flag (pev->weapons, kPrimaryWeaponMask | kSecondaryWeaponMask);
}

bool Bot::IsLowOnAmmo (const Weapon id, const float factor) const {
  return static_cast<float> (ammo_in_clip_[id]) < static_cast<float> (conf.FindWeaponById (id).max_clip) * factor;
}

bool Bot::HasAnyAmmoInClip () {
  bool has_ammo = false;

  if (!HasAnyWeapons ()) {
    return false;
  }
  const auto pri = GetBestOwnedWeaponIndex ();
  const auto sec = GetBestOwnedPistolIndex ();

  if (pri > 0 || sec > 0) {
    const auto &tab = conf.GetWeapons ();

    has_ammo = (pri > 0 && ammo_in_clip_[tab[pri].id] > 0) || (sec > 0 && ammo_in_clip_[tab[sec].id] > 0);
  }
  return has_ammo;
}

bool Bot::HasAnotherWeaponWithAmmoInClip (const Weapon except) const {
  if (!HasAnyWeapons ()) {
    return false;
  }

  for (int i = 1; i < kMaxWeapons; ++i) {
    const auto wid = static_cast<Weapon> (i);

    if (wid == except || wid == Weapon::Knife || wid == Weapon::C4 || wid == Weapon::Explosive || wid == Weapon::Flashbang ||
        wid == Weapon::Smoke) {
      continue;
    }
    if (has_flag (pev->weapons, ystl::bit (wid)) && ammo_in_clip_[wid] > 0) {
      return true;
    }
  }
  return false;
}

bool Bot::HasAnyAmmo () {
  if (!HasAnyWeapons ()) {
    return false;
  }
  const auto pri = GetBestOwnedWeaponIndex ();
  const auto sec = GetBestOwnedPistolIndex ();
  const auto &tab = conf.GetWeapons ();

  return (pri > 0 && GetAmmo (tab[pri].id) > 0) || (sec > 0 && GetAmmo (tab[sec].id) > 0);
}

void Bot::SelectBestWeapon () {
  // choose the best owned weapon and switch to it

  // if knife mode activated, force bot to use knife
  if (IsKnifeMode ()) {
    SelectWeaponById (Weapon::Knife);
    return;
  }

  if (reload_data_.is_reloading) {
    return;
  }
  const auto &tab = conf.GetWeapons ();

  int select_index = 0;
  int chosen_weapon_index = 0;

  // loop through all the weapons
  for (select_index = 0; select_index < tab.size<int32_t> (); ++select_index) {

    // is the bot not carrying this weapon?
    if (!has_flag (pev->weapons, ystl::bit (tab[select_index].id))) {
      continue;
    }

    const auto id = tab[select_index].id;
    bool ammo_left = false;

    // is the bot already holding this weapon and there is still ammo in clip?
    if (tab[select_index].id == current_weapon_ && (GetAmmoInClip () < 0 || GetAmmoInClip () >= tab[select_index].min_primary_ammo)) {
      ammo_left = true;
    }

    // is no ammo required for this weapon or enough ammo available to fire
    if (GetAmmo (id) >= tab[select_index].min_primary_ammo) {
      ammo_left = true;
    }

    // skip empty magazines while an enemy is near another loaded gun
    if (ammo_left && ammo_in_clip_[id] == 0 && has_flag (states_, Sense::SeeingEnemy | Sense::HearingEnemy) &&
        HasAnotherWeaponWithAmmoInClip (id)) {
      ammo_left = false;
    }

    if (ammo_left) {
      chosen_weapon_index = select_index;
    }
  }

  chosen_weapon_index %= kNumWeapons;
  select_index = chosen_weapon_index;

  const auto id = tab[select_index].id;

  // select this weapon if it isn't already selected
  if (current_weapon_ != id) {
    SelectWeaponById (tab[select_index].id);
  }
  reload_data_.Clear ();
}

void Bot::DrawKnifeForJump (float jump_distance_sq, float height_diff) {
  // knife gives extra running speed, so it's drawn before long jump links
  if (jump_knife_drawn_ || UsesKnife () || UsesPistol () || current_weapon_ == Weapon::Scout || current_weapon_ == Weapon::Explosive ||
      reload_data_.is_reloading || has_flag (states_, Sense::SeeingEnemy)) {
    return;
  }
  if (jump_distance_sq > ystl::sqrf (145.0f) || (height_diff > 32.0f && jump_distance_sq > ystl::sqrf (125.0f))) {
    SelectWeaponById (Weapon::Knife);
    jump_knife_drawn_ = true;

    // protect the run-up, restore becomes eligible only after the jump executes
    jump_knife_restore_timer_.start (kInfiniteDistance);
  }
}

void Bot::RestoreAfterJump () {
  // return to the best weapon if the knife in hands was drawn by navigation for the jump
  if (!jump_knife_drawn_ || !UsesKnife ()) {
    return;
  }
  if (!IsOnFloor () || !jump_knife_restore_timer_.elapsed ()) {
    return;
  }
  jump_knife_drawn_ = false;

  if (!IsKnifeMode () && HasAnyAmmo () && GetTaskId () != TaskId::EscapeFromBomb) {
    SelectBestWeapon ();
  }
}

void Bot::SelectSecondary () {
  const int old_weapons = pev->weapons;

  pev->weapons &= ~kPrimaryWeaponMask;
  SelectBestWeapon ();

  pev->weapons = old_weapons;
}

int Bot::GetBestOwnedWeaponIndex () const {
  // returns best owned weapon config index (0-25), not weapon id

  const auto &tab = conf.GetWeapons ();
  int idx = 0;

  // loop through all the weapons
  for (int i = 0; i < tab.size<int32_t> (); ++i) {
    // is the bot carrying this weapon?
    if (has_flag (pev->weapons, ystl::bit (tab[i].id))) {
      idx = i;
    }
  }
  return idx;
}

int Bot::GetBestOwnedPistolIndex () const {
  // returns best owned pistol config index (0-25), not weapon id

  const auto &tab = conf.GetWeapons ();
  int idx = 0;

  // loop through all the weapons
  for (int i = 0; i < tab.size<int32_t> () && i < kPrimaryWeaponMinIndex; ++i) {
    // is the bot carrying this weapon?
    if (has_flag (pev->weapons, ystl::bit (tab[i].id))) {
      idx = i;
    }
  }
  return idx;
}

void Bot::CheckReload () {
  // check the reload state
  const auto tid = GetTaskId ();

  // we're should not reload, while doing next tasks
  const bool uninterruptible_task = (tid == TaskId::PlantBomb || tid == TaskId::DefuseBomb || tid == TaskId::PickupItem ||
                                     tid == TaskId::ThrowExplosive || tid == TaskId::ThrowFlashbang || tid == TaskId::ThrowSmoke);

  // do not check for reload
  if (uninterruptible_task || is_using_grenade_ || UsesKnife ()) {
    reload_data_.state = Reload::None;
    return;
  }

  reload_data_.check_timer.start (3.0f);

  if (reload_data_.is_reloading) {
    const auto max_clip = conf.FindWeaponById (current_weapon_).max_clip;

    if (GetAmmoInClip () >= max_clip || GetAmmo () <= 0) {
      reload_data_.is_reloading = false;
    }
  }

  // refill a dry primary from reserve instead of staying secondary
  if (reload_data_.state == Reload::None && !has_flag (states_, Sense::SeeingEnemy) && HasPrimaryWeapon ()) {
    const auto pid = conf.GetWeapons ()[GetBestOwnedWeaponIndex ()].id;

    if (ammo_in_clip_[pid] <= 0 && GetAmmo (pid) > 0) {
      reload_data_.state = Reload::Primary;
    }
  }

  bool sequence_processed = false;

  while (reload_data_.state != Reload::None) {
    sequence_processed = true;

    auto wid = Weapon::Invalid;
    auto weapons = pev->weapons;

    if (reload_data_.state == Reload::Primary) {
      weapons &= kPrimaryWeaponMask;
    }
    else if (reload_data_.state == Reload::Secondary) {
      weapons &= kSecondaryWeaponMask;
    }

    if (weapons == 0) {
      // no weapon of this kind owned, proceed to the next reload stage
      ++reload_data_.state;

      if (reload_data_.state > Reload::Secondary) {
        reload_data_.state = Reload::None;
      }
      continue;
    }

    for (int i = 1; i < kMaxWeapons; ++i) {
      if (weapons & ystl::bit (i)) {
        wid = static_cast<Weapon> (i);
        break;
      }
    }
    const auto &prop = conf.GetWeaponProp (wid);

    if (IsLowOnAmmo (prop.id, 0.75f) && GetAmmo (prop.id) > 0) {
      // skip reload while an enemy is visible and another gun is loaded
      if (has_flag (states_, Sense::SeeingEnemy) && HasAnotherWeaponWithAmmoInClip (prop.id)) {
        reload_data_.state = Reload::None;
        reload_data_.is_reloading = false;
        break;
      }

      if (current_weapon_ != prop.id) {
        SelectWeaponById (prop.id);
      }
      pev->button &= ~IN_ATTACK;

      if (!(old_buttons_ & IN_RELOAD)) {
        pev->button |= IN_RELOAD; // press reload button
      }
      reload_data_.is_reloading = true;
      return;
    }

    // this stage has no reserve ammo, so advance to the next stage
    if (GetAmmo (prop.id) <= 0) {
      ++reload_data_.state;

      if (reload_data_.state > Reload::Secondary) {
        reload_data_.state = Reload::None;
      }
      continue;
    }

    // the weapon is already loaded enough - try to refresh the next one, but not while an enemy is around
    if (has_flag (states_, Sense::SeeingEnemy | Sense::HearingEnemy) || see_enemy_timer_.less_than (5.0f)) {
      reload_data_.state = Reload::None;
      break;
    }
    ++reload_data_.state;

    if (reload_data_.state > Reload::Secondary) {
      reload_data_.state = Reload::None;
    }
  }

  // reload ends on the last stage gun, so restore the best weapon
  if (sequence_processed && reload_data_.state == Reload::None && !reload_data_.is_reloading) {
    SelectBestWeapon ();
  }
}

int Bot::GetAmmo () const {
  return GetAmmo (current_weapon_);
}

int Bot::GetAmmo (Weapon id) const {
  const auto &prop = conf.GetWeaponProp (id);

  if (prop.ammo1 == -1 || prop.ammo1 > kMaxWeapons - 1) {
    return -1;
  }
  return ammo_[prop.ammo1];
}

void Bot::SelectWeaponByIndex (int index) {
  const auto &tab = conf.GetWeapons ();
  IssueCommand (tab[index].name.chars ());
}

void Bot::SelectWeaponById (Weapon id) {
  const auto &prop = conf.GetWeaponProp (id);
  IssueCommand (prop.classname.chars ());
}

void Bot::CheckBurstMode (float distance) {
  // this function checks burst mode, and switch it depending distance to to enemy

  if (HasShield ()) {
    return; // no checking when shield is active
  }

  // if current weapon is glock, disable burstmode on long distances, enable it else
  if (current_weapon_ == Weapon::Glock18 && distance < 300.0f && weapon_burst_mode_ == BurstMode::Off) {
    pev->button |= IN_ATTACK2;
  }
  else if (current_weapon_ == Weapon::Glock18 && distance >= 300.0f && weapon_burst_mode_ == BurstMode::On) {
    pev->button |= IN_ATTACK2;
  }

  // if current weapon is famas, disable burstmode on short distances, enable it else
  if (current_weapon_ == Weapon::Famas && distance > 400.0f && weapon_burst_mode_ == BurstMode::Off) {
    pev->button |= IN_ATTACK2;
  }
  else if (current_weapon_ == Weapon::Famas && distance <= 400.0f && weapon_burst_mode_ == BurstMode::On) {
    pev->button |= IN_ATTACK2;
  }
}

void Bot::CheckSilencer () {
  if ((current_weapon_ == Weapon::USP || current_weapon_ == Weapon::M4A1) && !HasShield () && game.IsNullEntity (enemy_)) {
    const int prob = (personality_ == Personality::Rusher ? 35 : 65);

    // aggressive bots don't like the silencer
    if (rg.chance (current_weapon_ == Weapon::USP ? prob / 2 : prob)) {
      // is the silencer not attached
      if (pev->weaponanim > 6) {
        pev->button |= IN_ATTACK2; // attach the silencer
      }
    }
    else {

      // is the silencer attached
      if (pev->weaponanim <= 6) {
        pev->button |= IN_ATTACK2; // detach the silencer
      }
    }
  }
}

void Bot::UpdatePickups () {
  // this function finds items to collect or use in the near of a bot

  // we're not allowed to run now
  if (IsPickupBlocked ()) {
    pickup_item_ = nullptr;
    pickup_type_ = Pickup::None;
    return;
  }

  const auto &interesting = game_state.GetInterestingEntities ();
  const float radius_sq = ystl::sqrf (cv_object_pickup_radius.As<float> ());

  // validate existing pickup if we have one
  if (ValidateExistingPickup (interesting, radius_sq)) {
    return;
  }

  // clear previous pickup state
  pickup_item_ = nullptr;
  pickup_type_ = Pickup::None;

  // iterate through interesting entities to find best pickup
  for (const auto &item : interesting) {
    const auto ent = item.ent;
    const ystl::Vector origin = game.GetEntityOrigin (ent);
    const bool is_bomb = game.IsBombEntity (ent);

    // skip invalid or ignored items
    if ((ent->v.effects & EF_NODRAW) || IsIgnoredItem (ent) || ystl::abs (origin.z - pev->origin.z) > (is_bomb ? 160.0f : 96.0f)) {
      continue;
    }

    // too far from us?
    if (pev->origin.distance_sq (origin) > radius_sq) {
      continue;
    }

    // check if line of sight to object is not blocked (i.e. visible)
    if (!(is_bomb ? SeesC4 (origin) : SeesItem (origin, ent->v.classname.str ()))) {
      continue;
    }

    // classify and validate pickup type
    auto [allowPickup, pickupType] = ClassifyPickupType (ent);

    if (!allowPickup) {
      continue;
    }

    // apply additional validation based on pickup type
    if (!ValidatePickupByType (ent, pickupType)) {
      continue;
    }

    // handle team-specific pickup logic
    if (!HandleTeamSpecificPickups (ent, origin, pickupType)) {
      continue;
    }

    // found valid pickup, store and exit loop
    pickup_item_ = ent;
    pickup_type_ = pickupType;

    // reset the navigation timer once a pickup target is acquired
    nav_timer_.start ();
    break;
  }

  // finalize pickup selection
  FinalizePickup ();
}

bool Bot::IsPickupBlocked () {
  // check if pickup logic is currently not allowed to run

  // zombie or chickens not allowed to pickup anything
  if (is_creature_) {
    return true;
  }

  // seeing enemy now, not good time to pickup anything
  if (has_flag (states_, Sense::SeeingEnemy)) {
    return true;
  }

  // bots on ladder don't have to search anything
  if (IsOnLadder ()) {
    return true;
  }

  // we're escaping from the bomb, don't bother!
  if (GetTaskId () == TaskId::EscapeFromBomb) {
    return true;
  }

  // knife mode is in progress?
  if (cv_jasonmode) {
    return true;
  }

  // no interesting entities, how?
  if (!game_state.HasInterestingEntities ()) {
    return true;
  }
  return false;
}

bool Bot::ValidateExistingPickup (const ystl::Array<InterestingEntity> &interesting, float radius_sq) {
  // check if current pickup item is still valid and reachable

  if (game.IsNullEntity (pickup_item_)) {
    return false;
  }
  auto pickup_item = pickup_item_;

  for (const auto &item : interesting) {
    const auto ent = item.ent;

    // in the periods of updating interesting entities we can get fake ones, that already were picked up
    if ((ent->v.effects & EF_NODRAW) || game.IsPlayerEntity (ent->v.owner)) {
      continue;
    }
    const ystl::Vector origin = game.GetEntityOrigin (ent);

    // too far from us?
    if (pev->origin.distance_sq (origin) > radius_sq) {
      continue;
    }

    if (ent == pickup_item && (game.IsBombEntity (ent) || SeesItem (origin, ent->v.classname.str ()))) {
      // item still exists and is visible
      return true;
    }
  }

  // item no longer valid, clear it
  pickup_item_ = nullptr;
  pickup_type_ = Pickup::None;

  return false;
}

ystl::Twin<bool, Pickup> Bot::ClassifyPickupType (edict_t *ent) {
  // determine what type of pickup an entity represents

  auto classname = ent->v.classname.str ();
  auto model = ent->v.model.str (9);

  const bool is_weapon_box = classname.starts_with ("weaponbox");
  const bool is_demolition_map = game.MapIs (MapFlags::Demolition);
  const bool is_hostage_rescue_map = game.MapIs (MapFlags::HostageRescue);
  const bool is_csdm = game.Is (GameFlags::CSDM);

  // hostages on rescue maps
  if (is_hostage_rescue_map && game.IsHostageEntity (ent)) {
    return { true, Pickup::Hostage };
  }

  // dropped c4 on demolition maps
  if (is_demolition_map && is_weapon_box && model == "backpack.mdl" && !cv_ignore_objectives) {
    return { true, Pickup::DroppedC4 };
  }

  // weapons, armoury entities, csdm weapons
  if ((is_weapon_box || classname.starts_with ("armoury_entity") || (is_csdm && classname.starts_with ("csdm"))) && !is_using_grenade_) {
    auto pickup_type = Pickup::Weapon;

    if (cv_pickup_ammo_and_kits) {
      pickup_type = CheckAmmoAndKitsPickup (model);
    }

    // weapon replacement is not allowed
    if (!cv_pickup_best) {
      return { false, Pickup::None };
    }
    return { true, pickup_type };
  }

  // shield pickup
  if (classname.starts_with ("weapon_shield") && !is_using_grenade_) {
    if (!cv_pickup_best) {
      return { false, Pickup::None };
    }
    return { true, Pickup::Shield };
  }

  // defusal kit for ct on demolition
  if (is_demolition_map && team_ == Team::CT && !has_defuser_ && classname.starts_with ("item_thighpack")) {
    return { true, Pickup::DefusalKit };
  }

  // planted c4
  if (is_demolition_map && classname.starts_with ("grenade") && conf.GetBombModelName () == model) {
    return { true, Pickup::PlantedC4 };
  }

  // custom items
  if (cv_pickup_custom_items && game.IsItemEntity (ent) && !classname.starts_with ("item_thighpack")) {
    return { true, Pickup::Items };
  }
  return { false, Pickup::None };
}

Pickup Bot::CheckAmmoAndKitsPickup (ystl::StringRef model) {
  // check if ammo/kits should be picked up based on current loadout

  const int primary_weapon_carried = GetBestOwnedWeaponIndex ();
  const int secondary_weapon_carried = GetBestOwnedPistolIndex ();

  const auto &tab = conf.GetWeapons ();
  const auto &primary = tab[primary_weapon_carried];
  const auto &secondary = tab[secondary_weapon_carried];

  const auto &primary_prop = conf.GetWeaponProp (primary.id);
  const auto &secondary_prop = conf.GetWeaponProp (secondary.id);

  // check secondary ammo pickup
  if (secondary_weapon_carried < kPrimaryWeaponMinIndex &&
      (static_cast<float> (GetAmmo (secondary.id)) > 0.3f * static_cast<float> (secondary_prop.ammo1_max)) && model == "357ammobox.mdl") {
    return Pickup::Weapon; // don't change type, skip pickup
  }

  // check primary ammo types
  if (!is_vip_ && primary_weapon_carried >= kPrimaryWeaponMinIndex &&
      (static_cast<float> (GetAmmo (primary.id)) > 0.3f * static_cast<float> (primary_prop.ammo1_max)) && !is_using_grenade_ && !HasShield ()) {

    auto weapon_type = conf.GetWeaponType (primary.id);

    const bool is_sniper_rifle = weapon_type == WeaponType::Sniper;
    const bool is_submachine = weapon_type == WeaponType::SMG;
    const bool is_shotgun = weapon_type == WeaponType::Shotgun;
    const bool is_rifle = weapon_type == WeaponType::Rifle || weapon_type == WeaponType::ZoomRifle;
    const bool is_heavy = weapon_type == WeaponType::Heavy;

    if ((!is_rifle && model == "9mmarclip.mdl") || (!is_shotgun && model == "shotbox.mdl") || (!is_submachine && model == "9mmclip.mdl") ||
        (!is_sniper_rifle && model == "crossbow_clip.mdl") || (!is_heavy && model == "chainammo.mdl")) {
      return Pickup::Weapon; // don't change type, skip pickup
    }
  }

  // check health and armor pickups
  if (health_value_ >= 100.0f && model == "medkit.mdl") {
    return Pickup::Weapon;
  }

  if (pev->armorvalue >= 100.0f && (model == "kevlar.mdl" || model == "battery.mdl" || model == "assault.mdl")) {
    return Pickup::Weapon;
  }

  // check grenade pickups
  if (has_flag (pev->weapons, ystl::bit (Weapon::Flashbang)) && model == kFlashbangModelName) {
    return Pickup::Weapon;
  }
  if (has_flag (pev->weapons, ystl::bit (Weapon::Explosive)) && model == kExplosiveModelName) {
    return Pickup::Weapon;
  }
  if (has_flag (pev->weapons, ystl::bit (Weapon::Smoke)) && model == kSmokeModelName) {
    return Pickup::Weapon;
  }

  // all checks passed, this is an ammo/kits pickup
  return Pickup::AmmoAndKits;
}

bool Bot::ValidatePickupByType (edict_t *ent, Pickup pickup_type) {
  // apply additional validation based on pickup type

  // weapon or ammo/kits pickup
  if (pickup_type == Pickup::Weapon || pickup_type == Pickup::AmmoAndKits) {
    if (is_vip_) {
      return false;
    }

    if (!no_ammo_pickup_timer_.elapsed ()) {
      return false; // cooldown after ammo-motivated pickup to prevent repick loops
    }

    if (!RateGroundWeapon (ent)) {
      // double check if it's ammo/kits by verifying model is not a weapon
      if (pickup_type == Pickup::AmmoAndKits) {
        auto model = ent->v.model.str (9);
        const auto &tab = conf.GetWeapons ();

        for (const auto &rw : tab) {
          if (rw.model == model) {
            return false; // it's a weapon, not ammo
          }
        }
        return true; // not found in weapon list, allow ammo pickup
      }
      return false;
    }

    if (!HasAnyAmmoInClip () && !HasAnyAmmo ()) {
      no_ammo_pickup_timer_.start (15.0f); // pickup was ammo-motivated, set cooldown
    }
    return true;
  }

  // shield pickup
  if (pickup_type == Pickup::Shield) {
    if (has_flag (pev->weapons, ystl::bit (Weapon::Elite)) || HasShield () || is_vip_) {
      return false;
    }

    if (HasPrimaryWeapon () && !RateGroundWeapon (ent)) {
      return false;
    }
    return true;
  }
  return true;
}

bool Bot::HandleTeamSpecificPickups (edict_t *ent, const ystl::Vector &origin, Pickup pickup_type) {
  // handle team-specific pickup logic and side effects

  // terrorist team specific
  if (team_ == Team::Terrorist) {
    if (pickup_type == Pickup::DroppedC4) {
      dest_origin_ = origin; // ensure we reached dropped bomb

      PushRadioChat (RadioChat::FoundC4);
      ClearSearchNodes ();

      return true;
    }

    if (pickup_type == Pickup::Hostage) {
      return HandleTerroristHostagePickup (ent, origin);
    }

    if (pickup_type == Pickup::PlantedC4) {
      return HandleTerroristBombPickup (origin);
    }
  }

  // counter-terrorist team specific
  if (team_ == Team::CT) {
    if (pickup_type == Pickup::Hostage) {
      return HandleCtHostagePickup (ent);
    }

    if (pickup_type == Pickup::PlantedC4) {
      return HandleCtBombPickup (origin);
    }

    if (pickup_type == Pickup::DroppedC4) {
      return HandleCtDroppedC4 (ent, origin);
    }
  }
  return true;
}

bool Bot::HandleTerroristHostagePickup (edict_t *ent, const ystl::Vector &origin) {
  // handle hostage pickup logic for terrorist team

  ignored_items_.push (ent);

  if (!defend_hostage_ && personality_ != Personality::Rusher && rg.chance (15 * Skill () / 50) && time_camping_ + 15.0f < game.Time () &&
      NumFriendsNear (pev->origin, 384.0f) < 3) {

    const int index = FindDefendNode (origin);

    StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex,
      game.Time () + rg (cv_camping_time_min.As<float> (), cv_camping_time_max.As<float> ()), true);
    StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + rg (3.0f, 6.0f), true);

    SelectCampButtons (index);
    defend_hostage_ = true;

    PushRadioChat (RadioChat::GoingToGuardHostages);
  }
  return false; // don't pickup hostage as terrorist
}

bool Bot::HandleTerroristBombPickup (const ystl::Vector &origin) {
  // handle planted c4 pickup logic for terrorist team

  if (!defended_bomb_) {
    defended_bomb_ = true;

    const int index = FindDefendNode (origin);
    const auto &path = graph[index];

    const float bomb_timer = mp_c4timer.As<float> ();
    const float time_mid_blowup = game_state.GetTimeBombPlanted () + (bomb_timer * 0.5f + bomb_timer * 0.25f) -
                                  graph.CalculateTravelTime (pev->maxspeed, pev->origin, path.origin);

    if (time_mid_blowup > game.Time ()) {
      ClearTask (TaskId::MoveTo);
      StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, time_mid_blowup, true);
      StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, time_mid_blowup, true);

      SelectCampButtons (index);

      if (rg.chance (85) && NumEnemiesNear (pev->origin, 768.0f) < 4) {
        PushRadioChat (RadioChat::DefendingBombsite);
      }
    }
    else {
      PushRadioChat (RadioChat::ShesGonnaBlow);
    }
  }
  return false; // don't pickup planted c4 as terrorist
}

bool Bot::HandleCtHostagePickup (edict_t *ent) {
  // handle hostage pickup logic for ct team

  if (game.IsNullEntity (ent) || ent->v.health <= 0) {
    return false; // never pickup dead hostage
  }

  // check if other bots already have this hostage
  for (const auto &other : bots) {
    if (other.is_alive_) {
      for (const auto &hostage : other.hostages_) {
        if (hostage == ent) {
          return false;
        }
      }
    }
  }

  // don't steal hostage from human teammate (hack)
  for (const auto &client : clients) {
    if (client.IsUsedAndAlive () && client.IsHuman () && client.team == team_ && client.IsInRadius (ent->v.origin, ystl::sqrf (240.0f))) {
      return false;
    }
  }
  return true;
}

bool Bot::HandleCtBombPickup (const ystl::Vector &origin) {
  // handle planted c4 pickup logic for ct team

  if (game.IsAliveEntity (enemy_)) {
    return false;
  }

  if (IsOutOfBombTimer ()) {
    CompleteTask ();
    StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);

    return false;
  }

  if (rg.chance (70)) {
    PushRadioChat (RadioChat::FoundC4Plant);
  }
  bool allow_pickup = !IsBombDefusing (origin) || has_progress_bar_;

  if (!defended_bomb_ && !allow_pickup) {
    defended_bomb_ = true;

    const int index = FindDefendNode (origin);
    const auto &path = graph[index];

    const float time_to_explode =
      game_state.GetTimeBombPlanted () + mp_c4timer.As<float> () - graph.CalculateTravelTime (pev->maxspeed, pev->origin, path.origin);

    ClearTask (TaskId::MoveTo);
    StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, time_to_explode, true);
    StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, time_to_explode, true);

    SelectCampButtons (index);

    if (rg.chance (85)) {
      PushRadioChat (RadioChat::DefendingBombsite);
    }
  }
  return allow_pickup;
}

bool Bot::HandleCtDroppedC4 (edict_t *ent, const ystl::Vector &origin) {
  // handle dropped c4 pickup logic for ct team

  ignored_items_.push (ent);

  if (!defended_bomb_ && rg.chance (25 + Skill () / 2) && health_value_ < 60) {
    const int index = FindDefendNode (origin);

    StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex,
      game.Time () + rg (cv_camping_time_min.As<float> (), cv_camping_time_max.As<float> ()), true);
    StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + rg (10.0f, 30.0f), true);

    SelectCampButtons (index);
    defended_bomb_ = true;

    PushRadioChat (RadioChat::GoingToGuardDroppedC4);
  }
  return false; // don't pickup dropped c4 as ct
}

void Bot::FinalizePickup () {
  // finalize pickup selection by checking conflicts and reachability

  if (game.IsNullEntity (pickup_item_)) {
    return;
  }

  // check if another bot is already picking up this item
  for (const auto &other : bots) {
    if (&other != this && other.is_alive_ && other.pickup_item_ == pickup_item_) {
      pickup_item_ = nullptr;
      pickup_type_ = Pickup::None;
      return;
    }
  }

  const ystl::Vector pickup_pos = game.GetEntityOrigin (pickup_item_);

  float high_offset = 20.0f;

  if (pickup_type_ == Pickup::Hostage) {
    high_offset = 50.0f;
  }
  else if (pickup_type_ == Pickup::PlantedC4 || pickup_type_ == Pickup::DroppedC4) {
    high_offset = 72.0f; // bomb can rest on top of a prop, bot defuses near it, not on it
  }
  else if (pickup_type_ == Pickup::Weapon || pickup_type_ == Pickup::AmmoAndKits) {
    high_offset = 48.0f;
  }

  // check if item is too high to reach, or if getting the item would hurt bot
  if (pickup_pos.z > GetEyesPos ().z + high_offset || IsDeadlyMove (pickup_pos)) {
    ignored_items_.push (pickup_item_);
    pickup_item_ = nullptr;
    pickup_type_ = Pickup::None;

    return;
  }
}

void Bot::EnsurePickupEntitiesClear () {
  // called when bot appears stuck - clears pickup state to prevent being stuck indefinitely

  const auto tid = GetTaskId ();

  // only clear pickup state when a pickup was actually pursued
  if (tid != TaskId::PickupItem && !has_flag (states_, Sense::PickupItem) && game.IsNullEntity (pickup_item_)) {
    return;
  }

  // don't ignore the item while planting/defusing (progress bar is active)
  if (!game.IsNullEntity (pickup_item_) && !has_progress_bar_) {
    ignored_items_.push (pickup_item_); // ignore this item, bot is stuck getting to it

    // keep the ignore list bounded, so the bot doesn't permanently lose interest in items
    if (ignored_items_.size () > 64) {
      ignored_items_.shift ();
    }
  }

  item_check_timer_.start (5.0f);
  pickup_type_ = Pickup::None;
  pickup_item_ = nullptr;

  if (tid == TaskId::PickupItem) {
    CompleteTask ();
  }

  // clear pickup state flag
  states_ &= ~Sense::PickupItem;

  // only re-route navigation when we actually aborted a pickup task; other tasks manage their own goal
  if (tid == TaskId::PickupItem) {
    FindValidNode ();
  }
}

bool Bot::IsLineBlockedBySmoke (const ystl::Vector &from, const ystl::Vector &to) {
  if (!game_state.HasActiveGrenades ()) {
    return false;
  }

  // distance along line of sight covered by smoke
  float total_smoked_length = 0.0f;

  ystl::Vector sight_dir = to - from;
  const float sight_length = sight_dir.normalize_in_place ();

  for (const auto &grenade : game_state.GetActiveGrenades ()) {
    if (grenade.kind != GrenadeKind::Smoke) {
      continue;
    }
    const auto pent = grenade.ent;

    if (game.IsNullEntity (pent)) {
      continue;
    }

    // check if sgtracked
    if (!sgtrack.Has (pent)) {
      continue;
    }

    // need drawn models
    if (pent->v.effects & EF_NODRAW) {
      continue;
    }

    // smoke must be on a ground
    if (!(pent->v.flags & FL_ONGROUND)) {
      continue;
    }

    const float smoke_radius_sq = ystl::sqrf (cv_smoke_grenade_radius.As<float> ());
    const auto &smoke_origin = sgtrack.Find (pent);

    ystl::Vector to_grenade = smoke_origin - from;
    float along_dist = to_grenade | sight_dir;

    // compute closest point to grenade along line of sight ray
    ystl::Vector close {};

    // constrain closest point to line segment
    if (along_dist < 0.0f) {
      close = from;
    }
    else if (along_dist >= sight_length) {
      close = to;
    }
    else {
      close = from + sight_dir * along_dist;
    }

    // if closest point is within smoke radius, the line overlaps the smoke cloud
    ystl::Vector to_close = close - smoke_origin;
    float length_sq = to_close.length_sq ();

    if (length_sq < smoke_radius_sq) {
      // some portion of the ray intersects the cloud

      const float from_sq = to_grenade.length_sq ();
      const float to_sq = (smoke_origin - to).length_sq ();

      if (from_sq < smoke_radius_sq) {
        if (to_sq < smoke_radius_sq) {
          // both 'from' and 'to' lie within the cloud entire length is smoked
          total_smoked_length += (to - from).length ();
        }
        else {
          // from is inside the cloud, to is outside, add half smoked length
          float half_smoked_length = ystl::sqrtf (smoke_radius_sq - length_sq);

          if (along_dist > 0.0f) {
            // ray goes thru 'close'
            total_smoked_length += half_smoked_length + (close - from).length ();
          }
          else {
            // ray starts after 'close'
            total_smoked_length += half_smoked_length - (close - from).length ();
          }
        }
      }
      else if (to_sq < smoke_radius_sq) {
        // from is outside the cloud, to is inside, add half smoked length
        const float half_smoked_length = ystl::sqrtf (smoke_radius_sq - length_sq);
        ystl::Vector v = to - smoke_origin;

        if ((v | sight_dir) > 0.0f) {
          // ray goes thru 'close'
          total_smoked_length += half_smoked_length + (close - to).length ();
        }
        else {
          // ray ends before 'close'
          total_smoked_length += half_smoked_length - (close - to).length ();
        }
      }
      else {
        // both ends are outside the cloud, so the ray fully crosses it
        const float smoked_length = 2.0f * ystl::sqrtf (smoke_radius_sq - length_sq);
        total_smoked_length += smoked_length;
      }
    }
  }

  // define how much smoke a bot can see thru
  const float max_smoked_length = 0.7f * cv_smoke_grenade_radius.As<float> ();

  // return true if the total length of smoke-covered line-of-sight is too much
  return total_smoked_length > max_smoked_length;
}

} // namespace bot
