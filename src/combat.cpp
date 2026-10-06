//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

int Bot::NumFriendsNear (const ystl::Vector &origin, const float radius) const {
  if (game.Is (GameFlags::FreeForAll)) {
    return 0; // no friends on free for all mode
  }

  int count = 0;
  const float radius_sq = ystl::sqrf (radius);

  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }

    if (client.IsInRadius (origin, radius_sq)) {
      count++;
    }
  }
  return count;
}

int Bot::NumEnemiesNear (const ystl::Vector &origin, const float radius) const {
  int count = 0;
  const float radius_sq = ystl::sqrf (radius);
  const bool free_for_all = game.Is (GameFlags::FreeForAll);

  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.ent == Ent ()) {
      continue;
    }

    // on free-for-all every other player is a hostile
    if (!free_for_all && client.IsSameTeam (team_)) {
      continue;
    }

    if (client.IsInRadius (origin, radius_sq)) {
      count++;
    }
  }
  return count;
}

bool Bot::IsEnemyHidden (edict_t *enemy) {
  if (!cv_check_enemy_rendering || game.IsNullEntity (enemy)) {
    return false;
  }
  const auto &v = enemy->v;

  const bool enemy_has_gun = has_flag (v.weapons, kPrimaryWeaponMask) || has_flag (v.weapons, kSecondaryWeaponMask);
  const bool enemy_gunfire = (v.button & IN_ATTACK) || (v.oldbuttons & IN_ATTACK);

  if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && (!enemy_gunfire || !enemy_has_gun)) {
    return true;
  }

  if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && enemy_gunfire && enemy_has_gun) {
    return false;
  }

  if (v.renderfx != kRenderFxHologram && v.renderfx != kRenderFxExplode && v.rendermode != kRenderNormal) {
    if (v.renderfx == kRenderFxGlowShell) {
      if (v.renderamt <= 20.0f && v.rendercolor.x <= 20.0f && v.rendercolor.y <= 20.0f && v.rendercolor.z <= 20.0f) {
        if (!enemy_gunfire || !enemy_has_gun) {
          return true;
        }
        return false;
      }
      else if (!enemy_gunfire && v.renderamt <= 60.0f && v.rendercolor.x <= 60.f && v.rendercolor.y <= 60.0f && v.rendercolor.z <= 60.0f) {
        return true;
      }
    }
    else if (v.renderamt <= 20.0f) {
      if (!enemy_gunfire || !enemy_has_gun) {
        return true;
      }
      return false;
    }
    else if (!enemy_gunfire && v.renderamt <= 60.0f) {
      return true;
    }
  }
  return false;
}

bool Bot::IsEnemyInvincible (edict_t *enemy) {
  if (!cv_check_enemy_invincibility || game.IsNullEntity (enemy)) {
    return false;
  }
  const auto &v = enemy->v;

  if (v.solid < SOLID_BBOX) {
    return true;
  }

  if (v.flags & FL_GODMODE) {
    return true;
  }

  if (ystl::fequal (v.takedamage, DAMAGE_NO)) {
    return true;
  }
  return false;
}

bool Bot::IsEnemyNoTarget (edict_t *enemy) {
  if (game.IsNullEntity (enemy)) {
    return false;
  }
  return !!(enemy->v.flags & FL_NOTARGET);
}

bool Bot::IsEnemyInDarkArea (edict_t *enemy) const {
  if (!cv_check_darkness || game.IsNullEntity (enemy)) {
    return false;
  }
  const auto enemy_node_index = graph.GetNearest (enemy->v.origin);

  if (!graph.Exists (enemy_node_index)) {
    return false;
  }
  const auto light_level = graph[enemy_node_index].light;

  if (light_level > 30.0f) {
    return false;
  }

  if (uses_nvg_ || (enemy->v.effects & EF_DIMLIGHT)) {
    return false;
  }
  const auto sky_color = illum.GetSkyColor ();
  const bool is_very_dark = (light_level < 3.0f && sky_color > 50.0f) || (light_level < 25.0f && sky_color <= 50.0f);

  if (!is_very_dark) {
    return false;
  }

  const bool has_weapon = !!(enemy->v.weapons & (kPrimaryWeaponMask | kSecondaryWeaponMask));
  const bool is_attacking = (enemy->v.button & IN_ATTACK) || (enemy->v.oldbuttons & IN_ATTACK);

  return !(is_attacking && has_weapon);
}

bool Bot::IsEnemyInDarkAreaCached (edict_t *enemy) {
  if (game.IsNullEntity (enemy)) {
    return false;
  }
  if (dark_area_check_timer_.elapsed () || dark_area_check_enemy_ != enemy ||
      dark_area_check_origin_.distance_sq (enemy->v.origin) > ystl::sqrf (64.0f)) {
    dark_area_check_enemy_ = enemy;
    dark_area_check_origin_ = enemy->v.origin;
    dark_area_result_ = IsEnemyInDarkArea (enemy);
    dark_area_check_timer_.start (0.1f);
  }
  return dark_area_result_;
}

bool Bot::CheckBodyParts (edict_t *target) {
  // this function checks visibility of a bot target

  // darkness blocks acquiring a new target, never drops a tracked one
  const bool dark_hidden = target != enemy_ && IsEnemyInDarkAreaCached (target);

  if (IsEnemyHidden (target) || IsEnemyInvincible (target) || IsEnemyNoTarget (target) || dark_hidden) {
    enemy_parts_ = Visibility::None;
    enemy_origin_.clear ();

    return false;
  }

  // hitboxes requested ?
  if (game.Is (GameFlags::HasStudioModels) && cv_use_hitbox_enemy_targeting && hitbox_enumerator_) {
    return CheckBodyPartsWithHitboxes (target);
  }
  return CheckBodyPartsWithOffsets (target);
}

bool Bot::CheckBodyPartsWithOffsets (edict_t *target) {
  Trace::Result result {};
  const ystl::Vector eyes = GetEyesPos ();

  auto spot = target->v.origin;
  auto self = Ent ();

  // creatures can't hurt behind anything
  const auto ignore_flags = is_creature_ ? TraceIgnore::None : (cv_aim_trace_consider_glass ? TraceIgnore::Monsters : TraceIgnore::Everything);

  const auto hits_target = [&] () -> bool {
    if (result.hit == target) {
      return true;
    }
    if (result.fraction >= 1.0f) {
      return trace.IsEndpointClear (result);
    }
    return false;
  };

  enemy_parts_ = Visibility::None;
  trace.Line (eyes, spot, ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Body;
    enemy_origin_ = result.end_pos;
  }

  // check top of head
  spot.z += 25.0f;
  trace.Line (eyes, spot, ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Head;
    enemy_origin_ = result.end_pos;
  }

  if (enemy_parts_ != Visibility::None) {
    return true;
  }

  constexpr auto kStandFeet = 34.0f;
  constexpr auto kCrouchFeet = 14.0f;
  constexpr auto kEdgeOffset = 13.0f;

  if (target->v.flags & FL_DUCKING) {
    spot.z = target->v.origin.z - kCrouchFeet;
  }
  else {
    spot.z = target->v.origin.z - kStandFeet;
  }
  trace.Line (eyes, spot, ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }
  ystl::Vector dir = (target->v.origin - pev->origin).normalize2d ();

  ystl::Vector perp (-dir.y, dir.x, 0.0f);
  spot = target->v.origin + ystl::Vector (perp.x * kEdgeOffset, perp.y * kEdgeOffset, 0);

  trace.Line (eyes, spot, ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }
  spot = target->v.origin - ystl::Vector (perp.x * kEdgeOffset, perp.y * kEdgeOffset, 0);

  trace.Line (eyes, spot, ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }
  return false;
}

bool Bot::CheckBodyPartsWithHitboxes (edict_t *target) {
  const auto self = Ent ();
  const auto refresh = frame_interval_ * 1.5f;

  Trace::Result result {};
  const ystl::Vector eyes = GetEyesPos ();

  const auto hits_target = [&] () -> bool {
    if (result.hit == target) {
      return true;
    }
    if (result.fraction >= 1.0f) {
      return trace.IsEndpointClear (result);
    }
    return false;
  };
  enemy_parts_ = Visibility::None;

  // creatures can't hurt behind anything
  const auto ignore_flags = is_creature_ ? TraceIgnore::None : TraceIgnore::Everything;

  // get the stomach hitbox
  trace.Line (eyes, hitbox_enumerator_->Get (target, PlayerPart::Stomach, refresh), ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Body;
    enemy_origin_ = result.end_pos;
  }

  // get the stomach hitbox
  trace.Line (eyes, hitbox_enumerator_->Get (target, PlayerPart::Head, refresh), ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Head;
    enemy_origin_ = result.end_pos;
  }

  if (enemy_parts_ != Visibility::None) {
    return true;
  }

  // get the left hitbox
  trace.Line (eyes, hitbox_enumerator_->Get (target, PlayerPart::LeftArm, refresh), ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }

  // get the right hitbox
  trace.Line (eyes, hitbox_enumerator_->Get (target, PlayerPart::RightArm, refresh), ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }

  // get the feet spot
  trace.Line (eyes, hitbox_enumerator_->Get (target, PlayerPart::Feet, refresh), ignore_flags, self, &result);

  if (hits_target ()) {
    enemy_parts_ |= Visibility::Other;
    enemy_origin_ = result.end_pos;

    return true;
  }
  return false;
}

bool Bot::SeesEnemy (edict_t *player) {
  auto is_behind_smoke_clouds = [&] (const ystl::Vector &pos) {
    if (cv_smoke_grenade_checks.As<int> () == 2) {
      return IsLineBlockedBySmoke (GetEyesPos (), pos);
    }
    return false;
  };

  if (game.IsNullEntity (player)) {
    return false;
  }
  bool ignore_field_of_view = false;

  if (cv_whose_your_daddy && game.IsPlayerEntity (pev->dmg_inflictor) && game.GetPlayerTeam (pev->dmg_inflictor) != team_) {
    ignore_field_of_view = true;
  }

  if ((ignore_field_of_view || IsInViewCone (player->v.origin)) && frustum.Check (view_frustum_, player) &&
      !is_behind_smoke_clouds (player->v.origin) && CheckBodyParts (player)) {
    return true;
  }
  return false;
}

void Bot::TrackEnemies () {
  if (LookupEnemies ()) {
    states_ |= Sense::SeeingEnemy;
  }
  else {
    states_ &= ~Sense::SeeingEnemy;

    enemy_ = nullptr;
    enemy_body_part_set_ = nullptr;
  }

  // recover from knife, if has anything to fire at enemy
  if (has_flag (states_, Sense::SeeingEnemy) && UsesKnife () && !IsKnifeMode () && HasAnyAmmo () && IsOnFloor ()) {
    auto enemy = enemy_;

    if (!game.IsNullEntity (enemy)) {
      const auto distance_sq = pev->origin.distance_sq (enemy->v.origin);

      if (distance_sq > ystl::sqrf (240.f)) {
        SelectBestWeapon ();
      }
    }
  }
}

bool Bot::LookupEnemies () {
  // this function tries to find the best suitable enemy for the bot

  enemy_parts_ = Visibility::None;
  enemy_origin_.clear ();

  // do not search for enemies while we're blinded, or shooting disabled by user
  if (!enemy_ignore_timer_.elapsed () || !blind_timer_.elapsed () || cv_ignore_enemies) {
    return false;
  }
  edict_t *player, *new_enemy = nullptr;
  float nearest_distance_sq = ystl::sqrf (view_distance_);

  // clear suspected flag
  if (!game.IsNullEntity (enemy_) && has_flag (states_, Sense::SeeingEnemy)) {
    states_ &= ~Sense::SuspectEnemy;
  }
  else if (game.IsNullEntity (enemy_) && see_enemy_timer_.less_than (4.0f) && game.IsAliveEntity (last_enemy_)) {
    states_ |= Sense::SuspectEnemy;

    const bool deny_last_enemy = pev->velocity.length_sq2d () > 0.0f && last_enemy_origin_.distance_sq (pev->origin) < ystl::sqrf (256.0f) &&
                                 shoot_time_ + 1.5f > game.Time ();

    if (!has_flag (aim_flags_, AimFlags::Enemy | AimFlags::PredictPath | AimFlags::Danger) && !deny_last_enemy &&
        SeesEntity (last_enemy_origin_, true)) {

      aim_flags_ |= AimFlags::LastEnemy;
    }
  }

  if (!game.IsNullEntity (enemy_)) {
    player = enemy_;

    // is player is alive
    if (!enemy_update_timer_.elapsed () && player->v.origin.distance_sq (pev->origin) < nearest_distance_sq && game.IsAliveEntity (player) &&
        SeesEnemy (player)) {

      new_enemy = player;
      enemy_update_timer_.start (UsesKnife () ? 1.25f : 0.85f);
    }
  }

  // the old enemy is no longer visible or
  if (game.IsNullEntity (new_enemy)) {
    uint8_t *set = nullptr;

    // setup potential visibility set from engine
    if (cv_use_engine_pvs_check) {
      set = game.GetVisibilitySet (this, true);
    }

    // ignore shielded enemies, while we have real one
    edict_t *shield_enemy = nullptr;

    if (cv_attack_monsters) {
      // search the world for monsters
      for (const auto &interesting : game_state.GetInterestingEntities ()) {
        if (interesting.kind != EntityKind::Monster) {
          continue;
        }
        const auto ent = interesting.ent;

        // check the engine pvs
        if (cv_use_engine_pvs_check && !game.CheckVisibility (ent, set)) {
          continue;
        }

        // see if bot can see the monster
        if (SeesEnemy (ent)) {
          // higher priority for big monsters
          const float scale_factor = (1.0f / CalculateScaleFactor (ent));
          const float distance_sq = ent->v.origin.distance_sq (pev->origin) * scale_factor;

          if (distance_sq < nearest_distance_sq) {
            nearest_distance_sq = distance_sq;
            new_enemy = ent;
          }
        }
      }
    }

    // search the world for players
    for (const auto &client : clients) {
      if (!client.IsUsedAndAlive () || client.IsSameTeam (team_)) {
        continue;
      }
      player = client.ent;

      const float distance_sq = player->v.origin.distance_sq (pev->origin);

      // extra skill player can see attacking enemies at extended range
      const bool extended_range = cv_whose_your_daddy && (player->v.button & (IN_ATTACK | IN_ATTACK2)) && view_distance_ < max_view_distance_;

      const float effective_nearest_sq = extended_range ? ystl::sqrf (max_view_distance_) : nearest_distance_sq;

      if (distance_sq >= effective_nearest_sq) {
        continue;
      }

      // check the engine pvs
      if (cv_use_engine_pvs_check && !game.CheckVisibility (player, set)) {
        continue;
      }

      // see if bot can see the player
      if (SeesEnemy (player)) {
        if (IsEnemyBehindShield (player)) {
          shield_enemy = player;
          continue;
        }
        nearest_distance_sq = distance_sq;
        new_enemy = player;

        // aim vip first on as maps
        if (game.MapIs (MapFlags::Assassination) && game.IsPlayerVip (new_enemy)) {
          break;
        }
      }
    }
    enemy_update_timer_.start (UsesKnife () ? 1.25f : 0.85f);

    if (game.IsNullEntity (new_enemy) && !game.IsNullEntity (shield_enemy)) {
      new_enemy = shield_enemy;
    }
  }

  if (new_enemy != nullptr && (game.IsPlayerEntity (new_enemy) || (cv_attack_monsters && game.IsMonsterEntity (new_enemy)))) {
    bots.SetEnemySpotted (true);

    aim_flags_ |= AimFlags::Enemy;
    states_ |= Sense::SeeingEnemy;

    // if enemy is still visible and in field of view, keep it keep track of when we last saw an enemy
    if (new_enemy == enemy_) {
      see_enemy_timer_.start ();

      // zero out reaction time
      actual_reaction_time_ = 0.0f;
      last_enemy_ = new_enemy;
      last_enemy_origin_ = new_enemy->v.origin;

      return true;
    }
    else {
      if (see_enemy_timer_.greater_than (3.0f) && (has_c4_ || has_hostage_ || !game.IsNullEntity (target_entity_))) {
        if (cv_radio_mode.As<int> () == 2) {
          HandleChatterEnemyDown ();
        }
        else if (cv_radio_mode.As<int> () == 1) {
          PushRadioChat (RadioChat::EnemySpotted);
        }
      }
      target_entity_ = nullptr; // stop following when we see an enemy

      enemy_surprise_timer_.start (cv_whose_your_daddy ? actual_reaction_time_ * 0.5f : actual_reaction_time_);

      // zero out reaction time
      actual_reaction_time_ = 0.0f;
      enemy_ = new_enemy;
      last_enemy_ = new_enemy;
      enemy_body_part_set_ = nullptr;
      last_enemy_origin_ = new_enemy->v.origin;
      enemy_reachable_timer_.invalidate ();

      // keep track of when we last saw an enemy
      see_enemy_timer_.start ();

      if (!(old_buttons_ & IN_ATTACK)) {
        return true;
      }

      // alert teammates watching this bot that have no enemy of their own
      for (auto &other : bots) {
        if (!other.is_alive_ || other.team_ != team_ || &other == this) {
          continue;
        }

        if (other.see_enemy_timer_.greater_than (2.0f) && game.IsNullEntity (other.last_enemy_) && util.IsVisible (pev->origin, other.Ent ()) &&
            other.IsInViewCone (pev->origin)) {

          other.last_enemy_ = new_enemy;
          other.last_enemy_origin_ = new_enemy->v.origin;
          other.see_enemy_timer_.start ();
          other.states_ |= (Sense::SuspectEnemy | Sense::HearingEnemy);
          other.aim_flags_ |= AimFlags::LastEnemy;
        }
      }
      return true;
    }
  }
  else if (!game.IsNullEntity (enemy_)) {
    new_enemy = enemy_;
    last_enemy_ = new_enemy;

    if (!game.IsAliveEntity (new_enemy)) {
      enemy_ = nullptr;
      enemy_body_part_set_ = nullptr;

      // shoot at dying players if no new enemy to give some more human-like illusion
      if (see_enemy_timer_.less_than (0.1f)) {
        if (!UsesSniper ()) {
          shoot_at_dead_timer_.start (ystl::clamp (agression_level_ * 1.25f, 0.15f, 0.25f));
          actual_reaction_time_ = 0.0f;
          states_ |= Sense::SuspectEnemy;

          return true;
        }
        return false;
      }

      else if (!shoot_at_dead_timer_.elapsed ()) {
        actual_reaction_time_ = 0.0f;
        states_ |= Sense::SuspectEnemy;

        return true;
      }
      return false;
    }

    // if no enemy visible check if last one shoot able through wall
    if (cv_shoots_thru_walls && IsPenetrableObstacleCached (new_enemy->v.origin)) {
      auto engage_thru_wall = [&] () {
        thru_wall_hold_timer_.start (rg (1.0f, 1.5f));
        see_enemy_timer_.start ();

        states_ |= Sense::SuspectEnemy;
        aim_flags_ |= AimFlags::LastEnemy;

        enemy_ = new_enemy;
        last_enemy_ = new_enemy;
        last_enemy_origin_ = new_enemy->v.origin;

        return true;
      };

      // keep already started engagement without re-rolling
      if (!thru_wall_hold_timer_.elapsed ()) {
        return engage_thru_wall ();
      }

      // roll at most once per backoff interval, so the chance is per-attempt, not per-frame
      if (thru_wall_reroll_timer_.elapsed ()) {
        thru_wall_reroll_timer_.start (rg (0.75f, 1.5f));

        if (GetThruWallChance (difficulty_data_->seen_thru_pct)) {
          return engage_thru_wall ();
        }
      }
    }
  }

  // check if bots should reload
  if ((aim_flags_ <= AimFlags::PredictPath && see_enemy_timer_.greater_than (3.0f) && game.IsNullEntity (last_enemy_) &&
        game.IsNullEntity (enemy_) && GetTaskId () != TaskId::ShootBreakable && GetTaskId () != TaskId::PlantBomb &&
        GetTaskId () != TaskId::DefuseBomb) ||
      game_state.IsRoundOver ()) {

    if (reload_data_.state == Reload::None) {
      reload_data_.state = Reload::Primary;
    }
  }

  // is the bot using a sniper rifle or a zoomable rifle?
  if ((UsesSniper () || UsesZoomableRifle ()) && zoom_check_timer_.remaining_time () < -1.0f) {
    if (pev->fov < 90.0f) {
      pev->button |= IN_ATTACK2;
    }
    else {
      zoom_check_timer_.invalidate ();
    }
  }
  return false;
}

ystl::Vector Bot::GetBodyOffsetError (edict_t *target, float distance) {
  if (game.IsNullEntity (target)) {
    return nullptr;
  }

  if (aim_error_timer_.elapsed ()) {
    // noob (0) -> divisor 640, easy (1) -> 1280, normal (2) -> 2560, etc
    const float hit_error = distance / (ystl::max (0.5f + static_cast<float> (difficulty_), 1.0f) * 1280.0f);
    const auto &maxs = target->v.maxs, &mins = target->v.mins;

    aim_last_error_ = ystl::Vector (rg (mins.x * hit_error, maxs.x * hit_error), rg (mins.y * hit_error, maxs.y * hit_error),
      rg (mins.z * hit_error * 0.5f, maxs.z * hit_error * 0.5f));

    const auto &aim_error = difficulty_data_->aim_error;
    aim_last_error_ += ystl::Vector (rg (-aim_error.x, aim_error.x), rg (-aim_error.y, aim_error.y), rg (-aim_error.z, aim_error.z));

    aim_error_timer_.start (rg (0.4f, 0.8f));
  }
  return aim_last_error_;
}

ystl::Vector Bot::GetEnemyBodyOffset () {
  // the purpose of this function, is to make bot aiming not so ideal

  // without visibility data reuse the last known enemy origin
  if (!enemy_parts_) {
    if (enemy_origin_.empty () && !last_enemy_origin_.empty () && !game.IsNullEntity (last_enemy_) && has_flag (states_, Sense::SuspectEnemy)) {
      return last_enemy_origin_ + GetBodyOffsetError (last_enemy_, last_enemy_origin_.distance (pev->origin));
    }
    return enemy_origin_.empty () ? last_enemy_origin_ : enemy_origin_;
  }
  const float distance = enemy_->v.origin.distance (pev->origin);

  // work on a local copy so we don't corrupt visibility state for other code
  auto vis_parts = enemy_parts_;

  // do not aim at head, at long distance (only if not using sniper weapon)
  if (has_flag (vis_parts, Visibility::Body) && !UsesSniper () && distance > 1000.0f + static_cast<float> (Skill ()) * 10.0f) {
    vis_parts &= ~Visibility::Head;
  }

  // do not aim at head while close enough to enemy and having sniper
  else if (distance < 800.0f && (current_weapon_ != Weapon::Scout) && UsesSniper ()) {
    vis_parts &= ~Visibility::Head;
  }

  ystl::Vector spot = enemy_->v.origin;

  // velocity-based lead prediction for non-sniper non-knife at distance
  ystl::Vector compensation = nullptr;

  if (!UsesSniper () && !UsesKnife () && distance > kSprayDistance) {
    compensation = (enemy_->v.velocity - pev->velocity) * frame_interval_ * 1.8f;
    compensation.z = 0.0f;
  }

  // get the correct head origin
  const auto head_origin = [&] (edict_t *e) -> ystl::Vector {
    return ystl::Vector { e->v.origin.x, e->v.origin.y, e->v.absmin.z + e->v.size.z * 0.81f } + GetCustomHeight (distance);
  };

  if (game.IsPlayerEntity (enemy_)) {
    // now take in account different parts of enemy body
    if (has_flag (vis_parts, Visibility::Head) && has_flag (vis_parts, Visibility::Body)) {
      auto headshot_pct = difficulty_data_->headshot_pct;

      // cache pause state to avoid multiple expensive calls
      const bool should_pause = NeedToPauseFiring (distance);
      const bool recoil_high = IsRecoilHigh ();

      // with too much recoil, shotgun, or when pause is needed, reduce headshot chance
      if (recoil_high || should_pause || (UsesShotgun () && distance > kSprayDistance)) {
        headshot_pct = distance > kSprayDistance ? 6 : ystl::min (headshot_pct, 20);
      }

      // re-roll head or body choice on a slow timer to keep aim steady
      if (enemy_body_part_set_ != enemy_) {
        if (head_roll_timer_.elapsed ()) {
          head_roll_timer_.start (rg (0.5f, 1.0f));
          enemy_body_part_set_ = rg.chance (headshot_pct) ? enemy_ : nullptr;
        }
      }
      else if (recoil_high || should_pause) {
        enemy_body_part_set_ = nullptr; // break head lock for the duration of the spray
      }

      if (enemy_body_part_set_ == enemy_) {
        spot = head_origin (enemy_);

        if (UsesSniper ()) {
          spot.z -= pev->view_ofs.z * 0.15f;
        }
      }
      else {
        spot = enemy_->v.origin;

        // expert aims high on body (toward neck), use enemy's size rather than bot's own view offset
        if (difficulty_ == Difficulty::Expert) {
          spot.z += enemy_->v.size.z * 0.1f;
        }
      }
    }
    else if (has_flag (vis_parts, Visibility::Body)) {
      spot = enemy_->v.origin;
    }
    else if (has_flag (vis_parts, Visibility::Other)) {
      spot = enemy_origin_;
    }
    else if (has_flag (vis_parts, Visibility::Head)) {
      spot = head_origin (enemy_);
    }
  }
  spot += compensation;

  // knife always aims center mass for skilled bots
  if (UsesKnife () && difficulty_ >= Difficulty::Normal) {
    spot = enemy_origin_;
  }

  // add recoil compensation for non-sniper weapons before setting m_lastenemyorigin
  if (!UsesSniper () && !UsesKnife () && distance > kSprayDistance) {
    // pull aim down to counter recoil using smoothed punch angle
    const float target = ystl::tanf (ystl::deg2rad (pev->punchangle.x)) * distance;
    recoil_compensation_ += (target - recoil_compensation_) * ystl::min (1.0f, frame_interval_ * 10.0f);

    spot.z -= recoil_compensation_;
  }
  else {
    recoil_compensation_ = 0.0f;
  }

  last_enemy_origin_ = spot;

  // add some error to unskilled bots
  if (difficulty_ < Difficulty::Normal) {
    spot += GetBodyOffsetError (enemy_, distance);
  }
  return spot;
}

ystl::Vector Bot::GetCustomHeight (float distance) const {
  enum DistanceIndex {
    Long,
    Middle,
    Short
  };

  constexpr float kOffsetRanges[9][3] = {
    { 0.0f, 0.0f,  0.0f  }, // none
    { 0.0f, 0.0f,  0.0f  }, // melee
    { 0.5f, -0.1f, -1.5f }, // pistol
    { 6.5f, 6.0f,  -2.0f }, // shotgun
    { 0.5f, -7.5f, -9.5f }, // zoomrifle
    { 0.5f, -7.5f, -9.5f }, // rifle
    { 0.5f, -7.5f, -9.5f }, // smg
    { 0.0f, -2.5f, -6.0f }, // sniper
    { 1.5f, -4.0f, -9.0f }  // heavy
  };

  // only high-skilled bots do that
  if (difficulty_ != Difficulty::Expert || (enemy_->v.flags & FL_DUCKING)) {
    return 0.0f;
  }

  // default distance index is short
  auto distance_index = DistanceIndex::Short;

  // set distance index appropriate to distance
  if (distance < 2048.0f && distance > kSprayDistanceX2) {
    distance_index = DistanceIndex::Long;
  }
  else if (distance > kSprayDistance && distance <= kSprayDistanceX2) {
    distance_index = DistanceIndex::Middle;
  }
  return { 0.0f, 0.0f, kOffsetRanges[ystl::to_underlying (weapon_type_)][distance_index] };
}

bool Bot::IsFriendInLineOfFire (float distance) const {
  // bot can't hurt teammates if friendly fire is disabled
  if (!mp_friendlyfire || game.Is (GameFlags::CSDM)) {
    return false;
  }

  const ystl::Vector eye_pos = GetEyesPos ();
  const ystl::Vector forward = pev->v_angle.forward ();
  const ystl::Vector trace_end = eye_pos + forward * distance;

  // also check if any teammate is within the firing cone, incl. ones just behind the cover we're firing through
  const float distance_sq = ystl::sqrf (distance + 256.0f);

  // fixed cone (~18 degrees); deriving it from the endpoint on the view ray degenerates to a single line
  constexpr float kFriendlyFireConeDot = 0.95f;

  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }

    const ystl::Vector &friend_origin = client.ent->v.origin;
    const float friend_dist_sq = friend_origin.distance_sq (pev->origin);

    // check if friend is within range and in the firing cone
    if (friend_dist_sq <= distance_sq && util.ViewDot (Ent (), friend_origin) >= kFriendlyFireConeDot) {
      return true;
    }
  }

  // trace line to check for direct hits on teammates
  Trace::Result tr {};
  trace.Line (eye_pos, trace_end, TraceIgnore::None, Ent (), &tr);

  if (game.IsPlayerEntity (tr.hit) && tr.hit != Ent ()) {
    if (game.GetPlayerTeam (tr.hit) == team_ && game.IsAliveEntity (tr.hit)) {
      return true;
    }
  }
  return false;
}

void Bot::FocusEnemy () {
  if (game.IsNullEntity (enemy_)) {
    return;
  }

  // aim for the head and/or body
  look_at_ = GetEnemyBodyOffset ();

  if (!enemy_surprise_timer_.elapsed ()) {
    return;
  }
  const float distance_sq = look_at_.distance_sq2d (GetEyesPos ()); // how far away is the enemy scum?

  const float dot = util.ViewDot (Ent (), enemy_origin_);
  const float enemy_dot = util.ViewDot (enemy_, pev->origin);

  if (distance_sq < ystl::sqrf (128.0f) && !UsesSniper ()) {
    if (UsesKnife ()) {
      if (distance_sq < ystl::sqrf (80.0f)) {
        wants_to_fire_ = true;
      }
      else {
        wants_to_fire_ = false;
      }
    }
    else if (dot > 0.80f) {
      wants_to_fire_ = true;
    }
    else {
      wants_to_fire_ = false;
    }
  }
  else {
    if (dot < 0.90f) {
      wants_to_fire_ = false;
    }
    else {
      if (UsesKnife ()) {
        wants_to_fire_ = true;
      }
      else {
        // enemy faces bot?
        if (enemy_dot >= 0.90f) {
          wants_to_fire_ = true;
        }
        else {
          if (dot > 0.99f) {
            wants_to_fire_ = true;
          }
          else {
            wants_to_fire_ = false;
          }
        }
      }
    }

    // fire anyway at close distance
    if (distance_sq < ystl::sqrf (90.0f)) {
      wants_to_fire_ = true;
    }
  }
}

void Bot::AttackMovement () {
  // no enemy? no need to do strafing
  if (game.IsNullEntity (enemy_)) {
    return;
  }

  // use enemy as dest origin if with knife
  if (UsesKnife () || is_creature_) {
    dest_origin_ = enemy_->v.origin;
  }

  if (last_used_nodes_timer_.elapsed_time () < -frame_interval_) {
    return;
  }

  // use actual distance to enemy, not to aim point (which has error/compensation offset)
  const auto distance_sq = pev->origin.distance_sq (enemy_->v.origin);

  auto approach = 0;

  if (UsesKnife () || is_creature_) {
    approach = 100;
  }
  else if (has_flag (states_, Sense::SuspectEnemy) && !has_flag (states_, Sense::SeeingEnemy)) {
    approach = 49;
  }
  else if (reload_data_.is_reloading || is_vip_ || infected_enemy_team_) {
    approach = 29;
  }
  else {
    approach = static_cast<int> (health_value_ * agression_level_);

    // snipers should hold position, not rush
    if (UsesSniper () && approach > 49) {
      approach = 49;
    }
  }
  const bool is_enemy_cone = IsInViewCone (enemy_->v.origin);

  // only take cover when bomb is not planted and enemy can see the bot or the bot is vip
  if (!game.Is (GameFlags::CSDM) && !IsKnifeMode ()) {
    if (has_flag (states_, Sense::SeeingEnemy) && approach < 30 && !game_state.IsBombPlanted () && cover_search_timer_.elapsed () &&
        (is_enemy_cone || is_vip_ || reload_data_.is_reloading || infected_enemy_team_)) {

      StartTask (TaskId::SeekCover, TaskPri::kSeekCover, kInvalidNodeIndex, 0.0f, true);

      if (!CheckWallOnBehind ()) {
        move_speed_ = -pev->maxspeed;
      }
    }
    else if (approach < 50) {
      move_speed_ = 0.0f;
    }
    else {
      move_speed_ = pev->maxspeed;
    }
  }
  const bool is_full_view = all_flags (enemy_parts_, Visibility::Head | Visibility::Body);

  if (fight_style_check_timer_.elapsed ()) {
    // snipers should stay put when they have a shot lined up
    if (UsesSniper () && !reload_data_.is_reloading && !sniper_stop_timer_.elapsed ()) {
      fight_style_ = Fight::Stay;
    }
    else if (UsesRifle () || UsesSubmachine () || UsesHeavy ()) {
      const auto rand = rg (1, 100);
      const bool enemy_weapon_is_sniper = has_flag (enemy_->v.weapons, kSniperWeaponMask);
      const auto group_of_enemies = distance_sq < ystl::sqrf (768.0f) && IsGroupOfEnemies (enemy_->v.origin, 384.0f);

      if (distance_sq < ystl::sqrf (768.0f)) {
        fight_style_ = Fight::Strafe;
      }
      else if (distance_sq < ystl::sqrf (1024.0f)) {
        if (group_of_enemies || (enemy_weapon_is_sniper && is_enemy_cone)) {
          fight_style_ = Fight::Strafe;
        }
        else if (rand < (UsesSubmachine () ? 50 : 30)) {
          fight_style_ = Fight::Strafe;
        }
        else {
          fight_style_ = Fight::Stay;
        }
      }
      else {
        if (group_of_enemies || (enemy_weapon_is_sniper && is_enemy_cone)) {
          fight_style_ = Fight::Strafe;
        }
        else if (rand < (UsesSubmachine () ? 80 : 90)) {
          fight_style_ = Fight::Stay;
        }
        else {
          fight_style_ = Fight::Strafe;
        }
      }
    }
    else if (UsesKnife ()) {
      fight_style_ = Fight::Strafe;
    }
    else {
      fight_style_ = Fight::Stay;
    }

    // do not try to strafe while ducking
    if ((IsDucking () || IsInNarrowPlace ()) || (!is_full_view && !fire_hurts_friend_)) {
      fight_style_ = Fight::Stay;
    }
    const auto pistol_strafe_distance = game.Is (GameFlags::CSDM) ? kSprayDistanceX2 * 3.0f : kSprayDistanceX2;

    // fire hurts friend value here is from previous frame, but acceptable, and saves us alot of cpu cycles
    if (approach < 30 || fire_hurts_friend_ ||
        ((UsesPistol () || UsesShotgun ()) && distance_sq < ystl::sqrf (pistol_strafe_distance) && is_enemy_cone)) {
      fight_style_ = Fight::Strafe;
    }
    fight_style_check_timer_.start (rg (1.0f, 3.0f));
  }

  if (distance_sq < ystl::sqrf (96.0f) && !UsesKnife ()) {
    move_speed_ = -pev->maxspeed;
  }

  // knife/creature: always strafe when close to dodge, keep running forward when far
  if ((UsesKnife () && is_enemy_cone) || is_creature_) {
    if (distance_sq < ystl::sqrf (100.0f)) {
      fight_style_ = Fight::Strafe;
    }
  }

  if (fight_style_ == Fight::Strafe) {
    auto swap_dodge_direction = [&] () {
      dodge_strafe_dir_ = (dodge_strafe_dir_ == Dodge::Left ? Dodge::Right : Dodge::Left);
    };

    // record strafe start time and origin for stuck detection
    auto commit_strafe = [&] () {
      combat_stuck_check_timer_.start ();
      last_combat_strafe_origin_ = pev->origin;
    };

    auto strafe_update_delay = [] () {
      return ystl::rg (0.3f, 0.8f);
    };

    // to start strafing, we have to first figure out if the target is on the left side or right side
    if (strafe_set_timer_.elapsed ()) {
      const ystl::Vector dir_to_point = (pev->origin - enemy_->v.origin).normalize2d ();
      const ystl::Vector right_side = enemy_->v.v_angle.right ().normalize2d ();

      if ((dir_to_point | right_side) < 0.0f) {
        dodge_strafe_dir_ = Dodge::Right;
      }
      else {
        dodge_strafe_dir_ = Dodge::Left;
      }

      if (rg.chance (30)) {
        swap_dodge_direction ();
      }
      strafe_set_timer_.start (strafe_update_delay ());
      commit_strafe ();
    }
    const float wall_check_dist = ystl::min (134.0f, ystl::sqrtf (distance_sq) * 0.25f);

    const bool wall_on_right = CheckWallOnRight (wall_check_dist);
    const bool wall_on_left = CheckWallOnLeft (wall_check_dist);

    if (dodge_strafe_dir_ == Dodge::Left) {
      if (!wall_on_left) {
        strafe_speed_ = -pev->maxspeed;
      }
      else if (!wall_on_right) {
        swap_dodge_direction ();

        strafe_set_timer_.start (strafe_update_delay ());
        commit_strafe ();
        strafe_speed_ = pev->maxspeed;
      }
      else {
        strafe_speed_ = 0.0f;
        strafe_set_timer_.start (strafe_update_delay ());
        commit_strafe ();
      }
    }
    else {
      if (!wall_on_right) {
        strafe_speed_ = pev->maxspeed;
      }
      else if (!wall_on_left) {
        swap_dodge_direction ();

        strafe_set_timer_.start (strafe_update_delay ());
        commit_strafe ();
        strafe_speed_ = -pev->maxspeed;
      }
      else {
        strafe_speed_ = 0.0f;
        strafe_set_timer_.start (strafe_update_delay ());
        commit_strafe ();
      }
    }

    // detect if combat strafe is stuck (not actually moving sideways since direction was committed)
    if (combat_stuck_check_timer_.greater_than (0.25f) && !ystl::fzero (strafe_speed_)) {
      const float strafe_moved = last_combat_strafe_origin_.distance_sq2d (pev->origin);

      if (strafe_moved < 4.0f && pev->velocity.length2d () > 0.0f) {
        swap_dodge_direction ();
        commit_strafe ();
        strafe_set_timer_.start (strafe_update_delay ());
      }
    }

    // do not move if inside "corridor"
    if (wall_on_right && wall_on_left && !UsesKnife ()) {
      strafe_speed_ = 0.0f;
      move_speed_ = 0.0f;

      strafe_set_timer_.start (3.0f);
      dodge_strafe_dir_ = Dodge::None;
    }

    // we're setting strafe speed regardless of move angles, so not resetting forward move here cause bots to behave strange
    if (!UsesKnife () && approach >= 30) {
      move_speed_ = 0.0f;
    }

    if (difficulty_ >= Difficulty::Normal && distance_sq < ystl::sqrf (kSprayDistance) && (reload_data_.is_reloading || health_value_ < 35.0f) &&
        jump_time_ + 5.0f < game.Time () && IsOnFloor () && pev->velocity.length2d () > 150.0f && !UsesSniper () && is_enemy_cone) {

      pev->button |= IN_JUMP;
    }
  }
  else if (fight_style_ == Fight::Stay) {
    const bool already_ducking = !duck_timer_.elapsed () || IsDucking () || ((pev->button | pev->oldbuttons) & IN_DUCK);

    if (already_ducking) {
      duck_timer_.start (frame_interval_ * 3.0f);
    }
    else if ((distance_sq > ystl::sqrf (kSprayDistanceX2) && HasPrimaryWeapon ()) && is_full_view && GetTaskId () != TaskId::SeekCover &&
             GetTaskId () != TaskId::Hunt) {

      // skilled bots duck at long range for accuracy, noobs don't think of it
      if (difficulty_ >= Difficulty::Normal) {
        const int enemy_nearest_index = graph.GetNearest (enemy_->v.origin);

        if (vistab.VisibleBothSides (current_node_index_, enemy_nearest_index, VisIndex::Crouch)) {
          duck_timer_.start (frame_interval_ * 3.0f);
        }
      }
    }
    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;
  }

  if (reload_data_.is_reloading) {
    move_speed_ = -pev->maxspeed;

    // only force unduck if we're not behind cover, ducking while reloading behind cover is beneficial
    if (is_enemy_cone) {
      duck_timer_.invalidate ();
    }
  }

  if (!IsInWater () && !IsOnLadder () && (!ystl::fzero (move_speed_) || !ystl::fzero (strafe_speed_))) {
    ystl::Vector right {}, forward {};
    pev->v_angle.angle_vectors (&forward, &right, nullptr);

    const ystl::Vector front = forward * move_speed_ * 0.2f;
    const ystl::Vector side = right * strafe_speed_ * 0.2f;
    const ystl::Vector spot = pev->origin + front + side + pev->velocity * frame_interval_;

    if (IsNotSafeToMove (spot)) {
      // try reversing strafe first, only reverse forward if strafe alone doesn't help
      const ystl::Vector strafe_fix = pev->origin + front + right * -strafe_speed_ * 0.2f + pev->velocity * frame_interval_;

      if (!IsNotSafeToMove (strafe_fix)) {
        strafe_speed_ = -strafe_speed_;
      }
      else {
        strafe_speed_ = -strafe_speed_;
        move_speed_ = -move_speed_;
      }
      pev->button &= ~IN_JUMP;
    }
  }
  IgnoreCollision ();
}

bool Bot::IsKnifeMode () {
  return cv_jasonmode || (UsesKnife () && !HasAnyWeapons ()) || is_creature_ ||
         (has_flag (states_, Sense::SeeingEnemy) && UsesKnife () && !HasAnyAmmoInClip ());
}

bool Bot::IsGrenadeWar () {
  const bool has_some_greandes = GetBestGrenadeCarriedId () != Weapon::Invalid;

  // if has grenade an not other weapons, assume we're in grenade war
  if (!HasAnyWeapons () && has_some_greandes) {
    return true;
  }

  // if we're forced to via cvar
  if (cv_grenadier_mode) {
    return true;
  }
  return game.MapIs (MapFlags::GrenadeWar); // in case map was flagged
}

void Bot::UpdateTeamCommands () {
  // prevent spamming

  if (team_order_timer_.remaining_time () > 2.0f || game.Is (GameFlags::FreeForAll) || !cv_radio_mode.As<int> ()) {
    return;
  }

  bool member_near = false;
  bool member_exists = false;

  // search teammates seen by this bot
  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }
    member_exists = true;

    if (SeesEntity (client.origin)) {
      member_near = true;
      break;
    }
  }

  // has teammates?
  if (member_near) {
    if (personality_ == Personality::Rusher && cv_radio_mode.As<int> () == 2) {
      PushRadioChat (RadioChat::StormTheFront);
    }
    else if (personality_ != Personality::Rusher && cv_radio_mode.As<int> () == 2) {
      PushRadioChat (RadioChat::TeamFallback);
    }
  }
  else if (member_exists && cv_radio_mode.As<int> () == 1) {
    PushRadioChat (RadioChat::TakingFireNeedAssistance);
  }
  else if (member_exists && cv_radio_mode.As<int> () == 2) {
    PushRadioChat (RadioChat::ScaredEmotion);
  }
  team_order_timer_.start (rg (15.0f, 30.0f));
}

bool Bot::IsGroupOfEnemies (const ystl::Vector &location, float radius) {
  int num_players = 0;

  // needs a square radius
  const float radius_sq = ystl::sqrf (radius);

  // search the world for enemy players
  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.IsSameTeam (team_)) {
      continue;
    }

    if (client.IsInRadius (location, radius_sq)) {
      if (!SeesEntity (client.origin)) {
        continue;
      }
      ++num_players;
    }
  }

  if (num_players < 2) {
    return false;
  }
  return true;
}

float Bot::CalculateScaleFactor (edict_t *ent) const {
  const ystl::Vector ent_size = ent->v.maxs - ent->v.mins;
  const float ent_area = 2.0f * (ent_size.x * ent_size.y + ent_size.y * ent_size.z + ent_size.x * ent_size.z);

  const ystl::Vector bot_size = pev->maxs - pev->mins;
  const float bot_area = 2.0f * (bot_size.x * bot_size.y + bot_size.y * bot_size.z + bot_size.x * bot_size.z);

  return ent_area / bot_area;
}

ystl::Vector Bot::CalcToss (const ystl::Vector &start, const ystl::Vector &stop) {
  // calculates the velocity vector needed to toss a grenade from start to stop using a parabolic arc

  static constexpr float kGravityFactor = 0.55f;
  static constexpr float kMinAscentTime = 0.1f;
  static constexpr float kMaxHeightDiff = 500.0f;
  static constexpr float kCeilingTraceHeight = 500.0f;
  static constexpr float kTargetHeightOffset = 15.0f;
  static constexpr float kVelocityScale = 0.777f;
  static constexpr float kMaxWallDot = 0.75f;
  static constexpr float kMinTraceFraction = 0.8f;

  // open-sky arc estimate, mirrors the flat throw rise in calcThrow
  static constexpr float kArcSpeed = 195.0f;
  static constexpr float kArcMaxFlightTime = 2.0f;
  static constexpr float kArcClampedFlightTime = 1.2f;

  const float gravity = sv_gravity.As<float> () * kGravityFactor;

  if (ystl::fzero (gravity)) {
    return nullptr;
  }

  // adjust target position slightly downward for better landing
  ystl::Vector target = stop;
  target.z -= kTargetHeightOffset;

  // reject if height difference is too extreme
  if (ystl::abs (target.z - start.z) > kMaxHeightDiff) {
    return nullptr;
  }

  // find the midpoint horizontally and trace up to find ceiling
  ystl::Vector mid_point = start + (target - start) * 0.5f;
  Trace::Result tr {};

  trace.Hull (mid_point, mid_point + ystl::Vector (0.0f, 0.0f, kCeilingTraceHeight), TraceIgnore::Monsters, head_hull, Ent (), &tr);

  // adjust apex height if ceiling is hit
  if (tr.fraction < 1.0f && tr.hit) {
    mid_point = tr.end_pos;
    mid_point.z = tr.hit->v.absmin.z - 1.0f;
  }
  else {
    float flight_time = (target - start).length () / kArcSpeed;

    if (flight_time > kArcMaxFlightTime) {
      flight_time = kArcClampedFlightTime;
    }
    const float half_time = flight_time * 0.5f;
    mid_point.z += 0.5f * gravity * half_time * half_time;
  }

  // validate that apex is above both start and end points
  if (mid_point.z < start.z || mid_point.z < target.z) {
    return nullptr;
  }

  // calculate time to ascend from start to apex
  const float ascent_height = mid_point.z - start.z;
  const float time_to_apex = ystl::sqrtf (ascent_height / (0.5f * gravity));

  if (time_to_apex < kMinAscentTime) {
    return nullptr;
  }

  // calculate time to descend from apex to target
  const float descent_height = mid_point.z - target.z;
  const float time_from_apex = ystl::sqrtf (descent_height / (0.5f * gravity));

  const float total_time = time_to_apex + time_from_apex;

  // calculate horizontal velocity components
  ystl::Vector velocity = (target - start) / total_time;

  // set vertical velocity for proper arc (velocity at start needed to reach apex)
  velocity.z = gravity * time_to_apex;

  // calculate apex position for collision checking
  ystl::Vector apex = start + velocity * time_to_apex;
  apex.z = mid_point.z;

  // verify clear path from start to apex
  trace.Hull (start, apex, TraceIgnore::None, head_hull, Ent (), &tr);

  if (tr.fraction < 1.0f || tr.all_solid) {
    return nullptr;
  }

  // verify clear path from target to apex (allowing monsters to be ignored)
  trace.Hull (target, apex, TraceIgnore::Monsters, head_hull, Ent (), &tr);

  if (!ystl::fequal (tr.fraction, 1.0f)) {
    // check if trajectory hits a wall at a steep angle
    const float dot = tr.plane_normal | (apex - target).normalize ();

    if (dot > kMaxWallDot || tr.fraction < kMinTraceFraction) {
      return nullptr;
    }
  }
  return velocity * kVelocityScale;
}

ystl::Vector Bot::CalcThrow (const ystl::Vector &start, const ystl::Vector &stop) {
  // compute flat throw velocity from start to stop, null if infeasible

  static constexpr float kGravityFactor = 0.55f;
  static constexpr float kThrowSpeed = 195.0f;
  static constexpr float kMinFlightTime = 0.01f;
  static constexpr float kMaxFlightTime = 2.0f;
  static constexpr float kClampedFlightTime = 1.2f;
  static constexpr float kVelocityScale = 0.7793f;
  static constexpr float kMaxWallDot = 0.75f;
  static constexpr float kMinTraceFraction = 0.8f;

  const float gravity = sv_gravity.As<float> () * kGravityFactor;

  if (ystl::fzero (gravity)) {
    return nullptr;
  }

  // calculate initial displacement vector
  ystl::Vector displacement = stop - start;

  // estimate flight time based on distance and desired throw speed
  float flight_time = displacement.length () / kThrowSpeed;

  if (flight_time < kMinFlightTime) {
    return nullptr;
  }

  // clamp maximum flight time to prevent unrealistic arcs
  if (flight_time > kMaxFlightTime) {
    flight_time = kClampedFlightTime;
  }

  const float half_time = flight_time * 0.5f;

  // calculate horizontal velocity (normalize by time)
  ystl::Vector velocity = displacement * (1.0f / flight_time);

  // add vertical velocity to compensate for gravity drop over the flight time formula: v_z = g * t / 2
  velocity.z += gravity * half_time;

  // calculate apex position for collision checking apex occurs at half the flight time
  ystl::Vector apex = start + displacement * 0.5f;

  // height gain at apex: h = 0.5 * g * (t/2)^2
  apex.z += 0.5f * gravity * half_time * half_time;

  Trace::Result tr {};

  // verify clear path from start to apex
  trace.Hull (start, apex, TraceIgnore::None, head_hull, Ent (), &tr);

  if (!ystl::fequal (tr.fraction, 1.0f)) {
    return nullptr;
  }

  // verify clear path from target to apex (allowing monsters to be ignored)
  trace.Hull (stop, apex, TraceIgnore::Monsters, head_hull, Ent (), &tr);

  if ((!ystl::fequal (tr.fraction, 1.0f) || tr.all_solid)) {
    // check if trajectory hits a wall at a steep angle
    const float dot = tr.plane_normal | (apex - stop).normalize ();

    if (dot > kMaxWallDot || tr.fraction < kMinTraceFraction) {
      return nullptr;
    }
  }
  return velocity * kVelocityScale;
}

edict_t *Bot::SetCorrectGrenadeVelocity (ystl::StringRef model) {
  edict_t *result = nullptr;

  game.SearchEntities ("classname", "grenade", [&] (edict_t *ent) {
    if (ent->v.owner == this->Ent () && game.IsEntityModelMatches (ent, model)) {
      result = ent;

      // set the correct velocity for the grenade
      if (grenade_.length_sq () > 100.0f) {
        ent->v.velocity = grenade_ + (grenade_ * frame_interval_ * 4.0f);
      }
      grenade_check_timer_.start (3.0f);

      SelectBestWeapon ();
      CompleteTask ();

      return EntitySearchResult::Break;
    }
    return EntitySearchResult::Continue;
  });
  return result;
}

void Bot::CheckGrenadesThrow () {
  // do not check cancel if we have grenade in out hands
  const bool preventible_tasks = HasBombTask ();
  const bool is_grenade_mode = IsGrenadeWar ();

  auto clear_throw_states = [] (Sense &states) {
    states &= ~(Sense::ThrowExplosive | Sense::ThrowFlashbang | Sense::ThrowSmoke);
  };

  // check if throwing a grenade is a good thing to do
  const auto throwing_condition =
    is_grenade_mode ? last_enemy_origin_.empty ()
                    : (preventible_tasks || IsInNarrowPlace () || cv_ignore_enemies || is_using_grenade_ || reload_data_.is_reloading ||
                        (IsKnifeMode () && !game_state.IsBombPlanted ()) || !grenade_check_timer_.elapsed () || last_enemy_origin_.empty ());

  if (throwing_condition) {
    clear_throw_states (states_);
    return;
  }

  // check again in some seconds
  grenade_check_timer_.start (kGrenadeCheckTime);

  const auto sense_condition = is_grenade_mode ? false : !has_flag (states_, Sense::SuspectEnemy | Sense::HearingEnemy);

  if (!game.IsAliveEntity (last_enemy_) || sense_condition) {
    clear_throw_states (states_);
    return;
  }

  // check if we have grenades to throw
  const auto grenade_to_throw = GetBestGrenadeCarriedId ();

  // if we don't have grenades no need to check it this round again
  if (grenade_to_throw == Weapon::Invalid) {
    grenade_check_timer_.start (15.0f); // changed since, czero can drop grenades from dead players

    clear_throw_states (states_);
    return;
  }
  else if (!is_grenade_mode) {
    int cancel_prob = agression_level_ > fear_level_ ? 5 : 20;

    if (grenade_to_throw == Weapon::Flashbang) {
      // lower cancel chance for higher difficulty (more tactical usage)
      cancel_prob = difficulty_ >= Difficulty::Normal ? 20 : 30;
    }
    else if (grenade_to_throw == Weapon::Smoke) {
      // reduce cancel chance in tactical situations
      if (in_bomb_zone_ || health_value_ < 50.0f || has_flag (last_enemy_->v.weapons, kSniperWeaponMask)) {
        cancel_prob = difficulty_ >= Difficulty::Normal ? 15 : 25;
      }
      else {
        cancel_prob = difficulty_ >= Difficulty::Normal ? 25 : 35;
      }
    }
    if (rg.chance (cancel_prob)) {
      clear_throw_states (states_);
      return;
    }
  }
  float distance_sq = last_enemy_origin_.distance_sq2d (pev->origin);

  // don't throw grenades at anything that isn't on the ground!
  if (!(last_enemy_->v.flags & (FL_ONGROUND | FL_PARTIALGROUND)) && !last_enemy_->v.waterlevel && last_enemy_origin_.z > pev->absmax.z) {
    distance_sq = kInfiniteDistance;
  }

  // too high to throw?
  if (last_enemy_->v.origin.z > pev->origin.z + 500.0f) {
    distance_sq = kInfiniteDistance;
  }

  // special condition if we're have valid current enemy
  if (!is_grenade_mode &&
      (has_flag (states_, Sense::SeeingEnemy) && game.IsAliveEntity (enemy_) && ((enemy_->v.button | enemy_->v.oldbuttons) & IN_ATTACK) &&
        util.IsVisible (pev->origin, enemy_)) &&
      util.IsInViewCone (pev->origin, enemy_)) {

    // do not throw away grenades if anyone is attacking us
    distance_sq = kInfiniteDistance;
  }

  // don't throw away nades if just seen the enemy
  if (!is_grenade_mode && see_enemy_timer_.less_than (kGrenadeCheckTime * 0.2f)) {
    distance_sq = kInfiniteDistance;
  }

  // enemy within a good throw distance?
  const auto grenade_to_throw_condition = is_grenade_mode                     ? kGrenadeDamageRadius / 4.0f
                                          : grenade_to_throw == Weapon::Smoke ? 200.0f
                                                                              : kGrenadeDamageRadius;

  if (distance_sq > ystl::sqrf (grenade_to_throw_condition) && distance_sq < ystl::sqrf (kGrenadeDamageRadius * 3.0f)) {
    bool allow_throwing = true;

    // care about different grenades
    switch (grenade_to_throw) {
    case Weapon::Explosive:
      if (mp_friendlyfire && NumFriendsNear (last_enemy_->v.origin, 256.0f) > 0) {
        allow_throwing = false;
      }
      else {
        const auto radius = ystl::max (192.0f, last_enemy_->v.velocity.length2d ());
        const ystl::Vector pos = last_enemy_->v.velocity.get2d () + last_enemy_->v.origin;

        auto predicted = graph.GetNearestInRadius (radius, pos, 12);

        if (predicted.empty ()) {
          states_ &= ~Sense::ThrowExplosive;
          break;
        }

        for (const auto &predict : predicted) {
          allow_throwing = true;

          if (!graph.Exists (predict)) {
            allow_throwing = false;
            continue;
          }
          throw_ = graph[predict].origin;

          auto throw_pos = CalcThrow (GetEyesPos (), throw_);

          if (throw_pos.length_sq () < 100.0f) {
            throw_pos = CalcToss (GetEyesPos (), throw_);
          }

          if (throw_pos.empty ()) {
            allow_throwing = false;
          }
          else {
            throw_.z += 110.0f;
            break;
          }
        }
      }

      if (allow_throwing) {
        states_ |= Sense::ThrowExplosive;
      }
      else {
        states_ &= ~Sense::ThrowExplosive;
      }
      break;

    case Weapon::Flashbang: {
      const auto radius = ystl::max (192.0f, last_enemy_->v.velocity.length2d ());
      const ystl::Vector pos = last_enemy_->v.velocity.get2d () + last_enemy_->v.origin;

      auto predicted = graph.GetNearestInRadius (radius, pos, 8);

      if (predicted.empty ()) {
        states_ &= ~Sense::ThrowFlashbang;
        break;
      }

      for (const auto &predict : predicted) {
        allow_throwing = true;

        if (!graph.Exists (predict)) {
          allow_throwing = false;
          continue;
        }
        throw_ = graph[predict].origin;

        // check if teammates are near the flash target
        if (NumFriendsNear (throw_, 256.0f) > 0) {
          allow_throwing = false;
          continue;
        }
        auto throw_pos = CalcThrow (GetEyesPos (), throw_);

        if (throw_pos.length_sq () < 100.0f) {
          throw_pos = CalcToss (GetEyesPos (), throw_);
        }

        if (throw_pos.empty ()) {
          allow_throwing = false;
        }
        else {
          throw_.z += 110.0f;
          break;
        }
      }

      if (allow_throwing) {
        states_ |= Sense::ThrowFlashbang;
      }
      else {
        states_ &= ~Sense::ThrowFlashbang;
      }
      break;
    }

    case Weapon::Smoke:
      if (allow_throwing && !game.IsNullEntity (last_enemy_)) {
        ystl::Vector smoke_target {};
        const ystl::Vector to_enemy = (last_enemy_->v.origin - pev->origin).normalize ();
        const float distance_to_enemy = last_enemy_->v.origin.distance (pev->origin);

        // context-aware smoke placement
        if (in_bomb_zone_ && (has_c4_ || GetTaskId () == TaskId::PlantBomb)) {
          // planting: smoke closer to bot for cover (30-40% distance)
          const float smoke_distance = distance_to_enemy * rg (0.3f, 0.4f);
          smoke_target = pev->origin + to_enemy * smoke_distance;
        }
        else if (health_value_ < 50.0f || reload_data_.is_reloading) {
          // retreating/low hp: smoke closer for escape cover (25-35% distance)
          const float smoke_distance = distance_to_enemy * rg (0.25f, 0.35f);
          smoke_target = pev->origin + to_enemy * smoke_distance;
        }
        else if (has_flag (last_enemy_->v.weapons, kSniperWeaponMask)) {
          // sniper threat: smoke at mid-range to block sightline (45-55% distance)
          const float smoke_distance = distance_to_enemy * rg (0.45f, 0.55f);
          smoke_target = pev->origin + to_enemy * smoke_distance;
        }
        else {
          // default: block sightline at optimal distance (40-60%)
          const float smoke_distance = distance_to_enemy * rg (0.4f, 0.6f);
          smoke_target = pev->origin + to_enemy * smoke_distance;
        }

        // find nearest valid node for smoke placement
        const int smoke_node_index = graph.GetNearest (smoke_target);

        if (graph.Exists (smoke_node_index)) {
          throw_ = graph[smoke_node_index].origin;

          // validate throw trajectory
          auto throw_pos = CalcThrow (GetEyesPos (), throw_);

          if (throw_pos.length_sq () < 100.0f) {
            throw_pos = CalcToss (GetEyesPos (), throw_);
          }

          if (throw_pos.empty ()) {
            allow_throwing = false;
          }
          else {
            throw_.z += 110.0f;
          }
        }
        else {
          allow_throwing = false;
        }
      }
      else {
        allow_throwing = false;
      }

      if (allow_throwing) {
        states_ |= Sense::ThrowSmoke;
      }
      else {
        states_ &= ~Sense::ThrowSmoke;
      }
      break;

    default:
      clear_throw_states (states_);
      return;
    }
    const float max_throw_time = game.Time () + kGrenadeCheckTime * 3.6f;

    if (has_flag (states_, Sense::ThrowExplosive)) {
      StartTask (TaskId::ThrowExplosive, TaskPri::kThrow, kInvalidNodeIndex, max_throw_time, false);
    }
    else if (has_flag (states_, Sense::ThrowFlashbang)) {
      StartTask (TaskId::ThrowFlashbang, TaskPri::kThrow, kInvalidNodeIndex, max_throw_time, false);
    }
    else if (has_flag (states_, Sense::ThrowSmoke)) {
      StartTask (TaskId::ThrowSmoke, TaskPri::kThrow, kInvalidNodeIndex, max_throw_time, false);
    }
  }
  else {
    clear_throw_states (states_);
  }
}

bool Bot::IsEnemyInSight (ystl::Vector &end_pos) {
  Trace::Result aim_hit_tr {};
  trace.Model (GetEyesPos (), GetEyesPos () + pev->v_angle.forward () * kInfiniteDistance, 0, enemy_, &aim_hit_tr);

  if (aim_hit_tr.hit != enemy_) {
    return false;
  }
  end_pos = aim_hit_tr.end_pos;
  return true;
}

bool Bot::IsEnemyNoticeable (float range) {
  // this function is back ported from regamedll with small changes

  if (IsOnLadder ()) {
    return false;
  }

  // determine percentage of player that is visible
  float cover_ratio = 0.0f;

  if (has_flag (enemy_parts_, Visibility::Body)) {
    cover_ratio += 40.0f;
  }

  if (has_flag (enemy_parts_, Visibility::Head)) {
    cover_ratio += 10.0f;
  }

  if (has_flag (enemy_parts_, Visibility::Other)) {
    cover_ratio += rg (10.0f, 25.0f);
  }
  constexpr float kCloseRange = 300.0f;
  constexpr float kFarRange = 1000.0f;

  float range_modifier {};

  if (range < kCloseRange) {
    range_modifier = 0.0f;
  }
  else if (range > kFarRange) {
    range_modifier = 1.0f;
  }
  else {
    range_modifier = (range - kCloseRange) / (kFarRange - kCloseRange);
  }

  // harder to notice when crouched
  bool is_crouching = (enemy_->v.flags & FL_DUCKING) == FL_DUCKING;

  // moving players are easier to spot
  float player_speed_sq = enemy_->v.velocity.length_sq ();
  float far_chance {}, close_chance {};

  constexpr float kRunSpeed = ystl::sqrf (200.0f);
  constexpr float kWalkSpeed = ystl::sqrf (30.0f);

  if (player_speed_sq > kRunSpeed) {
    return true; // running players are always easy to spot (must be standing to run)
  }
  else if (player_speed_sq > kWalkSpeed) {
    // walking players are less noticeable far away
    if (is_crouching) {
      close_chance = 90.0f;
      far_chance = 60.0f;
    }
    // standing
    else {
      close_chance = 100.0f;
      far_chance = 75.0f;
    }
  }
  else {
    // motionless players are hard to notice
    if (is_crouching) {
      // crouching and motionless - very tough to notice
      close_chance = 80.0f;
      far_chance = 5.0f; // takes about three seconds to notice (50% chance)
    }
    // standing
    else {
      close_chance = 100.0f;
      far_chance = 10.0f;
    }
  }

  const float disposition_chance = close_chance + (far_chance - close_chance) * range_modifier; // combine posture, speed, and range chances
  float notice_chance = disposition_chance * cover_ratio / 100.0f; // determine actual chance of noticing player

  notice_chance += (0.5f + 0.5f * (static_cast<float> (difficulty_) * 25.0f));

  // if we are alert, our chance of noticing is much higher
  if (agression_level_ > fear_level_) {
    notice_chance += 50.0f;
  }
  notice_chance = ystl::max (0.1f, notice_chance * ystl::abs (agression_level_ - fear_level_));

  return rg (0.0f, 100.0f) < notice_chance;
}

bool Bot::IsEnemyThreat () {
  if (game.IsNullEntity (enemy_) || has_flag (states_, Sense::SuspectEnemy) || GetTaskId () == TaskId::SeekCover) {
    return false;
  }

  // if bot is camping, he should be firing anyway and not leaving his position
  if (GetTaskId () == TaskId::Camp) {
    return false;
  }

  auto is_on_attack_distance = [&] (edict_t *e, const float distance) -> bool {
    const float distance_sq = e->v.origin.distance_sq (pev->origin);

    if (distance_sq < ystl::sqrf (distance)) {
      return true;
    }

    // knife users need to chase from further away to close the gap
    if (UsesKnife () && distance_sq < ystl::sqrf (distance)) {
      return true;
    }
    return false;
  };

  // if enemy is near or facing us directly
  if (is_on_attack_distance (enemy_, 256.0f) || (!UsesKnife () && IsInViewCone (enemy_->v.origin))) {
    return true;
  }
  return false;
}

bool Bot::ReactOnEnemy () {
  // the purpose of this function is check if task has to be interrupted because an enemy is near (run attack actions then)

  if (!IsEnemyThreat ()) {
    return false;
  }

  // enemy could have disconnected between isenemythreat and here
  if (game.IsNullEntity (enemy_)) {
    is_enemy_reachable_ = false;
    return false;
  }

  // special case for creatures
  if (is_creature_) {
    is_enemy_reachable_ = pev->origin.distance_sq2d (enemy_->v.origin) < ystl::sqrf (128.0f);

    if (is_enemy_reachable_) {
      nav_timer_.start ();
    }
    return is_enemy_reachable_;
  }

  if (enemy_reachable_timer_.elapsed ()) {
    const auto line_dist = enemy_->v.origin.distance (pev->origin);

    if (IsEnemyNoticeable (line_dist)) {
      is_enemy_reachable_ = true;
    }
    else {
      int own_index = current_node_index_;

      if (own_index == kInvalidNodeIndex) {
        own_index = FindNearestNode ();
      }
      const auto enemy_index = graph.GetNearest (enemy_->v.origin);
      const auto path_dist = planner.PreciseDistance (own_index, enemy_index);

      is_enemy_reachable_ = (path_dist - line_dist <= 112.0f) && !IsOnLadder ();
    }
    enemy_reachable_timer_.start (0.75f);
  }

  if (is_enemy_reachable_) {
    nav_timer_.start (); // override existing movement by attack movement
    return true;
  }
  return false;
}

bool Bot::GetThruWallChance (int pct) const {
  // keep thru-wall behavior unpredictable: for skill values above 25% roll anywhere in 25..pct range

  return rg.chance (pct > 25 ? rg (25, pct) : pct);
}

bool Bot::LastEnemyShootable () {
  // fire at remembered spot only if it can be penetrated
  if (!has_flag (aim_flags_, AimFlags::LastEnemy) || last_enemy_origin_.empty () || game.IsNullEntity (last_enemy_)) {
    return false;
  }
  return util.ViewDot (Ent (), last_enemy_origin_) >= 0.90f && IsPenetrableObstacleCached (last_enemy_origin_);
}

} // namespace bot
