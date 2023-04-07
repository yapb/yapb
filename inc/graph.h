//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// defines for nodes flags field (32 bits are available)
CR_DECLARE_SCOPED_ENUM (NodeFlag,
   Lift = cr::bit (1), // wait for lift to be down before approaching this node
   Crouch = cr::bit (2), // must crouch to reach this node
   Crossing = cr::bit (3), // a target node
   Goal = cr::bit (4), // mission goal point (bomb, hostage etc.)
   Ladder = cr::bit (5), // node is on ladder
   Rescue = cr::bit (6), // node is a hostage rescue point
   Camp = cr::bit (7), // node is a camping point
   NoHostage = cr::bit (8), // only use this node if no hostage
   DoubleJump = cr::bit (9), // bot help's another bot (requster) to get somewhere (using djump)
   Narrow = cr::bit (10), // node is inside some small space (corridor or such)
   Sniper = cr::bit (28), // it's a specific sniper point
   TerroristOnly = cr::bit (29), // it's a specific terrorist point
   CTOnly = cr::bit (30),  // it's a specific ct point
)

// defines for node connection flags field (16 bits are available)
CR_DECLARE_SCOPED_ENUM_TYPE (PathFlag, uint16_t,
   Jump = cr::bit (0) // must jump for this connection
)

// enum pathfind search type
CR_DECLARE_SCOPED_ENUM (FindPath,
   Fast = 0,
   Optimal,
   Safe
)

// defines node connection types
CR_DECLARE_SCOPED_ENUM (PathConnection,
   Outgoing = 0,
   Incoming,
   Bidirectional
)

// a* route state
CR_DECLARE_SCOPED_ENUM (RouteState,
   Open = 0,
   Closed,
   New
)

// node edit states
CR_DECLARE_SCOPED_ENUM (GraphEdit,
   On = cr::bit (1),
   Noclip = cr::bit (2),
   Auto = cr::bit (3)
)

// storage header options
CR_DECLARE_SCOPED_ENUM (StorageOption,
   Practice = cr::bit (0), // this is practice (experience) file
   Matrix = cr::bit (1), // this is floyd warshal path & distance matrix
   Vistable = cr::bit (2), // this is vistable data
   Graph = cr::bit (3), // this is a node graph data
   Official = cr::bit (4), // this is additional flag for graph indicates graph are official
   Recovered = cr::bit (5), // this is additional flag indicates graph converted from podbot and was bad
   Exten = cr::bit (6) // this is additional flag indicates that there's extension info
)

// storage header versions
CR_DECLARE_SCOPED_ENUM (StorageVersion,
   Graph = 2,
   Practice = 1,
   Vistable = 2,
   Matrix = 1,
   Podbot = 7
)

// lift usage states
CR_DECLARE_SCOPED_ENUM (LiftState,
   None = 0,
   LookingButtonOutside,
   WaitingFor,
   EnteringIn,
   WaitingForTeammates,
   LookingButtonInside,
   TravelingBy,
   Leaving
)

// node add flags
CR_DECLARE_SCOPED_ENUM (NodeAddFlag,
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
)

// a* route
struct Route {
   float g, f;
   int parent;
   RouteState state;
};

// general stprage header information structure
struct StorageHeader {
   int32_t magic;
   int32_t version;
   int32_t options;
   int32_t length;
   int32_t compressed;
   int32_t uncompressed;
};

// extension header for graph information
struct ExtenHeader {
   char author[32]; // original author of graph
   int32_t mapSize; // bsp size for checksumming map consistency
   char modified[32]; // by whom modified
};

// general waypoint header information structure
struct PODGraphHeader {
   char header[8];
   int32_t fileVersion;
   int32_t pointNumber;
   char mapName[32];
   char author[32];
};

// floyd-warshall matrices
struct Matrix {
   int16_t dist;
   int16_t index;
};

// experience data hold in memory while playing
struct Practice {
   int16_t damage[kGameTeamNum];
   int16_t index[kGameTeamNum];
   int16_t value[kGameTeamNum];
};

// defines linked waypoints
struct PathLink {
   Vector velocity;
   int32_t distance;
   uint16_t flags;
   int16_t index;
};

// defines visibility count
struct PathVis {
   uint16_t stand, crouch;
};

// define graph path structure for yapb
struct Path {
   int32_t number, flags;
   Vector origin, start, end;
   float radius, light, display;
   PathLink links[kMaxNodeLinks];
   PathVis vis;
};

// define waypoint structure for podbot  (will convert on load)
struct PODPath {
   int32_t number, flags;
   Vector origin;
   float radius, csx, csy, cex, cey;
   int16_t index[kMaxNodeLinks];
   uint16_t conflags[kMaxNodeLinks];
   Vector velocity[kMaxNodeLinks];
   int32_t distance[kMaxNodeLinks];
   PathVis vis;
};

// this structure links nodes returned from pathfinder
class PathWalk final : public DenyCopying {
private:
   size_t m_cursor {};
   size_t m_length {};

   UniquePtr <int32_t[]> m_path {};

public:
   explicit PathWalk () = default;
   ~PathWalk () = default;

public:
   int32_t &next () {
      return at (1);
   }

   int32_t &first () {
      return at (0);
   }

   int32_t &last () {
      return at (length () - 1);
   }

   int32_t &at (size_t index) {
      return m_path[m_cursor + index];
   }

   void shift () {
      ++m_cursor;
   }

   void reverse () {
      for (size_t i = 0; i < m_length / 2; ++i) {
         cr::swap (m_path[i], m_path[m_length - 1 - i]);
      }
   }

   size_t length () const {
      if (m_cursor >= m_length) {
         return 0;
      }
      return m_length - m_cursor;
   }

   bool hasNext () const {
      return length () > m_cursor;
   }

   bool empty () const {
      return !length ();
   }

   void add (int32_t node) {
      m_path[m_length++] = node;
   }

   void clear () {
      m_cursor = 0;
      m_length = 0;

      m_path[0] = 0;
   }

   void init (size_t length) {
      m_path = cr::makeUnique <int32_t []> (length);
   }
};

// graph operation class
class BotGraph final : public Singleton <BotGraph> {
public:
   friend class Bot;

private:
   struct Bucket {
      int x, y, z;
   };

   int m_editFlags {};
   int m_loadAttempts {};
   int m_cacheNodeIndex {};
   int m_lastJumpNode {};
   int m_findWPIndex {};
   int m_facingAtIndex {};
   int m_highestDamage[kGameTeamNum] {};
   int m_autoSaveCount {};

   float m_timeJumpStarted {};
   float m_autoPathDistance {};
   float m_pathDisplayTime {};
   float m_arrowDisplayTime {};

   bool m_isOnLadder {};
   bool m_endJumpPoint {};
   bool m_jumpLearnNode {};
   bool m_hasChanged {};
   bool m_needsVisRebuild {};
   bool m_narrowChecked {};

   Vector m_learnVelocity {};
   Vector m_learnPosition {};
   Vector m_bombOrigin {};
   Vector m_lastNode {};

   IntArray m_terrorPoints {};
   IntArray m_ctPoints {};
   IntArray m_goalPoints {};
   IntArray m_campPoints {};
   IntArray m_sniperPoints {};
   IntArray m_rescuePoints {};
   IntArray m_visitedGoals {};

   SmallArray <int32_t> m_buckets[kMaxBucketsInsidePos][kMaxBucketsInsidePos][kMaxBucketsInsidePos];
   SmallArray <Matrix> m_matrix {};
   SmallArray <Practice> m_practice {};
   SmallArray <Path> m_paths {};
   SmallArray <uint8_t> m_vistable {};

   String m_graphAuthor {};
   String m_graphModified {};

   ExtenHeader m_extenHeader {};
   StorageHeader m_graphHeader {};

   edict_t *m_editor {};

public:
   BotGraph ();
   ~BotGraph () = default;

public:
   int getFacingIndex ();
   int getFarest (const Vector &origin, float maxDistance = 32.0);
   int getNearest (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getNearestNoBuckets (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getEditorNearest ();
   int getDangerIndex (int team, int start, int goal);
   int getDangerValue (int team, int start, int goal);
   int getDangerDamage (int team, int start, int goal);
   int getPathDist (int srcIndex, int destIndex);
   int clearConnections (int index);
   int getBspSize ();

   float calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin);

   bool convertOldFormat ();
   bool isVisible (int srcIndex, int destIndex);
   bool isStandVisible (int srcIndex, int destIndex);
   bool isDuckVisible (int srcIndex, int destIndex);
   bool isConnected (int a, int b);
   bool isConnected (int index);
   bool isNodeReacheable (const Vector &src, const Vector &destination);
   bool checkNodes (bool teleportPlayer);
   bool loadPathMatrix ();
   bool isVisited (int index);

   bool saveGraphData ();
   bool loadGraphData ();
   bool canDownload ();

   template <typename U> bool saveStorage (StringRef ext, StringRef name, StorageOption options, StorageVersion version, const SmallArray <U> &data, ExtenHeader *exten);
   template <typename U> bool loadStorage (StringRef ext, StringRef name, StorageOption options, StorageVersion version, SmallArray <U> &data, ExtenHeader *exten, int32_t *outOptions);
   template <typename ...Args> bool raiseLoadingError (bool isGraph, MemFile &file, const char *fmt, Args &&...args);

   void saveOldFormat ();
   void reset ();
   void frame ();
   void loadPractice ();
   void loadVisibility ();
   void initNodesTypes ();
   void initLightLevels ();
   void initNarrowPlaces ();
   void addPath (int addIndex, int pathIndex, float distance);
   void add (int type, const Vector &pos = nullptr);
   void erase (int target);
   void toggleFlags (int toggleFlag);
   void setRadius (int index, float radius);
   void rebuildVisibility ();
   void pathCreate (char dir);
   void erasePath ();
   void cachePoint (int index);
   void calculatePathRadius (int index);
   void savePractice ();
   void saveVisibility ();
   void addBasic ();
   void eraseFromDisk ();
   void savePathMatrix ();
   void setSearchIndex (int index);
   void startLearnJump ();
   void setVisited (int index);
   void clearVisited ();
   void initBuckets ();
   void addToBucket (const Vector &pos, int index);
   void eraseFromBucket (const Vector &pos, int index);
   void setBombOrigin (bool reset = false, const Vector &pos = nullptr);
   void updateGlobalPractice ();
   void unassignPath (int from, int to);
   void setDangerValue (int team, int start, int goal, int value);
   void setDangerDamage (int team, int start, int goal, int value);
   void convertFromPOD (Path &path, const PODPath &pod);
   void convertToPOD (const Path &path, PODPath &pod);
   void convertCampDirection (Path &path);
   void setAutoPathDistance (const float distance);
   void showStats ();
   void showFileInfo ();

   const char *getDataDirectory (bool isMemoryFile = false);
   const char *getOldFormatGraphName (bool isMemoryFile = false);

   Bucket locateBucket (const Vector &pos);
   IntArray searchRadius (float radius, const Vector &origin, int maxCount = -1);
   const SmallArray <int32_t> &getNodesInBucket (const Vector &pos);

public:
   size_t getMaxRouteLength () const {
      return m_paths.length () / 2;
   }

   int getHighestDamageForTeam (int team) const {
      return m_highestDamage[team];
   }

   void setHighestDamageForTeam (int team, int value) {
      m_highestDamage[team] = value;
   }

   StringRef getAuthor () const {
      return m_graphAuthor;
   }

   StringRef getModifiedBy () const {
      return m_graphModified;
   }

   bool hasChanged () const {
      return m_hasChanged;
   }

   bool hasEditFlag (int flag) const {
      return !!(m_editFlags & flag);
   }

   void setEditFlag (int flag) {
      m_editFlags |= flag;
   }

   void clearEditFlag (int flag) {
      m_editFlags &= ~flag;
   }

   const Vector &getBombOrigin () const {
      return m_bombOrigin;
   }

   // access paths
   Path &operator [] (int index) {
      return m_paths[index];
   }

   // check nodes range
   bool exists (int index) const {
      return index >= 0 && index < length ();
   }

   // get real nodes num
   int32_t length () const {
      return m_paths.length <int32_t> ();
   }

   // check if has editor
   bool hasEditor () const {
      return !!m_editor;
   }

   // set's the node editor
   void setEditor (edict_t *ent) {
      m_editor = ent;
   }

   // get the current node editor
   edict_t *getEditor () {
      return m_editor;
   }
};

// we're need `bots`
#include <manager.h>

// helper for reporting load errors
template <typename ...Args> bool BotGraph::raiseLoadingError (bool isGraph, MemFile &file, const char *fmt, Args &&...args) {
   auto result = strings.format (fmt, cr::forward <Args> (args)...);
   logger.error (result);

   // if graph reset paths
   if (isGraph) {
      bots.kickEveryone (true);

      m_graphAuthor = result;
      m_paths.clear ();
   }
   file.close ();

   return false;
}

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (BotGraph, graph);
