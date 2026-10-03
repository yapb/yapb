//
// YaPB test host: unit/analyze_{cleaner,finish}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for analyze.cpp through a test-only hook (friend, no
// production behavior changes): the ConnectionCleaner triplet/pair/seam
// passes with bridge vetoes, and the GraphAnalyze optimizer passes,
// finish-flag parsing, goal/camp/team marking plus a full
// start-to-analyzed pipeline. Randomness is pinned via exact modes and
// extreme chances, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct AnalyzeHook {
  static int CleanCount (ConnectionCleaner &cleaner) {
    return cleaner.count_;
  }
  static const ConnectionCleaner::Link &CleanLink (ConnectionCleaner &cleaner, int pos) {
    return cleaner.links_[pos];
  }
  static int CleanIsolated (ConnectionCleaner &cleaner) {
    return cleaner.isolated_slot_;
  }
  static bool CleanProtected (ConnectionCleaner &cleaner, const ConnectionCleaner::Link &link) {
    return cleaner.IsProtected (link);
  }
  static bool CleanAlternate (ConnectionCleaner &cleaner, int from, int to, int skip_slot) {
    return cleaner.HasAlternateRoute (from, to, skip_slot);
  }
  static bool CleanRemove (ConnectionCleaner &cleaner, const char *own_code, const char *recip_code, int pos) {
    return cleaner.RemoveConnection (own_code, recip_code, pos);
  }
  static bool CleanPair (ConnectionCleaner &cleaner, int pos) {
    return cleaner.InspectPair (pos);
  }
  static int FinishFlags () {
    return GraphAnalyze::FinishFlags ();
  }
  static float Importance (int index) {
    return analyzer.NodeImportance (index);
  }
  static bool Collinear (int from, int mid, int to, float tolerance) {
    return analyzer.IsCollinear (from, mid, to, tolerance);
  }
  static bool FreeSlot (int index) {
    return analyzer.HasFreeLinkSlot (index);
  }
  static bool JumpLink (int index) {
    return analyzer.HasJumpLink (index);
  }
  static bool LinkJump (int from, int to) {
    return analyzer.LinkHasJump (from, to);
  }
  static bool Link2 (int from, int to) {
    return analyzer.LinkNodes (from, to);
  }
  static void Bypass (int index) {
    analyzer.BypassNode (index);
  }
  static bool PassCollinear () {
    return analyzer.PassRemoveCollinear ();
  }
  static bool PassDuplicates () {
    return analyzer.PassRemoveDuplicates ();
  }
  static bool PassUnconnected () {
    return analyzer.PassRemoveUnconnected ();
  }
  static bool OptimizeSlice () {
    return analyzer.OptimizeSlice ();
  }
  static void Cleanup () {
    analyzer.Cleanup ();
  }
  static bool Repair () {
    return analyzer.RepairConnectivity ();
  }
  static void Sanity () {
    analyzer.LogSanityBreakdown ();
  }
  static void MarkGoals () {
    analyzer.MarkGoals ();
  }
  static void EnterCamps () {
    analyzer.EnterCamps ();
  }
  static bool MarkCamps () {
    return analyzer.MarkCampsSlice ();
  }
  static void MarkTeams () {
    analyzer.MarkTeamSides ();
  }
  static int Stage () {
    return ystl::to_underlying (analyzer.finish_stage_);
  }
  static void ClearOptimized () {
    ystl::fill (analyzer.optimized_nodes_, false);
  }
};

namespace {

// node factory, mirrors the other graph tests
int AddAnaNode (const ystl::Vector &origin) {
  Path path {};
  path.origin = origin;
  path.number = graph.Length ();
  path.light = kInvalidLightLevel;

  for (auto &link : path.links) {
    link.index = kInvalidNodeIndex;
  }
  graph.paths_.push (path);
  return path.number;
}

void LinkAnaDist (int from, int to, float dist) {
  for (auto &slot : graph.paths_[from].links) {
    if (slot.index == kInvalidNodeIndex) {
      slot.index = static_cast<int16_t> (to);
      slot.distance = static_cast<int32_t> (dist);
      return;
    }
  }
  HOST_REQUIRE (false);
}

void LinkAna (int from, int to) {
  LinkAnaDist (from, to, graph.paths_[from].origin.distance (graph.paths_[to].origin));
}

int FindAnaSlot (int from, int to) {
  const auto &links = graph.paths_[from].links;

  for (int s = 0; s < kMaxNodeLinks; ++s) {
    if (links[s].index == to) {
      return s;
    }
  }
  return kInvalidNodeIndex;
}

void BuildAnaChain (int count, float spacing) {
  for (int i = 0; i < count; ++i) {
    AddAnaNode (ystl::Vector (spacing * i, 0.0f, 0.0f));
  }

  for (int i = 0; i < count - 1; ++i) {
    LinkAna (i, i + 1);
    LinkAna (i + 1, i);
  }
  graph.PopulateNodes ();
}

void BootAnalyze (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();

  // demolition flag before activation, goal marking needs the map type
  HOST_REQUIRE (engine.SpawnEntity ("func_bomb_target") != nullptr);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  // no installEngineTables here: GiveFnptrsToDll already bound the
  // tables, a second copy would alias the hooked pfnCreateNamedEntity
  // back into engfuncs and recurse forever on the next spawn
}

int CountAnaFlag (NodeFlag flag) {
  int found = 0;

  for (int i = 0; i < graph.Length (); ++i) {
    if (graph.Exists (i) && has_flag (graph.paths_[i].flags, flag)) {
      ++found;
    }
  }
  return found;
}

} // namespace

TEST_CASE ("unit/analyze_cleaner") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootAnalyze (engine, cs);
  graph.Reset ();

  // triplet center: tight angular cluster so the 40/80-degree gates
  // pass and the distance rule decides (P survives, R/Q drop via P)
  const int c0 = AddAnaNode (ystl::Vector (0.0f, 0.0f, 0.0f));
  const int p0 = AddAnaNode (ystl::Vector (100.0f, 0.0f, 0.0f));
  const int r0 = AddAnaNode (ystl::Vector (150.0f, 10.0f, 0.0f));
  const int q0 = AddAnaNode (ystl::Vector (200.0f, 20.0f, 0.0f));
  graph.PopulateNodes ();

  LinkAnaDist (c0, p0, 100.0f);
  LinkAnaDist (c0, q0, 200.0f);
  LinkAnaDist (c0, r0, 150.0f);
  LinkAna (p0, r0); // alternate route into R
  LinkAna (p0, q0); // alternate route into Q

  // self and dead slots never enter the working set...
  graph.paths_[c0].links[3].index = static_cast<int16_t> (c0);
  graph.paths_[c0].links[3].distance = 10;

  ConnectionCleaner clean0 (graph, c0);
  clean0.Collect ();
  CHECK (AnalyzeHook::CleanCount (clean0) == 3);

  // ascending by distance: shortest first (matches graph.cpp intent)
  clean0.SortByDistance ();
  CHECK (AnalyzeHook::CleanLink (clean0, 0).distance == 100.0f);
  CHECK (AnalyzeHook::CleanLink (clean0, 1).distance == 150.0f);
  CHECK (AnalyzeHook::CleanLink (clean0, 2).distance == 200.0f);

  // reference is the shortest link, bearings run low to high so the
  // angular gates see positive deltas (tight cluster: all < 40 deg)
  clean0.ComputeBearings ();
  CHECK (AnalyzeHook::CleanLink (clean0, 0).target == p0);
  CHECK (AnalyzeHook::CleanLink (clean0, 0).bearing == 0.0f);
  CHECK (AnalyzeHook::CleanLink (clean0, 1).target == r0);
  CHECK (AnalyzeHook::CleanLink (clean0, 2).target == q0);
  CHECK (AnalyzeHook::CleanLink (clean0, 2).bearing > AnalyzeHook::CleanLink (clean0, 1).bearing);
  CHECK (AnalyzeHook::CleanLink (clean0, 1).bearing - AnalyzeHook::CleanLink (clean0, 0).bearing < 40.0f);
  CHECK (AnalyzeHook::CleanLink (clean0, 2).bearing - AnalyzeHook::CleanLink (clean0, 0).bearing < 80.0f);

  // ...protection answers ladder pairs and jump links only
  CHECK (!AnalyzeHook::CleanProtected (clean0, AnalyzeHook::CleanLink (clean0, 0)));

  graph.paths_[c0].flags |= NodeFlag::Ladder;
  graph.paths_[p0].flags |= NodeFlag::Ladder;
  CHECK (AnalyzeHook::CleanProtected (clean0, AnalyzeHook::CleanLink (clean0, 0)));
  graph.paths_[c0].flags &= ~NodeFlag::Ladder;
  graph.paths_[p0].flags &= ~NodeFlag::Ladder;

  graph.paths_[c0].links[FindAnaSlot (c0, p0)].flags = static_cast<uint16_t> (ystl::to_underlying (PathFlag::Jump));
  CHECK (AnalyzeHook::CleanProtected (clean0, AnalyzeHook::CleanLink (clean0, 0)));
  graph.paths_[c0].links[FindAnaSlot (c0, p0)].flags = 0;

  // ...alternate routes see one-hop parallels and skip the tested slot
  CHECK (AnalyzeHook::CleanAlternate (clean0, c0, q0, FindAnaSlot (c0, q0)));
  CHECK (AnalyzeHook::CleanAlternate (clean0, c0, r0, FindAnaSlot (c0, r0)));
  CHECK (!AnalyzeHook::CleanAlternate (clean0, c0, p0, FindAnaSlot (c0, p0)));

  // ...the full run drops R by the triplet rule, then Q by the pair
  // rule, and the survivor still reaches both through P
  clean0.Run ();
  CHECK (clean0.RemovedCount () == 2);

  ConnectionCleaner recollect (graph, c0);
  recollect.Collect ();
  CHECK (AnalyzeHook::CleanCount (recollect) == 1);
  CHECK (AnalyzeHook::CleanLink (recollect, 0).target == p0);
  CHECK (graph.IsConnected (c0, p0));
  CHECK (graph.IsConnected (p0, q0));
  CHECK (graph.IsConnected (p0, r0));

  // pair center: the shorter link survives the P.2.3 branch...
  const int c1 = AddAnaNode (ystl::Vector (1000.0f, 0.0f, 0.0f));
  const int a1 = AddAnaNode (ystl::Vector (1100.0f, 0.0f, 0.0f));
  const int b1 = AddAnaNode (ystl::Vector (1200.0f, 0.0f, 0.0f));
  graph.PopulateNodes ();

  LinkAnaDist (c1, a1, 100.0f);
  LinkAnaDist (c1, b1, 200.0f);
  LinkAna (a1, b1);

  ConnectionCleaner clean1 (graph, c1);
  clean1.Collect ();
  clean1.SortByDistance ();
  clean1.ComputeBearings ();
  CHECK (AnalyzeHook::CleanPair (clean1, 1));
  CHECK (clean1.RemovedCount () == 1);

  ConnectionCleaner recollect1 (graph, c1);
  recollect1.Collect ();
  CHECK (AnalyzeHook::CleanCount (recollect1) == 1);
  CHECK (AnalyzeHook::CleanLink (recollect1, 0).target == a1);

  // veto center: with no alternate anywhere nothing moves, the seam
  // pass finds no isolated slot and also stands down...
  const int c2 = AddAnaNode (ystl::Vector (2000.0f, 0.0f, 0.0f));
  const int t1 = AddAnaNode (ystl::Vector (2100.0f, 0.0f, 0.0f));
  const int t2 = AddAnaNode (ystl::Vector (2000.0f + 150.0f * -0.1736f, 150.0f * 0.9848f, 0.0f));
  const int t3 = AddAnaNode (ystl::Vector (2000.0f + 120.0f * -0.9397f, 120.0f * -0.3420f, 0.0f));
  graph.PopulateNodes ();

  LinkAnaDist (c2, t1, 100.0f);
  LinkAnaDist (c2, t2, 150.0f);
  LinkAnaDist (c2, t3, 120.0f);

  ConnectionCleaner clean2 (graph, c2);
  clean2.Collect ();
  CHECK (AnalyzeHook::CleanCount (clean2) == 3);
  CHECK (!AnalyzeHook::CleanAlternate (clean2, c2, t2, FindAnaSlot (c2, t2)));
  CHECK (!AnalyzeHook::CleanRemove (clean2, "own", "recip", 1));

  clean2.Run ();
  CHECK (clean2.RemovedCount () == 0);
  CHECK (AnalyzeHook::CleanIsolated (clean2) == kInvalidNodeIndex);

  ConnectionCleaner recollect2 (graph, c2);
  recollect2.Collect ();
  CHECK (AnalyzeHook::CleanCount (recollect2) == 3);
}

TEST_CASE ("unit/analyze_finish") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootAnalyze (engine, cs);

  // ...finish flags accept numbers and names, unknown words clear...
  cv_graph_analyze_on_finish.Set ("all");
  CHECK (AnalyzeHook::FinishFlags () == 31);
  cv_graph_analyze_on_finish.Set ("none");
  CHECK (AnalyzeHook::FinishFlags () == 0);
  cv_graph_analyze_on_finish.Set ("optimize,clean");
  CHECK (AnalyzeHook::FinishFlags () == 3);
  cv_graph_analyze_on_finish.Set ("7");
  CHECK (AnalyzeHook::FinishFlags () == 7);
  cv_graph_analyze_on_finish.Set ("goals | camps");
  CHECK (AnalyzeHook::FinishFlags () == 12);
  cv_graph_analyze_on_finish.Set ("bogus-finish-name");
  CHECK (AnalyzeHook::FinishFlags () == 0);
  cv_graph_analyze_on_finish.Set ("");
  CHECK (AnalyzeHook::FinishFlags () == 0);
  cv_graph_analyze_on_finish.Set ("all");

  // ...a full start-to-analyzed pipeline settles the flood, runs the
  // optimizer, reverts the mass disconnect and marks the finish...
  // (frames advance under the lag cutoff or update stands down)
  graph.Reset ();
  BuildAnaChain (8, 100.0f);
  cv_graph_analyze_auto_save.Set (0);
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  analyzer.Start ();
  CHECK (analyzer.IsAnalyzing ());
  analyzer.Start (); // second start is a no-op
  CHECK (analyzer.IsAnalyzing ());

  bool settled = false;

  for (int i = 0; i < 400 && !settled; ++i) {
    engine.AdvanceTime (0.05f);
    analyzer.Update ();
    settled = analyzer.IsAnalyzed ();
  }
  CHECK (settled);
  CHECK (!analyzer.IsAnalyzing ());
  CHECK (AnalyzeHook::Stage () == ystl::to_underlying (GraphAnalyze::FinishStage::Idle));
  CHECK (graph.Length () >= 2 && graph.Length () < 8); // passes bypassed and cleaned
  CHECK (graph.Exists (0));
  cv_graph_analyze_auto_save.Set (1);

  // ...importance prices connections, goals, and inbound links...
  graph.Reset ();
  BuildAnaChain (4, 100.0f);
  AnalyzeHook::ClearOptimized ();
  CHECK (AnalyzeHook::Importance (99) == 0.0f);
  CHECK (AnalyzeHook::Importance (1) == 8.0f); // 1 + 2 outgoing * 2 + 2 inbound * 1.5

  graph.paths_[1].flags |= NodeFlag::Goal;
  CHECK (AnalyzeHook::Importance (1) == 1008.0f);
  graph.paths_[1].flags &= ~NodeFlag::Goal;

  // ...collinearity answers segments, offsets, and degenerates...
  CHECK (AnalyzeHook::Collinear (0, 1, 2, 32.0f));
  CHECK (!AnalyzeHook::Collinear (0, 3, 2, 32.0f));
  CHECK (!AnalyzeHook::Collinear (0, 0, 0, 32.0f));
  CHECK (!AnalyzeHook::Collinear (0, 99, 2, 32.0f));

  const int off = AddAnaNode (ystl::Vector (50.0f, 50.0f, 0.0f));
  graph.PopulateNodes ();
  CHECK (!AnalyzeHook::Collinear (0, off, 2, 32.0f));

  // ...link helpers never evict and never self-link...
  CHECK (AnalyzeHook::FreeSlot (0));
  CHECK (AnalyzeHook::Link2 (0, 3));
  CHECK (!AnalyzeHook::Link2 (0, 0));
  CHECK (AnalyzeHook::Link2 (0, 3)); // duplicate reports connected

  for (int s = 0; s < 6; ++s) {
    const int leaf = AddAnaNode (ystl::Vector (5000.0f + 10.0f * s, 0.0f, 0.0f));
    graph.PopulateNodes ();
    LinkAna (0, leaf);
  }
  const int outsider = AddAnaNode (ystl::Vector (6000.0f, 0.0f, 0.0f));
  graph.PopulateNodes ();
  CHECK (!AnalyzeHook::FreeSlot (0));
  CHECK (!AnalyzeHook::Link2 (0, outsider)); // full slots refuse new links
  CHECK (!AnalyzeHook::JumpLink (0));

  graph.FlagJumpLink (0, 3);
  CHECK (AnalyzeHook::JumpLink (0));
  CHECK (AnalyzeHook::LinkJump (0, 3));
  CHECK (!AnalyzeHook::LinkJump (1, 2));

  // ...bypass rewires around the dropped node and neutralizes it...
  graph.Reset ();
  BuildAnaChain (3, 100.0f);
  AnalyzeHook::ClearOptimized ();
  AnalyzeHook::Bypass (1);
  CHECK (graph.IsConnected (0, 2));
  CHECK (graph.paths_[1].flags == 0);

  bool midlinked = true;

  for (const auto &link : graph.paths_[1].links) {
    if (link.index != kInvalidNodeIndex) {
      midlinked = false;
    }
  }
  CHECK (midlinked);

  // ...duplicate, collinear and unconnected passes each fire once...
  graph.Reset ();
  BuildAnaChain (4, 100.0f);
  const int dup = AddAnaNode (ystl::Vector (120.0f, 0.0f, 0.0f));
  graph.PopulateNodes ();
  AnalyzeHook::ClearOptimized ();
  CHECK (AnalyzeHook::PassDuplicates ());
  CHECK (graph.paths_[dup].flags == 0);
  CHECK (!AnalyzeHook::PassDuplicates ());

  graph.Reset ();
  BuildAnaChain (3, 100.0f);
  AnalyzeHook::ClearOptimized ();
  CHECK (AnalyzeHook::PassCollinear ());
  CHECK (graph.IsConnected (0, 2));
  CHECK (!AnalyzeHook::PassCollinear ());

  graph.Reset ();
  BuildAnaChain (2, 100.0f);
  const int lone = AddAnaNode (ystl::Vector (5000.0f, 0.0f, 0.0f));
  graph.PopulateNodes ();
  AnalyzeHook::ClearOptimized ();
  CHECK (AnalyzeHook::PassUnconnected ());
  CHECK (graph.paths_[lone].flags == 0);

  // ...single slices report work, cleanup erases the neutralized...
  graph.Reset ();
  BuildAnaChain (3, 100.0f);
  AnalyzeHook::ClearOptimized ();
  CHECK (AnalyzeHook::OptimizeSlice ());

  graph.Reset ();
  BuildAnaChain (2, 100.0f);
  AnalyzeHook::ClearOptimized ();
  CHECK (!AnalyzeHook::OptimizeSlice ());

  graph.paths_[1].links[0].index = 99;
  graph.paths_[1].links[0].distance = 10;
  AnalyzeHook::Cleanup ();
  CHECK (graph.Length () == 0); // dangling node goes, then 0 dangles too

  // ...repair relinks a severed chain and rejects a tiny graph...
  graph.Reset ();
  BuildAnaChain (8, 100.0f);
  AnalyzeHook::ClearOptimized ();
  graph.UnassignPath (3, FindAnaSlot (3, 4));
  graph.UnassignPath (4, FindAnaSlot (4, 3));
  HOST_REQUIRE (!graph.IsConnected (3, 4));
  CHECK (AnalyzeHook::Repair ());
  CHECK (graph.Length () == 8);

  // repair heals reachability, not the exact severed links...
  ystl::Array<bool> fwd {};
  graph.ComputeForwardReachability (fwd);
  HOST_REQUIRE (fwd.size () == static_cast<size_t> (graph.Length ()));

  bool all_reached = true;

  for (int i = 0; i < graph.Length (); ++i) {
    all_reached = all_reached && fwd[i];
  }
  CHECK (all_reached);
  AnalyzeHook::Sanity (); // breakdown only logs, never crashes

  graph.Reset ();
  BuildAnaChain (3, 100.0f);
  CHECK (!AnalyzeHook::Repair ());

  // ...goals flag the bomb box, teams split defenders by spacing...
  graph.Reset ();
  BuildAnaChain (8, 100.0f);
  AnalyzeHook::ClearOptimized ();

  edict_t *bomb = engine.SpawnEntity ("func_bomb_target");
  HOST_REQUIRE (bomb != nullptr);
  bomb->v.absmin = ystl::Vector (150.0f, -50.0f, -50.0f);
  bomb->v.absmax = ystl::Vector (250.0f, 50.0f, 50.0f);

  cv_graph_analyze_on_finish.Set ("goals");
  AnalyzeHook::MarkGoals ();
  CHECK (CountAnaFlag (NodeFlag::Goal) == 1);
  CHECK (has_flag (graph.paths_[2].flags, NodeFlag::Goal));

  edict_t *spawn = engine.SpawnEntity ("info_player_deathmatch");
  HOST_REQUIRE (spawn != nullptr);
  const float spawn_pos[3] = { 2000.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (spawn, spawn_pos);

  cv_graph_analyze_on_finish.Set ("teams");
  AnalyzeHook::MarkTeams ();
  CHECK (CountAnaFlag (NodeFlag::CTOnly) == 3);
  CHECK (CountAnaFlag (NodeFlag::TerroristOnly) == 0);
  cv_graph_analyze_on_finish.Set ("all");

  // ...camps sweep without the flag is trivially done, with the flag
  // an empty map yields corners nowhere...
  AnalyzeHook::EnterCamps ();
  cv_graph_analyze_on_finish.Set ("none");
  CHECK (AnalyzeHook::MarkCamps ());

  cv_graph_analyze_on_finish.Set ("camps");
  AnalyzeHook::EnterCamps ();

  // slices are wall-clock budgeted, pump until done instead of one shot
  bool camps_done = false;

  for (int i = 0; i < 100 && !camps_done; ++i) {
    camps_done = AnalyzeHook::MarkCamps ();
  }
  CHECK (camps_done);
  CHECK (CountAnaFlag (NodeFlag::Camp) == 0);
  cv_graph_analyze_on_finish.Set ("all");

  // ...analysis stays down when autostart is off...
  analyzer.Suspend ();
  cv_graph_analyze_auto_start.Set (0);
  analyzer.Start ();
  CHECK (!analyzer.IsAnalyzing ());
  analyzer.Update ();
  CHECK (!analyzer.IsAnalyzing ());
  cv_graph_analyze_auto_start.Set (1);
}

} // namespace bot
