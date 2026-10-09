//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void Graph::Reset () {
  // this function initialize the graph structures

  edit_flags_ = GraphEdit::Off;
  auto_save_count_ = 0;

  learn_velocity_.clear ();
  learn_position_.clear ();
  last_node_.clear ();

  path_display_timer_.invalidate ();
  arrow_display_timer_.invalidate ();
  auto_path_distance_ = 250.0f;
  has_changed_ = false;
  narrow_checked_ = false;
  light_checked_ = false;

  info_.author.clear ();
  info_.modified.clear ();

  paths_.clear ();
  InitBuckets (); // stale indices would read out of bounds, rebuilt on load
}

int Graph::ClearConnections (int index) {
  // this function removes the useless paths connections from and to node pointed by index. this is based on code from pod-bot mm from kwo

  if (!Exists (index)) {
    return 0;
  }

  // restores the output mode on scope exit, printing all the stuff causes reliable message overflow
  struct RapidOutputGuard {
    RapidOutputGuard () {
      ctrl.SetRapidOutput (true);
    }

    ~RapidOutputGuard () {
      ctrl.SetRapidOutput (false);
    }
  };

  ConnectionCleaner cleaner { *this, index };
  cleaner.Collect ();

  if (cleaner.Empty ()) {
    Msg ("Cannot find path to the closest connected node to node number %d.", index);
    return 0;
  }

  // sort paths from the closest node to the farest away one, then calculate the bearings
  cleaner.SortByDistance ();
  cleaner.ComputeBearings ();

  // printing all the stuff causes reliable message overflow
  RapidOutputGuard guard {};
  cleaner.Run ();

  return cleaner.RemovedCount ();
}

int Graph::GetBspSize () {
  if (ystl::File bsp { ystl::strings.join_path (game.GetRunningModName (), "maps", game.GetMapName ()) + ".bsp", "rb" }) {
    return static_cast<int> (bsp.size ());
  }

  // worst case, load using engine (engfuncs.pfngetfilesize isn't available on some legacy engines)
  if (ystl::MemFile bsp { ystl::strings.join_path (game.GetRunningModName (), "maps", game.GetMapName ()) + ".bsp" }) {
    return static_cast<int> (bsp.size ());
  }
  return 0;
}

void Graph::AddPath (int add_index, int path_index, float distance) {
  if (!Exists (add_index) || !Exists (path_index) || path_index == add_index) {
    return;
  }
  auto &path = paths_[add_index];

  // don't allow paths get connected twice
  for (const auto &link : path.links) {
    if (link.index == path_index) {
      Msg ("Denied path creation from %d to %d (path already exists).", add_index, path_index);
      return;
    }
  }
  auto integer_distance = ystl::abs (static_cast<int> (distance));

  // check for free space in the connection indices
  for (auto &link : path.links) {
    if (link.index == kInvalidNodeIndex) {
      link.index = static_cast<int16_t> (path_index);
      link.distance = integer_distance;

      Msg ("Path added from %d to %d.", add_index, path_index);
      return;
    }
  }

  // there wasn't any free space. try exchanging it with a long-distance path
  int max_distance = 0;
  int slot = kInvalidNodeIndex;

  for (int i = 0; i < kMaxNodeLinks; ++i) {
    if (has_flag (path.links[i].flags, PathFlag::Jump)) {
      continue;
    }
    if (path.links[i].distance > max_distance) {
      max_distance = path.links[i].distance;
      slot = i;
    }
  }

  if (slot != kInvalidNodeIndex) {
    Msg ("Path added from %d to %d.", add_index, path_index);

    path.links[slot].index = static_cast<int16_t> (path_index);
    path.links[slot].distance = integer_distance;
    path.links[slot].flags = 0;
    path.links[slot].velocity.clear ();
  }
}

void Graph::FlagJumpLink (int from, int to) {
  if (!Exists (from) || !Exists (to) || from == to) {
    return;
  }

  for (auto &link : paths_[from].links) {
    if (link.index == to) {
      link.flags |= PathFlag::Jump;
      paths_[from].radius = 0.0f; // jump takeoff needs exact footing
      break;
    }
  }
}

bool Graph::HasFreeLinkSlot (int index) const {
  if (!Exists (index)) {
    return false;
  }

  for (const auto &link : paths_[index].links) {
    if (link.index == kInvalidNodeIndex) {
      return true;
    }
  }
  return false;
}

bool Graph::IsWalkableSlope (const ystl::Vector &src, const ystl::Vector &dst) const {
  // engine step height, anything above needs a hop
  const float step = sv_stepsize.As<float> ();

  if (step <= 0.0f) {
    return false;
  }
  const ystl::Vector delta = dst - src;

  // pure step-up without horizontal travel
  if (delta.length2d () < 1.0f) {
    return delta.z <= step;
  }
  Trace::Result tr {};

  // stepped ground follow, reject sudden rises above step height
  ystl::Vector check = src, down = src;
  down.z -= 1000.0f;

  trace.Line (check, down, TraceIgnore::Monsters, nullptr, &tr);
  float last_height = tr.fraction * 1000.0f;

  float distance_sq = dst.distance_sq (check);
  const ystl::Vector direction = delta.normalize ();

  // dense sampling, thin lips fall between sparse samples
  while (distance_sq > ystl::sqrf (5.0f)) {
    check = check + direction * 5.0f;
    down = check;
    down.z -= 1000.0f;

    trace.Line (check, down, TraceIgnore::Monsters, nullptr, &tr);
    const float height = tr.fraction * 1000.0f;

    if (height - last_height > step) {
      return false;
    }
    last_height = height;
    distance_sq = dst.distance_sq (check);
  }
  return true;
}

bool Graph::IsWalkableDrop (const ystl::Vector &src, const ystl::Vector &dst) const {
  // walk-off entry speed, conservative run pace
  static constexpr float kWalkOffSpeed = 200.0f;
  static constexpr float kLandWindow = 32.0f;

  const ystl::Vector delta = dst - src;

  // straight down a shaft always works, no horizontal motion to block
  if (delta.length2d () < 1.0f) {
    return delta.z < 0.0f;
  }

  if (delta.z >= 0.0f) {
    return false; // not a drop
  }
  const float gravity = sv_gravity.As<float> ();

  if (ystl::fzero (gravity)) {
    return false;
  }
  const float dist2d = delta.length2d ();
  const float time = dist2d / kWalkOffSpeed;

  // must arrive essentially at node height
  if (ystl::abs (src.z - 0.5f * gravity * time * time - dst.z) > kLandWindow) {
    return false;
  }

  // clearance along the fall curve
  const ystl::Vector dir = ystl::Vector { delta.x, delta.y, 0.0f } * (1.0f / dist2d);
  ystl::Vector prev = src;
  Trace::Result tr {};

  const int samples = ystl::clamp (static_cast<int> (dist2d / 15.0f), 2, 16);

  for (int i = 1; i <= samples; ++i) {
    const float t = time * static_cast<float> (i) / static_cast<float> (samples);
    ystl::Vector pos = src + dir * (kWalkOffSpeed * t);
    pos.z = src.z - 0.5f * gravity * t * t;

    trace.Hull (prev, pos, TraceIgnore::Monsters, head_hull, nullptr, &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      return false;
    }
    prev = pos;
  }
  return true;
}

int Graph::GetFarest (const ystl::Vector &origin, const float max_range) {
  // find the farest node to that origin, and return the index to this node

  int index = kInvalidNodeIndex;
  auto max_distance_sq = ystl::sqrf (max_range);

  for (const auto &path : paths_) {
    const float distance_sq = path.origin.distance_sq (origin);

    if (distance_sq > max_distance_sq) {
      index = path.number;
      max_distance_sq = distance_sq;
    }
  }
  return index;
}

int Graph::GetForAnalyzer (const ystl::Vector &origin, const float max_range) {
  // find the farest node to that origin, and return the index to this node

  int index = kInvalidNodeIndex;
  float maximum_distance_sq = ystl::sqrf (max_range);

  for (const auto &path : paths_) {
    const float distance_sq = path.origin.distance_sq (origin);

    if (distance_sq < maximum_distance_sq) {
      index = path.number;
      maximum_distance_sq = distance_sq;
    }
  }
  return index;
}

int Graph::GetNearestNoBuckets (const ystl::Vector &origin, const float range, NodeFlag flags) {
  // find the nearest node to that origin and return the index

  // fallback and go thru wall the nodes
  int index = kInvalidNodeIndex;
  float nearest_distance_sq = ystl::sqrf (range);

  for (const auto &path : paths_) {
    if (flags != -1 && !has_flag (path.flags, flags)) {
      continue; // if flag not -1 and node has no this flag, skip node
    }
    const float distance_sq = path.origin.distance_sq (origin);

    if (distance_sq < nearest_distance_sq) {
      index = path.number;
      nearest_distance_sq = distance_sq;
    }
  }
  return index;
}

int Graph::GetEditorNearest (const float max_range) {
  if (!HasEditFlag (GraphEdit::On)) {
    return kInvalidNodeIndex;
  }
  return GetNearestNoBuckets (editor_->v.origin, max_range);
}

int Graph::GetNearest (const ystl::Vector &origin, const float range, NodeFlag flags) {
  // find the nearest node to that origin and return the index

  // skip buckets on small maps, they give no speedup
  constexpr auto kMinNodesForBucketsThreshold = 164;

  if (Length () < kMinNodesForBucketsThreshold) {
    return GetNearestNoBuckets (origin, range, flags);
  }

  if (range > 256.0f && !ystl::fequal (range, kInfiniteDistance)) {
    return GetNearestNoBuckets (origin, range, flags);
  }
  const auto *bucket = GetNodesInBucket (origin);

  if (bucket == nullptr || bucket->size () < kMaxNodeLinks) {
    return GetNearestNoBuckets (origin, range, flags);
  }

  int index = kInvalidNodeIndex;
  auto nearest_distance_sq = ystl::sqrf (range);

  for (const auto &at : *bucket) {
    if (flags != -1 && !has_flag (paths_[at].flags, flags)) {
      continue; // if flag not -1 and node has no this flag, skip node
    }
    const float distance_sq = origin.distance_sq (paths_[at].origin);

    if (distance_sq < nearest_distance_sq) {
      index = at;
      nearest_distance_sq = distance_sq;
    }
  }

  // nothing found, try to find without buckets
  if (index == kInvalidNodeIndex) {
    return GetNearestNoBuckets (origin, range, flags);
  }
  return index;
}

ystl::SmallArray<int32_t> Graph::GetNearestInRadius (const float radius, const ystl::Vector &origin, int max_count) {
  // returns all nodes within radius from position

  const float radius_sq = ystl::sqrf (radius);

  ystl::SmallArray<int32_t> result {};
  const auto *bucket = GetNodesInBucket (origin);

  if (bucket == nullptr || bucket->size () < kMaxNodeLinks || radius_sq > ystl::sqrf (256.0f)) {
    for (const auto &path : paths_) {
      if (max_count != -1 && result.size<int32_t> () >= max_count) {
        break;
      }

      if (origin.distance_sq (path.origin) < radius_sq) {
        result.push (path.number);
      }
    }
    return result;
  }

  for (const auto &at : *bucket) {
    if (max_count != -1 && result.size<int32_t> () >= max_count) {
      break;
    }

    if (origin.distance_sq (paths_[at].origin) < radius_sq) {
      result.push (at);
    }
  }
  return result;
}

bool Graph::IsAnalyzed () const {
  return has_flag (info_.header.options, StorageOption::Analyzed);
}

bool Graph::IsConverted () const {
  return has_flag (info_.header.options, StorageOption::Converted);
}

bool Graph::IsImported () const {
  return has_flag (info_.header.options, StorageOption::Imported);
}

void Graph::Add (NodeAddFlag type, const ystl::Vector &pos) {
  if (!HasEditor () && !analyzer.IsAnalyzing ()) {
    return;
  }
  int index = kInvalidNodeIndex;
  Path *path = nullptr;

  bool add_new_node = true;
  ystl::Vector new_origin = pos;

  if (new_origin.empty ()) {
    if (!HasEditor ()) {
      return;
    }
    new_origin = editor_->v.origin;
  }
  bots.DisconnectAll ();
  has_changed_ = true;

  switch (type) {
  case NodeAddFlag::Camp:
    index = GetEditorNearest ();

    if (index != kInvalidNodeIndex) {
      path = &paths_[index];

      if (has_flag (path->flags, NodeFlag::Camp)) {
        path->start = editor_->v.v_angle.get2d ();
        EmitNotify (NotifySound::Done); // play "done" sound
        return;
      }
    }
    break;

  case NodeAddFlag::CampEnd:
    index = GetEditorNearest ();

    if (index != kInvalidNodeIndex) {
      path = &paths_[index];

      if (!has_flag (path->flags, NodeFlag::Camp)) {
        Msg ("This is not camping node.");
        return;
      }
      path->end = editor_->v.v_angle.get2d ();
      EmitNotify (NotifySound::Done); // play "done" sound
    }
    return;

  case NodeAddFlag::JumpStart:
    index = GetEditorNearest (25.0f);

    if (index != kInvalidNodeIndex && paths_[index].number >= 0) {
      const float distance_sq = editor_->v.origin.distance_sq (paths_[index].origin);

      if (distance_sq < ystl::sqrf (25.0f)) {
        add_new_node = false;

        path = &paths_[index];
        path->origin = (path->origin + learn_position_) * 0.5f;
      }
    }
    else {
      new_origin = learn_position_;
    }
    break;

  case NodeAddFlag::JumpEnd:
    index = GetEditorNearest (25.0f);

    if (index != kInvalidNodeIndex && paths_[index].number >= 0) {
      const float distance_sq = editor_->v.origin.distance_sq (paths_[index].origin);

      if (distance_sq < ystl::sqrf (25.0f)) {
        add_new_node = false;
        path = &paths_[index];

        int connection_flags = 0;

        for (const auto &link : path->links) {
          connection_flags += link.flags;
        }

        if (connection_flags == 0) {
          path->origin = (path->origin + editor_->v.origin) * 0.5f;
        }
      }
    }
    break;

  default:
    break;
  }

  if (add_new_node) {
    if (analyzer.IsAnalyzing ()) {
      for (const auto &cp : paths_) {
        if (new_origin.distance_sq (cp.origin) < ystl::sqrf (24.0f)) {
          return;
        }
      }
    }
    else {
      auto nearest = GetEditorNearest ();

      // do not allow to place node "inside" node, make at leat 10 units range
      if (Exists (nearest) && new_origin.distance_sq (paths_[nearest].origin) < ystl::sqrf (10.0f)) {
        Msg ("Can't add node. It's way to near to %d node. Please move some units anywhere.", nearest);
        return;
      }
    }

    // need to remove limit?
    if (paths_.size () >= kMaxNodes) {
      return;
    }
    paths_.emplace ();

    index = Length () - 1;
    path = &paths_[index];

    path->number = index;
    path->flags = 0;

    // store the origin (location) of this node
    path->origin = new_origin;
    path->start.clear ();
    path->end.clear ();

    path->display = 0.0f;
    path->light = kInvalidLightLevel;

    for (auto &link : path->links) {
      link.index = kInvalidNodeIndex;
      link.distance = 0;
      link.flags = 0;
      link.velocity.clear ();
    }

    // autosave nodes here and there
    if (!analyzer.IsAnalyzing () && cv_graph_auto_save_count && ++auto_save_count_ >= cv_graph_auto_save_count.As<int> ()) {
      if (SaveGraphData ()) {
        Msg ("Nodes has been autosaved...");
      }
      else {
        Msg ("Can't autosave graph data...");
      }
      auto_save_count_ = 0;
    }

    // store the last used node for the auto node code
    if (!analyzer.IsAnalyzing ()) {
      last_node_ = editor_->v.origin;
    }
  }

  if (type == NodeAddFlag::JumpStart) {
    last_jump_node_ = index;
  }
  else if (type == NodeAddFlag::JumpEnd) {
    const float distance = paths_[last_jump_node_].origin.distance (editor_->v.origin);
    AddPath (last_jump_node_, index, distance);

    for (auto &link : paths_[last_jump_node_].links) {
      if (link.index == index) {
        link.flags |= PathFlag::Jump;
        link.velocity = learn_velocity_;

        break;
      }
    }
    CalculatePathRadius (index);
    return;
  }

  if (!path || path->number == kInvalidNodeIndex) {
    return;
  }

  if (analyzer.IsCrouch () || (!analyzer.IsAnalyzing () && (editor_->v.flags & FL_DUCKING))) {
    path->flags |= NodeFlag::Crouch; // set a crouch node
  }

  if (!analyzer.IsAnalyzing () && editor_->v.movetype == MOVETYPE_FLY) {
    path->flags |= NodeFlag::Ladder;
  }
  else if (is_on_ladder_) {
    path->flags |= NodeFlag::Ladder;
  }

  switch (type) {
  case NodeAddFlag::TOnly:
    path->flags |= NodeFlag::Crossing;
    path->flags |= NodeFlag::TerroristOnly;
    break;

  case NodeAddFlag::CTOnly:
    path->flags |= NodeFlag::Crossing;
    path->flags |= NodeFlag::CTOnly;
    break;

  case NodeAddFlag::NoHostage:
    path->flags |= NodeFlag::NoHostage;
    break;

  case NodeAddFlag::Rescue:
    path->flags |= NodeFlag::Rescue;
    break;

  case NodeAddFlag::Camp:
    path->flags |= NodeFlag::Crossing;
    path->flags |= NodeFlag::Camp;

    if (!analyzer.IsAnalyzing ()) {
      path->start = editor_->v.v_angle;
      path->end = editor_->v.v_angle;
    }
    break;

  case NodeAddFlag::Goal:
    path->flags |= NodeFlag::Goal;
    break;

  default:
    break;
  }

  // ladder nodes need careful connections
  if (has_flag (path->flags, NodeFlag::Ladder)) {
    float nearest_distance = kInfiniteDistance;
    int dest_index = kInvalidNodeIndex;

    Trace::Result tr {};

    // calculate all the paths to this new node
    for (const auto &calc : paths_) {
      if (calc.number == index) {
        continue; // skip the node that was just added
      }

      // other ladder nodes should connect to this
      if (has_flag (calc.flags, NodeFlag::Ladder)) {
        // check if the node is reachable from the new one
        trace.Line (new_origin, calc.origin, TraceIgnore::Monsters, editor_, &tr);

        if (ystl::fequal (tr.fraction, 1.0f) && ystl::abs (new_origin.x - calc.origin.x) < 64.0f &&
            ystl::abs (new_origin.y - calc.origin.y) < 64.0f && ystl::abs (new_origin.z - calc.origin.z) < auto_path_distance_) {

          const float distance = new_origin.distance2d (calc.origin);

          AddPath (index, calc.number, distance);
          AddPath (calc.number, index, distance);
        }
      }
      else {
        const float distance = new_origin.distance2d (calc.origin);

        if (distance < nearest_distance) {
          dest_index = calc.number;
          nearest_distance = distance;
        }

        // check if the node is reachable from the new one
        if (IsNodeReacheable (new_origin, calc.origin)) {
          AddPath (index, calc.number, distance);
        }
      }
    }

    if (Exists (dest_index)) {
      const float distance = new_origin.distance2d (paths_[dest_index].origin);

      if (analyzer.IsAnalyzing ()) {
        AddPath (index, dest_index, distance);
        AddPath (dest_index, index, distance);
      }
      else {
        // check if the node is reachable from the new one (one-way)
        if (IsNodeReacheable (new_origin, paths_[dest_index].origin)) {
          AddPath (index, dest_index, new_origin.distance (paths_[dest_index].origin));
        }

        // check if the new one is reachable from the node (other way)
        if (IsNodeReacheable (paths_[dest_index].origin, new_origin)) {
          AddPath (dest_index, index, new_origin.distance (paths_[dest_index].origin));
        }
      }
    }
  }
  else {
    // calculate all the paths to this new node
    for (const auto &calc : paths_) {
      if (calc.number == index) {
        continue; // skip the node that was just added
      }
      const float distance = calc.origin.distance2d (new_origin);

      // check if the node is reachable from the new one (one-way)
      if (IsNodeReacheable (new_origin, calc.origin)) {
        AddPath (index, calc.number, distance);
      }

      // check if the new one is reachable from the node (other way)
      if (IsNodeReacheable (calc.origin, new_origin)) {
        AddPath (calc.number, index, distance);
      }

      // walk link over a lip still needs a hop at runtime, flag it
      if (analyzer.IsAnalyzing ()) {
        if (IsConnected (index, calc.number) && !IsWalkableSlope (new_origin, calc.origin)) {
          FlagJumpLink (index, calc.number);
        }

        if (IsConnected (calc.number, index) && !IsWalkableSlope (calc.origin, new_origin)) {
          FlagJumpLink (calc.number, index);
        }
      }
    }

    // analyzer precomputes jump topology, runtime only executes it
    if (analyzer.IsAnalyzing ()) {
      for (const auto &calc : paths_) {
        if (calc.number == index) {
          continue; // skip the node that was just added
        }
        const float distance = calc.origin.distance2d (new_origin);

        // downhill walk-off needs no takeoff, plain link suffices
        if (!IsConnected (index, calc.number) && HasFreeLinkSlot (index) && IsWalkableDrop (new_origin, calc.origin)) {
          AddPath (index, calc.number, distance);
        }
        // one-way jump link, never evict walk links for it
        else if (!IsConnected (index, calc.number) && HasFreeLinkSlot (index) && IsNodeReacheableWithJump (new_origin, calc.origin)) {
          AddPath (index, calc.number, distance);
          FlagJumpLink (index, calc.number);
        }

        if (!IsConnected (calc.number, index) && HasFreeLinkSlot (calc.number) && IsWalkableDrop (calc.origin, new_origin)) {
          AddPath (calc.number, index, distance);
        }
        else if (!IsConnected (calc.number, index) && HasFreeLinkSlot (calc.number) && IsNodeReacheableWithJump (calc.origin, new_origin)) {
          AddPath (calc.number, index, distance);
          FlagJumpLink (calc.number, index);
        }
      }
    }

    if (!analyzer.IsAnalyzing ()) {
      ClearConnections (index);
    }
  }
  EmitNotify (NotifySound::Added);
  CalculatePathRadius (index); // calculate the wayzone of this node

  // jump takeoff needs exact footing, restore zeroed wayzone
  if (analyzer.IsAnalyzing ()) {
    for (const auto &link : paths_[index].links) {
      if (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump)) {
        paths_[index].radius = 0.0f;
        break;
      }
    }
  }

  if (analyzer.IsAnalyzing ()) {
    analyzer.MarkOptimized (index);
  }
}

void Graph::Erase (int target) {
  has_changed_ = true;

  if (paths_.empty ()) {
    return;
  }
  bots.DisconnectAll ();

  const int index = (target == kInvalidNodeIndex) ? GetEditorNearest () : target;

  if (!Exists (index)) {
    return;
  }
  auto &path = paths_[index];

  // unassign paths that points to this nodes
  for (auto &connected : paths_) {
    for (auto &link : connected.links) {
      if (link.index == index) {
        link.index = kInvalidNodeIndex;
        link.flags = 0;
        link.distance = 0;
        link.velocity.clear ();
      }
    }
  }

  // relink nodes so the index will match path number
  for (auto &relink : paths_) {

    // if pathnumber bigger than deleted node
    if (relink.number > index) {
      --relink.number;
    }

    for (auto &neighbour : relink.links) {
      if (neighbour.index > index) {
        --neighbour.index;
      }
    }
  }
  paths_.remove (path);
  InitBuckets (); // renumbering invalidates all buckets, rebuilt on load
  EmitNotify (NotifySound::Change);
}

void Graph::ToggleFlags (NodeFlag toggle_flag) {
  // this function allow manually changing flags

  int index = GetEditorNearest ();

  if (index != kInvalidNodeIndex) {
    if (has_flag (paths_[index].flags, toggle_flag)) {
      paths_[index].flags = ystl::to_underlying (clear_flag (paths_[index].flags, toggle_flag));
    }
    else {
      if (toggle_flag == NodeFlag::Sniper && !has_flag (paths_[index].flags, NodeFlag::Camp)) {
        Msg ("Cannot assign sniper flag to node %d. This is not camp node.", index);
        return;
      }
      paths_[index].flags |= toggle_flag;
    }
    EmitNotify (NotifySound::Done); // play "done" sound
  }
}

void Graph::SetRadius (int index, float radius) {
  // this function allow manually setting the zone radius

  const int node = Exists (index) ? index : GetEditorNearest ();

  if (node != kInvalidNodeIndex) {
    paths_[node].radius = radius;
    EmitNotify (NotifySound::Done); // play "done" sound

    Msg ("Node %d has been set to radius %.2f.", node, radius);
  }
}

bool Graph::IsConnected (int a, int b) {
  // this function checks if node a has a connection to node b

  if (!Exists (a) || !Exists (b)) {
    return false;
  }

  for (const auto &link : paths_[a].links) {
    if (link.index == b) {
      return true;
    }
  }
  return false;
}

int Graph::GetFacingIndex () {
  // find the node the user is pointing at

  ystl::Twin<int32_t, float> result { kInvalidNodeIndex, 5.32f };
  auto nearest_node = GetEditorNearest ();

  // check bounds from eyes of editor
  const ystl::Vector editor_eyes = editor_->v.origin + editor_->v.view_ofs;

  for (const auto &path : paths_) {

    // skip nearest node to editor, since this used mostly for adding / removing paths
    if (path.number == nearest_node) {
      continue;
    }

    const ystl::Vector to = path.origin - editor_->v.origin;
    auto angles = (to.angles () - editor_->v.v_angle).clamp_angles ();

    // skip the nodes that are too far away from us, and we're not looking at them directly
    if (to.length_sq () > ystl::sqrf (cv_graph_draw_distance.As<float> ()) || ystl::abs (angles.y) > result.second) {
      continue;
    }

    // check if visible, (we're not using visibility tables here, as they not valid at time of node editing)
    Trace::Result tr {};
    trace.Line (editor_eyes, path.origin, TraceIgnore::Everything, editor_, &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      continue;
    }
    const float best_angle = angles.y;

    angles = -editor_->v.v_angle;
    angles.x = -angles.x;
    angles =
      (angles + ((path.origin - ystl::Vector (0.0f, 0.0f, has_flag (path.flags, NodeFlag::Crouch) ? 17.0f : 34.0f)) - editor_eyes).angles ())
        .clamp_angles ();

    if (angles.x > 0.0f) {
      continue;
    }
    result = { path.number, best_angle };
  }
  return result.first;
}

void Graph::PathCreate (PathConnection dir) {
  // this function allow player to manually create a path from one node to another

  int node_from = GetEditorNearest ();

  if (node_from == kInvalidNodeIndex) {
    Msg ("Unable to find nearest node in 50 units.");
    return;
  }
  int node_to = facing_at_index_;

  if (!Exists (node_to)) {
    if (Exists (cache_node_index_)) {
      node_to = cache_node_index_;
    }
    else {
      Msg ("Unable to find destination node.");
      return;
    }
  }

  if (node_to == node_from) {
    Msg ("Unable to connect node with itself.");
    return;
  }
  const float distance = paths_[node_from].origin.distance (paths_[node_to].origin);

  if (dir == PathConnection::Outgoing) {
    AddPath (node_from, node_to, distance);
  }
  else if (dir == PathConnection::Incoming) {
    AddPath (node_to, node_from, distance);
  }
  else if (dir == PathConnection::Jumping) {
    if (!IsConnected (node_from, node_to)) {
      AddPath (node_from, node_to, distance);
    }

    for (auto &link : paths_[node_from].links) {
      if (link.index == node_to && !has_flag (link.flags, PathFlag::Jump)) {
        link.flags |= PathFlag::Jump;
        paths_[node_from].radius = 0.0f;

        Msg ("Path added from %d to %d.", node_from, node_to);
      }
      else if (link.index == node_to && has_flag (link.flags, PathFlag::Jump)) {
        Msg ("Denied path creation from %d to %d (path already exists).", node_from, node_to);
      }
    }
  }
  else {
    AddPath (node_from, node_to, distance);
    AddPath (node_to, node_from, distance);
  }
  EmitNotify (NotifySound::Done); // play "done" sound
  has_changed_ = true;
}

void Graph::ErasePath () {
  // this function allow player to manually remove a path from one node to another

  int node_from = GetEditorNearest ();

  if (node_from == kInvalidNodeIndex) {
    Msg ("Unable to find nearest node in 50 units.");
    return;
  }
  int node_to = facing_at_index_;

  if (!Exists (node_to)) {
    if (Exists (cache_node_index_)) {
      node_to = cache_node_index_;
    }
    else {
      Msg ("Unable to find destination node.");
      return;
    }
  }

  // helper
  auto destroy = [] (PathLink &link) -> void {
    link.index = kInvalidNodeIndex;
    link.distance = 0;
    link.flags = 0;
    link.velocity.clear ();
  };

  for (auto &link : paths_[node_from].links) {
    if (link.index == node_to) {
      destroy (link);
      EmitNotify (NotifySound::Change);

      return;
    }
  }

  // not found this way ? check for incoming connections then
  ystl::swap (node_from, node_to);

  for (auto &link : paths_[node_from].links) {
    if (link.index == node_to) {
      destroy (link);
      EmitNotify (NotifySound::Change);

      return;
    }
  }
  Msg ("There is already no path on this node.");
}

void Graph::ResetPath (int index) {
  int node = index;

  if (!Exists (node)) {
    node = GetEditorNearest ();

    if (!Exists (node)) {
      Msg ("Unable to find nearest node in 50 units.");
      return;
    }
  }

  // helper
  auto destroy = [] (PathLink &link) -> void {
    link.index = kInvalidNodeIndex;
    link.distance = 0;
    link.flags = 0;
    link.velocity.clear ();
  };

  // clean all incoming
  for (auto &connected : paths_) {
    for (auto &link : connected.links) {
      if (link.index == node) {
        destroy (link);
      }
    }
  }

  // clean all outgoing connections
  for (auto &link : paths_[node].links) {
    destroy (link);
  }
  EmitNotify (NotifySound::Change);

  // notify use something evil happened
  Msg ("All paths for node #%d has been reset.", node);
}

void Graph::CachePoint (int index) {
  const int node = Exists (index) ? index : GetEditorNearest ();

  if (node == kInvalidNodeIndex) {
    cache_node_index_ = kInvalidNodeIndex;
    Msg ("Cached node cleared (nearby point not found in 50 units range).");

    return;
  }
  cache_node_index_ = node;
  Msg ("Node %d has been put into memory.", cache_node_index_);
}

void Graph::SetAutoPathDistance (const float distance) {
  auto_path_distance_ = distance;

  if (ystl::fzero (distance)) {
    Msg ("Autopathing is now disabled.");
  }
  else {
    Msg ("Autopath distance is set to %.2f.", distance);
  }
}

void Graph::ShowStats () {
  int terr_points = 0;
  int ct_points = 0;
  int goal_points = 0;
  int rescue_points = 0;
  int camp_points = 0;
  int sniper_points = 0;
  int no_hostage_points = 0;

  for (const auto &path : paths_) {
    if (has_flag (path.flags, NodeFlag::TerroristOnly)) {
      ++terr_points;
    }

    if (has_flag (path.flags, NodeFlag::CTOnly)) {
      ++ct_points;
    }

    if (has_flag (path.flags, NodeFlag::Goal)) {
      ++goal_points;
    }

    if (has_flag (path.flags, NodeFlag::Rescue)) {
      ++rescue_points;
    }

    if (has_flag (path.flags, NodeFlag::Camp)) {
      ++camp_points;
    }

    if (has_flag (path.flags, NodeFlag::Sniper)) {
      ++sniper_points;
    }

    if (has_flag (path.flags, NodeFlag::NoHostage)) {
      ++no_hostage_points;
    }
  }

  Msg ("Nodes: %d - T Points: %d", paths_.size (), terr_points);
  Msg ("CT Points: %d - Goal Points: %d", ct_points, goal_points);
  Msg ("Rescue Points: %d - Camp Points: %d", rescue_points, camp_points);
  Msg ("Block Hostage Points: %d - Sniper Points: %d", no_hostage_points, sniper_points);
}

void Graph::ShowFileInfo () {
  const auto &info = info_.header;
  const auto &exten = info_.exten;

  Msg ("header:");
  Msg ("  magic: %d", info.magic);
  Msg ("  version: %d", info.version);
  Msg ("  node_count: %d", info.length);
  Msg ("  compressed_size: %dkB", info.compressed / 1024);
  Msg ("  uncompressed_size: %dkB", info.uncompressed / 1024);
  Msg ("  options: %d", info.options); // display as string ?
  Msg ("  analyzed: %s", IsAnalyzed () ? conf.Translate ("yes") : conf.Translate ("no")); // display as string ?
  Msg ("  converted: %s", IsConverted () ? conf.Translate ("yes") : conf.Translate ("no")); // display as string ?
  Msg ("  imported: %s", IsImported () ? conf.Translate ("yes") : conf.Translate ("no")); // display as string ?
  Msg ("  pathfinder: %s", planner.IsPathsCheckFailed () ? (planner.IsMemoryLimitHit () ? "dijkstra" : "floyd") : "astar");

  Msg ("");

  Msg ("extensions:");
  Msg ("  author: %s", exten.author);
  Msg ("  modified_by: %s", exten.modified);
  Msg ("  bsp_size: %d", exten.map_size);
}

void Graph::EmitNotify (NotifySound sound) const {
  static ystl::HashMap<NotifySound, ystl::String> notify_sounds = {
    { NotifySound::Added,  "weapons/xbow_hit1.wav"     },
    { NotifySound::Change, "weapons/mine_activate.wav" },
    { NotifySound::Done,   "common/wpn_hudon.wav"      }
  };

  // notify editor
  if (game.IsPlayerEntity (editor_) && !silence_messages_) {
    game.PlaySound (editor_, notify_sounds[sound].chars ());
  }
}

bool GraphUrlResolver::MatchAlias (ystl::StringRef raw, Alias *out_alias, ystl::String *out_url) {
  if (!raw.starts_with ("@")) {
    return false;
  }
  ystl::String rest = raw.substr (1);
  ystl::String name = rest, suffix {};

  const size_t slash = rest.find ("/");

  if (slash != ystl::String::InvalidIndex) {
    name = rest.substr (0, slash);
    suffix = rest.substr (slash); // keeps the leading "/"
  }
  name.lowercase ();

  ystl::String base {};
  Alias alias = Alias::Unknown;

  if (name == "github") {
    base = Endpoint ("GraphGithubDownload", kGithubDownloadUrl);
    alias = Alias::Github;
  }
  else if (name == "russia" || name == "ru") {
    // download and upload live on different hosts here, the caller picks the right one; default to the worker base
    base = Endpoint ("GraphRussiaWorker", kRussiaWorkerUrl);
    alias = Alias::Russia;
  }
  else if (name == "workers") {
    base = Endpoint ("GraphWorkersUpload", kWorkersBaseUrl);
    alias = Alias::Workers;
  }
  else if (name == "http" || name == "legacy" || name == "jeefo") {
    base = Endpoint ("GraphLegacyBase", kLegacyBaseUrl);
    alias = Alias::Http;
  }

  if (alias == Alias::Unknown) {
    return false;
  }
  *out_alias = alias;
  *out_url = base + suffix;

  return true;
}

ystl::String GraphUrlResolver::Endpoint (ystl::StringRef key, ystl::StringRef fallback) {
  const auto value = conf.FetchCustom (key);

  if (!value.empty ()) {
    return ystl::String (value);
  }
  return ystl::String (fallback);
}

ystl::String GraphUrlResolver::ExpandImpl (ystl::StringRef raw, Alias *out_alias) {
  ystl::String value { raw };

  // configs and console paths disagree on quoting, trim it with whitespace
  value.trim (" \t\r\n\"'");

  if (value.empty ()) {
    return value;
  }

  Alias alias = Alias::None;
  ystl::String expanded {};

  if (MatchAlias (value, &alias, &expanded)) {
    if (out_alias != nullptr) {
      *out_alias = alias;
    }
    return expanded;
  }

  // unknown "@..." alias: fail loudly with empty instead of connecting somewhere unexpected
  if (value.starts_with ("@")) {
    if (out_alias != nullptr) {
      *out_alias = Alias::Unknown;
    }
    return {};
  }

  // legacy bare host[/path] values are assumed http
  if (value.find ("://") == ystl::String::InvalidIndex) {
    value = ystl::String ("http://") + value;
  }

  if (out_alias != nullptr) {
    *out_alias = Alias::None;
  }
  return value;
}
void GraphUrlResolver::WarnNoTlsOnce (ystl::StringRef feature, ystl::StringRef configured, ystl::StringRef note) {
  static bool warned = false;

  if (warned) {
    return;
  }
  warned = true;

  if (note.empty ()) {
    ctrl.Msg ("Graph DB: '%s' is configured as '%s', but this build has no TLS support.", feature, configured);
  }
  else {
    ctrl.Msg ("Graph DB: '%s' is configured as '%s', but this build has no TLS support. %s.", feature, configured, note);
  }
}

ystl::String GraphUrlResolver::Expand (ystl::StringRef raw) {
  return ExpandImpl (raw, nullptr);
}

ystl::String GraphUrlResolver::DownloadBase () {
  const auto raw = cv_graph_url.As<ystl::StringRef> ();

  Alias alias = Alias::None;
  ystl::String base = ExpandImpl (raw, &alias);

  if (base.empty ()) {
    return base;
  }

  // the russian copy serves downloads from sourcecraft, not from the github
  if (alias == Alias::Russia) {
    base = Endpoint ("GraphRussiaDownload", kRussiaDownloadUrl);
  }

  // download is https-only and has no plain http source, so on builds without tls it fails loudly with HttpOnly at request time
  if (!ystl::http.has_tls_support () && base.starts_with ("https://")) {
    WarnNoTlsOnce ("graph download", raw, "Download requires https and is unavailable.");
  }
  return base;
}

ystl::String GraphUrlResolver::UploadBase () {
  const auto raw = cv_graph_url_upload.As<ystl::StringRef> ();

  Alias alias = Alias::None;
  ystl::String base = ExpandImpl (raw, &alias);

  if (base.empty ()) {
    return base;
  }

  // the workers are https-only, downgrade our own aliases to the legacy http upload
  if (!ystl::http.has_tls_support () && base.starts_with ("https://") && (alias == Alias::Workers || alias == Alias::Russia)) {
    WarnNoTlsOnce ("graph upload", raw, "Falling back to plain http.");

    return Endpoint ("GraphLegacyUpload", kLegacyUploadUrl);
  }
  return base;
}

ystl::String GraphUrlResolver::DownloadUrl (ystl::StringRef map_name) {
  const auto base = DownloadBase ();

  if (base.empty ()) {
    return base;
  }
  ystl::String lower { map_name };
  lower.lowercase ();

  return ystl::HttpUrl::join (base, ystl::strings.format ("graph/%s.graph", lower.chars ()));
}

ystl::String GraphUrlResolver::UploadUrl () {
  ystl::String base = UploadBase ();

  // same as joinUrl (base, ""): upload is a POST to the base as-is
  base.rtrim ("/");

  return base;
}

ystl::String GraphUrlResolver::CollectUrl (ystl::StringRef have_csv) {
  if (!CanCollect ()) {
    return {};
  }
  ystl::String root = UploadBase ();

  // the worker exposes /collect at the server root
  if (root.ends_with ("/upload")) {
    root = root.substr (0, root.size () - 7);
  }
  root.rtrim ("/");

  ystl::String query = "collect?have=";
  query.append (have_csv);

  return ystl::HttpUrl::join (root, query);
}

bool GraphUrlResolver::CanDownload () {
  return !DownloadBase ().empty ();
}

bool GraphUrlResolver::CanCollect () {
  // collect exists only on the worker (https), without https it's forbidden
  if (!ystl::http.has_tls_support ()) {
    return false;
  }
  return UploadBase ().starts_with ("https://");
}

void Graph::SyncCollectOnline () {
  is_online_collected_ = true; // once per server start

  // path to graph files
  auto graph_files_path = bstor.BuildPath (StorageFile::Graph, false, true);

  // enumerate graph files
  ystl::FileEnumerator enumerator { ystl::strings.join_path (graph_files_path, "*.graph") };

  // listing of graphs locally available
  ystl::Array<ystl::String> local_graphs {};

  // collect all the files
  while (enumerator) {
    auto match = enumerator.get_match ();

    match = match.substr (match.find_last_of (kPathSeparator) + 1);
    match = match.substr (0, match.find_first_of ("."));

    local_graphs.emplace (match);
    enumerator.next ();
  }

  // no graphs ? unbelievable
  if (local_graphs.empty ()) {
    return;
  }

  // ask the database which of our graphs it's missing, and get the csv diff back right away
  const auto graph_list = ystl::String::join (local_graphs, ",");
  const auto collect_url = graph_urls.CollectUrl (graph_list);

  if (collect_url.empty ()) {
    return;
  }
  ystl::String local_file = ystl::plat.tmpfname ();

  // no temp file, no fun
  if (local_file.empty ()) {
    return;
  }

  // don't forget remove temporary file
  auto unlink_temporary = [&] () {
    if (ystl::plat.file_exists (local_file.chars ())) {
      ystl::plat.remove_file (local_file.chars ());
    }
  };

  // download collection diff
  if (!ystl::http.download_file (collect_url, local_file)) {
    unlink_temporary ();
    return;
  }
  ystl::Array<ystl::String> wanted {};

  // decode answer
  if (ystl::File lc { local_file, "rt" }) {
    ystl::String lines {};

    if (lc.get_line (lines)) {
      wanted = lines.split (",");
    }
    lc.close ();
  }
  unlink_temporary ();

  // if 'we're have something in diff, bailout
  if (wanted.empty ()) {
    return;
  }
  local_graphs.clear ();

  // convert graphs names into full paths
  for (const auto &wn : wanted) {
    if (wn == game.GetMapName ()) {
      continue; // skip current map always
    }
    local_graphs.emplace (ystl::strings.join_path (graph_files_path, wn) + ".graph");
  }

  // try to upload everything database wants
  for (const auto &lg : local_graphs) {
    if (!ystl::plat.file_exists (lg.chars ())) {
      continue;
    }
    StorageHeader hdr {};

    // read storage header and check if file not analyzed
    if (ystl::File gp { lg, "rb" }) {
      gp.le_read (hdr); // converted from on-disk little-endian layout

      // check the magic, graph is not analyzed and have some viable nodes number
      if (hdr.magic == kStorageMagic && !has_flag (hdr.options, StorageOption::Analyzed) && hdr.length > 48) {

        // upload is a plain POST to the base as-is, identical on worker and legacy
        ystl::http.upload_file (graph_urls.UploadUrl (), lg);
      }
      gp.close ();
    }
  }
}

void Graph::CollectOnline () {
  if (is_online_collected_ || !cv_graph_auto_collect_db) {
    return;
  }

  worker.Enqueue ([this] () {
    SyncCollectOnline ();
  });
}

void Graph::CalculatePathRadius (int index) {
  // calculate "wayzones" for the nearest node  (meaning a dynamic distance area to vary node origin)

  auto &path = paths_[index];
  ystl::Vector start {}, direction {};

  if (has_flag (path.flags, NodeFlag::Ladder | NodeFlag::Goal | NodeFlag::Camp | NodeFlag::Rescue | NodeFlag::Crouch) || jump_learn_node_) {
    path.radius = 0.0f;
    return;
  }

  for (const auto &test : path.links) {
    if (test.index != kInvalidNodeIndex && has_flag (paths_[test.index].flags, NodeFlag::Ladder)) {
      path.radius = 0.0f;
      return;
    }
  }
  Trace::Result tr {};
  bool way_blocked = false;

  for (int32_t scan_distance = 32; scan_distance < 128; scan_distance += 16) {
    auto scan = static_cast<float> (scan_distance);
    start = path.origin;

    direction = ystl::Vector (0.0f, 0.0f, 0.0f).forward () * scan;
    direction = direction.angles ();

    path.radius = scan;

    for (int32_t circle_radius = 0; circle_radius < 360; circle_radius += 20) {
      const ystl::Vector forward = direction.forward ();

      auto radius_start = start + forward * scan;
      auto radius_end = start + forward * scan;

      trace.Hull (radius_start, radius_end, TraceIgnore::Monsters, head_hull, nullptr, &tr);

      if (tr.fraction < 1.0f) {
        trace.Line (radius_start, radius_end, TraceIgnore::Monsters, nullptr, &tr);

        if (game.IsDoorEntity (tr.hit)) {
          path.radius = 0.0f;
          way_blocked = true;

          break;
        }
        way_blocked = true;
        path.radius -= 16.0f;

        break;
      }

      auto drop_start = start + forward * scan;
      auto drop_end = drop_start - ystl::Vector (0.0f, 0.0f, scan + 60.0f);

      trace.Hull (drop_start, drop_end, TraceIgnore::Monsters, head_hull, nullptr, &tr);

      if (tr.fraction >= 1.0f) {
        way_blocked = true;
        path.radius -= 16.0f;

        break;
      }
      drop_start = start - forward * scan;
      drop_end = drop_start - ystl::Vector (0.0f, 0.0f, scan + 60.0f);

      trace.Hull (drop_start, drop_end, TraceIgnore::Monsters, head_hull, nullptr, &tr);

      if (tr.fraction >= 1.0f) {
        way_blocked = true;
        path.radius -= 16.0f;
        break;
      }

      radius_end.z += 34.0f;
      trace.Hull (radius_start, radius_end, TraceIgnore::Monsters, head_hull, nullptr, &tr);

      if (tr.fraction < 1.0f) {
        way_blocked = true;
        path.radius -= 16.0f;

        break;
      }
      direction.y = ystl::wrap_angle (direction.y + static_cast<float> (circle_radius));
    }

    if (way_blocked) {
      break;
    }
  }
  path.radius -= 16.0f;

  if (path.radius < 0.0f) {
    path.radius = 0.0f;
  }
}

void Graph::SyncInitLightLevels () {
  // this function get's the light level for each node on the map

  // update light levels for all nodes
  for (auto &path : paths_) {
    path.light = illum.GetLightLevel (path.origin + ystl::Vector { 0.0f, 0.0f, 16.0f });
  }
  light_checked_ = true;

  // disable lightstyle animations on finish (will be auto-enabled on mapchange)
  illum.EnableAnimation (false);
}

void Graph::InitLightLevels () {
  // this function get's the light level for each node on the map

  // no nodes ? no light levels, and only one-time init
  if (paths_.empty () || light_checked_) {
    return;
  }
  const auto &[ts, cts] = bots.CountTeamPlayers ();

  // do calculation if some-one is already playing on the server
  if (!ts && !cts) {
    return;
  }

  worker.Enqueue ([this] () {
    SyncInitLightLevels ();
  });
}

void Graph::InitNarrowPlaces () {
  // this function checks all nodes if they are inside narrow places

  // no nodes ?
  if (paths_.empty () || narrow_checked_) {
    return;
  }
  constexpr int32_t kNarrowPlacesMinGraphVersion = 2;

  // if version 2 or higher, narrow places already initialized and saved into file
  if (info_.header.version >= kNarrowPlacesMinGraphVersion && !HasEditFlag (GraphEdit::On)) {
    narrow_checked_ = true;
    return;
  }
  Trace::Result tr {};

  const auto distance = 178.0f;
  const auto worldspawn = game.GetStartEntity ();
  const auto offset = ystl::Vector (0.0f, 0.0f, 16.0f);

  // check olny paths that have not too much connections
  for (auto &path : paths_) {

    // skip any goals and camp points
    if (has_flag (path.flags, NodeFlag::Camp | NodeFlag::Goal)) {
      continue;
    }
    int link_count = 0;

    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex || link.index == path.number) {
        continue;
      }

      if (++link_count > kMaxNodeLinks / 2) {
        break;
      }
    }

    // skip nodes with too much connections, this indicated we're not in narrow place
    if (link_count > kMaxNodeLinks / 2) {
      continue;
    }
    int accum_weight = 0;

    // we could use this one!
    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex || link.index == path.number) {
        continue;
      }
      const ystl::Vector ang = ((path.origin - paths_[link.index].origin).normalize () * distance).angles ();

      ystl::Vector forward {}, right {}, upward {};
      ang.angle_vectors (&forward, &right, &upward);

      // helper lambda
      auto direction_check = [&] (const ystl::Vector &to) -> bool {
        trace.Line (path.origin + offset, to, TraceIgnore::None, nullptr, &tr);

        // check if we're hit worldspawn entity
        if (tr.hit == worldspawn && tr.fraction < 1.0f) {
          return true;
        }
        return false;
      };

      if (direction_check (-forward * distance)) {
        accum_weight += 1;
      }

      if (direction_check (right * distance)) {
        accum_weight += 1;
      }

      if (direction_check (-right * distance)) {
        accum_weight += 1;
      }

      if (direction_check (upward * distance)) {
        accum_weight += 1;
      }
    }
    path.flags &= ~NodeFlag::Narrow;

    if (accum_weight > 1) {
      path.flags |= NodeFlag::Narrow;
    }
  }
  narrow_checked_ = true;
}

PointType Graph::FlagToPointType (NodeFlag flag) {
  if (has_flag (flag, NodeFlag::TerroristOnly)) {
    return PointType::Terrorist;
  }
  else if (has_flag (flag, NodeFlag::CTOnly)) {
    return PointType::CT;
  }
  else if (has_flag (flag, NodeFlag::Goal)) {
    return PointType::Goal;
  }
  else if (has_flag (flag, NodeFlag::Camp)) {
    return PointType::Camp;
  }
  else if (has_flag (flag, NodeFlag::Sniper)) {
    return PointType::Sniper;
  }
  else if (has_flag (flag, NodeFlag::Rescue)) {
    return PointType::Rescue;
  }
  return PointType::Count; // invalid
}

void Graph::PopulateNodes () {
  // clear all point arrays
  for (auto &points : points_) {
    points.clear ();
  }
  visited_goals_.clear ();
  node_numbers_.clear ();

  // single allocation instead of ~log2 growth reallocs on every map load
  node_numbers_.reserve (paths_.size ());

  for (const auto &path : paths_) {
    auto point_type = FlagToPointType (static_cast<NodeFlag> (path.flags));

    if (point_type != PointType::Count) {
      points_[ystl::to_underlying (point_type)].push (path.number);
    }
    node_numbers_.push (path.number);
  }
}

bool Graph::ConvertOldFormat () {
  ystl::MemFile fp (bstor.BuildPath (StorageFile::PodbotPWF, true));

  if (!fp) {
    return false;
  }

  LegacyHeader header {};
  ystl::memzero (&header, sizeof (header));

  // save for faster access
  auto map = game.GetMapName ();

  if (fp) {

    if (fp.le_read (header) == 0) { // converted from on-disk little-endian layout
      return false;
    }

    if (strncmp (header.header, kPodbotMagic, ystl::bufsize (kPodbotMagic)) == 0) {
      if (header.file_version != ystl::to_underlying (StorageVersion::Podbot)) {
        return false;
      }
      else if (!ystl::strings.matches (header.map_name, map)) {
        return false;
      }
      else {
        if (header.point_number == 0 || header.point_number > kMaxNodes) {
          return false;
        }
        Reset ();

        for (int i = 0; i < header.point_number; ++i) {
          Path path {};
          LegacyPath podpath {};

          if (fp.le_read (podpath) == 0) { // converted from on-disk little-endian layout
            return false;
          }
          ConvertFromLegacy (path, podpath);

          // more checks of node quality
          if (path.number < 0 || path.number > header.point_number) {
            return false;
          }
          // add to node array
          paths_.push (ystl::move (path));
        }
        fp.close ();

        // save new format in case loaded older one
        if (!paths_.empty ()) {
          Msg ("Converting old PWF to new format Graph.");

          info_.author = header.author;
          info_.header.options = static_cast<int32_t> (StorageOption::Converted);

          // clean editor so graph will be saved with header's author
          auto editor = editor_;
          editor_ = nullptr;

          auto result = SaveGraphData ();
          editor_ = editor;

          return result;
        }
      }
    }
    else {
      return false;
    }
  }
  else {
    return false;
  }
  return false;
}

bool Graph::LoadGraphData () {
  ExtenHeader exten {};
  StorageOption out_options = StorageOption::Invalid;

  // scope the download retry guard to this operation, stale attempts must not poison it
  bstor.ResetRetries ();

  info_.header = {};
  info_.exten = {};

  // re-initialize paths
  Reset ();

  // check if loaded
  const bool data_loaded = bstor.Load<Path> (paths_, &exten, &out_options);

  if (data_loaded) {
    InitBuckets ();

    // reserve table once to avoid regrowth on map load
    hash_table_.reserve (paths_.size ());

    // add data to buckets
    for (const auto &path : paths_) {
      AddToBucket (path.origin, path.number);
    }
    ystl::StringRef author = exten.author;

    if (has_flag (out_options, StorageOption::Official) || author.starts_with ("official") || author.size () < 2) {
      info_.author.assign (product.name);
    }
    else {
      info_.author.assign (author);
    }
    ystl::StringRef modified = exten.modified;

    if (!modified.empty () && !modified.contains ("(none)")) {
      info_.modified.assign (exten.modified);
    }

    vistab.Load (); // load/initialize visibility
    planner.Init (); // initialize our little path planner
    practice.Load (); // load bots practice

    PopulateNodes ();

    if (exten.map_size > 0) {
      const int map_size = GetBspSize ();

      if (map_size > 0 && map_size != exten.map_size) {
        Msg ("Warning: Graph data is probably not for this map. Please check bots behaviour.");
      }
    }

    // notify user about graph problems
    if (planner.IsPathsCheckFailed () && !graph.IsAnalyzed ()) {
      ctrl.Msg ("Warning: Graph data has failed sanity check.");
      ctrl.Msg ("Warning: Bots will use only shortest-path algo for path finding.");
      ctrl.Msg ("Warning: This may significantly affect bots behavior on this map.");
    }
    cv_debug_goal.Set (kInvalidNodeIndex);

    // try to do graph collection, and push them to graph database automatically
    CollectOnline ();

    return true;
  }
  else {
    // binary graph missing, but a text graph may be shipped alongside (see 'yb graph import')
    ystl::MemFile probe (ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.data, folders.graph,
      ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ())));

    if (probe) {
      ctrl.Msg (
        "Binary graph not found, but a text graph exists. Run '%s g import' + '%s g apply' to use it.", product.cmd_pri, product.cmd_pri);
    }
    probe.close ();
    analyzer.Start ();
  }
  return false;
}

bool Graph::CanDownload () {
  return graph_urls.CanDownload ();
}

bool Graph::SaveGraphData () {
  auto options = info_.header.options | StorageOption::Graph | StorageOption::Exten;
  ystl::String editor_name {};

  if (!HasEditor () && !info_.author.empty ()) {
    editor_name = info_.author;

    if (!game.IsDedicatedServer ()) {
      options |= StorageOption::Recovered;
    }
  }
  else if (!game.IsNullEntity (editor_)) {
    editor_name = editor_->v.netname.chars ();
  }
  else {
    editor_name = product.name;
  }

  // mark as analyzed
  if (analyzer.IsAnalyzed ()) {
    options |= StorageOption::Analyzed;
  }

  // mark as official
  if (editor_name.starts_with (product.name)) {
    options |= StorageOption::Official;
  }
  ExtenHeader exten {};

  // only modify the author if no author currently assigned to graph file
  if (info_.author.empty () || ystl::strings.is_empty (info_.exten.author)) {
    ystl::strings.copy (exten.author, editor_name.chars (), ystl::bufsize (exten.author));
  }
  else {
    ystl::strings.copy (exten.author, info_.exten.author, ystl::bufsize (exten.author));
  }

  // only update modified by, if name differs
  if (info_.author != editor_name && !ystl::strings.is_empty (info_.exten.author)) {
    ystl::strings.copy (exten.modified, editor_name.chars (), ystl::bufsize (exten.author));
  }
  exten.map_size = GetBspSize ();

  // ensure narrow places saved into file
  narrow_checked_ = false;
  light_checked_ = false;

  InitNarrowPlaces ();

  return bstor.Save<Path> (paths_, &exten, options);
}

void Graph::SaveOldFormat () {
  LegacyHeader header {};

  ystl::String editor_name {};

  if (!HasEditor () && !info_.author.empty ()) {
    editor_name = info_.author;
  }
  else if (!game.IsNullEntity (editor_)) {
    editor_name = editor_->v.netname.chars ();
  }
  else {
    editor_name = product.name;
  }

  ystl::strings.copy (header.header, kPodbotMagic, sizeof (kPodbotMagic));
  ystl::strings.copy (header.author, editor_name.chars (), ystl::bufsize (header.author));
  ystl::strings.copy (header.map_name, game.GetMapName (), ystl::bufsize (header.map_name));

  header.map_name[31] = 0;
  header.file_version = ystl::to_underlying (StorageVersion::Podbot);
  header.point_number = Length ();

  ystl::File fp {};

  // file was opened
  if (fp.open (bstor.BuildPath (StorageFile::PodbotPWF), "wb")) {
    // write the node header to the file (podbot format is little-endian on disk)
    fp.le_write (header);

    // save the node paths
    for (const auto &path : paths_) {
      LegacyPath pod {};
      ConvertToLegacy (path, pod);

      fp.le_write (pod);
    }
    fp.close ();
  }
  else {
    ystl::logger.error ("Error writing '%s.pwf' node file.", game.GetMapName ());
  }
}

float Graph::CalculateTravelTime (float max_speed, const ystl::Vector &src, const ystl::Vector &origin) {
  // this function returns 2d traveltime to a position

  return origin.distance2d (src) / max_speed;
}

// names for node flags, for the text graph format (narrow is derived, not exported)
static constexpr struct {
  NodeFlag flag;
  ystl::StringRef name;
} kNodeFlagNames[] = {
  { NodeFlag::Button,        "Button"        },
  { NodeFlag::Lift,          "Lift"          },
  { NodeFlag::Crouch,        "Crouch"        },
  { NodeFlag::Crossing,      "Crossing"      },
  { NodeFlag::Goal,          "Goal"          },
  { NodeFlag::Ladder,        "Ladder"        },
  { NodeFlag::Rescue,        "Rescue"        },
  { NodeFlag::Camp,          "Camp"          },
  { NodeFlag::NoHostage,     "NoHostage"     },
  { NodeFlag::DoubleJump,    "DoubleJump"    },
  { NodeFlag::Sniper,        "Sniper"        },
  { NodeFlag::TerroristOnly, "TerroristOnly" },
  { NodeFlag::CTOnly,        "CTOnly"        }
};

// names for graph file options, for the text graph format
static constexpr struct {
  StorageOption option;
  ystl::StringRef name;
} kStorageOptionNames[] = {
  { StorageOption::Analyzed,  "Analyzed"  },
  { StorageOption::Official,  "Official"  },
  { StorageOption::Recovered, "Recovered" },
  { StorageOption::Converted, "Converted" },
  { StorageOption::Imported,  "Imported"  }
};

void Graph::ExportNode (ystl::ConfNode &parent, const Path &path) {
  auto *node = &parent.add_block (ystl::strings.format ("%d", path.number));

  node->add_scalar ("origin", ystl::strings.format ("%.2f %.2f %.2f", path.origin.x, path.origin.y, path.origin.z));
  node->add_scalar ("radius", ystl::strings.format ("%.1f", path.radius));

  // light level is map-dependent, but stored in the binary graph, so keep it for a lossless roundtrip
  if (!ystl::fequal (path.light, kInvalidLightLevel)) {
    node->add_scalar ("light", ystl::strings.format ("%.1f", path.light));
  }

  // plain links go into a comma list, links with velocity/jump data or a non-default distance (e.g
  ystl::String plain {};
  ystl::Array<PathLink> specials {};

  for (const auto &link : path.links) {
    if (link.index == kInvalidNodeIndex) {
      continue;
    }
    bool special = link.flags != 0 || link.velocity.length_sq () > 0.0f;

    if (!special && Exists (link.index)) {
      const auto expected = static_cast<int> (path.origin.distance2d (paths_[link.index].origin));

      // preserve distances that can't be recomputed from origins (3d jump/ladder links)
      special = (link.distance != expected);
    }
    if (special) {
      specials.push (link);
      continue;
    }
    if (!plain.empty ()) {
      plain += ", ";
    }
    plain += ystl::strings.format ("%d", link.index);
  }
  if (!plain.empty ()) {
    node->add_scalar ("links", plain);
  }
  for (const auto &link : specials) {
    auto *special = &node->add_block ("link");

    special->add_scalar ("to", ystl::strings.format ("%d", link.index));
    special->add_scalar ("distance", ystl::strings.format ("%d", link.distance));

    if (link.flags != 0) {
      special->add_scalar ("flags", "Jump");
    }
    if (link.velocity.length_sq () > 0.0f) {
      special->add_scalar ("velocity", ystl::strings.format ("%.2f %.2f %.2f", link.velocity.x, link.velocity.y, link.velocity.z));
    }
  }
  ystl::String flags {};

  for (const auto &entry : kNodeFlagNames) {
    if (has_flag (path.flags, entry.flag)) {
      if (!flags.empty ()) {
        flags += ", ";
      }
      flags += entry.name;
    }
  }
  if (!flags.empty ()) {
    node->add_scalar ("flags", flags);
  }

  // camp direction angles, stored only for camp-style nodes
  if (path.start.length_sq () > 0.0f || path.end.length_sq () > 0.0f) {
    node->add_scalar ("camp", ystl::strings.format ("%.2f %.2f %.2f %.2f", path.start.x, path.start.y, path.end.x, path.end.y));
  }
}

bool Graph::ExportGraphText () {
  if (paths_.empty ()) {
    ctrl.Msg ("Unable to export graph text. Graph is empty.");
    return false;
  }
  ystl::ConfNode doc {};
  auto *meta = &doc.add_block ("Meta");

  meta->add_comment ("text representation of the graph file, edit carefully");
  meta->add_comment ("import with 'yb graph import', write the binary graph with 'yb graph save' afterwards");
  meta->add_scalar ("map", ystl::String (game.GetMapName ()).lowercase ());
  meta->add_scalar ("nodes", ystl::strings.format ("%d", paths_.size ()));

  if (info_.author.empty ()) {
    meta->add_scalar ("author", product.name);
  }
  else {
    meta->add_scalar ("author", info_.author);
  }
  if (!info_.modified.empty ()) {
    meta->add_scalar ("modified", info_.modified);
  }
  // always resolve the live bsp size, a stored zero (e.g. after a text import) must not leak into the export
  auto map_size = GetBspSize ();

  if (map_size <= 0) {
    map_size = info_.exten.map_size;
  }
  if (map_size > 0) {
    meta->add_scalar ("mapSize", ystl::strings.format ("%d", map_size));
  }

  ystl::String options {};

  for (const auto &entry : kStorageOptionNames) {
    if (has_flag (info_.header.options, entry.option)) {
      if (!options.empty ()) {
        options += ", ";
      }
      options += entry.name;
    }
  }
  if (!options.empty ()) {
    meta->add_scalar ("options", options);
  }
  auto *nodes = &doc.add_block ("Nodes");

  for (const auto &path : paths_) {
    ExportNode (*nodes, path);
  }
  const auto text = ystl::ConfWriter::write (doc);
  const auto file_path = ystl::strings.join_path (bstor.GetRunningPath (), folders.data, folders.graph,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));

  ystl::File file (file_path, "wb");

  if (!file) {
    ctrl.Msg ("Unable to write graph text file '%s'.", file_path.chars ());
    return false;
  }
  file.write (text.chars (), text.size (), 1);
  ctrl.Msg ("Graph text exported: '%s' (%d nodes). Commit it to the graphs repository.", file_path.chars (), paths_.size ());

  return true;
}

bool Graph::ImportGraphText (ystl::StringRef file_path) {
  ystl::MemFile file {};

  if (!file.open (file_path)) {
    ctrl.Msg ("Unable to open graph text file '%s'.", file_path.chars ());
    return false;
  }
  ystl::String text {};
  text.assign (file.view ());
  file.close ();

  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ctrl.Msg ("Graph text parse error: %s", parser.error ().chars ());
    return false;
  }
  const auto *doc = &parser.document ();
  const auto *meta = doc->find ("Meta");
  const auto *nodes = doc->find ("Nodes");

  if (!meta || !nodes) {
    ctrl.Msg ("Graph text is missing Meta or Nodes block.");
    return false;
  }
  const auto node_count = meta->get_int ("nodes", -1);

  if (node_count < 1 || node_count > kMaxNodes) {
    ctrl.Msg ("Graph text has invalid nodes count (%d).", node_count);
    return false;
  }
  if (nodes->size () != static_cast<size_t> (node_count)) {
    ctrl.Msg ("Graph text nodes count mismatch (declared %d, found %d).", node_count, nodes->size ());
    return false;
  }
  // first pass: origins, radius and flags, links are resolved on the second pass
  ystl::Array<Path, ReservePolicy::Proportional> paths {};

  paths.resize (static_cast<size_t> (node_count));

  auto node_index = 0;

  for (const auto &entry : nodes->children ()) {
    // node labels are plain decimal indices
    bool numeric = !entry->name ().empty ();

    for (const auto &ch : entry->name ()) {
      if (ch < '0' || ch > '9') {
        numeric = false;
        break;
      }
    }
    const auto declared = numeric ? entry->name ().as<int> () : -1;

    if (declared != node_index) {
      ctrl.Msg ("Graph text has unexpected node label '%s' (expected '%d').", entry->name ().chars (), node_index);
      return false;
    }
    auto &path = paths[static_cast<size_t> (node_index)];

    path.number = node_index;
    path.light = kInvalidLightLevel;
    path.display = 0.0f;
    path.vis = {};

    // zero-initialized links would point at node 0, empty slots must be invalid
    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
      link.distance = 0;
      link.flags = 0;
      link.velocity.clear ();
    }
    const auto *origin = entry->find ("origin");

    if (!origin) {
      ctrl.Msg ("Graph text node %d has no origin.", node_index);
      return false;
    }
    auto origin_parts = origin->as_list ();

    if (origin_parts.size () != 3) {
      ctrl.Msg ("Graph text node %d has invalid origin.", node_index);
      return false;
    }
    path.origin = ystl::Vector (origin_parts[0].as<float> (), origin_parts[1].as<float> (), origin_parts[2].as<float> ());
    path.radius = entry->get_float ("radius", 0.0f);

    // absent light means unknown and is refreshed on save
    path.light = entry->get_float ("light", kInvalidLightLevel);

    if (const auto *flags = entry->find ("flags")) {
      for (auto &token : flags->value ().split<ystl::String> (",")) {
        token.trim ();
        bool found = false;

        for (const auto &named : kNodeFlagNames) {
          if (token == named.name) {
            found = true;
            path.flags |= ystl::to_underlying (named.flag);
            break;
          }
        }
        if (!found) {
          ctrl.Msg ("Graph text node %d has unknown flag '%s'.", node_index, token.chars ());
          return false;
        }
      }
    }
    if (const auto *camp = entry->find ("camp")) {
      auto camp_parts = camp->as_list ();

      if (camp_parts.size () != 4) {
        ctrl.Msg ("Graph text node %d has invalid camp direction.", node_index);
        return false;
      }
      path.start = ystl::Vector (camp_parts[0].as<float> (), camp_parts[1].as<float> (), 0.0f);
      path.end = ystl::Vector (camp_parts[2].as<float> (), camp_parts[3].as<float> (), 0.0f);
    }
    ++node_index;
  }
  // second pass: links, plain links first, then the special (distance/velocity/jump) ones
  for (node_index = 0; node_index < node_count; ++node_index) {
    const auto *entry = nodes->at (static_cast<size_t> (node_index));
    auto &path = paths[static_cast<size_t> (node_index)];
    auto slot = 0;

    if (const auto *links = entry->find ("links")) {
      for (auto &token : links->value ().split<ystl::String> (",")) {
        token.trim ();

        if (token.empty ()) {
          continue;
        }
        const auto to = token.as<int> ();

        if (to < 0 || to >= node_count || to == node_index || slot >= kMaxNodeLinks) {
          ctrl.Msg ("Graph text node %d has invalid link '%s'.", node_index, token.chars ());
          return false;
        }
        path.links[slot].index = static_cast<int16_t> (to);
        path.links[slot].distance = static_cast<int> (path.origin.distance2d (paths[to].origin));
        ++slot;
      }
    }
    for (const auto &child : entry->children ()) {
      if (child->name () != "link") {
        continue;
      }
      const auto to = child->get_int ("to", kInvalidNodeIndex);

      if (to < 0 || to >= node_count || to == node_index || slot >= kMaxNodeLinks) {
        ctrl.Msg ("Graph text node %d has invalid special link (to = %d).", node_index, to);
        return false;
      }
      path.links[slot].index = static_cast<int16_t> (to);

      // explicit distance is written by the exporter; without it (old files) recompute as before
      if (const auto *distance = child->find ("distance")) {
        const auto stored = distance->as_int (-1);

        if (stored < 0) {
          ctrl.Msg ("Graph text node %d has invalid link distance (to = %d).", node_index, to);
          return false;
        }
        path.links[slot].distance = stored;
      }
      else {
        path.links[slot].distance = static_cast<int> (path.origin.distance2d (paths[to].origin));
      }

      if (const auto *flags = child->find ("flags")) {
        for (auto &token : flags->value ().split<ystl::String> (",")) {
          token.trim ();

          if (token.empty ()) {
            continue;
          }
          if (token == "Jump") {
            path.links[slot].flags |= ystl::to_underlying (PathFlag::Jump);
          }
          else {
            ctrl.Msg ("Graph text node %d has unknown link flag '%s'.", node_index, token.chars ());
            return false;
          }
        }
      }
      if (const auto *velocity = child->find ("velocity")) {
        auto velocity_parts = velocity->as_list ();

        if (velocity_parts.size () != 3) {
          ctrl.Msg ("Graph text node %d has invalid link velocity (to = %d).", node_index, to);
          return false;
        }
        path.links[slot].velocity =
          ystl::Vector (velocity_parts[0].as<float> (), velocity_parts[1].as<float> (), velocity_parts[2].as<float> ());
      }
      ++slot;
    }
  }
  bots.DisconnectAll ();
  Reset ();

  paths_ = ystl::move (paths);
  info_.header = {};
  info_.header.length = node_count;
  info_.exten = {};

  auto author = meta->get_string ("author", "");

  if (author.empty () || author.size () < 2) {
    info_.author.assign (product.name);
  }
  else {
    info_.author.assign (author);
  }
  auto modified = meta->get_string ("modified", "");

  if (!modified.empty () && !modified.contains ("(none)")) {
    info_.modified.assign (modified);
  }
  info_.exten.map_size = meta->get_int ("mapSize", 0);
  info_.header.options = ystl::to_underlying (StorageOption::Graph | StorageOption::Exten | StorageOption::Imported);

  ystl::strings.copy (info_.exten.author, info_.author.chars (), ystl::bufsize (info_.exten.author));

  if (!info_.modified.empty ()) {
    ystl::strings.copy (info_.exten.modified, info_.modified.chars (), ystl::bufsize (info_.exten.modified));
  }
  if (const auto *options = meta->find ("options")) {
    for (auto &token : options->value ().split<ystl::String> (",")) {
      token.trim ();

      for (const auto &named : kStorageOptionNames) {
        if (token == named.name) {
          info_.header.options |= ystl::to_underlying (named.option);
          break;
        }
      }
    }
  }
  InitBuckets ();
  hash_table_.reserve (paths_.size ());

  for (const auto &path : paths_) {
    AddToBucket (path.origin, path.number);
  }
  PopulateNodes ();
  planner.Init ();
  has_changed_ = true;

  ctrl.Msg ("Graph text imported: %d nodes. Run '%s graph apply' before adding bots.", node_count, product.cmd_pri);

  return true;
}

bool Graph::IsNodeReacheableEx (const ystl::Vector &src, const ystl::Vector &destination, const float max_height) const {
  Trace::Result tr {};

  float distance_sq = destination.distance_sq (src);

  if ((destination.z - src.z) >= 45.0f) {
    return false;
  }

  // is the destination not close enough?
  if (distance_sq > ystl::sqrf (auto_path_distance_)) {
    return false;
  }

  // check if we go through a func_illusionary, in which case return false
  trace.Hull (src, destination, TraceIgnore::Monsters, head_hull, editor_, &tr);

  if (tr.hit && tr.hit->v.classname.str () == "func_illusionary") {
    return false; // don't add path nodes through func_illusionaries
  }

  // check if this node is "visible"
  trace.Line (src, destination, TraceIgnore::Monsters, editor_, &tr);

  const bool is_door = game.IsDoorEntity (tr.hit);

  // if node is visible from current position (even behind head)
  if (trace.IsEndpointClear (tr) || is_door) {
    // if it's a door check if nothing blocks behind
    if (is_door) {
      trace.Line (tr.end_pos, destination, TraceIgnore::Monsters, tr.hit, &tr);

      if (tr.fraction < 1.0f) {
        return false;
      }
    }

    // check for special case of both nodes being in water
    if (engfuncs.pfnPointContents (src) == CONTENTS_WATER && engfuncs.pfnPointContents (destination) == CONTENTS_WATER) {
      return true; // then they're reachable each other
    }

    // is dest node higher than src? (45 is max jump height)
    if (destination.z > src.z + 45.0f) {
      ystl::Vector source_new = destination;
      ystl::Vector destination_new = destination;
      destination_new.z = destination_new.z - 50.0f; // straight down 50 units

      trace.Line (source_new, destination_new, TraceIgnore::Monsters, editor_, &tr);

      // check if we didn't hit anything, if not then it's in mid-air
      if (tr.fraction >= 1.0f) {
        return false; // can't reach this one
      }
    }

    // reject path if ground drops more than step height along it
    ystl::Vector direction = (destination - src).normalize (); // 1 unit long
    ystl::Vector check = src, down = src;

    down.z = down.z - 1000.0f; // straight down 1000 units

    trace.Line (check, down, TraceIgnore::Monsters, editor_, &tr);

    float last_height = tr.fraction * 1000.0f; // height from ground
    distance_sq = destination.distance_sq (check); // distance from goal

    while (distance_sq > ystl::sqrf (10.0f)) {
      // move 10 units closer to the goal
      check = check + (direction * 10.0f);

      down = check;
      down.z = down.z - 1000.0f; // straight down 1000 units

      trace.Line (check, down, TraceIgnore::Monsters, editor_, &tr);

      const float height = tr.fraction * 1000.0f; // height from ground

      // is the current height greater than the step height?
      if (height < last_height - max_height) {
        return false; // can't get there without jumping
      }
      last_height = height;
      distance_sq = destination.distance_sq (check); // distance from goal
    }
    return true;
  }
  return false;
}

bool Graph::IsNodeReacheable (const ystl::Vector &src, const ystl::Vector &destination) const {
  return IsNodeReacheableEx (src, destination, 45.0f);
}

bool Graph::IsNodeReacheableWithJump (const ystl::Vector &src, const ystl::Vector &destination) const {
  // validates a real jump arc, ground continuity is not required (gaps allowed)
  static constexpr float kJumpApproachSpeed = 260.0f; // fastest horizontal entry bots can use
  static constexpr float kArcStep = 12.0f; // arc sampling density

  if (destination.z - src.z > cv_graph_analyze_max_jump_height.As<float> ()) {
    return false;
  }

  if (src.distance_sq (destination) > ystl::sqrf (auto_path_distance_)) {
    return false;
  }
  const float gravity = sv_gravity.As<float> ();

  if (ystl::fzero (gravity)) {
    return false;
  }
  const ystl::Vector delta = destination - src;
  const float dist2d = delta.length2d ();

  // airtime from vertical motion, larger root lands on target height
  const float disc = kPlayerJumpTakeoff * kPlayerJumpTakeoff - 2.0f * gravity * delta.z;

  if (disc < 0.0f) {
    return false; // target too high for a jump
  }
  const float airtime = (kPlayerJumpTakeoff + ystl::sqrtf (disc)) / gravity;

  if (airtime <= 0.0f || dist2d / airtime > kJumpApproachSpeed) {
    return false; // unmakable without strafe skills bots lack
  }
  const ystl::Vector horizontal = ystl::Vector { delta.x, delta.y, 0.0f } * (1.0f / airtime);

  // swept hull along the arc, thin walls included by density
  const int samples = ystl::clamp (static_cast<int> (delta.length () / kArcStep), 4, 24);
  ystl::Vector prev = src;
  Trace::Result tr {};

  for (int i = 1; i <= samples; ++i) {
    const float t = airtime * static_cast<float> (i) / static_cast<float> (samples);
    ystl::Vector pos = src + horizontal * t;
    pos.z = src.z + kPlayerJumpTakeoff * t - 0.5f * gravity * t * t;

    trace.Hull (prev, pos, TraceIgnore::Monsters, head_hull, nullptr, &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      return false;
    }
    prev = pos;
  }
  return true;
}

void Graph::Frame () {
  // this function executes frame of graph operation code

  if (game.IsNullEntity (editor_)) {
    return; // this function is only valid with editor, and in graph enabled mode
  }

  // keep the clipping mode enabled, or it can be turned off after new round has started
  if (graph.HasEditFlag (GraphEdit::Noclip) && game.IsAliveEntity (editor_)) {
    editor_->v.movetype = MOVETYPE_NOCLIP;
  }

  float nearest_distance_sq = kInfiniteDistance;
  int nearest_index = kInvalidNodeIndex;

  // check if it's time to add jump node
  if (jump_learn_node_) {
    if (!end_jump_point_) {
      if (editor_->v.button & IN_JUMP) {
        Add (NodeAddFlag::JumpStart);

        time_jump_started_ = game.Time ();
        end_jump_point_ = true;
      }
      else {
        learn_velocity_ = editor_->v.velocity;
        learn_position_ = editor_->v.origin;
      }
    }
    else if (((editor_->v.flags & FL_ONGROUND) || editor_->v.movetype == MOVETYPE_FLY) && time_jump_started_ + 0.1f < game.Time ()) {
      Add (NodeAddFlag::JumpEnd);

      jump_learn_node_ = false;
      end_jump_point_ = false;
    }
  }

  // check if it's a auto-add-node mode enabled
  if (HasEditFlag (GraphEdit::Auto) && (editor_->v.flags & (FL_ONGROUND | FL_PARTIALGROUND))) {
    // find the distance from the last used node
    float distance_sq = last_node_.distance_sq (editor_->v.origin);

    if (distance_sq > ystl::sqrf (128.0f)) {
      // check that no other reachable nodes are nearby
      for (const auto &path : paths_) {
        if (IsNodeReacheable (editor_->v.origin, path.origin)) {
          distance_sq = path.origin.distance_sq (editor_->v.origin);

          if (distance_sq < nearest_distance_sq) {
            nearest_distance_sq = distance_sq;
          }
        }
      }

      // make sure nearest node is far enough away
      if (nearest_distance_sq >= ystl::sqrf (128.0f)) {
        Add (NodeAddFlag::Normal); // place a node here
      }
    }
  }
  facing_at_index_ = GetFacingIndex ();

  // reset the minimal distance changed before
  nearest_distance_sq = kInfiniteDistance;

  // now iterate through all nodes in a map, and draw required ones
  for (auto &path : paths_) {
    const float distance_sq = path.origin.distance_sq (editor_->v.origin);

    // check if node is within a distance, and is visible
    if (distance_sq < ystl::sqrf (cv_graph_draw_distance.As<float> ()) &&
        ((util.IsVisible (path.origin, editor_) && util.IsInViewCone (path.origin, editor_)) || !game.IsAliveEntity (editor_) ||
          distance_sq < ystl::sqrf (64.0f))) {

      // check the distance
      if (distance_sq < nearest_distance_sq) {
        nearest_index = path.number;
        nearest_distance_sq = distance_sq;
      }

      if (path.display + 1.0f < game.Time ()) {
        float node_height = 0.0f;

        // check the node height
        if (has_flag (path.flags, NodeFlag::Crouch)) {
          node_height = 36.0f;
        }
        else {
          node_height = 72.0f;
        }
        const float node_half_height = node_height * 0.5f;

        // all nodes are by default are green
        ystl::Color node_color { -1, -1, -1 };

        // colorize all other nodes
        if (has_flag (path.flags, NodeFlag::Goal)) {
          node_color = { 128, 0, 255 };
        }
        else if (has_flag (path.flags, NodeFlag::Ladder)) {
          node_color = { 128, 64, 0 };
        }
        else if (has_flag (path.flags, NodeFlag::Rescue)) {
          node_color = { 255, 255, 255 };
        }
        else if (has_flag (path.flags, NodeFlag::Camp)) {
          if (has_flag (path.flags, NodeFlag::TerroristOnly)) {
            node_color = { 255, 160, 160 };
          }
          else if (has_flag (path.flags, NodeFlag::CTOnly)) {
            node_color = { 160, 160, 255 };
          }
          else {
            node_color = { 0, 255, 255 };
          }
        }
        else if (has_flag (path.flags, NodeFlag::TerroristOnly)) {
          node_color = { 255, 0, 0 };
        }
        else if (has_flag (path.flags, NodeFlag::CTOnly)) {
          node_color = { 0, 0, 255 };
        }
        else {
          node_color = { 0, 255, 0 };
        }

        // colorize additional flags
        ystl::Color node_flag_color { -1, -1, -1 };

        // check the colors
        if (has_flag (path.flags, NodeFlag::Sniper)) {
          node_flag_color = { 130, 87, 0 };
        }
        else if (has_flag (path.flags, NodeFlag::NoHostage)) {
          node_flag_color = { 255, 255, 255 };
        }
        else if (has_flag (path.flags, NodeFlag::Lift)) {
          node_flag_color = { 255, 0, 255 };
        }
        int node_width = 14;

        if (Exists (facing_at_index_) && path.number == facing_at_index_) {
          node_width *= 2;
        }

        // draw node without additional flags
        if (node_flag_color.red == -1) {
          game.DrawLine (editor_, path.origin - ystl::Vector (0, 0, node_half_height), path.origin + ystl::Vector (0, 0, node_half_height),
            node_width + 1, 0, node_color, 250, 0, 10);
        }

        // draw node with flags
        else {
          game.DrawLine (editor_, path.origin - ystl::Vector (0, 0, node_half_height),
            path.origin - ystl::Vector (0, 0, node_half_height - node_height * 0.75f), node_width, 0, node_color, 250, 0, 10); // draw basic path
          game.DrawLine (editor_, path.origin - ystl::Vector (0, 0, node_half_height - node_height * 0.75f),
            path.origin + ystl::Vector (0, 0, node_half_height), node_width, 0, node_flag_color, 250, 0, 10); // draw additional path
        }
        path.display = game.Time ();
      }
    }
  }

  if (nearest_index == kInvalidNodeIndex) {
    return;
  }

  // draw arrow to a some importaint nodes
  if (Exists (find_wp_index_) || Exists (cache_node_index_) || Exists (facing_at_index_)) {
    // check for drawing code
    if (arrow_display_timer_.greater_than (0.5f)) {

      // finding node - pink arrow
      if (find_wp_index_ != kInvalidNodeIndex) {
        game.DrawLine (editor_, editor_->v.origin, paths_[find_wp_index_].origin, 10, 0, { 128, 0, 128 }, 200, 0, 5, DrawLineType::Arrow);
      }

      // cached node - yellow arrow
      if (cache_node_index_ != kInvalidNodeIndex) {
        game.DrawLine (editor_, editor_->v.origin, paths_[cache_node_index_].origin, 10, 0, { 255, 255, 0 }, 200, 0, 5, DrawLineType::Arrow);
      }

      // node user facing at - white arrow
      if (facing_at_index_ != kInvalidNodeIndex) {
        game.DrawLine (editor_, editor_->v.origin, paths_[facing_at_index_].origin, 10, 0, { 255, 255, 255 }, 200, 0, 5, DrawLineType::Arrow);
      }
      arrow_display_timer_.start ();
    }
  }

  // draw a paths, camplines and danger directions for nearest node
  if (nearest_distance_sq < ystl::sqrf (ystl::clamp (paths_[nearest_index].radius, 56.0f, 90.0f)) && path_display_timer_.elapsed ()) {
    constexpr float kPathDisplayRefresh = 0.96f;
    constexpr float kPathDisplayHold = kPathDisplayRefresh * 2.0f;

    path_display_timer_.start (kPathDisplayRefresh);

    // create path pointer for faster access
    const auto &path = paths_[nearest_index];

    // draw the camplines
    if (has_flag (path.flags, NodeFlag::Camp)) {
      float height = 36.0f;

      // check if it's a source
      if (has_flag (path.flags, NodeFlag::Crouch)) {
        height = 18.0f;
      }
      const ystl::Vector source = ystl::Vector (path.origin.x, path.origin.y, path.origin.z + height); // source
      const ystl::Vector start = path.origin + ystl::Vector (path.start.x, path.start.y, 0.0f).forward () * 500.0f; // camp start
      const ystl::Vector end = path.origin + ystl::Vector (path.end.x, path.end.y, 0.0f).forward () * 500.0f; // camp end

      // draw it now
      game.DrawLine (editor_, source, start, 10, 0, { 255, 0, 0 }, 200, 0, 10);
      game.DrawLine (editor_, source, end, 10, 0, { 255, 0, 0 }, 200, 0, 10);
    }

    // draw the connections
    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
        continue;
      }
      // jump connection
      if (has_flag (link.flags, PathFlag::Jump)) {
        game.DrawLine (editor_, path.origin, paths_[link.index].origin, 5, 0, { 255, 0, 128 }, 200, 0, 10);
      }
      else if (IsConnected (link.index, nearest_index)) { // twoway connection
        game.DrawLine (editor_, path.origin, paths_[link.index].origin, 5, 0, { 255, 255, 0 }, 200, 0, 10);
      }
      else { // oneway connection
        game.DrawLine (editor_, path.origin, paths_[link.index].origin, 5, 0, { 255, 255, 255 }, 200, 0, 10);
      }
    }

    // now look for oneway incoming connections
    for (const auto &connected : paths_) {
      if (IsConnected (connected.number, path.number) && !IsConnected (path.number, connected.number)) {
        game.DrawLine (editor_, path.origin, connected.origin, 5, 0, { 0, 192, 96 }, 200, 0, 10);
      }
    }

    // draw the radius circle
    ystl::Vector origin = has_flag (path.flags, NodeFlag::Crouch) ? path.origin : path.origin - ystl::Vector (0.0f, 0.0f, 18.0f);
    constexpr ystl::Color radius_color { 36, 36, 255 };

    // if radius is nonzero, draw a full circle
    if (path.radius > 0.0f) {
      const float sqr = ystl::sqrtf (ystl::sqrf (path.radius) * 0.5f);

      const ystl::Vector points[] = {
        { path.radius,  0.0f,         0.0f },
        { sqr,          -sqr,         0.0f },
        { 0.0f,         -path.radius, 0.0f },
        { -sqr,         -sqr,         0.0f },
        { -path.radius, 0.0f,         0.0f },
        { -sqr,         sqr,          0.0f },
        { 0.0f,         path.radius,  0.0f },
        { sqr,          sqr,          0.0f }
      };

      for (auto i = 0; i < kMaxNodeLinks; ++i) {
        game.DrawLine (editor_, origin + points[i], origin + points[(i + 1) % 8], 5, 0, radius_color, 200, 0, 10);
      }
    }
    else {
      const float sqr = ystl::sqrtf (32.0f);

      game.DrawLine (editor_, origin + ystl::Vector (sqr, -sqr, 0.0f), origin + ystl::Vector (-sqr, sqr, 0.0f), 5, 0, radius_color, 200, 0, 10);
      game.DrawLine (editor_, origin + ystl::Vector (-sqr, -sqr, 0.0f), origin + ystl::Vector (sqr, sqr, 0.0f), 5, 0, radius_color, 200, 0, 10);
    }

    // draw the danger directions
    if (!has_changed_) {
      const int danger_index_t = practice.GetIndex (Team::Terrorist, nearest_index, nearest_index);
      const int danger_index_ct = practice.GetIndex (Team::CT, nearest_index, nearest_index);

      if (Exists (danger_index_t)) {
        game.DrawLine (editor_, path.origin, paths_[danger_index_t].origin, 15, 0, { 255, 0, 0 }, 200, 0, 10,
          DrawLineType::Arrow); // draw a red arrow to this index's danger point
      }
      if (Exists (danger_index_ct)) {
        game.DrawLine (editor_, path.origin, paths_[danger_index_ct].origin, 15, 0, { 0, 0, 255 }, 200, 0, 10,
          DrawLineType::Arrow); // draw a blue arrow to this index's danger point
      }
    }
    // editor panels live on fixed hud slots, hold covers two refresh periods
    auto send_hud_message = [&] (ystl::Color color, float x, float y, int channel, ystl::StringRef text) {
      hudtextparms_t text_params {
        .x = x,
        .y = y,
        .effect = 0,
        .r1 = static_cast<uint8_t> (color.red),
        .g1 = static_cast<uint8_t> (color.green),
        .b1 = static_cast<uint8_t> (color.blue),
        .a1 = static_cast<uint8_t> (1),
        .r2 = static_cast<uint8_t> (color.red),
        .g2 = static_cast<uint8_t> (color.green),
        .b2 = static_cast<uint8_t> (color.blue),
        .a2 = static_cast<uint8_t> (1),
        .fadeinTime = 0.0f,
        .fadeoutTime = 0.0f,
        .holdTime = kPathDisplayHold,
        .fxTime = 0.0f,
        .channel = channel,
      };

      game.SendHudMessage (editor_, text_params, text);
    };

    // very helpful stuff
    auto get_node_data = [this] (ystl::StringRef type, int node) -> ystl::String {
      const auto &p = paths_[node];

      // check if any link is a jump path
      bool jump_point = false;

      for (const auto &link : p.links) {
        if (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump)) {
          jump_point = true;
          break;
        }
      }

      // build flags string
      ystl::String flags {};

      auto add_flag = [&flags, &p] (NodeFlag flag, ystl::StringRef name) {
        if (has_flag (p.flags, flag)) {
          flags += " ";
          flags += name;
        }
      };

      add_flag (NodeFlag::Lift, "LIFT");
      add_flag (NodeFlag::Crouch, "CROUCH");
      add_flag (NodeFlag::Camp, "CAMP");
      add_flag (NodeFlag::TerroristOnly, "TERRORIST");
      add_flag (NodeFlag::CTOnly, "CT");
      add_flag (NodeFlag::Sniper, "SNIPER");
      add_flag (NodeFlag::Goal, "GOAL");
      add_flag (NodeFlag::Ladder, "LADDER");
      add_flag (NodeFlag::Rescue, "RESCUE");
      add_flag (NodeFlag::DoubleJump, "JUMPHELP");
      add_flag (NodeFlag::NoHostage, "NOHOSTAGE");

      if (jump_point) {
        flags += " JUMP";
      }

      if (flags.empty ()) {
        flags.assign ("(none)");
      }

      // format the complete message
      ystl::String message {};
      message.assignf ("      %s node:\n"
                       "       Node %d of %d, Radius: %.1f, Light: %s\n"
                       "       Flags: %s\n"
                       "       Origin: (%.1f, %.1f, %.1f)\n",
        type, node, paths_.size () - 1, p.radius,
        ystl::fequal (p.light, kInvalidLightLevel) ? "Invalid" : ystl::strings.format ("%1.f", p.light), flags, p.origin.x, p.origin.y,
        p.origin.z);

      return message;
    };

    // display some information
    send_hud_message ({ 255, 255, 255 }, 0.0f, 0.025f, 3, get_node_data ("Current", nearest_index));

    // check if we need to show the cached point index
    if (cache_node_index_ != kInvalidNodeIndex) {
      send_hud_message ({ 255, 255, 255 }, 0.28f, 0.16f, 4, get_node_data ("Cached", cache_node_index_));
    }

    // check if we need to show the facing point index
    if (facing_at_index_ != kInvalidNodeIndex) {
      send_hud_message ({ 255, 255, 255 }, 0.28f, 0.025f, 5, get_node_data ("Facing", facing_at_index_));
    }
    ystl::String time_message = ystl::strings.format ("      Map: %s, Time: %s\n", game.GetMapName (), util.GetCurrentDateTime ());

    // if node is not changed display experience also
    if (!has_changed_) {
      const int danger_index_ct = practice.GetIndex (Team::CT, nearest_index, nearest_index);
      const int danger_index_t = practice.GetIndex (Team::Terrorist, nearest_index, nearest_index);

      ystl::String practice_text {};
      practice_text.assignf ("      Node practice data (index / damage):\n"
                             "       CT: %d / %d\n"
                             "       T:  %d / %d\n\n",
        danger_index_ct, danger_index_ct != kInvalidNodeIndex ? practice.GetDamage (Team::CT, nearest_index, danger_index_ct) : 0,
        danger_index_t, danger_index_t != kInvalidNodeIndex ? practice.GetDamage (Team::Terrorist, nearest_index, danger_index_t) : 0);

      send_hud_message ({ 255, 255, 255 }, 0.0f, 0.16f, 6, practice_text + time_message);
    }
    else {
      send_hud_message ({ 255, 255, 255 }, 0.0f, 0.16f, 6, time_message);
    }
  }
}

bool Graph::IsConnected (int index) {
  for (const auto &path : paths_) {
    if (path.number == index) {
      continue;
    }

    for (const auto &test : path.links) {
      if (test.index == index) {
        return true;
      }
    }
  }
  return false;
}

void Graph::ComputeForwardReachability (ystl::Array<bool> &forward) {
  const auto n = Length ();

  forward.resize (static_cast<size_t> (n));
  ystl::fill (forward, false);

  if (n < 1 || !Exists (0)) {
    return;
  }
  PathWalk walk {}; // stack for depth-first traversal
  walk.Init (static_cast<size_t> (n));

  forward[0] = true;
  walk.Add (0);

  while (!walk.Empty ()) {
    const int current = walk.First ();
    walk.Shift ();

    for (const auto &link : paths_[current].links) {
      if (Exists (link.index) && !forward[link.index]) {
        forward[link.index] = true;
        walk.Add (link.index);
      }
    }
  }
}

void Graph::ComputeBackwardReachability (ystl::Array<bool> &backward) {
  const auto n = Length ();

  backward.resize (static_cast<size_t> (n));
  ystl::fill (backward, false);

  if (n < 1 || !Exists (0)) {
    return;
  }
  PathWalk walk {}; // stack for depth-first traversal
  walk.Init (static_cast<size_t> (n));

  // reverse traversal over incoming links
  backward[0] = true;
  walk.Add (0);

  while (!walk.Empty ()) {
    const int current = walk.First ();
    walk.Shift ();

    for (int i = 0; i < n; ++i) {
      if (i == current || !Exists (i) || backward[i]) {
        continue;
      }

      for (const auto &link : paths_[i].links) {
        if (link.index == current) {
          backward[i] = true;
          walk.Add (i);
          break;
        }
      }
    }
  }
}

void Graph::ComputeReachability (ystl::Array<bool> &forward, ystl::Array<bool> &backward) {
  ComputeForwardReachability (forward);
  ComputeBackwardReachability (backward);
}

bool Graph::CheckNodes (bool teleport_player, bool only_paths) {
  auto teleport = [&] (const Path &path) -> void {
    if (teleport_player) {
      engfuncs.pfnSetOrigin (editor_, path.origin);
      SetEditFlag (GraphEdit::On | GraphEdit::Noclip);
    }
  };
  const bool show_errors = !only_paths;

  int terr_points = 0;
  int ct_points = 0;
  int goal_points = 0;
  int rescue_points = 0;

  for (const auto &path : paths_) {
    int connections = 0;

    if (path.number != static_cast<int> (paths_.index (path))) {
      if (show_errors) {
        Msg ("Node %d path differs from index %d.", path.number, paths_.index (path));
      }
      return false;
    }

    for (const auto &test : path.links) {
      if (test.index != kInvalidNodeIndex) {
        if (!Exists (test.index)) {
          if (show_errors) {
            Msg ("Node %d connected with invalid node %d.", path.number, test.index);
          }
          return false;
        }
        ++connections;
        break;
      }
    }

    if (connections == 0) {
      if (!IsConnected (path.number)) {
        if (show_errors) {
          Msg ("Node %d isn't connected with any other node.", path.number);
        }
        return false;
      }
    }

    if (has_flag (path.flags, NodeFlag::Camp)) {
      if (path.end.empty ()) {
        if (show_errors) {
          Msg ("Node %d camp-endposition not set.", path.number);
        }
        return false;
      }
    }
    else if (has_flag (path.flags, NodeFlag::TerroristOnly)) {
      ++terr_points;
    }
    else if (has_flag (path.flags, NodeFlag::CTOnly)) {
      ++ct_points;
    }
    else if (has_flag (path.flags, NodeFlag::Goal)) {
      ++goal_points;
    }
    else if (has_flag (path.flags, NodeFlag::Rescue)) {
      ++rescue_points;
    }

    for (const auto &test : path.links) {
      if (test.index != kInvalidNodeIndex) {
        if (!Exists (test.index)) {
          if (show_errors) {
            teleport (path);

            Msg ("Node %d path index %d out of range.", path.number, test.index);
          }
          return false;
        }
        else if (test.index == path.number) {
          if (show_errors) {
            teleport (path);

            Msg ("Node %d path index %d points to itself.", path.number, test.index);
          }
          return false;
        }
      }
    }
  }

  if (!only_paths && game.MapIs (MapFlags::HostageRescue)) {
    if (rescue_points == 0) {
      Msg ("You didn't set a rescue point.");
      return false;
    }
  }

  // only check paths, but not necessity of different nodes
  if (!only_paths) {
    if (terr_points == 0) {
      Msg ("You didn't set any terrorist important point.");
      return false;
    }
    else if (ct_points == 0) {
      Msg ("You didn't set any CT important point.");
      return false;
    }
    else if (goal_points == 0) {
      Msg ("You didn't set any goal point.");
      return false;
    }
  }

  // single shared traversal for both directions
  ystl::Array<bool> forward {}, backward {};
  ComputeReachability (forward, backward);

  for (const auto &path : paths_) {
    if (!forward[path.number]) {
      if (show_errors) {
        Msg ("Path broken from node 0 to node %d.", path.number);
      }
      teleport (path);

      return false;
    }
  }

  for (const auto &path : paths_) {
    if (!backward[path.number]) {
      if (show_errors) {
        teleport (path);

        Msg ("Path broken from node %d to node 0.", path.number);
      }
      return false;
    }
  }
  return true;
}

void Graph::SetVisited (int index) {
  if (!Exists (index)) {
    return;
  }
  if (!IsVisited (index) && has_flag (paths_[index].flags, NodeFlag::Goal)) {
    visited_goals_.push (index);
  }
}

void Graph::ClearVisited () {
  visited_goals_.clear ();
}

bool Graph::IsVisited (int index) {
  for (auto &visited : visited_goals_) {
    if (visited == index) {
      return true;
    }
  }
  return false;
}

void Graph::SeedBasicNodes () {
  // this function creates basic node types on map

  // first of all, if map contains ladder points, create it
  game.SearchEntities ("classname", "func_ladder", [&] (edict_t *ent) {
    ystl::Vector ladder_left = ent->v.absmin;
    ystl::Vector ladder_right = ent->v.absmax;
    ladder_left.z = ladder_right.z;

    Trace::Result tr {};
    ystl::Vector up {}, down {}, front {}, back {};

    ystl::Vector diff = ((ladder_left - ladder_right) ^ nullptr) * 15.0f;
    front = back = game.GetEntityOrigin (ent);

    front = front + diff; // front
    back = back - diff; // back

    up = down = front;
    down.z = ent->v.absmax.z;

    trace.Hull (down, up, TraceIgnore::Monsters, point_hull, nullptr, &tr);

    if (engfuncs.pfnPointContents (up) == CONTENTS_SOLID || !ystl::fequal (tr.fraction, 1.0f)) {
      up = down = back;
      down.z = ent->v.absmax.z;
    }

    trace.Hull (down, up - ystl::Vector (0.0f, 0.0f, 1000.0f), TraceIgnore::Monsters, point_hull, nullptr, &tr);
    up = tr.end_pos;

    ystl::Vector point = up + ystl::Vector (0.0f, 0.0f, 39.0f);
    is_on_ladder_ = true;

    do {
      if (GetNearestNoBuckets (point, 50.0f) == kInvalidNodeIndex) {
        Add (NodeAddFlag::NoHostage, point);
      }
      point.z += 160.0f;
    } while (point.z < down.z - 40.0f);

    point = down + ystl::Vector (0.0f, 0.0f, 38.0f);

    if (GetNearestNoBuckets (point, 50.0f) == kInvalidNodeIndex) {
      Add (NodeAddFlag::NoHostage, point);
    }
    is_on_ladder_ = false;

    return EntitySearchResult::Continue;
  });

  auto auto_create_for_entity = [] (NodeAddFlag type, ystl::StringRef classname) {
    game.SearchEntities ("classname", classname, [&] (edict_t *ent) {
      ystl::Vector pos = game.GetEntityOrigin (ent);

      Trace::Result tr {};
      trace.Line (pos, pos - ystl::Vector (0.0f, 0.0f, 999.0f), TraceIgnore::Monsters, nullptr, &tr);
      tr.end_pos.z += 36.0f;

      if (graph.GetNearestNoBuckets (tr.end_pos, 50.0f) == kInvalidNodeIndex) {
        graph.Add (type, tr.end_pos);
      }
      return EntitySearchResult::Continue;
    });
  };

  auto_create_for_entity (NodeAddFlag::Normal, "info_player_deathmatch"); // then terrortist spawnpoints
  auto_create_for_entity (NodeAddFlag::Normal, "info_player_start"); // then add ct spawnpoints
  auto_create_for_entity (NodeAddFlag::Normal, "info_vip_start"); // then vip spawnpoint

  auto_create_for_entity (NodeAddFlag::Normal, "armoury_entity"); // weapons on the map ?

  auto_create_for_entity (NodeAddFlag::Rescue, "func_hostage_rescue"); // hostage rescue zone
  auto_create_for_entity (NodeAddFlag::Rescue, "info_hostage_rescue"); // hostage rescue zone (same as above)

  auto_create_for_entity (NodeAddFlag::Goal, "func_bomb_target"); // bombspot zone
  auto_create_for_entity (NodeAddFlag::Goal, "info_bomb_target"); // bombspot zone (same as above)

  auto_create_for_entity (NodeAddFlag::Goal, "hostage_entity"); // hostage entities
  auto_create_for_entity (NodeAddFlag::Goal, "monster_scientist"); // hostage entities (same as above)

  auto_create_for_entity (NodeAddFlag::Goal, "func_vip_safetyzone"); // vip rescue (safety) zone
  auto_create_for_entity (NodeAddFlag::Goal, "func_escapezone"); // terrorist escape zone
}

void Graph::StartLearnJump () {
  jump_learn_node_ = true;
}

void Graph::SetSearchIndex (int index) {
  find_wp_index_ = index;

  if (Exists (find_wp_index_)) {
    Msg ("Showing direction to node %d.", find_wp_index_);
  }
  else {
    find_wp_index_ = kInvalidNodeIndex;
  }
}

Graph::Graph () {
  end_jump_point_ = false;
  jump_learn_node_ = false;
  has_changed_ = false;
  narrow_checked_ = false;
  light_checked_ = false;
  time_jump_started_ = 0.0f;

  last_jump_node_ = kInvalidNodeIndex;
  cache_node_index_ = kInvalidNodeIndex;
  find_wp_index_ = kInvalidNodeIndex;
  facing_at_index_ = kInvalidNodeIndex;
  is_on_ladder_ = false;

  edit_flags_ = GraphEdit::Off;

  path_display_timer_.invalidate ();
  arrow_display_timer_.invalidate ();
  auto_path_distance_ = 250.0f;

  editor_ = nullptr;
}

void Graph::EraseFromBucket (const ystl::Vector &pos, int index) {
  auto &data = hash_table_[LocateBucket (pos)];

  // node indices are unique per bucket, so at most one entry is removed
  data.erase_if ([index] (int value) {
    return value == index;
  });
}

int Graph::LocateBucket (const ystl::Vector &pos) const {
  static constexpr int32_t kFullWidth = 8192;
  static constexpr int32_t kHalfWidth = kFullWidth / 2;

  static constexpr uint32_t kMask = 0x007f80U;
  static constexpr uint32_t kSeed = 0x9e3779b9U;

  static constexpr int32_t kBucketBits = 7;

  auto hash_axis = [] (float axis, int32_t shift) -> int32_t {
    auto ah = ystl::clamp (static_cast<int32_t> (axis), -kHalfWidth, kHalfWidth);
    ah += kFullWidth;

    auto uh = static_cast<uint32_t> (ah);

    uh = (uh & kMask) >> shift;
    uh = (uh * kSeed) ^ (uh >> 7);

    return static_cast<int32_t> (uh) & 0x7f;
  };

  const int32_t hx = hash_axis (pos.x, 15);
  const int32_t hy = hash_axis (pos.y, 7);

  return hx + (hy << kBucketBits);
}

void Graph::UnassignPath (int from, int to) {
  auto &link = paths_[from].links[to];

  link = PathLink {
    .velocity = {},
    .distance = 0,
    .flags = 0,
    .index = kInvalidNodeIndex,
  };

  SetEditFlag (GraphEdit::On);
  has_changed_ = true;
}

void Graph::ConvertFromLegacy (Path &path, const LegacyPath &pod) const {
  path.number = pod.number;
  path.flags = pod.flags;
  path.origin = pod.origin;
  path.start = ystl::Vector (pod.csx, pod.csy, 0.0f);
  path.end = ystl::Vector (pod.cex, pod.cey, 0.0f);

  if (cv_graph_fixcamp) {
    ConvertCampDirection (path);
  }
  path.radius = pod.radius;
  path.light = kInvalidLightLevel;
  path.display = 0.0f;

  for (int i = 0; i < kMaxNodeLinks; ++i) {
    path.links[i].index = pod.index[i];
    path.links[i].distance = pod.distance[i];
    path.links[i].flags = pod.conflags[i];
    path.links[i].velocity = pod.velocity[i];
  }
  path.vis.stand = 0;
  path.vis.crouch = 0;
}

void Graph::ConvertToLegacy (const Path &path, LegacyPath &pod) {
  pod.number = path.number;
  pod.flags = path.flags;
  pod.origin = path.origin;
  pod.radius = path.radius;

  pod.csx = path.start.x;
  pod.csy = path.start.y;
  pod.cex = path.end.x;
  pod.cey = path.end.y;

  for (int i = 0; i < kMaxNodeLinks; ++i) {
    pod.index[i] = path.links[i].index;
    pod.distance[i] = path.links[i].distance;
    pod.conflags[i] = path.links[i].flags;
    pod.velocity[i] = path.links[i].velocity;
  }
  pod.vis.stand = path.vis.stand;
  pod.vis.crouch = path.vis.crouch;
}

void Graph::ConvertCampDirection (Path &path) const {
  // convert old vector camp directions to angles

  if (paths_.empty ()) {
    return;
  }
  const ystl::Vector offset = path.origin + ystl::Vector (0.0f, 0.0f, has_flag (path.flags, NodeFlag::Crouch) ? 15.0f : 17.0f);

  path.start = (ystl::Vector (path.start.x, path.start.y, path.origin.z) - offset).angles ();
  path.end = (ystl::Vector (path.end.x, path.end.y, path.origin.z) - offset).angles ();

  path.start.x = -path.start.x;
  path.end.x = -path.end.x;

  path.start.clamp_angles ();
  path.end.clamp_angles ();
}

void ModeWalls::Reset () {
  probed_ = false;
  shadow_count_ = 0;
  classname_.clear ();
  scan_timer_.invalidate ();

  count_ = 0;
}

void ModeWalls::OnRoundStart () {
  Rescan (true);
}

void ModeWalls::Frame () {
  Rescan (false);
}

bool ModeWalls::HasWalls () const {
  return count_ > 0;
}

bool ModeWalls::IsNodeBlocked (const ystl::Vector &point) const {
  for (int i = 0; i < count_; ++i) {
    const auto &box = boxes_[i];

    if (ystl::Vector::bbox_contains_point (point, box.mins, box.maxs)) {
      return true;
    }
  }
  return false;
}

bool ModeWalls::IsSegmentBlocked (const ystl::Vector &a, const ystl::Vector &b) const {
  for (int i = 0; i < count_; ++i) {
    if (SegmentHitsBox (a, b, boxes_[i].mins, boxes_[i].maxs)) {
      return true;
    }
  }
  return false;
}

void ModeWalls::Rescan (bool force) {
  if (!force && scan_timer_.less_than (1.0f)) {
    return;
  }
  scan_timer_.reset ();

  const ystl::StringRef classname = WallClassname ();

  if (!probed_ && !game.HasEntityInGame (classname)) {
    return;
  }
  probed_ = true;

  Box boxes[kMaxWalls] {};
  int count = 0;

  game.SearchEntities ("classname", classname, [&] (edict_t *ent) {
    // open wall is SOLID_NOT, closed is SOLID_BBOX
    if (count >= kMaxWalls || ent->v.solid < SOLID_BBOX) {
      return EntitySearchResult::Continue;
    }
    // inflate for bot hull
    boxes[count].mins = ent->v.absmin - ystl::Vector (18.0f, 18.0f, 4.0f);
    boxes[count].maxs = ent->v.absmax + ystl::Vector (18.0f, 18.0f, 4.0f);
    ++count;

    return EntitySearchResult::Continue;
  });

  if (count == shadow_count_) {
    bool same = true;

    for (int i = 0; i < count; ++i) {
      if (boxes[i].mins != shadow_[i].mins || boxes[i].maxs != shadow_[i].maxs) {
        same = false;
        break;
      }
    }
    if (same) {
      return;
    }
  }
  Publish (boxes, count);
}

void ModeWalls::Publish (const Box *boxes, int count) {
  for (int i = 0; i < count; ++i) {
    boxes_[i] = boxes[i];
    shadow_[i] = boxes[i];
  }
  shadow_count_ = count;
  count_ = count;

  // wall set changed, drop current paths so bots rebuild around it
  for (auto &bot : bots) {
    if (game.IsAliveEntity (bot.Ent ())) {
      bot.ClearSearchNodes ();
    }
  }
}

ystl::StringRef ModeWalls::WallClassname () {
  if (classname_.empty ()) {
    classname_ = conf.FetchCustom ("ModeWallClassname");

    if (classname_.empty ()) {
      classname_ = "test_effect";
    }
  }
  return classname_;
}

bool ModeWalls::SegmentHitsBox (const ystl::Vector &a, const ystl::Vector &b, const ystl::Vector &mins, const ystl::Vector &maxs) {
  float tmin = 0.0f, tmax = 1.0f;
  const ystl::Vector d = b - a;

  const auto axis = [&] (float origin, float dir, float mn, float mx) {
    if (ystl::fzero (dir)) {
      return origin >= mn && origin <= mx;
    }
    float t1 = (mn - origin) / dir;
    float t2 = (mx - origin) / dir;

    if (t1 > t2) {
      ystl::swap (t1, t2);
    }
    tmin = tmin > t1 ? tmin : t1;
    tmax = tmax < t2 ? tmax : t2;

    return tmin <= tmax;
  };

  return axis (a.x, d.x, mins.x, maxs.x) && axis (a.y, d.y, mins.y, maxs.y) && axis (a.z, d.z, mins.z, maxs.z);
}

} // namespace bot
