//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// analyze finish steps bitmask
namespace bot {

enum class AnalyzeFinish : int32_t {
  OptimizeNodes = ystl::bit (0),
  CleanPaths = ystl::bit (1),
  MarkGoals = ystl::bit (2),
  MarkCamps = ystl::bit (3),
  MarkTeams = ystl::bit (4),
  All = 31
};
YSTL_ENABLE_ENUM_FLAGS (AnalyzeFinish);

// removes the "useless" connections of a single node, based on pod-bot mm from kwo
class ConnectionCleaner final : public ystl::NonCopyable {
public:
  struct Link {
    int slot {}; // index into path::links
    int target {}; // node this link points to
    float distance {};
    float bearing {}; // direction relative to the shortest link, in [0, 360) range
  };

private:
  Graph &graph_;
  Path &path_;
  const int node_index_ {};

  ystl::FixedArray<Link, kMaxNodeLinks> links_ {};

  int count_ {};
  int removed_ {};

  // slot of the last angularly-isolated link found by the pair pass, checked by the seam pair pass
  int isolated_slot_ { kInvalidNodeIndex };

public:
  ConnectionCleaner (Graph &owner, int index) : graph_ (owner), path_ (owner.paths_[index]), node_index_ (index) {}
  ~ConnectionCleaner () = default;

  friend struct AnalyzeHook;

  // gather the valid links of the node
  void Collect ();

  // sort paths from the closest node to the farest away one
  void SortByDistance ();

  // compute bearings from closest node, then sort by angle
  void ComputeBearings ();

  // run triplet, pair and seam passes
  void Run ();

  bool Empty () const {
    return count_ == 0;
  }

  int RemovedCount () const {
    return removed_;
  }

private:
  // leave alone ladder connections and don't remove jump connections
  bool IsProtected (const Link &link) const;

  void RemoveAt (int pos);

  // two-hop alternate route, removals keep reachability by induction
  bool HasAlternateRoute (int from, int to, int skip_slot) const;

  // remove link and reciprocal non-jump links pointing back
  bool RemoveConnection (const char *own_code, const char *recip_code, int pos);

  // drop middle link inside 80 degree cone when it detours
  bool InspectTriplet (int pos);

  // two consecutive links within a 40 degrees cone, the longer one is a detour
  bool InspectPair (int pos);

  // for the pair wrapping around the 0/360 seam
  void InspectSeamPair ();
};

// next code is based on cs-ebot implementation, devised by efedursun125
class GraphAnalyze : public ystl::Singleton<GraphAnalyze> {
public:
  // staged finish states, heavy steps are sliced across frames
  enum class FinishStage {
    Idle,
    Optimize,
    Clear,
    Camps,
    Finalize
  };

public:
  GraphAnalyze () = default;
  ~GraphAnalyze () = default;

private:
  ystl::CountdownTimer update_timer_ {}; // initial delay before analysis
  ystl::CountdownTimer quiet_timer_ {}; // settles the flood before finish
  ystl::Stopwatch slice_ {}; // real-time frame budget

  FinishStage finish_stage_ { FinishStage::Idle }; // staged finish position
  int opt_pass_ {}; // optimizer iterations done
  int camp_cursor_ {}; // camp sweep resume position
  int camps_marked_ {}; // camp spots placed this run
  ystl::Array<bool> camp_forward_ {}, camp_backward_ {}; // frozen health for camp sweep

  bool basics_created_ {}; // basics nodes were created?
  bool is_crouch_ {}; // is node to be created as crouch ?
  bool is_analyzing_ {}; // we're in analyzing ?
  bool is_analyzed_ {}; // current node is analyzed
  bool expanded_nodes_[kMaxNodes] {}; // all nodes expanded ?
  bool optimized_nodes_[kMaxNodes] {}; // all nodes optimized ?

public:
  friend struct AnalyzeHook;

  // start analysis process
  void Start ();

  // update analysis process
  void Update ();

  // suspend analysis
  void Suspend ();

private:
  // real-time budget remaining in this frame
  bool SliceLeft () const;

  // resolved finish bitmask, accepts numbers and names
  static int FinishFlags ();

  // flood with nodes
  void Flood (const ystl::Vector &pos, const ystl::Vector &next, float range);

  // mark nodes as goals
  void MarkGoals ();

  // one budgeted camp sweep chunk, true when complete
  bool MarkCampsSlice ();

  // mark team-side nodes by nearest spawn dominance
  void MarkTeamSides ();

  // enter staged finish
  void BeginFinish ();

  // frozen sweep state for the camp stage
  void EnterCamps ();

  // one budgeted finish chunk
  void RunFinishSlice ();

  // single optimizer iteration, false when passes settle
  bool OptimizeSlice ();

  // optimizer passes (cs-ebot WaypointOptimizer)
  void BypassNode (int index);
  bool PassRemoveCollinear ();
  bool PassRemoveDuplicates ();
  bool PassRemoveUnconnected ();

  // link helpers: never evict existing links, never break jump topology
  bool HasFreeLinkSlot (int index) const;
  bool HasJumpLink (int index) const;
  bool LinkHasJump (int from, int to) const;
  bool LinkNodes (int from, int to);

  // connection importance used by the optimizer
  float NodeImportance (int index) const;

  // checks if mid lies on the from->to segment within tolerance
  bool IsCollinear (int from, int mid, int to, float tolerance) const;

  // cleanup bad nodes
  void Cleanup ();

  // connectivity repair: relink islands, erase leftovers (true when graph passes sanity)
  bool RepairConnectivity ();

  // per-class sanity failure counts, mirrors checkNodes verdict
  void LogSanityBreakdown ();

  // stage statistics for analysis debugging (nodes/links/connectivity)
  void LogGraphStats (const char *stage);

  // show overlay message about analyzing
  void DisplayOverlayMessage () const;

public:
  // node should be created as crouch
  bool IsCrouch () const {
    return is_crouch_;
  }

  // is currently analyzing ?
  bool IsAnalyzing () const {
    return is_analyzing_;
  }

  // current graph is analyzed graph ?
  bool IsAnalyzed () const {
    return is_analyzed_;
  }

  // mark as optimized
  void MarkOptimized (const int index) {
    if (index >= 0 && index < kMaxNodes) {
      optimized_nodes_[index] = true;
    }
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (GraphAnalyze, analyzer);

} // namespace bot
