//
// YaPB test host: bench/planner.
//
// Microbenchmarks for the pathfinding core: lazy epoch reset vs full route
// sweep, the edge-distance fast path vs the parent link scan, end-to-end
// finds on a synthetic grid graph for A*, Dijkstra and Floyd-Warshall, and
// the one-off Floyd matrix build cost.
//
// Unlicense
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

constexpr int kGridSide = 40;
constexpr int kGridNodes = kGridSide * kGridSide;

// worst-case supported graph (kMaxNodes == 4096) for A*/Dijkstra scaling
constexpr int kLargeSide = 64;
constexpr int kLargeNodes = kLargeSide * kLargeSide;

// Floyd is O(n^3): keep it small enough to build in a few seconds
constexpr int kFloydSide = 32;
constexpr int kFloydNodes = kFloydSide * kFloydSide;

// nodeAt/buildGridGraph share the current side
int g_grid_side = kGridSide;

int NodeAt (int x, int y) {
  return y * g_grid_side + x;
}

void BuildGridGraph (int side) {
  g_grid_side = side;
  graph.Reset ();

  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      Path path {};
      path.origin = ystl::Vector (static_cast<float> (x) * 100.0f, static_cast<float> (y) * 100.0f, 0.0f);
      path.number = graph.Length ();
      path.light = kInvalidLightLevel;

      for (auto &link : path.links) {
        link.index = kInvalidNodeIndex;
      }
      graph.paths_.push (path);
    }
  }

  auto link = [] (int from, int to) {
    const auto &from_path = graph.paths_[static_cast<size_t> (from)];
    const auto &to_path = graph.paths_[static_cast<size_t> (to)];

    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = static_cast<int32_t> (from_path.origin.distance (to_path.origin));
        return;
      }
    }
  };

  // 8-connected grid: every node gets up to 8 links, the max the graph allows
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      const int from = NodeAt (x, y);

      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          const int nx = x + dx;
          const int ny = y + dy;

          if (nx >= 0 && nx < side && ny >= 0 && ny < side) {
            link (from, NodeAt (nx, ny));
          }
        }
      }
    }
  }
  graph.PopulateNodes ();
}

void Boot (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
}

uint32_t g_rng = 0x1234567u;

int NextRandom (int bound) {
  g_rng = g_rng * 1664525u + 1013904223u;
  return static_cast<int> (g_rng % static_cast<uint32_t> (bound));
}

// fixed pair table, cycled so every repeat searches the same workload
constexpr int kPairCount = 256;
int g_pair_src[kPairCount] {};
int g_pair_dst[kPairCount] {};
int g_pair_index = 0;

void BuildPairs (int nodes) {
  for (int i = 0; i < kPairCount; ++i) {
    g_pair_src[i] = NextRandom (nodes);
    g_pair_dst[i] = NextRandom (nodes);

    if (g_pair_src[i] == g_pair_dst[i]) {
      g_pair_dst[i] = (g_pair_dst[i] + 1) % nodes;
    }
  }
  g_pair_index = 0;
}

// old parent-link scan kept locally so the bench can show why prod passes edge distance directly
class ScanHeuristic final : public PlannerHeuristic {
public:
  float G (Team, int current_index, int parent_index, int) const override {
    if (parent_index == kInvalidNodeIndex) {
      return 0.0f;
    }
    for (const auto &link : graph[parent_index].links) {
      if (link.index == current_index) {
        if (has_flag (graph[current_index].flags, NodeFlag::Crouch | NodeFlag::Ladder)) {
          return static_cast<float> (link.distance) * 1.5f;
        }
        return static_cast<float> (link.distance);
      }
    }
    return kInfiniteHeuristic;
  }
  float H (int index, int goal_index) const override {
    return plannerHeuristics::fast.H (index, goal_index);
  }
};

} // namespace

TEST_CASE ("bench/planner") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  Boot (engine, cs);
  BuildGridGraph (kGridSide);
  BuildPairs (kGridNodes);

  HOST_REQUIRE (graph.Length () == kGridNodes);

  // route reset: full sweep of every cell (the old clearRoute) vs one epoch bump
  ystl::Array<AStarAlgo::Route> routes {};
  routes.resize (static_cast<size_t> (kGridNodes));

  uint32_t epoch = 0;

  BENCHMARK ("astar/reset_full") {
    for (int i = 0; i < kGridNodes; ++i) {
      auto &route = routes[static_cast<size_t> (i)];
      route.g = route.f = 0.0f;
      route.parent = kInvalidNodeIndex;
      route.epoch = 0;
      route.state = RouteState::New;
    }
    ystl::benchmark::deoptimize_value (routes[kGridNodes - 1].g);
  }
  BENCHMARK_END;

  BENCHMARK ("astar/reset_epoch") {
    ++epoch;
    ystl::benchmark::deoptimize_value (epoch);
  }
  BENCHMARK_END;

  // g heuristic: scan parent links (old) vs a known edge distance (new)
  const int parent = NodeAt (20, 20);
  const auto &parent_links = graph.paths_[static_cast<size_t> (parent)].links;
  const int child = parent_links[kMaxNodeLinks - 1].index; // last slot, worst-case scan

  HOST_REQUIRE (child != kInvalidNodeIndex);

  const ScanHeuristic scan_kernel {};
  const int scan_edge = parent_links[kMaxNodeLinks - 1].distance;

  BENCHMARK ("astar/kernel_scan") {
    const float cost = scan_kernel.G (Team::CT, child, parent, 0);
    ystl::benchmark::deoptimize_value (cost);
  }
  BENCHMARK_END;

  BENCHMARK ("astar/kernel_direct") {
    const float cost = plannerHeuristics::fast.G (Team::CT, child, parent, scan_edge);
    ystl::benchmark::deoptimize_value (cost);
  }
  BENCHMARK_END;

  // end-to-end find on the grid: direct edge distance vs old parent scan
  AStarAlgo astar (graph.Length ());
  astar.SetHeuristic (&plannerHeuristics::fast);

  const ScanHeuristic scan_heuristic {};
  AStarAlgo astar_scan (graph.Length ());
  astar_scan.SetHeuristic (&scan_heuristic);

  int added = 0;
  ystl::Lambda<bool (int)> sink = [&] (int) {
    ++added;
    return true;
  };

  BENCHMARK ("astar/find_fast") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const auto result = astar.Find (Team::CT, g_pair_src[p], g_pair_dst[p], sink);
    ystl::benchmark::deoptimize_value (result);
  }
  BENCHMARK_END;

  BENCHMARK ("astar/find_scan") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const auto result = astar_scan.Find (Team::CT, g_pair_src[p], g_pair_dst[p], sink);
    ystl::benchmark::deoptimize_value (result);
  }
  BENCHMARK_END;

  // adjacent pair: search is cheap, per-search fixed cost shows through
  BENCHMARK ("astar/find_short") {
    const auto result = astar.Find (Team::CT, 0, 1, sink);
    ystl::benchmark::deoptimize_value (result);
  }
  BENCHMARK_END;

  // --- dijkstra on the same 40x40 grid ---
  DijkstraAlgo dijkstra;
  dijkstra.Init (graph.Length ());

  BENCHMARK ("dijkstra/find") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const bool ok = dijkstra.Find (g_pair_src[p], g_pair_dst[p], sink, nullptr);
    ystl::benchmark::deoptimize_value (ok);
  }
  BENCHMARK_END;

  DistanceTable dt {};
  BENCHMARK ("dijkstra/dist_all") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const bool ok = dijkstra.DistAll (g_pair_src[p], dt, 0);
    ystl::benchmark::deoptimize_value (dt.Length ());
    ystl::benchmark::deoptimize_value (ok);
  }
  BENCHMARK_END;

  // --- worst-case graph: 64x64 == kMaxNodes (4096) ---
  BuildGridGraph (kLargeSide);
  BuildPairs (kLargeNodes);

  HOST_REQUIRE (graph.Length () == kLargeNodes);

  AStarAlgo astar_big (graph.Length ());
  astar_big.SetHeuristic (&plannerHeuristics::fast);

  DijkstraAlgo dijkstra_big;
  dijkstra_big.Init (graph.Length ());

  BENCHMARK ("astar/find_4096") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const auto result = astar_big.Find (Team::CT, g_pair_src[p], g_pair_dst[p], sink);
    ystl::benchmark::deoptimize_value (result);
  }
  BENCHMARK_END;

  BENCHMARK ("dijkstra/find_4096") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const bool ok = dijkstra_big.Find (g_pair_src[p], g_pair_dst[p], sink, nullptr);
    ystl::benchmark::deoptimize_value (ok);
  }
  BENCHMARK_END;

  // --- floyd-warshall: one-off build cost + hot lookups on a 32x32 grid ---
  BuildGridGraph (kFloydSide);
  BuildPairs (kFloydNodes);

  HOST_REQUIRE (graph.Length () == kFloydNodes);

  FloydWarshallAlgo floyd;
  const ystl::Stopwatch build_clock {};
  const bool loaded = floyd.Load ();
  const double build_ns = build_clock.elapsed_ns ();

  ystl::benchmark::deoptimize_value (loaded);
  // count="1" makes the per-op column read as the single build wall time
  ystl::benchmark::report ("floyd/build", 1, build_ns);

  ystl::Lambda<bool (int)> fsink = [] (int) {
    return true;
  };

  BENCHMARK ("floyd/find") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const bool ok = floyd.Find (g_pair_src[p], g_pair_dst[p], fsink, nullptr);
    ystl::benchmark::deoptimize_value (ok);
  }
  BENCHMARK_END;

  BENCHMARK ("floyd/dist") {
    const int p = g_pair_index++ & (kPairCount - 1);
    const int d = floyd.Dist (g_pair_src[p], g_pair_dst[p]);
    ystl::benchmark::deoptimize_value (d);
  }
  BENCHMARK_END;

  ystl::benchmark::deoptimize_value (added);
  testhost::CloseFakeCs (cs);
}

} // namespace bot
