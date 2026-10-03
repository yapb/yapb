//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

float PlannerHeuristicDiversity::G (Team, [[maybe_unused]] int current_index, int parent_index, int edge_distance) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // random weight between 0.5 and 1.5 for variety, edge distance comes straight from the caller
  return static_cast<float> (edge_distance) * ystl::rg (0.5f, 1.5f);
}

float PlannerHeuristicDiversity::H (int index, int goal_index) const {
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float dx = start.origin.x - goal.origin.x;
  const float dy = start.origin.y - goal.origin.y;
  const float dz = start.origin.z - goal.origin.z;

  // base euclidean distance with random weight between 0.3 and 1.0
  const float base_dist = ystl::sqrtf (ystl::sqrf (dx) + ystl::sqrf (dy) + ystl::sqrf (dz));

  return base_dist * ystl::rg (0.3f, 1.0f);
}

float PlannerHeuristicFast::G (Team, int current_index, int parent_index, int edge_distance) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // we don't like ladder or crouch point
  if (has_flag (graph[current_index].flags, NodeFlag::Crouch | NodeFlag::Ladder)) {
    return static_cast<float> (edge_distance) * 1.5f;
  }
  return static_cast<float> (edge_distance);
}

float PlannerHeuristicFast::H (int index, int goal_index) const {
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    return ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance

  case 1:
    return ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    return (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
  }

  default:
  case 4:
    return ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
  }
}

float PlannerHeuristicFastHostage::G (Team, int current_index, int parent_index, int edge_distance) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // hostages cannot use no-hostage nodes or jump links
  if (graph.Exists (parent_index) && graph.Exists (current_index)) {
    if (has_flag (graph[current_index].flags, NodeFlag::NoHostage)) {
      return kInfiniteHeuristic;
    }
    for (const auto &link : graph[parent_index].links) {
      if (link.index == current_index && has_flag (link.flags, PathFlag::Jump)) {
        return kInfiniteHeuristic;
      }
    }
  }
  if (has_flag (graph[current_index].flags, NodeFlag::Crouch | NodeFlag::Ladder)) {
    return static_cast<float> (edge_distance) * 1.5f * 5.0f;
  }
  return static_cast<float> (edge_distance);
}

float PlannerHeuristicFastHostage::H (int index, int goal_index) const {
  if (has_flag (graph[index].flags, NodeFlag::NoHostage)) {
    return kInfiniteHeuristic;
  }
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    return ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance

  case 1:
    return ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    return (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
  }

  default:
  case 4:
    return ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
  }
}

float PlannerHeuristicOptimal::G (Team team, int current_index, int parent_index, int) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // only cost of current node, not its neighbors (fixes a* optimality)
  auto cost = practice.GetDamageEx (team, current_index, current_index, true);

  if (has_flag (graph[current_index].flags, NodeFlag::Crouch)) {
    cost *= 1.5f;
  }
  return cost;
}

float PlannerHeuristicOptimal::H (int index, int goal_index) const {
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    return ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance

  case 1:
    return ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    return (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
  }

  default:
  case 4:
    return ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
  }
}

float PlannerHeuristicOptimalHostage::G (Team team, int current_index, int parent_index, int) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // hostages cannot use no-hostage nodes or jump links
  if (graph.Exists (parent_index) && graph.Exists (current_index)) {
    if (has_flag (graph[current_index].flags, NodeFlag::NoHostage)) {
      return kInfiniteHeuristic;
    }
    for (const auto &link : graph[parent_index].links) {
      if (link.index == current_index && has_flag (link.flags, PathFlag::Jump)) {
        return kInfiniteHeuristic;
      }
    }
  }
  auto cost = practice.GetDamageEx (team, current_index, current_index, true);

  if (has_flag (graph[current_index].flags, NodeFlag::Crouch)) {
    cost *= 1.5f;
  }
  if (has_flag (graph[current_index].flags, NodeFlag::Crouch | NodeFlag::Ladder)) {
    return cost * 5.0f;
  }
  return cost;
}

float PlannerHeuristicOptimalHostage::H (int index, int goal_index) const {
  if (has_flag (graph[index].flags, NodeFlag::NoHostage)) {
    return kInfiniteHeuristic;
  }
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    return ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance

  case 1:
    return ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    return (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
  }

  default:
  case 4:
    return ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
  }
}

float PlannerHeuristicSafe::G (Team team, int current_index, int, int) const {
  // only cost of current node, not its neighbors (fixes a* optimality)
  auto cost = practice.GetDamageEx (team, current_index, current_index, false);

  if (has_flag (graph[current_index].flags, NodeFlag::Crouch)) {
    cost *= 1.5f;
  }
  return cost;
}

float PlannerHeuristicSafe::H (int index, int goal_index) const {
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;
  float base = 0.0f;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    base = ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance
    break;

  case 1:
    base = ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance
    break;

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    base = (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
    break;
  }

  default:
  case 4:
    base = ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
    break;
  }
  return base / (128.0f * 10.0f);
}

float PlannerHeuristicSafeHostage::G (Team team, int current_index, int parent_index, int) const {
  if (parent_index == kInvalidNodeIndex) {
    return 0.0f;
  }
  // hostages cannot use no-hostage nodes or jump links
  if (graph.Exists (parent_index) && graph.Exists (current_index)) {
    if (has_flag (graph[current_index].flags, NodeFlag::NoHostage)) {
      return kInfiniteHeuristic;
    }
    for (const auto &link : graph[parent_index].links) {
      if (link.index == current_index && has_flag (link.flags, PathFlag::Jump)) {
        return kInfiniteHeuristic;
      }
    }
  }
  auto cost = practice.GetDamageEx (team, current_index, current_index, false);

  if (has_flag (graph[current_index].flags, NodeFlag::Crouch)) {
    cost *= 1.5f;
  }
  if (has_flag (graph[current_index].flags, NodeFlag::Crouch | NodeFlag::Ladder)) {
    return cost * 5.0f;
  }
  return cost;
}

float PlannerHeuristicSafeHostage::H (int index, int goal_index) const {
  const auto &start = graph[index];
  const auto &goal = graph[goal_index];

  const float x = start.origin.x - goal.origin.x;
  const float y = start.origin.y - goal.origin.y;
  const float z = start.origin.z - goal.origin.z;
  float base = 0.0f;

  switch (cv_path_heuristic_mode.As<int> ()) {
  case 0:
    base = ystl::max (ystl::max (ystl::abs (x), ystl::abs (y)), ystl::abs (z)); // chebyshev distance
    break;

  case 1:
    base = ystl::abs (x) + ystl::abs (y) + ystl::abs (z); // manhattan distance
    break;

  case 2:
    return 0.0f; // no heuristic

  case 3: {
    const float dx = ystl::abs (x);
    const float dy = ystl::abs (y);
    const float dz = ystl::abs (z);

    const float dmin = ystl::min (ystl::min (dx, dy), dz);
    const float dmax = ystl::max (ystl::max (dx, dy), dz);
    const float dmid = dx + dy + dz - dmin - dmax;

    const float d1 = 1.0f;
    const float d2 = ystl::sqrtf (2.0f);
    const float d3 = ystl::sqrtf (3.0f);

    base = (d3 - d2) * dmin + (d2 - d1) * dmid + d1 * dmax; // diagonal distance
    break;
  }

  default:
  case 4:
    base = ystl::sqrtf (ystl::sqrf (x) + ystl::sqrf (y) + ystl::sqrf (z)); // euclidean distance
    break;
  }
  return base / (128.0f * 10.0f);
}

void AStarAlgo::ClearRoute () {
  if (!routes_.resize (static_cast<size_t> (size_))) {
    routes_.clear ();
    return;
  }

  for (int i = 0; i < size_; ++i) {
    auto route = &routes_[i];

    route->g = route->f = 0.0f;
    route->parent = kInvalidNodeIndex;
    route->epoch = 0;
    route->state = RouteState::New;
  }
}

bool AStarAlgo::CantSkipNode (const int a, const int b, bool skip_vis_check) {
  // never smooth a shortcut through a closed mode wall (vistable doesn't know about them)
  if (mode_walls.HasWalls () && mode_walls.IsSegmentBlocked (graph[a].origin, graph[b].origin)) {
    return true;
  }
  const auto &ag = graph[a];
  const auto &bg = graph[b];

  const bool has_zero_radius = ystl::fzero (ag.radius) || ystl::fzero (bg.radius);

  if (has_zero_radius) {
    return true;
  }

  if (!skip_vis_check) {
    const bool not_visible = !vistab.VisibleBothSides (ag.number, bg.number);

    if (not_visible) {
      return true;
    }
  }
  const bool too_high = ystl::abs (ag.origin.z - bg.origin.z) > 17.0f;

  if (too_high) {
    return true;
  }
  const bool too_narrow = has_flag ((ag.flags | bg.flags), NodeFlag::Narrow);

  if (too_narrow) {
    return true;
  }
  const float distance_sq = ag.origin.distance_sq (bg.origin);

  const bool too_far = distance_sq > ystl::sqrf (400.0f);
  const bool too_close = distance_sq < ystl::sqrf (40.0f);

  if (too_far || too_close) {
    return true;
  }
  for (const auto &link : ag.links) {
    if (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump)) {
      return true;
    }
  }

  for (const auto &link : bg.links) {
    if (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump)) {
      return true;
    }
  }
  return false;
}

void AStarAlgo::PostSmooth (NodeAdderFn on_added_node) {
  smoothed_path_.clear ();

  if (constructed_path_.size () <= 2) {
    for (const auto &node : constructed_path_) {
      smoothed_path_.push (node);
    }
  }
  else {
    int index = 0;
    smoothed_path_.push (constructed_path_.first ());

    for (size_t i = 1; i < constructed_path_.size () - 1; ++i) {
      if (CantSkipNode (smoothed_path_[index], constructed_path_[i + 1])) {
        ++index;
        smoothed_path_.push (constructed_path_[i]);
      }
    }
    smoothed_path_.push (constructed_path_.last ());
  }

  for (const auto &spn : smoothed_path_) {
    on_added_node (spn);
  }
}

AStarResult AStarAlgo::Find (Team bot_team, int src_index, int dest_index, NodeAdderFn on_added_node) {
  if (size_ < kMaxNodeLinks) {
    return AStarResult::InternalError; // astar needs some nodes to work with
  }

  if (heuristic_ == nullptr) {
    return AStarResult::InternalError;
  }

  if (src_index < 0 || src_index >= size_ || dest_index < 0 || dest_index >= size_) {
    return AStarResult::Failed;
  }

  if (!graph.Exists (src_index) || !graph.Exists (dest_index)) {
    return AStarResult::Failed;
  }

  if (routes_.size () != static_cast<size_t> (size_) && !routes_.resize (static_cast<size_t> (size_))) {
    return AStarResult::InternalError;
  }

  // bump the search stamp, hard resetting everything only on counter wrap
  if (++epoch_ == 0) {
    ClearRoute ();
    epoch_ = 1;
  }

  if (src_index == dest_index) {
    on_added_node (src_index);
    return AStarResult::Success;
  }

  auto src_route = RouteAt (src_index);
  const float src_h = heuristic_->H (src_index, dest_index);

  // put start node into open list
  src_route->g = heuristic_->G (bot_team, src_index, kInvalidNodeIndex, 0);
  src_route->f = src_route->g + src_h;
  src_route->state = RouteState::Open;

  route_que_.clear ();
  route_que_.emplace (src_index, src_route->f);

  const bool post_smooth_path = cv_path_astar_post_smooth && vistab.IsReady ();

  // always clear constructed path
  constructed_path_.clear ();

  size_t expanded_nodes = 0;

  while (!route_que_.empty ()) {
    // remove the first node from the open list
    int current_index = route_que_.pop ().index;

    // safety guard against corrupted graphs and runaway expansion loops
    if (!graph.Exists (current_index) || ++expanded_nodes > static_cast<size_t> (size_ * kMaxNodeLinks)) {
      route_que_.clear ();

      // infrom pathfinder to use floyds in that case
      planner.SetPathsCheckFailed (true);

      return AStarResult::InternalError;
    }
    auto cur_route = RouteAt (current_index);

    // skip if already processed (duplicate in queue from re-expansion)
    if (cur_route->state == RouteState::Closed) {
      continue;
    }

    // mark as closed immediately to prevent re-expansion
    cur_route->state = RouteState::Closed;

    // is the current node the goal node?
    if (current_index == dest_index) {
      // build the complete path
      do {
        if (post_smooth_path) {
          constructed_path_.push (current_index);
        }
        else {
          on_added_node (current_index);
        }
        current_index = routes_[current_index].parent;
      } while (current_index != kInvalidNodeIndex);

      // do a post-smooth if requested
      if (post_smooth_path) {
        PostSmooth (on_added_node);
      }
      return AStarResult::Success;
    }

    // now expand the current node
    for (const auto &child : graph[current_index].links) {
      if (child.index < 0 || child.index >= size_) {
        continue;
      }

      // skip links crossing a closed mode wall
      if (mode_walls.IsSegmentBlocked (graph[current_index].origin, graph[child.index].origin)) {
        continue;
      }
      auto child_route = RouteAt (child.index);

      const float edge_cost = heuristic_->G (bot_team, child.index, current_index, child.distance);
      float g = cur_route->g + edge_cost;

      const float turn_penalty = cv_path_turn_penalty.As<float> ();

      if (turn_penalty > 0.0f && edge_cost < kInfiniteHeuristic) {
        const int grandparent_index = cur_route->parent;

        if (grandparent_index != kInvalidNodeIndex) {
          const auto &gp_origin = graph[grandparent_index].origin;
          const auto &cur_origin = graph[current_index].origin;
          const auto &child_origin = graph[child.index].origin;

          const auto dir_prev = (cur_origin - gp_origin).normalize2d ();
          const auto dir_next = (child_origin - cur_origin).normalize2d ();

          const float dot = ystl::clamp (dir_prev.dot (dir_next), -1.0f, 1.0f);

          if (dot < 0.0f) {
            g += edge_cost * (1.0f - dot) * turn_penalty;
          }
        }
      }
      const float h = heuristic_->H (child.index, dest_index);
      const float f = g + h;

      if (child_route->state == RouteState::New || child_route->f > f) {

        // put the current child into open list
        child_route->parent = current_index;
        child_route->state = RouteState::Open;

        child_route->g = g;
        child_route->f = f;

        route_que_.emplace (child.index, f);
      }
    }
  }
  return AStarResult::Failed;
}

void FloydWarshallAlgo::Rebuild () {
  size_ = graph.Length ();
  matrix_.resize (static_cast<size_t> (ystl::sqrf (size_)));

  // matrix is unusable for reads until sync rebuild is done, main thread falls back to dijkstra
  rebuilding_.store (true, ystl::MemoryOrder::release);

  worker.Enqueue ([this] () {
    SyncRebuild ();
  });
}

void FloydWarshallAlgo::SyncRebuild () {
  auto matrix = matrix_.data ();

  // re-initialize matrix every load
  for (int i = 0; i < size_; ++i) {
    for (int j = 0; j < size_; ++j) {
      *(matrix + (i * size_) + j) = { kInvalidNodeIndex, kInfinity };
    }
  }

  for (int i = 0; i < size_; ++i) {
    for (const auto &link : graph[i].links) {
      if (!graph.Exists (link.index)) {
        continue;
      }
      *(matrix + (i * size_) + link.index) = { link.index, ystl::min (link.distance, static_cast<int32_t> (kInfinity) - 1) };
    }
  }

  for (int i = 0; i < size_; ++i) {
    (matrix + (i * size_) + i)->dist = 0;
  }

  for (int k = 0; k < size_; ++k) {
    for (int i = 0; i < size_; ++i) {
      for (int j = 0; j < size_; ++j) {
        const int dist_ik = (matrix + (i * size_) + k)->dist;
        const int dist_kj = (matrix + (k * size_) + j)->dist;

        // skip if either distance is infinity
        if (dist_ik >= kInfinity || dist_kj >= kInfinity) {
          continue;
        }

        const int distance = dist_ik + dist_kj;

        // only update if distance fits in int16_t and is better
        if (distance < kInfinity && distance < (matrix + (i * size_) + j)->dist) {
          *(matrix + (i * size_) + j) = { (matrix + (i * size_) + k)->index, static_cast<int16_t> (distance) };
        }
      }
    }
  }
  Save (); // save path matrix to file for faster access

  // matrix rebuilt, main thread can read it again
  rebuilding_.store (false, ystl::MemoryOrder::release);
}

bool FloydWarshallAlgo::Load () {
  size_ = graph.Length ();

  if (!size_) {
    return false;
  }
  const bool data_loaded = bstor.Load<Matrix> (matrix_);

  // do not rebuild if loaded
  if (data_loaded) {
    return true;
  }
  Rebuild (); // rebuilds matrix

  return true;
}

void FloydWarshallAlgo::Save () const {
  if (!size_) {
    return;
  }
  bstor.Save<Matrix> (matrix_);
}

bool FloydWarshallAlgo::Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance) {
  // validate input indices
  if (src_index < 0 || src_index >= size_ || dest_index < 0 || dest_index >= size_) {
    return false;
  }

  const int start_index = src_index;

  on_added_node (src_index);

  while (src_index != dest_index) {
    src_index = Cell (src_index, dest_index).index;

    // check for invalid index or out of bounds
    if (src_index < 0 || src_index >= size_) {
      return false;
    }

    if (!on_added_node (src_index)) {
      return true;
    }
  }

  // only fill path distance on full path
  if (path_distance != nullptr) {
    *path_distance = Dist (start_index, dest_index);
  }
  return true;
}

void DijkstraAlgo::Init (const int length) {
  size_ = length;

  const auto ulength = static_cast<size_t> (length);

  // no shrink: it would reallocate + copy both buffers right after resize for zero benefit
  distance_.resize (ulength);
  parent_.resize (ulength);

  // distAll output slots are sized once so hot-path distAll never allocates
  distance_all0_.resize (ulength);
  distance_all1_.resize (ulength);

  // size the open list once as it can hold up to length entries
  queue_.reserve (ulength);
  scratch_.reserve (ulength);
}

bool DijkstraAlgo::Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance) {
  if (src_index < 0 || src_index >= size_ || dest_index < 0 || dest_index >= size_) {
    return false;
  }
  queue_.clear ();

  parent_.fill (kInvalidNodeIndex);
  distance_.fill (kInfiniteDistanceLong);

  queue_.emplace (0, src_index);
  distance_[src_index] = 0;

  while (!queue_.empty ()) {
    const auto &route = queue_.pop ();
    auto current = route.second;

    // finished search
    if (current == dest_index) {
      break;
    }

    if (distance_[current] != route.first) {
      continue;
    }

    for (const auto &link : graph[current].links) {
      if (link.index < 0 || link.index >= size_) {
        continue;
      }

      // skip links crossing a closed mode wall
      if (mode_walls.IsSegmentBlocked (graph[current].origin, graph[link.index].origin)) {
        continue;
      }
      const auto dlink = distance_[current] + link.distance;

      if (dlink < distance_[link.index]) {
        distance_[link.index] = dlink;
        parent_[link.index] = current;

        queue_.emplace (distance_[link.index], link.index);
      }
    }
  }
  if (on_added_node) {
    scratch_.clear ();

    for (int i = dest_index; i != kInvalidNodeIndex; i = parent_[i]) {
      scratch_.push (i);
    }
    scratch_.reverse ();

    for (const auto &node : scratch_) {
      if (!on_added_node (node)) {
        break;
      }
    }
  }

  // always fill path distance if we're need to
  if (path_distance != nullptr) {
    *path_distance = distance_[dest_index];
  }
  return distance_[dest_index] < kInfiniteDistanceLong;
}

int DijkstraAlgo::Dist (int src_index, int dest_index) {
  int path_distance = 0;

  Find (src_index, dest_index, nullptr, &path_distance);
  return path_distance;
}

bool DijkstraAlgo::DistAll (int src_index, DistanceTable &distances, int slot) {
  if (src_index < 0 || src_index >= size_) {
    return false;
  }
  queue_.clear ();

  // distances only: find () owns m_distance / m_parent, so use an isolated output slot
  auto &distance = slot == 0 ? distance_all0_ : distance_all1_;
  distance.fill (kInfiniteDistanceLong);

  queue_.emplace (0, src_index);
  distance[src_index] = 0;

  // same expansion as find (), but without early exit
  while (!queue_.empty ()) {
    const auto &route = queue_.pop ();
    auto current = route.second;

    if (distance[current] != route.first) {
      continue;
    }

    for (const auto &link : graph[current].links) {
      if (link.index < 0 || link.index >= size_) {
        continue;
      }

      // skip links crossing a closed mode wall
      if (mode_walls.IsSegmentBlocked (graph[current].origin, graph[link.index].origin)) {
        continue;
      }
      const auto dlink = distance[current] + link.distance;

      if (dlink < distance[link.index]) {
        distance[link.index] = dlink;
        queue_.emplace (distance[link.index], link.index);
      }
    }
  }
  // hand out a view, no copy
  distances = DistanceTable (distance.data (), size_);
  return true;
}

PathPlanner::PathPlanner () {
  dijkstra_ = ystl::make_unique<DijkstraAlgo> ();
  floyd_ = ystl::make_unique<FloydWarshallAlgo> ();
}

void PathPlanner::Init () {
  const int length = graph.Length ();

  // float division: integer math truncates small graphs to zero megabytes
  const float memory_use =
    static_cast<float> (sizeof (FloydWarshallAlgo::Matrix) * ystl::sqrf (static_cast<size_t> (length))) / 1024.0f / 1024.0f;
  const float limit_in_mb = cv_path_floyd_memory_limit.As<float> ();

  // limits can change at runtime, always re-evaluate
  memory_limit_hit_ = false;

  // ensure nodes are valid
  paths_check_failed_ = !graph.CheckNodes (false, true);

  // if we're have too much memory for floyd matrices, planner will use dijkstra or uniform planner for other than pathfinding needs
  if (memory_use > limit_in_mb) {
    memory_limit_hit_ = true;

    // we're need floyd tables when graph has failed sanity checks
    if (paths_check_failed_ && memory_use <= limit_in_mb * 1.5f) {
      memory_limit_hit_ = false;
    }
  }
  dijkstra_->Init (length);

  // load (re-create) floyds, if we're not hitting memory limits
  if (!memory_limit_hit_) {
    floyd_->Load ();
  }
}

bool PathPlanner::HasRealPathDistance () const {
  return !memory_limit_hit_ || !cv_path_dijkstra_simple_distance;
}

bool PathPlanner::Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance) {
  if (!graph.Exists (src_index) || !graph.Exists (dest_index)) {
    return false;
  }

  // floyd matrix is precomputed without walls, so it's wall-unaware; use dijkstra whenever walls are up.
  // limit hit or floyd matrix is being rebuilt on worker thread, use dijkstra
  if (memory_limit_hit_ || floyd_->IsRebuilding () || mode_walls.HasWalls ()) {
    return dijkstra_->Find (src_index, dest_index, on_added_node, path_distance);
  }
  return floyd_->Find (src_index, dest_index, on_added_node, path_distance);
}

bool PathPlanner::DistAll (int src_index, DistanceTable &distances, int slot) {
  if (!graph.Exists (src_index)) {
    return false;
  }

  // dijkstra covers memory limit, rebuilds and walls (floyd matrix is wall-unaware)
  if (memory_limit_hit_ || floyd_->IsRebuilding () || mode_walls.HasWalls ()) {
    return dijkstra_->DistAll (src_index, distances, slot);
  }

  // single floyd row read: same one-pass cost class as dijkstra, no copy
  const auto row = floyd_->Row (src_index);

  if (row == nullptr) {
    return false;
  }
  distances = DistanceTable (row, graph.Length ());
  return true;
}

float PathPlanner::Dist (int src_index, int dest_index) {
  if (!graph.Exists (src_index) || !graph.Exists (dest_index)) {
    return kInfiniteDistanceLong;
  }

  if (src_index == dest_index) {
    return 0.0f;
  }

  // wall-aware distance: 2d shortcut would pretend goals across a wall are close
  if (mode_walls.HasWalls ()) {
    return static_cast<float> (dijkstra_->Dist (src_index, dest_index));
  }

  // limit hit or floyd matrix is being rebuilt on worker thread, use dijkstra
  if (memory_limit_hit_ || floyd_->IsRebuilding ()) {
    if (cv_path_dijkstra_simple_distance) {
      return graph[src_index].origin.distance2d (graph[dest_index].origin);
    }
    return static_cast<float> (dijkstra_->Dist (src_index, dest_index));
  }
  return static_cast<float> (floyd_->Dist (src_index, dest_index));
}

float PathPlanner::PreciseDistance (int src_index, int dest_index) {
  if (!graph.Exists (src_index) || !graph.Exists (dest_index)) {
    return static_cast<float> (kInfiniteDistanceLong);
  }

  // floyd matrix is wall-unaware, use dijkstra whenever walls are up
  // limit hit or floyd matrix is being rebuilt on worker thread, use dijkstra
  if (memory_limit_hit_ || floyd_->IsRebuilding () || mode_walls.HasWalls ()) {
    return static_cast<float> (dijkstra_->Dist (src_index, dest_index));
  }
  return static_cast<float> (floyd_->Dist (src_index, dest_index));
}

} // namespace bot
