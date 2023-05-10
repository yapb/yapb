//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

constexpr int kMaxNodes = 4096; // max nodes per graph
constexpr int kMaxNodeLinks = 8; // max links for single node

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
   Bidirectional,
   Jumping
)

// node edit states
CR_DECLARE_SCOPED_ENUM (GraphEdit,
   On = cr::bit (1),
   Noclip = cr::bit (2),
   Auto = cr::bit (3)
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

CR_DECLARE_SCOPED_ENUM (NotifySound,
   Done = 0,
   Change = 1,
   Added = 2
)

#include <vistable.h>

// general waypoint header information structure
struct PODGraphHeader {
   char header[8];
   int32_t fileVersion;
   int32_t pointNumber;
   char mapName[32];
   char author[32];
};

// defines linked waypoints
struct PathLink {
   Vector velocity;
   int32_t distance;
   uint16_t flags;
   int16_t index;
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

// graph operation class
class BotGraph final : public Singleton <BotGraph> {
public:
   friend class Bot;

private:
   int m_editFlags {};
   int m_cacheNodeIndex {};
   int m_lastJumpNode {};
   int m_findWPIndex {};
   int m_facingAtIndex {};
   int m_autoSaveCount {};

   float m_timeJumpStarted {};
   float m_autoPathDistance {};
   float m_pathDisplayTime {};
   float m_arrowDisplayTime {};

   bool m_isOnLadder {};
   bool m_endJumpPoint {};
   bool m_jumpLearnNode {};
   bool m_hasChanged {};
   bool m_narrowChecked {};
   bool m_silenceMessages {};

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

public:
   SmallArray <Path> m_paths {};
   HashMap <int32_t, Array <int32_t>, EmptyHash <int32_t>> m_hashTable;

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
   int getForAnalyzer (const Vector &origin, float maxDistance);
   int getNearest (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getNearestNoBuckets (const Vector &origin, float minDistance = kInfiniteDistance, int flags = -1);
   int getEditorNearest ();
   int clearConnections (int index);
   int getBspSize ();
   int locateBucket (const Vector &pos);

   float calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin);

   bool convertOldFormat ();
   bool isConnected (int a, int b);
   bool isConnected (int index);
   bool isNodeReacheableEx (const Vector &src, const Vector &destination, const float maxHeight);
   bool isNodeReacheable (const Vector &src, const Vector &destination);
   bool isNodeReacheableWithJump (const Vector &src, const Vector &destination);
   bool checkNodes (bool teleportPlayer);
   bool isVisited (int index);
   bool isAnalyzed () const;

   bool saveGraphData ();
   bool loadGraphData ();
   bool canDownload ();

   void saveOldFormat ();
   void reset ();
   void frame ();
   void populateNodes ();
   void initLightLevels ();
   void initNarrowPlaces ();
   void addPath (int addIndex, int pathIndex, float distance, int type);
   void add (int type, const Vector &pos = nullptr);
   void erase (int target);
   void toggleFlags (int toggleFlag);
   void setRadius (int index, float radius);
   void pathCreate (char dir);
   void erasePath ();
   void cachePoint (int index);
   void calculatePathRadius (int index);
   void addBasic ();
   void setSearchIndex (int index);
   void startLearnJump ();
   void setVisited (int index);
   void clearVisited ();
   void initBuckets ();
   void addToBucket (const Vector &pos, int index);
   void eraseFromBucket (const Vector &pos, int index);
   void setBombOrigin (bool reset = false, const Vector &pos = nullptr);
   void unassignPath (int from, int to);
   void convertFromPOD (Path &path, const PODPath &pod);
   void convertToPOD (const Path &path, PODPath &pod);
   void convertCampDirection (Path &path);
   void setAutoPathDistance (const float distance);
   void showStats ();
   void showFileInfo ();
   void emitNotify (int32_t sound);

   IntArray getNarestInRadius (float radius, const Vector &origin, int maxCount = -1);
   const IntArray &getNodesInBucket (const Vector &pos);

public:
   size_t getMaxRouteLength () const {
      return m_paths.length () / 2;
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

   // get the random node on map
   int32_t random () const {
      return rg.get (0, length () - 1);
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

   // slicence all graph messages or not
   void setMessageSilence (bool enable) {
      m_silenceMessages = enable;
   }

   // set exten header from binary storage
   void setExtenHeader (ExtenHeader *hdr) {
      memcpy (&m_extenHeader, hdr, sizeof (ExtenHeader));
   }

   // set graph header from binary storage
   void setGraphHeader (StorageHeader *hdr) {
      memcpy (&m_graphHeader, hdr, sizeof (StorageHeader));
   }

public:
   // graph heloer for sending message to correct channel
   template <typename ...Args> void msg (const char *fmt, Args &&...args);

public:
   Path *begin () {
      return m_paths.begin ();
   }

   Path *begin () const {
      return m_paths.begin ();
   }

   Path *end () {
      return m_paths.end ();
   }

   Path *end () const {
      return m_paths.end ();
   }
};

#include <manager.h>
#include <practice.h>

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (BotGraph, graph);
