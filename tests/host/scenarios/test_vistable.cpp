//
// YaPB test host: unit/vistable_{build,serial}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for vistable.cpp through the public API only (no
// production-code changes): incremental rebuild over a synthetic graph,
// visibility queries in a clear world, per-stance counters, guards,
// and disk round-trip with stale-layout rejection.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

// chain 0 - 1 - 2 - 3 with a crouch node and an island, dense numbering
void BuildVisGraph (int count) {
  graph.Reset ();

  for (int i = 0; i < count; ++i) {
    Path path {};
    path.origin = i < 4 ? ystl::Vector (100.0f * i, 0.0f, 0.0f) : ystl::Vector (2000.0f, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = 100;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  link (0, 1);
  link (1, 0);
  link (1, 2);
  link (2, 1);
  link (2, 3);
  link (3, 2);

  set_flag (graph.paths_[2].flags, NodeFlag::Crouch);

  graph.PopulateNodes ();
  planner.Init ();
}

void BootVis (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  HOST_REQUIRE (!analyzer.IsAnalyzing ());
}

// one rebuild() call advances a single source node: loop until ready
bool FinishRebuild (int max_iters = 40) {
  for (int i = 0; i < max_iters && !vistab.IsReady (); ++i) {
    vistab.Rebuild ();
  }
  return vistab.IsReady ();
}

// harness-only: on a real server the engine FS bridges the save (FS) and
// load (VFS) bases; the harness stages a plain directory, so mirror by hand
void MirrorVisFile () {
  const ystl::String save_path = bstor.BuildPath (StorageFile::Vistable);
  const ystl::String load_path = bstor.BuildPath (StorageFile::Vistable, true);
  ystl::File::make_path (ystl::String (save_path.substr (0, save_path.find_last_of (kPathSeparator))).chars ());
  ystl::File::make_path (ystl::String (load_path.substr (0, load_path.find_last_of (kPathSeparator))).chars ());

  ystl::File src (save_path, "rb");
  ystl::File dst (load_path, "wb");
  HOST_REQUIRE (src.eof () == false);

  for (int ch = src.get (); ch != EOF; ch = src.get ()) {
    dst.put_char (ch);
  }
}

} // namespace

TEST_CASE ("unit/vistable_build") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVis (engine, cs);

  // fresh table answers nothing but stays ready and harmless
  vistab.Rebuild (); // no rebuild requested, immediate return
  CHECK (vistab.IsReady ());
  CHECK (!vistab.Visible (0, 1));
  CHECK (!vistab.VisibleBothSides (0, 1));
  CHECK (!vistab.Visible (-1, 0));
  CHECK (!vistab.Visible (0, 99));

  // empty graph cannot build: load arms the rebuild, one pass settles it
  vistab.Load ();
  CHECK (!vistab.IsReady ());
  vistab.Rebuild ();
  CHECK (vistab.IsReady ());
  CHECK (!vistab.Visible (0, 0));

  // clear world: every pair sees each other from every stance
  BuildVisGraph (5);
  vistab.StartRebuild ();
  CHECK (!vistab.IsReady ());
  HOST_REQUIRE (FinishRebuild ());

  for (int src = 0; src < 5; ++src) {
    for (int dst = 0; dst < 5; ++dst) {
      CHECK (vistab.Visible (src, dst));
      CHECK (vistab.Visible (src, dst, VisIndex::Stand));
      CHECK (vistab.Visible (src, dst, VisIndex::Crouch));
      CHECK (vistab.VisibleBothSides (src, dst));
    }
    CHECK (!vistab.Visible (src, -1));
    CHECK (!vistab.Visible (-1, src));
    CHECK (!vistab.Visible (src, 99));

    // counters include the node itself, island included
    CHECK (graph[src].vis.stand == 5);
    CHECK (graph[src].vis.crouch == 5);
  }

  // rebuilds are idempotent
  vistab.StartRebuild ();
  CHECK (!vistab.IsReady ());
  HOST_REQUIRE (FinishRebuild ());
  CHECK (vistab.Visible (0, 4));
  CHECK (vistab.Visible (4, 0));
  CHECK (graph[4].vis.stand == 5);
}

TEST_CASE ("unit/vistable_serial") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootVis (engine, cs);
  BuildVisGraph (8); // storage loader rejects smaller graphs as damaged
  vistab.StartRebuild ();
  HOST_REQUIRE (FinishRebuild ());

  vistab.Save ();
  MirrorVisFile ();

  // disk data loads straight into service, no rebuild pass
  vistab.Load ();
  CHECK (vistab.IsReady ());
  CHECK (vistab.Visible (0, 7));
  CHECK (vistab.Visible (7, 0));
  CHECK (vistab.VisibleBothSides (2, 6));

  // resized graph rejects the stale layout and rebuilds instead
  Path extra {};
  extra.origin = ystl::Vector (2000.0f, 500.0f, 0.0f);
  extra.number = 8;
  extra.light = kInvalidLightLevel;

  for (auto &link : extra.links) {
    link.index = kInvalidNodeIndex;
  }
  graph.paths_.push (extra);
  graph.PopulateNodes ();

  vistab.Load ();
  CHECK (!vistab.IsReady ());
  HOST_REQUIRE (FinishRebuild ());
  CHECK (vistab.Visible (0, 8));
  CHECK (vistab.Visible (8, 0));
  CHECK (graph[8].vis.stand == 9);
}

} // namespace bot
