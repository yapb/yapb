//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
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
   Sniper = cr::bit (28), // it's a specific sniper point
   TerroristOnly = cr::bit (29), // it's a specific terrorist point
   CTOnly = cr::bit (30),  // it's a specific ct point
)

// defines for node connection flags field (16 bits are available)
CR_DECLARE_SCOPED_ENUM_TYPE (PathFlag, uint16,
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

// defines node add commands
CR_DECLARE_SCOPED_ENUM (GraphAdd,
   Normal = 0,
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
   Author = cr::bit (6) // this is additional flag indicates that there's author info
)

// storage header versions
CR_DECLARE_SCOPED_ENUM (StorageVersion,
   Graph = 1,
   Practice = 1,
   Vistable = 1,
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

// a* route
struct Route {
   float g, f;
   int parent;
   RouteState state;
};

// general stprage header information structure
struct StorageHeader {
   int32 magic;
   int32 version;
   int32 options;
   int32 length;
   int32 compressed;
   int32 uncompressed;
};

// general waypoint header information structure
struct PODGraphHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   char mapName[32];
   char author[32];
};

// floyd-warshall matrices
struct Matrix {
   int16 dist;
   int16 index;
};

// experience data hold in memory while playing
struct Practice {
   int16 damage[kGameTeamNum];
   int16 index[kGameTeamNum];
   int16 value[kGameTeamNum];
};

// defines linked waypoints
struct PathLink {
   Vector velocity;
   int32 distance;
   uint16 flags;
   int16 index;
};

// defines visibility count
struct PathVis {
   uint16 stand, crouch;
};

// define graph path structure for yapb
struct Path {
   int32 number, flags;
   Vector origin, start, end;
   float radius, light, display;
   PathLink links[kMaxNodeLinks];
   PathVis vis;
};

// define waypoint structure for podbot  (will convert on load)
struct PODPath {
   int32 number, flags;
   Vector origin;
   float radius, csx, csy, cex, cey;
   int16 index[kMaxNodeLinks];
   uint16 conflags[kMaxNodeLinks];
   Vector velocity[kMaxNodeLinks];
   int32 distance[kMaxNodeLinks];
   PathVis vis;
};

// this structure links nodes returned from pathfinder
class PathWalk final : public DenyCopying {
private:
   size_t m_cursor = 0;
   Array <int, ReservePolicy::Single, kMaxRouteLength> m_storage;

public:
   explicit PathWalk () = default;
   ~PathWalk () = default;

public:
   int32 &next () {
      return at (1);
   }

   int32 &first () {
      return at (0);
   }

   int32 &last () {
      return m_storage.last ();
   }

   int32 &at (size_t index) {
      return m_storage.at (m_cursor + index);
   }

   void shift () {
      ++m_cursor;
   }

   void reverse () {
      m_storage.reverse ();
   }

   size_t length () const {
      if (m_cursor > m_storage.length ()) {
         return 0;
      }
      return m_storage.length () - m_cursor;
   }

   bool hasNext () const {
      return m_cursor < m_storage.length ();
   }

   bool empty () const {
      return !length ();
   }

   void push (int node) {
      m_storage.push (node);
   }

   void clear () {
      m_cursor = 0;
      m_storage.clear ();
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

   int m_editFlags;
   int m_loadAttempts;
   int m_cacheNodeIndex;
   int m_lastJumpNode;
   int m_findWPIndex;
   int m_facingAtIndex;
   int m_highestDamage[kGameTeamNum];

   float m_timeJumpStarted;
   float m_autoPathDistance;
   float m_pathDisplayTime;
   float m_arrowDisplayTime;

   bool m_isOnLadder;
   bool m_endJumpPoint;
   bool m_jumpLearnNode;
   bool m_hasChanged;
   bool m_needsVisRebuild;

   Vector m_learnVelocity;
   Vector m_learnPosition;
   Vector m_bombOrigin;
   Vector m_lastNode;

   IntArray m_terrorPoints;
   IntArray m_ctPoints;
   IntArray m_goalPoints;
   IntArray m_campPoints;
   IntArray m_sniperPoints;
   IntArray m_rescuePoints;
   IntArray m_visitedGoals;

   SmallArray <int32> m_buckets[kMaxBucketsInsidePos][kMaxBucketsInsidePos][kMaxBucketsInsidePos];
   SmallArray <Matrix> m_matrix;
   SmallArray <Practice> m_practice;
   SmallArray <Path> m_paths;
   SmallArray <uint8> m_vistable;

   String m_tempStrings;
   edict_t *m_editor;

public:
   BotGraph ();
   ~BotGraph () = default;

public:

   int getFacingIndex ();
   int getFarest (const Vector &origin, float maxDistance = 32.0);
   int getNearest (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getNearestNoBuckets (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getEditorNeareset ();
   int getDangerIndex (int team, int start, int goal);
   int getDangerValue (int team, int start, int goal);
   int getDangerDamage (int team, int start, int goal);
   int getPathDist (int srcIndex, int destIndex);
   int clearConnections (int index);

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

   template <typename U> bool saveStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, const SmallArray <U> &data, uint8 *blob);
   template <typename U> bool loadStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, SmallArray <U> &data, uint8 *blob, int32 *outOptions);

   void saveOldFormat ();
   void initGraph ();
   void frame ();
   void loadPractice ();
   void loadVisibility ();
   void initNodesTypes ();
   void initLightLevels ();
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

   const char *getDataDirectory (bool isMemoryFile = false);
   const char *getOldFormatGraphName (bool isMemoryFile = false);

   Bucket locateBucket (const Vector &pos);
   IntArray searchRadius (float radius, const Vector &origin, int maxCount = -1);
   const SmallArray <int32> &getNodesInBucket (const Vector &pos);

public:
   int getHighestDamageForTeam (int team) const {
      return m_highestDamage[team];
   }

   void setHighestDamageForTeam (int team, int value) {
      m_highestDamage[team] = value;
   }

   const char *getAuthor () const {
      return m_tempStrings.chars ();
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

   void setAutoPathDistance (const float distance) {
      m_autoPathDistance = distance;
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
      return index >= 0 && index < static_cast <int> (m_paths.length ());
   }

   // get real nodes num
   int length () const {
      return m_paths.length ();
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

// explose global
static auto &graph = BotGraph::get ();