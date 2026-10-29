//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include "yapb_amxx.h"
#include <product.h>

#ifdef VERSION_GENERATED
  #include <version.build.h>
#else
  #include <version.h>
#endif
namespace bot {

const AmxxModuleInfo &YaPBModule::ModuleInfo () {
  static const ystl::String version =
    ystl::strings.format ("%s.%s (api %d.%d)", MODULE_VERSION, MODULE_COMMIT_COUNT, kBotModuleApiMajor, kBotModuleApiMinor);
  static const AmxxModuleInfo info { "YaPB API", product.author.chars (), version.chars (),
    1, // reload on mapchange
    "YB", "yapb", "" };

  return info;
}

bool AmxxCore::Bind (PFN_REQ_FNPTR request) {
  if (!request) {
    return false;
  }

  auto require = [request] (const char *name) {
    return request (name);
  };

  add_natives_ = reinterpret_cast<FnAddNatives> (require ("AddNatives"));
  get_modname_ = reinterpret_cast<FnGetModname> (require ("GetModname"));
  log_ = reinterpret_cast<FnLog> (require ("Log"));
  log_error_ = reinterpret_cast<FnLogError> (require ("LogError"));
  get_amx_addr_ = reinterpret_cast<FnGetAmxAddr> (require ("GetAmxAddr"));
  get_amx_string_ = reinterpret_cast<FnGetAmxString> (require ("GetAmxString"));
  set_amx_string_ = reinterpret_cast<FnSetAmxString> (require ("SetAmxString"));
  cell_to_real_ = reinterpret_cast<FnCellToReal> (require ("CellToReal"));
  real_to_cell_ = reinterpret_cast<FnRealToCell> (require ("RealToCell"));
  register_forward_ = reinterpret_cast<FnRegisterForward> (require ("RegisterForward"));
  execute_forward_ = reinterpret_cast<FnExecuteForward> (require ("ExecuteForward"));

  return add_natives_ && get_modname_ && log_ && log_error_ && get_amx_addr_ && get_amx_string_ && set_amx_string_ && cell_to_real_ && real_to_cell_ && register_forward_ && execute_forward_;
}

namespace {

/**
 * Gets the bot api version, packed as major << 16 | minor.
 *
 * @return          Packed api version, 0 if the bot is not loaded.
 */
// @native yb_get_api_version();
cell AMX_NATIVE_CALL yb_get_api_version (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetApiVersion () : 0;
}

/**
 * Gets the bot version string.
 *
 * @param output    Buffer to copy the version to.
 * @param len       Maximum buffer size.
 *
 * @return          Number of characters copied.
 */
// @native yb_get_bot_version(output[], len);
cell AMX_NATIVE_CALL yb_get_bot_version (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return amxx.SetAmxString (amx, params[1], api->GetBotVersion (), params[2]);
}

/**
 * Checks whether bots are added on the server.
 *
 * @return          True if bots are in game, false otherwise.
 */
// @native bool:yb_game_has_bots();
cell AMX_NATIVE_CALL yb_game_has_bots (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotsInGame () ? 1 : 0;
}

/**
 * Gets the bots in game count.
 *
 * @return          Number of bots.
 */
// @native yb_get_bot_count();
cell AMX_NATIVE_CALL yb_get_bot_count (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotCount () : 0;
}

/**
 * Adds a bot to the server.
 *
 * @param difficulty    Difficulty, see YBDifficulty.
 * @param personality   Personality, see YBPersonality.
 * @param team          CS team, see CsTeams.
 *
 * @return              1 if the request is queued, 0 otherwise.
 */
// @native bool:yb_add_bot(YBDifficulty:difficulty = YBDifficulty:YB_DIFFICULTY_NORMAL, YBPersonality:personality = YBPersonality:YB_PERSONALITY_NORMAL, CsTeams:team = CS_TEAM_UNASSIGNED);
cell AMX_NATIVE_CALL yb_add_bot (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return api->AddBot ("", params[1], params[2], params[3]) ? 1 : 0;
}

/**
 * Checks whether the player is a YaPB bot.
 *
 * @param index     Player index.
 *
 * @return          True if the player is a bot, false otherwise.
 */
// @native bool:yb_is_user_bot(index);
cell AMX_NATIVE_CALL yb_is_user_bot (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBot (params[1]) ? 1 : 0;
}

/**
 * Gets the bot origin.
 *
 * @param index     Bot index.
 * @param origin    ystl::Array to store the origin.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_get_bot_origin(index, Float:origin[3]);
cell AMX_NATIVE_CALL yb_get_bot_origin (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  auto result = api->GetBotOrigin (params[1]);

  if (!result) {
    return 0;
  }
  amxx.WriteVector (amx, params[2], result);

  return 1;
}

/**
 * Gets the bot current weapon id.
 *
 * @param index     Bot index.
 *
 * @return          Weapon id (CSW_*), 0 if the bot is invalid.
 */
// @native yb_get_bot_weapon(index);
cell AMX_NATIVE_CALL yb_get_bot_weapon (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotWeapon (params[1]) : 0;
}

/**
 * Gets the bot total ammo for the weapon id.
 *
 * @param index     Bot index.
 * @param weapon    Weapon id (CSW_*).
 *
 * @return          Ammo amount, 0 if invalid.
 */
// @native yb_get_bot_ammo(index, weapon);
cell AMX_NATIVE_CALL yb_get_bot_ammo (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotAmmo (params[1], params[2]) : 0;
}

/**
 * Gets the bot ammo in the current weapon clip.
 *
 * @param index     Bot index.
 *
 * @return          Ammo in clip, 0 if invalid.
 */
// @native yb_get_bot_ammo_in_clip(index);
cell AMX_NATIVE_CALL yb_get_bot_ammo_in_clip (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotAmmoInClip (params[1]) : 0;
}

/**
 * Gets the bot ping.
 *
 * @param index     Bot index.
 *
 * @return          Ping value, 0 if the bot is invalid.
 */
// @native yb_get_bot_ping(index);
cell AMX_NATIVE_CALL yb_get_bot_ping (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotPing (params[1]) : 0;
}

/**
 * Gets the bot difficulty.
 *
 * @param index     Bot index.
 *
 * @return          Difficulty, see YBDifficulty.
 */
// @native YBDifficulty:yb_get_bot_difficulty(index);
cell AMX_NATIVE_CALL yb_get_bot_difficulty (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotDifficulty (params[1]) : -1;
}

/**
 * Sets the bot difficulty.
 *
 * @param index         Bot index.
 * @param difficulty    Difficulty, see YBDifficulty.
 *
 * @return              1 on success, 0 otherwise.
 */
// @native bool:yb_set_bot_difficulty(index, YBDifficulty:difficulty);
cell AMX_NATIVE_CALL yb_set_bot_difficulty (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->SetBotDifficulty (params[1], params[2]) ? 1 : 0;
}

/**
 * Gets the bot personality.
 *
 * @param index     Bot index.
 *
 * @return          Personality, see YBPersonality.
 */
// @native YBPersonality:yb_get_bot_personality(index);
cell AMX_NATIVE_CALL yb_get_bot_personality (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotPersonality (params[1]) : -1;
}

/**
 * Gets the bot current task id.
 *
 * @param index     Bot index.
 *
 * @return          Task id, see YBTask.
 */
// @native YBTask:yb_get_bot_task(index);
cell AMX_NATIVE_CALL yb_get_bot_task (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotTask (params[1]) : 20;
}

/**
 * Gets the bot current task data.
 *
 * @param index     Bot index.
 *
 * @return          Task node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_get_bot_task_data(index);
cell AMX_NATIVE_CALL yb_get_bot_task_data (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotTaskData (params[1]) : -1;
}

/**
 * Gets the node index the bot is currently standing on.
 *
 * @param index     Bot index.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if dead or camping.
 */
// @native yb_get_bot_node(index);
cell AMX_NATIVE_CALL yb_get_bot_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetCurrentNodeId (params[1]) : -1;
}

/**
 * Gets the node index the bot is currently going to.
 *
 * @param index     Bot index.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_get_bot_goal(index);
cell AMX_NATIVE_CALL yb_get_bot_goal (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotGoal (params[1]) : -1;
}

/**
 * Forces the bot to go to the selected node.
 *
 * @param index     Bot index.
 * @param node      Goal node index.
 *
 * @return          1 on success, 0 if the node or bot is invalid.
 */
// @native yb_set_bot_goal_node(index, node);
cell AMX_NATIVE_CALL yb_set_bot_goal_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api || !api->IsNodeValid (params[2])) {
    return 0;
  }
  api->SetBotGoal (params[1], params[2]);

  return 1;
}

/**
 * Forces the bot to go to the selected origin.
 *
 * @param index     Bot index.
 * @param origin    ystl::Vector of the location.
 *
 * @return          1 on success, 0 otherwise.
 */
// @native yb_set_bot_goal_origin(index, const Float:origin[3]);
cell AMX_NATIVE_CALL yb_set_bot_goal_origin (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[2], origin);
  api->SetBotGoalOrigin (params[1], origin);

  return 1;
}

/**
 * Gets the bot remaining path length.
 *
 * @param index     Bot index.
 *
 * @return          Number of nodes left in the path.
 */
// @native yb_get_bot_path_length(index);
cell AMX_NATIVE_CALL yb_get_bot_path_length (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotPathLength (params[1]) : 0;
}

/**
 * Gets a node from the bot current path.
 *
 * @param index     Bot index.
 * @param offset    Offset from the current path cursor, 0 is the next node.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_get_bot_path_node(index, offset);
cell AMX_NATIVE_CALL yb_get_bot_path_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotPathNode (params[1], params[2]) : -1;
}

/**
 * Checks whether the bot is stuck.
 *
 * @param index     Bot index.
 *
 * @return          True if stuck, false otherwise.
 */
// @native bool:yb_is_bot_stuck(index);
cell AMX_NATIVE_CALL yb_is_bot_stuck (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotStuck (params[1]) ? 1 : 0;
}

/**
 * Gets the bot cumulative stuck time.
 *
 * @param index     Bot index.
 *
 * @return          Stuck time in seconds.
 */
// @native Float:yb_get_bot_stuck_time(index);
cell AMX_NATIVE_CALL yb_get_bot_stuck_time (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return amxx.RealToCell (api ? api->GetBotStuckTime (params[1]) : 0.0f);
}

/**
 * Checks whether the bot is camping.
 *
 * @param index     Bot index.
 *
 * @return          True if camping, false otherwise.
 */
// @native bool:yb_is_bot_camping(index);
cell AMX_NATIVE_CALL yb_is_bot_camping (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotCamping (params[1]) ? 1 : 0;
}

/**
 * Enables or disables bot movement.
 *
 * @param index     Bot index.
 * @param movement  True to allow movement, false to freeze the bot.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_set_bot_movement(index, bool:movement);
cell AMX_NATIVE_CALL yb_set_bot_movement (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  api->SetBotMovement (params[1], params[2] != 0);

  return 1;
}

/**
 * Checks whether bot movement is enabled.
 *
 * @param index     Bot index.
 *
 * @return          True if movement is allowed, false otherwise.
 */
// @native bool:yb_is_bot_movement(index);
cell AMX_NATIVE_CALL yb_is_bot_movement (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotMovement (params[1]) ? 1 : 0;
}

/**
 * Enables or disables bot simulation.
 *
 * @param index     Bot index.
 * @param simulate  True to allow simulation, false to suspend all bot ticks.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_set_bot_simulation(index, bool:simulate);
cell AMX_NATIVE_CALL yb_set_bot_simulation (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return api->SetBotSimulation (params[1], params[2] != 0) ? 1 : 0;
}

/**
 * Checks whether bot simulation is enabled.
 *
 * @param index     Bot index.
 *
 * @return          True if simulation is allowed, false otherwise.
 */
// @native bool:yb_is_bot_simulating(index);
cell AMX_NATIVE_CALL yb_is_bot_simulating (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotSimulating (params[1]) ? 1 : 0;
}

/**
 * Checks whether the bot is alive.
 *
 * @param index     Bot index.
 *
 * @return          True if the bot is alive, false otherwise.
 */
// @native bool:yb_is_bot_alive(index);
cell AMX_NATIVE_CALL yb_is_bot_alive (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotAlive (params[1]) ? 1 : 0;
}

/**
 * Checks whether the bot is leaving the server.
 *
 * @param index     Bot index.
 *
 * @return          True if the bot is stale, false otherwise.
 */
// @native bool:yb_is_bot_stale(index);
cell AMX_NATIVE_CALL yb_is_bot_stale (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotStale (params[1]) ? 1 : 0;
}

/**
 * Kicks a single bot.
 *
 * @param index         Bot index.
 * @param silent        Suppress the kick message.
 * @param reduce_quota  Reduce the bot quota after the kick.
 *
 * @return              1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_kick_bot(index, bool:silent = true, bool:reduce_quota = true);
cell AMX_NATIVE_CALL yb_kick_bot (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return api->KickBot (params[1], params[2] != 0, params[3] != 0) ? 1 : 0;
}

/**
 * Sets the bot personality.
 *
 * @param index         Bot index.
 * @param personality   Personality, see YBPersonality.
 *
 * @return              1 on success, 0 if the bot is invalid or personality is out of range.
 */
// @native bool:yb_set_bot_personality(index, YBPersonality:personality);
cell AMX_NATIVE_CALL yb_set_bot_personality (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->SetBotPersonality (params[1], params[2]) ? 1 : 0;
}

/**
 * Gets the current bot weapon classname.
 *
 * @param index     Bot index.
 * @param output    Buffer to copy the classname to.
 * @param len       Maximum buffer size.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_get_bot_weapon_name(index, output[], len);
cell AMX_NATIVE_CALL yb_get_bot_weapon_name (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return amxx.SetAmxString (amx, params[2], api->GetBotWeaponName (params[1]), params[3]) > 0 ? 1 : 0;
}

/**
 * Gets the bot quota.
 *
 * @return          Bot quota.
 */
// @native yb_get_quota();
cell AMX_NATIVE_CALL yb_get_quota (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetQuota () : 0;
}

/**
 * Sets the bot quota.
 *
 * @param count     New bot quota.
 *
 * @return          1 on success, 0 if the bot is not loaded.
 */
// @native bool:yb_set_quota(count);
cell AMX_NATIVE_CALL yb_set_quota (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->SetQuota (params[1]) ? 1 : 0;
}

/**
 * Gets the bot look at origin.
 *
 * @param index     Bot index.
 * @param origin    ystl::Array to store the origin.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_get_bot_look_at(index, Float:origin[3]);
cell AMX_NATIVE_CALL yb_get_bot_look_at (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  auto result = api->GetBotLookAt (params[1]);

  if (!result) {
    return 0;
  }
  amxx.WriteVector (amx, params[2], result);

  return 1;
}

/**
 * Forces the bot look target, a zero vector restores ai aiming.
 *
 * @param index     Bot index.
 * @param origin    ystl::Vector to look at.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_set_bot_look_at(index, const Float:origin[3]);
cell AMX_NATIVE_CALL yb_set_bot_look_at (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[2], origin);
  api->SetBotLookAt (params[1], origin);

  return 1;
}

/**
 * Gets the bot enemy entity index.
 *
 * @param index     Bot index.
 *
 * @return          Entity index, YB_INVALID_ENTITY_INDEX if none.
 */
// @native yb_get_bot_enemy(index);
cell AMX_NATIVE_CALL yb_get_bot_enemy (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotEnemy (params[1]) : 0;
}

/**
 * Gets the bot last enemy entity index.
 *
 * @param index     Bot index.
 *
 * @return          Entity index, YB_INVALID_ENTITY_INDEX if none.
 */
// @native yb_get_bot_last_enemy(index);
cell AMX_NATIVE_CALL yb_get_bot_last_enemy (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotLastEnemy (params[1]) : 0;
}

/**
 * Gets the bot last victim entity index.
 *
 * @param index     Bot index.
 *
 * @return          Entity index, YB_INVALID_ENTITY_INDEX if none.
 */
// @native yb_get_bot_last_victim(index);
cell AMX_NATIVE_CALL yb_get_bot_last_victim (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetBotLastVictim (params[1]) : 0;
}

/**
 * Checks whether the bot enemy is reachable.
 *
 * @param index     Bot index.
 *
 * @return          True if reachable, false otherwise.
 */
// @native bool:yb_is_bot_enemy_reachable(index);
cell AMX_NATIVE_CALL yb_is_bot_enemy_reachable (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsBotEnemyReachable (params[1]) ? 1 : 0;
}

/**
 * Gets the bot last enemy origin.
 *
 * @param index     Bot index.
 * @param origin    ystl::Array to store the origin.
 *
 * @return          1 on success, 0 if the bot is invalid.
 */
// @native bool:yb_get_bot_last_enemy_origin(index, Float:origin[3]);
cell AMX_NATIVE_CALL yb_get_bot_last_enemy_origin (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  auto result = api->GetBotLastEnemyOrigin (params[1]);

  if (!result) {
    return 0;
  }
  amxx.WriteVector (amx, params[2], result);

  return 1;
}

/**
 * Checks whether the current map has a graph file.
 *
 * @return          True if a graph is available, false otherwise.
 */
// @native bool:yb_has_graph();
cell AMX_NATIVE_CALL yb_has_graph (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->HasGraph () ? 1 : 0;
}

/**
 * Gets the total graph nodes count.
 *
 * @return          Number of nodes.
 */
// @native yb_get_node_count();
cell AMX_NATIVE_CALL yb_get_node_count (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetNodeCount () : 0;
}

/**
 * Checks whether the graph node index is valid.
 *
 * @param node      Graph node index.
 *
 * @return          True if the node is valid, false otherwise.
 */
// @native bool:yb_is_node_valid(node);
cell AMX_NATIVE_CALL yb_is_node_valid (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsNodeValid (params[1]) ? 1 : 0;
}

/**
 * Gets the graph node flags by index.
 *
 * @param node      Graph node index.
 *
 * @return          Node flags, see YBNodeFlag.
 */
// @native YBNodeFlag:yb_get_node_flags(node);
cell AMX_NATIVE_CALL yb_get_node_flags (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetNodeFlags (params[1]) : 0;
}

/**
 * Gets the graph node origin by index.
 *
 * @param node      Graph node index.
 * @param origin    ystl::Array to store the node origin.
 *
 * @return          1 on success, 0 if the node is invalid.
 */
// @native bool:yb_get_node_origin(node, Float:origin[3]);
cell AMX_NATIVE_CALL yb_get_node_origin (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  auto result = api->GetNodeOrigin (params[1]);

  if (!result) {
    return 0;
  }
  amxx.WriteVector (amx, params[2], result);

  return 1;
}

/**
 * Gets the graph node index nearest to the location.
 *
 * @param origin    ystl::Vector of the location.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_get_nearest_node(const Float:origin[3]);
cell AMX_NATIVE_CALL yb_get_nearest_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return -1;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[1], origin);

  return api->GetNearestNode (origin);
}

/**
 * Gets the node nearest to the origin within max distance.
 *
 * @param origin        ystl::Vector of the location.
 * @param maxDistance   Maximum search distance.
 *
 * @return              Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_find_nearest_node(const Float:origin[3], Float:maxDistance = 9999999.0);
cell AMX_NATIVE_CALL yb_find_nearest_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return -1;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[1], origin);

  return api->FindNearestNode (origin, amxx.CellToReal (params[2]));
}

/**
 * Gets the node farest from the origin beyond min distance.
 *
 * @param origin        ystl::Vector of the location.
 * @param maxDistance   Minimum search distance.
 *
 * @return              Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_find_farest_node(const Float:origin[3], Float:maxDistance = 32.0);
cell AMX_NATIVE_CALL yb_find_farest_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return -1;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[1], origin);

  return api->FindFarestNode (origin, amxx.CellToReal (params[2]));
}

/**
 * Gets the node nearest to the origin within radius.
 *
 * @param origin    ystl::Vector of the location.
 * @param radius    Search radius.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_find_nearest_node_in_radius(const Float:origin[3], Float:radius);
cell AMX_NATIVE_CALL yb_find_nearest_node_in_radius (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return -1;
  }
  float origin[3] {};
  amxx.ReadVector (amx, params[1], origin);

  return api->FindNearestNodeInRadius (origin, amxx.CellToReal (params[2]));
}

/**
 * Gets the distance between two nodes.
 *
 * @param srcNode   Source node index.
 * @param destNode  Destination node index.
 *
 * @return          Distance in units, -1 if a node is invalid.
 */
// @native Float:yb_get_node_distance(srcNode, destNode);
cell AMX_NATIVE_CALL yb_get_node_distance (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return amxx.RealToCell (api ? api->GetNodeDistance (params[1], params[2]) : -1.0f);
}

/**
 * Checks whether two nodes are reachable from each other.
 *
 * @param srcNode   Source node index.
 * @param destNode  Destination node index.
 *
 * @return          True if reachable, false otherwise.
 */
// @native bool:yb_is_node_reachable(srcNode, destNode);
cell AMX_NATIVE_CALL yb_is_node_reachable (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api && api->IsNodeReachable (params[1], params[2]) ? 1 : 0;
}

/**
 * Gets the node radius.
 *
 * @param node      Graph node index.
 *
 * @return          Node radius, 0 if the node is invalid.
 */
// @native Float:yb_get_node_radius(node);
cell AMX_NATIVE_CALL yb_get_node_radius (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return amxx.RealToCell (api ? api->GetNodeRadius (params[1]) : 0.0f);
}

/**
 * Gets the node light level.
 *
 * @param node      Graph node index.
 *
 * @return          Light level, 0 if the node is invalid.
 */
// @native Float:yb_get_node_light(node);
cell AMX_NATIVE_CALL yb_get_node_light (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return amxx.RealToCell (api ? api->GetNodeLight (params[1]) : 0.0f);
}

/**
 * Gets the node outgoing links count.
 *
 * @param node      Graph node index.
 *
 * @return          Number of links.
 */
// @native yb_get_node_link_count(node);
cell AMX_NATIVE_CALL yb_get_node_link_count (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetNodeLinkCount (params[1]) : 0;
}

/**
 * Gets the node link target by index.
 *
 * @param node      Graph node index.
 * @param index     Link slot index.
 *
 * @return          Target node index, YB_INVALID_NODE_INDEX if none.
 */
// @native yb_get_node_link(node, index);
cell AMX_NATIVE_CALL yb_get_node_link (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetNodeLink (params[1], params[2]) : -1;
}

/**
 * Gets the node link flags by index.
 *
 * @param node      Graph node index.
 * @param index     Link slot index.
 *
 * @return          Link flags, see PathFlag.
 */
// @native yb_get_node_link_flags(node, index);
cell AMX_NATIVE_CALL yb_get_node_link_flags (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetNodeLinkFlags (params[1], params[2]) : 0;
}

/**
 * Gets a random node of the specified type.
 *
 * @param type      Node type, see YBNodeType.
 *
 * @return          Graph node index, YB_INVALID_NODE_INDEX if none.
 */
// @native YBNodeType:yb_get_random_node(type);
cell AMX_NATIVE_CALL yb_get_random_node (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  return api ? api->GetRandomNode (params[1]) : -1;
}

/**
 * Gets the graph author.
 *
 * @param output    Buffer to copy the author to.
 * @param len       Maximum buffer size.
 *
 * @return          Number of characters copied.
 */
// @native yb_get_graph_author(output[], len);
cell AMX_NATIVE_CALL yb_get_graph_author (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return amxx.SetAmxString (amx, params[1], api->GetGraphAuthor (), params[2]);
}

/**
 * Gets the graph modified by.
 *
 * @param output    Buffer to copy the modifier to.
 * @param len       Maximum buffer size.
 *
 * @return          Number of characters copied.
 */
// @native yb_get_graph_modified(output[], len);
cell AMX_NATIVE_CALL yb_get_graph_modified (AMX *amx, cell *params) {
  auto api = yapb.Api ();

  if (!api) {
    return 0;
  }
  return amxx.SetAmxString (amx, params[1], api->GetGraphModified (), params[2]);
}

/**
 * Queues a bot text message.
 *
 * @param index     Bot index.
 * @param message   Message text.
 * @param team_only Send only to the bot team.
 * @param force     Bypass the global chat cvar.
 *
 * @return          1 if the message was queued, 0 otherwise.
 */
// @native bool:yb_bot_say(index, const message[], bool:team_only = false, bool:force = false);
cell AMX_NATIVE_CALL yb_bot_say (AMX *amx, cell *params) {
  auto api = yapb.ChatApi ();

  if (!api) {
    return 0;
  }

  int length = 0;
  auto message = amxx.GetAmxString (amx, params[2], 2, length);

  if (!message || length <= 0) {
    return 0;
  }

  const bool team_only = params[0] >= 3 ? params[3] != 0 : false;
  const bool force = params[0] >= 4 ? params[4] != 0 : false;

  return api->SendBotChat (params[1], message, team_only, force) ? 1 : 0;
}

/**
 * Sets who may chat for the bot.
 *
 * @param index     Bot index.
 * @param mode      Chat mode, see YBChatMode.
 *
 * @return          1 on success, 0 otherwise.
 */
// @native bool:yb_set_bot_chat(index, YBChatMode:mode);
cell AMX_NATIVE_CALL yb_set_bot_chat (AMX *amx, cell *params) {
  auto api = yapb.ChatApi ();

  if (!api) {
    return 0;
  }
  return api->SetBotChatMode (params[1], static_cast<BotChatMode> (params[2])) ? 1 : 0;
}

/**
 * Gets who may chat for the bot.
 *
 * @param index     Bot index.
 *
 * @return          Chat mode, see YBChatMode.
 */
// @native YBChatMode:yb_get_bot_chat(index);
cell AMX_NATIVE_CALL yb_get_bot_chat (AMX *amx, cell *params) {
  auto api = yapb.ChatApi ();

  if (!api) {
    return ystl::to_underlying (BotChatMode::Invalid);
  }
  return ystl::to_underlying (api->GetBotChatMode (params[1]));
}

AMX_NATIVE_INFO bot_natives[] = {
  { "yb_get_api_version",             yb_get_api_version             },
  { "yb_get_bot_version",             yb_get_bot_version             },
  { "yb_game_has_bots",               yb_game_has_bots               },
  { "yb_get_bot_count",               yb_get_bot_count               },
  { "yb_add_bot",                     yb_add_bot                     },
  { "yb_is_user_bot",                 yb_is_user_bot                 },
  { "yb_get_bot_origin",              yb_get_bot_origin              },
  { "yb_get_bot_weapon",              yb_get_bot_weapon              },
  { "yb_get_bot_ammo",                yb_get_bot_ammo                },
  { "yb_get_bot_ammo_in_clip",        yb_get_bot_ammo_in_clip        },
  { "yb_get_bot_ping",                yb_get_bot_ping                },
  { "yb_get_bot_difficulty",          yb_get_bot_difficulty          },
  { "yb_set_bot_difficulty",          yb_set_bot_difficulty          },
  { "yb_get_bot_personality",         yb_get_bot_personality         },
  { "yb_get_bot_task",                yb_get_bot_task                },
  { "yb_get_bot_task_data",           yb_get_bot_task_data           },
  { "yb_get_bot_node",                yb_get_bot_node                },
  { "yb_get_bot_goal",                yb_get_bot_goal                },
  { "yb_set_bot_goal_node",           yb_set_bot_goal_node           },
  { "yb_set_bot_goal_origin",         yb_set_bot_goal_origin         },
  { "yb_get_bot_path_length",         yb_get_bot_path_length         },
  { "yb_get_bot_path_node",           yb_get_bot_path_node           },
  { "yb_is_bot_stuck",                yb_is_bot_stuck                },
  { "yb_get_bot_stuck_time",          yb_get_bot_stuck_time          },
  { "yb_is_bot_camping",              yb_is_bot_camping              },
  { "yb_set_bot_movement",            yb_set_bot_movement            },
  { "yb_is_bot_movement",             yb_is_bot_movement             },
  { "yb_set_bot_simulation",          yb_set_bot_simulation          },
  { "yb_is_bot_simulating",           yb_is_bot_simulating           },
  { "yb_is_bot_alive",                yb_is_bot_alive                },
  { "yb_is_bot_stale",                yb_is_bot_stale                },
  { "yb_kick_bot",                    yb_kick_bot                    },
  { "yb_set_bot_personality",         yb_set_bot_personality         },
  { "yb_get_bot_weapon_name",         yb_get_bot_weapon_name         },
  { "yb_get_quota",                   yb_get_quota                   },
  { "yb_set_quota",                   yb_set_quota                   },
  { "yb_get_bot_look_at",             yb_get_bot_look_at             },
  { "yb_set_bot_look_at",             yb_set_bot_look_at             },
  { "yb_get_bot_enemy",               yb_get_bot_enemy               },
  { "yb_get_bot_last_enemy",          yb_get_bot_last_enemy          },
  { "yb_get_bot_last_victim",         yb_get_bot_last_victim         },
  { "yb_is_bot_enemy_reachable",      yb_is_bot_enemy_reachable      },
  { "yb_get_bot_last_enemy_origin",   yb_get_bot_last_enemy_origin   },
  { "yb_has_graph",                   yb_has_graph                   },
  { "yb_get_node_count",              yb_get_node_count              },
  { "yb_is_node_valid",               yb_is_node_valid               },
  { "yb_get_node_flags",              yb_get_node_flags              },
  { "yb_get_node_origin",             yb_get_node_origin             },
  { "yb_get_nearest_node",            yb_get_nearest_node            },
  { "yb_find_nearest_node",           yb_find_nearest_node           },
  { "yb_find_farest_node",            yb_find_farest_node            },
  { "yb_find_nearest_node_in_radius", yb_find_nearest_node_in_radius },
  { "yb_get_node_distance",           yb_get_node_distance           },
  { "yb_is_node_reachable",           yb_is_node_reachable           },
  { "yb_get_node_radius",             yb_get_node_radius             },
  { "yb_get_node_light",              yb_get_node_light              },
  { "yb_get_node_link_count",         yb_get_node_link_count         },
  { "yb_get_node_link",               yb_get_node_link               },
  { "yb_get_node_link_flags",         yb_get_node_link_flags         },
  { "yb_get_random_node",             yb_get_random_node             },
  { "yb_get_graph_author",            yb_get_graph_author            },
  { "yb_get_graph_modified",          yb_get_graph_modified          },
  { "yb_bot_say",                     yb_bot_say                     },
  { "yb_set_bot_chat",                yb_set_bot_chat                },
  { "yb_get_bot_chat",                yb_get_bot_chat                },

  { nullptr,                          nullptr                        }
};
} // namespace

// amxx forwards, parsed into yapb.inc by tools/gen_amxx_inc.py

/**
 * Called when a bot is spawned.
 */
// @forward yb_bot_spawned (index);

/**
 * Called when a bot is killed.
 */
// @forward yb_bot_killed (victim, killer);

/**
 * Called when a bot is removed.
 */
// @forward yb_bot_removed (index, bool:kicked);

/**
 * Called before a text chat message is emitted.
 *
 * Return PLUGIN_HANDLED to block it.
 */
// @forward yb_chat_pre (sender, const message[], bool:team_only, YBChatSource:source);

/**
 * Called after a text chat message is emitted.
 */
// @forward yb_chat_post (sender, const message[], bool:team_only, YBChatSource:source);

int YaPBModule::EventSink (const BotEvent &event) {
  return yapb.Dispatch (event);
}

int YaPBModule::Dispatch (const BotEvent &event) {
  switch (event.type) {
    case BotEventType::Spawned:
      return bot_spawned_forward_.Execute (event.entity);

    case BotEventType::Killed:
      return bot_killed_forward_.Execute (event.entity, event.arg1);

    case BotEventType::Removed:
      return bot_removed_forward_.Execute (event.entity, event.arg1);

    case BotEventType::ChatPre:
      return chat_pre_forward_.Execute (event.entity, event.message, event.team_only ? 1 : 0,
        ystl::to_underlying (event.source)) != 0 ? 1 : 0;

    case BotEventType::ChatPost:
      chat_post_forward_.Execute (event.entity, event.message, event.team_only ? 1 : 0,
        ystl::to_underlying (event.source));
      return 0;
  }
  return 0;
}

void YaPBModule::Load () {
  amxx.AddNatives (bot_natives);

  if (botdll_) {
    botdll_.unload ();
  }
  api_ = nullptr;
  chat_api_ = nullptr;

  // any failure leaves the natives disabled and the api null
  auto fail = [this] (ystl::StringRef message) {
    amxx.log_ ("ERROR: %s\n", message.chars ());
    DisableNatives ();
  };

  ystl::String bot_path = ystl::strings.format ("%s/addons/yapb/bin/yapb%s", amxx.GetModname (), kLibrarySuffix);

  if (!ystl::plat.file_exists (bot_path.chars ())) {
    return fail (ystl::strings.format ("unable to locate YaPB binary at %s", bot_path.chars ()));
  }

  if (!botdll_.load (bot_path)) {
    return fail (ystl::strings.format ("nable to open YaPB binary %s: %s", bot_path.chars (), ystl::SharedLibrary::last_error ().chars ()));
  }
  auto api = botdll_.resolve<Export> ("GetBotAPI");

  if (!api) {
    return fail (ystl::strings.format ("YaPB binary %s misses the GetBotAPI export", bot_path.chars ()));
  }
  api_ = api (kBotModuleVersion);

  if (!api_) {
    return fail (ystl::strings.format ("YaPB binary %s returned no API, api %d.%d required", bot_path.chars (),
      IBotModule::ApiMajor (kBotModuleVersion), IBotModule::ApiMinor (kBotModuleVersion)));
  }

  // chat is an optional extension on top of the main interface
  if (api_->GetApiVersion () >= kBotModuleVersion) {
    chat_api_ = api_->GetChatAPI ();
  }

  if (!bot_spawned_forward_.IsValid ()) {
    bot_spawned_forward_.Register ("yb_bot_spawned", kForwardIgnore, kForwardCell);
    bot_killed_forward_.Register ("yb_bot_killed", kForwardIgnore, kForwardCell, kForwardCell);
    bot_removed_forward_.Register ("yb_bot_removed", kForwardIgnore, kForwardCell, kForwardCell);
  }

  if (!chat_pre_forward_.IsValid ()) {
    chat_pre_forward_.Register ("yb_chat_pre", kForwardStop,
      kForwardCell, kForwardString, kForwardCell, kForwardCell);
    chat_post_forward_.Register ("yb_chat_post", kForwardIgnore,
      kForwardCell, kForwardString, kForwardCell, kForwardCell);
  }

  api_->SetEventSink (&YaPBModule::EventSink);
}

void YaPBModule::Unload () {
  if (api_) {
    api_->SetEventSink (nullptr);
  }
  if (botdll_) {
    botdll_.unload ();
  }
  api_ = nullptr;
  chat_api_ = nullptr;
  DisableNatives ();
}

void YaPBModule::DisableNatives () {
  for (size_t i = 0; bot_natives[i].name; ++i) {
    bot_natives[i].func = [] (AMX *amx, cell *params) -> cell {
      amxx.log_error_ (amx, ystl::to_underlying (AmxError::Native), "Native is unavailable. YaPB binary isn't loaded.");
      return 0;
    };
  }
}

// amxx core interface
YSTL_EXPORT int AMXX_Query (int *interface_version, AmxxModuleInfo *module_info) {
  if (!interface_version || !module_info) {
    return ystl::to_underlying (AmxxStatus::Param);
  }

  if (*interface_version != kAmxxInterfaceVersion) {
    *interface_version = kAmxxInterfaceVersion;
    return ystl::to_underlying (AmxxStatus::IfVers);
  }
  *module_info = YaPBModule::ModuleInfo ();

  return ystl::to_underlying (AmxxStatus::Ok);
}

YSTL_EXPORT int AMXX_CheckGame (const char *game) {
  return ystl::to_underlying (AmxxGameStatus::Ok);
}

YSTL_EXPORT int AMXX_Attach (PFN_REQ_FNPTR request) {
  if (!amxx.Bind (request)) {
    return ystl::to_underlying (AmxxStatus::FuncNotPresent);
  }
  yapb.Load ();

  return ystl::to_underlying (AmxxStatus::Ok);
}

YSTL_EXPORT int AMXX_Detach () {
  yapb.Unload ();

  return ystl::to_underlying (AmxxStatus::Ok);
}

} // namespace bot

// override new/delete globally, need to be included in .cpp file
#include <ystl/memory_override.h>
