//
// YaPB Bot Takeover.
//
// Lets a dead player take control of a teammate bot: the player is respawned
// at the bot position with the bot inventory, while the bot gets retired.
//
// Based on BotFeatures by Vaqtincha.
//

#include <amxmodx>
#include <amxmisc>
#include <fakemeta>
#include <hamsandwich>
#include <reapi>
#include <yapb>

#pragma semicolon 1

new const PLUGIN_NAME[] = "YaPB Bot Takeover";
new const PLUGIN_VERSION[] = "1.0.0";
new const PLUGIN_AUTHOR[] = "Vaqtincha & YB Team";
new const LOG_FILE[] = "yapb_takeover.log";

new const INVALID_WEAPONS_BS = (1 << any:WEAPON_NONE) | (1 << any:WEAPON_KNIFE);
new const USEFUL_SLOTS_BS = (1 << any:PRIMARY_WEAPON_SLOT) | (1 << any:PISTOL_SLOT);
enum {
  MAX_TRANSFER_ITEMS = 32,
  MAX_TRANSFER_WEAPONS = _:WEAPON_P90 + 1,
  TRANSFER_ITEM_NONE = 0,
  TRANSFER_ITEM_REMOVED = 1,
  TRANSFER_ITEM_MOVED = 2,
  TRANSFER_ITEM_LOST = 3
};

enum {
  PF_SIM_OFF = 1 << 0,
  PF_SPAWN_PENDING = 1 << 1,
  PF_SPAWN_DONE = 1 << 2,
  PF_SPAWN_VELOCITY = 1 << 3,
  PF_SPAWN_ICON = 1 << 4
};

enum {
  TF_ACTIVE = 1 << 0,
  TF_C4_SOURCE = 1 << 1,
  TF_C4_TARGET = 1 << 2,
  TF_DEFUSER_SOURCE = 1 << 3,
  TF_DEFUSER_TARGET = 1 << 4,
  TF_SHIELD_SOURCE = 1 << 5,
  TF_SHIELD_TARGET = 1 << 6
};

enum _:PlayerData {
  pd_bot,
  pd_uses,
  pd_vip,
  pd_vip_player,
  pd_vip_owner,
  pd_bot_model,
  pd_flags,
  Float:pd_velocity[3]
}

enum _:TransferData {
  td_count,
  td_flags,
  Float:td_source_health,
  td_source_armor,
  ArmorType:td_source_armor_type,
  td_source_nvg,
  Float:td_health,
  td_armor,
  ArmorType:td_armor_type,
  td_nvg,
  td_items[MAX_TRANSFER_ITEMS],
  td_item_ammo[MAX_TRANSFER_ITEMS],
  td_item_ammo_target[MAX_TRANSFER_ITEMS],
  td_item_state[MAX_TRANSFER_ITEMS],
  td_target_ammo[MAX_TRANSFER_WEAPONS]
}

new g_player[MAX_CLIENTS + 1][PlayerData];
new g_transfer[MAX_CLIENTS + 1][TransferData];

bool:has_state (const id, const flags) {
  return bool:(g_player[id][pd_flags] & flags);
}

set_state (const id, const flags, bool:enabled) {
  if (enabled) {
    g_player[id][pd_flags] |= flags;
  }
  else {
    g_player[id][pd_flags] &= ~flags;
  }
}

bool:transfer_flag (const id, const flags) {
  return bool:(g_transfer[id][td_flags] & flags);
}

set_transfer_flag (const id, const flags, bool:enabled) {
  if (enabled) {
    g_transfer[id][td_flags] |= flags;
  }
  else {
    g_transfer[id][td_flags] &= ~flags;
  }
}

new bool:g_round_ended;
new bool:g_map_has_bomb_target;
new bool:g_map_has_vip_zone;
new g_max_players;
new g_hud_sync;

new HookChain:g_spawn_hook;
new HookChain:g_target_hook;
new HamHook:g_strip_hook;

new bool:g_enabled;
new bool:g_use_key;
new bool:g_team_only;
new bool:g_eye;
new bool:g_chase;
new g_max_uses;
new bool:g_allow_vip;
new bool:g_back_items;
new bool:g_chat;
new bool:g_bot_chat;
new bool:g_hud;
new bool:g_scoreboard;
new bool:g_autoswitch;
new bool:g_log;


new const g_model_names[][] = {
  "", "urban", "terror", "leet", "arctic", "gsg9",
  "gign", "sas", "guerilla", "vip", "militia", "spetsnaz"
};

new const any:g_default_ct_models[] = {
  MODEL_CT_URBAN, MODEL_CT_GSG9, MODEL_CT_GIGN, MODEL_CT_SAS
};

new const g_bot_chat_keys[][] = {
  "TAKEOVER_BOT_CHAT1",
  "TAKEOVER_BOT_CHAT2",
  "TAKEOVER_BOT_CHAT3",
  "TAKEOVER_BOT_CHAT4",
  "TAKEOVER_BOT_CHAT5"
};

public plugin_init () {
  register_plugin (PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR);

  if (!yb_api_at_least (3, 2)) {
    set_fail_state ("YaPB API 3.2 or newer is required");
    return;
  }

  bind_pcvar_num (create_cvar ("yb_takeover_enabled", "1", FCVAR_NONE,
    "Enable bot takeover"), g_enabled);
  bind_pcvar_num (create_cvar ("yb_takeover_use_key", "0", FCVAR_NONE,
    "Control key: 0 = N, 1 = E"), g_use_key);
  bind_pcvar_num (create_cvar ("yb_takeover_team_only", "1", FCVAR_NONE,
    "Allow taking only same-team bots"), g_team_only);
  bind_pcvar_num (create_cvar ("yb_takeover_eye", "1", FCVAR_NONE,
    "Allow takeover in first-person observer mode"), g_eye);
  bind_pcvar_num (create_cvar ("yb_takeover_chase", "1", FCVAR_NONE,
    "Allow takeover in free chase observer mode"), g_chase);
  bind_pcvar_num (create_cvar ("yb_takeover_max_uses", "0", FCVAR_NONE,
    "Maximum takeovers per round, 0 = unlimited"), g_max_uses);
  bind_pcvar_num (create_cvar ("yb_takeover_allow_vip", "0", FCVAR_NONE,
    "Allow taking VIP bots"), g_allow_vip);
  bind_pcvar_num (create_cvar ("yb_takeover_back_items", "0", FCVAR_NONE,
    "Return useful weapons to the bot on round restart"), g_back_items);
  bind_pcvar_num (create_cvar ("yb_takeover_chat", "1", FCVAR_NONE,
    "Announce takeover in chat"), g_chat);
  bind_pcvar_num (create_cvar ("yb_takeover_bot_chat", "1", FCVAR_NONE,
    "Show a bot chat reaction on takeover"), g_bot_chat);
  bind_pcvar_num (create_cvar ("yb_takeover_hud", "1", FCVAR_NONE,
    "Show observer takeover hints"), g_hud);
  bind_pcvar_num (create_cvar ("yb_takeover_scoreboard", "0", FCVAR_NONE,
    "Mirror controlled bot status on the scoreboard"), g_scoreboard);
  bind_pcvar_num (create_cvar ("yb_takeover_autoswitch", "0", FCVAR_NONE,
    "Auto-spectate a random teammate bot after death"), g_autoswitch);
  bind_pcvar_num (create_cvar ("yb_takeover_log", "0", FCVAR_NONE,
    "Write takeover events to yapb_takeover.log"), g_log);
  create_cvar ("yb_takeover_stop_botfeatures", "1", FCVAR_NONE,
    "Pause BotFeatures if it is loaded");

  AutoExecConfig (true, "yapb_takeover");
  register_dictionary ("yapb_takeover.txt");

  g_max_players = get_maxplayers ();
  g_map_has_bomb_target = bool:get_member_game (m_bMapHasBombTarget);
  g_map_has_vip_zone = bool:get_member_game (m_bMapHasVIPSafetyZone);
  g_hud_sync = CreateHudSyncObj ();

  register_clcmd ("nightvision", "cmd_nightvision");
  register_message (get_user_msgid ("ScoreAttrib"), "msg_score_attrib");
  register_message (get_user_msgid ("HealthInfo"), "msg_health_info");

  RegisterHookChain (RG_CBasePlayer_Observer_Think, "on_observer_think", .post = true);
  RegisterHookChain (RG_RoundEnd, "on_round_end", .post = true);
  RegisterHookChain (RG_CSGameRules_RestartRound, "on_restart_pre", .post = false);
  RegisterHookChain (RG_CSGameRules_RestartRound, "on_restart_post", .post = true);
  RegisterHookChain (RG_CSGameRules_PlayerKilled, "on_player_killed", .post = false);
  RegisterHookChain (RG_CBasePlayer_Killed, "on_player_killed_post", .post = true);

  g_target_hook = RegisterHookChain (RG_CBasePlayer_Observer_IsValidTarget, "on_observer_target", .post = true);

  g_spawn_hook = RegisterHookChain (RG_CSGameRules_GetPlayerSpawnSpot, "on_spawn_spot", .post = false);
  DisableHookChain (g_spawn_hook);

  if (rg_find_ent_by_class (NULLENT, "player_weaponstrip", true) > 0) {
    g_strip_hook = RegisterHam (Ham_Use, "player_weaponstrip", "on_weaponstrip", .Post = false);
    DisableHamForward (g_strip_hook);
  }
}

public plugin_cfg () {
  if (get_cvar_num ("yb_takeover_stop_botfeatures")) {
    stop_botfeatures ();
  }
}

stop_botfeatures () {
  new file[64], name[64];

  for (new plugin = 0; plugin < get_pluginsnum (); plugin++) {
    get_plugin (plugin, file, charsmax (file), name, charsmax (name));

    if (get_func_id ("CBasePlayer_Observer_Think", plugin) == -1 &&
        get_func_id ("ClCmd_ControlBot", plugin) == -1) {
      continue;
    }

    if (pause ("ac", file)) {
      log_to_file (LOG_FILE, "Paused BotFeatures: %s", file);
    }
    else {
      log_to_file (LOG_FILE, "Failed to pause BotFeatures: %s", file);
    }
  }
}

public client_putinserver (id) {
  g_player[id][pd_bot] = 0;
  g_player[id][pd_uses] = 0;
  g_player[id][pd_vip] = false;
  g_player[id][pd_vip_player] = false;
  g_player[id][pd_vip_owner] = 0;
  g_player[id][pd_bot_model] = 0;
  clear_transfer_state (id);
  set_state(id, PF_SIM_OFF, false);
  set_state(id, PF_SPAWN_PENDING, false);
  set_state(id, PF_SPAWN_VELOCITY, false);
  set_state(id, PF_SPAWN_ICON, false);
  set_state(id, PF_SPAWN_DONE, false);
}

public yb_bot_spawned (index) {
  restore_bot_simulation (index);
}

public yb_bot_removed (index, bool:kicked) {
  set_state(index, PF_SIM_OFF, false);

  for (new player = 1; player <= g_max_players; player++) {
    if (is_user_connected (player) && g_player[player][pd_bot] == index) {
      rollback_vip (player, index);
      full_reset_player (player, false);
    }
  }
}

public client_disconnected (id) {
  remove_task (id);

  new controlled_bot = g_player[id][pd_bot];

  if (transfer_flag(id, TF_ACTIVE) && controlled_bot > 0 && controlled_bot <= g_max_players &&
      is_user_connected (controlled_bot) && yb_is_user_bot (controlled_bot)) {
    rollback_transfer (id, controlled_bot);
  }

  if (controlled_bot > 0 && controlled_bot <= g_max_players) {
    restore_bot_simulation (controlled_bot);
    rollback_vip (id, controlled_bot);
  }

  if (yb_is_user_bot (id) || has_state(id, PF_SIM_OFF)) {
    set_state(id, PF_SIM_OFF, false);

    for (new player = 1; player <= g_max_players; player++) {
      if (is_user_connected (player) && g_player[player][pd_bot] == id) {
        rollback_vip (player, id);
        full_reset_player (player, false);
      }
    }
  }

  g_player[id][pd_bot] = 0;
  g_player[id][pd_uses] = 0;
  g_player[id][pd_vip] = false;
  g_player[id][pd_vip_player] = false;
  g_player[id][pd_vip_owner] = 0;
  g_player[id][pd_bot_model] = 0;
  clear_transfer_state (id);
  set_state(id, PF_SIM_OFF, false);
  set_spawn_pending (id, false);
  set_state(id, PF_SPAWN_VELOCITY, false);
  set_state(id, PF_SPAWN_ICON, false);
  set_state(id, PF_SPAWN_DONE, false);
}

public cmd_nightvision (id) {
  if (g_use_key) {
    return PLUGIN_CONTINUE;
  }

  if (is_user_alive (id)) {
    return PLUGIN_CONTINUE;
  }

  new bot = can_takeover (id);

  if (!bot) {
    return PLUGIN_CONTINUE;
  }

  takeover (id, bot);
  return PLUGIN_HANDLED;
}

public on_observer_think (const id) {
  if (!g_use_key) {
    return HC_CONTINUE;
  }

  if ((get_member (id, m_afButtonPressed) & IN_USE)) {
    new bot = can_takeover (id);

    if (bot) {
      takeover (id, bot);
    }
  }

  return HC_CONTINUE;
}

public on_observer_target (const id, const target, const bool:same_team) {
  if (!g_hud || g_round_ended) {
    return;
  }

  if (!g_enabled) {
    ClearSyncHud (id, g_hud_sync);
    return;
  }

  new bot = GetHookChainReturn (ATYPE_INTEGER);

  if (bot <= 0 || !yb_is_user_bot (bot) || !yb_is_bot_alive (bot) || yb_is_bot_stale (bot) || !observer_mode_allowed (id)) {
    ClearSyncHud (id, g_hud_sync);
    return;
  }

  new player_team = get_user_team (id);
  new bot_team = get_user_team (bot);

  if (player_team != 1 && player_team != 2) {
    ClearSyncHud (id, g_hud_sync);
    return;
  }

  if ((g_team_only && player_team != bot_team) || bot_controller (bot) > 0) {
    ClearSyncHud (id, g_hud_sync);
    return;
  }

  new name[32], text[128];
  get_user_name (bot, name, charsmax (name));

  if (g_map_has_vip_zone && get_member (bot, m_bIsVIP) && !g_allow_vip) {
    format (text, charsmax (text), "%L", id, "TAKEOVER_VIP_DENIED");
    set_hudmessage (100, 100, 255, -1.0, 0.60, 0, 0.0, 1.5, 0.5, 0.5, -1);
    ShowSyncHudMsg (id, g_hud_sync, "%s", text);
    return;
  }

  format (text, charsmax (text), "%L", id, g_use_key ? "TAKEOVER_PROMPT_E" : "TAKEOVER_PROMPT_N", name);

  set_hudmessage (25, 200, 20, -1.0, 0.60, 0, 0.0, 1.5, 0.5, 0.5, -1);
  ShowSyncHudMsg (id, g_hud_sync, "%s", text);
}

public on_round_end (const WinStatus:status, const ScenarioEventEndRound:event, const Float:delay) {
  g_round_ended = true;
  DisableHookChain (g_target_hook);
}

public on_restart_pre () {
  new players[32], count;
  get_players (players, count, "ch");

  g_round_ended = false;
  EnableHookChain (g_target_hook);
  DisableHookChain (g_spawn_hook);

  for (new slot_player = 1; slot_player <= g_max_players; slot_player++) {
    set_state(slot_player, PF_SPAWN_PENDING, false);
  }

  new bool:complete = bool:get_member_game (m_bCompleteReset);
  new bool:back_items = g_back_items;

  for (new i = 0; i < count; i++) {
    new id = players[i];
    new bot = g_player[id][pd_bot];
    new bool:back_transfer_failed;

    if (bot > 0 && is_user_connected (bot) && yb_is_user_bot (bot) && !yb_is_bot_stale (bot)) {
      if (back_items && !complete) {
        new transferred = transfer_inventory (id, bot, true);

        if (transferred < 0) {
          back_transfer_failed = true;
          rollback_transfer (bot, id);
          clear_transfer_state (bot);
          log_takeover ("Back transfer failed: player=%d bot=%d", id, bot);
        }
        else {
          clear_transfer_state (bot);

          if (transferred > 0 && (transferred & USEFUL_SLOTS_BS)) {
            set_member (bot, m_bNotKilled, true);
          }
        }
      }

      set_member (id, m_bNotKilled, false);
    }

    if (g_player[id][pd_uses] > 0) {
      full_reset_player (id, !back_transfer_failed);
    }
  }
}

public on_restart_post () {
  for (new id = 1; id <= g_max_players; id++) {
    if (!has_state(id, PF_SIM_OFF)) {
      continue;
    }

    restore_bot_simulation (id);
  }
}

public on_player_killed (const victim, const killer, const inflictor) {
  if (victim < 1 || victim > g_max_players) {
    return HC_CONTINUE;
  }

  new victim_bot = g_player[victim][pd_bot];

  if (victim_bot > 0) {
    g_player[victim][pd_bot] = 0;
  }

  if (victim == killer) {
    return HC_CONTINUE;
  }

  if (victim_bot > 0 && is_user_connected (victim_bot) && yb_is_user_bot (victim_bot) && !yb_is_bot_stale (victim_bot)) {
    SetHookChainArg (1, ATYPE_INTEGER, victim_bot);
  }

  new killer_bot = (killer > 0 && killer <= g_max_players) ? g_player[killer][pd_bot] : 0;

  if (killer_bot > 0 && is_user_connected (killer_bot) && yb_is_user_bot (killer_bot) && !yb_is_bot_stale (killer_bot)) {
    SetHookChainArg (2, ATYPE_INTEGER, killer_bot);

    if (killer == inflictor) {
      SetHookChainArg (3, ATYPE_INTEGER, get_member (killer, m_pActiveItem));
    }

    set_member (killer, m_flNextDecalTime, get_gametime ());
  }

  return HC_CONTINUE;
}

public on_player_killed_post (const victim, const attacker, const gib) {
  if (victim < 1 || victim > g_max_players || !g_autoswitch || yb_is_user_bot (victim)) {
    return;
  }

  new bot = random_teammate (get_user_team (victim));

  if (!bot) {
    return;
  }

  new Float:time = get_gametime ();

  set_entvar (victim, var_deadflag, DEAD_DEAD);
  set_member (victim, m_fDeadTime, time - 3.1);
  set_member (victim, m_hObserverTarget, bot);
  ExecuteHam (Ham_Think, victim);
  rg_internal_cmd (victim, "specmode", "4");
  set_member (victim, m_flNextObserverInput, time + 2.0);
}

public on_spawn_spot (const id) {
  if (id < 1 || id > g_max_players || !has_state(id, PF_SPAWN_PENDING)) {
    return HC_CONTINUE;
  }

  set_spawn_pending (id, false);

  new bot = g_player[id][pd_bot];

  if (bot <= 0) {
    return HC_CONTINUE;
  }

  if (!is_user_connected (bot) || !yb_is_user_bot (bot) || yb_is_bot_stale (bot)) {
    abort_takeover (id, bot);
    return HC_CONTINUE;
  }

  if (g_round_ended) {
    abort_takeover (id, bot);
    return HC_CONTINUE;
  }

  new Float:origin[3], Float:angles[3], Float:mins[3], Float:maxs[3];
  new hull = (get_entvar (bot, var_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN;

  if (!yb_get_bot_origin (bot, origin)) {
    abort_takeover (id, bot);
    return HC_CONTINUE;
  }

  get_entvar (bot, var_v_angle, angles);
  get_entvar (bot, var_mins, mins);
  get_entvar (bot, var_maxs, maxs);

  if (!set_player_position (id, origin, angles, mins, maxs, hull, bot, g_player[id][pd_velocity])) {
    client_print_color (id, print_team_default, "%L", id, "TAKEOVER_NO_SPACE");
    abort_takeover (id, bot);
    return HC_CONTINUE;
  }

  if (transfer_inventory (bot, id, false) < 0) {
    abort_takeover (id, bot);
    return HC_CONTINUE;
  }

  clear_transfer_state (id);
  set_state(id, PF_SPAWN_DONE, true);
  set_state(id, PF_SPAWN_VELOCITY, true);
  set_state(id, PF_SPAWN_ICON, true);
  set_task (0.1, "apply_spawn_state", id);
  kill_bot (bot, origin);

  SetHookChainReturn (ATYPE_INTEGER, id);
  return HC_SUPERCEDE;
}

public msg_score_attrib (const msg_id, const msg_dest, const msg_ent) {
  if (!g_scoreboard) {
    return PLUGIN_CONTINUE;
  }

  new flags = get_msg_arg_int (2);
  new player = get_msg_arg_int (1);

  if (player <= 0 || player > g_max_players) {
    return PLUGIN_CONTINUE;
  }

  if (g_player[player][pd_bot] > 0 && !(flags & SCORE_STATUS_DEAD)) {
    set_msg_arg_int (2, ARG_BYTE, flags | SCORE_STATUS_DEAD);
  }

  if (yb_is_user_bot (player) && (flags & SCORE_STATUS_DEAD) && bot_controller (player) > 0) {
    set_msg_arg_int (2, ARG_BYTE, flags & ~SCORE_STATUS_DEAD);
  }

  return PLUGIN_CONTINUE;
}

public msg_health_info (const msg_id, const msg_dest, const msg_ent) {
  if (!g_scoreboard) {
    return PLUGIN_CONTINUE;
  }

  new bot = get_msg_arg_int (1);

  if (bot <= 0 || !yb_is_user_bot (bot) || get_msg_arg_int (2) != 1) {
    return PLUGIN_CONTINUE;
  }

  new player = bot_controller (bot);

  if (player > 0) {
    new health = get_user_health (player);

    if (health > 0) {
      set_msg_arg_int (2, ARG_LONG, health);
    }
  }

  return PLUGIN_CONTINUE;
}

public on_weaponstrip (const entity, const activator, const caller, const use_type, const Float:value) {
  if (activator < 1 || activator > g_max_players || !is_user_connected (activator) || g_player[activator][pd_bot] <= 0) {
    return HAM_IGNORED;
  }

  DisableHamForward (g_strip_hook);
  return HAM_SUPERCEDE;
}

can_takeover (id) {
  if (!g_enabled || g_round_ended || is_user_alive (id)) {
    return 0;
  }

  new bot = get_entvar (id, var_iuser2);

  if (bot <= 0 || bot == id || !yb_is_user_bot (bot) || !yb_is_bot_alive (bot)) {
    return 0;
  }

  if (yb_is_bot_stale (bot) || !yb_is_bot_simulating (bot)) {
    return 0;
  }

  if (!observer_mode_allowed (id)) {
    return 0;
  }

  new player_team = get_user_team (id);
  new bot_team = get_user_team (bot);

  if (player_team != 1 && player_team != 2) {
    return 0;
  }

  if (g_team_only && player_team != bot_team) {
    return 0;
  }

  if (bot_controller (bot) > 0) {
    client_print_color (id, print_team_default, "%L", id, "TAKEOVER_BUSY");
    return 0;
  }

  new max_uses = g_max_uses;

  if (max_uses > 0 && g_player[id][pd_uses] >= max_uses) {
    client_print_color (id, print_team_default, "%L", id, "TAKEOVER_LIMIT");
    return 0;
  }

  if (g_map_has_vip_zone && get_member (bot, m_bIsVIP) && !g_allow_vip) {
    return 0;
  }

  return bot;
}

observer_mode_allowed (id) {
  new mode = get_entvar (id, var_iuser1);

  if (mode == OBS_IN_EYE) {
    return g_eye;
  }

  if (mode == OBS_CHASE_FREE) {
    return g_chase;
  }

  return 0;
}

bot_controller (bot) {
  for (new player = 1; player <= g_max_players; player++) {
    if (!yb_is_user_bot (player) && is_user_connected (player) && g_player[player][pd_bot] == bot) {
      if (has_state(player, PF_SPAWN_PENDING) || is_user_alive (player)) {
        return player;
      }
    }
  }

  return 0;
}

clear_transfer_state (player) {
  set_transfer_flag(player, TF_ACTIVE, false);
  g_transfer[player][td_count] = 0;
  g_transfer[player][td_source_health] = 0.0;
  g_transfer[player][td_source_armor] = 0;
  g_transfer[player][td_source_armor_type] = ARMOR_NONE;
  g_transfer[player][td_source_nvg] = false;
  g_transfer[player][td_health] = 0.0;
  g_transfer[player][td_armor] = 0;
  g_transfer[player][td_armor_type] = ARMOR_NONE;
  g_transfer[player][td_nvg] = false;
  set_transfer_flag(player, TF_C4_SOURCE, false);
  set_transfer_flag(player, TF_C4_TARGET, false);
  set_transfer_flag(player, TF_DEFUSER_SOURCE, false);
  set_transfer_flag(player, TF_DEFUSER_TARGET, false);
  set_transfer_flag(player, TF_SHIELD_SOURCE, false);
  set_transfer_flag(player, TF_SHIELD_TARGET, false);

  for (new index = 0; index < MAX_TRANSFER_ITEMS; index++) {
    g_transfer[player][td_items][index] = 0;
    g_transfer[player][td_item_ammo][index] = 0;
    g_transfer[player][td_item_ammo_target][index] = 0;
    g_transfer[player][td_item_state][index] = TRANSFER_ITEM_NONE;
  }

  for (new weapon = 0; weapon < MAX_TRANSFER_WEAPONS; weapon++) {
    g_transfer[player][td_target_ammo][weapon] = 0;
  }
}

rollback_transfer (receiver, source) {
  if (receiver <= 0 || receiver > g_max_players || source <= 0 || source > g_max_players) {
    log_takeover ("Rollback invalid indices: receiver=%d source=%d", receiver, source);
    return;
  }

  if (!is_user_connected (receiver) || !is_user_connected (source)) {
    log_takeover ("Rollback disconnected indices: receiver=%d source=%d", receiver, source);
    return;
  }

  if (!transfer_flag(receiver, TF_ACTIVE)) {
    log_takeover ("Rollback skipped inactive journal: receiver=%d source=%d", receiver, source);
    return;
  }

  while (g_transfer[receiver][td_count] > 0) {
    new index = --g_transfer[receiver][td_count];
    new item = g_transfer[receiver][td_items][index];
    new item_state = g_transfer[receiver][td_item_state][index];
    new ammo = g_transfer[receiver][td_item_ammo][index];
    new target_ammo = g_transfer[receiver][td_item_ammo_target][index];

    g_transfer[receiver][td_items][index] = 0;
    g_transfer[receiver][td_item_state][index] = TRANSFER_ITEM_NONE;

    if (item_state == TRANSFER_ITEM_NONE) {
      continue;
    }

    if (item <= 0 || is_nullent (item)) {
      log_takeover ("Rollback item entity unavailable: receiver=%d source=%d state=%d", receiver, source, item_state);
      continue;
    }

    new WeaponIdType:id = WeaponIdType:get_member (item, m_iId);
    new bool:restored;

    if (item_state == TRANSFER_ITEM_MOVED) {
      if (ExecuteHam (Ham_RemovePlayerItem, receiver, item)) {
        if (ExecuteHam (Ham_AddPlayerItem, source, item)) {
          ExecuteHam (Ham_Item_AttachToPlayer, item, source);
          restored = true;
        }
      }

      if (!restored && ExecuteHam (Ham_AddPlayerItem, source, item)) {
        ExecuteHam (Ham_Item_AttachToPlayer, item, source);
        restored = true;
      }
    }
    else {
      if (ExecuteHam (Ham_AddPlayerItem, source, item)) {
        ExecuteHam (Ham_Item_AttachToPlayer, item, source);
        restored = true;
      }

      if (!restored && ExecuteHam (Ham_RemovePlayerItem, receiver, item) && ExecuteHam (Ham_AddPlayerItem, source, item)) {
        ExecuteHam (Ham_Item_AttachToPlayer, item, source);
        restored = true;
      }
    }

    if (!restored) {
      if (ExecuteHam (Ham_AddPlayerItem, receiver, item)) {
        ExecuteHam (Ham_Item_AttachToPlayer, item, receiver);
        log_takeover ("Rollback retained item on receiver: receiver=%d source=%d item=%d", receiver, source, item);
      }
      else {
        log_takeover ("Rollback item recovery failed: receiver=%d source=%d item=%d state=%d", receiver, source, item, item_state);
      }
    }

    if (id != WeaponIdType:0 && _:id >= 0 && _:id < MAX_TRANSFER_WEAPONS) {
      rg_set_user_bpammo (source, id, ammo);
      rg_set_user_bpammo (receiver, id, target_ammo);
    }
  }

  new bool:source_c4 = bool:get_member (source, m_bHasC4);
  new bool:target_c4 = bool:get_member (receiver, m_bHasC4);

  if (transfer_flag(receiver, TF_C4_SOURCE) && !source_c4 && target_c4) {
    rg_transfer_c4 (receiver, source);
  }
  else if (!transfer_flag(receiver, TF_C4_SOURCE) && source_c4) {
    rg_remove_item (source, "weapon_c4");
  }

  source_c4 = bool:get_member (source, m_bHasC4);
  target_c4 = bool:get_member (receiver, m_bHasC4);

  if (transfer_flag(receiver, TF_C4_TARGET) && !target_c4 && source_c4) {
    rg_transfer_c4 (source, receiver);
  }
  else if (!transfer_flag(receiver, TF_C4_TARGET) && target_c4) {
    rg_remove_item (receiver, "weapon_c4");
  }

  if (bool:get_member (source, m_bHasC4) != transfer_flag(receiver, TF_C4_SOURCE) ||
      bool:get_member (receiver, m_bHasC4) != transfer_flag(receiver, TF_C4_TARGET)) {
    log_takeover ("Rollback C4 mismatch: receiver=%d source=%d", receiver, source);
  }

  new bool:source_defuser = bool:get_member (source, m_bHasDefuser);
  new bool:target_defuser = bool:get_member (receiver, m_bHasDefuser);

  if (transfer_flag(receiver, TF_DEFUSER_SOURCE) && !source_defuser) {
    rg_give_defusekit (source);
    source_defuser = bool:get_member (source, m_bHasDefuser);
  }
  else if (!transfer_flag(receiver, TF_DEFUSER_SOURCE) && source_defuser) {
    rg_remove_item (source, "item_thighpack");
    source_defuser = bool:get_member (source, m_bHasDefuser);
  }

  if (source_defuser && !transfer_flag(receiver, TF_DEFUSER_TARGET) && target_defuser) {
    rg_remove_item (receiver, "item_thighpack");
  }
  else if (transfer_flag(receiver, TF_DEFUSER_TARGET) && !target_defuser) {
    rg_give_defusekit (receiver);
  }
  else if (!transfer_flag(receiver, TF_DEFUSER_TARGET) && target_defuser) {
    rg_remove_item (receiver, "item_thighpack");
  }

  source_defuser = bool:get_member (source, m_bHasDefuser);
  target_defuser = bool:get_member (receiver, m_bHasDefuser);

  if (source_defuser != transfer_flag(receiver, TF_DEFUSER_SOURCE) || target_defuser != transfer_flag(receiver, TF_DEFUSER_TARGET)) {
    log_takeover ("Rollback defuser mismatch: receiver=%d source=%d", receiver, source);
  }

  new bool:source_shield = bool:get_member (source, m_bOwnsShield);
  new bool:target_shield = bool:get_member (receiver, m_bOwnsShield);

  if (transfer_flag(receiver, TF_SHIELD_SOURCE) && !source_shield) {
    rg_give_shield (source);
    source_shield = bool:get_member (source, m_bOwnsShield);
  }
  else if (!transfer_flag(receiver, TF_SHIELD_SOURCE) && source_shield) {
    rg_remove_item (source, "weapon_shield");
    source_shield = bool:get_member (source, m_bOwnsShield);
  }

  if (source_shield && !transfer_flag(receiver, TF_SHIELD_TARGET) && target_shield) {
    rg_remove_item (receiver, "weapon_shield");
  }
  else if (transfer_flag(receiver, TF_SHIELD_TARGET) && !target_shield) {
    rg_give_shield (receiver);
  }
  else if (!transfer_flag(receiver, TF_SHIELD_TARGET) && target_shield) {
    rg_remove_item (receiver, "weapon_shield");
  }

  source_shield = bool:get_member (source, m_bOwnsShield);
  target_shield = bool:get_member (receiver, m_bOwnsShield);

  if (source_shield != transfer_flag(receiver, TF_SHIELD_SOURCE) || target_shield != transfer_flag(receiver, TF_SHIELD_TARGET)) {
    log_takeover ("Rollback shield mismatch: receiver=%d source=%d", receiver, source);
  }

  if (g_transfer[receiver][td_source_armor] > 0 && g_transfer[receiver][td_source_armor_type] != ARMOR_NONE) {
    rg_set_user_armor (source, g_transfer[receiver][td_source_armor], g_transfer[receiver][td_source_armor_type]);
  }
  else {
    rg_set_user_armor (source, 0, ARMOR_NONE);
  }

  set_member (source, m_bHasNightVision, g_transfer[receiver][td_source_nvg]);
  set_entvar (source, var_health, g_transfer[receiver][td_source_health]);

  if (g_transfer[receiver][td_armor] > 0 && g_transfer[receiver][td_armor_type] != ARMOR_NONE) {
    rg_set_user_armor (receiver, g_transfer[receiver][td_armor], g_transfer[receiver][td_armor_type]);
  }
  else {
    rg_set_user_armor (receiver, 0, ARMOR_NONE);
  }

  set_member (receiver, m_bHasNightVision, g_transfer[receiver][td_nvg]);
  set_entvar (receiver, var_health, g_transfer[receiver][td_health]);

  set_member (source, m_bHasPrimary, get_member (source, m_rgpPlayerItems, PRIMARY_WEAPON_SLOT) > 0);
  set_member (receiver, m_bHasPrimary, get_member (receiver, m_rgpPlayerItems, PRIMARY_WEAPON_SLOT) > 0);

  set_transfer_flag(receiver, TF_ACTIVE, false);
}

rollback_vip (id, bot) {
  if (!g_player[id][pd_vip]) {
    return;
  }

  set_member_game (m_pVIP, g_player[id][pd_vip_owner]);
  set_member (id, m_bIsVIP, bool:g_player[id][pd_vip_player]);

  if (bot > 0 && bot <= g_max_players && is_user_connected (bot) && yb_is_user_bot (bot)) {
    set_member (bot, m_bIsVIP, true);
    set_member (bot, m_iModelName, g_player[id][pd_bot_model]);
  }

  g_player[id][pd_vip] = false;
  g_player[id][pd_vip_player] = false;
  g_player[id][pd_vip_owner] = 0;
  g_player[id][pd_bot_model] = 0;
}

restore_bot_simulation (bot) {
  if (bot <= 0 || bot > g_max_players || !has_state(bot, PF_SIM_OFF)) {
    return;
  }

  if (!is_user_connected (bot) || !yb_is_user_bot (bot)) {
    set_state(bot, PF_SIM_OFF, false);
    return;
  }

  if (yb_is_bot_stale (bot) || !yb_is_bot_alive (bot)) {
    return;
  }

  if (yb_set_bot_simulation (bot, true)) {
    set_state(bot, PF_SIM_OFF, false);
    log_takeover ("SimulationOn: bot=%d", bot);
  }
  else {
    log_takeover ("SimulationOn failed: bot=%d", bot);
  }
}

has_pending_spawn () {
  for (new player = 1; player <= g_max_players; player++) {
    if (has_state(player, PF_SPAWN_PENDING)) {
      return true;
    }
  }

  return false;
}

set_spawn_pending (id, bool:pending) {
  set_state(id, PF_SPAWN_PENDING, pending);

  if (pending) {
    EnableHookChain (g_spawn_hook);
  }
  else if (!has_pending_spawn ()) {
    DisableHookChain (g_spawn_hook);
  }
}

takeover (id, bot) {
  clear_transfer_state (id);
  set_state(id, PF_SPAWN_VELOCITY, false);
  set_state(id, PF_SPAWN_ICON, false);
  get_entvar (bot, var_velocity, g_player[id][pd_velocity]);

  if (!yb_set_bot_simulation (bot, false)) {
    log_takeover ("SimulationOff failed: player=%d bot=%d", id, bot);
    return;
  }

  set_state(bot, PF_SIM_OFF, true);

  new model = _:get_member (bot, m_iModelName);

  if (model < 0 || model >= sizeof (g_model_names)) {
    model = 0;
  }

  g_player[id][pd_vip] = false;

  if (g_map_has_vip_zone && get_member (bot, m_bIsVIP)) {
    g_player[id][pd_vip] = true;
    g_player[id][pd_vip_player] = bool:get_member (id, m_bIsVIP);
    g_player[id][pd_vip_owner] = get_member_game (m_pVIP);
    g_player[id][pd_bot_model] = model;

    set_member_game (m_pVIP, id);
    set_member (id, m_bIsVIP, true);
    set_member (id, m_bNotKilled, false);
    set_member (bot, m_bIsVIP, false);
    set_member (bot, m_iModelName, g_default_ct_models[random (sizeof (g_default_ct_models))]);
  }
  else {
    set_member (id, m_bNotKilled, true);
  }

  if (!g_model_names[model][0]) {
    rg_set_user_model (id, get_user_team (id) == 2 ? "terror" : "urban", true);
  }
  else {
    rg_set_user_model (id, g_model_names[model], true);
  }

  g_player[id][pd_bot] = bot;
  g_player[id][pd_uses]++;

  if (g_strip_hook > HamHook:0) {
    EnableHamForward (g_strip_hook);
  }

  new Float:origin[3];
  new hull = (get_entvar (bot, var_flags) & FL_DUCKING) ? HULL_HEAD : HULL_HUMAN;

  if (!yb_get_bot_origin (bot, origin) || !is_free_space (origin, hull, bot)) {
    abort_takeover (id, bot);
    return;
  }

  remove_task (id);
  set_spawn_pending (id, true);
  set_state(id, PF_SPAWN_DONE, false);
  rg_round_respawn (id);
  set_task (1.0, "validate_takeover", id);

  log_takeover ("Takeover: player=%d bot=%d uses=%d", id, bot, g_player[id][pd_uses]);

  if (g_chat) {
    announce_takeover (id, bot);
  }

  if (g_bot_chat) {
    new bot_name[32], text[128];
    get_user_name (bot, bot_name, charsmax (bot_name));
    format (text, charsmax (text), "%L", id, g_bot_chat_keys[random (sizeof (g_bot_chat_keys))], bot_name);
    client_print_color (id, print_team_default, "%s", text);
  }
}

public apply_spawn_state (id) {
  if (!is_user_connected (id)) {
    set_state(id, PF_SPAWN_VELOCITY, false);
    set_state(id, PF_SPAWN_ICON, false);
    return;
  }

  if (!has_state(id, PF_SPAWN_DONE)) {
    if (has_state(id, PF_SPAWN_VELOCITY) || has_state(id, PF_SPAWN_ICON)) {
      set_task (0.1, "apply_spawn_state", id);
    }

    return;
  }

  if (has_state(id, PF_SPAWN_VELOCITY)) {
    set_entvar (id, var_velocity, g_player[id][pd_velocity]);
    set_state(id, PF_SPAWN_VELOCITY, false);
  }

  if (has_state(id, PF_SPAWN_ICON)) {
    send_scenario_icon (id, "hostage1", 1, 150);
    set_state(id, PF_SPAWN_ICON, false);
  }
}

public validate_takeover (id) {
  if (!is_user_connected (id)) {
    return;
  }

  new bot = g_player[id][pd_bot];

  if (bot > 0 && !has_state(id, PF_SPAWN_DONE)) {
    abort_takeover (id, bot);
  }
}

abort_takeover (id, bot) {
  set_spawn_pending (id, false);

  if (g_strip_hook > HamHook:0) {
    DisableHamForward (g_strip_hook);
  }

  if (transfer_flag(id, TF_ACTIVE) && bot > 0 && bot <= g_max_players && is_user_connected (bot) && yb_is_user_bot (bot)) {
    rollback_transfer (id, bot);
  }

  rollback_vip (id, bot);
  restore_bot_simulation (bot);

  set_member (id, m_bNotKilled, false);
  rg_reset_user_model (id, true);

  if (g_player[id][pd_uses] > 0) {
    g_player[id][pd_uses]--;
  }

  g_player[id][pd_bot] = 0;
  clear_transfer_state (id);
  set_state(id, PF_SPAWN_VELOCITY, false);
  set_state(id, PF_SPAWN_ICON, false);
  set_state(id, PF_SPAWN_DONE, false);
  log_takeover ("Takeover aborted: player=%d bot=%d", id, bot);
}

full_reset_player (id, bool:remove_items = true) {
  remove_task (id);
  set_spawn_pending (id, false);

  if (g_strip_hook > HamHook:0) {
    DisableHamForward (g_strip_hook);
  }

  if (remove_items) {
    rg_remove_all_items (id);
  }

  set_member (id, m_bNotKilled, false);
  rg_reset_user_model (id, true);
  g_player[id][pd_bot] = 0;
  g_player[id][pd_uses] = 0;
  g_player[id][pd_vip] = false;
  g_player[id][pd_vip_player] = false;
  g_player[id][pd_vip_owner] = 0;
  g_player[id][pd_bot_model] = 0;
  clear_transfer_state (id);
  set_state(id, PF_SPAWN_VELOCITY, false);
  set_state(id, PF_SPAWN_ICON, false);
  set_state(id, PF_SPAWN_DONE, false);
}

transfer_inventory (source, receiver, bool:back) {
  if (source <= 0 || source > g_max_players || receiver <= 0 || receiver > g_max_players ||
      source == receiver || !is_user_connected (source) || !is_user_connected (receiver)) {
    return -1;
  }

  if (back && g_map_has_vip_zone && get_member (source, m_bIsVIP)) {
    return 0;
  }

  clear_transfer_state (receiver);

  new ArmorType:source_armor_type, ArmorType:receiver_armor_type;

  g_transfer[receiver][td_source_health] = Float:get_entvar (source, var_health);
  g_transfer[receiver][td_source_armor] = rg_get_user_armor (source, source_armor_type);
  g_transfer[receiver][td_source_armor_type] = source_armor_type;
  g_transfer[receiver][td_source_nvg] = bool:get_member (source, m_bHasNightVision);
  g_transfer[receiver][td_health] = Float:get_entvar (receiver, var_health);
  g_transfer[receiver][td_armor] = rg_get_user_armor (receiver, receiver_armor_type);
  g_transfer[receiver][td_armor_type] = receiver_armor_type;
  g_transfer[receiver][td_nvg] = bool:get_member (receiver, m_bHasNightVision);
  set_transfer_flag(receiver, TF_C4_SOURCE, bool:get_member (source, m_bHasC4));
  set_transfer_flag(receiver, TF_C4_TARGET, bool:get_member (receiver, m_bHasC4));
  set_transfer_flag(receiver, TF_DEFUSER_SOURCE, bool:get_member (source, m_bHasDefuser));
  set_transfer_flag(receiver, TF_DEFUSER_TARGET, bool:get_member (receiver, m_bHasDefuser));
  set_transfer_flag(receiver, TF_SHIELD_SOURCE, bool:get_member (source, m_bOwnsShield));
  set_transfer_flag(receiver, TF_SHIELD_TARGET, bool:get_member (receiver, m_bOwnsShield));
  set_transfer_flag(receiver, TF_ACTIVE, true);
  g_transfer[receiver][td_count] = 0;

  new bool:failed;

  for (new weapon = 0; weapon < MAX_TRANSFER_WEAPONS; weapon++) {
    g_transfer[receiver][td_target_ammo][weapon] = rg_get_user_bpammo (receiver, WeaponIdType:weapon);
  }

  if (!back) {
    if (receiver != source) {
      rg_remove_all_items (receiver);
    }

    set_entvar (receiver, var_health, get_entvar (source, var_health));
  }

  rg_set_user_armor (receiver, 0, ARMOR_NONE);
  set_member (receiver, m_bHasNightVision, false);

  new ArmorType:armor_type;
  new armor = rg_get_user_armor (source, armor_type);

  if (armor > 0 && armor_type != ARMOR_NONE) {
    rg_set_user_armor (receiver, armor, armor_type);
  }

  new ArmorType:check_armor_type;
  new check_armor = rg_get_user_armor (receiver, check_armor_type);

  if (armor > 0) {
    if (check_armor != armor || check_armor_type != armor_type) {
      failed = true;
    }
  }
  else if (check_armor != 0) {
    failed = true;
  }

  if (get_member (source, m_bHasNightVision)) {
    set_member (receiver, m_bHasNightVision, true);
  }

  if (bool:get_member (receiver, m_bHasNightVision) != bool:get_member (source, m_bHasNightVision)) {
    failed = true;
  }

  if (g_map_has_bomb_target) {
    if (!back && get_member (source, m_bHasC4)) {
      rg_transfer_c4 (source, receiver);

      if (get_member (source, m_bHasC4) || !get_member (receiver, m_bHasC4)) {
        failed = true;
      }
    }

    if (get_member (source, m_bHasDefuser)) {
      rg_give_defusekit (receiver);

      if (get_member (receiver, m_bHasDefuser)) {
        if (!rg_remove_item (source, "item_thighpack")) {
          failed = true;
        }
      }
      else {
        failed = true;
      }
    }
  }

  new slots;
  new WeaponIdType:id;
  new ammo;
  new target_ammo;
  new item;
  new next;
  new Float:release_throw;
  new InventorySlotType:slot;
  new active = get_member (source, m_pActiveItem);
  new bool:active_transferred;

  for (slot = PRIMARY_WEAPON_SLOT; slot <= GRENADE_SLOT; slot++) {
    item = get_member (source, m_rgpPlayerItems, slot);

    while (item > 0 && !is_nullent (item)) {
      next = get_member (item, m_pNext);
      id = WeaponIdType:0;
      ammo = 0;
      target_ammo = 0;

      if (g_transfer[receiver][td_count] >= MAX_TRANSFER_ITEMS) {
        failed = true;
        break;
      }

      if (slot == GRENADE_SLOT && active == item) {
        release_throw = Float:get_member (active, m_flReleaseThrow);

        if (release_throw == 0.0) {
          ExecuteHam (Ham_Item_Holster, active, 1);
          set_entvar (source, var_button, get_entvar (source, var_button) & ~IN_ATTACK);
        }
        else if (release_throw > 0.0) {
          ExecuteHam (Ham_Weapon_RetireWeapon, active);
          active = get_member (source, m_pActiveItem);
          item = next;
          continue;
        }
      }

      if (slot != KNIFE_SLOT) {
        id = WeaponIdType:get_member (item, m_iId);

        if (!(INVALID_WEAPONS_BS & (1 << any:id))) {
          ammo = rg_get_user_bpammo (source, id);
        }
      }
      else {
        id = WeaponIdType:WEAPON_KNIFE;
      }

      if (_:id >= 0 && _:id < MAX_TRANSFER_WEAPONS) {
        target_ammo = g_transfer[receiver][td_target_ammo][_:id];
      }

      new journal_index = g_transfer[receiver][td_count];

      g_transfer[receiver][td_items][journal_index] = item;
      g_transfer[receiver][td_item_ammo][journal_index] = ammo;
      g_transfer[receiver][td_item_ammo_target][journal_index] = target_ammo;
      g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_NONE;
      g_transfer[receiver][td_count] = journal_index + 1;

      if (ExecuteHam (Ham_RemovePlayerItem, source, item)) {
        g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_REMOVED;

        if (ExecuteHam (Ham_AddPlayerItem, receiver, item)) {
          g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_MOVED;

          user_has_weapon (source, any:id, 0);
          ExecuteHam (Ham_Item_AttachToPlayer, item, receiver);

          if (ammo > 0) {
            rg_set_user_bpammo (receiver, id, ammo);
            rg_set_user_bpammo (source, id, 0);
          }

          slots |= (1 << any:slot);

          if (active == item) {
            active_transferred = true;
          }
        }
        else {
          failed = true;

          if (ExecuteHam (Ham_AddPlayerItem, source, item)) {
            ExecuteHam (Ham_Item_AttachToPlayer, item, source);

            if (id != WeaponIdType:0) {
              rg_set_user_bpammo (source, id, ammo);
              rg_set_user_bpammo (receiver, id, target_ammo);
            }

            g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_NONE;
            log_takeover ("Transfer rolled back: source=%d receiver=%d item=%d", source, receiver, item);
          }
          else {
            g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_LOST;
            log_takeover ("Transfer rollback failed: source=%d receiver=%d item=%d", source, receiver, item);
          }
        }
      }
      else {
        if (id != WeaponIdType:0) {
          rg_set_user_bpammo (source, id, ammo);
          rg_set_user_bpammo (receiver, id, target_ammo);
        }

        g_transfer[receiver][td_item_state][journal_index] = TRANSFER_ITEM_NONE;
        failed = true;
        log_takeover ("Transfer remove failed: source=%d receiver=%d item=%d", source, receiver, item);
      }

      item = next;
    }
  }

  if (active_transferred && active > 0 && !is_nullent (active)) {
    rg_switch_weapon (receiver, active);
  }

  if (get_member (source, m_bOwnsShield)) {
    rg_give_shield (receiver);

    if (get_member (receiver, m_bOwnsShield)) {
      if (!rg_remove_item (source, "weapon_shield")) {
        failed = true;
      }
    }
    else {
      failed = true;
    }

    if (active_transferred && !yb_is_user_bot (receiver) && get_member (source, m_bShieldDrawn)) {
      ExecuteHam (Ham_Weapon_SecondaryAttack, active);
    }
  }

  set_member (source, m_bHasPrimary, get_member (source, m_rgpPlayerItems, PRIMARY_WEAPON_SLOT) > 0);
  set_member (receiver, m_bHasPrimary, get_member (receiver, m_rgpPlayerItems, PRIMARY_WEAPON_SLOT) > 0);

  if (back && !failed && source != receiver) {
    rg_set_user_armor (source, 0, ARMOR_NONE);
    set_member (source, m_bHasNightVision, false);
  }

  return failed ? -1 : slots;
}

kill_bot (bot, const Float:origin[3]) {
  set_member (bot, m_vBlastVector, origin);
  set_member (bot, m_bKilledByBomb, true);
  set_entvar (bot, var_effects, EF_NODRAW);
  dllfunc (DLLFunc_ClientKill, bot);

  log_takeover ("KillBot: bot=%d", bot);
}

bool:set_player_position (id, Float:origin[3], const Float:angles[3], const Float:mins[3], const Float:maxs[3], hull, skip, const Float:velocity[3]) {
  new Float:move[4][3] = {
    { 1.0, 1.0, 0.0 },
    { 1.0, -1.0, 0.0 },
    { -1.0, -1.0, 0.0 },
    { -1.0, 1.0, 0.0 }
  };

  new Float:base[3], Float:candidate[3];
  base[0] = origin[0];
  base[1] = origin[1];
  base[2] = origin[2];

  for (new attempt = 0; attempt < sizeof (move); attempt++) {
    candidate[0] = base[0] - mins[0] * move[attempt][0];
    candidate[1] = base[1] - mins[1] * move[attempt][1];
    candidate[2] = base[2];

    if (is_free_space (candidate, hull, skip)) {
      if (hull == HULL_HEAD) {
        set_entvar (id, var_flags, get_entvar (id, var_flags) | FL_DUCKING);
        set_entvar (id, var_button, get_entvar (id, var_button) | IN_DUCK);
      }

      engfunc (EngFunc_SetSize, id, mins, maxs);
      engfunc (EngFunc_SetOrigin, id, candidate);

      set_entvar (id, var_velocity, velocity);
      set_entvar (id, var_v_angle, Float:{0.0, 0.0, 0.0});
      set_entvar (id, var_angles, angles);
      set_entvar (id, var_punchangle, Float:{0.0, 0.0, 0.0});
      set_entvar (id, var_fixangle, 1);

      return true;
    }
  }

  return false;
}

bool:is_free_space (const Float:origin[3], hull, skip) {
  engfunc (EngFunc_TraceHull, origin, origin, DONT_IGNORE_MONSTERS, hull, skip, 0);

  return bool:(!get_tr2 (0, TR_StartSolid) && !get_tr2 (0, TR_AllSolid) && get_tr2 (0, TR_InOpen));
}

random_teammate (team) {
  new players[32], count;
  new candidates[32], found;

  get_players (players, count, "adh");

  for (new i = 0; i < count; i++) {
    new bot = players[i];

    if (get_user_team (bot) != team || !yb_is_bot_alive (bot) || yb_is_bot_stale (bot)) {
      continue;
    }

    if (g_map_has_vip_zone && get_member (bot, m_bIsVIP) && !g_allow_vip) {
      continue;
    }

    candidates[found++] = bot;
  }

  if (!found) {
    return 0;
  }

  return candidates[found > 1 ? random (found) : 0];
}

announce_takeover (id, bot) {
  new players[32], count;
  new name[32], bot_name[32];

  get_user_name (id, name, charsmax (name));
  get_user_name (bot, bot_name, charsmax (bot_name));
  get_players (players, count, "ch");

  for (new i = 0; i < count; i++) {
    new receiver = players[i];

    if (receiver != id) {
      client_print_color (receiver, id, "%L", receiver, "TAKEOVER_ANNOUNCE", name, bot_name);
    }
  }
}

send_scenario_icon (id, const sprite[], status, alpha) {
  static msg_id;

  if (msg_id <= 0 && (msg_id = get_user_msgid ("Scenario")) <= 0) {
    return;
  }

  message_begin (MSG_ONE_UNRELIABLE, msg_id, .player = id);
  write_byte (status);
  write_string (sprite);
  write_byte (alpha);
  message_end ();
}

log_takeover (const format[], any:...) {
  if (!g_log) {
    return;
  }

  new message[256];
  vformat (message, charsmax (message), format, 2);
  log_to_file (LOG_FILE, "%s", message);
}
