//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// amxx module api version, packed as major << 16 | minor.
namespace bot {

constexpr int kBotModuleApiMajor = 3;
constexpr int kBotModuleApiMinor = 0;
constexpr int kBotModuleVersion = (kBotModuleApiMajor << 16) | kBotModuleApiMinor;

// basic module interface, if you need to additional stuff, please post an issue
class IBotModule {
public:
  virtual ~IBotModule () = default;

public:
  // decode a packed api version
  static constexpr int ApiMajor (int version) {
    return version >> 16;
  }

  static constexpr int ApiMinor (int version) {
    return version & 0xFFFF;
  }

public:
  // get the bot version string
  virtual const char *GetBotVersion () = 0;

  // checks if bots are currently in game
  virtual bool IsBotsInGame () = 0;

  // checks whether specified players is a yapb bot
  virtual bool IsBot (int entity) = 0;

  // gets the node nearest to origin
  virtual int GetNearestNode (float *origin) = 0;

  // checks whether node is valid
  virtual bool IsNodeValid (int node) = 0;

  // gets the node origin
  virtual float *GetNodeOrigin (int node) = 0;

  // get the bots current active node
  virtual int GetCurrentNodeId (int entity) = 0;

  // force bot to go to the selected node
  virtual void SetBotGoal (int entity, int node) = 0;

  // get's the bot current goal node
  virtual int GetBotGoal (int entity) = 0;

  // force bot to go to selected origin
  virtual void SetBotGoalOrigin (int entity, float *origin) = 0;

  // checks whether graph nodes is available on map
  virtual bool HasGraph () = 0;

  // get's the graph node flags
  virtual int GetNodeFlags (int node) = 0;

  // get's the bot origin
  virtual float *GetBotOrigin (int entity) = 0;

  // get's the bot ping
  virtual int GetBotPing (int entity) = 0;

  // get's the bot current weapon id
  virtual int GetBotWeapon (int entity) = 0;

  // get's the bot total ammo for weapon id
  virtual int GetBotAmmo (int entity, int weapon) = 0;

  // get's the bot ammo in current weapon clip
  virtual int GetBotAmmoInClip (int entity) = 0;

  // get's the bot difficulty
  virtual int GetBotDifficulty (int entity) = 0;

  // set's the bot difficulty, returns false when out of range
  virtual bool SetBotDifficulty (int entity, int difficulty) = 0;

  // get's the bot personality
  virtual int GetBotPersonality (int entity) = 0;

  // get's the bot current task id
  virtual int GetBotTask (int entity) = 0;

  // get's the bot current task data (node index)
  virtual int GetBotTaskData (int entity) = 0;

  // get's the bots in game count
  virtual int GetBotCount () = 0;

  // get's the bot enemy entity index
  virtual int GetBotEnemy (int entity) = 0;

  // get's the bot last enemy entity index
  virtual int GetBotLastEnemy (int entity) = 0;

  // get's the bot last victim entity index
  virtual int GetBotLastVictim (int entity) = 0;

  // checks whether the bot enemy is reachable
  virtual bool IsBotEnemyReachable (int entity) = 0;

  // get's the bot last enemy origin
  virtual float *GetBotLastEnemyOrigin (int entity) = 0;

  // checks whether the bot is stuck
  virtual bool IsBotStuck (int entity) = 0;

  // get's the bot cumulative stuck time
  virtual float GetBotStuckTime (int entity) = 0;

  // get's the bot look at origin
  virtual float *GetBotLookAt (int entity) = 0;

  // get's the bot remaining path length
  virtual int GetBotPathLength (int entity) = 0;

  // checks whether the bot is camping
  virtual bool IsBotCamping (int entity) = 0;

  // gets the node nearest to origin within max distance
  virtual int FindNearestNode (float *origin, float max_distance) = 0;

  // gets the node farest from origin within max distance
  virtual int FindFarestNode (float *origin, float max_distance) = 0;

  // gets the node nearest to origin within radius
  virtual int FindNearestNodeInRadius (float *origin, float radius) = 0;

  // checks whether two nodes are reachable
  virtual bool IsNodeReachable (int src_node, int dest_node) = 0;

  // gets distance between two nodes
  virtual float GetNodeDistance (int src_node, int dest_node) = 0;

  // gets total graph nodes count
  virtual int GetNodeCount () = 0;

  // gets the node radius
  virtual float GetNodeRadius (int node) = 0;

  // gets the node light level
  virtual float GetNodeLight (int node) = 0;

  // gets the node outgoing links count
  virtual int GetNodeLinkCount (int node) = 0;

  // gets the node link target by index
  virtual int GetNodeLink (int node, int index) = 0;

  // gets the node link flags by index
  virtual int GetNodeLinkFlags (int node, int index) = 0;

  // gets the random node of the specified type
  virtual int GetRandomNode (int type) = 0;

  // gets the graph author
  virtual const char *GetGraphAuthor () = 0;

  // gets the graph modified by
  virtual const char *GetGraphModified () = 0;

  // adds a bot, returns true when the request is queued
  virtual bool AddBot (const char *name, int difficulty, int personality, int team) = 0;

  // enable/disable bot movement from the amxx api
  virtual void SetBotMovement (int entity, bool move) = 0;

  // checks whether bot movement is enabled
  virtual bool IsBotMovement (int entity) = 0;

  // gets the node at offset from the bot current path cursor
  virtual int GetBotPathNode (int entity, int offset) = 0;

  // force the bot look target, zero vector resets to ai
  virtual void SetBotLookAt (int entity, float *origin) = 0;

  // gets the packed bot api version
  virtual int GetApiVersion () = 0;
};

} // namespace bot
