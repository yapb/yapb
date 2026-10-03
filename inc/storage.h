//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// storage file magic (podbot)
namespace bot {

constexpr char kPodbotMagic[8] = { 'P', 'O', 'D', 'W', 'A', 'Y', '!', ystl::kNullChar };

constexpr int32_t kStorageMagic = 0x59415042; // storage magic for yapb-data files
constexpr int32_t kStorageMagicUB = 0x544f4255; // support also the fork format (merged back into yapb)

// storage header options
enum class StorageOption : int32_t {
  Invalid = 0, // invalid storage option by default
  Practice = ystl::bit (0), // this is practice (experience) file
  Matrix = ystl::bit (1), // this is floyd warshal path & distance matrix
  Vistable = ystl::bit (2), // this is vistable data
  Graph = ystl::bit (3), // this is a node graph data
  Official = ystl::bit (4), // this is additional flag for graph indicates graph are official
  Recovered = ystl::bit (5), // this is additional flag indicates graph converted from podbot and was bad
  Exten = ystl::bit (6), // this is additional flag indicates that there's extension info
  Analyzed = ystl::bit (7), // this graph has been analyzed
  Converted = ystl::bit (8), // converted from a pwf format
  Imported = ystl::bit (9) // imported from the text (conf) graph format
};
YSTL_ENABLE_ENUM_FLAGS (StorageOption);

// storage header versions
enum class StorageVersion : int32_t {
  Graph = 2,
  Practice = 6,
  Vistable = 5,
  Matrix = 2,
  Podbot = 7
};
YSTL_ENABLE_ENUM_HASH (StorageVersion);

enum class StorageFile : uint32_t {
  Vistable = 0,
  LogFile = 1,
  Practice = 2,
  Graph = 3,
  Pathmatrix = 4,
  PodbotPWF = 5
};
YSTL_ENABLE_ENUM_HASH (StorageFile);

class Storage final : public ystl::Singleton<Storage> {
private:
  struct SaveLoadData {
    ystl::String name {};
    StorageOption option {};
    StorageVersion version {};

  public:
    SaveLoadData (ystl::StringRef name, StorageOption option, StorageVersion version) : name (name), option (option), version (version) {}
  };

private:
  int retries_ {};
  bool use_non_relative_paths_ {};

public:
  Storage () = default;
  ~Storage () = default;

public:
  // converts type to save/load options
  template <typename U> SaveLoadData GuessType ();

  // loads the data and decompress ulz
  template <typename U, ReservePolicy R = ReservePolicy::PowerOfTwo>
  bool Load (ystl::Array<U, R> &data, ExtenHeader *exten = nullptr, StorageOption *out_options = nullptr);

  // saves the data and compress with ulz
  template <typename U, ReservePolicy R = ReservePolicy::PowerOfTwo>
  bool Save (const ystl::Array<U, R> &data, ExtenHeader *exten = nullptr, StorageOption pass_options = StorageOption::Invalid);

  // report fatal error loading stuff (non-template core, defined in storage.cpp)
  bool ErrorImpl (bool is_graph, bool is_debug, ystl::MemFile &file, ystl::StringRef message);

  // report fatal error loading stuff (formats message, forwards to impl)
  template <typename... Args> bool Error (bool is_graph, bool is_debug, ystl::MemFile &file, const char *fmt, Args &&...args) {
    return ErrorImpl (is_graph, is_debug, file, ystl::strings.format (fmt, ystl::forward<Args> (args)...));
  }

  // builds the filename to requested filename
  ystl::String BuildPath (StorageFile type, bool is_memory_load = false, bool without_map_name = false);

  // get's relative path against bot library (bot library should reside in bin dir)
  ystl::StringRef GetRunningPath ();

  // same as above, but with valve-specific filesystem paths (loadfileforme)
  ystl::StringRef GetRunningPathVfs ();

  // converts storage option to storage filename
  StorageFile StorageToBotFile (StorageOption options);

  // remove all bot related files from disk
  void UnlinkFromDisk (bool only_training_data, bool silence_messages);

  // is correctly installed and running from correct folder?
  void CheckInstallLocation ();

public:
  // loading the graph may attempt to recurse loading, with converting or download, reset retry counter
  void ResetRetries () {
    retries_ = 0;
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Storage, bstor);

} // namespace bot
