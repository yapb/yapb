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
  static bool HasFreeLinkSlot (const Graph &g, int index) {
    return g.HasFreeLinkSlot (index);
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

TEST_CASE ("unit/graph_gaps") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  edict_t *editor = engine.SpawnClient ("Editor");
  HOST_REQUIRE (editor != nullptr);
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.SetEditor (editor);

  // guard rails: invalid indices are silent no-ops
  graph.Reset ();
  CHECK (graph.ClearConnections (99) == 0);
  graph.Erase (99); // empty graph: nothing to erase
  graph.FlagJumpLink (-1, 0);
  graph.FlagJumpLink (0, 99);
  graph.FlagJumpLink (0, 0);
  CHECK (!GraphHook::HasFreeLinkSlot (graph, 99));
  CHECK (!graph.IsConnected (99, 0));
  CHECK (!graph.IsConnected (0, 99));

  // jump flagging: missing link is a no-op, present link gets flagged
  ResetEdit ();
  const int j0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int j1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  graph.Erase (99); // unknown index: nothing to erase
  graph.FlagJumpLink (j0, j1); // no link yet: nothing happens
  CHECK (!has_flag (graph.paths_[0].links[0].flags, PathFlag::Jump));
  LinkTestNodes (j0, j1, 100);
  graph.FlagJumpLink (j0, j1);
  CHECK (has_flag (graph.paths_[0].links[0].flags, PathFlag::Jump));
  CHECK (graph.paths_[0].radius == 0.0f);

  // link slots: fresh node has room, a full node has none
  CHECK (GraphHook::HasFreeLinkSlot (graph, j1));
  for (int i = 2; i < 10; ++i) {
    AddTestNode (ystl::Vector (static_cast<float> (200 + i * 100), 0.0f, 0.0f));
  }
  for (int i = 1; i < 9; ++i) {
    LinkTestNodes (j1, i, 100);
  }
  CHECK (!GraphHook::HasFreeLinkSlot (graph, j1));

  // path surgery without a nearest node or a destination
  ResetEdit ();
  graph.CachePoint (99); // empty graph: cache cleared with a message
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  graph.PathCreate (PathConnection::Outgoing); // nearest exists, destination does not
  graph.ErasePath (); // same hole on the removal side
  graph.Reset ();
  graph.SetEditFlag (GraphEdit::On);
  graph.ErasePath (); // no nearest node at all
  graph.ResetPath (99); // neither index nor editor vicinity resolve
  graph.PathCreate (PathConnection::Outgoing); // no nearest node

  // directional surgery through cache and facing slots
  const int s0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int s1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.CachePoint (s0);
  graph.PathCreate (PathConnection::Outgoing); // destination is the node itself
  graph.CachePoint (s1);
  graph.PathCreate (PathConnection::Jumping); // fresh jump link
  CHECK (has_flag (graph.paths_[s0].links[0].flags, PathFlag::Jump));
  graph.PathCreate (PathConnection::Jumping); // same jump again: denied
  graph.ResetPath (s1); // keep s1 clean for the no-path check below
  graph.CachePoint (s1);
  graph.ErasePath (); // nothing either way: reports no path
  LinkTestNodes (s1, s0, 100);
  graph.ErasePath (); // incoming direction is torn down instead
  CHECK (!graph.IsConnected (s1, s0));

  // facing detection: blocked trace skips the node, high node skips on pitch
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  CHECK (graph.GetFacingIndex () == kInvalidNodeIndex);
  engine.SetTraceLineHook (nullptr);

  // high node above the editor fails the pitch gate and is skipped
  ResetEdit ();
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  AddTestNode (ystl::Vector (100.0f, 0.0f, 200.0f));
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  CHECK (graph.GetFacingIndex () == kInvalidNodeIndex);

  // buckets: 180 nodes, ten of them sharing the origin bucket
  graph.Reset ();
  for (int i = 0; i < 170; ++i) {
    const int node = AddTestNode (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f), node);
  }
  for (int i = 170; i < 180; ++i) {
    const int node = AddTestNode (ystl::Vector (4.0f, 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (4.0f, 0.0f, 0.0f), node);
  }

  // wide range falls back to the linear scan even on big graphs
  const ystl::Vector probe (4.0f, 0.0f, 0.0f);
  CHECK (graph.GetNearest (probe, 300.0f) == graph.GetNearestNoBuckets (probe, 300.0f));

  // missing bucket falls back as well
  graph.InitBuckets ();
  CHECK (graph.GetNearest (probe) == graph.GetNearestNoBuckets (probe));

  // flag filter skips the whole bucket, then falls back and finds nothing
  graph.Reset ();
  for (int i = 0; i < 170; ++i) {
    const int node = AddTestNode (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f), node);
  }
  for (int i = 170; i < 180; ++i) {
    const int node = AddTestNode (ystl::Vector (4.0f, 0.0f, 0.0f));
    graph.AddToBucket (ystl::Vector (4.0f, 0.0f, 0.0f), node);
  }
  CHECK (graph.GetNearest (probe, kInfiniteDistance, NodeFlag::Goal) == kInvalidNodeIndex);

  // radius search inside a fat bucket honors the cap and skips far members
  const int far = AddTestNode (ystl::Vector (500.0f, 0.0f, 0.0f));
  graph.AddToBucket (ystl::Vector (4.0f, 0.0f, 0.0f), far);
  const auto capped = graph.GetNearestInRadius (100.0f, probe, 2);
  CHECK (capped.size () == 2);
  const auto all = graph.GetNearestInRadius (100.0f, probe, -1);
  CHECK (all.size () >= 10 && !ContainsNode (all, far));

  // bsp size reads a real file from the mod directory
  const ystl::String bsp_path =
    ystl::strings.join_path (game.GetRunningModName (), "maps", game.GetMapName ()) + ".bsp";
  const ystl::String maps_dir = ystl::strings.join_path (game.GetRunningModName (), "maps");
  ystl::File::make_path (maps_dir.chars ());
  {
    ystl::File bsp {};
    HOST_REQUIRE (bsp.open (bsp_path.chars (), "wb"));
    const char filler[8] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h' };
    bsp.write (filler, 8);
    bsp.close ();
  }
  CHECK (graph.GetBspSize () == 8);
  ::remove (bsp_path.chars ());

  // empty-graph reachability traversals return empty sets
  graph.Reset ();
  ystl::Array<bool> forward {}, backward {};
  graph.ComputeForwardReachability (forward);
  graph.ComputeBackwardReachability (backward);
  CHECK (forward.empty () && backward.empty ());

  // node sanity battery: every failure leg reports and returns false
  ResetEdit ();
  const int c0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int c1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int c2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));
  const int c3 = AddTestNode (ystl::Vector (300.0f, 0.0f, 0.0f));

  // healthy ring carrying every required point type
  LinkTestNodes (c0, c1, 100);
  LinkTestNodes (c1, c0, 100);
  LinkTestNodes (c1, c2, 100);
  LinkTestNodes (c2, c1, 100);
  LinkTestNodes (c2, c0, 200);
  LinkTestNodes (c0, c2, 200);
  LinkTestNodes (c0, c3, 100);
  LinkTestNodes (c3, c0, 100);
  graph.paths_[c0].flags |= ystl::to_underlying (NodeFlag::TerroristOnly);
  graph.paths_[c1].flags |= ystl::to_underlying (NodeFlag::CTOnly);
  graph.paths_[c2].flags |= ystl::to_underlying (NodeFlag::Goal);
  graph.paths_[c3].flags |= ystl::to_underlying (NodeFlag::Rescue);
  bool ok = graph.CheckNodes (false, false);
  CHECK (ok);

  // number mismatch on an otherwise healthy graph
  graph.paths_[c2].number = 9;
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);
  graph.paths_[c2].number = 2;

  // invalid outgoing link
  graph.paths_[c0].links[0].index = 99;
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);
  graph.paths_[c0].links[0].index = 1;

  // isolated node with no links at all
  graph.ResetPath (c3);
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);
  LinkTestNodes (c0, c3, 100);
  LinkTestNodes (c3, c0, 100);

  // camp node without an end position
  graph.paths_[c0].flags |= ystl::to_underlying (NodeFlag::Camp);
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);
  graph.paths_[c0].flags &= ~ystl::to_underlying (NodeFlag::Camp);

  // self link, teleporting the editor to the defect
  graph.paths_[c0].links[2].index = 0;
  ok = graph.CheckNodes (true, false);
  CHECK (!ok);
  graph.paths_[c0].links[2].index = kInvalidNodeIndex;

  // out of range link, teleporting the editor to the defect
  graph.paths_[c0].links[2].index = 99;
  ok = graph.CheckNodes (true, false);
  CHECK (!ok);
  graph.paths_[c0].links[2].index = kInvalidNodeIndex;

  // missing point types gate the counts one by one
  graph.paths_[c0].flags &= ~ystl::to_underlying (NodeFlag::TerroristOnly);
  graph.paths_[c1].flags &= ~ystl::to_underlying (NodeFlag::CTOnly);
  graph.paths_[c2].flags &= ~ystl::to_underlying (NodeFlag::Goal);
  graph.paths_[c3].flags &= ~ystl::to_underlying (NodeFlag::Rescue);
  ok = graph.CheckNodes (false, false); // no terrorist points
  CHECK (!ok);
  graph.paths_[c0].flags |= ystl::to_underlying (NodeFlag::TerroristOnly);
  ok = graph.CheckNodes (false, false); // no ct points
  CHECK (!ok);
  graph.paths_[c1].flags |= ystl::to_underlying (NodeFlag::CTOnly);
  ok = graph.CheckNodes (false, false); // no goal points
  CHECK (!ok);
  graph.paths_[c2].flags |= ystl::to_underlying (NodeFlag::Goal);
  graph.paths_[c3].flags |= ystl::to_underlying (NodeFlag::Rescue);

  // forward failure: node 3 points at 0 but stays out of the reachable set
  graph.ResetPath (c3);
  LinkTestNodes (c3, c0, 100);
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);

  // backward failure: node 3 is reachable from 0 but never returns
  graph.ResetPath (c3);
  LinkTestNodes (c0, c3, 100);
  ok = graph.CheckNodes (false, false);
  CHECK (!ok);

  // healthy loop passes, in both verbosity modes
  LinkTestNodes (c3, c0, 100);
  ok = graph.CheckNodes (false, false);
  CHECK (ok);
  ok = graph.CheckNodes (false, true);
  CHECK (ok);

  // quiet mode skips messages and teleports on the same defects
  graph.paths_[c2].number = 9;
  ok = graph.CheckNodes (false, true);
  CHECK (!ok);
  graph.paths_[c2].number = 2;
  graph.ResetPath (c3);
  LinkTestNodes (c3, c0, 100);
  ok = graph.CheckNodes (false, true);
  CHECK (!ok);

  // the node store caps at the compile-time limit
  graph.Reset ();
  graph.SetEditFlag (GraphEdit::On);

  for (int i = 0; i < 4096; ++i) {
    AddTestNode (ystl::Vector (static_cast<float> (i * 10), 0.0f, 0.0f));
  }
  CHECK (graph.Length () == 4096);
  editor->v.origin = ystl::Vector (100000.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (100000.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 4096);

  graph.SetEditor (nullptr);
  graph.SetEditFlag (GraphEdit::Off);
  graph.Reset ();
  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_urls") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  game.Precache ();

  // alias expansion: trims quotes, maps known aliases, rejects unknown ones
  CHECK (graph_urls.Expand ("").empty ());
  const ystl::String github_expanded = graph_urls.Expand ("  \"@github/graph/de_test.graph\"  ");
  CHECK (github_expanded == "https://raw.githubusercontent.com/yapb/graph/refs/heads/master/graph/de_test.graph");
  CHECK (graph_urls.Expand ("@ru/maps") == "https://d5d5lttn6kg3uqou32ei.g4vq2kuy.apigw.yandexcloud.net/maps");
  CHECK (graph_urls.Expand ("@russia") == "https://d5d5lttn6kg3uqou32ei.g4vq2kuy.apigw.yandexcloud.net");
  CHECK (graph_urls.Expand ("@workers/x") == "https://graph-worker.yapb.workers.dev/x");
  CHECK (graph_urls.Expand ("@http/y") == "http://yapb.jeefo.net/y");
  CHECK (graph_urls.Expand ("@legacy") == "http://yapb.jeefo.net");
  CHECK (graph_urls.Expand ("@jeefo/z") == "http://yapb.jeefo.net/z");
  CHECK (graph_urls.Expand ("@nope/x").empty ());
  CHECK (graph_urls.Expand ("example.com/x") == "http://example.com/x");
  CHECK (graph_urls.Expand ("http://example.com/x") == "http://example.com/x");
  CHECK (graph_urls.Expand ("https://example.com/x") == "https://example.com/x");

  // download base: empty disables, first https use warns once without tls
  cv_graph_url.Set ("");
  CHECK (graph_urls.DownloadBase ().empty ());
  CHECK (graph_urls.DownloadUrl ("de_test").empty ());
  CHECK (!graph_urls.CanDownload ());
  CHECK (!graph.CanDownload ());

  // the tls warning is a one-shot: count server prints carrying it
  auto tls_warns = [&] () {
    size_t count = 0;

    for (size_t i = 0; i < engine.Calls ().size (); ++i) {
      if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), "no TLS") != nullptr) {
        ++count;
      }
    }
    return count;
  };

  const size_t warns_before = tls_warns ();
  cv_graph_url.Set ("@github");
  const ystl::String base = graph_urls.DownloadBase ();
  CHECK (base == "https://raw.githubusercontent.com/yapb/graph/refs/heads/master");
  CHECK (tls_warns () == warns_before + 1); // first https use warns once
  graph_urls.DownloadBase ();
  CHECK (tls_warns () == warns_before + 1); // second use stays silent

  // russia swaps the download host, plain hosts stay untouched
  cv_graph_url.Set ("@russia");
  CHECK (graph_urls.DownloadBase () == "https://sourcecraft.dev/api/file/yapb/graph");
  CHECK (graph_urls.CanDownload ());
  CHECK (graph.CanDownload ());

  // unknown aliases resolve empty through the same entry points
  cv_graph_url.Set ("@nope");
  CHECK (graph_urls.DownloadBase ().empty ());
  CHECK (graph_urls.DownloadUrl ("de_test").empty ());

  // download url lowercases the map name and appends the graph path
  cv_graph_url.Set ("@github");
  const ystl::String dl = graph_urls.DownloadUrl ("DE_TEST");
  CHECK (dl == "https://raw.githubusercontent.com/yapb/graph/refs/heads/master/graph/de_test.graph");

  // upload base: empty disables, workers/russia fall back to plain http here
  cv_graph_url_upload.Set ("");
  CHECK (graph_urls.UploadBase ().empty ());
  CHECK (graph_urls.UploadUrl ().empty ());
  cv_graph_url_upload.Set ("@workers");
  CHECK (graph_urls.UploadBase () == "http://yapb.jeefo.net/upload");
  CHECK (graph.CanDownload ());
  cv_graph_url_upload.Set ("@russia");
  CHECK (graph_urls.UploadBase () == "http://yapb.jeefo.net/upload");
  cv_graph_url_upload.Set ("http://example.com/up/");
  CHECK (graph_urls.UploadBase () == "http://example.com/up/");
  CHECK (graph_urls.UploadUrl () == "http://example.com/up");
  cv_graph_url_upload.Set ("https://example.com/up");
  CHECK (graph_urls.UploadBase () == "https://example.com/up");

  // collect needs the worker over https: unavailable in this build
  CHECK (graph_urls.CollectUrl ("de_test").empty ());
  CHECK (!graph_urls.CanCollect ());

  cv_graph_url.Set ("@github");
  cv_graph_url_upload.Set ("@workers");

  bots.Destroy ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_walls") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  game.Precache ();

  // worldspawn grounds the null-entity checks behind the entity lookups
  edict_t *world = engine.SpawnEntity ("worldspawn");
  HOST_REQUIRE (world != nullptr);
  game.OnSpawnEntity (world);

  // no walls probed yet: everything is clear
  mode_walls.Reset ();
  CHECK (!mode_walls.HasWalls ());
  CHECK (!mode_walls.IsNodeBlocked (ystl::Vector (0.0f, 0.0f, 0.0f)));
  CHECK (!mode_walls.IsSegmentBlocked (ystl::Vector (0.0f, 0.0f, 0.0f), ystl::Vector (100.0f, 0.0f, 0.0f)));

  // round start without the plugin entity leaves the probe cold
  mode_walls.OnRoundStart ();
  CHECK (!mode_walls.HasWalls ());

  // plugin wall appears: open bbox, inflated by the bot hull
  edict_t *wall = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (wall != nullptr);
  wall->v.solid = SOLID_BBOX;
  wall->v.absmin = ystl::Vector (0.0f, 0.0f, 0.0f);
  wall->v.absmax = ystl::Vector (64.0f, 64.0f, 64.0f);

  mode_walls.OnRoundStart ();
  CHECK (mode_walls.HasWalls ());
  CHECK (mode_walls.IsNodeBlocked (ystl::Vector (32.0f, 32.0f, 32.0f)));
  CHECK (!mode_walls.IsNodeBlocked (ystl::Vector (1000.0f, 0.0f, 0.0f)));
  CHECK (mode_walls.IsSegmentBlocked (ystl::Vector (-100.0f, 32.0f, 32.0f), ystl::Vector (200.0f, 32.0f, 32.0f)));
  CHECK (mode_walls.IsSegmentBlocked (ystl::Vector (200.0f, 32.0f, 32.0f), ystl::Vector (-100.0f, 32.0f, 32.0f)));
  CHECK (!mode_walls.IsSegmentBlocked (ystl::Vector (1000.0f, 0.0f, 0.0f), ystl::Vector (2000.0f, 0.0f, 0.0f)));
  CHECK (!mode_walls.IsSegmentBlocked (ystl::Vector (0.0f, 1000.0f, 0.0f), ystl::Vector (100.0f, 1000.0f, 0.0f)));

  // frame inside the scan period is a no-op, later frames reuse the shadow
  mode_walls.Frame ();
  CHECK (mode_walls.HasWalls ());
  engine.AdvanceTime (1.5f);
  mode_walls.Frame ();
  CHECK (mode_walls.HasWalls ());

  // moved wall republishes the set
  wall->v.absmax = ystl::Vector (128.0f, 64.0f, 64.0f);
  engine.AdvanceTime (1.5f);
  mode_walls.Frame ();
  CHECK (mode_walls.IsNodeBlocked (ystl::Vector (100.0f, 32.0f, 32.0f)));

  // closed (non-solid) plugin entity is skipped by the scan
  edict_t *closed = engine.SpawnEntity ("test_effect");
  HOST_REQUIRE (closed != nullptr);
  closed->v.solid = SOLID_NOT;
  engine.AdvanceTime (1.5f);
  mode_walls.Frame ();
  CHECK (mode_walls.HasWalls ());

  // more walls than slots: the scan caps at the limit
  for (int i = 0; i < 65; ++i) {
    edict_t *extra = engine.SpawnEntity ("test_effect");
    HOST_REQUIRE (extra != nullptr);
    extra->v.solid = SOLID_BBOX;
    extra->v.absmin = ystl::Vector (static_cast<float> (1000 + i * 10), 0.0f, 0.0f);
    extra->v.absmax = ystl::Vector (static_cast<float> (1005 + i * 10), 0.0f, 0.0f);
  }
  engine.AdvanceTime (1.5f);
  mode_walls.Frame ();
  CHECK (mode_walls.HasWalls ());

  mode_walls.Reset ();
  CHECK (!mode_walls.HasWalls ());

  bots.Destroy ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_build") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  engine.SetDedicatedServer (false); // listen server: recovered graphs carry a mark
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  edict_t *editor = engine.SpawnClient ("Editor");
  HOST_REQUIRE (editor != nullptr);
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.SetEditor (editor);

  // drop leftovers from aborted runs, every leg below is hermetic either way
  ::remove (bstor.BuildPath (StorageFile::Graph).chars ());
  ::remove (bstor.BuildPath (StorageFile::Graph, true).chars ());

  // online collection guards: disabled, then collecting, then once-only
  cv_graph_auto_collect_db.Set (0.0f);
  graph.CollectOnline ();
  cv_graph_auto_collect_db.Set (1.0f);
  graph.CollectOnline ();
  graph.CollectOnline ();
  graph.SyncCollectOnline (); // nothing stored yet: early return

  // node creation through the analyzer: dedup, auto-links, jump topology
  ResetEdit ();
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  analyzer.Start ();
  graph.SetEditor (nullptr);
  graph.Add (NodeAddFlag::Normal); // no editor to place from: refused
  CHECK (graph.Length () == 1);
  graph.SetEditor (editor);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (10.0f, 0.0f, 0.0f)); // too close: denied
  CHECK (graph.Length () == 1);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 2);
  CHECK (graph.IsConnected (0, 1) && graph.IsConnected (1, 0));

  // zero step height turns plain links into flagged jump links
  sv_stepsize.Set (0);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (200.0f, 0.0f, 0.0f));
  sv_stepsize.Set (18);
  HOST_REQUIRE (graph.Length () == 3);
  CHECK (graph.IsConnected (1, 2));
  CHECK (has_flag (graph.paths_[2].links[0].flags, PathFlag::Jump));
  CHECK (graph.paths_[2].radius == 0.0f);

  // jump topology over a blocked sightline: drop links first, jump arcs next
  const int td = AddTestNode (ystl::Vector (48.0f, 0.0f, 0.0f));
  const int tj = AddTestNode (ystl::Vector (80.0f, 0.0f, 40.0f));
  (void)td;
  (void)tj;

  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.0f;
  });
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 100.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (0.0f, 0.0f, 36.0f));
  editor->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (0.0f, 30.0f, 0.0f));
  engine.SetTraceLineHook (nullptr);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  // the blocked pair grew drop and jump links instead of plain ones
  CHECK (graph.IsConnected (5, 3));
  bool saw_jump = false;

  for (const auto &link : graph.paths_[6].links) {
    saw_jump = saw_jump || (link.index != kInvalidNodeIndex && has_flag (link.flags, PathFlag::Jump));
  }
  CHECK (saw_jump);

  // camp re-creation next to an existing camp node only refreshes it
  ResetEdit ();
  editor->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Camp, ystl::Vector (500.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 1);
  graph.Add (NodeAddFlag::Camp, ystl::Vector (505.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 1);

  // jump-start next to an existing node reuses it, far away it learns a spot
  const int jn = AddTestNode (ystl::Vector (1000.0f, 0.0f, 0.0f));
  graph.StartLearnJump ();
  editor->v.origin = ystl::Vector (900.0f, 0.0f, 0.0f);
  editor->v.velocity = ystl::Vector (10.0f, 0.0f, 0.0f);
  editor->v.button = 0;
  graph.Frame ();
  editor->v.origin = ystl::Vector (1005.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpStart, ystl::Vector (1005.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 2);
  editor->v.origin = ystl::Vector (3000.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpStart, ystl::Vector (3000.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 3);
  editor->v.origin = ystl::Vector (3100.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpEnd, ystl::Vector (3100.0f, 0.0f, 0.0f));
  HOST_REQUIRE (graph.Length () == 4);
  CHECK (graph.IsConnected (2, 3));

  // jump-end next to a connected node keeps its links, next to a bare one adjusts it
  const int je = AddTestNode (ystl::Vector (2000.0f, 0.0f, 0.0f));
  const int je2 = AddTestNode (ystl::Vector (2050.0f, 0.0f, 0.0f));
  LinkTestNodes (je, je2, 50);
  editor->v.origin = ystl::Vector (2005.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpEnd, ystl::Vector (2005.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 6);
  const int jb = AddTestNode (ystl::Vector (2100.0f, 0.0f, 0.0f));
  editor->v.origin = ystl::Vector (2105.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::JumpEnd, ystl::Vector (2105.0f, 0.0f, 0.0f));
  CHECK (graph.Length () == 7);
  (void)jn;
  (void)jb;

  // ladder nodes wire to nearby ladders and to the closest plain node
  editor->v.movetype = MOVETYPE_FLY;
  graph.Add (NodeAddFlag::Normal, ystl::Vector (4000.0f, 0.0f, 50.0f));
  HOST_REQUIRE (graph.Length () == 8);
  CHECK (has_flag (graph.paths_[7].flags, NodeFlag::Ladder));
  graph.Add (NodeAddFlag::Normal, ystl::Vector (4050.0f, 0.0f, 10.0f));
  HOST_REQUIRE (graph.Length () == 9);
  CHECK (graph.IsConnected (8, 7) && graph.IsConnected (7, 8));
  editor->v.movetype = MOVETYPE_WALK;

  analyzer.Start ();
  CHECK (analyzer.IsAnalyzing ());
  editor->v.origin = ystl::Vector (4150.0f, 0.0f, 20.0f);
  editor->v.movetype = MOVETYPE_FLY;
  graph.Add (NodeAddFlag::Normal, ystl::Vector (4150.0f, 0.0f, 20.0f));
  HOST_REQUIRE (graph.Length () == 10);
  editor->v.movetype = MOVETYPE_WALK;
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  // autosave triggers on the node counter through the real save path
  const ystl::String gpath = bstor.BuildPath (StorageFile::Graph);
  ystl::String gdir = gpath.substr (0, gpath.find_last_of (kPathSeparator));
  ystl::File::make_path (gdir.chars ());

  // the loader reads through the game filesystem, mirror the saved file over
  auto mirror_graph = [&] () {
    const ystl::String from = bstor.BuildPath (StorageFile::Graph);
    const ystl::String to = bstor.BuildPath (StorageFile::Graph, true);
    ystl::String to_dir = to.substr (0, to.find_last_of (kPathSeparator));
    ystl::File::make_path (to_dir.chars ());

    ystl::File src (from.chars (), "rb");
    ystl::File dst (to.chars (), "wb");

    if (!src || !dst) {
      return false;
    }

    for (int ch = src.get (); ch != EOF; ch = src.get ()) {
      dst.put_char (ch);
    }
    return true;
  };

  cv_graph_auto_save_count.Set (1);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (5000.0f, 0.0f, 0.0f));
  cv_graph_auto_save_count.Set (15);
  CHECK (graph.Length () == 11);

  // missing binary falls back to the analyzer without a graph
  const ystl::String vfs_graph = bstor.BuildPath (StorageFile::Graph, true);
  ::remove (vfs_graph.chars ());
  bool missing = graph.LoadGraphData ();
  CHECK (!missing);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  // starting twice in a row is a silent no-op
  analyzer.Start ();
  analyzer.Start ();
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  // a shipped text graph is announced before the same fallback
  const ystl::String text_probe = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.data, folders.graph,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
  ystl::String text_dir = text_probe.substr (0, text_probe.find_last_of (kPathSeparator));
  ystl::File::make_path (text_dir.chars ());
  {
    ystl::File hint {};
    HOST_REQUIRE (hint.open (text_probe.chars (), "wb"));
    hint.write ("hello", 5);
    hint.close ();
  }
  missing = graph.LoadGraphData ();
  CHECK (!missing);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);
  ::remove (text_probe.chars ());

  // the fallback legs wiped the graph: rebuild, leaving an island behind
  ResetEdit ();
  for (int i = 0; i < 9; ++i) {
    AddTestNode (ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f));
  }

  for (int i = 0; i < 8; ++i) {
    LinkTestNodes (i, i + 1, 100);
    LinkTestNodes (i + 1, i, 100);
  }
  AddTestNode (ystl::Vector (5000.0f, 0.0f, 0.0f));

  // saving variations: product author, named author, dedicated and listen
  graph.SetEditor (nullptr);
  graph.info_.author.clear ();
  bool saved = graph.SaveGraphData ();
  CHECK (saved);

  // mismatched bsp size warns about a foreign graph
  const ystl::String warn_bsp = ystl::strings.join_path (game.GetRunningModName (), "maps", game.GetMapName ()) + ".bsp";
  ystl::String warn_dir = ystl::strings.join_path (game.GetRunningModName (), "maps");
  ystl::File::make_path (warn_dir.chars ());
  {
    ystl::File bsp {};
    HOST_REQUIRE (bsp.open (warn_bsp.chars (), "wb"));
    const char small[10] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j' };
    bsp.write (small, 10);
    bsp.close ();
  }
  saved = graph.SaveGraphData ();
  CHECK (saved);
  bool warn_mirrored = mirror_graph ();
  CHECK (warn_mirrored);
  {
    ystl::File bsp {};
    HOST_REQUIRE (bsp.open (warn_bsp.chars (), "wb"));
    const char big[20] = { 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't' };
    bsp.write (big, 20);
    bsp.close ();
  }
  bool warn_loaded = graph.LoadGraphData ();
  CHECK (warn_loaded);
  bool mirrored = mirror_graph ();
  CHECK (mirrored);
  bool loaded = graph.LoadGraphData ();
  CHECK (loaded);
  CHECK (graph.info_.author == product.name);

  graph.info_.author.assign ("Tester");
  saved = graph.SaveGraphData ();
  CHECK (saved);
  mirrored = mirror_graph ();
  CHECK (mirrored);

  graph.SetEditor (editor);
  editor->v.netname = string_t (engine.AllocString ("Editor"));
  graph.info_.author.assign ("A");
  ystl::strings.copy (graph.info_.exten.author, "B", ystl::bufsize (graph.info_.exten.author));
  saved = graph.SaveGraphData ();
  CHECK (saved);
  mirrored = mirror_graph ();
  CHECK (mirrored);
  loaded = graph.LoadGraphData ();
  CHECK (loaded);

  // convert the legacy podbot format, including its failure legs
  const ystl::String pwf_path = bstor.BuildPath (StorageFile::PodbotPWF, true);
  ystl::String pwf_dir = pwf_path.substr (0, pwf_path.find_last_of (kPathSeparator));
  ystl::File::make_path (pwf_dir.chars ());

  auto write_pwf = [&] (const char *magic, int32_t version, const char *map, int32_t count, int bad_number) {
    ystl::File fp {};
    HOST_REQUIRE (fp.open (pwf_path.chars (), "wb"));

    LegacyHeader header {};
    ystl::strings.copy (header.header, magic, sizeof (header.header));
    ystl::strings.copy (header.map_name, map, ystl::bufsize (header.map_name));
    ystl::strings.copy (header.author, "Tester", ystl::bufsize (header.author));
    header.file_version = version;
    header.point_number = count;
    fp.le_write (header);

    for (int i = 0; i < count; ++i) {
      LegacyPath pod {};
      pod.number = (bad_number != kInvalidNodeIndex && i == 0) ? bad_number : i;
      pod.origin = ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f);
      pod.index[0] = kInvalidNodeIndex;
      pod.index[1] = kInvalidNodeIndex;
      pod.index[2] = kInvalidNodeIndex;
      pod.index[3] = kInvalidNodeIndex;
      pod.index[4] = kInvalidNodeIndex;
      pod.index[5] = kInvalidNodeIndex;
      pod.index[6] = kInvalidNodeIndex;
      pod.index[7] = kInvalidNodeIndex;
      fp.le_write (pod);
    }
    fp.close ();
  };

  write_pwf ("BADMAGIC", 7, "de_test", 1, kInvalidNodeIndex);
  CHECK (!graph.ConvertOldFormat ());
  write_pwf (kPodbotMagic, 8, "de_test", 1, kInvalidNodeIndex);
  CHECK (!graph.ConvertOldFormat ());
  write_pwf (kPodbotMagic, 7, "de_dust", 1, kInvalidNodeIndex);
  CHECK (!graph.ConvertOldFormat ());
  write_pwf (kPodbotMagic, 7, "de_test", 0, kInvalidNodeIndex);
  CHECK (!graph.ConvertOldFormat ());
  write_pwf (kPodbotMagic, 7, "de_test", 1, 99);
  CHECK (!graph.ConvertOldFormat ());

  // truncated body fails the pod read
  {
    ystl::File fp {};
    HOST_REQUIRE (fp.open (pwf_path.chars (), "wb"));

    LegacyHeader header {};
    ystl::strings.copy (header.header, kPodbotMagic, sizeof (header.header));
    ystl::strings.copy (header.map_name, "de_test", ystl::bufsize (header.map_name));
    header.file_version = ystl::to_underlying (StorageVersion::Podbot);
    header.point_number = 1;
    fp.le_write (header);
    fp.close ();
  }
  CHECK (!graph.ConvertOldFormat ());

  // truncated header fails the header read
  {
    ystl::File fp {};
    HOST_REQUIRE (fp.open (pwf_path.chars (), "wb"));
    const char stub[10] = { 'P', 'O', 'D', 'W', 'A', 'Y', '!', '!', '!', '!' };
    fp.write (stub, 10);
    fp.close ();
  }
  CHECK (!graph.ConvertOldFormat ());

  // full legacy graph converts and saves through the new format
  write_pwf (kPodbotMagic, ystl::to_underlying (StorageVersion::Podbot), "de_test", 8, kInvalidNodeIndex);
  graph.SetEditor (nullptr);
  bool converted = graph.ConvertOldFormat ();
  CHECK (converted);
  CHECK (graph.Length () == 8);
  CHECK (graph.info_.author == "Tester");
  graph.SetEditor (editor);
  ::remove (pwf_path.chars ());

  // legacy save honors editor, author and fallback naming
  const ystl::String pwf_disk = bstor.BuildPath (StorageFile::PodbotPWF);
  ::remove (pwf_disk.chars ());
  ystl::File::make_path (pwf_disk.chars ()); // a directory blocks the write
  graph.SaveOldFormat ();
  CHECK (ystl::plat.remove_directory (pwf_disk.chars ()));
  ystl::String pwf_disk_dir = pwf_disk.substr (0, pwf_disk.find_last_of (kPathSeparator));
  ystl::File::make_path (pwf_disk_dir.chars ());
  graph.SaveOldFormat ();
  graph.SetEditor (nullptr);
  graph.info_.author.assign ("Tester");
  graph.SaveOldFormat ();
  graph.info_.author.clear ();
  graph.SaveOldFormat ();
  graph.SetEditor (editor);

  // light levels: empty, unplayed, played and cached server
  graph.Reset ();
  graph.InitLightLevels (); // no nodes: silent return
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  graph.InitLightLevels (); // no players yet: silent return
  clients[0].ent = editor;
  clients[0].flags |= ClientFlags::Used;
  clients[0].team2 = Team::Terrorist;
  graph.InitLightLevels (); // a player is on: refreshes inline
  graph.SyncInitLightLevels (); // direct refresh works the same way
  clients[0].ent = nullptr;
  clients[0].flags = static_cast<ClientFlags> (0);
  graph.InitLightLevels (); // checked already: silent return

  // narrow places: crowded nodes bail out early, airy ones get measured
  ResetEdit ();
  const int w0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));

  for (int i = 1; i <= 6; ++i) {
    const int w = AddTestNode (ystl::Vector (static_cast<float> (i * 100), 0.0f, 0.0f));
    LinkTestNodes (w0, w, 100);
  }
  const int w7 = AddTestNode (ystl::Vector (1000.0f, 0.0f, 0.0f));
  const int w8 = AddTestNode (ystl::Vector (1100.0f, 0.0f, 0.0f));
  LinkTestNodes (w7, w8, 100);
  LinkTestNodes (w8, w7, 100);

  // walled in on all sides: every direction reports back, the node narrows
  editor->v.velocity.clear ();
  engine.AdvanceTime (1.0f); // expire trace cache entries from the saves above
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = game.GetStartEntity (); // every ray stops at the world
  });
  graph.InitNarrowPlaces ();
  CHECK (has_flag (graph.paths_[w7].flags, NodeFlag::Narrow));
  engine.SetTraceLineHook (nullptr);
  (void)w8;

  // versioned graphs skip, fresh ones scan
  graph.info_.header.version = 2;
  graph.ClearEditFlag (GraphEdit::On);
  graph.InitNarrowPlaces ();
  graph.SetEditFlag (GraphEdit::On);
  graph.info_.header.version = 0;
  graph.InitNarrowPlaces ();

  // versioned graphs skip, fresh ones scan
  graph.info_.header.version = 2;
  graph.ClearEditFlag (GraphEdit::On);
  graph.InitNarrowPlaces ();
  graph.SetEditFlag (GraphEdit::On);
  graph.info_.header.version = 0;
  graph.InitNarrowPlaces ();

  // basic nodes grow from map entities, twice for the nearby skips
  edict_t *ladder = engine.SpawnEntity ("func_ladder");
  HOST_REQUIRE (ladder != nullptr);
  ladder->v.absmin = ystl::Vector (0.0f, 0.0f, 0.0f);
  ladder->v.absmax = ystl::Vector (64.0f, 64.0f, 256.0f);

  const char *kinds[] = {
    "info_player_deathmatch", "info_player_start", "info_vip_start", "armoury_entity", "func_hostage_rescue", "info_hostage_rescue",
    "func_bomb_target", "info_bomb_target", "hostage_entity", "monster_scientist", "func_vip_safetyzone", "func_escapezone"
  };

  for (size_t i = 0; i < 12; ++i) {
    const float base = static_cast<float> (5000 + i * 1000);
    edict_t *first = engine.SpawnEntity (kinds[i]);
    HOST_REQUIRE (first != nullptr);
    first->v.origin = ystl::Vector (base, 0.0f, 0.0f);

    edict_t *second = engine.SpawnEntity (kinds[i]);
    HOST_REQUIRE (second != nullptr);
    second->v.origin = ystl::Vector (base, 0.0f, 0.0f);
  }
  // seeding through the analyzer wires ladders with jump topology
  analyzer.Start ();
  const int before_seed = graph.Length ();
  graph.SeedBasicNodes ();
  analyzer.Suspend ();
  graph.SetMessageSilence (false);
  CHECK (graph.Length () > before_seed);

  engine.SetPointContentsHook ([] (const float *v) {
    (void)v;
    return CONTENTS_SOLID;
  });
  const int before_second = graph.Length ();
  graph.SeedBasicNodes ();
  engine.SetPointContentsHook (nullptr);
  CHECK (graph.Length () >= before_second);

  // autosave without enough nodes reports the failure
  ResetEdit ();
  AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  cv_graph_auto_save_count.Set (1);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (200.0f, 0.0f, 0.0f));
  cv_graph_auto_save_count.Set (15);
  CHECK (graph.Length () == 3);

  // clear sightlines keep the fresh nodes wide
  graph.InitNarrowPlaces ();
  CHECK (!has_flag (graph.paths_[2].flags, NodeFlag::Narrow));

  // camp direction conversion is a no-op on an empty graph
  graph.Reset ();
  cv_graph_fixcamp.Set (1.0f);
  Path camp_path {};
  graph.ConvertCampDirection (camp_path);
  cv_graph_fixcamp.Set (0.0f);

  graph.SetEditor (nullptr);
  graph.SetEditFlag (GraphEdit::Off);
  graph.Reset ();
  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_import") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  conf.SetupMemoryFiles (); // ystl::MemFile reads go through pfnLoadFileForMe

  ystl::File::make_path ("testdata-graph");
  game.Precache ();

  // faulty documents are rejected with a message, the graph stays untouched
  auto import_doc = [&] (const char *name, const ystl::String &text) {
    ystl::File out {};

    if (!out.open (name, "wb")) {
      return false;
    }
    out.write (text.chars (), text.size ());
    out.close ();

    const bool ok = graph.ImportGraphText (name);
    ::remove (name);
    return ok;
  };

  graph.Reset ();
  CHECK (!graph.ImportGraphText ("testdata-graph/missing.graph.txt"));

  CHECK (!import_doc ("testdata-graph/bad-parse.graph.txt", "Meta {"));
  CHECK (graph.Length () == 0);

  CHECK (!import_doc ("testdata-graph/no-nodes.graph.txt", "Meta {\n nodes = 1\n}\n"));
  CHECK (!import_doc ("testdata-graph/no-meta.graph.txt", "Nodes {\n 0 {\n origin = 0.00 0.00 0.00\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-count.graph.txt", "Meta {\n nodes = 0\n}\nNodes {\n}\n"));
  CHECK (!import_doc ("testdata-graph/count-mismatch.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-label.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n x {\n origin = 0.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-order.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n"
    " 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/no-origin.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-origin.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n origin = 1 2\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-flag.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n flags = Frobnicate\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-camp.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n camp = 1 2 3\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-link.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n links = 99\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/many-links.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n links = 1, 1, 1, 1, 1, 1, 1, 1, 1\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-special.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n link {\n to = 99\n }\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-distance.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n link {\n to = 1\n distance = -5\n }\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-linkflag.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n link {\n to = 1\n flags = Fly\n }\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (!import_doc ("testdata-graph/bad-velocity.graph.txt",
    "Meta {\n nodes = 2\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n"
    " link {\n to = 1\n velocity = 1.00 2.00\n }\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 0.0\n }\n}\n"));
  CHECK (graph.Length () == 0);

  // rich document roundtrips flags, camp data, light and special links
  const bool rich = import_doc ("testdata-graph/rich.graph.txt",
    "Meta {\n nodes = 3\n author = Tester\n modified = Bob\n mapSize = 1234\n options = Analyzed, Official\n}\n"
    "Nodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n light = 200.0\n links = 1,, 2\n"
    " flags = TerroristOnly, Camp\n camp = 10.00 20.00 30.00 40.00\n }\n"
    " 1 {\n origin = 100.00 0.00 0.00\n radius = 32.5\n link {\n to = 2\n distance = 999\n flags = , Jump\n"
    " velocity = 1.00 2.00 3.00\n }\n }\n"
    " 2 {\n origin = 200.00 0.00 0.00\n radius = 0.0\n }\n}\n");
  CHECK (rich);
  HOST_REQUIRE (graph.Length () == 3);
  CHECK (has_flag (graph.paths_[0].flags, NodeFlag::TerroristOnly | NodeFlag::Camp));
  CHECK (graph.paths_[0].light == 200.0f);
  CHECK (graph.IsConnected (0, 1) && graph.IsConnected (0, 2));
  CHECK (graph.paths_[1].links[0].index == 2 && graph.paths_[1].links[0].distance == 999);
  CHECK (has_flag (graph.paths_[1].links[0].flags, PathFlag::Jump));
  CHECK (graph.info_.exten.map_size == 1234);
  CHECK (graph.info_.modified == "Bob");

  // minimal document falls back to product naming and skips optionals
  graph.Reset ();
  const bool minimal = import_doc ("testdata-graph/minimal.graph.txt",
    "Meta {\n nodes = 1\n}\nNodes {\n 0 {\n origin = 0.00 0.00 0.00\n radius = 0.0\n }\n}\n");
  CHECK (minimal);
  HOST_REQUIRE (graph.Length () == 1);
  CHECK (graph.info_.author == product.name);

  // text export: empty graph refuses, then option arms on both sides
  graph.Reset ();
  CHECK (!graph.ExportGraphText ());

  const int x0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Button | NodeFlag::Lift | NodeFlag::Crouch | NodeFlag::Crossing | NodeFlag::DoubleJump));
  const int x1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Camp | NodeFlag::Sniper));
  const int x2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Goal | NodeFlag::Rescue | NodeFlag::NoHostage | NodeFlag::TerroristOnly |
      NodeFlag::CTOnly | NodeFlag::Ladder | NodeFlag::Sniper));
  graph.paths_[0].light = 200.0f;
  graph.paths_[1].radius = 32.5f;
  graph.paths_[1].start = ystl::Vector (10.0f, 20.0f, 0.0f);
  graph.paths_[1].end = ystl::Vector (40.0f, 50.0f, 0.0f);
  LinkTestNodes (x0, x1, 999); // mismatched distance forces a special link entry
  LinkTestNodes (x0, x2, 200);
  (void)x1;
  (void)x2;

  graph.info_.author.clear ();
  graph.info_.modified.clear ();
  graph.info_.header.options = 0;
  graph.info_.exten.map_size = 0;
  ystl::String export_dir = ystl::strings.join_path (bstor.GetRunningPath (), folders.data, folders.graph);
  const ystl::String export_file = ystl::strings.join_path (export_dir,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));
  ::remove (export_file.chars ()); // drop leftovers from other runs
  ystl::File::make_path (export_file.chars ()); // a directory blocks the write
  CHECK (!graph.ExportGraphText ());
  CHECK (ystl::plat.remove_directory (export_file.chars ()));
  ystl::File::make_path (export_dir.chars ());
  CHECK (graph.ExportGraphText ());

  graph.info_.author.assign ("Tester");
  graph.info_.modified.assign ("Bob");
  graph.info_.exten.map_size = 123;
  graph.info_.header.options =
    ystl::to_underlying (StorageOption::Analyzed) | ystl::to_underlying (StorageOption::Official);
  CHECK (graph.ExportGraphText ());
  ::remove (export_file.chars ());

  // stats count every point type, file info prints without crashing
  graph.ShowStats ();
  graph.ShowFileInfo ();

  bool saw_stats = false;

  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    if (engine.Calls ()[i].name == "ServerPrint" && strstr (engine.Calls ()[i].detail.chars (), "Sniper Points:") != nullptr) {
      saw_stats = true;
    }
  }
  CHECK (saw_stats);

  graph.Reset ();
  bots.Destroy ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_frame") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  edict_t *editor = engine.SpawnClient ("Editor");
  HOST_REQUIRE (editor != nullptr);
  editor->v.origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  editor->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.flags |= FL_ONGROUND;
  graph.SetEditor (editor);
  graph.SetEditFlag (GraphEdit::On);

  // every node color and flag combination lives near the editor viewport
  const int f0 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Lift | NodeFlag::Crouch | NodeFlag::Camp | NodeFlag::TerroristOnly | NodeFlag::CTOnly |
      NodeFlag::Sniper | NodeFlag::Goal | NodeFlag::Ladder | NodeFlag::Rescue | NodeFlag::DoubleJump | NodeFlag::NoHostage));
  graph.paths_[0].radius = 40.0f;
  graph.paths_[0].light = 200.0f;
  const int f1 = AddTestNode (ystl::Vector (140.0f, 0.0f, 0.0f));
  const int f2 = AddTestNode (ystl::Vector (200.0f, 0.0f, 0.0f));
  const int f3 = AddTestNode (ystl::Vector (300.0f, 0.0f, 0.0f));
  const int f4 = AddTestNode (ystl::Vector (400.0f, 0.0f, 0.0f));
  LinkTestNodes (f0, f1, 40);
  graph.paths_[0].links[0].flags |= PathFlag::Jump; // jump takeoff off the hud node
  LinkTestNodes (f1, f0, 40);
  LinkTestNodes (f0, f2, 100);
  LinkTestNodes (f2, f0, 100);
  LinkTestNodes (f0, f3, 200);
  LinkTestNodes (f4, f0, 300); // one-way incoming into the hud node
  const int f5 = AddTestNode (ystl::Vector (320.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Camp | NodeFlag::Crouch));
  graph.paths_[5].start = ystl::Vector (10.0f, 20.0f, 0.0f);
  graph.paths_[5].end = ystl::Vector (30.0f, 40.0f, 0.0f);
  const int f6 = AddTestNode (ystl::Vector (330.0f, 0.0f, 0.0f)); // zero wayzone: cross marker
  AddTestNode (ystl::Vector (250.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Ladder));
  AddTestNode (ystl::Vector (260.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Rescue));
  AddTestNode (ystl::Vector (270.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Camp | NodeFlag::TerroristOnly));
  AddTestNode (ystl::Vector (280.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Camp | NodeFlag::CTOnly));
  AddTestNode (ystl::Vector (290.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::TerroristOnly));
  AddTestNode (ystl::Vector (310.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::CTOnly));
  AddTestNode (ystl::Vector (340.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::NoHostage));
  AddTestNode (ystl::Vector (350.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Lift));
  AddTestNode (ystl::Vector (255.0f, 0.0f, 0.0f), ystl::to_underlying (NodeFlag::Rescue));
  (void)f2;
  (void)f3;
  (void)f4;
  (void)f5;
  (void)f6;

  // practice data backs the danger arrows and the hud experience block
  graph.SetSearchIndex (f2);
  graph.CachePoint (f3);
  practice.Load ();
  practice.SetIndex (Team::Terrorist, f0, f0, f1);
  practice.SetIndex (Team::CT, f0, f0, f1);
  practice.SetIndex (Team::Terrorist, f1, f1, f0);
  practice.SetIndex (Team::CT, f1, f1, f0);

  // full draw, timer-gated skip, then redraw after the refresh period
  engine.AdvanceTime (2.0f);
  graph.Frame ();
  graph.Frame ();
  engine.AdvanceTime (1.0f);
  graph.Frame ();

  // changed graphs skip the experience block and show only the clock
  editor->v.origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (450.0f, 0.0f, 0.0f));
  graph.Frame ();

  // camp direction fans for crouching and standing camps
  editor->v.origin = ystl::Vector (315.0f, 0.0f, 0.0f);
  engine.AdvanceTime (1.0f);
  graph.Frame ();
  editor->v.origin = ystl::Vector (272.0f, 0.0f, 0.0f);
  engine.AdvanceTime (1.0f);
  graph.Frame ();

  // zero wayzone draws a cross instead of a circle
  editor->v.origin = ystl::Vector (328.0f, 0.0f, 0.0f);
  engine.AdvanceTime (1.0f);
  graph.Frame ();

  // clipping mode sticks through round restarts
  graph.SetEditFlag (GraphEdit::Noclip);
  graph.Frame ();
  CHECK (editor->v.movetype == MOVETYPE_NOCLIP);
  editor->v.movetype = MOVETYPE_WALK;

  // no nodes under the editor ends the frame early
  graph.Reset ();
  graph.Frame ();

  // jump learning runs start and end through the frame
  graph.StartLearnJump ();
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.button |= IN_JUMP;
  graph.Frame ();
  HOST_REQUIRE (graph.Length () == 1);
  editor->v.button &= ~IN_JUMP;
  editor->v.origin = ystl::Vector (500.0f, 0.0f, 0.0f);
  engine.AdvanceTime (0.2f);
  graph.Frame ();
  HOST_REQUIRE (graph.Length () == 2);
  CHECK (graph.IsConnected (0, 1));
  CHECK (has_flag (graph.paths_[0].links[0].flags, PathFlag::Jump));

  // automatic node placement: skipped near nodes, added far away
  graph.Reset ();
  graph.SetEditor (editor);
  graph.SetEditFlag (GraphEdit::On | GraphEdit::Auto);
  const int a0 = AddTestNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int a1 = AddTestNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  LinkTestNodes (a0, a1, 100);
  LinkTestNodes (a1, a0, 100);
  (void)a0;
  (void)a1;

  editor->v.origin = ystl::Vector (60.0f, 0.0f, 0.0f);
  editor->v.movetype = MOVETYPE_WALK;
  graph.Add (NodeAddFlag::Normal, ystl::Vector (60.0f, 0.0f, 0.0f)); // last auto node sits here
  editor->v.origin = ystl::Vector (61.0f, 0.0f, 0.0f);
  graph.Frame (); // reachable nodes nearby: nothing to do
  CHECK (graph.Length () == 3);

  editor->v.origin = ystl::Vector (5000.0f, 0.0f, 0.0f);
  graph.Add (NodeAddFlag::Normal, ystl::Vector (5000.0f, 0.0f, 0.0f));
  editor->v.origin = ystl::Vector (61.0f, 0.0f, 0.0f);
  graph.Frame (); // reachable nodes nearby: nothing to do
  CHECK (graph.Length () == 4);
  editor->v.origin = ystl::Vector (5200.0f, 0.0f, 0.0f);
  engine.AdvanceTime (1.0f);
  graph.Frame (); // nothing reachable out here: a node is placed
  CHECK (graph.Length () == 5);

  graph.SetEditor (nullptr);
  graph.SetEditFlag (GraphEdit::Off);
  graph.Reset ();
  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/graph_reach") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  testhost::InstallEngineTables (engine);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  analyzer.Suspend ();
  graph.SetMessageSilence (false);

  edict_t *editor = engine.SpawnClient ("Editor");
  HOST_REQUIRE (editor != nullptr);
  editor->v.origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  editor->v.view_ofs = ystl::Vector (0.0f, 0.0f, 28.0f);
  editor->v.v_angle = ystl::Vector (0.0f, 0.0f, 0.0f);
  graph.SetEditor (editor);
  graph.SetEditFlag (GraphEdit::On);

  // illusionary walls never take nodes, even on a clear sightline
  edict_t *illusion = engine.SpawnEntity ("func_illusionary");
  HOST_REQUIRE (illusion != nullptr);
  engine.SetTraceHullHook ([illusion] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 1.0f;
    out->pHit = illusion;
  });
  CHECK (!graph.IsNodeReacheable (ystl::Vector (1000.0f, 0.0f, 0.0f), ystl::Vector (1100.0f, 0.0f, 0.0f)));
  engine.SetTraceHullHook (nullptr);

  // doors pass behind themselves, blocked cavities do not
  edict_t *door = engine.SpawnEntity ("func_door");
  HOST_REQUIRE (door != nullptr);
  int line_calls = 0;
  engine.SetTraceLineHook ([door, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip,
    TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (line_calls++ == 0) {
      out->flFraction = 0.5f;
      out->pHit = door;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  CHECK (graph.IsNodeReacheable (ystl::Vector (2000.0f, 0.0f, 0.0f), ystl::Vector (2100.0f, 0.0f, 0.0f)));
  line_calls = 0;
  engine.SetTraceLineHook ([door, &line_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip,
    TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (line_calls++ == 0) {
      out->flFraction = 0.5f;
      out->pHit = door;
    }
    else {
      out->flFraction = 0.0f;
    }
  });
  CHECK (!graph.IsNodeReacheable (ystl::Vector (2200.0f, 0.0f, 0.0f), ystl::Vector (2300.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);

  // water to water is always reachable
  engine.SetPointContentsHook ([] (const float *v) {
    (void)v;
    return CONTENTS_WATER;
  });
  CHECK (graph.IsNodeReacheable (ystl::Vector (3000.0f, 0.0f, 0.0f), ystl::Vector (3100.0f, 0.0f, 0.0f)));
  engine.SetPointContentsHook (nullptr);

  // a ground drop along the walk line vetoes the link
  int drop_calls = 0;
  engine.SetTraceLineHook ([&drop_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (drop_calls++ >= 2) {
      out->flFraction = 0.5f;
    }
    else {
      out->flFraction = 1.0f;
    }
  });
  CHECK (!graph.IsNodeReacheable (ystl::Vector (4000.0f, 0.0f, 0.0f), ystl::Vector (4200.0f, 0.0f, 0.0f)));
  engine.SetTraceLineHook (nullptr);

  // jump arcs: no gravity, too tall, too fast and blocked flights all fail
  sv_gravity.Set (0);
  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (5000.0f, 0.0f, 0.0f), ystl::Vector (5100.0f, 0.0f, 0.0f)));
  sv_gravity.Set (800);

  cv_graph_analyze_max_jump_height.Set (64.0f);
  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (5200.0f, 0.0f, 0.0f), ystl::Vector (5200.0f, 0.0f, 50.0f)));
  cv_graph_analyze_max_jump_height.Set (44.0f);

  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (5300.0f, 0.0f, 0.0f), ystl::Vector (5550.0f, 0.0f, 0.0f)));

  engine.SetTraceHullHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  CHECK (!graph.IsNodeReacheableWithJump (ystl::Vector (5600.0f, 0.0f, 0.0f), ystl::Vector (5700.0f, 0.0f, 0.0f)));
  engine.SetTraceHullHook (nullptr);

  // walk-off validation: matched falls pass, deep ones do not
  CHECK (GraphHook::IsWalkableDrop (graph, ystl::Vector (6000.0f, 0.0f, 100.0f), ystl::Vector (6070.0f, 0.0f, 50.0f)));
  CHECK (!GraphHook::IsWalkableDrop (graph, ystl::Vector (6000.0f, 0.0f, 100.0f), ystl::Vector (6100.0f, 0.0f, 60.0f)));

  // wayzone sizing: ladders, ladder neighbours, doors and drops
  graph.Reset ();
  graph.SetEditFlag (GraphEdit::On);
  const int r0 = AddTestNode (ystl::Vector (7000.0f, 0.0f, 0.0f),
    ystl::to_underlying (NodeFlag::Ladder));
  graph.CalculatePathRadius (r0);
  CHECK (graph.paths_[r0].radius == 0.0f);

  const int r1 = AddTestNode (ystl::Vector (7100.0f, 0.0f, 0.0f));
  LinkTestNodes (r1, r0, 100);
  graph.CalculatePathRadius (r1);
  CHECK (graph.paths_[r1].radius == 0.0f);

  engine.SetTraceHullHook ([door] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
    out->pHit = door;
  });
  engine.SetTraceLineHook ([door] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 1.0f;
    out->pHit = door;
  });
  const int r2 = AddTestNode (ystl::Vector (7200.0f, 0.0f, 0.0f));
  graph.CalculatePathRadius (r2);
  CHECK (graph.paths_[r2].radius == 0.0f);
  engine.SetTraceHullHook (nullptr);
  engine.SetTraceLineHook (nullptr);

  // blocked sightlines without doors shrink the wayzone the plain way
  engine.SetTraceHullHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  engine.SetTraceLineHook ([] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);
    out->flFraction = 0.5f;
  });
  const int r5 = AddTestNode (ystl::Vector (7500.0f, 0.0f, 0.0f));
  graph.CalculatePathRadius (r5);
  CHECK (graph.paths_[r5].radius == 0.0f);
  engine.SetTraceHullHook (nullptr);
  engine.SetTraceLineHook (nullptr);
  (void)r5;

  // the far wall alone blocks the radius readout as well
  int wall_calls = 0;
  engine.SetTraceHullHook ([&wall_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (wall_calls % 4 == 0) {
      out->flFraction = 1.0f; // the near wall stays clear
    }
    else {
      out->flFraction = 0.5f; // drops land, the far wall catches
    }
    ++wall_calls;
  });
  const int r6 = AddTestNode (ystl::Vector (7600.0f, 0.0f, 0.0f));
  graph.CalculatePathRadius (r6);
  CHECK (graph.paths_[r6].radius == 0.0f);
  engine.SetTraceHullHook (nullptr);
  (void)r6;

  // open drops shrink the wayzone, solid ground keeps it wide
  int zone_calls = 0;
  engine.SetTraceHullHook ([&zone_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (zone_calls == 0) {
      out->flFraction = 1.0f; // solid wall ahead: keep scanning the drops
    }
    else if (zone_calls == 1) {
      out->flFraction = 0.5f; // ground below: the first drop lands
    }
    else {
      out->flFraction = 1.0f; // void behind: the second drop misses
    }
    ++zone_calls;
  });
  const int r3 = AddTestNode (ystl::Vector (7300.0f, 0.0f, 0.0f));
  graph.CalculatePathRadius (r3);
  CHECK (graph.paths_[r3].radius == 0.0f);
  engine.SetTraceHullHook (nullptr);

  // clear ground all around leaves the widest wayzone
  int open_calls = 0;
  engine.SetTraceHullHook ([&open_calls] (const float *v1, const float *v2, int no_monsters, edict_t *skip, TraceResult *out) {
    testhost::FakeEngine::TraceNoHit (v1, v2, no_monsters, skip, out);

    if (open_calls % 4 == 1 || open_calls % 4 == 2) {
      out->flFraction = 0.5f; // drops find ground both ways
    }
    else {
      out->flFraction = 1.0f; // walls ahead and behind stay clear
    }
    ++open_calls;
  });
  const int r4 = AddTestNode (ystl::Vector (7400.0f, 0.0f, 0.0f));
  graph.CalculatePathRadius (r4);
  CHECK (graph.paths_[r4].radius == 96.0f);
  engine.SetTraceHullHook (nullptr);
  (void)r4;
  (void)r0;
  (void)r1;
  (void)r2;
  (void)r3;

  graph.SetEditor (nullptr);
  graph.SetEditFlag (GraphEdit::Off);
  graph.Reset ();
  bots.Destroy ();
  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

} // namespace bot
