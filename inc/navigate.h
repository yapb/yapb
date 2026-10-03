//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// collision states - response states (0-3) must come first for array indexing
namespace bot {

enum class CollisionState : uint32_t {
  Jump = 0,
  StrafeLeft = 1,
  StrafeRight = 2,
  Duck = 3,
  Undecided = 4,
  Probing = 5,
  NoMove = 6
};

// collision probes
enum class CollisionProbe : uint32_t {
  Jump = ystl::bit (0), // probe jump when colliding
  Duck = ystl::bit (1), // probe duck when colliding
  Strafe = ystl::bit (2), // probe strafing when colliding
  None = 0
};
YSTL_ENABLE_ENUM_FLAGS (CollisionProbe);

// goal tactic
enum class GoalTactic : int32_t {
  Defensive = 0,
  Camp,
  Offensive,
  Goal,
  RescueHostage
};

// ladder move direction
enum class LadderDir : int32_t {
  Down,
  Up
};

// door block type - what is blocking the door
enum class DoorBlockType : uint8_t {
  None = 0, // nothing blocking
  Teammate, // teammate on other side
  Enemy, // enemy on other side
  Obstacle // door stuck closed or other obstruction
};

// helper structure for collision state weighting
struct CollisionWeight {
  CollisionState state {};
  int32_t weight {};
};

// this structure links nodes returned from pathfinder
class PathWalk final : public ystl::NonCopyable {
  friend struct NavigateHook;

private:
  size_t cursor_ {};
  size_t size_ {};
  size_t capacity_ {};

  ystl::UniquePtr<int32_t[]> path_ {};

public:
  explicit PathWalk () = default;
  ~PathWalk () = default;

public:
  // access element at relative offset from cursor (no bounds check - use when sure it's safe)
  int32_t &At (size_t index) {
    return path_[cursor_ + index];
  }

  // get first node in current path
  int32_t &First () {
    return At (0);
  }

  // get second node in current path (next to visit) - requires hasnext() to be true
  int32_t &Next () {
    return At (1);
  }

  // get third node in current path (two steps ahead) - requires hasnextx2() to be true
  int32_t &NextX2 () {
    return At (2);
  }

  // get last node in current path - requires length() > 0
  int32_t &Last () {
    return At (Length () - 1);
  }

  // advance cursor by one position
  void Shift () {
    if (cursor_ < size_) {
      ++cursor_;
    }
  }

  // reverse the path in place (resets cursor to beginning)
  void Reverse () {
    for (size_t i = 0; i < size_ / 2; ++i) {
      ystl::swap (path_[i], path_[size_ - 1 - i]);
    }
    cursor_ = 0;
  }

  // get remaining path length (cached computation)
  size_t Length () const {
    if (cursor_ >= size_) {
      return 0;
    }
    return size_ - cursor_;
  }

  // check if there are at least 2 nodes remaining (can call next() safely)
  bool HasNext () const {
    return size_ - cursor_ > 1;
  }

  // check if there are at least 3 nodes remaining (can call nextx2() safely)
  bool HasNextX2 () const {
    return size_ - cursor_ > 2;
  }

  // check if path is empty
  bool Empty () const {
    return cursor_ >= size_;
  }

  // add node to path
  void Add (int32_t node) {
    if (path_ && size_ < capacity_) {
      path_[size_++] = node;
    }
  }

  // clear the path (reset cursor and length)
  void Clear () {
    cursor_ = 0;
    size_ = 0;

    if (path_ && capacity_ > 0) {
      path_[0] = 0;
    }
  }

  // reset path for a new build (worker-side staging usage)
  void ResetForBuild () {
    cursor_ = 0;
    size_ = 0;
  }

  // copy all nodes from other path (main-thread apply of published path)
  void CopyFrom (const PathWalk &other) {
    if (other.size_ > capacity_) {
      return; // should never happen, capacities are equal
    }
    cursor_ = 0;
    size_ = other.size_;

    for (size_t i = 0; i < size_; ++i) {
      path_[i] = other.path_[i];
    }
  }

  // initialize path with specified capacity
  void Init (size_t length) {
    capacity_ = ystl::min (length, static_cast<size_t> (kMaxNodes));
    path_ = ystl::make_unique<int32_t[]> (capacity_);

    cursor_ = 0;
    size_ = 0;
  }

  // get total capacity of path buffer
  size_t Capacity () const {
    return capacity_;
  }
};

// bot navigation data mixin
class NavigateData {
  friend class DebugPanel;
  friend struct NavigateHook;
  friend struct PracticeHook;
  friend struct BehaviorHook;
  friend struct CombatHook;
  friend struct VisionHook;
  friend struct TasksHook;
  friend struct WeaponsHook;

public:
  using CollisionWeights = ystl::FixedArray<CollisionWeight, kNumCollisionResponses>;

  // metadata of a pathfinding result (synchronous, main thread)
  struct PathMeta {
    bool has_path {}; // build buffer contains valid path to apply
    bool invalidate_prev_goal {}; // reset previous goal index
    bool invalidate_goal_task {}; // reset goal task data
    bool update_goal_task {}; // retarget goal task data to goalTaskIndex
    int chosen_goal_index {}; // goal chosen by pathfinder
    int prev_goal_index {}; // new previous goal index (valid when invalidateprevgoal is set)
    int goal_task_index {}; // new goal task data (valid when updategoaltask is set)
    float goal_value {}; // goal ranking value
    float repath_delay {}; // repath timer start delay (zero means don't touch timer)
  };

protected:
  // scratch buffer for path builds, reused between searches to avoid allocations
  PathWalk build_path_ {};

  ystl::FixedArray<CollisionState, kNumCollisionResponses> collide_moves_ {}; // sorted array of movements
  uint32_t coll_state_index_ {}; // index into collide moves
  PathFlag current_travel_flags_ {}; // connection flags like jumping

  int loosed_bomb_node_index_ {}; // nearest to loosed bomb node
  int planted_bomb_node_index_ {}; // nearest to planted bomb node
  int current_node_index_ {}; // current node index
  int travel_start_index_ {}; // travel start index to double jump action
  ystl::FixedArray<int32_t, 5> previous_nodes_ {}; // previous node indexes from node find
  NodeFlag path_flags_ {}; // current node flags
  int need_avoid_grenade_ {}; // which direction to strafe away
  int camp_direction_ {}; // camp facing direction
  int camp_buttons_ {}; // buttons to press while camping
  int try_open_door_ {}; // attempt's to open the door
  int door_block_count_ {}; // counter for door blocking attempts
  DoorBlockType door_block_type_ {}; // type of door blocker
  LiftState lift_state_ {}; // state of lift handling
  LadderDir ladder_dir_ {}; // ladder move direction
  int rechoice_goal_count_ {}; // multiple failed goals?

  ystl::CountdownTimer first_collide_timer_ {}; // time of first collision
  ystl::CountdownTimer probe_timer_ {}; // time of probing different moves
  ystl::CountdownTimer last_coll_timer_ {}; // time until next collision check
  ystl::CountdownTimer door_open_timer_ {}; // time to next door open check
  ystl::CountdownTimer door_hit_timer_ {}; // specific time after hitting the door
  ystl::IntervalTimer door_blocked_timer_ {}; // time when door was first blocked
  ystl::IntervalTimer nav_timer_ {}; // time node chosen by bot
  ystl::IntervalTimer last_used_nodes_timer_ {}; // last time bot followed nodes
  float time_camping_ {}; // time to camp
  ystl::CountdownTimer next_camp_dir_timer_ {}; // time next camp direction change
  ystl::CountdownTimer button_push_timer_ {}; // time to push the button
  ystl::CountdownTimer lift_usage_timer_ {}; // time to use lift
  float jump_time_ {}; // time last jump happened

  bool move_to_goal_ {}; // bot currently moving to goal??
  ystl::CountdownTimer stuck_timer_ {}; // latched stuck state, no flap frame to frame
  bool jump_finished_ {}; // has bot finished jumping
  bool check_terrain_ {}; // check for terrain
  bool bomb_search_overridden_ {}; // use normal node if applicable when near the bomb
  bool jump_sequence_ {}; // next path link will be jump link
  bool jump_knife_drawn_ {}; // navigation drew knife for the jump link, see drawknifeforjump ()
  ystl::CountdownTimer jump_knife_restore_timer_ {}; // earliest moment to return best weapon after the jump
  bool is_fall_down_ {}; // is it falling?
  bool bot_movement_ {}; // bot movement allowed ?

  PathWalk path_walk_ {}; // pointer to current node from path
  CollisionState collision_state_ {}; // collision state
  FindPathType path_type_ {}; // which pathfinder to use
  Dodge dodge_strafe_dir_ {}; // direction to strafe
  ystl::UniquePtr<class AStarAlgo> planner_ {};

  edict_t *lift_entity_ {}; // pointer to lift entity
  edict_t *avoid_grenade_ {}; // pointer to grenade entity to avoid
  edict_t *hindrance_ {}; // the hindrance

  float avoid_strafe_dir_ {}; // committed player avoidance strafe direction
  ystl::CountdownTimer avoid_commit_timer_ {}; // time until which avoidance direction is committed

  ystl::CountdownTimer avoid_strafe_change_timer_ {}; // timer for terrain avoidance strafe direction change
  float terrain_strafe_dir_ {}; // current strafe direction for terrain avoidance (+1 or -1)

  float stuck_accumulated_time_ {}; // cumulative time spent moving too little
  ystl::IntervalTimer combat_stuck_check_timer_ {}; // last frame combat strafe was verified
  ystl::Vector last_combat_strafe_origin_ {}; // position when combat strafe was last set

  ystl::Vector lift_travel_pos_ {}; // lift travel position
  ystl::Vector dest_origin_ {}; // origin of move destination
  ystl::Vector path_origin_ {}; // origin of node
  ystl::Vector move_angles_ {}; // bot move angles
  ystl::Vector desired_velocity_ {}; // desired velocity for jump nodes
  ystl::Vector fall_down_point_[2] {}; // falling point

  Path *path_ {}; // pointer to the current path node

  ystl::CountdownTimer approaching_ladder_timer_ {}; // bot is approaching ladder
  ystl::CountdownTimer lost_reachable_node_timer_ {}; // bot's issuing next node, probably he's lost
  ystl::CountdownTimer fix_fall_timer_ {}; // timer we're fixed fall last time
  ystl::CountdownTimer repath_timer_ {}; // bots is going to repath his route
  ystl::CountdownTimer path_enqueue_timer_ {}; // throttles repeated path rebuilds

public:
  // is the bot currently stuck
  bool IsStuckState () const {
    return !stuck_timer_.elapsed ();
  }

  // cumulative time the bot spent stuck
  float StuckTime () const {
    return stuck_accumulated_time_;
  }

  // nodes left in the current path
  int RemainingPathLength () const {
    return static_cast<int> (path_walk_.Length ());
  }

  // node at offset from the current path cursor
  int PathNodeAt (int offset) {
    if (offset < 0 || static_cast<size_t> (offset) >= path_walk_.Length ()) {
      return kInvalidNodeIndex;
    }
    return path_walk_.At (static_cast<size_t> (offset));
  }

  float goal_value_ {}; // ranking value for this node
  ystl::CountdownTimer fall_down_timer_ {}; // time bot started to fall
  float duck_for_jump_ {}; // is bot needed to duck for double jump

  int prev_goal_index_ {}; // holds destination goal node
  int chosen_goal_index_ {}; // used for experience, same as above

  bool jump_ready_ {}; // is double jump ready

  edict_t *double_jump_entity_ {}; // pointer to entity that request double jump
  ystl::Vector double_jump_origin_ {}; // origin of double jump
  ystl::Deque<int32_t> goal_history_ {};
};

} // namespace bot
