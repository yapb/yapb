//
// YaPB test host: unit/graph_{core,edit,serial}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for graph.cpp/h on synthetic in-memory graphs:
// core (paths, search, reachability, conversion, text roundtrip),
// edit (editor-driven mutations), without skipping untested helpers.
//

#include <cstdio>

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct GraphHook {
  static bool IsWalkableSlope (const Graph &g, const ystl::Vector &src, const ystl::Vector &dst) {
    return g.IsWalkableSlope (src, dst);
  }
  static bool IsWalkableDrop (const Graph &g, const ystl::Vector &src, const ystl::Vector &dst) {
    return g.IsWalkableDrop (src, dst);
  }
};

namespace {

// node with all link slots invalidated (fresh PathLink defaults to index 0,
// which would read as a link to node 0)
int AddTestNode (const ystl::Vector &pos, int32_t flags = 0) {
  Path path {};
  path.origin = pos;
  path.flags = flags;
  path.light = kInvalidLightLevel; // add() inits unknown light the same way

  for (auto &link : path.links) {
    link.index = kInvalidNodeIndex;
  }
  graph.paths_.push (path);

  const int index = static_cast<int> (graph.paths_.size ()) - 1;
  graph.paths_[static_cast<size_t> (index)].number = index;

  return index;
}

void LinkTestNodes (int from, int to, int distance, uint16_t flags = 0) {
  for (auto &link : graph.paths_[static_cast<size_t> (from)].links) {
    if (link.index == kInvalidNodeIndex) {
      link.index = static_cast<int16_t> (to);
      link.distance = distance;
      link.flags = flags;
      return;
    }
  }
  HOST_REQUIRE (false); // no free link slot in the fixture
}

bool ContainsNode (const ystl::SmallArray<int32_t> &list, int32_t index) {
  for (const auto &at : list) {
    if (at == index) {
      return true;
    }
  }
  return false;
}

// reset() wipes the edit flag as well: editor tests always restore it
void ResetEdit () {
  graph.Reset ();
  graph.SetEditFlag (GraphEdit::On);
}

} // namespace

TEST_CASE ("unit/graph_core") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // precache resolves the GameRef cvars (sv_gravity et al.)
  game.Precache ();

  // reset() wipes paths, buckets, authors and the changed flag
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  graph.info_.author.assign ("tester");
  graph.Reset ();

  CHECK (graph.Length () == 0);
  CHECK (!graph.HasChanged ());
  CHECK (graph.info_.author.empty ());
  CHECK (!graph.HasEditFlag (GraphEdit::On));
  CHECK (graph.GetNodesInBucket (ystl::Vector (0.0f, 0.0f, 0.0f)) == nullptr);

  // add() without an editor is a no-op (analyzer is idle too)
  graph.Add (NodeAddFlag::Normal, ystl::Vector (100.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 0);

  // link management: invalid/self/duplicate handling and eviction
  const int n0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int n1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int n2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));

  graph.AddPath (-1, n1, 10.0f);
  graph.AddPath (n0, 99, 10.0f);
  graph.AddPath (n0, n0, 10.0f);
  CHECK (!graph.IsConnected (n0, n1));

  graph.AddPath (n0, n1, 10.0f);
  CHECK (graph.IsConnected (n0, n1));
  CHECK (!graph.IsConnected (n1, n0));

  graph.AddPath (n0, n1, 100.0f); // duplicate denied
  int n0links = 0;
  for (const auto &link : graph.paths_[0].links) {
    n0links += link.index != kInvalidNodeIndex ? 1 : 0;
  }
  CHECK (n0links == 1);

  // jump links survive eviction, the longest walk link goes instead
  const uint16_t jump = ystl::to_underlying (PathFlag::Jump);
  graph.AddPath (n0, n2, 1000.0f);
  graph.paths_[0].links[1].flags = jump;

  // nodes 3..10 exist now; fill the remaining 6 slots of node 0
  for (int i = 3; i <= 10; ++i) {
    AddTestNode (ystl::Vector (1000.0f + i * 10.0f, 0.0f, 0.0f));
  }
  for (int i = 3; i <= 8; ++i) {
    graph.AddPath (n0, i, static_cast<float> (i * 10));
  }
  // node 0 full: 8 links (slot0 -> n1, slot1 -> n2 jump, slots -> 3..8)
  graph.AddPath (n0, 9, 5.0f); // evicts the longest non-jump link (node 8)

  CHECK (!graph.IsConnected (n0, 8));
  CHECK (graph.IsConnected (n0, 9));
  CHECK (graph.IsConnected (n0, n2)); // jump survives
  CHECK (graph.paths_[0].links[1].flags == jump);

  // all-jump node: eviction finds no victim, the graph is untouched
  for (int i = 0; i < kMaxNodeLinks; ++i) {
    graph.paths_[0].links[i].flags = jump;
  }
  graph.AddPath (n0, 8, 5.0f);
  CHECK (!graph.IsConnected (n0, 8)); // no non-jump victim available
  CHECK (graph.IsConnected (n0, 9)); // eviction did not disturb the rest

  // incoming connectivity: node 1 points back at node 0
  LinkTestNodes (n1, n0, 100);
  CHECK (graph.IsConnected (n0)); // something points at node 0

  // slot-level removal drops exactly one slot
  graph.UnassignPath (n0, 0);
  CHECK (!graph.IsConnected (n0, n1));
  CHECK (graph.IsConnected (n0)); // node 1 still points back at node 0

  // reachability over a chain with an island
  graph.Reset ();
  const int c0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int c1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int c2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));
  const int c3 = AddTestNode (ystl::Vector (900.0f, 0.0f, 0.0f));
  LinkTestNodes (c0, c1, 100);
  LinkTestNodes (c1, c2, 100);

  ystl::Array<bool> forward {}, backward {};
  graph.ComputeReachability (forward, backward);
  HOST_REQUIRE (forward.size () == 4 && backward.size () == 4);
  CHECK (forward[0] && forward[1] && forward[2] && !forward[3]);
  CHECK (backward[0] && !backward[1] && !backward[2] && !backward[3]);

  CHECK (graph.IsConnected (c1)); // something points at node 1
  CHECK (!graph.IsConnected (c3)); // island: nobody points at node 3

  // visited markers only track goal nodes
  CHECK (!graph.IsVisited (c1));
  graph.SetVisited (c0); // not a goal: not recorded
  CHECK (!graph.IsVisited (c0));
  graph.paths_[1].flags |= NodeFlag::Goal;
  graph.SetVisited (c1);
  CHECK (graph.IsVisited (c1));
  graph.ClearVisited ();
  CHECK (!graph.IsVisited (c1));
  graph.SetVisited (99); // invalid index is a no-op

  // nearest queries: exact, ranged, flagged and farthest
  CHECK (graph.GetNearestNoBuckets (ystl::Vector (5.0f, 0.0f, 0.0f)) == c0);
  CHECK (graph.GetNearestNoBuckets (ystl::Vector (0.0f, 0.0f, 0.0f), 10.0f) == c0);
  CHECK (graph.GetNearestNoBuckets (ystl::Vector (500.0f, 0.0f, 0.0f), 10.0f) == kInvalidNodeIndex);
  CHECK (graph.GetNearest (ystl::Vector (150.0f, 0.0f, 0.0f)) == c1); // small map: bucket fallback
  CHECK (graph.GetNearest (ystl::Vector (150.0f, 0.0f, 0.0f), kInfiniteDistance, NodeFlag::Goal) == c1);
  CHECK (graph.GetNearest (ystl::Vector (150.0f, 0.0f, 0.0f), kInfiniteDistance, NodeFlag::Camp) == kInvalidNodeIndex);
  CHECK (graph.GetForAnalyzer (ystl::Vector (0.0f, 0.0f, 0.0f), 1000.0f) == c0);
  CHECK (graph.GetForAnalyzer (ystl::Vector (500.0f, 0.0f, 0.0f), 10.0f) == kInvalidNodeIndex);
  CHECK (graph.GetFarest (ystl::Vector (0.0f, 0.0f, 0.0f), 0.0f) == c3);

  const auto in_radius = graph.GetNearestInRadius (150.0f, ystl::Vector (0.0f, 0.0f, 0.0f));
  CHECK (in_radius.size () == 2 && ContainsNode (in_radius, c0) && ContainsNode (in_radius, c1));

  // buckets: shared positions land in one bucket, init clears them
  graph.Reset ();
  for (int i = 0; i < 10; ++i) {
    const int node = AddTestNode (ystl::Vector (static_cast<float> (i), 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (static_cast<float> (i), 0.0f, 0.0f), node);
  }
  const auto *bucket = graph.GetNodesInBucket (ystl::Vector (4.0f, 0.0f, 0.0f));
  HOST_REQUIRE (bucket != nullptr);
  CHECK (bucket->size () == 10);

  graph.EraseFromBucket (ystl::Vector (4.0f, 0.0f, 0.0f), 4);
  CHECK (graph.GetNodesInBucket (ystl::Vector (4.0f, 0.0f, 0.0f))->size () == 9);

  graph.InitBuckets ();
  CHECK (graph.GetNodesInBucket (ystl::Vector (4.0f, 0.0f, 0.0f)) == nullptr);
  CHECK (graph.LocateBucket (ystl::Vector (1.0f, 2.0f, 3.0f)) == graph.LocateBucket (ystl::Vector (1.0f, 2.0f, 3.0f)));

  // bucket search path on a 200-node line agrees with the linear scan
  graph.Reset ();
  for (int i = 0; i < 200; ++i) {
    const int node = AddTestNode (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f), node);
  }
  // ten nodes share the origin bucket so the bucket branch is really taken
  for (int i = 200; i < 210; ++i) {
    const int node = AddTestNode (ystl::Vector (static_cast<float> (i - 200), 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (2.0f, 0.0f, 0.0f), node);
  }
  const ystl::Vector probe (2.0f, 0.0f, 0.0f);
  CHECK (graph.GetNearest (probe) == graph.GetNearestNoBuckets (probe));

  // maxCount caps the result at exactly maxCount entries
  const auto capped = graph.GetNearestInRadius (100000.0f, ystl::Vector (0.0f, 0.0f, 0.0f), 1);
  CHECK (capped.size () == 1);
  const auto capped2 = graph.GetNearestInRadius (100000.0f, ystl::Vector (0.0f, 0.0f, 0.0f), 2);
  CHECK (capped2.size () == 2);
  const auto uncapped = graph.GetNearestInRadius (100000.0f, ystl::Vector (0.0f, 0.0f, 0.0f), -1);
  CHECK (uncapped.size () == 210);

  // travel time is pure 2d distance over speed
  CHECK (graph.CalculateTravelTime (260.0f, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (260.0f, 0.0f, 50.0f)) == 1.0f);

  // flag to point-type mapping, first match wins
  CHECK (Graph::FlagToPointType (NodeFlag::TerroristOnly) == PointType::Terrorist);
  CHECK (Graph::FlagToPointType (NodeFlag::CTOnly) == PointType::CT);
  CHECK (Graph::FlagToPointType (NodeFlag::Goal) == PointType::Goal);
  CHECK (Graph::FlagToPointType (NodeFlag::Camp) == PointType::Camp);
  CHECK (Graph::FlagToPointType (NodeFlag::Sniper) == PointType::Sniper);
  CHECK (Graph::FlagToPointType (NodeFlag::Rescue) == PointType::Rescue);
  CHECK (Graph::FlagToPointType (NodeFlag::Button) == PointType::Count);
  CHECK (Graph::FlagToPointType (NodeFlag::TerroristOnly | NodeFlag::Goal) == PointType::Terrorist);

  // point buckets fill from flags, random draws stay inside
  graph.Reset ();
  const int t = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::TerroristOnly));
  const int g = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Goal));
  graph.PopulateNodes ();

  CHECK (graph.GetPoints (PointType::Terrorist).size () == 1);
  CHECK (graph.GetPoints (PointType::Terrorist)[0] == t);
  CHECK (graph.GetPoints (PointType::Goal).size () == 1);
  CHECK (graph.GetPoints (PointType::Goal)[0] == g);
  CHECK (graph.GetRandomPoint (PointType::Terrorist) == t);
  CHECK (graph.GetNodeNumbers ().size () == 2);
  CHECK (graph.GetNodeNumbers ()[0] == t && graph.GetNodeNumbers ()[1] == g);

  // legacy conversion roundtrip, camp directions optional via cvar
  LegacyPath legacy {};
  legacy.number = 3;
  legacy.flags = ystl::to_underlying (NodeFlag::Camp);
  legacy.origin = ystl::Vector (10.0f, 20.0f, 30.0f);
  legacy.csx = 100.0f;
  legacy.csy = 200.0f;
  legacy.cex = 300.0f;
  legacy.cey = 400.0f;
  legacy.radius = 32.5f;
  legacy.index[0] = 1;
  legacy.distance[0] = 150;
  legacy.conflags[0] = 1;
  legacy.velocity[0] = ystl::Vector (1.0f, 2.0f, 3.0f);
  for (int i = 1; i < kMaxNodeLinks; ++i) {
    legacy.index[i] = kInvalidNodeIndex;
  }

  Path converted {};
  graph.ConvertFromLegacy (converted, legacy); // fixcamp defaults to off
  CHECK (converted.number == 3);
  CHECK (converted.origin.x == 10.0f && converted.origin.z == 30.0f);
  CHECK (converted.start.x == 100.0f && converted.start.y == 200.0f);
  CHECK (converted.end.x == 300.0f && converted.end.y == 400.0f);
  CHECK (converted.radius == 32.5f);
  CHECK (converted.links[0].index == 1 && converted.links[0].distance == 150);
  CHECK (converted.links[0].flags == 1);
  CHECK (converted.links[0].velocity.z == 3.0f);
  CHECK (converted.light == kInvalidLightLevel);

  LegacyPath back {};
  graph.ConvertToLegacy (converted, back);
  CHECK (back.number == 3 && back.radius == 32.5f);
  CHECK (back.csx == 100.0f && back.cey == 400.0f);
  CHECK (back.index[0] == 1 && back.distance[0] == 150 && back.conflags[0] == 1);

  cv_graph_fixcamp.Set (1.0f);
  Path camped {};
  graph.ConvertFromLegacy (camped, legacy); // m_paths holds t/g above: gate open
  CHECK (camped.start.length_sq () > 0.0f && camped.start.x != 100.0f);
  CHECK (camped.start.x >= -180.0f && camped.start.x <= 180.0f);
  cv_graph_fixcamp.Set (0.0f);

  // missing podbot file converts to false without touching the graph
  CHECK (!graph.ConvertOldFormat ());
  CHECK (graph.Length () == 2);

  // visited markers, search index, auto distance and cache messaging
  graph.SetVisited (g);
  CHECK (graph.IsVisited (g));

  graph.SetSearchIndex (g);

  bool saw_direction = false;
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), "direction") != nullptr) {
      saw_direction = true;
    }
  }
  CHECK (saw_direction);

  graph.SetSearchIndex (99); // invalid: silent reset, no crash

  graph.SetAutoPathDistance (0.0f);
  graph.SetAutoPathDistance (50.0f);

  bool saw_auto = false;
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), "Autopath") != nullptr) {
      saw_auto = true;
    }
  }
  CHECK (saw_auto);
  graph.SetAutoPathDistance (250.0f); // documented default, reachability depends on it

  graph.CachePoint (t);
  graph.CachePoint (99); // invalid: cleared with a message, no crash

  // reachability: flat hops pass, high/far hops fail, jump arc is validated
  CHECK (graph.IsNodeReacheable (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (!graph.IsNodeReacheable (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100.0f)));
  CHECK (!graph.IsNodeReacheable (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (1000.0f, 0.0f, 0.0f)));
  CHECK (graph.IsNodeReacheableWithJump (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (300.0f, 0.0f, 0.0f)));
  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100.0f)));

  graph.SetAutoPathDistance (50.0f);
  CHECK (!graph.IsNodeReacheable (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  graph.SetAutoPathDistance (250.0f);

  // walkable slope: engine step height gates the follow
  CHECK (GraphHook::IsWalkableSlope (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 10.0f)));
  CHECK (!GraphHook::IsWalkableSlope (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100.0f)));
  CHECK (GraphHook::IsWalkableSlope (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));

  // a rising lip above step height aborts the follow (fresh coords: no cache)
  static int slope_calls = 0;
  slope_calls = 0;
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = (slope_calls++ == 0) ? 0.1f : 0.5f;
  });
  CHECK (!GraphHook::IsWalkableSlope (graph, ystl::Vector (10000.0f, 0.0f, 0.0f), ystl::Vector (10100.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);

  // zero step height rejects every slope
  sv_stepsize.Set (0);
  CHECK (!GraphHook::IsWalkableSlope (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  sv_stepsize.Set (18);

  // walkable drop: only drops with a matching fall curve and clear air
  CHECK (GraphHook::IsWalkableDrop (graph, ystl::Vector (0.0f, 0.0f, 100.0f), ystl::Vector (0.0f, 0.0f, 50.0f)));
  CHECK (!GraphHook::IsWalkableDrop (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (0.0f, 0.0f, 100.0f)));
  CHECK (!GraphHook::IsWalkableDrop (graph, ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  CHECK (GraphHook::IsWalkableDrop (graph, ystl::Vector (0.0f, 0.0f, 100.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));

  // a blocked fall curve aborts the drop
  engine.SetTraceHullHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  CHECK (!GraphHook::IsWalkableDrop (graph, ystl::Vector (20000.0f, 0.0f, 100.0f), ystl::Vector (20100.0f, 0.0f, 0.0f)));
  engine.SetTraceHullHook (nullptr);

  // zero gravity is never a drop
  sv_gravity.Set (0);
  CHECK (!GraphHook::IsWalkableDrop (graph, ystl::Vector (0.0f, 0.0f, 100.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));
  sv_gravity.Set (800);

  graph.Reset ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_edit") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  // no graph file on disk: activation auto-started the analyzer; editor
  // tests want the loaded-graph world instead (also restores messages)
  analyzer.Suspend ();
  graph.SetMessageSilence (false);
  CHECK (!analyzer.IsAnalyzing ());

  // frame without an editor is a silent no-op
  graph.Frame ();
  CHECK (!graph.HasEditor ());

  edict_t *editor = engine.SpawnClient ("Editor");
  HOST_REQUIRE (editor != nullptr);
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  editor->v.v_angle = ystl::Vector (10.0f, 20.0f, 30.0f);
  graph.SetEditor (editor);
  graph.SetEditFlag (GraphEdit::On);
  CHECK (graph.HasEditor ());
  CHECK (graph.GetEditor () == editor);

  // typed node creation, each carrying its flags
  ResetEdit ();
  graph.Add (NodeAddFlag::Normal, ystl::Vector (100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 1);
  CHECK (graph.paths_[0].number == 0);
  CHECK (graph.paths_[0].origin.x == 100.0f);

  editor->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (105.0f, 0.0f, 0.0f)); // too close: denied
  CHECK (graph.Length () == 1);

  graph.Add (NodeAddFlag::TOnly, ystl::Vector (200.0f, 0.0f, 0.0f));
  graph.Add (NodeAddFlag::CTOnly, ystl::Vector (300.0f, 0.0f, 0.0f));
  graph.Add (NodeAddFlag::NoHostage, ystl::Vector (400.0f, 0.0f, 0.0f));
  graph.Add (NodeAddFlag::Rescue, ystl::Vector (500.0f, 0.0f, 0.0f));
  graph.Add (NodeAddFlag::Goal, ystl::Vector (600.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 6);
  CHECK (has_flag (graph.paths_[1].flags, NodeFlag::Crossing | NodeFlag::TerroristOnly));
  CHECK (has_flag (graph.paths_[2].flags, NodeFlag::Crossing | NodeFlag::CTOnly));
  CHECK (has_flag (graph.paths_[3].flags, NodeFlag::NoHostage));
  CHECK (has_flag (graph.paths_[4].flags, NodeFlag::Rescue));
  CHECK (has_flag (graph.paths_[5].flags, NodeFlag::Goal));

  // camp nodes record the editor angles, camp-end updates them
  graph.Add (NodeAddFlag::Camp, ystl::Vector (700.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 7);
  CHECK (has_flag (graph.paths_[6].flags, NodeFlag::Camp));
  CHECK (graph.paths_[6].start.y == 20.0f && graph.paths_[6].end.y == 20.0f);

  editor->v.v_angle = ystl::Vector (40.0f, 50.0f, 60.0f);
  editor->v.origin = ystl::Vector (700.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::CampEnd, ystl::Vector (0.0f, 0.0f, 0.0f));
  CHECK (graph.paths_[6].end.y == 50.0f);

  editor->v.origin = ystl::Vector (100.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::CampEnd, ystl::Vector (800.0f, 0.0f, 0.0f)); // plain node: refused
  CHECK (graph.Length () == 7);

  // posture flags come from the editor state
  editor->v.flags |= FL_DUCKING;
  graph.Add (NodeAddFlag::Normal, ystl::Vector (900.0f, 0.0f, 0.0f));
  CHECK (has_flag (graph.paths_[7].flags, NodeFlag::Crouch));
  editor->v.flags &= ~FL_DUCKING;

  editor->v.movetype = MOVETYPE_FLY;
  graph.Add (NodeAddFlag::Normal, ystl::Vector (1000.0f, 0.0f, 0.0f));
  CHECK (has_flag (graph.paths_[8].flags, NodeFlag::Ladder));
  editor->v.movetype = MOVETYPE_WALK;

  // jump pair learns a flagged link between the two nodes
  ResetEdit ();
  editor->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpStart, ystl::Vector (0.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 1);
  graph.Add (NodeAddFlag::JumpEnd, ystl::Vector (100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 2);
  CHECK (graph.IsConnected (0, 1));
  CHECK (has_flag (graph.paths_[0].links[0].flags, PathFlag::Jump));

  // erase renumbers nodes and rewires links above the hole
  ResetEdit ();
  const int e0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int e1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int e2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));
  LinkTestNodes (e0, e1, 100);
  LinkTestNodes (e1, e0, 100);
  LinkTestNodes (e1, e2, 100);
  LinkTestNodes (e2, e1, 100);
  LinkTestNodes (e2, e0, 200);
  LinkTestNodes (e0, e2, 200);

  graph.Erase (e1);
  HOST_REQUIRE (graph.Length () == 2);
  CHECK (graph.paths_[0].number == 0 && graph.paths_[1].number == 1);
  // old 0 -> 2 slid down to 0 -> 1, old 2 -> 0 slid down to 1 -> 0
  CHECK (graph.IsConnected (0, 1));
  CHECK (graph.IsConnected (1, 0));

  // path surgery through the facing/cache slots
  ResetEdit ();
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  const int p0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int p1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int p2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));
  LinkTestNodes (p0, p1, 100);
  LinkTestNodes (p1, p0, 100);
  LinkTestNodes (p1, p2, 100);
  LinkTestNodes (p2, p1, 100);
  LinkTestNodes (p2, p0, 200);
  LinkTestNodes (p0, p2, 200);

  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.CachePoint (p1);
  graph.ErasePath ();
  CHECK (!graph.IsConnected (p0, p1));
  CHECK (graph.IsConnected (p1, p0)); // reverse direction untouched

  graph.ResetPath (p2);
  CHECK (!graph.IsConnected (p1, p2));
  CHECK (!graph.IsConnected (p2, p1));
  CHECK (!graph.IsConnected (p2, p0));

  // directional creation through the same slots
  ResetEdit ();
  const int d0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int d1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f)); // level with the eyes for facing
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.CachePoint (d1);

  graph.PathCreate (PathConnection::Outgoing);
  CHECK (graph.IsConnected (d0, d1) && !graph.IsConnected (d1, d0));

  graph.PathCreate (PathConnection::Incoming); // adds the reverse direction
  CHECK (graph.IsConnected (d0, d1) && graph.IsConnected (d1, d0));

  graph.PathCreate (PathConnection::Bidirectional);
  CHECK (graph.IsConnected (d0, d1) && graph.IsConnected (d1, d0));

  graph.PathCreate (PathConnection::Jumping);
  CHECK (has_flag (graph.paths_[d0].links[0].flags, PathFlag::Jump));
  CHECK (graph.paths_[d0].radius == 0.0f);

  // flag and radius editing through the nearest node
  graph.ToggleFlags (NodeFlag::Camp);
  CHECK (has_flag (graph.paths_[d0].flags, NodeFlag::Camp));

  graph.ToggleFlags (NodeFlag::Sniper); // camp present: allowed
  CHECK (has_flag (graph.paths_[d0].flags, NodeFlag::Sniper));

  graph.ToggleFlags (NodeFlag::Sniper); // toggles back off
  CHECK (!has_flag (graph.paths_[d0].flags, NodeFlag::Sniper));

  graph.ToggleFlags (NodeFlag::Camp); // camp off again
  graph.ToggleFlags (NodeFlag::Sniper); // refused without camp
  CHECK (!has_flag (graph.paths_[d0].flags, NodeFlag::Sniper));

  graph.SetRadius (d0, 55.5f);
  CHECK (graph.paths_[d0].radius == 55.5f);

  CHECK (graph.GetEditorNearest () == d0);

  // facing detection along the editor view direction
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (graph.GetFacingIndex () == d1);

  editor->v.v_angle = ystl::Vector (0.0f, 180.0f, 0.0f);
  CHECK (graph.GetFacingIndex () == kInvalidNodeIndex);

  // notify sounds reach the editor unless silenced
  const size_t sounds_before = engine.HeardSounds ().size ();
  graph.EmitNotify (NotifySound::Done);
  HOST_REQUIRE (engine.HeardSounds ().size () == sounds_before + 1);
  CHECK (engine.HeardSounds ()[sounds_before].sample == "common/wpn_hudon.wav");

  graph.SetMessageSilence (true);
  graph.EmitNotify (NotifySound::Done);
  CHECK (engine.HeardSounds ().size () == sounds_before + 1);
  graph.SetMessageSilence (false);

  // basic nodes grow from map entities
  ResetEdit ();
  edict_t *dm = engine.SpawnEntity ("info_player_deathmatch");
  edict_t *ct = engine.SpawnEntity ("info_player_start");
  edict_t *bomb = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (dm != nullptr && ct != nullptr && bomb != nullptr);
  dm->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  ct->v.origin = ystl::Vector (1000.0f, 0.0f, 0.0f);
  bomb->v.origin = ystl::Vector (2000.0f, 0.0f, 0.0f);

  const int basics_before = graph.Length ();
  graph.SeedBasicNodes ();
  CHECK (graph.Length () >= basics_before + 3);

  bool saw_goal = false;
  for (const auto &path : graph.paths_) {
    saw_goal = saw_goal || has_flag (path.flags, NodeFlag::Goal);
  }
  CHECK (saw_goal);

  // stats and file info print without crashing
  graph.ShowStats ();
  graph.ShowFileInfo ();

  bool saw_stats = false;
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), "Nodes:") != nullptr) {
      saw_stats = true;
    }
  }
  CHECK (saw_stats);

  // frame with an editor runs the edit logic without crashing
  graph.Frame ();

  graph.SetEditor (nullptr);
  graph.SetEditFlag (GraphEdit::Off);
  ResetEdit ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_serial") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  conf.SetupMemoryFiles (); // ystl::MemFile reads go through pfnLoadFileForMe

  // three nodes: plain links, a jump/velocity special and camp data
  ResetEdit ();
  const int s0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int s1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Camp));
  const int s2 = AddTestNode (ystl::Vector (0.0f, 100.0f, 0.0f), ystl::to_underlying (NodeFlag::Goal));
  graph.paths_[1].radius = 32.5f;
  graph.paths_[1].start = ystl::Vector (10.0f, 20.0f, 0.0f);
  graph.paths_[1].end = ystl::Vector (40.0f, 50.0f, 0.0f);
  LinkTestNodes (s0, s1, 100);
  LinkTestNodes (s0, s2, 100);
  LinkTestNodes (s1, s2, 150, ystl::to_underlying (PathFlag::Jump));
  graph.paths_[1].links[0].velocity = ystl::Vector (1.0f, 2.0f, 3.0f);
  LinkTestNodes (s2, s0, 100);

  // export contract: scalars and blocks read back as written
  ystl::ConfNode doc {};
  graph.ExportNode (doc, graph.paths_[0]);

  const ystl::ConfNode *block = doc.find ("0");
  HOST_REQUIRE (block != nullptr);
  HOST_REQUIRE (block->find ("origin") != nullptr);
  CHECK (block->find ("origin")->value () == "0.00 0.00 0.00");
  CHECK (block->find ("links")->value () == "1, 2");
  CHECK (block->find ("flags") == nullptr); // no flags on node 0

  ystl::ConfNode doc1 {};
  graph.ExportNode (doc1, graph.paths_[1]);
  const ystl::ConfNode *camp = doc1.find ("1");
  HOST_REQUIRE (camp != nullptr);
  CHECK (camp->find ("radius")->value () == "32.5");
  CHECK (camp->find ("flags")->value () == "Camp");
  CHECK (camp->find ("camp")->value () == "10.00 20.00 40.00 50.00");

  bool saw_jump = false;
  for (const auto *entry : camp->children ()) {
    if (entry->name () == "link") {
      HOST_REQUIRE (entry->find ("to") != nullptr);
      if (entry->find ("to")->value () == "2") {
        saw_jump = true;
        CHECK (entry->find ("distance")->value () == "150");
        CHECK (entry->find ("flags")->value () == "Jump");
        CHECK (entry->find ("velocity")->value () == "1.00 2.00 3.00");
      }
    }
  }
  CHECK (saw_jump);

  // full text roundtrip through a real file
  ystl::ConfNode roundtrip {};
  auto &meta = roundtrip.add_block ("Meta");
  meta.add_scalar ("nodes", "3");
  auto &nodes = roundtrip.add_block ("Nodes");
  graph.ExportNode (nodes, graph.paths_[0]);
  graph.ExportNode (nodes, graph.paths_[1]);
  graph.ExportNode (nodes, graph.paths_[2]);

  const ystl::String text = ystl::ConfWriter::write (roundtrip);

  ystl::File::make_path ("testdata-graph");
  const char *tmp_name = "testdata-graph/roundtrip.graph.txt";
  {
    ystl::File out {};
    HOST_REQUIRE (out.open (tmp_name, "wb"));
    out.write (text.chars (), text.size ());
    out.close ();
  }
  HOST_REQUIRE (graph.ImportGraphText (tmp_name));
  ::remove (tmp_name);

  HOST_REQUIRE (graph.Length () == 3);

  for (int i = 0; i < 3; ++i) {
    const auto &path = graph.paths_[i];
    CHECK (path.number == i);
  }
  CHECK (graph.paths_[0].origin.length () < 0.02f);
  CHECK ((graph.paths_[1].origin - ystl::Vector (100.0f, 0.0f, 0.0f)).length () < 0.02f);
  CHECK (graph.paths_[1].radius == 32.5f);
  CHECK (has_flag (graph.paths_[1].flags, NodeFlag::Camp));
  CHECK (has_flag (graph.paths_[2].flags, NodeFlag::Goal));
  CHECK (graph.IsConnected (0, 1) && graph.IsConnected (0, 2));
  CHECK (graph.IsConnected (1, 2) && graph.IsConnected (2, 0));
  CHECK (graph.paths_[1].light == kInvalidLightLevel); // absent light stays unknown

  // camp angles survive with format precision (z is not stored)
  CHECK ((graph.paths_[1].start - ystl::Vector (10.0f, 20.0f, 0.0f)).length () < 0.02f);
  CHECK ((graph.paths_[1].end - ystl::Vector (40.0f, 50.0f, 0.0f)).length () < 0.02f);

  ResetEdit ();
  testhost::CloseFakeCs (cs);
}

} // namespace bot
