//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

// module interface implementation
namespace bot {

class Module : public IBotModule {
public:
  virtual ~Module () override = default;

private:
  // map amx player index (1-based) to bot
  Bot *GetBot (int index) {
    if (index < 1) {
      return nullptr;
    }
    return bots[game.PlayerOfIndex (index - 1)];
  }

  // entity index of an edict, 0 when null
  int EntIndex (edict_t *ent) {
    return ent ? game.IndexOfEntity (ent) : 0;
  }

  // local vector from a float pointer
  ystl::Vector ToVector (float *origin) {
    return ystl::Vector (origin[0], origin[1], origin[2]);
  }

public:
  // get the bot version string
  virtual const char *GetBotVersion () override {
    return MODULE_VERSION "." MODULE_COMMIT_COUNT;
  }

  // checks if bots are currently in game
  virtual bool IsBotsInGame () override {
    return bots.GetBotCount () > 0;
  }

  // checks whether specified players is a yapb bot
  virtual bool IsBot (int entity) override {
    return GetBot (entity) != nullptr;
  }

  // gets the node nearest to origin
  virtual int GetNearestNode (float *origin) override {
    if (graph.Length () > 0) {
      return graph.GetNearestNoBuckets (origin);
    }
    return kInvalidNodeIndex;
  }

  // checks wether node is valid
  virtual bool IsNodeValid (int node) override {
    return graph.Exists (node);
  }

  // gets the node origin
  virtual float *GetNodeOrigin (int node) override {
    if (!graph.Exists (node)) {
      return nullptr;
    }
    return graph[node].origin;
  }

  // get the bots current active node
  virtual int GetCurrentNodeId (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->GetCurrentNodeIndex ();
    }
    return kInvalidNodeIndex;
  }

  // force bot to go to the selected node
  virtual void SetBotGoal (int entity, int node) override {
    if (!graph.Exists (node)) {
      return;
    }
    auto bot = GetBot (entity);

    if (bot) {
      return bot->SendBotToOrigin (graph[node].origin);
    }
  }

  // get's the bot current goal node
  virtual int GetBotGoal (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->chosen_goal_index_ == kInvalidNodeIndex ? bot->Task ()->data : bot->chosen_goal_index_;
    }
    return kInvalidNodeIndex;
  }

  // force bot to go to selected origin
  virtual void SetBotGoalOrigin (int entity, float *origin) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->SendBotToOrigin (origin);
    }
  }

  // checks whether graph nodes is available on map
  virtual bool HasGraph () override {
    return graph.Length () > 0;
  }

  // get's the graph node flags
  virtual int GetNodeFlags (int node) override {
    if (graph.Length () > 0 && graph.Exists (node)) {
      return graph[node].flags;
    }
    return 0;
  }

  // get's the bot origin
  virtual float *GetBotOrigin (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->pev->origin;
    }
    return nullptr;
  }

  // get's the bot ping
  virtual int GetBotPing (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return static_cast<int> (bot->ping_);
    }
    return 0;
  }

  // get's the bot current weapon id
  virtual int GetBotWeapon (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return static_cast<int> (bot->current_weapon_);
    }
    return 0;
  }

  // get's the bot total ammo for weapon id
  virtual int GetBotAmmo (int entity, int weapon) override {
    auto bot = GetBot (entity);

    if (bot && weapon >= 0 && weapon < kMaxWeapons) {
      return bot->GetAmmo (static_cast<Weapon> (weapon));
    }
    return 0;
  }

  // get's the bot ammo in current weapon clip
  virtual int GetBotAmmoInClip (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->GetAmmoInClip ();
    }
    return 0;
  }

  // get's the bot difficulty
  virtual int GetBotDifficulty (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return static_cast<int> (bot->difficulty_);
    }
    return static_cast<int> (Difficulty::Invalid);
  }

  // set's the bot difficulty
  virtual bool SetBotDifficulty (int entity, int difficulty) override {
    auto bot = GetBot (entity);

    if (!bot || difficulty < 0 || difficulty > static_cast<int> (Difficulty::Expert)) {
      return false;
    }
    bot->SetNewDifficulty (static_cast<Difficulty> (difficulty));

    return true;
  }

  // get's the bot personality
  virtual int GetBotPersonality (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return static_cast<int> (bot->personality_);
    }
    return static_cast<int> (Personality::Invalid);
  }

  // get's the bot current task id
  virtual int GetBotTask (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return static_cast<int> (bot->GetTaskId ());
    }
    return static_cast<int> (TaskId::None);
  }

  // get's the bot current task data (node index)
  virtual int GetBotTaskData (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->Task ()->data;
    }
    return kInvalidNodeIndex;
  }

  // get's the bots in game count
  virtual int GetBotCount () override {
    return bots.GetBotCount ();
  }

  // get's the bot enemy entity index
  virtual int GetBotEnemy (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return EntIndex (bot->enemy_);
    }
    return 0;
  }

  // get's the bot last enemy entity index
  virtual int GetBotLastEnemy (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return EntIndex (bot->last_enemy_);
    }
    return 0;
  }

  // get's the bot last victim entity index
  virtual int GetBotLastVictim (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return EntIndex (bot->last_victim_);
    }
    return 0;
  }

  // checks whether the bot enemy is reachable
  virtual bool IsBotEnemyReachable (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->is_enemy_reachable_;
    }
    return false;
  }

  // get's the bot last enemy origin
  virtual float *GetBotLastEnemyOrigin (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->last_enemy_origin_;
    }
    return nullptr;
  }

  // checks whether the bot is stuck
  virtual bool IsBotStuck (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->IsStuckState ();
    }
    return false;
  }

  // get's the bot cumulative stuck time
  virtual float GetBotStuckTime (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->StuckTime ();
    }
    return 0.0f;
  }

  // get's the bot look at origin
  virtual float *GetBotLookAt (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->LookAtVector ();
    }
    return nullptr;
  }

  // get's the bot remaining path length
  virtual int GetBotPathLength (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->RemainingPathLength ();
    }
    return 0;
  }

  // checks whether the bot is camping
  virtual bool IsBotCamping (int entity) override {
    auto bot = GetBot (entity);

    if (bot) {
      return bot->GetTaskId () == TaskId::Camp;
    }
    return false;
  }

  // gets the node nearest to origin within max distance
  virtual int FindNearestNode (float *origin, float max_distance) override {
    if (graph.Length () > 0) {
      return graph.GetNearest (ToVector (origin), max_distance);
    }
    return kInvalidNodeIndex;
  }

  // gets the node farest from origin within max distance
  virtual int FindFarestNode (float *origin, float max_distance) override {
    if (graph.Length () > 0) {
      return graph.GetFarest (ToVector (origin), max_distance);
    }
    return kInvalidNodeIndex;
  }

  // gets the node nearest to origin within radius
  virtual int FindNearestNodeInRadius (float *origin, float radius) override {
    if (graph.Length () > 0) {
      auto nodes = graph.GetNearestInRadius (radius, ToVector (origin));

      if (!nodes.empty ()) {
        return nodes[0];
      }
    }
    return kInvalidNodeIndex;
  }

  // checks whether two nodes are reachable
  virtual bool IsNodeReachable (int src_node, int dest_node) override {
    if (!graph.Exists (src_node) || !graph.Exists (dest_node)) {
      return false;
    }
    return graph.IsNodeReacheable (graph[src_node].origin, graph[dest_node].origin);
  }

  // gets distance between two nodes
  virtual float GetNodeDistance (int src_node, int dest_node) override {
    if (!graph.Exists (src_node) || !graph.Exists (dest_node)) {
      return -1.0f;
    }
    for (const auto &link : graph[src_node].links) {
      if (link.index == dest_node) {
        return static_cast<float> (link.distance);
      }
    }
    return graph[src_node].origin.distance (graph[dest_node].origin);
  }

  // gets total graph nodes count
  virtual int GetNodeCount () override {
    return graph.Length ();
  }

  // gets the node radius
  virtual float GetNodeRadius (int node) override {
    if (graph.Exists (node)) {
      return graph[node].radius;
    }
    return 0.0f;
  }

  // gets the node light level
  virtual float GetNodeLight (int node) override {
    if (graph.Exists (node)) {
      return graph[node].light;
    }
    return 0.0f;
  }

  // gets the node outgoing links count
  virtual int GetNodeLinkCount (int node) override {
    if (!graph.Exists (node)) {
      return 0;
    }
    int count = 0;

    for (const auto &link : graph[node].links) {
      if (link.index != kInvalidNodeIndex) {
        ++count;
      }
    }
    return count;
  }

  // gets the node link target by index
  virtual int GetNodeLink (int node, int index) override {
    if (!graph.Exists (node) || index < 0 || index >= kMaxNodeLinks) {
      return kInvalidNodeIndex;
    }
    return graph[node].links[index].index;
  }

  // gets the node link flags by index
  virtual int GetNodeLinkFlags (int node, int index) override {
    if (!graph.Exists (node) || index < 0 || index >= kMaxNodeLinks) {
      return 0;
    }
    return graph[node].links[index].flags;
  }

  // gets the random node of the specified type
  virtual int GetRandomNode (int type) override {
    if (graph.Length () > 0 && type >= 0 && type < static_cast<int> (PointType::Count)) {
      auto &points = graph.GetPoints (static_cast<PointType> (type));

      if (!points.empty ()) {
        return points.random ();
      }
    }
    return kInvalidNodeIndex;
  }

  // gets the graph author
  virtual const char *GetGraphAuthor () override {
    return graph.GetAuthor ().chars ();
  }

  // gets the graph modified by
  virtual const char *GetGraphModified () override {
    return graph.GetModifiedBy ().chars ();
  }

  // adds a bot, returns true when the request is queued
  virtual bool AddBot (const char *name, int difficulty, int personality, int team) override {
    Team bot_team = Team::Unassigned;

    if (team == 1) {
      bot_team = Team::Terrorist;
    }
    else if (team == 2) {
      bot_team = Team::CT;
    }
    bots.Addbot (name, static_cast<Difficulty> (difficulty), static_cast<Personality> (personality), bot_team, -1, false);

    return true;
  }

  // enable/disable bot movement from the amxx api
  virtual void SetBotMovement (int entity, bool move) override {
    auto bot = GetBot (entity);

    if (bot) {
      bot->SetMoveAllowed (move);
    }
  }

  // checks whether bot movement is enabled
  virtual bool IsBotMovement (int entity) override {
    auto bot = GetBot (entity);

    return bot ? bot->IsMoveAllowed () : false;
  }

  // gets the node at offset from the bot current path cursor
  virtual int GetBotPathNode (int entity, int offset) override {
    auto bot = GetBot (entity);

    return bot ? bot->PathNodeAt (offset) : kInvalidNodeIndex;
  }

  // force the bot look target, zero vector resets to ai
  virtual void SetBotLookAt (int entity, float *origin) override {
    auto bot = GetBot (entity);

    if (bot) {
      bot->ForcedLookAt () = ystl::Vector (origin[0], origin[1], origin[2]);
    }
  }

  // gets the packed bot api version
  virtual int GetApiVersion () override {
    return kBotModuleVersion;
  }
};

YSTL_EXPORT IBotModule *GetBotAPI (int version) {
  // same major, and a module no newer than the bot (interface is append-only)
  if (IBotModule::ApiMajor (version) != IBotModule::ApiMajor (kBotModuleVersion) ||
      IBotModule::ApiMinor (version) > IBotModule::ApiMinor (kBotModuleVersion)) {
    return nullptr;
  }
  static Module bot_module {};

  return &bot_module;
}

} // namespace bot
