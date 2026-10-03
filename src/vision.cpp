//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

float Bot::IsInFov (const ystl::Vector &destination) const {
  // signed shortest yaw difference wrapped into [-180, 180), then absolute:
  // always the true angle to the target, zero means straight ahead, max 180.
  // wrapping the difference (not the operands) keeps this correct even when the
  // view yaw is stored unnormalized.
  return ystl::abs (ystl::wrap_angle (pev->v_angle.y - destination.yaw ()));
}

bool Bot::IsInViewCone (const ystl::Vector &origin) {
  // return true if the origin lies inside the view cone

  return util.IsInViewCone (origin, Ent ());
}

bool Bot::SeesItem (const ystl::Vector &destination, ystl::StringRef classname) {
  Trace::Result tr {};

  // trace a line from bot's eyes to destination
  trace.Line (GetEyesPos (), destination, TraceIgnore::Monsters, Ent (), &tr);

  // check if line of sight to object is not blocked (i.e. visible)
  if (tr.start_solid) {
    return false;
  }

  if (tr.fraction >= 1.0f) {
    return true;
  }

  if (tr.hit && classname == tr.hit->v.classname.str ()) {
    return true;
  }

  // blocked by the prop the item rests on, so tolerate mostly-clear line of sight
  return tr.fraction >= 0.95f;
}

bool Bot::SeesC4 (const ystl::Vector &destination) {
  Trace::Result tr {};

  // trace a line from bot's eyes to destination
  trace.Line (GetEyesPos (), destination, TraceIgnore::Monsters, Ent (), &tr);

  // c4 is non-solid and its origin hugs the surface it rests on, so tolerate mostly-clear line of sight
  return !tr.start_solid && tr.fraction >= 0.80f;
}

bool Bot::SeesEntity (const ystl::Vector &dest, bool from_body) {
  Trace::Result tr {};

  // trace a line from bot's eyes to destination
  trace.Line (from_body ? pev->origin : GetEyesPos (), dest, TraceIgnore::Everything, Ent (), &tr);

  // check if line of sight to object is not blocked (i.e. visible)
  return tr.fraction >= 1.0f && trace.IsEndpointClear (tr);
}

void Bot::CheckDarkness () {

  // do not check for darkness at the start of the round
  if (spawn_timer_.less_than (5.0f) || !graph.Exists (current_node_index_)) {
    return;
  }

  // do not check every frame
  if (!path_ || !check_dark_timer_.elapsed () || ystl::fequal (path_->light, kInvalidLightLevel)) {
    return;
  }

  const auto light_level = path_->light;
  const auto sky_color = illum.GetSkyColor ();
  const auto flash_on = (pev->effects & EF_DIMLIGHT);

  if (mp_flashlight && !has_nvg_) {
    const auto tid = GetTaskId ();

    if (!flash_on && tid != TaskId::Camp && tid != TaskId::Attack && heard_sound_timer_.greater_than (3.0f) && flash_level_ > 30 &&
        ((sky_color > 50.0f && light_level < 10.0f) || (sky_color <= 50.0f && light_level < 40.0f))) {

      pev->impulse = 100;
    }
    else if (flash_on && (((light_level > 15.0f && sky_color > 50.0f) || (light_level > 45.0f && sky_color <= 50.0f)) || tid == TaskId::Camp ||
                           tid == TaskId::Attack || flash_level_ <= 0 || heard_sound_timer_.less_than (3.0f))) {

      pev->impulse = 100;
    }
  }
  else if (has_nvg_) {
    if (flash_on) {
      pev->impulse = 100;
    }
    else if (!uses_nvg_ && ((sky_color > 50.0f && light_level < 15.0f) || (sky_color <= 50.0f && light_level < 40.0f))) {
      IssueCommand ("nightvision");
    }
    else if (uses_nvg_ && ((light_level > 20.0f && sky_color > 50.0f) || (light_level > 45.0f && sky_color <= 50.0f))) {
      IssueCommand ("nightvision");
    }
  }
  check_dark_timer_.start (rg (2.0f, 4.0f));
}

void Bot::UpdateBodyAngles () {
  constexpr float kValue = 1.0f / 3.0f;

  // set the body angles to point the gun correctly
  pev->angles.x = -pev->v_angle.x * kValue;
  pev->angles.y = pev->v_angle.y;

  pev->angles.clamp_angles ();

  // calculate frustum plane data here, since look angles update functions call this last one
  frustum.Calculate (view_frustum_, pev->v_angle, GetEyesPos (), Frustum::kFov);
}

void Bot::UpdateLookAngles () {
  // plugin-set look target wins over the ai one
  if (!ForcedLookAt ().empty ()) {
    look_at_ = ForcedLookAt ();
    SetAimDirection ();

    return;
  }

  const float actual_delta = game.Time () - look_update_time_;
  const float delta = ystl::clamp (actual_delta, ystl::kFloatEqualEpsilon, kViewFrameUpdate);

  look_update_time_ += delta;

  // no valid look target yet: hold current view direction instead of aiming at garbage/empty vector
  if (look_at_.empty ()) {
    look_at_ = GetEyesPos () + pev->v_angle.forward () * 500.0f;
  }

  // adjust all body and view angles to face an absolute vector
  ystl::Vector direction = (look_at_ - GetEyesPos ()).angles ();
  direction.x = -direction.x; // invert for engine

  direction.clamp_angles ();

  // just force direction
  if (difficulty_ == Difficulty::Expert && has_flag (aim_flags_, AimFlags::Enemy) && (wants_to_fire_ || UsesSniper ()) && cv_whose_your_daddy) {

    pev->v_angle = direction;
    pev->v_angle.clamp_angles ();

    UpdateBodyAngles ();
    return;
  }
  constexpr float kPitchFactor = 2.0f; // pitch is 2x stiffer (legacy feel)
  constexpr float kYawDampingRatio = 1.0f; // yaw critically damped (no oscillation)
  constexpr float kPitchDampingRatio = 0.8f; // pitch slightly underdamped for faster vertical tracking
  constexpr float kSnapAngle = 1.0f; // deg, snap-to-target window

  const bool engaging = has_flag (aim_flags_, AimFlags::Enemy | AimFlags::Grenade) || wants_to_fire_;

  AimConfig yaw {};
  AimConfig pitch {};

  yaw.stiffness = 200.0f;
  pitch.stiffness = kPitchFactor * yaw.stiffness;

  float accelerate = 3000.0f;

  if (engaging) {
    const float aim_bonus = static_cast<float> (Skill ()) / 100.0f;
    accelerate += 300.0f * aim_bonus;
    yaw.stiffness += 100.0f * aim_bonus;
    pitch.stiffness += kPitchFactor * 100.0f * aim_bonus; // keep 2x ratio
  }

  // per-axis damping; yaw critical, pitch underdamped for faster vertical tracking
  yaw.damping = kYawDampingRatio * 2.0f * ystl::sqrtf (yaw.stiffness);
  pitch.damping = kPitchDampingRatio * 2.0f * ystl::sqrtf (pitch.stiffness);

  yaw.max_accel = pitch.max_accel = accelerate;
  yaw.snap_angle = pitch.snap_angle = kSnapAngle;

  ideal_angles_ = pev->v_angle;

  float angle_diff_pitch = ystl::angles_difference (direction.x, ideal_angles_.x);
  float angle_diff_yaw = ystl::angles_difference (direction.y, ideal_angles_.y);

  // prevent reverse-facing turn when navigating without priority targets
  auto adjust_yaw_for_navigation = [&] () {
    const auto look_delta = look_at_ - GetEyesPos ();

    // on slope transitions target can become almost vertical; avoid unstable yaw corrections there
    if (look_delta.length_sq2d () < ystl::sqrf (48.0f)) {
      return;
    }
    const float forward = move_angles_.y;
    const float current = ystl::wrap_angle (pev->v_angle.y - forward);
    const float target = ystl::wrap_angle (direction.y - forward);

    // both on same side - no adjustment needed
    if (current * target >= 0.0f) {
      return;
    }

    // only correct large yaw jumps across the boundary to avoid jitter
    if (ystl::abs (current - target) >= 180.0f && ystl::abs (angle_diff_yaw) > 120.0f) {
      angle_diff_yaw = (angle_diff_yaw > 0.0f) ? -180.0f : 180.0f;
    }
  };

  if (move_to_goal_ && !engaging && !path_origin_.empty () && !IsOnLadder ()) {
    adjust_yaw_for_navigation ();
  }

  // freeze pitch when the target is almost straight above or below
  if (!engaging && (look_at_ - GetEyesPos ()).length_sq2d () < ystl::sqrf (48.0f)) {
    angle_diff_pitch = 0.0f;
  }

  ideal_angles_.y = look_yaw_.Update (yaw, angle_diff_yaw, ideal_angles_.y, delta);
  ideal_angles_.x = look_pitch_.Update (pitch, angle_diff_pitch, ideal_angles_.x, delta);

  ideal_angles_.clamp_angles ();
  ideal_angles_.x = ystl::clamp (ideal_angles_.x, -89.0f, 89.0f);

  pev->v_angle = ideal_angles_;
  pev->v_angle.z = 0.0f;

  UpdateBodyAngles ();
}

bool Frustum::IsObjectInsidePlane (const Plane &plane, const ystl::Vector &center, float height, float radius) const {
  // calculate signed distance from center to plane
  const float dist = (plane.normal | center) + plane.result;

  // calculate conservative bounding sphere radius (diagonal of cylinder)
  const float bounding_radius = ystl::sqrtf (radius * radius + (height * 0.5f) * (height * 0.5f));

  // object is visible if any part lies near the positive plane side
  return dist + bounding_radius >= -16.0f;
}

void Frustum::Calculate (Planes &planes, const ystl::Vector &view_angle, const ystl::Vector &view_offset, float fov) const {
  ystl::Vector forward {}, right {}, up {};
  view_angle.angle_vectors (&forward, &right, &up);

  // honor the actual (possibly zoomed) field of view; fall back to 90 when unset
  if (fov <= 0.0f || fov > 170.0f) {
    fov = 90.0f;
  }

  const float tan_half = ystl::tanf (fov * ystl::deg2rad (1.0f) * 0.5f);
  const float far_width = 2.0f * tan_half * kMaxViewDistance;
  const float far_height = far_width / kAspectRatio;
  const float near_width = 2.0f * tan_half * kMinViewDistance;
  const float near_height = near_width / kAspectRatio;

  auto fc = view_offset + forward * kMaxViewDistance;
  auto nc = view_offset + forward * kMinViewDistance;

  auto up_half_far = up * far_height * 0.5f;
  auto right_half_far = right * far_width * 0.5f;
  auto up_half_near = up * near_height * 0.5f;
  auto right_half_near = right * near_width * 0.5f;

  auto ftl = fc - right_half_far + up_half_far;
  auto ftr = fc + right_half_far + up_half_far;
  auto fbl = fc - right_half_far - up_half_far;
  auto fbr = fc + right_half_far - up_half_far;
  auto ntl = nc - right_half_near + up_half_near;
  auto ntr = nc + right_half_near + up_half_near;
  auto nbl = nc - right_half_near - up_half_near;
  auto nbr = nc + right_half_near - up_half_near;

  auto set_plane = [&] (PlaneSide side, const ystl::Vector &v1, const ystl::Vector &v2, const ystl::Vector &v3) {
    auto &plane = planes[ystl::to_underlying (side)];

    // normals point inward (toward view volume)
    plane.normal = ((v2 - v1) ^ (v3 - v1)).normalize ();
    plane.result = -(plane.normal | v2);
  };

  set_plane (PlaneSide::Top, ftr, ntr, ntl);
  set_plane (PlaneSide::Bottom, fbl, nbl, nbr);
  set_plane (PlaneSide::Left, ftl, ntl, nbl);
  set_plane (PlaneSide::Right, fbr, nbr, ntr);
  set_plane (PlaneSide::Near, ntr, nbr, nbl);
  set_plane (PlaneSide::Far, ftl, fbl, fbr);
}

bool Frustum::Check (const Planes &planes, edict_t *ent) const {
  // validate entity
  if (!ent || ent->free) {
    return false;
  }
  const auto &origin = ent->v.origin;

  // use actual player dimensions based on stance
  const float height = (ent->v.flags & FL_DUCKING) ? 36.0f : 72.0f;
  const float radius = 16.0f;

  for (const auto &plane : planes) {
    if (!IsObjectInsidePlane (plane, origin, height, radius)) {
      return false;
    }
  }
  return true;
}

void Bot::SetAimDirection () {
  constexpr auto kStandingEyeOffset = ystl::Vector (0.0f, 0.0f, 17.0f);

  auto flags = aim_flags_;

  // don't allow bot to look at danger positions under certain circumstances
  if (!has_flag (flags, AimFlags::Grenade | AimFlags::Enemy | AimFlags::Entity)) {

    // check if narrow place and we're duck, do not predict enemies in that situation
    const bool ducked_in_narrow_place = IsInNarrowPlace () && (has_flag (path_flags_, NodeFlag::Crouch) || (pev->button & IN_DUCK));

    if (ducked_in_narrow_place || IsOnLadder () || IsInWater () || has_flag (path_flags_, NodeFlag::Ladder) ||
        has_flag (current_travel_flags_, PathFlag::Jump)) {

      flags &= ~(AimFlags::LastEnemy | AimFlags::PredictPath);
      can_set_aim_direction_ = false;
    }

    // don't switch view right away after loosing focus with current enemy
    if ((shoot_time_ + 1.0f > game.Time () || see_enemy_timer_.less_than (1.125f))

        && forget_last_victim_timer_.elapsed () && !last_enemy_origin_.empty () && game.IsPlayerEntity (last_enemy_) &&
        !game.IsPlayerEntity (enemy_)) {

      flags |= AimFlags::LastEnemy;
    }
  }

  // last victim: look at last kill position briefly (stop early to avoid snap-back)
  auto try_to_look_at_last_target = [&] (bool &target_set) {
    if (game.IsNullEntity (enemy_) && difficulty_ >= Difficulty::Normal && !forget_last_victim_timer_.elapsed () &&
        !last_victim_origin_.empty ()) {

      look_at_ = last_victim_origin_ + pev->view_ofs;
      target_set = true;
    }
  };

  if (has_flag (flags, AimFlags::Override)) {
    look_at_ = look_at_safe_;
  }
  else if (has_flag (flags, AimFlags::Grenade)) {
    look_at_ = throw_;

    const float distance = throw_.distance (pev->origin);
    const float height_diff = throw_.z - pev->origin.z;

    float coord_correction = 0.0f;

    if (distance >= 100.0f) {
      const float base_correction = 0.25f * height_diff;

      if (distance >= 800.0f) {
        const float clamped_dist = ystl::min (distance, 1200.0f);
        const float angle = ystl::min (37.0f * (clamped_dist - 800.0f) / 800.0f, 45.0f);

        coord_correction = clamped_dist * ystl::tanf (ystl::deg2rad (angle)) + base_correction;
      }
      else {
        coord_correction = base_correction;
      }
    }
    look_at_.z += coord_correction * 0.5f;
  }
  else if (has_flag (flags, AimFlags::Enemy)) {
    FocusEnemy ();
  }
  else if (has_flag (flags, AimFlags::Flash) && blind_timer_.elapsed () && ForcedLookAt ().empty ()) {
    look_at_ = avoid_flash_look_;
  }
  else if (has_flag (flags, AimFlags::Entity)) {
    look_at_ = entity_;

    // do not look at hostages legs
    if (pickup_type_ == Pickup::Hostage) {
      look_at_.z += 48.0f;
    }
    else if (pickup_type_ == Pickup::Weapon) {
      look_at_.z += 72.0f;
    }
  }
  else if (has_flag (flags, AimFlags::LastEnemy)) {
    look_at_ = last_enemy_origin_;

    // did bot just see enemy (or is holding a thru-wall engagement) and is quite aggressive?
    if (!thru_wall_hold_timer_.elapsed () || see_enemy_timer_.less_than (2.0f - actual_reaction_time_ + base_agression_level_)) {

      // feel free to fire if shootable
      if (!UsesSniper () && LastEnemyShootable ()) {
        wants_to_fire_ = true;
      }
    }
  }
  else if (has_flag (flags, AimFlags::PredictPath)) {
    bool change_predicted_enemy = true;

    if (next_tracking_timer_.elapsed () && tracking_edict_ == last_enemy_ && game.IsAliveEntity (last_enemy_)) {
      change_predicted_enemy = false;
    }

    auto do_fail_predict = [this] () -> void {
      if (last_predict_index_ != current_node_index_ && next_tracking_timer_.remaining_time () > -0.5f) {
        return; // do not fail instantly
      }
      aim_flags_ &= ~AimFlags::PredictPath;

      tracking_edict_ = nullptr;
      look_at_predict_.clear ();
    };

    auto path_length = last_predict_length_;
    auto predict_node = last_predict_index_;

    auto is_predicted_index_applicable = [&] () -> bool {
      if (!graph.Exists (predict_node)) {
        return false;
      }

      if (predict_cache_.is_valid) {
        float time_since_cache = game.Time () - predict_cache_.timestamp;
        float max_enemy_move = time_since_cache * 320.0f;

        if (last_enemy_origin_.distance_sq (predict_cache_.enemy_origin) > ystl::sqrf (max_enemy_move * 1.5f)) {
          predict_cache_.is_valid = false;
          return false;
        }
      }

      Trace::Result result {};
      trace.Line (GetEyesPos (), graph[predict_node].origin + pev->view_ofs, TraceIgnore::None, Ent (), &result);

      if (result.fraction < 0.5f) {
        return false;
      }
      const float dist_to_predict_node_sq = graph[predict_node].origin.distance_sq (pev->origin);

      if (dist_to_predict_node_sq >= ystl::sqrf (2048.0f) || dist_to_predict_node_sq <= ystl::sqrf (256.0f)) {
        return false;
      }

      if (!vistab.Visible (current_node_index_, predict_node) || !vistab.Visible (previous_nodes_[0], predict_node)) {
        predict_node = kInvalidNodeIndex;
        path_length = kInfiniteDistanceLong;

        return false;
      }
      return IsNodeValidForPredict (predict_node) && path_length < cv_max_nodes_for_predict.As<int> () &&
             NumEnemiesNear (graph[predict_node].origin, 1024.0f) > 0;
    };

    if (change_predicted_enemy) {
      if (is_predicted_index_applicable ()) {
        look_at_predict_ = graph[predict_node].origin;

        next_tracking_timer_.start (0.75f);
        tracking_edict_ = last_enemy_;
      }
      else {
        do_fail_predict ();
      }
    }
    else {
      if (!is_predicted_index_applicable ()) {
        do_fail_predict ();
      }
    }

    if (!look_at_predict_.empty ()) {
      look_at_ = look_at_predict_;
    }
  }
  else if (has_flag (flags, AimFlags::Camp)) {
    bool target_set = false;

    // do not snap view from last victim
    try_to_look_at_last_target (target_set);

    if (!target_set) {
      look_at_ = look_at_safe_;
    }
  }
  else if (has_flag (flags, AimFlags::Nav)) {
    const ystl::Vector dest_origin = dest_origin_ + pev->view_ofs;
    const bool on_ladder_move = has_flag (path_flags_, NodeFlag::Ladder) || IsOnLadder () || IsPreviousLadder ();

    bool vertical_move = on_ladder_move;

    if (on_ladder_move || pev->velocity.z > 16.0f) {
      vertical_move_hold_.start (0.2f);
      vertical_move = true;
    }
    else if (!vertical_move_hold_.elapsed ()) {
      vertical_move = true;
    }

    bool target_set = false;

    // danger: look at known danger node if far enough
    if (bots.EnemySpotted () && num_enemies_left_ > 0 && can_set_aim_direction_ && see_enemy_timer_.greater_than (4.0f) &&
        graph.Exists (current_node_index_) && !has_flag (aim_flags_, AimFlags::PredictPath)) {

      int danger_index = kInvalidNodeIndex;

      // keep running the committed glance while it stays valid
      if (!danger_glance_.elapsed () && graph.Exists (danger_glance_node_) && !has_flag (graph[danger_glance_node_].flags, NodeFlag::Crouch) &&
          vistab.VisibleBothSides (current_node_index_, danger_glance_node_) &&
          pev->origin.distance_sq (graph[danger_glance_node_].origin) >= ystl::sqrf (240.0f)) {

        danger_index = danger_glance_node_;
      }
      else if (danger_glance_gap_.elapsed ()) {
        // idle: try to start a new glance
        danger_index = practice.GetIndex (team_, current_node_index_, current_node_index_);

        if (graph.Exists (danger_index) && vistab.VisibleBothSides (current_node_index_, danger_index) &&
            !has_flag (graph[danger_index].flags, NodeFlag::Crouch) &&
            pev->origin.distance_sq (graph[danger_index].origin) >= ystl::sqrf (240.0f)) {

          danger_glance_node_ = danger_index;

          // base glance time, held longer for distant danger spots
          const float distance = pev->origin.distance (graph[danger_index].origin);
          const float scaled = ystl::clamp ((distance - 240.0f) / (2048.0f - 240.0f), 0.0f, 1.0f);

          // hotspot boost: the closer the chosen direction is to the team's hottest spot, the longer the stare
          const float row_damage = static_cast<float> (practice.GetDamage (team_, current_node_index_, danger_index));
          const float hotspot = ystl::clamp (row_damage / practice.GetTeamDamage<float> (team_), 0.0f, 1.0f);

          const float duration = (2.2f + scaled * 1.4f + hotspot * 2.0f) * rg (0.9f, 1.15f);

          danger_glance_.start (duration);
          danger_glance_gap_.start (duration + rg (2.0f, 4.0f)); // glance duration + forward looking gap
        }
        else {
          danger_index = kInvalidNodeIndex;
        }
      }

      if (graph.Exists (danger_index)) {
        look_at_ = graph[danger_index].origin + kStandingEyeOffset;
        aim_flags_ |= AimFlags::Danger;

        target_set = true;
      }
    }

    // lookahead: look ahead on path
    if (!target_set && !vertical_move && move_to_goal_ && can_set_aim_direction_ && IsOnFloor () && !IsDucking () &&
        graph.Exists (current_node_index_) && path_walk_.HasNext () && pev->origin.distance_sq (dest_origin) < ystl::sqrf (384.0f) &&
        path_->radius >= 16.0f && path_->flags == 0 && graph[path_walk_.Next ()].flags == 0) {

      const auto next_path_index = path_walk_.Next ();

      if (graph.Exists (next_path_index) && ystl::abs (graph[next_path_index].origin.z - path_origin_.z) < 8.0f) {

        const auto is_narrow_place = IsInNarrowPlace ();

        if (path_walk_.Length () > 2 && !is_narrow_place) {
          const auto next_path_index_x2 = path_walk_.NextX2 ();

          // prefer two-node lookahead, fall back to one-node if not visible
          if (vistab.VisibleBothSides (current_node_index_, next_path_index_x2)) {
            look_at_ = graph[next_path_index_x2].origin + pev->view_ofs;
          }
          else {
            look_at_ = graph[next_path_index].origin + pev->view_ofs;
          }
          target_set = true;
        }
        else if (!is_narrow_place) {
          look_at_ = graph[next_path_index].origin + pev->view_ofs;
          target_set = true;
        }
      }
    }

    // ladder: look at next ladder node when climbing/descending
    if (!target_set && vertical_move) {
      if (path_walk_.HasNext ()) {
        const auto &next_path = graph[path_walk_.Next ()];

        if (has_flag (next_path.flags, NodeFlag::Ladder) && dest_origin_.distance_sq (pev->origin) < ystl::sqrf (96.0f)) {
          if (next_path.origin.z > path_origin_.z + 26.0f) {

            // ascending: look up at next ladder node
            look_at_ = next_path.origin + pev->view_ofs;
            target_set = true;
          }
          else if (next_path.origin.z < path_origin_.z - 26.0f) {

            // descending: look down at next ladder node
            look_at_ = next_path.origin + pev->view_ofs;
            target_set = true;
          }
        }
      }

      // ladder: look forward toward destination when climbing/descending
      if (!target_set && IsOnLadder () && IsPreviousLadder () && !has_flag (path_flags_, NodeFlag::Ladder)) {

        // look toward destination instead of backward at previous node to prevent climbing backwards
        if (ladder_dir_ == LadderDir::Down) {

          // descending: look at a point in front and below, closer to current position
          ystl::Vector forward {};
          move_angles_.angle_vectors (&forward, nullptr, nullptr);

          look_at_ = pev->origin + forward * 48.0f;
          look_at_.z = pev->origin.z - 24.0f;
        }
        else {
          // ascending: look toward destination
          look_at_ = dest_origin_;
          look_at_.z = ystl::max (dest_origin_.z, pev->origin.z) + 16.0f;
        }
        target_set = true;
      }
    }

    // persist last valid target across toggles, else fall back
    if (target_set) {
      nav_look_at_ = look_at_;
      nav_look_at_node_ = current_node_index_;
    }
    else if (!nav_look_at_.empty () && nav_look_at_node_ == current_node_index_) {
      look_at_ = nav_look_at_;
    }
    else {
      look_at_ = dest_origin;

      if (!vertical_move) {
        look_at_.z = GetEyesPos ().z;
      }
    }
    try_to_look_at_last_target (target_set);
  }

  if (look_at_.empty ()) {
    look_at_ = dest_origin_;
  }
}

} // namespace bot
