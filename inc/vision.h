//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

#include "constant.h"

// view frustum for bots
namespace bot {

class Frustum : public ystl::Singleton<Frustum> {
public:
  struct Plane {
    ystl::Vector normal {};
    float result {}; // d coefficient: -(normal · point)
  };

  enum class PlaneSide : int32_t {
    Top = 0,
    Bottom,
    Left,
    Right,
    Near,
    Far,
    Num
  };

public:
  using Planes = Plane[ystl::to_underlying (PlaneSide::Num)];

public:
  static constexpr float kFov = 75.0f;
  static constexpr float kAspectRatio = 16.0f / 9.0f;

  static constexpr float kMaxViewDistance = 4096.0f;
  static constexpr float kMinViewDistance = 10.0f;

public:
  explicit Frustum () = default;

public:
  // updates bot view frustum, honoring the actual (possibly zoomed) field of view
  void Calculate (Planes &planes, const ystl::Vector &view_angle, const ystl::Vector &view_offset, float fov = 90.0f) const;

  // check if object inside frustum plane
  bool IsObjectInsidePlane (const Plane &plane, const ystl::Vector &center, float height, float radius) const;

  // check if entity origin inside view plane
  bool Check (const Planes &planes, edict_t *ent) const;
};

// declare global frustum data
YSTL_EXPOSE_GLOBAL_SINGLETON (Frustum, frustum);

// prediction cache for tracking enemies
struct PredictCache {
  int node_index { kInvalidNodeIndex };
  int path_length { kInfiniteDistanceLong };
  ystl::Vector position {};
  ystl::Vector enemy_origin {};
  float timestamp {};
  float valid_until {};
  bool is_valid { false };

public:
  bool IsStillValid (const ystl::Vector &current_enemy_origin, float current_time) const {
    if (!is_valid || current_time >= valid_until) {
      return false;
    }
    const float max_move = (current_time - timestamp) * 320.0f;

    return enemy_origin.distance_sq (current_enemy_origin) <= ystl::sqrf (max_move);
  }
};

// per-axis critically damped spring-damper for aim angle tracking
struct AimConfig {
  float stiffness {}; // deg/s^2 per degree of error
  float damping {}; // deg/s per unit velocity; 2*sqrt(stiffness) is critical
  float max_accel {}; // deg/s^2
  float snap_angle {}; // deg, snap to target within this window (kills residual velocity)
};

class AimSpring {
public:
  float velocity {}; // current angular velocity (deg/s)

public:
  void Reset () {
    velocity = 0.0f;
  }

  // feed signed error (shortest path) and a base angle; returns the new angle
  float Update (const AimConfig &cfg, float error, float current, float delta) {

    // snap to target when aiming error is negligible
    if (ystl::abs (error) < cfg.snap_angle) {
      velocity = 0.0f;
      return ystl::wrap_angle (current + error);
    }
    const float accel = ystl::clamp (cfg.stiffness * error - cfg.damping * velocity, -cfg.max_accel, cfg.max_accel);

    velocity += delta * accel;
    return ystl::wrap_angle (current + delta * velocity);
  }
};

// bot vision and aiming data mixin
class VisionData {
  friend class DebugPanel;
  friend struct TestHook;
  friend struct VisionHook;

protected:
  int last_predict_index_ {}; // last predicted path index
  int last_predict_length_ {}; // last predicted path length

  PredictCache predict_cache_ {}; // predict cached

  ystl::CountdownTimer predict_enqueue_timer_ {}; // throttles repeated prediction recalculation

  AimSpring look_yaw_ {}; // yaw aim spring
  AimSpring look_pitch_ {}; // pitch aim spring
  float look_update_time_ {}; // lookangles update time
  ystl::CountdownTimer aim_error_timer_ {}; // time to update error vector

  Frustum::Planes view_frustum_ {};

  ystl::Vector ideal_angles_ {}; // angle wanted
  ystl::Vector aim_last_error_ {}; // last calculated aim error
  ystl::Vector look_at_ {}; // vector bot should look at
  ystl::Vector look_at_safe_ {}; // aiming vector when camping
  ystl::Vector look_at_predict_ {}; // aiming vector when predicting
  ystl::Vector entity_ {}; // origin of entities like buttons etc

public:
  // origin the bot currently aims at
  ystl::Vector &LookAtVector () {
    return look_at_;
  }

  float view_distance_ {}; // current view distance
  float max_view_distance_ {}; // maximum view distance
  ystl::CountdownTimer next_tracking_timer_ {}; // time node index for tracking player is recalculated
  ystl::CountdownTimer check_dark_timer_ {}; // check for darkness time

  int flash_level_ {}; // flashlight level
  bool can_set_aim_direction_ {}; // can choose aiming direction
  edict_t *tracking_edict_ {}; // pointer to last tracked player when camping/hiding

  // nav target persistence: reuse last valid target when unstable conditions toggle frame-to-frame
  ystl::Vector nav_look_at_ {}; // last valid nav lookat target
  int nav_look_at_node_ { kInvalidNodeIndex }; // current node when target was set (invalidates on node change)
  ystl::CountdownTimer vertical_move_hold_ {}; // holds vertical-move state to avoid frame-to-frame flicker on stairs/slopes

  // committed danger node to keep the glance from flickering
  int danger_glance_node_ { kInvalidNodeIndex }; // committed danger node for the current glance
  ystl::CountdownTimer danger_glance_ {}; // runs while the glance is active
  ystl::CountdownTimer danger_glance_gap_ {}; // covers the glance itself plus the forward-looking gap after it
};

} // namespace bot
