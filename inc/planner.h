//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

namespace bot {

const float kInfiniteHeuristic = 65535.0f; // max out heuristic value

// a* route state
enum class RouteState : int32_t {
  Open = 0,
  Closed,
  New
};

// a * find path result
enum class AStarResult : int32_t {
  Success = 0,
  Failed,
  InternalError
};

// node added
using NodeAdderFn = const ystl::Lambda<bool (int)> &;

// a* heuristic strategy: pairs g and h so they can never be mismatched
class PlannerHeuristic {
public:
  virtual ~PlannerHeuristic () = default;

  // cost of moving from parent to current node, edgeDistance is the direct link distance
  virtual float G (Team team, int current_index, int parent_index, int edge_distance) const = 0;

  // estimated cost from a node to the goal
  virtual float H (int index, int goal_index) const = 0;
};

// route variety, random heuristics
class PlannerHeuristicDiversity final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// shortest path
class PlannerHeuristicFast final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// shortest path, hostage-aware
class PlannerHeuristicFastHostage final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// least dangerous path
class PlannerHeuristicOptimal final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// least dangerous path, hostage-aware
class PlannerHeuristicOptimalHostage final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// kill-weighted path
class PlannerHeuristicSafe final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// kill-weighted path, hostage-aware
class PlannerHeuristicSafeHostage final : public PlannerHeuristic {
public:
  float G (Team team, int current_index, int parent_index, int edge_distance) const override;
  float H (int index, int goal_index) const override;
};

// built-in heuristic strategy instances
namespace plannerHeuristics {
inline const PlannerHeuristicDiversity diversity {};
inline const PlannerHeuristicFast fast {};
inline const PlannerHeuristicFastHostage fast_hostage {};
inline const PlannerHeuristicOptimal optimal {};
inline const PlannerHeuristicOptimalHostage optimal_hostage {};
inline const PlannerHeuristicSafe safe {};
inline const PlannerHeuristicSafeHostage safe_hostage {};
} // namespace plannerHeuristics

// a* algorithm for bots
class AStarAlgo final : public ystl::NonCopyable {
public:
  struct Route {
    float g {}, f {};
    int parent { kInvalidNodeIndex };
    uint32_t epoch {}; // search stamp, zero means never touched
    RouteState state { RouteState::New };
  };

private:
  // open list entry
  struct OpenNode {
    int32_t index {};
    float f {};

    OpenNode () = default;
    OpenNode (const int32_t index, const float f) : index (index), f (f) {}

    bool operator< (const OpenNode &rhs) const {
      return f < rhs.f;
    }
  };

private:
  ystl::BinaryHeap<OpenNode> route_que_ {};
  ystl::Array<Route> routes_ {};

  const PlannerHeuristic *heuristic_ {}; // g/h strategy, set by the caller

  uint32_t epoch_ {}; // bumped on every find (), stale route cells are lazily reset
  int size_ {};

  ystl::Array<int> constructed_path_ {};
  ystl::Array<int> smoothed_path_ {};

private:
  // clears the currently built route
  void ClearRoute ();

  // get a route cell for the current search, lazily resetting stale ones
  Route *RouteAt (const int index) {
    auto route = &routes_[index];

    if (route->epoch != epoch_) {
      route->epoch = epoch_;
      route->g = route->f = 0.0f;
      route->parent = kInvalidNodeIndex;
      route->state = RouteState::New;
    }
    return route;
  }

  // do a post-smoothing after a* finished constructing path
  void PostSmooth (NodeAdderFn on_added_node);

public:
  explicit AStarAlgo (const int length) {
    Init (length);
  }

  AStarAlgo () = default;
  ~AStarAlgo () = default;

public:
  // do the pathfinding
  AStarResult Find (Team bot_team, int src_index, int dest_index, NodeAdderFn on_added_node);

public:
  // initialize astar with valid path length
  void Init (const int length) {
    size_ = length;
    ClearRoute ();

    route_que_.reserve (GetMaxLength ());
    constructed_path_.reserve (GetMaxLength ());
    smoothed_path_.reserve (GetMaxLength ());

    constructed_path_.shrink ();
    smoothed_path_.shrink ();
  }

  // set the g/h heuristic strategy
  void SetHeuristic (const PlannerHeuristic *heuristic) {
    heuristic_ = heuristic;
  }

  // get route max length, route length should not be larger than half of map nodes
  size_t GetMaxLength () const {
    return size_ / 2 + kMaxNodes / 256;
  }

public:
  // can the node can be skipped?
  static bool CantSkipNode (const int a, const int b, bool skip_vis_check = false);
};

// floyd-warshall shortest path algorithm
class FloydWarshallAlgo final {
private:
  int size_ {};
  ystl::Atomic<bool> rebuilding_ {}; // matrix is being rebuilt on worker thread, unusable for reads

public:
  // infinity value for floyd-warshall (max int16_t - 1 to prevent overflow on addition)
  static constexpr int16_t kInfinity = ystl::numeric_limits<int16_t>::max () - 1;

public:
  // floyd-warshall matrices
  struct Matrix {
    int16_t index { kInvalidNodeIndex };
    int16_t dist { kInfinity };

  public:
    Matrix () = default;
    ~Matrix () = default;

  public:
    Matrix (const int index, const int dist) : index (static_cast<int16_t> (index)), dist (static_cast<int16_t> (dist)) {}
  };

private:
  ystl::Array<Matrix, ReservePolicy::Proportional> matrix_ {};

public:
  FloydWarshallAlgo () = default;
  ~FloydWarshallAlgo () = default;

public:
  // is matrix being rebuilt on worker thread ?
  bool IsRebuilding () const {
    return rebuilding_.load (ystl::MemoryOrder::acquire);
  }

private:
  // create floyd matrics
  void SyncRebuild ();

  // async rebuild
  void Rebuild ();

public:
  // load matrices from disk
  bool Load ();

  // flush matrices to disk, so we will not rebuild them on load same map
  void Save () const;

  // do the pathfinding
  bool Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance = nullptr);

public:
  // flat matrix cell accessor
  Matrix &Cell (const int src_index, const int dest_index) {
    return *(matrix_.data () + (src_index * size_) + dest_index);
  }

  const Matrix &Cell (const int src_index, const int dest_index) const {
    return *(matrix_.data () + (src_index * size_) + dest_index);
  }

public:
  // distance between two nodes with pathfinder
  int Dist (int src_index, int dest_index) {
    // validate input indices to prevent out-of-bounds access
    if (src_index < 0 || src_index >= size_ || dest_index < 0 || dest_index >= size_) {
      return kInfinity;
    }
    return static_cast<int> (Cell (src_index, dest_index).dist);
  }

  // contiguous row for a source, valid until next rebuild
  const Matrix *Row (const int src_index) const {
    if (src_index < 0 || src_index >= size_) {
      return nullptr;
    }
    return matrix_.data () + (src_index * size_);
  }
};

YSTL_LE_FIELDS (FloydWarshallAlgo::Matrix, index, dist);

// non-owning view over a per-source distance table (dijkstra flat ints or floyd matrix row)
class DistanceTable final {
private:
  const FloydWarshallAlgo::Matrix *cells_ {};
  const int *ints_ {};
  int size_ {};

public:
  DistanceTable () = default;
  DistanceTable (const int *ints, const int length) : ints_ (ints), size_ (length) {}
  DistanceTable (const FloydWarshallAlgo::Matrix *cells, const int length) : cells_ (cells), size_ (length) {}

public:
  int At (const int index) const {
    return cells_ != nullptr ? static_cast<int> (cells_[index].dist) : ints_[index];
  }

  int Length () const {
    return size_;
  }

  explicit operator bool () const {
    return size_ > 0;
  }
};

// dijkstra shortest path algorithm
class DijkstraAlgo final {
private:
  using Route = ystl::Twin<int, int>;

private:
  ystl::Array<int> distance_ {};
  ystl::Array<int> parent_ {};
  ystl::Array<int> scratch_ {};
  ystl::Array<int> distance_all0_ {}; // distAll output slot 0, kept apart from find () scratch
  ystl::Array<int> distance_all1_ {}; // distAll output slot 1

  ystl::BinaryHeap<Route> queue_ {};
  int size_ {};

public:
  DijkstraAlgo () = default;
  ~DijkstraAlgo () = default;

public:
  // initialize dijkstra with valid path length
  void Init (const int length);

  // do the pathfinding
  bool Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance = nullptr);

  // distances from src to every node in a single pass (wall-aware, one search instead of one per node)
  bool DistAll (int src_index, DistanceTable &distances, int slot);

  // distance between two nodes with pathfinder
  int Dist (int src_index, int dest_index);
};

// the bot path planner
class PathPlanner : public ystl::Singleton<PathPlanner> {
private:
  ystl::UniquePtr<DijkstraAlgo> dijkstra_ {};
  ystl::UniquePtr<FloydWarshallAlgo> floyd_ {};
  bool memory_limit_hit_ {};
  bool paths_check_failed_ {};

public:
  PathPlanner ();
  ~PathPlanner () = default;

public:
  // initialize all planners
  void Init ();

  // has real path distance (instead  of distance2d) ?
  bool HasRealPathDistance () const;

public:
  // get the dijkstra algo
  decltype (auto) GetDijkstra () {
    return dijkstra_.get ();
  }

  // get the floyd algo
  decltype (auto) GetFloydWarshall () {
    return floyd_.get ();
  }

public:
  bool IsPathsCheckFailed () const {
    return paths_check_failed_;
  }

  bool IsMemoryLimitHit () const {
    return memory_limit_hit_;
  }

  void SetPathsCheckFailed (const bool value) {
    paths_check_failed_ = value;
  }

public:
  // do the pathfinding
  bool Find (int src_index, int dest_index, NodeAdderFn on_added_node, int *path_distance = nullptr);

  // distances from src to every node in a single pass (one dijkstra / one floyd row)
  bool DistAll (int src_index, DistanceTable &distances, int slot);

  // distance between two nodes with pathfinder
  float Dist (int src_index, int dest_index);

  // get the precise distanace regardless of cvar
  float PreciseDistance (int src_index, int dest_index);
};

YSTL_EXPOSE_GLOBAL_SINGLETON (PathPlanner, planner);

} // namespace bot
