//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

template <typename U, ReservePolicy R> bool Storage::Load (ystl::Array<U, R> &data, ExtenHeader *exten, StorageOption *out_options) {
  auto type = GuessType<U> ();
  ystl::String filename = BuildPath (StorageToBotFile (type.option), true);

  // graphs can be downloaded
  const bool is_graph = has_flag (type.option, StorageOption::Graph);
  const bool is_debug = ctrl.IsDebug () || game.IsDeveloperMode ();

  ystl::MemFile file (filename); // open the file

  // if graph & attempted to load multiple times, bail out, we're failed
  if (is_graph && ++retries_ > 2) {
    ResetRetries ();

    return Error (is_graph, is_debug, file, "Unable to load %s (filename: '%s'). Giving up after repeated attempts.", type.name, filename);
  }

  // downloader for graph
  auto download = [&] () -> bool {
    ctrl.Debug ("Graph download debug: url='%s' base='%s' tls=%d.", cv_graph_url.As<ystl::StringRef> ().chars (),
      graph_urls.DownloadBase ().chars (), ystl::http.has_tls_support () ? 1 : 0);

    if (!graph.CanDownload ()) {
      ctrl.Debug ("Graph download skipped, downloads disabled or empty url.");
      return false;
    }
    auto to_download = BuildPath (StorageToBotFile (type.option), false);
    auto from_download = graph_urls.DownloadUrl (game.GetMapName ());

    if (from_download.empty ()) {
      ctrl.Debug ("Graph download skipped, empty download url for map '%s'.", game.GetMapName ());
      return false;
    }

    // try to download
    if (ystl::http.download_file (from_download, to_download)) {
      ctrl.Msg ("%s file '%s' successfully downloaded. Processing...", type.name, filename);
      return true;
    }
    else {
      const auto code = ystl::http.get_last_status_code ();

      ctrl.Msg ("Can't download '%s' from '%s' to '%s'... (%d).", filename, from_download, to_download, code);

      if (code == HttpClientResult::HttpOnly) {
        ctrl.Msg ("%s.", graph_urls.HttpsUnsupportedText ());
      }
    }
    return false;
  };

  // tries to reload or open pwf file
  auto try_reload = [&] () -> bool {
    file.close ();

    if (!is_graph) {
      return false;
    }

    if (download ()) {
      return Load (data, exten, out_options);
    }

    if (graph.ConvertOldFormat ()) {
      return Load (data, exten, out_options);
    }
    return false;
  };

  // no open no fun
  if (!file) {
    if (try_reload ()) {
      return true;
    }
    return Error (is_graph, is_debug, file, "Unable to open %s file for reading (filename: '%s').", type.name, filename);
  }

  // file exists, now clear old data before loading new
  data.clear ();

  // resize data to fit the stuff
  auto resize_data = [&] (const size_t length) {
    data.resize (length);
  };

  // erase the current graph just in case
  auto unlink_if_graph = [&] () {
    if (is_graph) {
      UnlinkFromDisk (false, true);
    }
  };

  // read the header (converted from on-disk little-endian layout)
  StorageHeader hdr {};
  file.le_read (hdr);

  // check the magic
  if (hdr.magic != kStorageMagic && hdr.magic != kStorageMagicUB) {
    unlink_if_graph ();

    if (try_reload ()) {
      return true;
    }
    return Error (is_graph, is_debug, file, "Unable to read magic of %s (filename: '%s').", type.name, filename);
  }

  // check the path-numbers
  if (!is_graph && hdr.length != graph.Length ()) {
    return Error (is_graph, is_debug, file, "Damaged %s (filename: '%s'). Mismatch number of nodes (got: '%d', need: '%d').", type.name,
      filename, hdr.length, graph.Length ());
  }

  // check the count
  if (hdr.length == 0 || hdr.length > kMaxNodes || hdr.length < kMaxNodeLinks) {
    unlink_if_graph ();

    if (try_reload ()) {
      return true;
    }
    return Error (
      is_graph, is_debug, file, "Damaged %s (filename: '%s'). Paths length is overflowed (got: '%d').", type.name, filename, hdr.length);
  }
  const auto file_version = static_cast<int> (hdr.version);
  const auto need_version = static_cast<int> (type.version);

  // check the version
  if (file_version > need_version && is_graph) {
    ctrl.Msg ("Graph version mismatch %s (filename: '%s'). Version number differs (got: '%d', need: '%d') Please, upgrade %s.", type.name,
      filename, file_version, need_version, product.name);
  }
  else if (file_version != need_version && !is_graph) {
    return Error (is_graph, is_debug, file, "Damaged %s (filename: '%s'). Version number differs (got: '%d', need: '%d').", type.name, filename,
      file_version, need_version);
  }

  // save graph version
  if (is_graph) {
    graph.SetGraphHeader (&hdr);
  }

  // check the storage type
  if ((hdr.options & type.option) != type.option) {
    return Error (is_graph, is_debug, file, "Incorrect storage format for %s (filename: '%s').", type.name, filename);
  }
  const auto compressed_size = static_cast<size_t> (hdr.compressed);
  const auto number_nodes = static_cast<size_t> (hdr.length);
  const auto uncompressed_size = static_cast<size_t> (hdr.uncompressed);

  ystl::ULZ ulz {};
  ystl::Array<uint8_t, ReservePolicy::Proportional> compressed (compressed_size + sizeof (uint8_t) * ystl::ULZ::Excess);

  // graph is not resized upon load
  if (is_graph) {
    if (uncompressed_size > number_nodes * sizeof (U)) {
      return Error (is_graph, is_debug, file, "Damaged %s (filename: '%s'). Uncompressed size is too large (got: '%d', need: '%d').", type.name,
        filename, hdr.uncompressed, static_cast<int> (number_nodes * sizeof (U)));
    }
    resize_data (number_nodes);
  }
  else {
    resize_data ((uncompressed_size + sizeof (U) - 1) / sizeof (U));
  }

  // read compressed data
  const auto compressed_span = ystl::Span<uint8_t> { compressed.data (), compressed_size };

  if (file.read (compressed_span) == compressed_size) {
    // try to uncompress
    if (ulz.uncompress (compressed_span, ystl::Span<uint8_t> { reinterpret_cast<uint8_t *> (data.data ()),
                                           static_cast<size_t> (hdr.uncompressed) }) == ystl::ULZ::UncompressFailure) {
      return Error (is_graph, is_debug, file, "Unable to decompress ystl::ULZ data for %s (filename: '%s').", type.name, filename);
    }
    else {
      // convert the payload from on-disk little-endian layout (no-op on little-endian systems)
      ystl::leio::le_swap (ystl::Span<U> { data.data (), data.size () });

      if (out_options) {
        *out_options = static_cast<StorageOption> (hdr.options);
      }

      // author of graph.. save
      if (has_flag (hdr.options, StorageOption::Exten) && exten != nullptr) {
        const auto exten_size = sizeof (ExtenHeader);
        const auto actually_read = file.le_read (*exten) * exten_size;

        if (is_graph) {
          ResetRetries ();

          ExtenHeader exten_header {};
          ystl::strings.copy (exten_header.author, exten->author, ystl::bufsize (exten->author));

          if (exten_size <= actually_read) {
            // write modified by, only if the name is different
            if (!ystl::strings.is_empty (exten_header.author) &&
                strncmp (exten_header.author, exten->modified, ystl::bufsize (exten_header.author)) != 0) {

              ystl::strings.copy (exten_header.modified, exten->modified, ystl::bufsize (exten->modified));
            }
          }
          else {
            ystl::strings.copy (exten_header.modified, "(none)", ystl::bufsize (exten->modified));
          }
          exten_header.map_size = exten->map_size;

          // tell graph about exten header
          graph.SetExtenHeader (&exten_header);
        }
      }

      // for visibility tables load counts of stand/count numbers
      if (has_flag (type.option, StorageOption::Vistable)) {
        for (auto &path : graph) {
          file.le_read (path.vis); // converted from on-disk little-endian layout
        }
      }

      data.shrink (); // drop bit_ceil + ulz slack

      if (game.IsDeveloperMode ()) {
        ctrl.Msg ("Loaded Bots %s data v%d (Memory: %.2fMB).", type.name, hdr.version,
          static_cast<float> (data.size () * sizeof (U)) / 1024.0f / 1024.0f);
      }
      file.close ();

      return true;
    }
  }
  else {
    return Error (is_graph, is_debug, file, "Unable to read ystl::ULZ data for %s (filename: '%s').", type.name, filename);
  }
  return false;
}

template <typename U, ReservePolicy R> bool Storage::Save (const ystl::Array<U, R> &data, ExtenHeader *exten, StorageOption pass_options) {
  auto type = GuessType<U> ();

  // append additional options
  if (pass_options != 0) {
    type.option |= pass_options;
  }
  const auto is_graph = has_flag (type.option, StorageOption::Graph);

  // do not allow to save graph with less than 8 nodes
  if (is_graph && graph.Length () < kMaxNodeLinks) {
    ctrl.Msg ("Can't save graph data with less than %d nodes. Please add some more before saving.", kMaxNodeLinks);
    return false;
  }
  ystl::String filename = BuildPath (StorageToBotFile (type.option));

  if (data.empty ()) {
    if (is_graph || ctrl.IsDebug ()) {
      ystl::logger.error ("Unable to save %s file. Empty data. (filename: '%s').", type.name, filename);
    }
    return false;
  }
  else if (is_graph) {
    for (auto &path : graph) {
      path.display = 0.0f;
      path.light = illum.GetLightLevel (path.origin);
    }
  }

  // open the file
  ystl::File file (filename, "wb");

  // no open no fun
  if (!file) {
    ystl::logger.error ("Unable to open %s file for writing (filename: '%s').", type.name, filename);
    return false;
  }
  const auto raw_length = data.size () * sizeof (U);
  ystl::Array<uint8_t, ReservePolicy::Proportional> compressed (
    static_cast<size_t> (ystl::ULZ::max_compressed_size (static_cast<int32_t> (raw_length))));

  // swap payload into a staging copy on big-endian as disk layout is little-endian
  ystl::Array<uint8_t, ReservePolicy::Proportional> staging {};
  ystl::Span<const uint8_t> raw_data { reinterpret_cast<const uint8_t *> (data.data ()), raw_length };

  if constexpr (!ystl::ByteOrder::is_little_endian ()) {
    staging.resize (raw_length);
    memcpy (staging.data (), data.data (), raw_length);

    ystl::leio::le_swap (ystl::Span<U> { reinterpret_cast<U *> (staging.data ()), data.size () });
    raw_data = staging;
  }

  // try to compress
  ystl::ULZ ulz {};
  const auto compressed_length = static_cast<size_t> (ulz.compress (raw_data, compressed));

  if (compressed_length > 0) {
    StorageHeader hdr {
      .magic = kStorageMagic,
      .version = static_cast<int32_t> (type.version),
      .options = static_cast<int32_t> (type.option),
      .length = graph.Length (),
      .compressed = static_cast<int32_t> (compressed_length),
      .uncompressed = static_cast<int32_t> (raw_length),
    };

    file.le_write (hdr); // converted to on-disk little-endian layout
    file.write (ystl::Span<uint8_t> { compressed.data (), compressed_length });

    // for visibility tables save counts of stand/count numbers
    if (has_flag (type.option, StorageOption::Vistable)) {
      for (auto &path : graph) {
        file.le_write (path.vis); // converted to on-disk little-endian layout
      }
    }

    // add extension
    if (has_flag (type.option, StorageOption::Exten) && exten != nullptr) {
      file.le_write (*exten); // converted to on-disk little-endian layout
    }

    // notify only about graph, other data is debug-only
    if (is_graph) {
      ctrl.Msg ("Successfully saved Bots %s data.", type.name);
    }
    else {
      ctrl.Debug ("Successfully saved Bots %s data.", type.name);
    }
  }
  else {
    ystl::logger.error ("Unable to compress %s data (filename: '%s').", type.name, filename);
    return false;
  }
  return true;
}

bool Storage::ErrorImpl (bool is_graph, bool is_debug, ystl::MemFile &file, ystl::StringRef message) {
  // display error only for graph file
  if (is_graph || is_debug) {
    ystl::logger.error ("%s", message.chars ());
  }

  // if graph reset paths
  if (is_graph) {
    bots.KickEveryone (true);
    graph.Reset ();
  }
  file.close ();

  return false;
}

template <typename U> Storage::SaveLoadData Storage::GuessType () {
  if constexpr (ystl::is_same_v<U, FloydWarshallAlgo::Matrix>) {
    return { "Pathmatrix", StorageOption::Matrix, StorageVersion::Matrix };
  }
  else if constexpr (ystl::is_same_v<U, Practice::Entry>) {
    return { "Practice", StorageOption::Practice, StorageVersion::Practice };
  }
  else if constexpr (ystl::is_same_v<U, GraphVistable::VisStorage>) {
    return { "Vistable", StorageOption::Vistable, StorageVersion::Vistable };
  }
  else if constexpr (ystl::is_same_v<U, Path>) {
    return { "Graph", StorageOption::Graph, StorageVersion::Graph };
  }
}

ystl::String Storage::BuildPath (StorageFile file, bool is_memory_load, bool without_map_name) {
  using FilePath = ystl::Twin<ystl::String, ystl::String>;

  static ystl::HashMap<StorageFile, FilePath> paths = {
    { StorageFile::Vistable,   FilePath (folders.train,  "vis")   },
    { StorageFile::Practice,   FilePath (folders.train,  "prc")   },
    { StorageFile::Pathmatrix, FilePath (folders.train,  "pmx")   },
    { StorageFile::LogFile,    FilePath (folders.logs,   "txt")   },
    { StorageFile::Graph,      FilePath (folders.graph,  "graph") },
    { StorageFile::PodbotPWF,  FilePath (folders.podbot, "pwf")   }
  };

  ystl::Array<ystl::String> path {};

  // if not memory file we're don't need game dir
  if (is_memory_load) {
    path.emplace (GetRunningPathVfs ());
  }
  else {
    path.emplace (GetRunningPath ());
  }

  // the datadir
  path.emplace (folders.data);

  // append real filepath
  path.emplace (paths[file].first);

  // if file is logfile use correct logfile name with date
  if (file == StorageFile::LogFile) {
    time_t ticks = time (&ticks);
    tm timeinfo {};

    ystl::plat.loctime (&timeinfo, &ticks);
    char timebuf[16] {};
    strftime (timebuf, sizeof timebuf, "L%d%m%Y", &timeinfo);
    path.emplace (ystl::strings.format ("%s_%s.%s", product.name_lower, timebuf, paths[file].second));
  }
  else if (!without_map_name) {
    ystl::String map_name = game.GetMapName ();
    path.emplace (ystl::strings.format ("%s.%s", map_name.lowercase (), paths[file].second));
  }

  // finally use correct path separators for us
  return ystl::String::join (path, kPathSeparator);
}

StorageFile Storage::StorageToBotFile (StorageOption options) {
  // converts storage option to storage filename

  if (has_flag (options, StorageOption::Graph)) {
    return StorageFile::Graph;
  }
  else if (has_flag (options, StorageOption::Matrix)) {
    return StorageFile::Pathmatrix;
  }
  else if (has_flag (options, StorageOption::Vistable)) {
    return StorageFile::Vistable;
  }
  else if (has_flag (options, StorageOption::Practice)) {
    return StorageFile::Practice;
  }
  return StorageFile::Graph;
}

void Storage::UnlinkFromDisk (bool only_training_data, bool silence_messages) {
  // this function removes graph file from the hard disk

  ystl::Array<ystl::String> unlinkable {};
  bots.KickEveryone (true);

  // if we're delete graph, delete all corresponding to it files
  if (!only_training_data) {
    unlinkable.emplace (BuildPath (StorageFile::Graph)); // graph itself
  }
  unlinkable.emplace (BuildPath (StorageFile::Practice)); // corresponding to practice
  unlinkable.emplace (BuildPath (StorageFile::Vistable)); // corresponding to vistable
  unlinkable.emplace (BuildPath (StorageFile::Pathmatrix)); // corresponding to matrix

  for (const auto &item : unlinkable) {
    if (ystl::plat.file_exists (item.chars ())) {
      ystl::plat.remove_file (item.chars ());

      if (!silence_messages) {
        ctrl.Msg ("ystl::File %s, has been deleted from the hard disk", item);
      }
    }
    else if (!silence_messages) {
      ystl::logger.error ("Unable to open %s", item);
    }
  }
  graph.Reset (); // re-initialize points

  if (only_training_data) {
    graph.LoadGraphData ();

    // take bots  back to game
    cv_quota.Revert ();
  }
}

ystl::StringRef Storage::GetRunningPath () {
  // this function get's relative path against bot library (bot library should reside in bin dir)

  static ystl::String path {};

  // we're do not do relative (against bot's library) paths on specific cases
  if (use_non_relative_paths_) {
    if (path.empty ()) {
      path = ystl::strings.join_path (game.GetRunningModName (), folders.addons, folders.bot);
    }
    return path;
  }

  // compute the full path to the our folder
  if (path.empty ()) {
    path = ystl::SharedLibrary::path (&bstor);

    if (path.empty ()) {
      ystl::logger.fatal ("Unable to detect library path. Giving up...");
    }

    // remove dot prefix
    if (path.starts_with (".")) {
      path.ltrim (".\\/");
    }
    auto parts = path.substr (1).split (kPathSeparator);

    parts.pop (); // remove library name
    parts.pop (); // remove bin directory

    path = path.substr (0, 1) + ystl::String::join (parts, kPathSeparator);
  }
  return path;
}

ystl::StringRef Storage::GetRunningPathVfs () {
  static ystl::String path {};

  // we're do not do relative (against bot's library) paths on android
  if (use_non_relative_paths_) {
    if (path.empty ()) {
      path = ystl::strings.join_path (folders.addons, folders.bot);
    }
    return path;
  }

  if (path.empty ()) {
    path = GetRunningPath ();

    path = path.substr (path.rfind (game.GetRunningModName ())); // skip to the game dir
    path = path.substr (path.find (kPathSeparator) + 1); // skip the game dir
  }
  return path;
}

void Storage::CheckInstallLocation () {
  if (ystl::plat.android || ystl::plat.emscripten) {
    use_non_relative_paths_ = true;
    return;
  }
  ystl::String path = ystl::SharedLibrary::path (&bstor);

  if (path.empty ()) {
    use_non_relative_paths_ = true;
    return;
  }
  ystl::String dpath = ystl::strings.join_path (folders.addons, folders.bot, folders.bin);
  path = path.substr (0, path.rfind (kPathSeparator));

  use_non_relative_paths_ = !path.lowercase ().ends_with (dpath.lowercase ());
}

// explicit instantiation definitions for the concrete storage payload types
#define BOT_STORAGE_INSTANTIATE_PAYLOAD(U) \
   template bool Storage::Load<U, ReservePolicy::PowerOfTwo> (ystl::Array<U, ReservePolicy::PowerOfTwo> &, ExtenHeader *, StorageOption *); \
   template bool Storage::Save<U, ReservePolicy::PowerOfTwo> (const ystl::Array<U, ReservePolicy::PowerOfTwo> &, ExtenHeader *, StorageOption); \
   template bool Storage::Load<U, ReservePolicy::Proportional> (ystl::Array<U, ReservePolicy::Proportional> &, ExtenHeader *, StorageOption *); \
   template bool Storage::Save<U, ReservePolicy::Proportional> (const ystl::Array<U, ReservePolicy::Proportional> &, ExtenHeader *, StorageOption);

BOT_STORAGE_INSTANTIATE_PAYLOAD (FloydWarshallAlgo::Matrix)
BOT_STORAGE_INSTANTIATE_PAYLOAD (Practice::Entry)
BOT_STORAGE_INSTANTIATE_PAYLOAD (GraphVistable::VisStorage)
BOT_STORAGE_INSTANTIATE_PAYLOAD (Path)

#undef BOT_STORAGE_INSTANTIATE_PAYLOAD

} // namespace bot
