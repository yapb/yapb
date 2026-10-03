//
// YaPB test host: unit/planner_search.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for planner.cpp: the PlannerHeuristic g/h functions
// across modes and flags, the A* search with its guards, Dijkstra,
// Floyd-Warshall and the PathPlanner facade with the memory-limit
// fallback. Randomness is pinned via exact modes and extreme
// chances, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

// chain 0 - 7 with an island (8) and a no-hostage spur (9),
// plus a close unmatched node (10) for the skip predicates
void BuildPlannerGraph () {
  graph.Reset ();

  for (int i = 0; i < 8; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto add_node = [] (const ystl::Vector &origin, int32_t flags) {
    Path path {};
    path.origin = origin;
    path.number = graph.Length ();
    path.flags = flags;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
    return path.number;
  };
  const int island = add_node (ystl::Vector (2000.0f, 0.0f, 0.0f), 0);
  const int nohost = add_node (ystl::Vector (800.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::NoHostage));
  const int close = add_node (ystl::Vector (20.0f, 0.0f, 30.0f), 0);

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  for (int i = 0; i < 7; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  link (7, nohost, 100);
  link (nohost, 7, 100);

  graph.PopulateNodes ();
  planner.Init ();

  HOST_REQUIRE (island == 8 && nohost == 9 && close == 10);
}

void BootPlanner (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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

int FindLinkSlot (int from, int to) {
  const auto &links = graph.paths_[from].links;

  for (int s = 0; s < kMaxNodeLinks; ++s) {
    if (links[s].index == to) {
      return s;
    }
  }
  return kInvalidNodeIndex;
}

} // namespace

TEST_CASE ("unit/planner_search") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootPlanner (engine, cs);
  BuildPlannerGraph ();

  // distance heuristics across all five modes, axis exact...
  cv_path_heuristic_mode.Set (0);
  CHECK (plannerHeuristics::fast.H (0, 7) == 700.0f);
  cv_path_heuristic_mode.Set (1);
  CHECK (plannerHeuristics::fast.H (0, 7) == 700.0f);
  cv_path_heuristic_mode.Set (2);
  CHECK (plannerHeuristics::fast.H (0, 7) == 0.0f);
  cv_path_heuristic_mode.Set (3);
  CHECK (plannerHeuristics::fast.H (0, 7) == 700.0f);
  cv_path_heuristic_mode.Set (4);
  CHECK (plannerHeuristics::fast.H (0, 7) == 700.0f);
  cv_path_heuristic_mode.Set (0);

  // ...hostage flavor bans no-hostage nodes, weighted scales down...
  CHECK (plannerHeuristics::fast_hostage.H (9, 7) == kInfiniteHeuristic);
  CHECK (plannerHeuristics::fast_hostage.H (1, 7) == 600.0f);
  CHECK (plannerHeuristics::safe.H (0, 7) == 0.546875f);

  // ...link-distance costs use the caller edge distance, crouch inflates...
  CHECK (plannerHeuristics::fast.G (Team::CT, 1, 0, 100) == 100.0f);
  CHECK (plannerHeuristics::fast.G (Team::CT, 1, kInvalidNodeIndex, 0) == 0.0f);

  graph.paths_[2].flags |= NodeFlag::Crouch;
  CHECK (plannerHeuristics::fast.G (Team::CT, 2, 1, 100) == 150.0f);
  CHECK (plannerHeuristics::fast_hostage.G (Team::CT, 2, 1, 100) == 750.0f);
  graph.paths_[2].flags &= ~NodeFlag::Crouch;

  CHECK (plannerHeuristics::fast_hostage.G (Team::CT, 9, 7, 100) == kInfiniteHeuristic);
  CHECK (plannerHeuristics::fast_hostage.G (Team::CT, 1, 0, 100) == 100.0f);

  // ...jump links also block hostage transports...
  graph.paths_[0].links[FindLinkSlot (0, 1)].flags = static_cast<uint16_t> (ystl::to_underlying (PathFlag::Jump));
  CHECK (plannerHeuristics::fast_hostage.G (Team::CT, 1, 0, 100) == kInfiniteHeuristic);
  CHECK (plannerHeuristics::safe_hostage.G (Team::CT, 1, 0, 0) == kInfiniteHeuristic);
  CHECK (plannerHeuristics::optimal_hostage.G (Team::CT, 1, 0, 0) == kInfiniteHeuristic);
  graph.paths_[0].links[FindLinkSlot (0, 1)].flags = 0;

  // ...kill costs are the team-damage floor without practice data...
  // (unloaded practice still prices every node at 1)
  CHECK (plannerHeuristics::optimal.G (Team::CT, 1, 0, 0) == 1.0f);
  CHECK (plannerHeuristics::optimal.G (Team::CT, 1, kInvalidNodeIndex, 0) == 0.0f);
  CHECK (plannerHeuristics::safe.G (Team::CT, 1, 0, 0) == 0.0f);
  CHECK (plannerHeuristics::optimal_hostage.G (Team::CT, 9, 7, 0) == kInfiniteHeuristic);
  CHECK (plannerHeuristics::optimal_hostage.G (Team::CT, 1, 0, 0) == 1.0f);
  CHECK (plannerHeuristics::safe_hostage.G (Team::CT, 1, 0, 0) == 0.0f);

  graph.paths_[2].flags |= NodeFlag::Crouch;
  CHECK (plannerHeuristics::optimal.G (Team::CT, 2, 1, 0) == 1.5f);
  CHECK (plannerHeuristics::optimal_hostage.G (Team::CT, 2, 1, 0) == 7.5f);
  graph.paths_[2].flags &= ~NodeFlag::Crouch;

  // ...random flavors stay inside their documented bands...
  const float random_g = plannerHeuristics::diversity.G (Team::CT, 1, 0, 100);
  CHECK (random_g >= 50.0f && random_g <= 150.0f);
  CHECK (plannerHeuristics::diversity.G (Team::CT, 1, kInvalidNodeIndex, 0) == 0.0f);

  const float random_h = plannerHeuristics::diversity.H (0, 7);
  CHECK (random_h >= 210.0f && random_h <= 700.0f);

  // ...skip predicates: zero radius, height, narrow, range, jumps...
  graph.paths_[0].radius = 48.0f;
  graph.paths_[1].radius = 48.0f;
  CHECK (!AStarAlgo::CantSkipNode (0, 1, true));

  graph.paths_[1].flags |= NodeFlag::Narrow;
  CHECK (AStarAlgo::CantSkipNode (0, 1, true));
  graph.paths_[1].flags &= ~NodeFlag::Narrow;

  graph.paths_[0].radius = 0.0f;
  CHECK (AStarAlgo::CantSkipNode (0, 1, true)); // zero radius never skips
  graph.paths_[0].radius = 48.0f;

  CHECK (AStarAlgo::CantSkipNode (0, 10, true)); // 30 higher and too close
  CHECK (AStarAlgo::CantSkipNode (0, 8, true)); // island out of range

  graph.paths_[0].links[FindLinkSlot (0, 1)].flags = static_cast<uint16_t> (ystl::to_underlying (PathFlag::Jump));
  CHECK (AStarAlgo::CantSkipNode (0, 1, true));
  graph.paths_[0].links[FindLinkSlot (0, 1)].flags = 0;

  // ...visibility check runs only when asked, empty table denies...
  CHECK (AStarAlgo::CantSkipNode (0, 1, false));
  graph.paths_[0].radius = 0.0f;
  graph.paths_[1].radius = 0.0f;

  // ...a* walks the chain backwards, guards hold...
  ystl::SmallArray<int> seq {};
  ystl::Lambda<bool (int)> sink = [&] (int node) {
    seq.push (node);
    return true;
  };
  AStarAlgo astar (graph.Length ());
  astar.SetHeuristic (&plannerHeuristics::fast);

  CHECK (astar.Find (Team::CT, 0, 7, sink) == AStarResult::Success);
  HOST_REQUIRE (seq.size () == 8);

  bool ordered = true;

  for (size_t i = 0; i < seq.size (); ++i) {
    ordered = ordered && seq[i] == 7 - static_cast<int> (i);
  }
  CHECK (ordered);

  seq.clear ();
  CHECK (astar.Find (Team::CT, 2, 2, sink) == AStarResult::Success);
  CHECK (seq.size () == 1 && seq[0] == 2);

  CHECK (astar.Find (Team::CT, -1, 7, sink) == AStarResult::Failed);
  CHECK (astar.Find (Team::CT, 0, 99, sink) == AStarResult::Failed);
  CHECK (astar.Find (Team::CT, 0, 8, sink) == AStarResult::Failed);

  AStarAlgo unset (graph.Length ());
  CHECK (unset.Find (Team::CT, 0, 7, sink) == AStarResult::InternalError);

  AStarAlgo tiny (3);
  tiny.SetHeuristic (&plannerHeuristics::fast);
  CHECK (tiny.Find (Team::CT, 0, 2, sink) == AStarResult::InternalError);

  // ...hostage-flavored a* still routes the plain chain...
  seq.clear ();
  astar.SetHeuristic (&plannerHeuristics::fast_hostage);
  CHECK (astar.Find (Team::CT, 0, 7, sink) == AStarResult::Success);
  CHECK (seq.size () == 8 && seq[0] == 7);

  // ...dijkstra walks it forwards with exact distances...
  DijkstraAlgo dijkstra;
  dijkstra.Init (graph.Length ());

  ystl::SmallArray<int> dseq {};
  ystl::Lambda<bool (int)> dsink = [&] (int node) {
    dseq.push (node);
    return true;
  };
  int path_distance = -1;
  CHECK (dijkstra.Find (0, 7, dsink, &path_distance));
  CHECK (path_distance == 700);
  HOST_REQUIRE (dseq.size () == 8);

  bool dordered = true;

  for (size_t i = 0; i < dseq.size (); ++i) {
    dordered = dordered && dseq[i] == static_cast<int> (i);
  }
  CHECK (dordered);
  CHECK (dijkstra.Dist (0, 7) == 700);
  CHECK (!dijkstra.Find (-1, 7, dsink, nullptr));
  CHECK (!dijkstra.Find (0, 8, dsink, nullptr));

  // ...floyd rebuilds from the live graph and routes by next hop...
  FloydWarshallAlgo floyd;
  HOST_REQUIRE (floyd.Load ());
  CHECK (!floyd.IsRebuilding ());
  CHECK (floyd.Dist (0, 7) == 700);
  CHECK (floyd.Dist (0, 8) == FloydWarshallAlgo::kInfinity);
  CHECK (floyd.Cell (0, 7).index == 1);
  CHECK (floyd.Cell (7, 7).dist == 0);

  ystl::SmallArray<int> fseq {};
  ystl::Lambda<bool (int)> fsink = [&] (int node) {
    fseq.push (node);
    return true;
  };
  int floyd_distance = -1;
  CHECK (floyd.Find (0, 7, fsink, &floyd_distance));
  CHECK (floyd_distance == 700);
  CHECK (fseq.size () == 8 && fseq[0] == 0 && fseq[7] == 7);
  CHECK (!floyd.Find (0, 8, fsink, nullptr));
  CHECK (!floyd.Find (0, 99, fsink, nullptr));

  // ...the facade routes, measures exactly, and falls back cleanly...
  // (islands fail graph sanity, the planner records it and carries on)
  CHECK (planner.IsPathsCheckFailed ());
  CHECK (planner.HasRealPathDistance ());

  ystl::SmallArray<int> pseq {};
  ystl::Lambda<bool (int)> psink = [&] (int node) {
    pseq.push (node);
    return true;
  };
  int planner_distance = -1;
  CHECK (planner.Find (0, 7, psink, &planner_distance));
  CHECK (planner_distance == 700);
  CHECK (pseq.size () == 8);
  CHECK (planner.Dist (0, 7) == 700.0f);
  CHECK (planner.PreciseDistance (0, 7) == 700.0f);
  CHECK (!planner.Find (0, 99, psink, nullptr));
  CHECK (planner.Dist (0, 99) == static_cast<float> (kInfiniteDistanceLong));

  // ...forced memory pressure falls back to dijkstra with simple 2d...
  // (real cvar path: zero limit trips init() even on tiny test graphs)
  {
    const float saved_limit = cv_path_floyd_memory_limit.As<float> ();
    cv_path_floyd_memory_limit.Set (0.0f);
    planner.Init ();
    CHECK (planner.IsMemoryLimitHit ());
    CHECK (!planner.HasRealPathDistance ());
    CHECK (planner.PreciseDistance (0, 7) == 700.0f);
    CHECK (planner.Dist (0, 10) == graph[0].origin.distance2d (graph[10].origin));
    CHECK (planner.PreciseDistance (0, 10) == static_cast<float> (kInfiniteDistanceLong));
    cv_path_floyd_memory_limit.Set (saved_limit);
    planner.Init ();
    CHECK (!planner.IsMemoryLimitHit ());
  }
}

} // namespace bot
