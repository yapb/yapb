//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void ConnectionCleaner::Collect () {
  for (int i = 0; i < kMaxNodeLinks; ++i) {
    const auto &link = path_.links[i];

    if (!graph_.Exists (link.index) || link.index == node_index_) {
      continue;
    }
    links_[count_++] = { i, link.index, static_cast<float> (link.distance), 0.0f };
  }
}

void ConnectionCleaner::SortByDistance () {
  ystl::bubble_sort (links_.data (), static_cast<size_t> (count_), [] (const Link &a, const Link &b) {
    return a.distance < b.distance;
  });
}

void ConnectionCleaner::ComputeBearings () {
  const float reference = (graph_.paths_[links_[0].target].origin - path_.origin).angles ().y;

  for (int i = 0; i < count_; ++i) {
    auto &cur = links_[i];

    cur.bearing = (graph_.paths_[cur.target].origin - path_.origin).angles ().y - reference;

    if (cur.bearing < 0.0f) {
      cur.bearing += 360.0f;
    }
  }

  ystl::bubble_sort (links_.data (), static_cast<size_t> (count_), [] (const Link &a, const Link &b) {
    return a.bearing < b.bearing;
  });
}

bool ConnectionCleaner::IsProtected (const Link &link) const {
  return (has_flag (path_.flags, NodeFlag::Ladder) && has_flag (graph_.paths_[link.target].flags, NodeFlag::Ladder)) ||
         has_flag (path_.links[link.slot].flags, PathFlag::Jump);
}

void ConnectionCleaner::RemoveAt (int pos) {
  for (int i = pos; i < count_ - 1; ++i) {
    links_[i] = links_[i + 1];
  }
  --count_;
}

bool ConnectionCleaner::HasAlternateRoute (int from, int to, int skip_slot) const {
  const auto &from_links = graph_.paths_[from].links;

  for (int s = 0; s < kMaxNodeLinks; ++s) {
    const int mid = from_links[s].index;

    if (s == skip_slot || mid == kInvalidNodeIndex || mid == from) {
      continue;
    }

    // parallel link is a one-hop alternate route by itself
    if (mid == to || graph_.IsConnected (mid, to)) {
      return true;
    }
  }
  return false;
}

bool ConnectionCleaner::RemoveConnection (const char *own_code, const char *recip_code, int pos) {
  const Link &link = links_[pos];

  // veto bridge removal, target must stay reachable without this link
  if (!HasAlternateRoute (node_index_, link.target, link.slot)) {
    return false;
  }

  // disconnect bots once before mutating the graph
  bots.DisconnectAll ();

  graph_.Msg (own_code, node_index_, link.target);
  graph_.UnassignPath (node_index_, link.slot);
  ++removed_;

  for (int j = 0; j < kMaxNodeLinks; ++j) {
    const auto &back = graph_.paths_[link.target].links[j];

    if (back.index == node_index_ && !has_flag (back.flags, PathFlag::Jump)) {
      // keep back-links that are bridges the other way
      if (!HasAlternateRoute (link.target, node_index_, j)) {
        continue;
      }
      graph_.Msg (recip_code, link.target, node_index_);
      graph_.UnassignPath (link.target, j);
      ++removed_;
    }
  }
  RemoveAt (pos);
  return true;
}

bool ConnectionCleaner::InspectTriplet (int pos) {
  const auto &cur = links_[pos], &prev = links_[pos - 1], &prev2 = links_[pos - 2];

  if (cur.bearing - prev2.bearing >= 80.0f || IsProtected (prev)) {
    return false;
  }

  if ((cur.distance + prev2.distance) * 1.1f / 2.0f < prev.distance) {
    return RemoveConnection (
      "Removing a useless (P.0.1) connection from index = %d to %d.", "Removing a useless (P.0.2) connection from index = %d to %d.", pos - 1);
  }
  return false;
}

bool ConnectionCleaner::InspectPair (int pos) {
  const auto &cur = links_[pos], &prev = links_[pos - 1];

  if (cur.bearing - prev.bearing >= 40.0f) {
    isolated_slot_ = cur.slot; // remember for the seam pair pass
    return false;
  }

  if (prev.distance < cur.distance * 1.1f) {
    if (!IsProtected (cur)) {
      return RemoveConnection (
        "Removing a useless (P.2.1) connection from index = %d to %d.", "Removing a useless (P.2.2) connection from index = %d to %d.", pos);
    }
    return false;
  }

  if (cur.distance < prev.distance * 1.1f && !IsProtected (prev)) {
    return RemoveConnection (
      "Removing a useless (P.2.3) connection from index = %d to %d.", "Removing a useless (P.2.4) connection from index = %d to %d.", pos - 1);
  }
  return false;
}

void ConnectionCleaner::InspectSeamPair () {
  int pos = -1;

  for (int i = 0; i < count_; ++i) {
    if (links_[i].slot == isolated_slot_) {
      pos = i;
      break;
    }
  }

  if (pos < 0) {
    return; // the remembered link was already removed by the pair pass
  }
  const auto &isolated = links_[pos];

  if (isolated.bearing - links_[0].bearing < 40.0f || 360.0f - isolated.bearing - links_[0].bearing < 40.0f) {
    if (isolated.distance * 1.1f < links_[0].distance) {
      if (!IsProtected (links_[0])) {
        RemoveConnection (
          "Removing a useless (P.3.1) connection from index = %d to %d.", "Removing a useless (P.3.2) connection from index = %d to %d.", 0);
      }
    }
    else if (links_[0].distance * 1.1f < isolated.distance && !IsProtected (isolated)) {
      RemoveConnection (
        "Removing a useless (P.3.3) connection from index = %d to %d.", "Removing a useless (P.3.4) connection from index = %d to %d.", pos);
    }
  }
}

void ConnectionCleaner::Run () {
  for (int i = 2; i < count_;) {
    if (!InspectTriplet (i)) {
      ++i;
    }
  }

  for (int i = 1; i < count_;) {
    if (!InspectPair (i)) {
      ++i;
    }
  }
  InspectSeamPair ();
}

void GraphAnalyze::Start () {
  if (is_analyzing_) {
    return;
  }

  // start analyzer in few seconds after level initialized
  if (cv_graph_analyze_auto_start) {
    update_timer_.start (3.0f);
    quiet_timer_.invalidate ();
    basics_created_ = false;

    // fresh staged finish state
    finish_stage_ = FinishStage::Idle;
    opt_pass_ = 0;
    camp_cursor_ = 0;
    camps_marked_ = 0;
    camp_forward_.clear ();
    camp_backward_.clear ();

    // set as we're analyzing
    is_analyzing_ = true;

    // silence all graph messages
    graph.SetMessageSilence (true);

    // set all nodes as not expanded
    ystl::fill (expanded_nodes_, false);

    // set all nodes as not optimized
    ystl::fill (optimized_nodes_, false);
    ctrl.Msg ("Starting map analysis.");
  }
  else {
    update_timer_.invalidate ();
  }
}

bool GraphAnalyze::SliceLeft () const {
  return slice_.elapsed_ms () < static_cast<double> (cv_graph_analyze_slice_ms.As<float> ());
}

int GraphAnalyze::FinishFlags () {
  ystl::String text { cv_graph_analyze_on_finish.As<ystl::StringRef> () };
  text.trim ();

  // plain number keeps working as before
  if (!text.empty () && text.find_first_of ("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ") == ystl::String::InvalidIndex) {
    return text.as<int> () & ystl::to_underlying (AnalyzeFinish::All);
  }
  int flags = 0;
  ystl::String token {};

  // comma/space/plus/pipe separated names
  for (size_t i = 0; i <= text.size (); ++i) {
    const char ch = i < text.size () ? text[i] : ',';

    if (ch != ',' && ch != ' ' && ch != '+' && ch != '|' && ch != '\t') {
      token += ch;
      continue;
    }
    token.trim ();

    if (!token.empty ()) {
      token.lowercase ();

      if (token == "optimize") {
        flags |= ystl::to_underlying (AnalyzeFinish::OptimizeNodes);
      }
      else if (token == "clean") {
        flags |= ystl::to_underlying (AnalyzeFinish::CleanPaths);
      }
      else if (token == "goals") {
        flags |= ystl::to_underlying (AnalyzeFinish::MarkGoals);
      }
      else if (token == "camps") {
        flags |= ystl::to_underlying (AnalyzeFinish::MarkCamps);
      }
      else if (token == "teams") {
        flags |= ystl::to_underlying (AnalyzeFinish::MarkTeams);
      }
      else if (token == "all") {
        flags = ystl::to_underlying (AnalyzeFinish::All);
      }
      else if (token == "none") {
        flags = 0;
      }
      else if (token[0] >= '0' && token[0] <= '9') {
        flags |= token.as<int> () & ystl::to_underlying (AnalyzeFinish::All);
      }
      else {
        static bool warned = false; // typo floods every frame otherwise

        if (!warned) {
          warned = true;
          ctrl.Msg ("Unknown analyze finish flag '%s'.", token.chars ());
        }
      }
    }
    token.clear ();
  }
  return flags & ystl::to_underlying (AnalyzeFinish::All);
}

void GraphAnalyze::Update () {
  if (!update_timer_.started () || !is_analyzing_) {
    return;
  }

  if (!update_timer_.elapsed ()) {
    return; // initial delay only
  }

  // circuit breaker, never pile work onto a lagging server
  constexpr float kLagCutoffSeconds = 0.1f;

  if (globals->frametime > kLagCutoffSeconds) {
    return;
  }

  // staged finish runs on the same budget
  if (finish_stage_ != FinishStage::Idle) {
    RunFinishSlice ();
    return;
  }
  slice_.start ();
  DisplayOverlayMessage ();

  // add basic nodes
  if (!basics_created_) {
    graph.SeedBasicNodes ();
    basics_created_ = true;
  }
  const int length_at_start = graph.Length ();
  bool expanded = false;

  for (int i = 0; i < graph.Length () && i < kMaxNodes; ++i) {
    if (!SliceLeft ()) {
      return; // out of budget, resume next frame
    }

    if (!graph.Exists (i) || expanded_nodes_[i]) {
      continue;
    }
    expanded_nodes_[i] = true;
    expanded = true;

    auto pos = graph[i].origin;
    const auto range = cv_graph_analyze_distance.As<float> ();

    const ystl::Vector directions[] = {
      { range,  0.0f,   0.0f   },
      { -range, 0.0f,   0.0f   },
      { 0.0f,   range,  0.0f   },
      { 0.0f,   -range, 0.0f   },
      { range,  range,  0.0f   },
      { -range, range,  0.0f   },
      { range,  -range, 0.0f   },
      { -range, -range, 0.0f   },
      { range,  0.0f,   128.0f },
      { -range, 0.0f,   128.0f },
      { 0.0f,   range,  128.0f },
      { 0.0f,   -range, 128.0f },
      { range,  range,  128.0f },
      { -range, range,  128.0f },
      { range,  -range, 128.0f },
      { -range, -range, 128.0f },
    };

    for (const auto &direction : directions) {
      Flood (pos, pos + direction, range);
    }
  }

  // full scan without progress means the flood settled
  if (expanded || graph.Length () != length_at_start) {
    quiet_timer_.invalidate ();
  }
  else {
    if (!quiet_timer_.started ()) {
      quiet_timer_.start (2.0f);
    }

    if (quiet_timer_.elapsed ()) {
      BeginFinish ();
    }
  }
}

void GraphAnalyze::Suspend () {
  update_timer_.start (kInfiniteDistance);
  is_analyzing_ = false;
  is_analyzed_ = false;
  basics_created_ = false;

  // drop staged finish state
  finish_stage_ = FinishStage::Idle;
  opt_pass_ = 0;
  camp_cursor_ = 0;
  camps_marked_ = 0;
  quiet_timer_.invalidate ();
  camp_forward_.clear ();
  camp_backward_.clear ();
}

void GraphAnalyze::BeginFinish () {
  // optimizer disabled means raw flood output, skip passes and cleanup
  if (!has_flag (FinishFlags (), AnalyzeFinish::OptimizeNodes)) {
    MarkGoals ();
    EnterCamps ();
    return;
  }
  ystl::fill (optimized_nodes_, false);
  LogGraphStats ("pre-passes");

  opt_pass_ = 0;
  finish_stage_ = FinishStage::Optimize;
}

void GraphAnalyze::EnterCamps () {
  camp_cursor_ = 0;
  camps_marked_ = 0;

  // frozen health for the sweep, flags only accumulate below
  if (has_flag (FinishFlags (), AnalyzeFinish::MarkCamps)) {
    graph.ComputeReachability (camp_forward_, camp_backward_);
  }
  finish_stage_ = FinishStage::Camps;
}

void GraphAnalyze::RunFinishSlice () {
  slice_.start ();
  DisplayOverlayMessage ();

  switch (finish_stage_) {
  case FinishStage::Optimize:
    while (SliceLeft ()) {
      if (!OptimizeSlice ()) {
        // passes settled, atomic tail of the optimize phase
        LogGraphStats ("post-passes");
        Cleanup ();

        LogGraphStats ("post-cleanup");
        MarkGoals ();

        finish_stage_ = FinishStage::Clear;
        break;
      }
    }
    break;

  case FinishStage::Clear: {
    // clear phase only rewires links, full restore is exact
    ystl::Array<Path> links_backup {};
    const bool guarded = links_backup.extend (graph.paths_);

    // strict regression check, pre-clear state may already be broken
    auto reached_count = [] () {
      ystl::Array<bool> forward {};
      graph.ComputeForwardReachability (forward);

      int reached = 0;
      const auto n = graph.Length ();

      for (int i = 0; i < n; ++i) {
        if (forward[i]) {
          ++reached;
        }
      }
      return reached;
    };
    const int reached_before = guarded ? reached_count () : 0;

    if (has_flag (FinishFlags (), AnalyzeFinish::CleanPaths)) {
      for (auto i = 0; i < graph.Length (); ++i) {
        graph.ClearConnections (i);
      }
    }

    // revert mass disconnection, dense graph beats shattered one
    if (guarded && reached_count () < reached_before) {
      graph.paths_.clear ();
      graph.paths_.extend (links_backup);

      ctrl.Debug ("[analyze] clear phase broke connectivity, reverted.");
    }
    LogGraphStats ("post-clear");
    EnterCamps ();
    break;
  }

  case FinishStage::Camps:
    if (MarkCampsSlice ()) {
      MarkTeamSides ();
      finish_stage_ = FinishStage::Finalize;
    }
    break;

  case FinishStage::Finalize: {
    is_analyzed_ = true;
    is_analyzing_ = false;
    update_timer_.invalidate ();

    // un-silence all graph messages
    graph.SetMessageSilence (false);

    ctrl.Msg ("Completed map analysis. Total nodes: %d.", graph.Length ());

    // auto save bots graph
    if (cv_graph_analyze_auto_save) {
      // never persist a graph that fails sanity, repair first
      if (!RepairConnectivity ()) {
        LogSanityBreakdown ();
        ctrl.Msg ("Graph failed sanity check after repair. Results NOT saved.");

        analyzer.Suspend (); // drop analysis state, stale flags must not leak

        // restore last good graph from disk, probe first so a miss never re-arms analysis
        bool restored = false;

        {
          ystl::MemFile probe (bstor.BuildPath (StorageFile::Graph, true));

          if (probe) {
            probe.close ();
            restored = graph.LoadGraphData ();
          }
        }

        if (restored) {
          cv_quota.Revert (); // bots play the restored graph
        }
        // else honest empty state, quota stays dead until a graph appears
      }
      else {
        if (!graph.SaveGraphData ()) {
          ctrl.Msg ("Can't save analyzed graph. Internal error.");
        }
        else if (!graph.LoadGraphData ()) {
          ctrl.Msg ("Can't load analyzed graph. Internal error.");
        }
        else {
          vistab.StartRebuild ();
          ctrl.EnableDrawModels (false);

          cv_quota.Revert ();
        }
      }
    }
    finish_stage_ = FinishStage::Idle;
    break;
  }

  case FinishStage::Idle:
    break;
  }
}

// single optimizer iteration, passes rescan cleanly after mutations
bool GraphAnalyze::OptimizeSlice () {
  if (graph.Length () == 0 || opt_pass_ >= 10) {
    return false;
  }
  ++opt_pass_;

  if (PassRemoveCollinear ()) {
    return true;
  }

  if (PassRemoveDuplicates ()) {
    return true;
  }
  return PassRemoveUnconnected ();
}

void GraphAnalyze::LogGraphStats (const char *stage) {
  if (!ctrl.IsDebug ()) {
    return;
  }
  const auto count = graph.Length ();

  int links = 0;
  int no_outgoing = 0;
  int bypassed = 0;

  for (int i = 0; i < count && i < kMaxNodes; ++i) {
    if (optimized_nodes_[i]) {
      ++bypassed;
    }

    bool outgoing = false;

    for (const auto &link : graph.paths_[i].links) {
      if (link.index != kInvalidNodeIndex && link.index != i && graph.Exists (link.index)) {
        ++links;
        outgoing = true;
      }
    }

    if (!outgoing) {
      ++no_outgoing;
    }
  }
  int reached = 0;

  if (count > 0) {
    ystl::Array<bool> forward {};
    graph.ComputeForwardReachability (forward);

    for (int i = 0; i < count && i < static_cast<int> (forward.size ()); ++i) {
      if (forward[i]) {
        ++reached;
      }
    }
  }
  ctrl.Debug ("[analyze] %s: nodes=%d links=%d noOutgoing=%d bypassed=%d reachedFrom0=%d", stage, count, links, no_outgoing, bypassed, reached);
}

float GraphAnalyze::NodeImportance (int index) const {
  if (!graph.Exists (index)) {
    return 0.0f;
  }
  const auto &path = graph[index];
  float importance = 1.0f;

  int connections = 0;

  for (const auto &link : path.links) {
    if (link.index != kInvalidNodeIndex && graph.Exists (link.index) && link.index != index) {
      ++connections;
    }
  }
  importance += static_cast<float> (connections) * 2.0f;

  if (has_flag (path.flags, NodeFlag::Goal)) {
    importance += 1000.0f;
  }
  if (has_flag (path.flags, NodeFlag::Ladder)) {
    importance += 500.0f;
  }
  if (has_flag (path.flags, NodeFlag::Rescue)) {
    importance += 500.0f;
  }
  if (has_flag (path.flags, NodeFlag::Camp)) {
    importance += 300.0f;
  }
  if (has_flag (path.flags, NodeFlag::Button)) {
    importance += 250.0f;
  }

  for (int i = 0; i < graph.Length (); ++i) {
    if (i == index) {
      continue;
    }
    for (const auto &link : graph[i].links) {
      if (link.index == index) {
        importance += 1.5f;
        break;
      }
    }
  }
  return importance;
}

bool GraphAnalyze::IsCollinear (int from_idx, int mid_idx, int to_idx, float tolerance) const {
  if (!graph.Exists (from_idx) || !graph.Exists (mid_idx) || !graph.Exists (to_idx)) {
    return false;
  }

  const ystl::Vector &from = graph[from_idx].origin;
  const ystl::Vector line = graph[to_idx].origin - from;

  const float line_len_sq = line.length_sq ();

  if (line_len_sq < 1.0f) {
    return false;
  }
  const ystl::Vector &mid = graph[mid_idx].origin;
  const ystl::Vector to_mid = mid - from;

  const float t = to_mid.dot (line) / line_len_sq;

  if (t < 0.0f || t > 1.0f) {
    return false;
  }
  return (mid - (from + line * t)).length () < tolerance;
}

void GraphAnalyze::BypassNode (int index) {
  if (!graph.Exists (index)) {
    return;
  }

  // remember where the node we're about to drop can go
  ystl::Array<int> outgoing {};

  for (const auto &link : graph[index].links) {
    if (graph.Exists (link.index) && link.index != index && !optimized_nodes_[link.index]) {
      outgoing.emplace (link.index);
    }
  }

  // rewire every node that points here straight to those destinations
  for (int i = 0; i < graph.Length (); ++i) {
    if (i == index) {
      continue;
    }
    bool incoming = false;

    for (auto &link : graph[i].links) {
      if (link.index == index) {
        incoming = true;
        link.index = kInvalidNodeIndex;
        link.distance = 0;
        link.flags = 0;
        link.velocity.clear ();
        break;
      }
    }

    if (!incoming) {
      continue;
    }

    for (const auto &dest : outgoing) {
      if (dest == i) {
        continue;
      }
      LinkNodes (i, dest);
    }
  }

  // neutralize the node itself, so cleanup () removes it
  graph[index].flags = 0;

  for (auto &link : graph[index].links) {
    link.index = kInvalidNodeIndex;
    link.distance = 0;
    link.flags = 0;
    link.velocity.clear ();
  }
}

bool GraphAnalyze::PassRemoveCollinear () {
  constexpr float kCollinearTolerance = 32.0f;
  constexpr auto kProtected = NodeFlag::Goal | NodeFlag::Ladder | NodeFlag::Camp | NodeFlag::Rescue | NodeFlag::Button;

  for (int i = 0; i < graph.Length (); ++i) {
    if (i == 0 || optimized_nodes_[i] || has_flag (graph[i].flags, kProtected)) {
      continue; // node 0 is the root of all traversals, never bypass it
    }

    for (int from = 0; from < graph.Length (); ++from) {
      if (from == i || optimized_nodes_[from]) {
        continue;
      }
      bool has_from = false;

      for (const auto &link : graph[from].links) {
        if (link.index == i) {
          has_from = true;
          break;
        }
      }

      if (!has_from) {
        continue;
      }
      bool collinear = false;

      for (const auto &link : graph[i].links) {
        const int to = link.index;

        if (!graph.Exists (to) || to == from || to == i || optimized_nodes_[to]) {
          continue;
        }

        // never bypass jump topology, and only bypass walkable shortcuts
        if (LinkHasJump (from, i) || LinkHasJump (i, to)) {
          continue;
        }

        if (IsCollinear (from, i, to, kCollinearTolerance) && graph.IsNodeReacheable (graph[from].origin, graph[to].origin)) {
          collinear = true;
          break;
        }
      }

      if (collinear) {
        BypassNode (i);
        optimized_nodes_[i] = true;
        return true;
      }
    }
  }
  return false;
}

bool GraphAnalyze::PassRemoveDuplicates () {
  constexpr float kDuplicateDistance = 128.0f * 0.3f;
  constexpr auto kProtected = NodeFlag::Goal | NodeFlag::Ladder | NodeFlag::Camp | NodeFlag::Rescue | NodeFlag::Button;

  for (int i = 0; i < graph.Length (); ++i) {
    if (i == 0 || optimized_nodes_[i] || has_flag (graph[i].flags, kProtected) || HasJumpLink (i)) {
      continue; // node 0 is the root of all traversals, never bypass it
    }

    for (int j = i + 1; j < graph.Length (); ++j) {
      if (j == 0 || optimized_nodes_[j] || has_flag (graph[j].flags, kProtected) || HasJumpLink (j) ||
          graph[i].origin.distance (graph[j].origin) >= kDuplicateDistance) {
        continue;
      }
      const float importance_i = NodeImportance (i);
      const float importance_j = NodeImportance (j);

      const int to_remove = importance_i < importance_j ? i : j;
      const int to_keep = importance_i < importance_j ? j : i;

      // inherit the outgoing links of the node we're removing
      for (const auto &link : graph[to_remove].links) {
        if (graph.Exists (link.index) && link.index != to_remove && link.index != to_keep && !optimized_nodes_[link.index]) {
          LinkNodes (to_keep, link.index);
        }
      }

      BypassNode (to_remove);
      optimized_nodes_[to_remove] = true;
      return true;
    }
  }
  return false;
}

bool GraphAnalyze::PassRemoveUnconnected () {
  constexpr auto kProtected = NodeFlag::Goal | NodeFlag::Rescue;

  for (int i = 0; i < graph.Length (); ++i) {
    if (i == 0 || optimized_nodes_[i] || has_flag (graph[i].flags, kProtected)) {
      continue; // node 0 is the root of all traversals, never erase it silently
    }

    int outgoing = 0;

    for (const auto &link : graph[i].links) {
      if (graph.Exists (link.index) && link.index != i) {
        ++outgoing;
      }
    }

    if (outgoing > 0) {
      continue;
    }
    bool incoming = false;

    for (int j = 0; j < graph.Length () && !incoming; ++j) {
      if (j == i || optimized_nodes_[j]) {
        continue;
      }
      for (const auto &link : graph[j].links) {
        if (link.index == i) {
          incoming = true;
          break;
        }
      }
    }

    if (!incoming) {
      BypassNode (i);
      optimized_nodes_[i] = true;
      return true;
    }
  }
  return false;
}

bool GraphAnalyze::HasFreeLinkSlot (int index) const {
  if (!graph.Exists (index)) {
    return false;
  }

  for (const auto &link : graph[index].links) {
    if (link.index == kInvalidNodeIndex) {
      return true;
    }
  }
  return false;
}

bool GraphAnalyze::HasJumpLink (int index) const {
  if (!graph.Exists (index)) {
    return false;
  }

  for (const auto &link : graph[index].links) {
    if (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump)) {
      return true;
    }
  }

  for (int i = 0; i < graph.Length (); ++i) {
    if (i == index) {
      continue;
    }

    for (const auto &link : graph[i].links) {
      if (link.index == index && has_flag (link.flags, PathFlag::Jump)) {
        return true;
      }
    }
  }
  return false;
}

bool GraphAnalyze::LinkHasJump (int from, int to) const {
  if (!graph.Exists (from) || !graph.Exists (to)) {
    return false;
  }

  for (const auto &link : graph[from].links) {
    if (link.index == to) {
      return has_flag (link.flags, PathFlag::Jump);
    }
  }
  return false;
}

bool GraphAnalyze::LinkNodes (int from, int to) {
  if (!graph.Exists (from) || !graph.Exists (to) || from == to) {
    return false;
  }

  // same semantics as cs-ebot AddPathConnection: only use a free slot,
  // never evict a good link to make room for a bypass one
  if (graph.IsConnected (from, to) || !HasFreeLinkSlot (from)) {
    return graph.IsConnected (from, to);
  }
  graph.AddPath (from, to, graph[from].origin.distance (graph[to].origin));
  return graph.IsConnected (from, to);
}

void GraphAnalyze::Cleanup () {
  // walk backwards, so erasing a node does not shift the ones below it
  for (int i = graph.Length () - 1; i >= 0; --i) {
    int connections = 0;
    bool deleted = false;

    for (const auto &link : graph[i].links) {
      if (link.index != kInvalidNodeIndex) {
        if (link.index < 0 || link.index >= graph.Length () || link.index == i) {
          graph.Erase (i);
          deleted = true;
          break;
        }
        ++connections;
      }
    }

    if (!deleted && !connections && !graph.IsConnected (i)) {
      graph.Erase (i);
    }
  }
}

void GraphAnalyze::LogSanityBreakdown () {
  constexpr int kMaxSamples = 8; // indices printed per class, counts are exact

  const auto n = graph.Length ();

  if (n < 1) {
    ctrl.Msg ("Sanity breakdown: graph is empty.");
    return;
  }
  ystl::Array<bool> forward {}, backward {};
  graph.ComputeReachability (forward, backward);

  int bad_number = 0, dangling = 0, self_link = 0, isolated = 0, camp_end = 0, fwd_bad = 0, bwd_bad = 0;
  ystl::Array<int> bad_number_at {}, dangling_at {}, self_link_at {}, isolated_at {}, camp_end_at {}, fwd_bad_at {}, bwd_bad_at {};

  // single sampler, caps stored indices but keeps exact counts
  auto sample = [] (ystl::Array<int> &at, int &count, int index) {
    ++count;

    if (at.size () < static_cast<size_t> (kMaxSamples)) {
      at.push (index);
    }
  };

  for (int i = 0; i < n; ++i) {
    if (!graph.Exists (i)) {
      continue;
    }
    const auto &path = graph[i];

    if (path.number != i) {
      sample (bad_number_at, bad_number, i);
    }
    bool outgoing = false;

    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
        continue;
      }

      if (link.index == i) {
        sample (self_link_at, self_link, i);
      }
      else if (!graph.Exists (link.index)) {
        sample (dangling_at, dangling, i);
      }
      else {
        outgoing = true;
      }
    }

    if (!outgoing && !graph.IsConnected (i)) {
      sample (isolated_at, isolated, i);
    }

    if (has_flag (path.flags, NodeFlag::Camp) && path.end.empty ()) {
      sample (camp_end_at, camp_end, i);
    }
  }

  for (int i = 0; i < n; ++i) {
    if (!graph.Exists (i)) {
      continue;
    }

    if (!forward[i]) {
      sample (fwd_bad_at, fwd_bad, i);
    }

    if (!backward[i]) {
      sample (bwd_bad_at, bwd_bad, i);
    }
  }

  // one line per non-zero class, samples listed after the count
  auto report = [] (ystl::StringRef name, int count, const ystl::Array<int> &at) {
    if (count < 1) {
      return;
    }
    ystl::String samples {};

    for (size_t k = 0; k < at.size (); ++k) {
      samples.appendf ("%s%d", k > 0 ? ", " : "", at[k]);
    }

    if (samples.empty ()) {
      ctrl.Msg ("Sanity breakdown: %s: %d.", name.chars (), count);
    }
    else {
      ctrl.Msg ("Sanity breakdown: %s: %d (e.g. %s).", name.chars (), count, samples.chars ());
    }
  };

  report ("misnumbered", bad_number, bad_number_at);
  report ("dangling links", dangling, dangling_at);
  report ("self links", self_link, self_link_at);
  report ("isolated", isolated, isolated_at);
  report ("camp w/o end", camp_end, camp_end_at);
  report ("unreachable from 0", fwd_bad, fwd_bad_at);
  report ("cannot reach 0", bwd_bad, bwd_bad_at);
}

bool GraphAnalyze::RepairConnectivity () {
  constexpr float kRepairRadius = 250.0f; // same gate as reachability traces use by default
  constexpr int kMaxCandidates = 8; // nearest healthy nodes tried per island node

  constexpr int kMaxRounds = 3; // repaired nodes bridge further islands next round
  constexpr int kMaxTests = 2048; // hard cap on engine traces

  auto n = graph.Length ();

  if (n < kMaxNodeLinks || !graph.Exists (0)) {
    return false;
  }

  // silence per-link chatter, summary is logged once below
  struct SilenceGuard {
    SilenceGuard () {
      graph.SetMessageSilence (true);
    }
    ~SilenceGuard () {
      graph.SetMessageSilence (false);
    }
  };
  SilenceGuard silence {};

  ystl::Array<bool> forward {}, backward {};
  graph.ComputeReachability (forward, backward);

  // shattered graph is not a few islands, per-node bridging only burns traces
  int healthy = 0;

  for (int x = 0; x < n; ++x) {
    if (graph.Exists (x) && forward[x] && backward[x]) {
      ++healthy;
    }
  }
  const bool catastrophic = healthy * 2 < n;

  int tests = 0;
  int relinked = 0;
  int erased = 0;

  // linkNodes helper refuses eviction, repair must not churn good links
  auto try_link = [&] (int from, int to) -> bool {
    // no trace spent on doomed attempts
    if (graph.IsConnected (from, to) || !HasFreeLinkSlot (from)) {
      return false;
    }

    if (++tests > kMaxTests) {
      return false;
    }

    // walk bridge first, jump arc across gaps second
    if (graph.IsNodeReacheable (graph[from].origin, graph[to].origin) && LinkNodes (from, to)) {
      ++relinked;
      return true;
    }

    if (graph.IsNodeReacheableWithJump (graph[from].origin, graph[to].origin)) {
      graph.AddPath (from, to, graph[from].origin.distance (graph[to].origin));
      graph.FlagJumpLink (from, to);
      ++relinked;
      return true;
    }
    return false;
  };

  for (int round = 0; round < kMaxRounds && !catastrophic; ++round) {
    graph.ComputeReachability (forward, backward);

    bool progress = false;

    for (int x = 0; x < n; ++x) {
      if (!graph.Exists (x)) {
        continue;
      }
      bool need_fwd = !forward[x], need_bwd = !backward[x];

      if (!need_fwd && !need_bwd) {
        continue;
      }
      const auto cands = graph.GetNearestInRadius (kRepairRadius, graph[x].origin, kMaxCandidates);

      for (const auto &cand : cands) {
        if (tests >= kMaxTests || cand == x || !graph.Exists (cand) || !forward[cand] || !backward[cand]) {
          continue;
        }

        // forward break needs a healthy bridge into x
        if (need_fwd && try_link (cand, x)) {
          need_fwd = false;
          progress = true;
        }

        // backward break needs a healthy bridge out of x
        if (need_bwd && try_link (x, cand)) {
          need_bwd = false;
          progress = true;
        }

        if (!need_fwd && !need_bwd) {
          break;
        }
      }

      if (tests >= kMaxTests) {
        break;
      }
    }

    if (!progress) {
      break;
    }
  }

  // erase fixpoint over forward-bad only, survivors keep their paths
  for (;;) {
    graph.ComputeReachability (forward, backward);

    ystl::Array<int> drop {};

    for (int x = 0; x < n; ++x) {
      if (!graph.Exists (x) || forward[x]) {
        continue;
      }
      drop.push (x); // backward-only nodes are legitimate one-way features
    }

    if (drop.empty ()) {
      break;
    }

    // walk backwards, so erasing a node does not shift the ones below it
    for (int i = drop.size<int> () - 1; i >= 0; --i) {
      graph.Erase (drop[i]);
    }
    erased += drop.size<int> ();
    n = graph.Length ();
  }
  LogGraphStats ("post-repair");

  if (relinked > 0 || erased > 0) {
    ctrl.Debug ("Graph repair: relinked %d paths, erased %d nodes.", relinked, erased);
  }

  // storage requires a minimum node count, do not even ask below it
  if (graph.Length () < kMaxNodeLinks) {
    return false;
  }

  // structural damage is never saveable, crashes lurk behind it
  const auto m = graph.Length ();

  for (int x = 0; x < m; ++x) {
    if (!graph.Exists (x)) {
      continue;
    }
    const auto &path = graph[x];

    if (path.number != x) {
      return false;
    }

    for (const auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
        continue;
      }

      if (link.index == x || !graph.Exists (link.index)) {
        return false;
      }
    }

    if (has_flag (path.flags, NodeFlag::Camp) && path.end.empty ()) {
      return false;
    }

    if (!forward[x]) {
      return false; // erase fixpoint guarantees none, safety net
    }
  }

  // backward one-way survivors are playable, planners handle directed graphs
  int one_way = 0;

  for (int x = 0; x < m; ++x) {
    if (graph.Exists (x) && !backward[x]) {
      ++one_way;
    }
  }

  if (one_way > 0) {
    ctrl.Msg ("Graph repair: %d one-way nodes kept, bots may not return from them.", one_way);
  }
  return true;
}

void GraphAnalyze::DisplayOverlayMessage () const {
  auto listenserver_edict = game.GetLocalEntity ();

  if (game.IsNullEntity (listenserver_edict) || !is_analyzing_) {
    return;
  }
  constexpr ystl::StringRef analyze_hud_messsage = "+-----------------------------------------------------------------+\n"
                                                   "         Map analysis for bots is in progress. Please Wait..       \n"
                                                   "+-----------------------------------------------------------------+\n";

  hudtextparms_t text_params {
    .x = -1.0f,
    .y = -1.0f,
    .effect = 1,
    .r1 = static_cast<uint8_t> (255),
    .g1 = static_cast<uint8_t> (31),
    .b1 = static_cast<uint8_t> (75),
    .a1 = static_cast<uint8_t> (0),
    .r2 = static_cast<uint8_t> (255),
    .g2 = static_cast<uint8_t> (31),
    .b2 = static_cast<uint8_t> (75),
    .a2 = static_cast<uint8_t> (0),
    .fadeinTime = 0.0078125f,
    .fadeoutTime = 0.0078125f,
    .holdTime = 1.0f,
    .fxTime = 0.25f,
    .channel = 1,
  };

  game.SendHudMessage (listenserver_edict, text_params, analyze_hud_messsage);
}

void GraphAnalyze::Flood (const ystl::Vector &pos, const ystl::Vector &next, float range) {
  range *= 0.75f;

  Trace::Result tr {};
  trace.Hull (pos, { next.x, next.y, next.z + 19.0f }, TraceIgnore::Monsters, head_hull, nullptr, &tr);

  // we're can't reach next point
  if (!ystl::fequal (tr.fraction, 1.0f) && !game.IsBreakableEntity (tr.hit)) {
    return;
  }

  // we're have something in around, skip
  if (graph.Exists (graph.GetForAnalyzer (tr.end_pos, range))) {
    return;
  }
  trace.Hull (tr.end_pos, { tr.end_pos.x, tr.end_pos.y, tr.end_pos.z - 999.0f }, TraceIgnore::Monsters, head_hull, nullptr, &tr);

  // ground is away for a break
  if (ystl::fequal (tr.fraction, 1.0f)) {
    return;
  }
  const ystl::Vector next_pos = { tr.end_pos.x, tr.end_pos.y, tr.end_pos.z + 19.0f };

  const int end_index = graph.GetForAnalyzer (next_pos, range);
  const int target_index = graph.GetNearestNoBuckets (next_pos, 250.0f);

  if (graph.Exists (end_index) || !graph.Exists (target_index)) {
    return;
  }
  const auto &target_pos = graph[target_index].origin;

  // re-check there's nothing nearby, and add something we're want
  if (!graph.Exists (graph.GetNearestNoBuckets (next_pos, range))) {
    is_crouch_ = false;
    trace.Line (next_pos, { next_pos.x, next_pos.y, next_pos.z + 36.0f }, TraceIgnore::Monsters, nullptr, &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      is_crouch_ = true;
    }
    auto test_pos = is_crouch_ ? ystl::Vector { next_pos.x, next_pos.y, next_pos.z - 18.0f } : next_pos;

    if ((graph.IsNodeReacheable (target_pos, test_pos) && graph.IsNodeReacheable (test_pos, target_pos)) ||
        (graph.IsNodeReacheableWithJump (test_pos, target_pos) && graph.IsNodeReacheableWithJump (target_pos, test_pos))) {

      graph.Add (NodeAddFlag::Normal, is_crouch_ ? ystl::Vector { next_pos.x, next_pos.y, next_pos.z - 9.0f } : next_pos);
    }
  }
}

void GraphAnalyze::MarkGoals () {
  if (!has_flag (FinishFlags (), AnalyzeFinish::MarkGoals)) {
    return;
  }

  // goals on unreachable nodes are useless and trip the repair guard
  ystl::Array<bool> forward {}, backward {};
  graph.ComputeReachability (forward, backward);

  auto update_node_flags = [&] (NodeFlag type, ystl::StringRef classname) {
    game.SearchEntities ("classname", classname, [&] (edict_t *ent) {
      for (int i = 0; i < graph.Length (); ++i) {
        auto &path = graph[i];

        // numbering itself is validated by checkNodes, never trust it blindly
        if (path.number != i || !forward[i] || !backward[i]) {
          continue;
        }
        const ystl::Vector bb = path.origin + ystl::Vector (1.0f, 1.0f, 1.0f);

        if (!ystl::Vector::bbox_contains_point2_d (bb, ent->v.absmin, ent->v.absmax)) {
          continue;
        }
        path.flags |= type;
      }
      return EntitySearchResult::Continue;
    });
  };

  if (game.MapIs (MapFlags::Demolition)) {
    update_node_flags (NodeFlag::Goal, "func_bomb_target"); // bombspot zone
    update_node_flags (NodeFlag::Goal, "info_bomb_target"); // bombspot zone (same as above)
  }
  else if (game.MapIs (MapFlags::HostageRescue)) {
    update_node_flags (NodeFlag::Rescue, "func_hostage_rescue"); // hostage rescue zone
    update_node_flags (NodeFlag::Rescue, "info_hostage_rescue"); // hostage rescue zone (same as above)
    update_node_flags (NodeFlag::Rescue, "info_player_start"); // then add ct spawnpoints

    update_node_flags (NodeFlag::Goal, "hostage_entity"); // hostage entities
    update_node_flags (NodeFlag::Goal, "monster_scientist"); // hostage entities (same as above)
  }
  else if (game.MapIs (MapFlags::Assassination)) {
    update_node_flags (NodeFlag::Goal, "func_vip_safetyzone"); // vip rescue (safety) zone
    update_node_flags (NodeFlag::Goal, "func_escapezone"); // terrorist escape zone
  }
}

bool GraphAnalyze::MarkCampsSlice () {
  if (!has_flag (FinishFlags (), AnalyzeFinish::MarkCamps)) {
    return true;
  }
  slice_.start ();
  constexpr float kCornerRange = 240.0f; // wall probe distance
  constexpr float kSightRange = 1024.0f; // sightline probe distance
  constexpr float kMinSight = 450.0f; // shortest acceptable sightline

  constexpr int kRays = 8; // horizontal wall probes
  constexpr float kCampSpacing = 400.0f; // min distance between camp nodes
  constexpr int kMaxCamps = 64; // global cap

  constexpr auto kProtected = NodeFlag::Goal | NodeFlag::Ladder | NodeFlag::Camp | NodeFlag::Rescue | NodeFlag::Button;

  // previously accepted spots carry over via their flags
  ystl::SmallArray<ystl::Vector> accepted {};
  const auto n = graph.Length ();

  for (int i = 0; i < n; ++i) {
    if (graph.Exists (i) && has_flag (graph[i].flags, NodeFlag::Camp)) {
      accepted.push (graph[i].origin);
    }
  }
  Trace::Result tr {};

  for (; camp_cursor_ < n && camps_marked_ < kMaxCamps && SliceLeft (); ++camp_cursor_) {
    const int i = camp_cursor_;

    if (!graph.Exists (i) || !camp_forward_[i] || !camp_backward_[i]) {
      continue;
    }
    auto &path = graph[i];

    if (has_flag (path.flags, kProtected) || has_flag (path.flags, NodeFlag::NoHostage) || HasJumpLink (i)) {
      continue;
    }
    const ystl::Vector eye = path.origin + ystl::Vector { 0.0f, 0.0f, 28.0f };
    bool blocked[kRays] {};

    for (int k = 0; k < kRays; ++k) {
      const ystl::Vector dir = ystl::Vector { 0.0f, static_cast<float> (k) * 45.0f, 0.0f }.forward ();

      trace.Line (eye, eye + dir * kCornerRange, TraceIgnore::Monsters, nullptr, &tr);
      blocked[k] = !ystl::fequal (tr.fraction, 1.0f);
    }

    // longest contiguous open arc, corners only
    int best_start = -1, best_len = 0;

    for (int s = 0; s < kRays; ++s) {
      if (blocked[s]) {
        continue;
      }
      int len = 0;

      while (len < kRays && !blocked[(s + len) % kRays]) {
        ++len;
      }

      if (len > best_len) {
        best_len = len;
        best_start = s;
      }
    }

    if (best_len < 1 || best_len > 6) {
      continue;
    }

    // sightline along the arc middle, closets need not apply
    const float mid_yaw = ystl::wrap_angle ((static_cast<float> (best_start) + static_cast<float> (best_len - 1) * 0.5f) * 45.0f);
    const ystl::Vector mid_dir = ystl::Vector { 0.0f, mid_yaw, 0.0f }.forward ();

    trace.Line (eye, eye + mid_dir * kSightRange, TraceIgnore::Monsters, nullptr, &tr);

    if (tr.fraction * kSightRange < kMinSight) {
      continue;
    }

    // keep spacing between camp nodes
    bool crowded = false;

    for (const auto &at : accepted) {
      if (path.origin.distance_sq (at) < ystl::sqrf (kCampSpacing)) {
        crowded = true;
        break;
      }
    }

    if (crowded) {
      continue;
    }

    // facing sweep covers the open arc edges
    const float yaw_a = ystl::wrap_angle ((static_cast<float> (best_start) - 0.5f) * 45.0f);
    const float yaw_b = ystl::wrap_angle ((static_cast<float> (best_start + best_len - 1) + 0.5f) * 45.0f);

    path.flags |= NodeFlag::Crossing;
    path.flags |= NodeFlag::Camp;

    path.start = ystl::Vector { 0.0f, yaw_a, 0.0f };
    path.end = ystl::Vector { 0.0f, yaw_b, 0.0f };
    path.radius = 0.0f;

    accepted.push (path.origin);
    ++camps_marked_;
  }
  const bool done = camp_cursor_ >= n || camps_marked_ >= kMaxCamps;

  if (done && camps_marked_ > 0) {
    ctrl.Msg ("Marked %d corner camp spots.", camps_marked_);
  }
  return done;
}

void GraphAnalyze::MarkTeamSides () {
  if (!has_flag (FinishFlags (), AnalyzeFinish::MarkTeams)) {
    return;
  }
  constexpr float kDefendRadius = 700.0f; // defenders cluster around the objective
  constexpr float kDefendSpacing = 250.0f; // min distance between defensive nodes
  constexpr int kMaxDefend = 48; // defensive cap
  constexpr float kAttackHomeRadius = 800.0f; // attackers start beyond own spawn
  constexpr float kAttackGoalRadius = 500.0f; // ...and away from the objective
  constexpr float kAttackSpacing = 500.0f; // min distance between offensive nodes
  constexpr int kMaxAttack = 32; // offensive cap, attackers need few sparse spots
  constexpr auto kSkip = NodeFlag::Goal | NodeFlag::Rescue | NodeFlag::Ladder | NodeFlag::Camp | NodeFlag::Button;

  // defenders guard the objective, attackers work the routes (podbot canon)
  Team defenders = Team::Invalid, attackers = Team::Invalid;

  if (game.MapIs (MapFlags::Demolition)) {
    defenders = Team::CT;
    attackers = Team::Terrorist;
  }
  else if (game.MapIs (MapFlags::HostageRescue)) {
    defenders = Team::Terrorist;
    attackers = Team::CT;
  }
  else if (game.MapIs (MapFlags::Assassination)) {
    defenders = Team::Terrorist;
    attackers = Team::CT;
  }
  else if (game.MapIs (MapFlags::Escape)) {
    defenders = Team::CT;
    attackers = Team::Terrorist;
  }
  else {
    return; // unknown type, no anchors to mark against
  }

  // objective origins anchor both roles
  ystl::SmallArray<ystl::Vector> goals {};

  for (int i = 0; i < graph.Length (); ++i) {
    if (graph.Exists (i) && has_flag (graph[i].flags, NodeFlag::Goal)) {
      goals.push (graph[i].origin);
    }
  }

  if (goals.empty ()) {
    return;
  }

  // own spawn origins fence the attackers off their home
  ystl::SmallArray<ystl::Vector> own_spawns {};
  const ystl::StringRef own_spawn_class = attackers == Team::Terrorist ? "info_player_deathmatch" : "info_player_start";

  game.SearchEntities ("classname", own_spawn_class, [&] (edict_t *ent) {
    own_spawns.push (game.GetEntityOrigin (ent));
    return EntitySearchResult::Continue;
  });

  // spread check against same-role picks
  auto crowded = [] (const ystl::SmallArray<ystl::Vector> &accepted, const ystl::Vector &origin, float spacing) {
    for (const auto &at : accepted) {
      if (origin.distance_sq (at) < ystl::sqrf (spacing)) {
        return true;
      }
    }
    return false;
  };
  auto flag_side = [] (Path &path, Team team) {
    path.flags |= NodeFlag::Crossing;

    if (team == Team::Terrorist) {
      path.flags |= NodeFlag::TerroristOnly;
    }
    else {
      path.flags |= NodeFlag::CTOnly;
    }
  };
  ystl::SmallArray<ystl::Vector> accepted_def {}, accepted_att {};
  int marked_t = 0, marked_c = 0;

  auto count_side = [&] (Team team) {
    if (team == Team::Terrorist) {
      ++marked_t;
    }
    else {
      ++marked_c;
    }
  };
  const auto n = graph.Length ();

  for (int i = 0; i < n; ++i) {
    if (!graph.Exists (i) || has_flag (graph[i].flags, kSkip)) {
      continue;
    }
    auto &path = graph[i];
    float goal_dist = kInfiniteDistance, own_dist = kInfiniteDistance;

    for (const auto &goal : goals) {
      goal_dist = ystl::min (goal_dist, path.origin.distance2d (goal));
    }

    for (const auto &spawn : own_spawns) {
      own_dist = ystl::min (own_dist, path.origin.distance2d (spawn));
    }

    if (goal_dist <= kDefendRadius && static_cast<int> (accepted_def.size ()) < kMaxDefend &&
        !crowded (accepted_def, path.origin, kDefendSpacing)) {
      flag_side (path, defenders);
      accepted_def.push (path.origin);
      count_side (defenders);
    }
    else if (!own_spawns.empty () && own_dist > kAttackHomeRadius && goal_dist > kAttackGoalRadius &&
             static_cast<int> (accepted_att.size ()) < kMaxAttack && !crowded (accepted_att, path.origin, kAttackSpacing)) {
      flag_side (path, attackers);
      accepted_att.push (path.origin);
      count_side (attackers);
    }
  }

  if (marked_t > 0 || marked_c > 0) {
    ctrl.Debug ("Marked %d T-side and %d CT-side nodes.", marked_t, marked_c);
  }
}

} // namespace bot
