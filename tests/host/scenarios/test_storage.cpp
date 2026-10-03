//
// YaPB test host: unit/storage_{paths,io}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for storage.cpp through the public API only (no
// production-code changes): file mapping, path forms, save/load guards
// and negative reads, error handling and disk unlinking.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootStorage (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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

void BuildStorageGraph (int count) {
  graph.Reset ();

  for (int i = 0; i < count; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }
  graph.PopulateNodes ();
}

bool EndsWith (ystl::StringRef str, ystl::StringRef suffix) {
  return str.size () >= suffix.size () && str.substr (str.size () - suffix.size ()) == suffix;
}

void MakeParent (ystl::StringRef path) {
  const ystl::String parent = ystl::String (path.substr (0, path.find_last_of (kPathSeparator)));
  ystl::File::make_path (parent.chars ());
}

void WriteBytes (ystl::StringRef path, const char *data, size_t length) {
  MakeParent (path);

  ystl::File file (path, "wb");
  HOST_REQUIRE (!file.eof ());

  for (size_t i = 0; i < length; ++i) {
    file.put_char (data[i]);
  }
}

} // namespace

TEST_CASE ("unit/storage_paths") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootStorage (engine, cs);

  // option to file mapping, first match wins, graph is the default
  CHECK (bstor.StorageToBotFile (StorageOption::Graph) == StorageFile::Graph);
  CHECK (bstor.StorageToBotFile (StorageOption::Matrix) == StorageFile::Pathmatrix);
  CHECK (bstor.StorageToBotFile (StorageOption::Vistable) == StorageFile::Vistable);
  CHECK (bstor.StorageToBotFile (StorageOption::Practice) == StorageFile::Practice);
  CHECK (bstor.StorageToBotFile (StorageOption::Invalid) == StorageFile::Graph);
  CHECK (bstor.StorageToBotFile (StorageOption::Graph | StorageOption::Practice) == StorageFile::Graph);
  CHECK (bstor.StorageToBotFile (StorageOption::Official) == StorageFile::Graph);

  // per-type extensions ride the map name
  CHECK (EndsWith (bstor.BuildPath (StorageFile::Practice), ".prc"));
  CHECK (EndsWith (bstor.BuildPath (StorageFile::Vistable), ".vis"));
  CHECK (EndsWith (bstor.BuildPath (StorageFile::Pathmatrix), ".pmx"));
  CHECK (EndsWith (bstor.BuildPath (StorageFile::Graph), ".graph"));

  // memory (VFS) loads resolve elsewhere: documents the harness gap
  // that file-mirroring tests work around, not a production split
  const ystl::String save_path = bstor.BuildPath (StorageFile::Practice);
  const ystl::String load_path = bstor.BuildPath (StorageFile::Practice, true);
  CHECK (!save_path.empty ());
  CHECK (!load_path.empty ());
  CHECK (save_path != load_path);
  CHECK (EndsWith (load_path, ".prc"));

  // mapless paths stop at the directory
  CHECK (!EndsWith (bstor.BuildPath (StorageFile::Practice, false, true), ".prc"));
  CHECK (!EndsWith (bstor.BuildPath (StorageFile::Graph, true, true), ".graph"));

  // log files carry a dated bot name
  const ystl::String log_path = bstor.BuildPath (StorageFile::LogFile);
  CHECK (EndsWith (log_path, ".txt"));
  CHECK (log_path.find (product.name_lower) != ystl::String::InvalidIndex);

  // install probe never fails, paths stay usable afterwards
  bstor.CheckInstallLocation ();
  CHECK (!bstor.GetRunningPath ().empty ());
  CHECK (EndsWith (bstor.BuildPath (StorageFile::Practice), ".prc"));

  bots.Destroy ();
}

TEST_CASE ("unit/storage_io") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootStorage (engine, cs);

  // empty payloads never touch the disk
  ystl::Array<Practice::Entry> empty {};
  CHECK (!bstor.Save (empty));

  // undersized graphs are refused up front
  BuildStorageGraph (3);
  ystl::Array<Path> few {};
  few.push (Path {});
  few.push (Path {});
  few.push (Path {});
  CHECK (!bstor.Save (few));

  // missing files read as failure, quietly for training data
  BuildStorageGraph (8);
  const ystl::String load_path = bstor.BuildPath (StorageFile::Practice, true);
  MakeParent (load_path);

  if (ystl::plat.file_exists (load_path.chars ())) {
    ystl::plat.remove_file (load_path.chars ());
  }
  ystl::Array<Practice::Entry> entries {};
  CHECK (!bstor.Load (entries));

  // garbage magic is rejected without side effects
  WriteBytes (load_path, "JUNKJUNKJUNKJUNKJUNKJUNKJUNKJUNKJUNKJUNKJUNKJUNK", 48);
  CHECK (graph.Length () == 8);
  CHECK (!bstor.Load (entries));
  CHECK (graph.Length () == 8);

  // node-count mismatch against the live graph is rejected too
  StorageHeader hdr {};
  hdr.magic = kStorageMagic;
  hdr.version = static_cast<int32_t> (StorageVersion::Practice);
  hdr.options = static_cast<int32_t> (StorageOption::Practice);
  hdr.length = 99;
  {
    MakeParent (load_path);
    ystl::File file (load_path, "wb");
    HOST_REQUIRE (!file.eof ());
    file.le_write (hdr);
  }
  CHECK (!bstor.Load (entries));

  // direct error reports fail closed...
  ystl::MemFile missing ("no_such_storage_file_xyz");
  CHECK (!bstor.Error (false, false, missing, "quiet probe %d", 1));

  // ...and the graph variant additionally drops the graph
  ystl::MemFile missing_graph ("no_such_graph_file_xyz");
  CHECK (!bstor.Error (true, false, missing_graph, "graph probe"));
  CHECK (graph.Length () == 0);

  // unlinking removes every training file plus the graph file itself
  BuildStorageGraph (8);
  const ystl::String graph_path = bstor.BuildPath (StorageFile::Graph);
  const ystl::String prac_path = bstor.BuildPath (StorageFile::Practice);
  const ystl::String vis_path = bstor.BuildPath (StorageFile::Vistable);
  const ystl::String pmx_path = bstor.BuildPath (StorageFile::Pathmatrix);

  WriteBytes (graph_path, "x", 1);
  WriteBytes (prac_path, "x", 1);
  WriteBytes (vis_path, "x", 1);
  WriteBytes (pmx_path, "x", 1);
  HOST_REQUIRE (ystl::plat.file_exists (graph_path.chars ()));

  bstor.UnlinkFromDisk (false, true);
  CHECK (!ystl::plat.file_exists (graph_path.chars ()));
  CHECK (!ystl::plat.file_exists (prac_path.chars ()));
  CHECK (!ystl::plat.file_exists (vis_path.chars ()));
  CHECK (!ystl::plat.file_exists (pmx_path.chars ()));
  CHECK (graph.Length () == 0);

  bots.Destroy ();
}

} // namespace bot
