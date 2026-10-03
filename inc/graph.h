//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

namespace bot {

constexpr int kMaxNodes = 4096; // max nodes per graph
constexpr int kMaxNodeLinks = 8; // max links for single node

// defines for nodes flags field (32 bits are available)
enum class NodeFlag : int32_t {
  Invalid = -1, // invalid node flag
  Button = ystl::bit (0), // use a nearby button (lifts, doors, etc.)
  Lift = ystl::bit (1), // wait for lift to be down before approaching this node
  Crouch = ystl::bit (2), // must crouch to reach this node
  Crossing = ystl::bit (3), // a target node
  Goal = ystl::bit (4), // mission goal point (bomb, hostage etc.)
  Ladder = ystl::bit (5), // node is on ladder
  Rescue = ystl::bit (6), // node is a hostage rescue point
  Camp = ystl::bit (7), // node is a camping point
  NoHostage = ystl::bit (8), // only use this node if no hostage
  DoubleJump = ystl::bit (9), // bot help's another bot (requster) to get somewhere (using djump)
  Narrow = ystl::bit (10), // node is inside some small space (corridor or such)
  Sniper = ystl::bit (28), // it's a specific sniper point
  TerroristOnly = ystl::bit (29), // it's a specific terrorist point
  CTOnly = ystl::bit (30), // it's a specific ct point
};
YSTL_ENABLE_ENUM_FLAGS (NodeFlag);

// defines for node connection flags field (16 bits are available)
enum class PathFlag : uint16_t {
  None = 0,
  Jump = ystl::bit (0) // must jump for this connection
};
YSTL_ENABLE_ENUM_FLAGS (PathFlag);

// enum pathfind search type
enum class FindPathType : int32_t {
  Fast = 0,
  Optimal,
  Safe,
  Diversity // uses random heuristics for route variety
};

// defines node connection types
enum class PathConnection : int32_t {
  Outgoing = 0,
  Incoming,
  Bidirectional,
  Jumping
};

// node edit states
enum class GraphEdit : int32_t {
  On = ystl::bit (1),
  Noclip = ystl::bit (2),
  Auto = ystl::bit (3),
  Off = 0
};
YSTL_ENABLE_ENUM_FLAGS (GraphEdit);

// lift usage states
enum class LiftState : int32_t {
  None = 0,
  LookingButtonOutside,
  WaitingFor,
  EnteringIn,
  WaitingForTeammates,
  LookingButtonInside,
  TravelingBy,
  Leaving
};

// node add flags
enum class NodeAddFlag : int32_t {
  Normal = 0,
  TOnly = 1,
  CTOnly = 2,
  NoHostage = 3,
  Rescue = 4,
  Camp = 5,
  CampEnd = 6,
  JumpStart = 9,
  JumpEnd = 10,
  Goal = 100
};

enum class NotifySound : int32_t {
  Done = 0,
  Change = 1,
  Added = 2
};

// point types for indexed access
enum class PointType : int32_t {
  Terrorist = 0,
  CT,
  Goal,
  Camp,
  Sniper,
  Rescue,
  Count
};

} // namespace bot

#include "vistable.h"

namespace bot {

// general waypoint header information structure for podbot
struct LegacyHeader {
  char header[8] {};
  int32_t file_version {};
  int32_t point_number {};
  char map_name[32] {};
  char author[32] {};
};

YSTL_LE_FIELDS (LegacyHeader, header, fileVersion, pointNumber, mapName, author);

// defines linked nodes
struct PathLink {
  ystl::Vector velocity {};
  int32_t distance {};
  uint16_t flags {};
  int16_t index {};
};

YSTL_LE_FIELDS (PathLink, velocity, distance, flags, index);

// define graph path structure for yapb
struct Path {
  int32_t number {}, flags {};
  ystl::Vector origin {}, start {}, end {};
  float radius {}, light {}, display {};
  PathLink links[kMaxNodeLinks] {};
  PathVis vis {};
};

YSTL_LE_FIELDS (Path, number, flags, origin, start, end, radius, light, display, links, vis);

// define waypoint structure for podbot (will convert on load)
struct LegacyPath {
  int32_t number {}, flags {};
  ystl::Vector origin {};
  float radius {}, csx {}, csy {}, cex {}, cey {};
  int16_t index[kMaxNodeLinks] {};
  uint16_t conflags[kMaxNodeLinks] {};
  ystl::Vector velocity[kMaxNodeLinks] {};
  int32_t distance[kMaxNodeLinks] {};
  PathVis vis {};
};

YSTL_LE_FIELDS (LegacyPath, number, flags, origin, radius, csx, csy, cex, cey, index, conflags, velocity, distance, vis);

// general storage header information structure
struct StorageHeader {
  int32_t magic {};
  int32_t version {};
  int32_t options {};
  int32_t length {};
  int32_t compressed {};
  int32_t uncompressed {};
};

YSTL_LE_FIELDS (StorageHeader, magic, version, options, length, compressed, uncompressed);

// extension header for graph information
struct ExtenHeader {
  char author[32] {}; // original author of graph
  int32_t map_size {}; // bsp size for checksumming map consistency
  char modified[32] {}; // by whom modified
};

YSTL_LE_FIELDS (ExtenHeader, author, mapSize, modified);

// single place that centralizes every graph database endpoints
class GraphUrlResolver final : public ystl::Singleton<GraphUrlResolver> {
public:
  // well-known cvar aliases
  static constexpr ystl::StringRef kAliasGithub = "@github";
  static constexpr ystl::StringRef kAliasRussia = "@russia";
  static constexpr ystl::StringRef kAliasWorkers = "@workers";
  static constexpr ystl::StringRef kAliasHttp = "@http";
  static constexpr ystl::StringRef kAliasLegacy = "@legacy";

  // canonical endpoint literals behind the aliases
  static constexpr ystl::StringRef kGithubDownloadUrl = "https://raw.githubusercontent.com/yapb/graph/refs/heads/master";
  static constexpr ystl::StringRef kRussiaDownloadUrl = "https://sourcecraft.dev/api/file/yapb/graph";

  static constexpr ystl::StringRef kRussiaWorkerUrl = "https://d5d5lttn6kg3uqou32ei.g4vq2kuy.apigw.yandexcloud.net";
  static constexpr ystl::StringRef kWorkersBaseUrl = "https://graph-worker.yapb.workers.dev";

  static constexpr ystl::StringRef kLegacyBaseUrl = "http://yapb.jeefo.net";
  static constexpr ystl::StringRef kLegacyUploadUrl = "http://yapb.jeefo.net/upload";

  // connectivity probe default (plain tcp host:port, neutral always-on host)
  static constexpr ystl::StringRef kConnectivityHost = "google.com:443";

public:
  GraphUrlResolver () = default;
  ~GraphUrlResolver () = default;

  // expands aliases, keeps full urls as-is, maps legacy bare hosts to http. pure, never warns
  ystl::String Expand (ystl::StringRef raw);

  // effective bases honoring the tls capability of this build (may warn once)
  ystl::String DownloadBase ();
  ystl::String UploadBase ();

  // final urls. empty when the feature is disabled or unavailable on this build
  ystl::String DownloadUrl (ystl::StringRef map_name);
  ystl::String UploadUrl ();
  ystl::String CollectUrl (ystl::StringRef have_csv);

  bool CanDownload ();
  bool CanCollect ();

  // human-readable text for HttpClientResult::HttpOnly in user messages
  const char *HttpsUnsupportedText () {
    return "https is unsupported by this build (no TLS)";
  }

private:
  enum class Alias {
    None,
    Github,
    Russia,
    Workers,
    Http,
    Unknown,
  };

  static bool MatchAlias (ystl::StringRef raw, Alias *out_alias, ystl::String *out_url);
  static ystl::String ExpandImpl (ystl::StringRef raw, Alias *out_alias);
  static void WarnNoTlsOnce (ystl::StringRef feature, ystl::StringRef configured, ystl::StringRef note);

  // endpoint literal from custom.cfg (GraphDatabase section), code fallback if unset
  static ystl::String Endpoint (ystl::StringRef key, ystl::StringRef fallback);
};

// graph operation class
class Graph final : public ystl::Singleton<Graph> {
public:
  friend class Bot;
  friend struct GraphHook;

private:
  GraphEdit edit_flags_ {};

  int cache_node_index_ {};
  int last_jump_node_ {};
  int find_wp_index_ {};
  int facing_at_index_ {};
  int auto_save_count_ {};

  float time_jump_started_ {};
  float auto_path_distance_ {};

  ystl::CountdownTimer path_display_timer_ {};
  ystl::IntervalTimer arrow_display_timer_ {};

  bool is_on_ladder_ {};
  bool end_jump_point_ {};
  bool jump_learn_node_ {};
  bool has_changed_ {};
  bool narrow_checked_ {};
  bool silence_messages_ {};

  ystl::Atomic<bool> light_checked_ {};
  ystl::Atomic<bool> is_online_collected_ {};

  ystl::Vector learn_velocity_ {};
  ystl::Vector learn_position_ {};
  ystl::Vector last_node_ {};

  // free link slot check, jump links never evict walk links
  bool HasFreeLinkSlot (int index) const;

  // true when the ascent can be walked with engine step height
  bool IsWalkableSlope (const ystl::Vector &src, const ystl::Vector &dst) const;

  // true when the drop can be walked off without takeoff
  bool IsWalkableDrop (const ystl::Vector &src, const ystl::Vector &dst) const;

  ystl::SmallArray<int32_t> points_[ystl::to_underlying (PointType::Count)];
  ystl::SmallArray<int32_t> visited_goals_ {};

  ystl::Array<int32_t> node_numbers_ {};

public:
  ystl::Array<Path, ReservePolicy::Proportional> paths_ {};
  ystl::HashMap<int32_t, ystl::Array<int32_t>, ystl::IdentityHash<int32_t>> hash_table_ {};

  struct GraphInfo {
    ystl::String author {};
    ystl::String modified {};
    ExtenHeader exten {};
    StorageHeader header {};
  } info_ {};

  edict_t *editor_ {};

public:
  Graph ();
  ~Graph () = default;

public:
  int GetFacingIndex ();
  int GetFarest (const ystl::Vector &origin, const float max_range = 32.0);
  int GetForAnalyzer (const ystl::Vector &origin, const float max_range);
  // main thread only, mutates buckets hash map
  int GetNearest (const ystl::Vector &origin, const float range = kInfiniteDistance, NodeFlag flags = NodeFlag::Invalid);

  // worker-safe variant
  int GetNearestNoBuckets (const ystl::Vector &origin, const float range = kInfiniteDistance, NodeFlag flags = NodeFlag::Invalid);
  int GetEditorNearest (const float max_range = 50.0f);
  int ClearConnections (int index);
  int GetBspSize ();
  int LocateBucket (const ystl::Vector &pos) const;

  float CalculateTravelTime (float max_speed, const ystl::Vector &src, const ystl::Vector &origin);

  bool ConvertOldFormat ();
  bool IsConnected (int a, int b);
  bool IsConnected (int index);
  bool IsNodeReacheableEx (const ystl::Vector &src, const ystl::Vector &destination, const float max_height) const;
  bool IsNodeReacheable (const ystl::Vector &src, const ystl::Vector &destination) const;
  bool IsNodeReacheableWithJump (const ystl::Vector &src, const ystl::Vector &destination) const;
  bool CheckNodes (bool teleport_player, bool only_paths = false);

  // forward/backward reachability from node 0 over current links
  void ComputeForwardReachability (ystl::Array<bool> &forward);
  void ComputeBackwardReachability (ystl::Array<bool> &backward);
  void ComputeReachability (ystl::Array<bool> &forward, ystl::Array<bool> &backward);
  bool IsVisited (int index);

  bool SaveGraphData ();
  bool LoadGraphData ();
  bool CanDownload ();
  bool IsAnalyzed () const;
  bool IsConverted () const;
  bool IsImported () const;

  // text (conf format) export/import of the graph, for human-friendly version control
  bool ExportGraphText ();
  bool ImportGraphText (ystl::StringRef file_path);

  void SaveOldFormat ();
  void Reset ();
  void Frame ();
  void PopulateNodes ();
  void SyncInitLightLevels ();
  void InitLightLevels ();
  void InitNarrowPlaces ();
  void AddPath (int add_index, int path_index, float distance);
  void Add (NodeAddFlag type, const ystl::Vector &pos = nullptr);
  void Erase (int target);

  // flag an existing link as jump, zero takeoff wayzone
  void FlagJumpLink (int from, int to);
  void ToggleFlags (NodeFlag toggle_flag);
  void SetRadius (int index, float radius);
  void PathCreate (PathConnection dir);
  void ErasePath ();
  void ResetPath (int index);
  void CachePoint (int index);
  void CalculatePathRadius (int index);
  void SeedBasicNodes ();
  void SetSearchIndex (int index);
  void StartLearnJump ();
  void SetVisited (int index);
  void ClearVisited ();

  // assembles a single node into the text document (see exportgraphtext)
  void ExportNode (ystl::ConfNode &parent, const Path &path);

  void EraseFromBucket (const ystl::Vector &pos, int index);
  void UnassignPath (int from, int to);
  void ConvertFromLegacy (Path &path, const LegacyPath &pod) const;
  void ConvertToLegacy (const Path &path, LegacyPath &pod);
  void ConvertCampDirection (Path &path) const;
  void SetAutoPathDistance (const float distance);
  void ShowStats ();
  void ShowFileInfo ();
  void EmitNotify (NotifySound sound) const;
  void SyncCollectOnline ();
  void CollectOnline ();

  ystl::SmallArray<int32_t> GetNearestInRadius (const float radius, const ystl::Vector &origin, int max_count = -1);

  // helper to convert nodeflag to pointtype
  static PointType FlagToPointType (NodeFlag flag);

  // get random point of specified type
  int32_t GetRandomPoint (PointType type) const {
    return points_[ystl::to_underlying (type)].random ();
  }

public:
  ystl::StringRef GetAuthor () const {
    return info_.author;
  }

  ystl::StringRef GetModifiedBy () const {
    return info_.modified;
  }

  bool HasChanged () const {
    return has_changed_;
  }

  bool HasEditFlag (GraphEdit flag) const {
    return has_flag (edit_flags_, flag);
  }

  void SetEditFlag (GraphEdit flag) {
    edit_flags_ |= flag;
  }

  void ClearEditFlag (GraphEdit flag) {
    edit_flags_ &= ~flag;
  }

  // access paths
  Path &operator[] (int index) {
    return paths_[index];
  }

  // check nodes range
  bool Exists (int32_t index) const {
    return index >= 0 && index < paths_.size<int32_t> ();
  }

  // get real nodes num
  int32_t Length () const {
    return paths_.size<int32_t> ();
  }

  // get the random node on map
  int32_t Random () const {
    return ystl::rg (0, Length () - 1);
  }

  // check if has editor
  bool HasEditor () const {
    return !!editor_;
  }

  // set's the node editor
  void SetEditor (edict_t *ent) {
    editor_ = ent;
  }

  // get the current node editor
  edict_t *GetEditor () {
    return editor_;
  }

  // silence all graph messages or not
  void SetMessageSilence (bool enable) {
    silence_messages_ = enable;
  }

  // set exten header from binary storage
  void SetExtenHeader (ExtenHeader *hdr) {
    memcpy (&info_.exten, hdr, sizeof (ExtenHeader));
  }

  // set graph header from binary storage
  void SetGraphHeader (StorageHeader *hdr) {
    memcpy (&info_.header, hdr, sizeof (StorageHeader));
  }

  // gets the node numbers
  const ystl::Array<int32_t> &GetNodeNumbers () {
    return node_numbers_;
  }

  // access points by type
  ystl::SmallArray<int32_t> &GetPoints (PointType type) {
    return points_[ystl::to_underlying (type)];
  }

  const ystl::SmallArray<int32_t> &GetPoints (PointType type) const {
    return points_[ystl::to_underlying (type)];
  }

  // reinitialize buckets
  void InitBuckets () {
    hash_table_.zap ();
  }

  // get the bucket of nodes near position, read-only, returns nullptr when bucket is missing
  const ystl::Array<int32_t> *GetNodesInBucket (const ystl::Vector &pos) {
    return hash_table_.find (LocateBucket (pos));
  }

  // add a node to position bucket
  void AddToBucket (const ystl::Vector &pos, int index) {
    hash_table_[LocateBucket (pos)].emplace (index);
  }

public:
  // graph helper for sending message to correct channel
  template <typename... Args> void Msg (const char *fmt, Args &&...args);

  // check if debug output for given level is enabled (also respects silence)
  bool IsDebug (int level = 1) const;

  // unified debug output: forwards to msg () only when yb_debug >= level
  template <typename... Args> void Debug (const char *fmt, Args &&...args);
  template <typename... Args> void Debug (int level, const char *fmt, Args &&...args);

public:
  Path *begin () {
    return paths_.begin ();
  }

  const Path *begin () const {
    return paths_.begin ();
  }

  Path *end () {
    return paths_.end ();
  }

  const Path *end () const {
    return paths_.end ();
  }
};

} // namespace bot

#include "practice.h"
#include "control.h"

namespace bot {

// graph helper for sending message to correct channel
template <typename... Args> inline void Graph::Msg (const char *fmt, Args &&...args) {
  if (silence_messages_) {
    return; // no messages while analyzing (too much spam)
  }
  Control::instance ().Msg (fmt, ystl::forward<Args> (args)...);
}

inline bool Graph::IsDebug (int level) const {
  return !silence_messages_ && Control::instance ().IsDebug (level);
}

template <typename... Args> inline void Graph::Debug (const char *fmt, Args &&...args) {
  debug (1, fmt, ystl::forward<Args> (args)...);
}

template <typename... Args> inline void Graph::Debug (int level, const char *fmt, Args &&...args) {
  if (IsDebug (level)) {
    msg (fmt, ystl::forward<Args> (args)...);
  }
}

// Mode 2x2 plugin walls (classname "test_effect", solid toggles BBOX/NOT).
// Auto-detected at runtime, no cvar.
class ModeWalls final : public ystl::Singleton<ModeWalls> {
public:
  static constexpr int kMaxWalls = 64;

  struct Box {
    ystl::Vector mins {}, maxs {};
  };

private:
  Box boxes_[kMaxWalls] {};
  Box shadow_[kMaxWalls] {};
  int shadow_count_ {};

  int count_ {};

  bool probed_ {};
  ystl::IntervalTimer scan_timer_ {};
  ystl::String classname_ {};

public:
  void Reset ();
  void OnRoundStart ();
  void Frame ();

  bool HasWalls () const;
  bool IsNodeBlocked (const ystl::Vector &point) const;
  bool IsSegmentBlocked (const ystl::Vector &a, const ystl::Vector &b) const;

private:
  void Rescan (bool force);
  void Publish (const Box *boxes, int count);
  ystl::StringRef WallClassname ();
  static bool SegmentHitsBox (const ystl::Vector &a, const ystl::Vector &b, const ystl::Vector &mins, const ystl::Vector &maxs);
};

// expose globals
YSTL_EXPOSE_GLOBAL_SINGLETON (Graph, graph);
YSTL_EXPOSE_GLOBAL_SINGLETON (ModeWalls, mode_walls);
YSTL_EXPOSE_GLOBAL_SINGLETON (GraphUrlResolver, graph_urls);

} // namespace bot
