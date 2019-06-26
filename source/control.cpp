//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_display_menu_text ("yb_display_menu_text", "1");
ConVar yb_password ("yb_password", "", VT_PASSWORD);
ConVar yb_password_key ("yb_password_key", "_ybpw");

int BotControl::cmdAddBot (void) {
   enum args { alias = 1, difficulty, personality, team, model, name, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if team is specified, modify args to set team
   if (m_args[alias].find ("_ct", 0) != String::INVALID_INDEX) {
      m_args.set (team, "2");
   }
   else if (m_args[alias].find ("_t", 0) != String::INVALID_INDEX) {
      m_args.set (team, "1");
   }

   // if highskilled bot is requsted set personality to rusher and maxout difficulty
   if (m_args[alias].find ("hs", 0) != String::INVALID_INDEX) {
      m_args.set (difficulty, "4");
      m_args.set (personality, "1");
   }
   bots.addbot (m_args[name], m_args[difficulty], m_args[personality], m_args[team], m_args[model], true);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdKickBot (void) {
   enum args { alias = 1, team, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if team is specified, kick from specified tram
   if (m_args[alias].find ("_ct", 0) != String::INVALID_INDEX || getInt (team) == 2 || getStr (team) == "ct") {
      bots.kickFromTeam (TEAM_COUNTER);
   }
   else if (m_args[alias].find ("_t", 0) != String::INVALID_INDEX || getInt (team) == 1 || getStr (team) == "t") {
      bots.kickFromTeam (TEAM_TERRORIST);
   }
   else {
      bots.kickRandom ();
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdKickBots (void) {
   enum args { alias = 1, instant, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // check if we're need to remove bots instantly
   auto kickInstant = getStr (instant) == "instant";

   // kick the bots
   bots.kickEveryone (kickInstant);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdKillBots (void) {
   enum args { alias = 1, team, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if team is specified, kick from specified tram
   if (m_args[alias].find ("_ct", 0) != String::INVALID_INDEX || getInt (team) == 2 || getStr (team) == "ct") {
      bots.killAllBots (TEAM_COUNTER);
   }
   else if (m_args[alias].find ("_t", 0) != String::INVALID_INDEX || getInt (team) == 1 || getStr (team) == "t") {
      bots.killAllBots (TEAM_TERRORIST);
   }
   else {
      bots.killAllBots ();
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdFill (void) {
   enum args { alias = 1, team, count, difficulty, personality, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (team)) {
      return CMD_STATUS_BADFORMAT;
   }
   bots.serverFill (getInt (team), hasArg (personality) ? getInt (personality) : -1, hasArg (difficulty) ? getInt (difficulty) : -1, hasArg (count) ? getInt (count) : -1);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdVote (void) {
   enum args { alias = 1, mapid, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (mapid)) {
      return CMD_STATUS_BADFORMAT;
   }
   int mapID = getInt (mapid);

   // loop through all players
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = bots.getBot (i);

      if (bot != nullptr)
         bot->m_voteMap = mapID;
   }
   msg ("All dead bots will vote for map #%d", mapID);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWeaponMode (void) {
   enum args { alias = 1, type, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (type)) {
      return CMD_STATUS_BADFORMAT;
   }
   HashMap <String, int> modes;

   modes.put ("kinfe", 1);
   modes.put ("pistol", 2);
   modes.put ("shotgun", 3);
   modes.put ("smg", 4);
   modes.put ("rifle", 5);
   modes.put ("sniper", 6);
   modes.put ("stanard", 7);

   auto mode = getStr (type);

   // check if selected mode exists
   if (!modes.exists (mode)) {
      return CMD_STATUS_BADFORMAT;
   }
   bots.setWeaponMode (modes[mode]);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdVersion (void) {
   msg ("%s v%s (build %u)", PRODUCT_NAME, PRODUCT_VERSION, util.buildNumber ());
   msg ("  compiled: %s %s by %s", __DATE__, __TIME__, PRODUCT_GIT_COMMIT_AUTHOR);
   msg ("  commit: %scommit/%s", PRODUCT_COMMENTS, PRODUCT_GIT_HASH);
   msg ("  url: %s", PRODUCT_URL);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointMenu (void) {
   enum args { alias = 1, max };

   // waypoints is not supported on DS yet
   if (game.isDedicated ()) {
      return CMD_STATUS_LISTENSERV;
   }
   showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdMenu (void) {
   enum args { alias = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // reset the current menu
   showMenu (BOT_MENU_INVALID);

   if (getStr (cmd) == "cmd" && util.isAlive (m_ent)) {
      showMenu (BOT_MENU_COMMANDS);
   }
   else {
      showMenu (BOT_MENU_MAIN);
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdList (void) {
   enum args { alias = 1, max };

   bots.listBots ();
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypoint (void) {
   enum args { root, alias, cmd, cmd2, max };

   // waypoints is not supported on DS yet
   if (game.isDedicated ()) {
      return CMD_STATUS_LISTENSERV;
   }

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // should be moved to class?
   static HashMap <String, BotCmd> commands;
   static Array <String> descriptions;

   // fill only once
   if (descriptions.empty ()) {

      // separate function
      auto addWaypointCommand = [&] (const String & cmd, const String & format, const String & help, Handler handler) -> void {
         commands.put (cmd, { cmd, format, help, cr::forward <Handler> (handler) });
         descriptions.push (cmd);
      };

      // add waypoint commands
      addWaypointCommand ("on", "on [display|auto|noclip|models]", "Enables displaying of waypoints, autowaypoint, noclip cheat", &BotControl::cmdWaypointOn);
      addWaypointCommand ("off", "off [display|auto|noclip|models]", "Disables displaying of waypoints, autowaypoint, noclip cheat", &BotControl::cmdWaypointOff);
      addWaypointCommand ("menu", "menu [noarguments]", "Opens and displays bots waypoint edtior.", &BotControl::cmdWaypointMenu);
      addWaypointCommand ("add", "add [noarguments]", "Opens and displays bots waypoint add  menu.", &BotControl::cmdWaypointAdd);
      addWaypointCommand ("addbasic", "menu [noarguments]", "Adds basic waypoints such as player spawn points, goals and ladders.", &BotControl::cmdWaypointAddBasic);
      addWaypointCommand ("save", "save [noarguments]", "Save waypoint file to disk.", &BotControl::cmdWaypointSave);
      addWaypointCommand ("load", "load [noarguments]", "Load waypoint file from disk.", &BotControl::cmdWaypointLoad);
      addWaypointCommand ("erase", "erase [iamsure]", "Erases the waypoint file from disk.", &BotControl::cmdWaypointErase);
      addWaypointCommand ("delete", "delete [nearest|index]", "Deletes single waypoint from map.", &BotControl::cmdWaypointDelete);
      addWaypointCommand ("check", "check [noarguments]", "Check if waypoints working correctly.", &BotControl::cmdWaypointCheck);
      addWaypointCommand ("cache", "cache [nearest|index]", "Caching waypoint for future use.", &BotControl::cmdWaypointCache);
      addWaypointCommand ("clean", "clean [all|nearest|index]", "Clean useless path connections from all or single waypoint.", &BotControl::cmdWaypointClean);
      addWaypointCommand ("setradius", "setradius [radius] [nearest|index]", "Sets the radius for waypoint.", &BotControl::cmdWaypointSetRadius);
      addWaypointCommand ("flags", "flags [noarguments]", "Open and displays menu for modifying flags for nearest point.", &BotControl::cmdWaypointSetFlags);
      addWaypointCommand ("teleport", "teleport [index]", "Teleports player to specified waypoint index.", &BotControl::cmdWaypointTeleport);

      // add path commands
      addWaypointCommand ("path_create", "path_create [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathCreate);
      addWaypointCommand ("path_create_in", "path_create_in [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathCreate);
      addWaypointCommand ("path_create_out", "path_create_out [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathCreate);
      addWaypointCommand ("path_create_both", "path_create_both [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathCreate);
      addWaypointCommand ("path_delete", "path_create_both [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathDelete);
      addWaypointCommand ("path_set_autopath", "path_set_autoath [max_distance]", "Opens and displays path creation menu.", &BotControl::cmdWaypointPathSetAutoDistance);
   }
   if (commands.exists (getStr (cmd))) {
      auto &item = commands[getStr (cmd)];

      // waypoints have only bad format return status
      int status = (this->*item.handler) ();

      if (status == CMD_STATUS_BADFORMAT) {
         msg ("Incorrect usage of \"%s %s %s\" command. Correct usage is:\n\n\t%s\n\nPlease use correct format.", m_args[root].chars (), m_args[alias].chars (), item.name.chars (), item.format.chars ());
      }
   }
   else {
      if (getStr (cmd) == "help" && hasArg (cmd2) && commands.exists (getStr (cmd2))) {
         auto &item = commands[getStr (cmd2)];

         msg ("Command: \"%s %s %s\"\nFormat: %s\nHelp: %s", m_args[root].chars (), m_args[alias].chars (), item.name.chars (), item.format.chars (), item.help.chars ());
      }
      else {
         for (auto &desc : descriptions) {
            auto &item = commands[desc];
            msg ("   %s - %s", item.name.chars (), item.help.chars ());
         }
         msg ("Currently waypoints are %s", waypoints.hasEditFlag (WS_EDIT_ENABLED) ? "Enabled" : "Disabled");
      }
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointOn (void) {
   enum args { alias = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // enable various features of editor
   if (getStr (option) == "empty" || getStr (option) == "display" || getStr (option) == "models") {
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      enableDrawModels (true);

      msg ("Waypoint editor has been enabled.");
   }
   else if (getStr (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_NOCLIP;

      waypoints.setEditFlag (WS_EDIT_ENABLED | WS_EDIT_NOCLIP);
      enableDrawModels (true);

      msg ("Waypoint editor has been enabled with noclip mode.");
   }
   else if (getStr (option) == "auto") {
      waypoints.setEditFlag (WS_EDIT_ENABLED | WS_EDIT_AUTO);
      enableDrawModels (true);

      msg ("Waypoint editor has been enabled with autowaypoint mode.");
   }

   if (waypoints.hasEditFlag (WS_EDIT_ENABLED)) {
      extern ConVar mp_roundtime, mp_freezetime, mp_timelimit;

      mp_roundtime.set (0);
      mp_freezetime.set (0);
      mp_timelimit.set (0);
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointOff (void) {
   enum args { waypoint = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // enable various features of editor
   if (getStr (option) == "empty" || getStr (option) == "display") {
      waypoints.clearEditFlag (WS_EDIT_ENABLED | WS_EDIT_AUTO | WS_EDIT_NOCLIP);
      enableDrawModels (false);

      msg ("Waypoint editor has been disabled.");
   }
   else if (getStr (option) == "models") {
      enableDrawModels (false);

      msg ("Waypoint editor has disabled spawn points highlighting.");
   }
   else if (getStr (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_WALK;
      waypoints.clearEditFlag (WS_EDIT_NOCLIP);

      msg ("Waypoint editor has disabled noclip mode.");
   }
   else if (getStr (option) == "auto") {
      waypoints.clearEditFlag (WS_EDIT_AUTO);
      msg ("Waypoint editor has disabled autowaypoint mode.");
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointAdd (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // show the menu
   showMenu (BOT_MENU_WAYPOINT_TYPE);
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointAddBasic (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   waypoints.addBasic ();
   msg ("Basic waypoints was added.");

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointSave (void) {
   enum args { waypoint = 1, cmd, nocheck, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if no check is set save anyway
   if (getStr (nocheck) == "nocheck") {
      waypoints.save ();

      msg ("All waypoints has been saved and written to disk (IGNORING QUALITY CONTROL).");
   }
   else {
      if (waypoints.checkNodes ()) {
         waypoints.save ();
         msg ("All waypoints has been saved and written to disk.");
      }
      else {
         msg ("Could not save save waypoints to disk. Waypoint check has failed.");
      }
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointLoad (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // just save waypoints on request
   if (waypoints.load ()) {
      msg ("Waypoints successfully loaded.");
   }
   else {
      msg ("Could not load waypoints. See console...");
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointErase (void) {
   enum args { waypoint = 1, cmd, iamsure, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // prevent accidents when waypoints are deleted unintentionally
   if (getStr (iamsure) == "iamsure") {
      waypoints.eraseFromDisk ();
   }
   else {
      msg ("Please, append \"iamsure\" as parameter to get waypoints erased from the disk.");
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointDelete (void) {
   enum args { waypoint = 1, cmd, nearest, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (getStr (nearest) == "empty" || getStr (nearest) == "nearest") {
      waypoints.erase (INVALID_WAYPOINT_INDEX);
   }
   else {
      int index = getInt (nearest);

      // check for existence
      if (waypoints.exists (index)) {
         waypoints.erase (index);
         msg ("Waypoint #%d has beed deleted.", index);
      }
      else {
         msg ("Could not delete waypoints #%d.", index);
      }
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointCheck (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // check if nodes are ok
   if (waypoints.checkNodes ()) {
      msg ("Waypoints seems to be OK.");
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointCache (void) {
   enum args { waypoint = 1, cmd, nearest, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (getStr (nearest) == "empty" || getStr (nearest) == "nearest") {
      waypoints.cachePoint (INVALID_WAYPOINT_INDEX);

      msg ("Nearest waypoint has been put into the memory.");
   }
   else {
      int index = getInt (nearest);

      // check for existence
      if (waypoints.exists (index)) {
         waypoints.cachePoint (index);
         msg ("Waypoint #%d has been put into the memory.", index);
      }
      else {
         msg ("Could not put waypoint #%d into the memory.", index);
      }
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointClean (void) {
   enum args { waypoint = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // if "all" passed clean up all the paths
   if (getStr (option) == "all") {
      int removed = 0;

      for (int i = 0; i < waypoints.length (); i++) {
         removed += waypoints.removeUselessConnections (i);
      }
      msg ("Done. Processed %d waypoints. %d useless paths was cleared.", waypoints.length (), removed);
   }
   else if (getStr (option) == "empty" || getStr (option) == "nearest") {
      int removed = waypoints.removeUselessConnections (waypoints.getEditorNeareset ());

      msg ("Done. Processed waypoint #%d. %d useless paths was cleared.", waypoints.getEditorNeareset (), removed);
   }
   else {
      int index = getInt (option);

      // check for existence
      if (waypoints.exists (index)) {
         int removed = waypoints.removeUselessConnections (index);

         msg ("Done. Processed waypoint #%d. %d useless paths was cleared.", index, removed);
      }
      else {
         msg ("Could not process waypoint #%d clearance.", index);
      }
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointSetRadius (void) {
   enum args { waypoint = 1, cmd, radius, index, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // radius is a must
   if (!hasArg (radius)) {
      return CMD_STATUS_BADFORMAT;
   }
   int radiusIndex = INVALID_WAYPOINT_INDEX;

   if (getStr (index) == "empty" || getStr (index) == "nearest") {
      radiusIndex = waypoints.getEditorNeareset ();
   }
   else {
      radiusIndex = getInt (index);
   }
   float value = getStr (radius).toFloat ();

   waypoints.setRadius (radiusIndex, value);
   msg ("Waypoint #%d has been set to radius %.2f.", radiusIndex, value);

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointSetFlags (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   //show the flag menu
   showMenu (BOT_MENU_WAYPOINT_FLAG);
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointTeleport (void) {
   enum args { waypoint = 1, cmd, teleport_index, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (teleport_index)) {
      return CMD_STATUS_BADFORMAT;
   }
   int index = getInt (teleport_index);

   // check for existence
   if (waypoints.exists (index)) {
      engfuncs.pfnSetOrigin (game.getLocalEntity (), waypoints[index].origin);

      msg ("You have been teleported to waypoint #%d.", index);

      // turn waypoints on
      waypoints.setEditFlag (WS_EDIT_ENABLED | WS_EDIT_NOCLIP);
   }
   else {
      msg ("Could not teleport to waypoint #%d.", index);
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointPathCreate (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // choose the direction for path creation
   if (m_args[cmd].find ("_both", 0) != String::INVALID_INDEX) {
      waypoints.pathCreate (CONNECTION_BOTHWAYS);
   }
   else if (m_args[cmd].find ("_in", 0) != String::INVALID_INDEX) {
      waypoints.pathCreate (CONNECTION_INCOMING);
   }
   else if (m_args[cmd].find ("_out", 0) != String::INVALID_INDEX) {
      waypoints.pathCreate (CONNECTION_OUTGOING);
   }
   else {
      showMenu (BOT_MENU_WAYPOINT_PATH);
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointPathDelete (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);

   // delete the patch
   waypoints.erasePath ();

   return CMD_STATUS_HANDLED;
}

int BotControl::cmdWaypointPathSetAutoDistance (void) {
   enum args { waypoint = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn waypoints on
   waypoints.setEditFlag (WS_EDIT_ENABLED);
   showMenu (BOT_MENU_WAYPOINT_AUTOPATH);

   return CMD_STATUS_HANDLED;
}

int BotControl::menuMain (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      m_isMenuFillCommand = false;
      showMenu (BOT_MENU_CONTROL);
      break;

   case 2:
      showMenu (BOT_MENU_FEATURES);
      break;

   case 3:
      m_isMenuFillCommand = true;
      showMenu (BOT_MENU_TEAM_SELECT);
      break;

   case 4:
      bots.killAllBots ();
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;

   default:
      showMenu (BOT_MENU_MAIN);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuFeatures (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      showMenu (BOT_MENU_WEAPON_MODE);
      break;

   case 2:
      showMenu (game.isDedicated () ? BOT_MENU_FEATURES : BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;

   case 3:
      showMenu (BOT_MENU_PERSONALITY);
      break;

   case 4:
      extern ConVar yb_debug;
      yb_debug.set (yb_debug.integer () ^ 1);

      showMenu (BOT_MENU_FEATURES);
      break;

   case 5:
      if (util.isAlive (m_ent)) {
         showMenu (BOT_MENU_COMMANDS);
      }
      else {
         showMenu (BOT_MENU_INVALID); // reset menu display
         msg ("You're dead, and have no access to this menu");
      }
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuControl (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      bots.createRandom (true);
      showMenu (BOT_MENU_CONTROL);
      break;

   case 2:
      showMenu (BOT_MENU_DIFFICULTY);
      break;

   case 3:
      bots.kickRandom ();
      showMenu (BOT_MENU_CONTROL);
      break;

   case 4:
      bots.kickEveryone ();
      break;

   case 5:
      kickBotByMenu (1);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWeaponMode (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
      bots.setWeaponMode (item);
      showMenu (BOT_MENU_WEAPON_MODE);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuPersonality (int item) {
   if (!m_isMenuFillCommand) {
      return CMD_STATUS_HANDLED;
   }
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
      bots.serverFill (m_menuServerFillTeam, item - 2, m_interMenuData[0]);
      showMenu (BOT_MENU_INVALID);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuDifficulty (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      m_interMenuData[0] = 0;
      break;

   case 2:
      m_interMenuData[0] = 1;
      break;

   case 3:
      m_interMenuData[0] = 2;
      break;

   case 4:
      m_interMenuData[0] = 3;
      break;

   case 5:
      m_interMenuData[0] = 4;
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   showMenu (BOT_MENU_PERSONALITY);

   return CMD_STATUS_HANDLED;
}

int BotControl::menuTeamSelect (int item) {
   if (m_isMenuFillCommand) {
      switch (item) {
      case 1:
      case 2:
      case 3:
      case 4:
         bots.serverFill (m_menuServerFillTeam, item - 2, m_interMenuData[0]);
         showMenu (BOT_MENU_INVALID);
         break;

      case 10:
         showMenu (BOT_MENU_INVALID);
         break;
      }
      return CMD_STATUS_HANDLED;
   }
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 5:
      m_interMenuData[1] = item;

      if (item == 5) {
         m_interMenuData[2] = item;
         bots.addbot ("", m_interMenuData[0], m_interMenuData[3], m_interMenuData[1], m_interMenuData[2], true);
      }
      else {
         showMenu (item == 1 ? BOT_MENU_TERRORIST_SELECT : BOT_MENU_CT_SELECT);
      }
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuClassSelect (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
      m_interMenuData[2] = item;
      bots.addbot ("", m_interMenuData[0], m_interMenuData[3], m_interMenuData[1], m_interMenuData[2], true);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuCommands (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display
   Bot *bot = nullptr;

   switch (item) {
   case 1:
   case 2:
      if (util.findNearestPlayer (reinterpret_cast <void **> (&bot), m_ent, 600.0f, true, true, true) && bot->m_hasC4 && !bot->hasHostage ()) {
         if (item == 1) {
            bot->startDoubleJump (m_ent);
         }
         else {
            bot->resetDoubleJump ();
         }
      }
      showMenu (BOT_MENU_COMMANDS);
      break;

   case 3:
   case 4:
      if (util.findNearestPlayer (reinterpret_cast <void **> (&bot), m_ent, 600.0f, true, true, true, true, item == 4 ? false : true)) {
         bot->dropWeaponForUser (m_ent, item == 4 ? false : true);
      }
      showMenu (BOT_MENU_COMMANDS);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointPage1 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      if (waypoints.hasEditFlag (WS_EDIT_ENABLED)) {
         waypoints.clearEditFlag (WS_EDIT_ENABLED);
         enableDrawModels (false);

         msg ("Waypoint editor has been disabled.");
      }
      else {
         waypoints.setEditFlag (WS_EDIT_ENABLED);
         enableDrawModels (true);

         msg ("Waypoint editor has been enabled.");
      }
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;

   case 2:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      waypoints.cachePoint (INVALID_WAYPOINT_INDEX);

      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;

   case 3:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      showMenu (BOT_MENU_WAYPOINT_PATH);
      break;

   case 4:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      waypoints.erasePath ();
      
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;

   case 5:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      showMenu (BOT_MENU_WAYPOINT_TYPE);
      break;

   case 6:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      waypoints.erase (INVALID_WAYPOINT_INDEX);

      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;

   case 7:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      showMenu (BOT_MENU_WAYPOINT_AUTOPATH);
      break;

   case 8:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      showMenu (BOT_MENU_WAYPOINT_RADIUS);
      break;

   case 9:
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointPage2 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1: {
      int terrPoints = 0;
      int ctPoints = 0;
      int goalPoints = 0;
      int rescuePoints = 0;
      int campPoints = 0;
      int sniperPoints = 0;
      int noHostagePoints = 0;

      for (int i = 0; i < waypoints.length (); i++) {
         Path &path = waypoints[i];

         if (path.flags & FLAG_TF_ONLY) {
            terrPoints++;
         }

         if (path.flags & FLAG_CF_ONLY) {
            ctPoints++;
         }

         if (path.flags & FLAG_GOAL) {
            goalPoints++;
         }

         if (path.flags & FLAG_RESCUE) {
            rescuePoints++;
         }

         if (path.flags & FLAG_CAMP) {
            campPoints++;
         }

         if (path.flags & FLAG_SNIPER) {
            sniperPoints++;
         }

         if (path.flags & FLAG_NOHOSTAGE) {
            noHostagePoints++;
         }
      }
      msg ("Waypoints: %d - T Points: %d\n"
         "CT Points: %d - Goal Points: %d\n"
         "Rescue Points: %d - Camp Points: %d\n"
         "Block Hostage Points: %d - Sniper Points: %d\n",
         waypoints.length (), terrPoints, ctPoints, goalPoints, rescuePoints, campPoints, noHostagePoints, sniperPoints);

      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
   } break;

   case 2:
      waypoints.setEditFlag (WS_EDIT_ENABLED);

      if (waypoints.hasEditFlag (WS_EDIT_AUTO)) {
         waypoints.clearEditFlag (WS_EDIT_AUTO);
      }
      else {
         waypoints.setEditFlag (WS_EDIT_AUTO);
      }
      msg ("Auto-Waypoint %s", waypoints.hasEditFlag (WS_EDIT_AUTO) ? "Enabled" : "Disabled");

      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 3:
      waypoints.setEditFlag (WS_EDIT_ENABLED);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;

   case 4:
      if (waypoints.checkNodes ()) {
         waypoints.save ();
      }
      else {
         msg ("Waypoint not saved\nThere are errors, see console");
      }
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 5:
      waypoints.save ();
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 6:
      waypoints.load ();
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 7:
      if (waypoints.checkNodes ()) {
         msg ("Nodes works fine");
      }
      else {
         msg ("There are errors, see console");
      }
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 8:
      waypoints.setEditFlag (WS_EDIT_ENABLED | WS_EDIT_NOCLIP);
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE2);
      break;

   case 9:
      showMenu (BOT_MENU_WAYPOINT_MAIN_PAGE1);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointRadius (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display
   waypoints.setEditFlag (WS_EDIT_ENABLED); // turn waypoints on in case

   constexpr float radius[] = { 0.0f, 8.0f, 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 128.0f };

   if (item >= 1 && item <= 9) {
      waypoints.setRadius (INVALID_WAYPOINT_INDEX, radius[item - 1]);
      showMenu (BOT_MENU_WAYPOINT_RADIUS);
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointType (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
      waypoints.push (item - 1);
      showMenu (BOT_MENU_WAYPOINT_TYPE);
      break;

   case 8:
      waypoints.push (100);
      showMenu (BOT_MENU_WAYPOINT_TYPE);
      break;

   case 9:
      waypoints.startLearnJump ();
      showMenu (BOT_MENU_WAYPOINT_TYPE);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointFlag (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      waypoints.toggleFlags (FLAG_NOHOSTAGE);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;

   case 2:
      waypoints.toggleFlags (FLAG_TF_ONLY);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;

   case 3:
      waypoints.toggleFlags (FLAG_CF_ONLY);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;

   case 4:
      waypoints.toggleFlags (FLAG_LIFT);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;

   case 5:
      waypoints.toggleFlags (FLAG_SNIPER);
      showMenu (BOT_MENU_WAYPOINT_FLAG);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuAutoPathDistance (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   constexpr float distances[] = { 0.0f, 100.0f, 130.0f, 160.0f, 190.0f, 220.0f, 250.0f };
   float result = 0.0f;

   if (item >= 1 && item <= 7) {
      result = distances[item - 1];
      waypoints.setAutoPathDistance (result);
   }

   if (cr::fzero (result)) {
      msg ("Autopathing is now disabled.");
   }
   else {
      msg ("Autopath distance is set to %.2f.", result);
   }
   showMenu (BOT_MENU_WAYPOINT_AUTOPATH);

   return CMD_STATUS_HANDLED;
}

int BotControl::menuKickPage1 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
   case 8:
      bots.kickBot (item - 1);
      kickBotByMenu (1);
      break;

   case 9:
      kickBotByMenu (2);
      break;

   case 10:
      showMenu (BOT_MENU_CONTROL);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuKickPage2 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
   case 8:
      bots.kickBot (item + 8 - 1);
      kickBotByMenu (2);
      break;

   case 9:
      kickBotByMenu (3);
      break;

   case 10:
      kickBotByMenu (1);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuKickPage3 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
   case 8:
      bots.kickBot (item + 16 - 1);
      kickBotByMenu (3);
      break;

   case 9:
      kickBotByMenu (4);
      break;

   case 10:
      kickBotByMenu (2);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuKickPage4 (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
   case 8:
      bots.kickBot (item + 24 - 1);
      kickBotByMenu (4);
      break;

   case 10:
      kickBotByMenu (3);
      break;
   }
   return CMD_STATUS_HANDLED;
}

int BotControl::menuWaypointPath (int item) {
   showMenu (BOT_MENU_INVALID); // reset menu display

   switch (item) {
   case 1:
      waypoints.pathCreate (CONNECTION_OUTGOING);
      showMenu (BOT_MENU_WAYPOINT_PATH);
      break;

   case 2:
      waypoints.pathCreate (CONNECTION_INCOMING);
      showMenu (BOT_MENU_WAYPOINT_PATH);
      break;

   case 3:
      waypoints.pathCreate (CONNECTION_BOTHWAYS);
      showMenu (BOT_MENU_WAYPOINT_PATH);
      break;

   case 10:
      showMenu (BOT_MENU_INVALID);
      break;
   }
   return CMD_STATUS_HANDLED;
}

bool BotControl::executeCommands (void) {
   if (m_args.empty ()) {
      return false;
   }

   // handle only "yb" and "yapb" commands
   if (m_args[0] != "yb" && m_args[0] != "yapb") {
      return false;
   }

   auto aliasMatch = [] (String & test, const String & cmd, String & aliasName) -> bool {
      for (auto &alias : test.split ("|")) {
         if (stricmp (alias.chars (), cmd.chars ()) == 0) {
            aliasName = alias;
            return true;
         }
      }
      return false;
   };
   String cmd;

   // give some help
   if (m_args.length () > 1 && stricmp ("help", m_args[1].chars ()) == 0) {
      for (auto &item : m_cmds) {
         if (aliasMatch (item.name, m_args[2], cmd)) {
            msg ("Command: \"%s %s\"\nFormat: %s\nHelp: %s", m_args[0].chars (), cmd.chars (), item.format.chars (), item.help.chars ());

            String aliases;

            for (auto &alias : item.name.split ("|")) {
               aliases.formatAppend ("%s, ", alias.chars ());
            }
            aliases.trimRight (", ");
            msg ("Aliases: %s", aliases.chars ());

            return true;
         }
      }

      if (m_args[2].empty ()) {
         return true;
      }
      else {
         msg ("No help found for \"%s\"", m_args[2].chars ());
      }
      return true;
   }
   cmd.clear ();

   // if no args passed just print all the commands
   if (m_args.length () == 1) {
      msg ("usage %s <command> [arguments]", m_args[0].chars ());
      msg ("valid commands are: ");

      for (auto &item : m_cmds) {
         msg ("   %s - %s", item.name.split ("|")[0].chars (), item.help.chars ());
      }
      return true;
   }

   // first search for a actual cmd
   for (auto &item : m_cmds) {
      auto root = m_args[0].chars ();

      if (aliasMatch (item.name, m_args[1], cmd)) {
         auto alias = cmd.chars ();

         switch ((this->*item.handler) ()) {
         case CMD_STATUS_HANDLED:
         default:
            break;

         case CMD_STATUS_LISTENSERV:
            msg ("Command \"%s %s\" is only available from the listenserver console.", root, alias);
            break;

         case CMD_STATUS_DENIED:
            msg ("Access to command \"%s %s\" is denied by ACL.", root, alias);
            break;

         case CMD_STATUS_BADFORMAT:
            msg ("Incorrect usage of \"%s %s\" command. Correct usage is:\n\n\t%s\n\nPlease type \"%s help %s\" to get more information.", root, alias, item.format.chars (), root, alias);
            break;
         }

         m_isFromConsole = false;
         return true;
      }
   }
   msg ("Unrecognized command: %s", m_args[1].chars ());

   //  not handled
   return false;
}

bool BotControl::executeMenus (void) {
   if (!util.isPlayer (m_ent) || game.isBotCmd ()) {
      return false;
   }
   auto &issuer = util.getClient (game.indexOfEntity (m_ent) - 1);

   // check if we are host entity or client with admin flag
   if (m_ent != game.getLocalEntity () && !(issuer.flags & CF_ADMIN)) {
      return false;
   }

   // check if it's menu select, and some key pressed
   if (getStr (0) != "menuselect" || getStr (1).empty () || issuer.menu == BOT_MENU_INVALID) {
      return false;
   }

   // let's get handle
   for (auto &menu : m_menus) {
      if (menu.ident == issuer.menu) {
         return (this->*menu.handler) (getStr (1).toInt32 ());
      }
   }
   return false;
}

void BotControl::showMenu (int id) {
   static bool s_menusParsed = false;

   // make menus looks like we need only once
   if (!s_menusParsed) {
      for (auto &parsed : m_menus) {
         const String &translated = game.translate (parsed.text.chars ());

         // translate all the things
         parsed.text = translated;

         // make menu looks best
         if (!(game.is (GAME_LEGACY))) {
            for (int j = 0; j < 10; j++) {
               parsed.text.replace (util.format ("%d.", j), util.format ("\\r%d.\\w", j));
            }
         }
      }
      s_menusParsed = true;
   }

   if (!util.isPlayer (m_ent)) {
      return;
   }
   Client &client = util.getClient (game.indexOfEntity (m_ent) - 1);

   if (id == BOT_MENU_INVALID) {
      MessageWriter (MSG_ONE_UNRELIABLE, game.getMessageId (NETMSG_SHOWMENU), Vector::null (), m_ent)
         .writeShort (0)
         .writeChar (0)
         .writeByte (0)
         .writeString ("");

      client.menu = id;
      return;
   }

   for (auto &display : m_menus) {
      if (display.ident == id) {
         const char *text = (game.is (GAME_XASH_ENGINE | GAME_MOBILITY) && !yb_display_menu_text.boolean ()) ? " " : display.text.chars ();
         MessageWriter msg;

         while (strlen (text) >= 64) {
            msg.start (MSG_ONE_UNRELIABLE, game.getMessageId (NETMSG_SHOWMENU), Vector::null (), m_ent)
               .writeShort (display.slots)
               .writeChar (-1)
               .writeByte (1);

            for (int i = 0; i < 64; i++) {
               msg.writeChar (text[i]);
            }
            msg.end ();
            text += 64;
         }

         MessageWriter (MSG_ONE_UNRELIABLE, game.getMessageId (NETMSG_SHOWMENU), Vector::null (), m_ent)
            .writeShort (display.slots)
            .writeChar (-1)
            .writeByte (0)
            .writeString (text);

         client.menu = id;
         engfuncs.pfnClientCommand (m_ent, "speak \"player/geiger1\"\n"); // Stops others from hearing menu sounds..
      }
   }
}

void BotControl::kickBotByMenu (int page) {
   if (page > 4 || page < 1) {
      return;
   }

   String menus;
   menus.format ("\\yBots Remove Menu (%d/4):\\w\n\n", page);

   int menuKeys = (page == 4) ? cr::bit (9) : (cr::bit (8) | cr::bit (9));
   int menuKey = (page - 1) * 8;

   for (int i = menuKey; i < page * 8; i++) {
      auto bot = bots.getBot (i);

      if (bot != nullptr && (bot->pev->flags & FL_FAKECLIENT)) {
         menuKeys |= cr::bit (cr::abs (i - menuKey));
         menus.formatAppend ("%1.1d. %s%s\n", i - menuKey + 1, STRING (bot->pev->netname), bot->m_team == TEAM_COUNTER ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else {
         menus.formatAppend ("\\d %1.1d. Not a Bot\\w\n", i - menuKey + 1);
      }
   }
   menus.formatAppend ("\n%s 0. Back", (page == 4) ? "" : " 9. More...\n");

   // force to clear current menu
   showMenu (BOT_MENU_INVALID);

   auto id = BOT_MENU_KICK_PAGE_1 - 1 + page;

   for (auto &menu : m_menus) {
      if (menu.ident == id) {
         menu.slots = menuKeys & static_cast <unsigned int> (-1);
         menu.text = menus;

         break;
      }
   }
   showMenu (id);
}

void BotControl::msg (const char *fmt, ...) {
   va_list ap;
   char buffer[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (buffer, cr::bufsize (buffer), fmt, ap);
   va_end (ap);

   if (m_isFromConsole) {
      game.clientPrint (m_ent, buffer);
   }
   else {
      game.centerPrint (m_ent, buffer);
   }
}

void BotControl::assignAdminRights (edict_t *ent, char *infobuffer) {
   if (!game.isDedicated () || util.isFakeClient (ent)) {
      return;
   }
   const String &key = yb_password_key.str ();
   const String &password = yb_password.str ();

   if (!key.empty () && !password.empty ()) {
      auto &client = util.getClient (game.indexOfEntity (ent) - 1);

      if (password == engfuncs.pfnInfoKeyValue (infobuffer, key.chars ())) {
         client.flags |= CF_ADMIN;
      }
      else {
         client.flags &= ~CF_ADMIN;
      }
   }
}

void BotControl::maintainAdminRights (void) {
   if (!game.isDedicated ()) {
      return;
   }

   for (int i = 0; i < game.maxClients (); i++) {
      edict_t *player = game.entityOfIndex (i + 1);

      // code below is executed only on dedicated server
      if (util.isPlayer (player) && !util.isFakeClient (player)) {
         Client &client = util.getClient (i);

         if (client.flags & CF_ADMIN) {
            if (util.isEmptyStr (yb_password_key.str ()) && util.isEmptyStr (yb_password.str ())) {
               client.flags &= ~CF_ADMIN;
            }
            else if (!!strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ())))) {
               client.flags &= ~CF_ADMIN;
               game.print ("Player %s had lost remote access to %s.", STRING (player->v.netname), PRODUCT_SHORT_NAME);
            }
         }
         else if (!(client.flags & CF_ADMIN) && !util.isEmptyStr (yb_password_key.str ()) && !util.isEmptyStr (yb_password.str ())) {
            if (strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ()))) == 0) {
               client.flags |= CF_ADMIN;
               game.print ("Player %s had gained full remote access to %s.", STRING (player->v.netname), PRODUCT_SHORT_NAME);
            }
         }
      }
   }
}

BotControl::BotControl (void) {
   m_ent = nullptr;
   m_isFromConsole = false;
   m_isMenuFillCommand = false;
   m_menuServerFillTeam = 5;

   auto addCommand = [&] (const String & cmd, const String & format, const String & help, Handler handler) -> void {
      m_cmds.push ({ cmd, format, help, cr::forward <Handler> (handler) });
   };

   addCommand ("add|addbot|add_ct|addbot_ct|add_t|addbot_t|addhs|addhs_t|addhs_ct", "add [difficulty[personality[team[model[name]]]]]", "Adding specific bot into the game.", &BotControl::cmdAddBot);
   addCommand ("kick|kickone|kick_ct|kick_t|kickbot_ct|kickbot_t", "kick [team]", "Kicks off the random bot from the game.", &BotControl::cmdKickBot);
   addCommand ("removebots|kickbots|kickall", "removebots [instant]", "Kicks all the bots from the game.", &BotControl::cmdKickBots);
   addCommand ("kill|killbots|killall|kill_ct|kill_t", "kill [team]", "Kills the specified team / all the bots.", &BotControl::cmdKillBots);
   addCommand ("fill|fillserver", "fill [team[count[difficulty[pesonality]]]]", "Fill the server (add bots) with specified parameters.", &BotControl::cmdFill);
   addCommand ("vote|votemap", "vote [map_id]", "Forces all the bot to vote to specified map.", &BotControl::cmdVote);
   addCommand ("weapons|weaponmode", "weapons [knife|pistol|shotgun|smg|rifle|sniper|standard]", "Sets the bots weapon mode to use", &BotControl::cmdWeaponMode);
   addCommand ("menu|botmenu", "menu [cmd]", "Opens the main bot menu, or command menu if specified.", &BotControl::cmdMenu);
   addCommand ("version|ver|about", "version [no arguments]", "Displays version information about bot build.", &BotControl::cmdVersion);
   addCommand ("wpmenu|wptmenu", "wpmenu [noarguments]", "Opens and displays bots waypoint edtior.", &BotControl::cmdWaypointMenu);
   addCommand ("list|listbots", "list [noarguments]", "Lists the bots currently playing on server.", &BotControl::cmdList);
   addCommand ("waypoint|wp|wpt|waypoint", "waypoint [help]", "Handles waypoint operations.", &BotControl::cmdWaypoint);

   // declare the menus
   createMenus ();
}

void BotControl::handleEngineCommands (void) {
   ctrl.setArgs (cr::move (ctrl.collectArgs ()));
   ctrl.setIssuer (game.getLocalEntity ());

   ctrl.setFromConsole (true);
   ctrl.executeCommands ();
}

bool BotControl::handleClientCommands (edict_t *ent) {
   setArgs (cr::move (collectArgs ()));
   setIssuer (ent);

   setFromConsole (true);
   return executeCommands ();
}

bool BotControl::handleMenuCommands (edict_t *ent) {
   setArgs (cr::move (collectArgs ()));
   setIssuer (ent);

   setFromConsole (false);
   return ctrl.executeMenus ();
}

void BotControl::enableDrawModels (const bool enable) {
   StringArray entities;

   entities.push ("info_player_start");
   entities.push ("info_player_deathmatch");
   entities.push ("info_vip_start");

   for (auto &entity : entities) {
      edict_t *ent = nullptr;

      while (!game.isNullEntity (ent = engfuncs.pfnFindEntityByString (ent, "classname", entity.chars ()))) {
         if (enable) {
            ent->v.effects &= ~EF_NODRAW;
         }
         else {
            ent->v.effects |= EF_NODRAW;
         }
      }
   }
}

void BotControl::createMenus (void) {
   auto keys = [] (int numKeys) -> int {
      int result = 0;

      for (int i = 0; i < numKeys; i++) {
         result |= cr::bit (i);
      }
      result |= cr::bit (9);

      return result;
   };

   // bots main menu
   m_menus.push ({
      BOT_MENU_MAIN, keys (4),
      "\\yMain Menu\\w\n\n"
      "1. Control Bots\n"
      "2. Features\n\n"
      "3. Fill Server\n"
      "4. End Round\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuMain) });


   // bots features menu
   m_menus.push ({
      BOT_MENU_FEATURES, keys (5),
      "\\yBots Features\\w\n\n"
      "1. Weapon Mode Menu\n"
      "2. Waypoint Menu\n"
      "3. Select Personality\n\n"
      "4. Toggle Debug Mode\n"
      "5. Command Menu\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuFeatures) });

   // bot control menu
   m_menus.push ({
      BOT_MENU_CONTROL, keys (5),
      "\\yBots Control Menu\\w\n\n"
      "1. Add a Bot, Quick\n"
      "2. Add a Bot, Specified\n\n"
      "3. Remove Random Bot\n"
      "4. Remove All Bots\n\n"
      "5. Remove Bot Menu\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuControl) });

   // weapon mode select menu
   m_menus.push ({
      BOT_MENU_WEAPON_MODE, keys (7),
      "\\yBots Weapon Mode\\w\n\n"
      "1. Knives only\n"
      "2. Pistols only\n"
      "3. Shotguns only\n"
      "4. Machine Guns only\n"
      "5. Rifles only\n"
      "6. Sniper Weapons only\n"
      "7. All Weapons\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWeaponMode) });

   // personality select menu
   m_menus.push ({
      BOT_MENU_PERSONALITY, keys (4),
      "\\yBots Personality\\w\n\n"
      "1. Random\n"
      "2. Normal\n"
      "3. Aggressive\n"
      "4. Careful\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuPersonality) });

   // difficulty select menu
   m_menus.push ({
      BOT_MENU_DIFFICULTY, keys (5),
      "\\yBots Difficulty Level\\w\n\n"
      "1. Newbie\n"
      "2. Average\n"
      "3. Normal\n"
      "4. Professional\n"
      "5. Godlike\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuDifficulty) });

   // team select menu
   m_menus.push ({
      BOT_MENU_TEAM_SELECT, keys (5),
      "\\ySelect a team\\w\n\n"
      "1. Terrorist Force\n"
      "2. Counter-Terrorist Force\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuTeamSelect) });

   // terrorist model select menu
   m_menus.push ({
      BOT_MENU_TERRORIST_SELECT, keys (5),
      "\\ySelect an appearance\\w\n\n"
      "1. Phoenix Connexion\n"
      "2. L337 Krew\n"
      "3. Arctic Avengers\n"
      "4. Guerilla Warfare\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuClassSelect) });

   // counter-terrorist model select menu
   m_menus.push ({
      BOT_MENU_CT_SELECT, keys (5),
      "\\ySelect an appearance\\w\n\n"
      "1. Seal Team 6 (DEVGRU)\n"
      "2. German GSG-9\n"
      "3. UK SAS\n"
      "4. French GIGN\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuClassSelect) });

   // command menu
   m_menus.push ({
      BOT_MENU_COMMANDS, keys (4),
      "\\yBot Command Menu\\w\n\n"
      "1. Make Double Jump\n"
      "2. Finish Double Jump\n\n"
      "3. Drop the C4 Bomb\n"
      "4. Drop the Weapon\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuCommands) });

   // main waypoint menu
   m_menus.push ({
      BOT_MENU_WAYPOINT_MAIN_PAGE1, keys (9),
      "\\yWaypoint Operations (Page 1)\\w\n\n"
      "1. Show/Hide waypoints\n"
      "2. Cache waypoint\n"
      "3. Create path\n"
      "4. Delete path\n"
      "5. Add waypoint\n"
      "6. Delete waypoint\n"
      "7. Set Autopath Distance\n"
      "8. Set Radius\n\n"
      "9. Next...\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointPage1) });

   // main waypoint menu (page 2)
   m_menus.push ({
      BOT_MENU_WAYPOINT_MAIN_PAGE2, keys (9),
      "\\yWaypoint Operations (Page 2)\\w\n\n"
      "1. Waypoint stats\n"
      "2. Autowaypoint on/off\n"
      "3. Set flags\n"
      "4. Save waypoints\n"
      "5. Save without checking\n"
      "6. Load waypoints\n"
      "7. Check waypoints\n"
      "8. Noclip cheat on/off\n\n"
      "9. Previous...\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointPage2) });

   // select waypoint radius menu
   m_menus.push ({
      BOT_MENU_WAYPOINT_RADIUS, keys (9),
      "\\yWaypoint Radius\\w\n\n"
      "1. SetRadius 0\n"
      "2. SetRadius 8\n"
      "3. SetRadius 16\n"
      "4. SetRadius 32\n"
      "5. SetRadius 48\n"
      "6. SetRadius 64\n"
      "7. SetRadius 80\n"
      "8. SetRadius 96\n"
      "9. SetRadius 128\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointRadius) });

   // waypoint add menu
   m_menus.push ({
      BOT_MENU_WAYPOINT_TYPE, keys (9),
      "\\yWaypoint Type\\w\n\n"
      "1. Normal\n"
      "\\r2. Terrorist Important\n"
      "3. Counter-Terrorist Important\n"
      "\\w4. Block with hostage / Ladder\n"
      "\\y5. Rescue Zone\n"
      "\\w6. Camping\n"
      "7. Camp End\n"
      "\\r8. Map Goal\n"
      "\\w9. Jump\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointType) });

   // set waypoint flag menu
   m_menus.push ({
      BOT_MENU_WAYPOINT_FLAG, keys (5),
      "\\yToggle Waypoint Flags\\w\n\n"
      "1. Block with Hostage\n"
      "2. Terrorists Specific\n"
      "3. CTs Specific\n"
      "4. Use Elevator\n"
      "5. Sniper Point (\\yFor Camp Points Only!\\w)\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointFlag) });

   // auto-path max distance
   m_menus.push ({
      BOT_MENU_WAYPOINT_AUTOPATH, keys (7),
      "\\yAutoPath Distance\\w\n\n"
      "1. Distance 0\n"
      "2. Distance 100\n"
      "3. Distance 130\n"
      "4. Distance 160\n"
      "5. Distance 190\n"
      "6. Distance 220\n"
      "7. Distance 250 (Default)\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuAutoPathDistance) });

   // path connections
   m_menus.push ({
      BOT_MENU_WAYPOINT_PATH, keys (3),
      "\\yCreate Path (Choose Direction)\\w\n\n"
      "1. Outgoing Path\n"
      "2. Incoming Path\n"
      "3. Bidirectional (Both Ways)\n\n"
      "0. Exit",
      cr::forward <MenuHandler> (&BotControl::menuWaypointPath) });

   const String &empty = "";

   // kick menus
   m_menus.push ({ BOT_MENU_KICK_PAGE_1, 0x0, empty, cr::forward <MenuHandler> (&BotControl::menuKickPage1)  });
   m_menus.push ({ BOT_MENU_KICK_PAGE_2, 0x0, empty, cr::forward <MenuHandler> (&BotControl::menuKickPage2)  });
   m_menus.push ({ BOT_MENU_KICK_PAGE_3, 0x0, empty, cr::forward <MenuHandler> (&BotControl::menuKickPage3)  });
   m_menus.push ({ BOT_MENU_KICK_PAGE_4, 0x0, empty, cr::forward <MenuHandler> (&BotControl::menuKickPage4) });
}
