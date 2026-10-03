//
// YaPB AMXX sample plugin.
//
// An admin plugin that inspects and controls YaPB bots through the
// natives exposed by the "yapb" module. Compile and drop into
// addons/amxmodx/plugins, then add to plugins.ini.
//
// console commands (admin):
//   yb_info <target>         dump bot state
//   yb_freeze <target> <0|1> enable/disable movement
//   yb_look <target> [reset] force/reset look target
//   yb_goal <target> <node>  send bot to a graph node
//   yb_path <target>         print the bot current path
//   yb_add [t|ct]            add a bot
//   yb_graph                 graph info + nearest node to you
//

#include <amxmodx>
#include <amxmisc>
#include <yapb>

#define PLUGIN_NAME    "YaPB Sample"
#define PLUGIN_VERSION "1.0"

public plugin_init ()
{
  register_plugin (PLUGIN_NAME, PLUGIN_VERSION, "YaPB");

  register_clcmd ("yb_info",   "cmd_info",   ADMIN_CFG);
  register_clcmd ("yb_freeze", "cmd_freeze", ADMIN_CFG);
  register_clcmd ("yb_look",   "cmd_look",   ADMIN_CFG);
  register_clcmd ("yb_goal",   "cmd_goal",   ADMIN_CFG);
  register_clcmd ("yb_path",   "cmd_path",   ADMIN_CFG);
  register_clcmd ("yb_add",    "cmd_add",    ADMIN_CFG);
  register_clcmd ("yb_graph",  "cmd_graph",  ADMIN_CFG);
}

// resolve the first argument as a player, the bot native index is the player index
stock bot_target (id)
{
  new arg[32];

  read_argv (1, arg, charsmax (arg));
  return cmd_target (id, arg, CMDTARGET_OBEY_IMMUNITY | CMDTARGET_ALLOW_SELF);
}

// make sure the target is a live yapb bot
stock bool:must_be_bot (id, target)
{
  if (!target || !is_user_connected (target))
  {
    client_print (id, print_chat, "[YaPB] no such player");
    return false;
  }

  if (!yb_is_user_bot (target))
  {
    client_print (id, print_chat, "[YaPB] %n is not a bot", target);
    return false;
  }
  return true;
}

stock user_origin_f (id, Float:origin[3])
{
  new pos[3];

  get_user_origin (id, pos);
  origin[0] = float (pos[0]);
  origin[1] = float (pos[1]);
  origin[2] = float (pos[2]);
}

// yb_info <target>
public cmd_info (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new name[32], Float:origin[3];
  new Difficulty:difficulty = yb_get_bot_difficulty (target);
  new Personality:personality = yb_get_bot_personality (target);
  new Task:task = yb_get_bot_task (target);
  new weapon = yb_get_bot_weapon (target);

  get_user_name (target, name, charsmax (name));

  client_print (id, print_console, "[YaPB] %s (index %d)", name, target);
  client_print (id, print_console, "  node %d | goal %d | task %d (data %d)",
    yb_get_bot_node (target), yb_get_bot_goal (target), _:task, yb_get_bot_task_data (target));
  client_print (id, print_console, "  weapon %d | clip %d | ammo %d | ping %d",
    weapon, yb_get_bot_ammo_in_clip (target), yb_get_bot_ammo (target, weapon), yb_get_bot_ping (target));
  client_print (id, print_console, "  difficulty %d | personality %d | movement %d",
    _:difficulty, _:personality, yb_is_bot_movement (target));
  client_print (id, print_console, "  stuck %d (%.2fs) | camping %d | path %d",
    yb_is_bot_stuck (target), yb_get_bot_stuck_time (target), yb_is_bot_camping (target),
    yb_get_bot_path_length (target));

  if (yb_get_bot_origin (target, origin))
  {
    new enemy = yb_get_bot_enemy (target);

    client_print (id, print_console, "  origin %.0f %.0f %.0f | enemy %d (reachable %d)",
      origin[0], origin[1], origin[2], enemy, yb_is_bot_enemy_reachable (target));
  }
  return PLUGIN_HANDLED;
}

// yb_freeze <target> <0|1>
public cmd_freeze (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new arg[8], value;

  read_argv (2, arg, charsmax (arg));

  if (arg[0])
  {
    value = str_to_num (arg);
  }
  else
  {
    value = yb_is_bot_movement (target) ? 0 : 1;
  }

  new bool:movement = value != 0;

  yb_set_bot_movement (target, movement);
  client_print (id, print_chat, "[YaPB] %n movement %s", target, movement ? "on" : "off");

  return PLUGIN_HANDLED;
}

// yb_look <target> [reset]
public cmd_look (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new Float:origin[3], arg[8];

  read_argv (2, arg, charsmax (arg));

  if (equali (arg, "reset"))
  {
    yb_set_bot_look_at (target, origin); // zero vector restores ai aiming
    client_print (id, print_chat, "[YaPB] %n look reset", target);
  }
  else
  {
    user_origin_f (id, origin);
    yb_set_bot_look_at (target, origin);
    client_print (id, print_chat, "[YaPB] %n now looks at you", target);
  }
  return PLUGIN_HANDLED;
}

// yb_goal <target> <node>
public cmd_goal (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new arg[16], node;

  read_argv (2, arg, charsmax (arg));
  node = str_to_num (arg);

  if (!yb_set_bot_goal_node (target, node))
  {
    client_print (id, print_chat, "[YaPB] node %d is invalid", node);
    return PLUGIN_HANDLED;
  }
  client_print (id, print_chat, "[YaPB] %n sent to node %d", target, node);

  return PLUGIN_HANDLED;
}

// yb_path <target>
public cmd_path (id, level, cid)
{
  new target = bot_target (id);

  if (!must_be_bot (id, target))
  {
    return PLUGIN_HANDLED;
  }

  new length = yb_get_bot_path_length (target);

  client_print (id, print_console, "[YaPB] path of %n (%d nodes):", target, length);

  for (new i = 0; i < length && i < 10; i++)
  {
    client_print (id, print_console, "  [%d] node %d", i, yb_get_bot_path_node (target, i));
  }
  return PLUGIN_HANDLED;
}

// yb_add [t|ct]
public cmd_add (id, level, cid)
{
  new arg[8], CsTeams:team = CS_TEAM_UNASSIGNED;

  read_argv (1, arg, charsmax (arg));

  if (equali (arg, "t"))
  {
    team = CS_TEAM_T;
  }
  else if (equali (arg, "ct"))
  {
    team = CS_TEAM_CT;
  }

  if (!yb_add_bot (Difficulty:Normal, Personality:Normal, team))
  {
    client_print (id, print_chat, "[YaPB] failed to add a bot");
    return PLUGIN_HANDLED;
  }
  client_print (id, print_chat, "[YaPB] adding a bot, count is now %d", yb_get_bot_count ());

  return PLUGIN_HANDLED;
}

// yb_graph
public cmd_graph (id, level, cid)
{
  if (!yb_has_graph ())
  {
    client_print (id, print_chat, "[YaPB] this map has no graph");
    return PLUGIN_HANDLED;
  }

  new author[32], modified[32], version[32], api = yb_get_api_version ();

  yb_get_bot_version (version, charsmax (version));
  yb_get_graph_author (author, charsmax (author));
  yb_get_graph_modified (modified, charsmax (modified));

  client_print (id, print_console, "[YaPB] %s | api %d.%d | graph %d nodes by '%s'",
    version, api >>> 16, api & 0xFFFF, yb_get_node_count (), author);

  if (modified[0])
  {
    client_print (id, print_console, "  modified by '%s'", modified);
  }

  // nearest node to the caller, with decoded flags
  new Float:origin[3], node;

  user_origin_f (id, origin);
  node = yb_get_nearest_node (origin);

  if (yb_is_node_valid (node))
  {
    new Float:nodeOrigin[3];
    new NodeFlag:flags;

    yb_get_node_origin (node, nodeOrigin);
    flags = yb_get_node_flags (node);

    client_print (id, print_console, "[YaPB] nearest node %d at %.0f %.0f %.0f",
      node, nodeOrigin[0], nodeOrigin[1], nodeOrigin[2]);
    client_print (id, print_console, "  lift %d | crouch %d | camp %d | sniper %d | links %d",
      !!(_:flags & _:Lift), !!(_:flags & _:Crouch), !!(_:flags & _:Camp), !!(_:flags & _:Sniper),
      yb_get_node_link_count (node));
  }
  else
  {
    client_print (id, print_console, "[YaPB] no node found near you");
  }
  return PLUGIN_HANDLED;
}
