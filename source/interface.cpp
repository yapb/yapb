//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

// console vars
ConVar yb_ignore_cvars_on_changelevel ("yb_ignore_cvars_on_changelevel", "yb_quota,yb_autovacate");
ConVar yb_password ("yb_password", "", VT_PASSWORD);
ConVar yb_password_key ("yb_password_key", "_ybpw");
ConVar yb_language ("yb_language", "en");
ConVar yb_version ("yb_version", PRODUCT_VERSION, VT_READONLY);

ConVar mp_startmoney ("mp_startmoney", nullptr, VT_NOREGISTER, true, "800");

int handleBotCommands (edict_t *ent, const char *arg0, const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *arg5, const char *self) {
   // adding one bot with random parameters to random team
   if (stricmp (arg0, "addbot") == 0 || stricmp (arg0, "add") == 0) {
      bots.addbot (arg4, arg1, arg2, arg3, arg5, true);
   }

   // adding one bot with high difficulty parameters to random team
   else if (stricmp (arg0, "addbot_hs") == 0 || stricmp (arg0, "addhs") == 0) {
      bots.addbot (arg4, "4", "1", arg3, arg5, true);
   }

   // adding one bot with random parameters to terrorist team
   else if (stricmp (arg0, "addbot_t") == 0 || stricmp (arg0, "add_t") == 0) {
      bots.addbot (arg4, arg1, arg2, "1", arg5, true);
   }

   // adding one bot with random parameters to counter-terrorist team
   else if (stricmp (arg0, "addbot_ct") == 0 || stricmp (arg0, "add_ct") == 0) {
      bots.addbot (arg4, arg1, arg2, "2", arg5, true);
   }

   // kicking off one bot from the terrorist team
   else if (stricmp (arg0, "kickbot_t") == 0 || stricmp (arg0, "kick_t") == 0) {
      bots.kickFromTeam (TEAM_TERRORIST);
   }

   // kicking off one bot from the counter-terrorist team
   else if (stricmp (arg0, "kickbot_ct") == 0 || stricmp (arg0, "kick_ct") == 0) {
      bots.kickFromTeam (TEAM_COUNTER);
   }
   // kills all bots on the terrorist team
   else if (stricmp (arg0, "killbots_t") == 0 || stricmp (arg0, "kill_t") == 0) {
      bots.killAllBots (TEAM_TERRORIST);
   }

   // kills all bots on the counter-terrorist team
   else if (stricmp (arg0, "killbots_ct") == 0 || stricmp (arg0, "kill_ct") == 0) {
      bots.killAllBots (TEAM_COUNTER);
   }

   // list all bots playeing on the server
   else if (stricmp (arg0, "listbots") == 0 || stricmp (arg0, "list") == 0) {
      bots.listBots ();
   }

   // kick off all bots from the played server
   else if (stricmp (arg0, "kickbots") == 0 || stricmp (arg0, "kickall") == 0) {
      bots.kickEveryone ();
   }

   // kill all bots on the played server
   else if (stricmp (arg0, "killbots") == 0 || stricmp (arg0, "killall") == 0) {
      bots.killAllBots ();
   }

   // kick off one random bot from the played server
   else if (stricmp (arg0, "kickone") == 0 || stricmp (arg0, "kick") == 0) {
      bots.kickRandom ();
   }

   // fill played server with bots
   else if (stricmp (arg0, "fillserver") == 0 || stricmp (arg0, "fill") == 0) {
      bots.serverFill (atoi (arg1), isEmptyStr (arg2) ? -1 : atoi (arg2), isEmptyStr (arg3) ? -1 : atoi (arg3), isEmptyStr (arg4) ? -1 : atoi (arg4));
   }

   // select the weapon mode for bots
   else if (stricmp (arg0, "weaponmode") == 0 || stricmp (arg0, "wmode") == 0) {
      int selection = atoi (arg1);

      // check is selected range valid
      if (selection >= 1 && selection <= 7)
         bots.setWeaponMode (selection);
      else
         engine.clientPrint (ent, "Choose weapon from 1 to 7 range");
   }

   // force all bots to vote to specified map
   else if (stricmp (arg0, "votemap") == 0) {
      if (!isEmptyStr (arg1)) {
         int nominatedMap = atoi (arg1);

         // loop through all players
         for (int i = 0; i < engine.maxClients (); i++) {
            Bot *bot = bots.getBot (i);

            if (bot != nullptr)
               bot->m_voteMap = nominatedMap;
         }
         engine.clientPrint (ent, "All dead bots will vote for map #%d", nominatedMap);
      }
   }

   // displays version information
   else if (stricmp (arg0, "version") == 0 || stricmp (arg0, "ver") == 0) {
      char versionData[] = "------------------------------------------------\n"
                           "Name: %s\n"
                           "Version: %s (Build: %u)\n"
                           "Compiled: %s, %s\n"
                           "Git Hash: %s\n"
                           "Git Commit Author: %s\n"
                           "------------------------------------------------";

      engine.clientPrint (ent, versionData, PRODUCT_NAME, PRODUCT_VERSION, buildNumber (), __DATE__, __TIME__, PRODUCT_GIT_HASH, PRODUCT_GIT_COMMIT_AUTHOR);
   }

   // display some sort of help information
   else if (strcmp (arg0, "?") == 0 || strcmp (arg0, "help") == 0) {
      engine.clientPrint (ent, "Bot Commands:");
      engine.clientPrint (ent, "%s version\t - display version information.", self);
      engine.clientPrint (ent, "%s add\t - create a bot in current game.", self);
      engine.clientPrint (ent, "%s fill\t - fill the server with random bots.", self);
      engine.clientPrint (ent, "%s kickall\t - disconnects all bots from current game.", self);
      engine.clientPrint (ent, "%s killbots\t - kills all bots in current game.", self);
      engine.clientPrint (ent, "%s kick\t - disconnect one random bot from game.", self);
      engine.clientPrint (ent, "%s weaponmode\t - select bot weapon mode.", self);
      engine.clientPrint (ent, "%s votemap\t - allows dead bots to vote for specific map.", self);
      engine.clientPrint (ent, "%s cmenu\t - displaying bots command menu.", self);

      if (strcmp (arg1, "full") == 0 || strcmp (arg1, "?") == 0) {
         engine.clientPrint (ent, "%s add_t\t - creates one random bot to terrorist team.", self);
         engine.clientPrint (ent, "%s add_ct\t - creates one random bot to ct team.", self);
         engine.clientPrint (ent, "%s kick_t\t - disconnect one random bot from terrorist team.", self);
         engine.clientPrint (ent, "%s kick_ct\t - disconnect one random bot from ct team.", self);
         engine.clientPrint (ent, "%s kill_t\t - kills all bots on terrorist team.", self);
         engine.clientPrint (ent, "%s kill_ct\t - kills all bots on ct team.", self);
         engine.clientPrint (ent, "%s list\t - display list of bots currently playing.", self);
         engine.clientPrint (ent, "%s deletewp\t - erase waypoint file from hard disk (permanently).", self);

         if (!engine.isDedicated ()) {
            engine.print ("%s autowp\t - toggle autowaypointing.", self);
            engine.print ("%s wp\t - toggle waypoint showing.", self);
            engine.print ("%s wp on noclip\t - enable noclip cheat", self);
            engine.print ("%s wp save nocheck\t - save waypoints without checking.", self);
            engine.print ("%s wp add\t - open menu for waypoint creation.", self);
            engine.print ("%s wp delete\t - delete waypoint nearest to host edict.", self);
            engine.print ("%s wp menu\t - open main waypoint menu.", self);
            engine.print ("%s wp addbasic\t - creates basic waypoints on map.", self);
            engine.print ("%s wp find\t - show direction to specified waypoint.", self);
            engine.print ("%s wp load\t - load the waypoint file from hard disk.", self);
            engine.print ("%s wp check\t - checks if all waypoints connections are valid.", self);
            engine.print ("%s wp cache\t - cache nearest waypoint.", self);
            engine.print ("%s wp teleport\t - teleport hostile to specified waypoint.", self);
            engine.print ("%s wp setradius\t - manually sets the wayzone radius for this waypoint.", self);
            engine.print ("%s path autodistance - opens menu for setting autopath maximum distance.", self);
            engine.print ("%s path cache\t - remember the nearest to player waypoint.", self);
            engine.print ("%s path create\t - opens menu for path creation.", self);
            engine.print ("%s path delete\t - delete path from cached to nearest waypoint.", self);
            engine.print ("%s path create_in\t - creating incoming path connection.", self);
            engine.print ("%s path create_out\t - creating outgoing path connection.", self);
            engine.print ("%s path create_both\t - creating both-ways path connection.", self);
            engine.print ("%s exp save\t - save the experience data.", self);
         }
      }
   }
   else if (stricmp (arg0, "bot_takedamage") == 0 && !isEmptyStr (arg1)) {
      bool isOn = !!(atoi (arg1) == 1);

      for (int i = 0; i < engine.maxClients (); i++) {
         Bot *bot = bots.getBot (i);

         if (bot != nullptr) {
            bot->pev->takedamage = isOn ? 0.0f : 1.0f;
         }
      }
   }

   // displays main bot menu
   else if (stricmp (arg0, "botmenu") == 0 || stricmp (arg0, "menu") == 0) {
      showMenu (ent, BOT_MENU_MAIN);
   }

   // display command menu
   else if (stricmp (arg0, "cmdmenu") == 0 || stricmp (arg0, "cmenu") == 0) {
      if (isAlive (ent)) {
         showMenu (ent, BOT_MENU_COMMANDS);
      }
      else {
         showMenu (ent, BOT_MENU_INVALID); // reset menu display
         engine.centerPrint ("You're dead, and have no access to this menu");
      }
   }

   // waypoint manimupulation (really obsolete, can be edited through menu) (supported only on listen server)
   else if (stricmp (arg0, "waypoint") == 0 || stricmp (arg0, "wp") == 0 || stricmp (arg0, "wpt") == 0) {
      if (engine.isDedicated () || engine.isNullEntity (g_hostEntity)) {
         return 2;
      }

      // enables or disable waypoint displaying
      if (stricmp (arg1, "on") == 0) {
         g_waypointOn = true;
         engine.print ("Waypoint Editing Enabled");

         // enables noclip cheat
         if (!isEmptyStr (arg2) && stricmp (arg2, "noclip") == 0) {
            if (g_editNoclip) {
               g_hostEntity->v.movetype = MOVETYPE_WALK;
               engine.print ("Noclip Cheat Disabled");

               g_editNoclip = false;
            }
            else {
               g_hostEntity->v.movetype = MOVETYPE_NOCLIP;
               engine.print ("Noclip Cheat Enabled");

               g_editNoclip = true;
            }
         }
         engine.execCmd ("yapb wp mdl on");
      }

      // switching waypoint editing off
      else if (stricmp (arg1, "off") == 0) {
         g_waypointOn = false;
         g_editNoclip = false;
         g_hostEntity->v.movetype = MOVETYPE_WALK;

         engine.print ("Waypoint Editing Disabled");
         engine.execCmd ("yapb wp mdl off");
      }

      // toggles displaying player models on spawn spots
      else if (stricmp (arg1, "mdl") == 0 || stricmp (arg1, "models") == 0) {

         auto toggleDraw = [] (bool draw, const String &stuff) {
            edict_t *ent = nullptr;

            while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityByString (ent, "classname", stuff.chars ()))) {
               if (draw) {
                  ent->v.effects &= ~EF_NODRAW;
               }
               else {
                  ent->v.effects |= EF_NODRAW;
               }
            }
         };
         StringArray entities;

         entities.push ("info_player_start");
         entities.push ("info_player_deathmatch");
         entities.push ("info_vip_start");

         if (stricmp (arg2, "on") == 0) {
            for (auto &type : entities) {
               toggleDraw (true, type);
            }

            engine.execCmd ("mp_roundtime 9"); // reset round time to maximum
            engine.execCmd ("mp_timelimit 0"); // disable the time limit
            engine.execCmd ("mp_freezetime 0"); // disable freezetime
         }
         else if (stricmp (arg2, "off") == 0) {
            for (auto &type : entities) {
               toggleDraw (false, type);
            }
         }
      }

      // show direction to specified waypoint
      else if (stricmp (arg1, "find") == 0) {
         waypoints.setSearchIndex (atoi (arg2));
      }

      // opens adding waypoint menu
      else if (stricmp (arg1, "add") == 0) {
         g_waypointOn = true; // turn waypoints on
         showMenu (g_hostEntity, BOT_MENU_WAYPOINT_TYPE);
      }

      // creates basic waypoints on the map (ladder/spawn points/goals)
      else if (stricmp (arg1, "addbasic") == 0) {
         waypoints.addBasic ();
         engine.centerPrint ("Basic waypoints was Created");
      }

      // delete nearest to host edict waypoint
      else if (stricmp (arg1, "delete") == 0) {
         g_waypointOn = true; // turn waypoints on
         waypoints.erase ();
      }

      // save waypoint data into file on hard disk
      else if (stricmp (arg1, "save") == 0) {
         const char *waypointSaveMessage = engine.translate ("Waypoints Saved");

         if (strcmp (arg2, "nocheck") == 0) {
            waypoints.save ();
            engine.print (waypointSaveMessage);
         }
         else if (waypoints.checkNodes ()) {
            waypoints.save ();
            engine.print (waypointSaveMessage);
         }
      }

      // remove waypoint and all corresponding files from hard disk
      else if (stricmp (arg1, "erase") == 0) {
         waypoints.eraseFromDisk ();
      }

      // load all waypoints again (overrides all changes, that wasn't saved)
      else if (stricmp (arg1, "load") == 0) {
         if (waypoints.load ())
            engine.print ("Waypoints loaded");
      }

      // check all nodes for validation
      else if (stricmp (arg1, "check") == 0) {
         if (waypoints.checkNodes ())
            engine.centerPrint ("Nodes work Fine");
      }

      // opens menu for setting (removing) waypoint flags
      else if (stricmp (arg1, "flags") == 0) {
         showMenu (g_hostEntity, BOT_MENU_WAYPOINT_FLAG);
      }

      // setting waypoint radius
      else if (stricmp (arg1, "setradius") == 0) {
         waypoints.setRadius (atoi (arg2));
      }

      // remembers nearest waypoint
      else if (stricmp (arg1, "cache") == 0) {
         waypoints.cachePoint ();
      }

      // teleport player to specified waypoint
      else if (stricmp (arg1, "teleport") == 0) {
         int teleportPoint = atoi (arg2);

         if (waypoints.exists (teleportPoint)) {
            Path &path = waypoints[teleportPoint];

            g_engfuncs.pfnSetOrigin (g_hostEntity, path.origin);
            g_waypointOn = true;

            engine.print ("Player '%s' teleported to waypoint #%d (x:%.1f, y:%.1f, z:%.1f)", STRING (g_hostEntity->v.netname), teleportPoint, path.origin.x, path.origin.y, path.origin.z); //-V807
            g_editNoclip = true;
         }
      }

      // displays waypoint menu
      else if (stricmp (arg1, "menu") == 0) {
         showMenu (g_hostEntity, BOT_MENU_WAYPOINT_MAIN_PAGE1);
      }

      // otherwise display waypoint current status
      else {
         engine.print ("Waypoints are %s", g_waypointOn == true ? "Enabled" : "Disabled");
      }
   }

   // path waypoint editing system (supported only on listen server)
   else if (stricmp (arg0, "pathwaypoint") == 0 || stricmp (arg0, "path") == 0 || stricmp (arg0, "pwp") == 0) {
      if (engine.isDedicated () || engine.isNullEntity (g_hostEntity)) {
         return 2;
      }

      // opens path creation menu
      if (stricmp (arg1, "create") == 0) {
         showMenu (g_hostEntity, BOT_MENU_WAYPOINT_PATH);
      }

      // creates incoming path from the cached waypoint
      else if (stricmp (arg1, "create_in") == 0) {
         waypoints.pathCreate (CONNECTION_INCOMING);
      }

      // creates outgoing path from current waypoint
      else if (stricmp (arg1, "create_out") == 0) {
         waypoints.pathCreate (CONNECTION_OUTGOING);
      }

      // creates bidirectional path from cached to current waypoint
      else if (stricmp (arg1, "create_both") == 0) {
         waypoints.pathCreate (CONNECTION_BOTHWAYS);
      }

      // delete special path
      else if (stricmp (arg1, "delete") == 0) {
         waypoints.erasePath ();
      }

      // sets auto path maximum distance
      else if (stricmp (arg1, "autodistance") == 0) {
         showMenu (g_hostEntity, BOT_MENU_WAYPOINT_AUTOPATH);
      }
   }

   // automatic waypoint handling (supported only on listen server)
   else if (stricmp (arg0, "autowaypoint") == 0 || stricmp (arg0, "autowp") == 0) {
      if (engine.isDedicated () || engine.isNullEntity (g_hostEntity)) {
         return 2;
      }

      // enable autowaypointing
      if (stricmp (arg1, "on") == 0) {
         g_autoWaypoint = true;
         g_waypointOn = true; // turn this on just in case
      }

      // disable autowaypointing
      else if (stricmp (arg1, "off") == 0) {
         g_autoWaypoint = false;
      }

      // display status
      engine.print ("Auto-Waypoint %s", g_autoWaypoint ? "Enabled" : "Disabled");
   }

   // experience system handling (supported only on listen server)
   else if (stricmp (arg0, "experience") == 0 || stricmp (arg0, "exp") == 0) {
      if (engine.isDedicated () || engine.isNullEntity (g_hostEntity)) {
         return 2;
      }

      // write experience table (and visibility table) to hard disk
      if (stricmp (arg1, "save") == 0) {
         waypoints.saveExperience ();
         waypoints.saveVisibility ();

         engine.print ("Experience tab saved");
      }
   }
   else {
      return 0; // command is not handled by bot
   }
   return 1; // command was handled by bot
}

void processBotConfigs (bool onlyMain) {
   static bool setMemoryPointers = true;

   if (setMemoryPointers) {
      MemoryLoader::ref ().setup (g_engfuncs.pfnLoadFileForMe, g_engfuncs.pfnFreeFile);
      setMemoryPointers = true;
   }

   auto isCommentLine = [] (const char *line) {
      char ch = *line;
      return ch == '#' || ch == '/' || ch == '\r' || ch == ';' || ch == 0 || ch == ' ';
   };

   MemFile fp;
   char lineBuffer[512];

   // this is does the same as exec of engine, but not overwriting values of cvars spcified in yb_ignore_cvars_on_changelevel
   if (onlyMain) {
      static bool firstLoad = true;

      auto needsToIgnoreVar = [](StringArray &list, const char *needle) {
         for (auto &var : list) {
            if (var == needle) {
               return true;
            }
         }
         return false;
      };

      if (openConfig ("yapb.cfg", "YaPB main config file is not found.", &fp, false)) {
         while (fp.gets (lineBuffer, 255)) {
            if (isCommentLine (lineBuffer)) {
               continue;
            }
            if (firstLoad) {
               engine.execCmd (lineBuffer);
               continue;
            }
            auto keyval = String (lineBuffer).split (" ");

            if (keyval.length () > 1) {
               auto ignore = String (yb_ignore_cvars_on_changelevel.str ()).split (",");

               auto key = keyval[0].trim ().chars ();
               auto cvar = g_engfuncs.pfnCVarGetPointer (key);

               if (cvar != nullptr) {
                  auto value = const_cast <char *> (keyval[1].trim ().trim ("\"").trim ().chars ());

                  if (needsToIgnoreVar (ignore, key)) {
                     if (!!stricmp (value, cvar->string)) {
                        engine.print ("Bot CVAR '%s' is differs from stored in config (%s/%s). Ignoring.", cvar->name, cvar->string, value);

                        // ensure cvar will have old value
                        g_engfuncs.pfnCvar_DirectSet (cvar, cvar->string);
                     }
                     else {
                        g_engfuncs.pfnCvar_DirectSet (cvar, value);
                     }
                  }
               }
               else
                  engine.execCmd (lineBuffer);
            }
         }
         fp.close ();
      }
      firstLoad = false;
      return;
   }
   KeywordFactory replies;

   // fixes for crashing if configs couldn't be accessed
   g_chatFactory.reserve (CHAT_TOTAL);
   g_chatterFactory.reserve (Chatter_Total);

   // NAMING SYSTEM INITIALIZATION
   if (openConfig ("names.cfg", "Name configuration file not found.", &fp, true)) {
      g_botNames.clear ();

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         StringArray pair = String (lineBuffer).split ("\t\t");

         if (pair.length () > 1) {
            strncpy (lineBuffer, pair[0].trim ().chars (), cr::bufsize (lineBuffer));
         }

         String::trimChars (lineBuffer);
         lineBuffer[32] = 0;

         BotName item;
         item.name = lineBuffer;
         item.usedBy = 0;

         if (pair.length () > 1) {
            item.steamId = pair[1].trim ();
         }
         g_botNames.push (item);
      }
      fp.close ();
   }

   // CHAT SYSTEM CONFIG INITIALIZATION
   if (openConfig ("chat.cfg", "Chat file not found.", &fp, true)) {
     
      int chatType = -1;

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         String section (lineBuffer, strlen (lineBuffer) - 1);
         section.trim ();

         if (section == "[KILLED]") {
            chatType = 0;
            continue;
         }
         else if (section == "[BOMBPLANT]") {
            chatType = 1;
            continue;
         }
         else if (section == "[DEADCHAT]") {
            chatType = 2;
            continue;
         }
         else if (section == "[REPLIES]") {
            chatType = 3;
            continue;
         }
         else if (section == "[UNKNOWN]") {
            chatType = 4;
            continue;
         }
         else if (section == "[TEAMATTACK]") {
            chatType = 5;
            continue;
         }
         else if (section == "[WELCOME]") {
            chatType = 6;
            continue;
         }
         else if (section == "[TEAMKILL]") {
            chatType = 7;
            continue;
         }

         if (chatType != 3) {
            lineBuffer[79] = 0;
         }

         switch (chatType) {
         case 0:
            g_chatFactory[CHAT_KILLING].push (lineBuffer);
            break;

         case 1:
            g_chatFactory[CHAT_BOMBPLANT].push (lineBuffer);
            break;

         case 2:
            g_chatFactory[CHAT_DEAD].push (lineBuffer);
            break;

         case 3:
            if (strstr (lineBuffer, "@KEY") != nullptr) {
               if (!replies.keywords.empty () && !replies.replies.empty ()) {
                  g_replyFactory.push (cr::forward <KeywordFactory> (replies));
                  replies.replies.clear ();
               }

               replies.keywords.clear ();
               replies.keywords = String (&lineBuffer[4]).split (",");

               for (auto &keywords : replies.keywords) {
                  keywords.trim ().trim ("\"");
               }
            }
            else if (!replies.keywords.empty ()) {
               replies.replies.push (lineBuffer);
            }
            break;

         case 4:
            g_chatFactory[CHAT_NOKW].push (lineBuffer);
            break;

         case 5:
            g_chatFactory[CHAT_TEAMATTACK].push (lineBuffer);
            break;

         case 6:
            g_chatFactory[CHAT_WELCOME].push (lineBuffer);
            break;

         case 7:
            g_chatFactory[CHAT_TEAMKILL].push (lineBuffer);
            break;
         }
      }
      fp.close ();
   }
   else {
      extern ConVar yb_chat;
      yb_chat.set (0);
   }

   // GENERAL DATA INITIALIZATION
   if (openConfig ("general.cfg", "General configuration file not found. Loading defaults", &fp)) {
      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         auto pair = String (lineBuffer).split ("=");

         if (pair.length () != 2) {
            continue;
         }

         for (auto &trim : pair) {
            trim.trim ();
         }
         auto splitted = pair[1].split (",");

         if (pair[0] == "MapStandard") {
            if (splitted.length () != NUM_WEAPONS) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < NUM_WEAPONS; i++) {
               g_weaponSelect[i].teamStandard = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "MapAS") {
            if (splitted.length () != NUM_WEAPONS) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < NUM_WEAPONS; i++) {
               g_weaponSelect[i].teamAS = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "GrenadePercent") {
            if (splitted.length () != 3) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < 3; i++) {
               g_grenadeBuyPrecent[i] = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "Economics") {
            if (splitted.length () != 11) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < 11; i++) {
               g_botBuyEconomyTable[i] = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "PersonalityNormal") {
            if (splitted.length () != NUM_WEAPONS) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < NUM_WEAPONS; i++) {
               g_normalWeaponPrefs[i] = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "PersonalityRusher") {
            if (splitted.length () != NUM_WEAPONS) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < NUM_WEAPONS; i++) {
               g_rusherWeaponPrefs[i] = splitted[i].toInt32 ();
            }
         }
         else if (pair[0] == "PersonalityCareful") {
            if (splitted.length () != NUM_WEAPONS) {
               logEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].chars ());
            }

            for (int i = 0; i < NUM_WEAPONS; i++) {
               g_carefulWeaponPrefs[i] = splitted[i].toInt32 ();
            }
         }
      }
      fp.close ();
   }

   // CHATTER SYSTEM INITIALIZATION
   if ((g_gameFlags & GAME_SUPPORT_BOT_VOICE) && yb_communication_type.integer () == 2 && openConfig ("chatter.cfg", "Couldn't open chatter system configuration", &fp)) {
      const float ChatterInfinity = MAX_CHATTER_REPEAT;

      struct EventMap {
         const char *str;
         int code;
         float repeat;
      } chatterEventMap[] = {
         { "Radio_CoverMe", Radio_CoverMe, ChatterInfinity },
         { "Radio_YouTakePoint", Radio_YouTakePoint, ChatterInfinity },
         { "Radio_HoldPosition", Radio_HoldPosition, ChatterInfinity },
         { "Radio_RegroupTeam", Radio_RegroupTeam, ChatterInfinity },
         { "Radio_FollowMe", Radio_FollowMe, ChatterInfinity },
         { "Radio_TakingFire", Radio_TakingFire, ChatterInfinity },
         { "Radio_GoGoGo", Radio_GoGoGo, ChatterInfinity },
         { "Radio_Fallback", Radio_Fallback, ChatterInfinity },
         { "Radio_StickTogether", Radio_StickTogether, ChatterInfinity },
         { "Radio_GetInPosition", Radio_GetInPosition, ChatterInfinity },
         { "Radio_StormTheFront", Radio_StormTheFront, ChatterInfinity },
         { "Radio_ReportTeam", Radio_ReportTeam, ChatterInfinity },
         { "Radio_Affirmative", Radio_Affirmative, ChatterInfinity },
         { "Radio_EnemySpotted", Radio_EnemySpotted, ChatterInfinity },
         { "Radio_NeedBackup", Radio_NeedBackup, ChatterInfinity },
         { "Radio_SectorClear", Radio_SectorClear, ChatterInfinity },
         { "Radio_InPosition", Radio_InPosition, ChatterInfinity },
         { "Radio_ReportingIn", Radio_ReportingIn, ChatterInfinity },
         { "Radio_ShesGonnaBlow", Radio_ShesGonnaBlow, ChatterInfinity },
         { "Radio_Negative", Radio_Negative, ChatterInfinity },
         { "Radio_EnemyDown", Radio_EnemyDown, ChatterInfinity },
         { "Chatter_DiePain", Chatter_DiePain, ChatterInfinity },
         { "Chatter_GoingToPlantBomb", Chatter_GoingToPlantBomb, ChatterInfinity },
         { "Chatter_GoingToGuardVIPSafety", Chatter_GoingToGuardVIPSafety, ChatterInfinity },
         { "Chatter_RescuingHostages", Chatter_RescuingHostages, ChatterInfinity },
         { "Chatter_TeamKill", Chatter_TeamKill, ChatterInfinity },
         { "Chatter_GuardingVipSafety", Chatter_GuardingVipSafety, ChatterInfinity },
         { "Chatter_PlantingC4", Chatter_PlantingC4, ChatterInfinity },
         { "Chatter_InCombat", Chatter_InCombat,  ChatterInfinity },
         { "Chatter_SeeksEnemy", Chatter_SeeksEnemy, ChatterInfinity },
         { "Chatter_Nothing", Chatter_Nothing,  ChatterInfinity },
         { "Chatter_EnemyDown", Chatter_EnemyDown, ChatterInfinity },
         { "Chatter_UseHostage", Chatter_UseHostage, ChatterInfinity },
         { "Chatter_WonTheRound", Chatter_WonTheRound, ChatterInfinity },
         { "Chatter_QuicklyWonTheRound", Chatter_QuicklyWonTheRound, ChatterInfinity },
         { "Chatter_NoEnemiesLeft", Chatter_NoEnemiesLeft, ChatterInfinity },
         { "Chatter_FoundBombPlace", Chatter_FoundBombPlace, ChatterInfinity },
         { "Chatter_WhereIsTheBomb", Chatter_WhereIsTheBomb, ChatterInfinity },
         { "Chatter_DefendingBombSite", Chatter_DefendingBombSite, ChatterInfinity },
         { "Chatter_BarelyDefused", Chatter_BarelyDefused, ChatterInfinity },
         { "Chatter_NiceshotCommander", Chatter_NiceshotCommander, ChatterInfinity },
         { "Chatter_ReportingIn", Chatter_ReportingIn, 10.0f },
         { "Chatter_SpotTheBomber", Chatter_SpotTheBomber, 4.3f },
         { "Chatter_VIPSpotted", Chatter_VIPSpotted, 5.3f },
         { "Chatter_FriendlyFire", Chatter_FriendlyFire, 2.1f },
         { "Chatter_GotBlinded", Chatter_GotBlinded, 5.0f },
         { "Chatter_GuardDroppedC4", Chatter_GuardDroppedC4, 3.0f },
         { "Chatter_DefusingC4", Chatter_DefusingC4, 3.0f },
         { "Chatter_FoundC4", Chatter_FoundC4, 5.5f },
         { "Chatter_ScaredEmotion", Chatter_ScaredEmotion, 6.1f },
         { "Chatter_HeardEnemy", Chatter_ScaredEmotion, 12.8f },
         { "Chatter_SniperWarning", Chatter_SniperWarning, 14.3f },
         { "Chatter_SniperKilled", Chatter_SniperKilled, 2.1f },
         { "Chatter_OneEnemyLeft", Chatter_OneEnemyLeft, 2.5f },
         { "Chatter_TwoEnemiesLeft", Chatter_TwoEnemiesLeft, 2.5f },
         { "Chatter_ThreeEnemiesLeft", Chatter_ThreeEnemiesLeft, 2.5f },
         { "Chatter_NiceshotPall", Chatter_NiceshotPall, 2.0f },
         { "Chatter_GoingToGuardHostages", Chatter_GoingToGuardHostages, 3.0f },
         { "Chatter_GoingToGuardDoppedBomb", Chatter_GoingToGuardDoppedBomb, 3.0f },
         { "Chatter_OnMyWay", Chatter_OnMyWay, 1.5f },
         { "Chatter_LeadOnSir", Chatter_LeadOnSir, 5.0f },
         { "Chatter_Pinned_Down", Chatter_Pinned_Down, 5.0f },
         { "Chatter_GottaFindTheBomb", Chatter_GottaFindTheBomb, 3.0f },
         { "Chatter_You_Heard_The_Man", Chatter_You_Heard_The_Man, 3.0f },
         { "Chatter_Lost_The_Commander", Chatter_Lost_The_Commander, 4.5f },
         { "Chatter_NewRound", Chatter_NewRound, 3.5f },
         { "Chatter_CoverMe", Chatter_CoverMe, 3.5f },
         { "Chatter_BehindSmoke", Chatter_BehindSmoke, 3.5f },
         { "Chatter_BombSiteSecured", Chatter_BombSiteSecured, 3.5f },
         { "Chatter_GoingToCamp", Chatter_GoingToCamp, 25.0f },
         { "Chatter_Camp", Chatter_Camp, 25.0f },
      };

      while (fp.gets (lineBuffer, 511)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }

         if (strncmp (lineBuffer, "RewritePath", 11) == 0) {
            extern ConVar yb_chatter_path;
            yb_chatter_path.set (String (&lineBuffer[12]).trim ().chars ());
         }
         else if (strncmp (lineBuffer, "Event", 5) == 0) {
            auto items = String (&lineBuffer[6]).split ("=");

            if (items.length () != 2) {
               logEntry (true, LL_ERROR, "Error in chatter config file syntax... Please correct all errors.");
               continue;
            }

            for (auto &item : items) {
               item.trim ();
            }
            items[1].trim ("(;)");

            for (size_t i = 0; i < cr::arrsize (chatterEventMap); i++) {
               auto event = &chatterEventMap[i];

               if (stricmp (event->str, items[0].chars ()) == 0) {
                  // this does common work of parsing comma-separated chatter line
                  auto sounds = items[1].split (",");

                  for (auto &sound : sounds) {
                     sound.trim ().trim ("\"");
                     float duration = engine.getWaveLen (sound.chars ());

                     if (duration > 0.0f) {
                        g_chatterFactory[event->code].push ({ sound, event->repeat, duration });
                     }
                  }
                  sounds.clear ();
               }
            }
         }
      }
      fp.close ();
   }
   else {
      yb_communication_type.set (1);
      logEntry (true, LL_DEFAULT, "Chatter Communication disabled.");
   }

   // LOCALIZER INITITALIZATION
   if (!(g_gameFlags & GAME_LEGACY) && openConfig ("lang.cfg", "Specified language not found", &fp, true)) {
      if (engine.isDedicated ()) {
         return; // dedicated server will use only english translation
      }
      enum Lang { LANG_ORIGINAL, LANG_TRANSLATED, LANG_UNDEFINED } langState = static_cast <Lang> (LANG_UNDEFINED);

      char buffer[1024] = { 0, };
      Pair<String, String> lang;

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }

         if (strncmp (lineBuffer, "[ORIGINAL]", 10) == 0) {
            langState = LANG_ORIGINAL;

            if (buffer[0] != 0) {
               lang.second = buffer;
               lang.second.trim ();

               buffer[0] = 0;
            }

            if (!lang.second.empty () && !lang.first.empty ()) {
               engine.addTranslation (lang.first, lang.second);
            }
         }
         else if (strncmp (lineBuffer, "[TRANSLATED]", 12) == 0) {

            lang.first = buffer;
            lang.first.trim ();

            langState = LANG_TRANSLATED;
            buffer[0] = 0;
         }
         else {
            switch (langState) {
            case LANG_ORIGINAL:
               strncat (buffer, lineBuffer, 1024 - 1 - strlen (buffer));
               break;

            case LANG_TRANSLATED:
               strncat (buffer, lineBuffer, 1024 - 1 - strlen (buffer));
               break;

            case LANG_UNDEFINED:
               break;
            }
         }
      }
      fp.close ();
   }
   else if (g_gameFlags & GAME_LEGACY) {
      logEntry (true, LL_DEFAULT, "Multilingual system disabled, due to your Counter-Strike Version!");
   }
   else if (strcmp (yb_language.str (), "en") != 0) {
      logEntry (true, LL_ERROR, "Couldn't load language configuration");
   }

   // set personality weapon pointers here
   g_weaponPrefs[PERSONALITY_NORMAL] = reinterpret_cast <int *> (&g_normalWeaponPrefs);
   g_weaponPrefs[PERSONALITY_RUSHER] = reinterpret_cast <int *> (&g_rusherWeaponPrefs);
   g_weaponPrefs[PERSONALITY_CAREFUL] = reinterpret_cast <int *> (&g_carefulWeaponPrefs);

   g_timePerSecondUpdate = 0.0f;
}

void GameDLLInit (void) {
   // this function is a one-time call, and appears to be the second function called in the
   // DLL after GiveFntprsToDll() has been called. Its purpose is to tell the MOD DLL to
   // initialize the game before the engine actually hooks into it with its video frames and
   // clients connecting. Note that it is a different step than the *server* initialization.
   // This one is called once, and only once, when the game process boots up before the first
   // server is enabled. Here is a good place to do our own game session initialization, and
   // to register by the engine side the server commands we need to administrate our bots.

   auto commandHandler = [](void) {
      if (handleBotCommands (g_hostEntity, isEmptyStr (g_engfuncs.pfnCmd_Argv (1)) ? "help" : g_engfuncs.pfnCmd_Argv (1), g_engfuncs.pfnCmd_Argv (2), g_engfuncs.pfnCmd_Argv (3), g_engfuncs.pfnCmd_Argv (4), g_engfuncs.pfnCmd_Argv (5), g_engfuncs.pfnCmd_Argv (6), g_engfuncs.pfnCmd_Argv (0)) == 0) {
         engine.print ("Unknown command: %s", g_engfuncs.pfnCmd_Argv (1));
      }
   };

   // register server command(s)
   engine.registerCmd ("yapb", commandHandler);
   engine.registerCmd ("yb", commandHandler);

   // set correct version string
   yb_version.set (format ("%d.%d.%d", PRODUCT_VERSION_DWORD_INTERNAL, buildNumber ()));

   // execute main config
   processBotConfigs (true);

   // register fake metamod command handler if we not! under mm
   if (!(g_gameFlags & GAME_METAMOD)) {
      engine.registerCmd ("meta", [](void) { engine.print ("You're launched standalone version of yapb. Metamod is not installed or not enabled!"); });
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnGameInit ();
}

void Touch (edict_t *pentTouched, edict_t *pentOther) {
   // this function is called when two entities' bounding boxes enter in collision. For example,
   // when a player walks upon a gun, the player entity bounding box collides to the gun entity
   // bounding box, and the result is that this function is called. It is used by the game for
   // taking the appropriate action when such an event occurs (in our example, the player who
   // is walking upon the gun will "pick it up"). Entities that "touch" others are usually
   // entities having a velocity, as it is assumed that static entities (entities that don't
   // move) will never touch anything. Hence, in our example, the pentTouched will be the gun
   // (static entity), whereas the pentOther will be the player (as it is the one moving). When
   // the two entities both have velocities, for example two players colliding, this function
   // is called twice, once for each entity moving.

   if (!engine.isNullEntity (pentTouched) && (pentTouched->v.flags & FL_FAKECLIENT) && pentOther != engine.getStartEntity ()) {
      Bot *bot = bots.getBot (pentTouched);

      if (bot != nullptr && pentOther != bot->ent ()) {

         if (isPlayer (pentOther) && isAlive (pentOther)) {
            bot->avoidIncomingPlayers (pentOther);
         }
         else {
            bot->processBreakables (pentOther);
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnTouch (pentTouched, pentOther);
}

int Spawn (edict_t *ent) {
   // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
   // world, in other words to 'display') the entity pointed to by ent in the game. The
   // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
   // and any MOD is supposed to implement one for each of its entities.

   engine.precache ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
   }
   int result = g_functionTable.pfnSpawn (ent); // get result

   if (ent->v.rendermode == kRenderTransTexture) {
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
   }
   return result;
}

void UpdateClientData (const struct edict_s *ent, int sendweapons, struct clientdata_s *cd) {
   extern ConVar yb_latency_display;

   if ((g_gameFlags & GAME_SUPPORT_SVC_PINGS) && yb_latency_display.integer () == 2) {
      bots.sendPingOffsets (const_cast <edict_t *> (ent));
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnUpdateClientData (ent, sendweapons, cd);
}

int ClientConnect (edict_t *ent, const char *name, const char *addr, char rejectReason[128]) {
   // this function is called in order to tell the MOD DLL that a client attempts to connect the
   // game. The entity pointer of this client is ent, the name under which he connects is
   // pointed to by the pszName pointer, and its IP address string is pointed by the pszAddress
   // one. Note that this does not mean this client will actually join the game ; he could as
   // well be refused connection by the server later, because of latency timeout, unavailable
   // game resources, or whatever reason. In which case the reason why the game DLL (read well,
   // the game DLL, *NOT* the engine) refuses this player to connect will be printed in the
   // rejectReason string in all letters. Understand that a client connecting process is done
   // in three steps. First, the client requests a connection from the server. This is engine
   // internals. When there are already too many players, the engine will refuse this client to
   // connect, and the game DLL won't even notice. Second, if the engine sees no problem, the
   // game DLL is asked. This is where we are. Once the game DLL acknowledges the connection,
   // the client downloads the resources it needs and synchronizes its local engine with the one
   // of the server. And then, the third step, which comes *AFTER* ClientConnect (), is when the
   // client officially enters the game, through the ClientPutInServer () function, later below.
   // Here we hook this function in order to keep track of the listen server client entity,
   // because a listen server client always connects with a "loopback" address string. Also we
   // tell the bot manager to check the bot population, in order to always have one free slot on
   // the server for incoming clients.

   // check if this client is the listen server client
   if (strcmp (addr, "loopback") == 0) {
      g_hostEntity = ent; // save the edict of the listen server client...
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
   }
   return g_functionTable.pfnClientConnect (ent, name, addr, rejectReason);
}

void ClientDisconnect (edict_t *ent) {
   // this function is called whenever a client is VOLUNTARILY disconnected from the server,
   // either because the client dropped the connection, or because the server dropped him from
   // the game (latency timeout). The effect is the freeing of a client slot on the server. Note
   // that clients and bots disconnected because of a level change NOT NECESSARILY call this
   // function, because in case of a level change, it's a server shutdown, and not a normal
   // disconnection. I find that completely stupid, but that's it. Anyway it's time to update
   // the bots and players counts, and in case the client disconnecting is a bot, to back its
   // brain(s) up to disk. We also try to notice when a listenserver client disconnects, so as
   // to reset his entity pointer for safety. There are still a few server frames to go once a
   // listen server client disconnects, and we don't want to send him any sort of message then.

   int index = engine.indexOfEntity (ent) - 1;
   assert (index >= 0 && index < MAX_ENGINE_PLAYERS);

   Bot *bot = bots.getBot (index);

   // check if its a bot
   if (bot != nullptr && bot->pev == &ent->v) {
      bot->showChaterIcon (false);
      bots.destroy (index);
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnClientDisconnect (ent);
}

void ClientUserInfoChanged (edict_t *ent, char *infobuffer) {
   // this function is called when a player changes model, or changes team. Occasionally it
   // enforces rules on these changes (for example, some MODs don't want to allow players to
   // change their player model). But most commonly, this function is in charge of handling
   // team changes, recounting the teams population, etc...

   if (engine.isDedicated () && !isFakeClient (ent)) {
      const String &key = yb_password_key.str ();
      const String &password = yb_password.str ();

      if (!key.empty () && !password.empty ()) {
         int clientIndex = engine.indexOfEntity (ent) - 1;

         if (password == g_engfuncs.pfnInfoKeyValue (infobuffer, key.chars ())) {
            g_clients[clientIndex].flags |= CF_ADMIN;
         }
         else {
            g_clients[clientIndex].flags &= ~CF_ADMIN;
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnClientUserInfoChanged (ent, infobuffer);
}

void ClientCommand (edict_t *ent) {
   // this function is called whenever the client whose player entity is ent issues a client
   // command. How it works is that clients all have a global string in their client DLL that
   // stores the command string; if ever that string is filled with characters, the client DLL
   // sends it to the engine as a command to be executed. When the engine has executed that
   // command, that string is reset to zero. By the server side, we can access this string
   // by asking the engine with the CmdArgv(), CmdArgs() and CmdArgc() functions that work just
   // like executable files argument processing work in C (argc gets the number of arguments,
   // command included, args returns the whole string, and argv returns the wanted argument
   // only). Here is a good place to set up either bot debug commands the listen server client
   // could type in his game console, or real new client commands, but we wouldn't want to do
   // so as this is just a bot DLL, not a MOD. The purpose is not to add functionality to
   // clients. Hence it can lack of commenting a bit, since this code is very subject to change.

   const char *command = g_engfuncs.pfnCmd_Argv (0);
   const char *arg1 = g_engfuncs.pfnCmd_Argv (1);

   static int fillServerTeam = 5;
   static bool fillCommand = false;

   int issuerPlayerIndex = engine.indexOfEntity (ent) - 1;

   if (!engine.isBotCmd () && (ent == g_hostEntity || (g_clients[issuerPlayerIndex].flags & CF_ADMIN))) {
      if (stricmp (command, "yapb") == 0 || stricmp (command, "yb") == 0) {
         int state = handleBotCommands (ent, isEmptyStr (arg1) ? "help" : arg1, g_engfuncs.pfnCmd_Argv (2), g_engfuncs.pfnCmd_Argv (3), g_engfuncs.pfnCmd_Argv (4), g_engfuncs.pfnCmd_Argv (5), g_engfuncs.pfnCmd_Argv (6), g_engfuncs.pfnCmd_Argv (0));

         switch (state) {
         case 0:
            engine.clientPrint (ent, "Unknown command: %s", arg1);
            break;

         case 2:
            engine.clientPrint (ent, "Command \"%s\", can only be executed from server console.", arg1);
            break;
         }

         if (g_gameFlags & GAME_METAMOD) {
            RETURN_META (MRES_SUPERCEDE);
         }
         return;
      }
      else if (stricmp (command, "menuselect") == 0 && !isEmptyStr (arg1) && g_clients[issuerPlayerIndex].menu != BOT_MENU_INVALID) {
         Client *client = &g_clients[issuerPlayerIndex];
         int selection = atoi (arg1);

         if (client->menu == BOT_MENU_WAYPOINT_TYPE) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               waypoints.push (selection - 1);
               showMenu (ent, BOT_MENU_WAYPOINT_TYPE);

               break;

            case 8:
               waypoints.push (100);
               showMenu (ent, BOT_MENU_WAYPOINT_TYPE);

               break;

            case 9:
               waypoints.startLearnJump ();
               showMenu (ent, BOT_MENU_WAYPOINT_TYPE);

               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_FLAG) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               waypoints.toggleFlags (FLAG_NOHOSTAGE);
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 2:
               waypoints.toggleFlags (FLAG_TF_ONLY);
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 3:
               waypoints.toggleFlags (FLAG_CF_ONLY);
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 4:
               waypoints.toggleFlags (FLAG_LIFT);
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 5:
               waypoints.toggleFlags (FLAG_SNIPER);
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_MAIN_PAGE1) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               if (g_waypointOn)
                  engine.execCmd ("yapb waypoint off");
               else
                  engine.execCmd ("yapb waypoint on");

               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);
               break;

            case 2:
               g_waypointOn = true;
               waypoints.cachePoint ();
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);

               break;

            case 3:
               g_waypointOn = true;
               showMenu (ent, BOT_MENU_WAYPOINT_PATH);
               break;

            case 4:
               g_waypointOn = true;
               waypoints.erasePath ();
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);

               break;

            case 5:
               g_waypointOn = true;
               showMenu (ent, BOT_MENU_WAYPOINT_TYPE);
               break;

            case 6:
               g_waypointOn = true;
               waypoints.erase ();
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);

               break;

            case 7:
               g_waypointOn = true;
               showMenu (ent, BOT_MENU_WAYPOINT_AUTOPATH);
               break;

            case 8:
               g_waypointOn = true;
               showMenu (ent, BOT_MENU_WAYPOINT_RADIUS);
               break;

            case 9:
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_MAIN_PAGE2) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
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
               engine.print ("Waypoints: %d - T Points: %d\n"
                              "CT Points: %d - Goal Points: %d\n"
                              "Rescue Points: %d - Camp Points: %d\n"
                              "Block Hostage Points: %d - Sniper Points: %d\n",
                              waypoints.length (), terrPoints, ctPoints, goalPoints, rescuePoints, campPoints, noHostagePoints, sniperPoints);

               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
            } break;

            case 2:
               g_waypointOn = true;
               g_autoWaypoint &= 1;
               g_autoWaypoint ^= 1;

               engine.centerPrint ("Auto-Waypoint %s", g_autoWaypoint ? "Enabled" : "Disabled");
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);

               break;

            case 3:
               g_waypointOn = true;
               showMenu (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 4:
               if (waypoints.checkNodes ()) {
                  waypoints.save ();
               }
               else {
                  engine.centerPrint ("Waypoint not saved\nThere are errors, see console");
               }
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 5:
               waypoints.save ();
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 6:
               waypoints.load ();
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 7:
               if (waypoints.checkNodes ()) {
                  engine.centerPrint ("Nodes works fine");
               }
               else {
                  engine.centerPrint ("There are errors, see console");
               }
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 8:
               engine.execCmd ("yapb wp on noclip");
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 9:
               showMenu (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_RADIUS) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            g_waypointOn = true; // turn waypoints on in case

            const int radiusValue[] = {0, 8, 16, 32, 48, 64, 80, 96, 128};

            if (selection >= 1 && selection <= 9) {
               waypoints.setRadius (radiusValue[selection - 1]);
               showMenu (ent, BOT_MENU_WAYPOINT_RADIUS);
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_MAIN) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               fillCommand = false;
               showMenu (ent, BOT_MENU_CONTROL);
               break;

            case 2:
               showMenu (ent, BOT_MENU_FEATURES);
               break;

            case 3:
               fillCommand = true;
               showMenu (ent, BOT_MENU_TEAM_SELECT);
               break;

            case 4:
               bots.killAllBots ();
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;

            default:
               showMenu (ent, BOT_MENU_MAIN);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_CONTROL) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               bots.createRandom (true);
               showMenu (ent, BOT_MENU_CONTROL);
               break;

            case 2:
               showMenu (ent, BOT_MENU_DIFFICULTY);
               break;

            case 3:
               bots.kickRandom ();
               showMenu (ent, BOT_MENU_CONTROL);
               break;

            case 4:
               bots.kickEveryone ();
               break;

            case 5:
               bots.kickBotByMenu (ent, 1);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_FEATURES) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               showMenu (ent, BOT_MENU_WEAPON_MODE);
               break;

            case 2:
               showMenu (ent, engine.isDedicated () ? BOT_MENU_FEATURES : BOT_MENU_WAYPOINT_MAIN_PAGE1);
               break;

            case 3:
               showMenu (ent, BOT_MENU_PERSONALITY);
               break;

            case 4:
               extern ConVar yb_debug;
               yb_debug.set (yb_debug.integer () ^ 1);

               showMenu (ent, BOT_MENU_FEATURES);
               break;

            case 5:
               if (isAlive (ent)) {
                  showMenu (ent, BOT_MENU_COMMANDS);
               }
               else {
                  showMenu (ent, BOT_MENU_INVALID); // reset menu display
                  engine.centerPrint ("You're dead, and have no access to this menu");
               }
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_COMMANDS) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display
            Bot *bot = nullptr;

            switch (selection) {
            case 1:
            case 2:
               if (findNearestPlayer (reinterpret_cast <void **> (&bot), client->ent, 600.0f, true, true, true)) {
                  if (!bot->m_hasC4 && !bot->hasHostage ()) {
                     if (selection == 1) {
                        bot->startDoubleJump (client->ent);
                     }
                     else {
                        bot->resetDoubleJump ();
                     }
                  }
               }
               showMenu (ent, BOT_MENU_COMMANDS);
               break;

            case 3:
            case 4:
               if (findNearestPlayer (reinterpret_cast <void **> (&bot), ent, 600.0f, true, true, true, true, selection == 4 ? false : true)) {
                  bot->dropWeaponForUser (ent, selection == 4 ? false : true);
               }
               showMenu (ent, BOT_MENU_COMMANDS);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_AUTOPATH) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            const float autoDistanceValue[] = {0.0f, 100.0f, 130.0f, 160.0f, 190.0f, 220.0f, 250.0f};

            if (selection >= 1 && selection <= 7) {
               g_autoPathDistance = autoDistanceValue[selection - 1];
            }

            if (g_autoPathDistance == 0.0f) {
               engine.centerPrint ("AutoPath disabled");
            }
            else {
               engine.centerPrint ("AutoPath maximum distance set to %.2f", g_autoPathDistance);
            }
            showMenu (ent, BOT_MENU_WAYPOINT_AUTOPATH);

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_PATH) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
               waypoints.pathCreate (CONNECTION_OUTGOING);
               showMenu (ent, BOT_MENU_WAYPOINT_PATH);
               break;

            case 2:
               waypoints.pathCreate (CONNECTION_INCOMING);
               showMenu (ent, BOT_MENU_WAYPOINT_PATH);
               break;

            case 3:
               waypoints.pathCreate (CONNECTION_BOTHWAYS);
               showMenu (ent, BOT_MENU_WAYPOINT_PATH);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_DIFFICULTY) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            client->menu = BOT_MENU_PERSONALITY;

            switch (selection) {
            case 1:
               g_storeAddbotVars[0] = 0;
               break;

            case 2:
               g_storeAddbotVars[0] = 1;
               break;

            case 3:
               g_storeAddbotVars[0] = 2;
               break;

            case 4:
               g_storeAddbotVars[0] = 3;
               break;

            case 5:
               g_storeAddbotVars[0] = 4;
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }
            showMenu (ent, BOT_MENU_PERSONALITY);

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_TEAM_SELECT && fillCommand) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            if (selection < 3) {
               extern ConVar mp_limitteams;
               extern ConVar mp_autoteambalance;

               // turn off cvars if specified team
               mp_limitteams.set (0);
               mp_autoteambalance.set (0);
            }

            switch (selection) {
            case 1:
            case 2:
            case 5:
               fillServerTeam = selection;
               showMenu (ent, BOT_MENU_DIFFICULTY);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_PERSONALITY && fillCommand) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
               bots.serverFill (fillServerTeam, selection - 2, g_storeAddbotVars[0]);
               showMenu (ent, BOT_MENU_INVALID);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_TEAM_SELECT) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 5:
               g_storeAddbotVars[1] = selection;

               if (selection == 5) {
                  g_storeAddbotVars[2] = 5;
                  bots.addbot ("", g_storeAddbotVars[0], g_storeAddbotVars[3], g_storeAddbotVars[1], g_storeAddbotVars[2], true);
               }
               else {
                  if (selection == 1) {
                     showMenu (ent, BOT_MENU_TERRORIST_SELECT);
                  }
                  else {
                     showMenu (ent, BOT_MENU_CT_SELECT);
                  }
               }
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_PERSONALITY) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
               g_storeAddbotVars[3] = selection - 2;
               showMenu (ent, BOT_MENU_TEAM_SELECT);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_TERRORIST_SELECT || client->menu == BOT_MENU_CT_SELECT) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
               g_storeAddbotVars[2] = selection;
               bots.addbot ("", g_storeAddbotVars[0], g_storeAddbotVars[3], g_storeAddbotVars[1], g_storeAddbotVars[2], true);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_WEAPON_MODE) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               bots.setWeaponMode (selection);
               showMenu (ent, BOT_MENU_WEAPON_MODE);
               break;

            case 10:
               showMenu (ent, BOT_MENU_INVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_1) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.kickBot (selection - 1);
               bots.kickBotByMenu (ent, 1);
               break;

            case 9:
               bots.kickBotByMenu (ent, 2);
               break;

            case 10:
               showMenu (ent, BOT_MENU_CONTROL);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_2) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.kickBot (selection + 8 - 1);
               bots.kickBotByMenu (ent, 2);
               break;

            case 9:
               bots.kickBotByMenu (ent, 3);
               break;

            case 10:
               bots.kickBotByMenu (ent, 1);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_3) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.kickBot (selection + 16 - 1);
               bots.kickBotByMenu (ent, 3);
               break;

            case 9:
               bots.kickBotByMenu (ent, 4);
               break;

            case 10:
               bots.kickBotByMenu (ent, 2);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_4) {
            showMenu (ent, BOT_MENU_INVALID); // reset menu display

            switch (selection) {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.kickBot (selection + 24 - 1);
               bots.kickBotByMenu (ent, 4);
               break;

            case 10:
               bots.kickBotByMenu (ent, 3);
               break;
            }

            if (g_gameFlags & GAME_METAMOD) {
               RETURN_META (MRES_SUPERCEDE);
            }
            return;
         }
      }
   }

   if (!engine.isBotCmd () && (stricmp (command, "say") == 0 || stricmp (command, "say_team") == 0)) {
      Bot *bot = nullptr;

      if (strcmp (arg1, "dropme") == 0 || strcmp (arg1, "dropc4") == 0) {
         if (findNearestPlayer (reinterpret_cast <void **> (&bot), ent, 300.0f, true, true, true)) {
            bot->dropWeaponForUser (ent, isEmptyStr (strstr (arg1, "c4")) ? false : true);
         }
         return;
      }

      bool alive = isAlive (ent);
      int team = -1;

      if (strcmp (command, "say_team") == 0) {
         team = engine.getTeam (ent);
      }

      for (int i = 0; i < engine.maxClients (); i++) {
         const Client &client = g_clients[i];

         if (!(client.flags & CF_USED) || (team != -1 && team != client.team) || alive != isAlive (client.ent)) {
            continue;
         }
         Bot *target = bots.getBot (i);

         if (target != nullptr) {
            target->m_sayTextBuffer.entityIndex = engine.indexOfEntity (ent);

            if (isEmptyStr (g_engfuncs.pfnCmd_Args ())) {
               continue;
            }
            target->m_sayTextBuffer.sayText = g_engfuncs.pfnCmd_Args ();
            target->m_sayTextBuffer.timeNextChat = engine.timebase () + target->m_sayTextBuffer.chatDelay;
         }
      }
   }

   int clientIndex = engine.indexOfEntity (ent) - 1;
   const Client &radioTarget = g_clients[clientIndex];

   // check if this player alive, and issue something
   if ((radioTarget.flags & CF_ALIVE) && g_radioSelect[clientIndex] != 0 && strncmp (command, "menuselect", 10) == 0) {
      int radioCommand = atoi (arg1);

      if (radioCommand != 0) {
         radioCommand += 10 * (g_radioSelect[clientIndex] - 1);

         if (radioCommand != Radio_Affirmative && radioCommand != Radio_Negative && radioCommand != Radio_ReportingIn) {
            for (int i = 0; i < engine.maxClients (); i++) {
               Bot *bot = bots.getBot (i);

               // validate bot
               if (bot != nullptr && bot->m_team == radioTarget.team && ent != bot->ent () && bot->m_radioOrder == 0) {
                  bot->m_radioOrder = radioCommand;
                  bot->m_radioEntity = ent;
               }
            }
         }
         g_lastRadioTime[radioTarget.team] = engine.timebase ();
      }
      g_radioSelect[clientIndex] = 0;
   }
   else if (strncmp (command, "radio", 5) == 0) {
      g_radioSelect[clientIndex] = atoi (&command[5]);
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnClientCommand (ent);
}

void ServerActivate (edict_t *pentEdictList, int edictCount, int clientMax) {
   // this function is called when the server has fully loaded and is about to manifest itself
   // on the network as such. Since a mapchange is actually a server shutdown followed by a
   // restart, this function is also called when a new map is being loaded. Hence it's the
   // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
   // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
   // Once this function has been called, the server can be considered as "running".

   cleanupGarbage ();
   processBotConfigs (false); // initialize all config files

   // do a level initialization
   engine.levelInitialize ();

   // do level initialization stuff here...
   waypoints.init ();
   waypoints.load ();

   // execute main config
   processBotConfigs (true);

   if (File::exists (format ("%s/maps/%s_yapb.cfg", engine.getModName (), engine.getMapName ()))) {
      engine.execCmd ("exec maps/%s_yapb.cfg", engine.getMapName ());
      engine.print ("Executing Map-Specific config file");
   }
   bots.initQuota ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnServerActivate (pentEdictList, edictCount, clientMax);

   waypoints.rebuildVisibility ();
}

void ServerDeactivate (void) {
   // this function is called when the server is shutting down. A particular note about map
   // changes: changing the map means shutting down the server and starting a new one. Of course
   // this process is transparent to the user, but either in single player when the hero reaches
   // a new level and in multiplayer when it's time for a map change, be aware that what happens
   // is that the server actually shuts down and restarts with a new map. Hence we can use this
   // function to free and deinit anything which is map-specific, for example we free the memory
   // space we m'allocated for our BSP data, since a new map means new BSP data to interpret. In
   // any case, when the new map will be booting, ServerActivate() will be called, so we'll do
   // the loading of new bots and the new BSP data parsing there.

   // save collected experience on shutdown
   waypoints.saveExperience ();
   waypoints.saveVisibility ();

   // destroy global killer entity
   bots.destroyKillerEntity ();

   // set state to unprecached
   engine.setUnprecached ();

   cleanupGarbage ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnServerDeactivate ();
}

void StartFrame (void) {
   // this function starts a video frame. It is called once per video frame by the engine. If
   // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
   // placing a hook on it, we have a good place to do things that should be done continuously
   // during the game, for example making the bots think (yes, because no Think() function exists
   // for the bots by the MOD side, remember). Also here we have control on the bot population,
   // for example if a new player joins the server, we should disconnect a bot, and if the
   // player population decreases, we should fill the server with other bots.

   // run periodic update of bot states
   bots.frame ();

   // record some stats of all players on the server
   for (int i = 0; i < engine.maxClients (); i++) {
      edict_t *player = engine.entityOfIndex (i + 1);
      Client &client = g_clients[i];

      if (!engine.isNullEntity (player) && (player->v.flags & FL_CLIENT)) {
         client.ent = player;
         client.flags |= CF_USED;

         if (isAlive (player)) {
            client.flags |= CF_ALIVE;
         }
         else {
            client.flags &= ~CF_ALIVE;
         }

         if (client.flags & CF_ALIVE) {
            // keep the clipping mode enabled, or it can be turned off after new round has started
            if (g_hostEntity == player && g_editNoclip) {
               g_hostEntity->v.movetype = MOVETYPE_NOCLIP;
            }
            client.origin = player->v.origin;
            simulateSoundUpdates (i);
         }
      }
      else {
         client.flags &= ~(CF_USED | CF_ALIVE);
         client.ent = nullptr;
      }
   }

   if (g_waypointOn && !engine.isDedicated () && !engine.isNullEntity (g_hostEntity)) {
      waypoints.frame ();
   }
   bots.updateDeathMsgState (false);

   if (g_timePerSecondUpdate < engine.timebase ()) {
      checkWelcome ();

      for (int i = 0; i < engine.maxClients (); i++) {
         edict_t *player = engine.entityOfIndex (i + 1);

         // code below is executed only on dedicated server
         if (engine.isDedicated () && !engine.isNullEntity (player) && (player->v.flags & FL_CLIENT) && !(player->v.flags & FL_FAKECLIENT)) {
            Client &client = g_clients[i];

            if (client.flags & CF_ADMIN) {
               if (isEmptyStr (yb_password_key.str ()) && isEmptyStr (yb_password.str ())) {
                  client.flags &= ~CF_ADMIN;
               }
               else if (!!strcmp (yb_password.str (), g_engfuncs.pfnInfoKeyValue (g_engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ())))) {
                  client.flags &= ~CF_ADMIN;
                  engine.print ("Player %s had lost remote access to yapb.", STRING (player->v.netname));
               }
            }
            else if (!(client.flags & CF_ADMIN) && !isEmptyStr (yb_password_key.str ()) && !isEmptyStr (yb_password.str ())) {
               if (strcmp (yb_password.str (), g_engfuncs.pfnInfoKeyValue (g_engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ()))) == 0) {
                  client.flags |= CF_ADMIN;
                  engine.print ("Player %s had gained full remote access to yapb.", STRING (player->v.netname));
               }
            }
         }
      }
      bots.calculatePingOffsets ();

      if (g_gameFlags & GAME_METAMOD) {
         static auto dmActive = g_engfuncs.pfnCVarGetPointer ("csdm_active");
         static auto freeForAll = g_engfuncs.pfnCVarGetPointer ("mp_freeforall");

         if (dmActive && freeForAll) {
            if (dmActive->value > 0.0f) {
               g_gameFlags |= GAME_CSDM;

               if (freeForAll->value > 0.0f) {
                  g_gameFlags |= GAME_CSDM_FFA;
               }
               else if (g_gameFlags & GAME_CSDM_FFA) {
                  g_gameFlags &= ~GAME_CSDM_FFA;
               }
            }
            else if (g_gameFlags & GAME_CSDM) {
               g_gameFlags &= ~GAME_CSDM;
            }
         }
      }
      g_timePerSecondUpdate = engine.timebase () + 1.0f;
   }

   if (bots.getBotCount () > 0) {
      // keep track of grenades on map
      bots.updateActiveGrenade ();

      // keep track of intresting entities
      bots.updateIntrestingEntities ();
   }

   // keep bot number up to date
   bots.maintainQuota ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_functionTable.pfnStartFrame ();

   // **** AI EXECUTION STARTS ****
   bots.framePeriodic ();
   // **** AI EXECUTION FINISH ****
}

void StartFrame_Post (void) {
   // this function starts a video frame. It is called once per video frame by the engine. If
   // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
   // placing a hook on it, we have a good place to do things that should be done continuously
   // during the game, for example making the bots think (yes, because no Think() function exists
   // for the bots by the MOD side, remember).  Post version called only by metamod.

   // **** AI EXECUTION STARTS ****
   bots.framePeriodic ();
   // **** AI EXECUTION FINISH ****

   RETURN_META (MRES_IGNORED);
}

int Spawn_Post (edict_t *ent) {
   // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
   // world, in other words to 'display') the entity pointed to by ent in the game. The
   // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
   // and any MOD is supposed to implement one for each of its entities. Post version called
   // only by metamod.

   // solves the bots unable to see through certain types of glass bug.
   if (ent->v.rendermode == kRenderTransTexture) {
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents
   }
   RETURN_META_VALUE (MRES_IGNORED, 0);
}

void ServerActivate_Post (edict_t *, int, int) {
   // this function is called when the server has fully loaded and is about to manifest itself
   // on the network as such. Since a mapchange is actually a server shutdown followed by a
   // restart, this function is also called when a new map is being loaded. Hence it's the
   // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
   // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
   // Once this function has been called, the server can be considered as "running". Post version
   // called only by metamod.

   waypoints.rebuildVisibility ();

   RETURN_META (MRES_IGNORED);
}

void pfnChangeLevel (char *s1, char *s2) {
   // the purpose of this function is to ask the engine to shutdown the server and restart a
   // new one running the map whose name is s1. It is used ONLY IN SINGLE PLAYER MODE and is
   // transparent to the user, because it saves the player state and equipment and restores it
   // back in the new level. The "changelevel trigger point" in the old level is linked to the
   // new level's spawn point using the s2 string, which is formatted as follows: "trigger_name
   // to spawnpoint_name", without spaces (for example, "tr_1atotr_2lm" would tell the engine
   // the player has reached the trigger point "tr_1a" and has to spawn in the next level on the
   // spawn point named "tr_2lm".

   // save collected experience on map change
   waypoints.saveExperience ();
   waypoints.saveVisibility ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnChangeLevel (s1, s2);
}

edict_t *pfnFindEntityByString (edict_t *edictStartSearchAfter, const char *field, const char *value) {
   // round starts in counter-strike 1.5
   if ((g_gameFlags & GAME_LEGACY) && strcmp (value, "info_map_parameters") == 0) {
      initRound ();
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
   }
   return g_engfuncs.pfnFindEntityByString (edictStartSearchAfter, field, value);
}

void pfnEmitSound (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch) {
   // this function tells the engine that the entity pointed to by "entity", is emitting a sound
   // which fileName is "sample", at level "channel" (CHAN_VOICE, etc...), with "volume" as
   // loudness multiplicator (normal volume VOL_NORM is 1.0), with a pitch of "pitch" (normal
   // pitch PITCH_NORM is 100.0), and that this sound has to be attenuated by distance in air
   // according to the value of "attenuation" (normal attenuation ATTN_NORM is 0.8 ; ATTN_NONE
   // means no attenuation with distance). Optionally flags "fFlags" can be passed, which I don't
   // know the heck of the purpose. After we tell the engine to emit the sound, we have to call
   // SoundAttachToThreat() to bring the sound to the ears of the bots. Since bots have no client DLL
   // to handle this for them, such a job has to be done manually.

   attachSoundsToClients (entity, sample, volume);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnEmitSound (entity, channel, sample, volume, attenuation, flags, pitch);
}

void pfnClientCommand (edict_t *ent, char const *format, ...) {
   // this function forces the client whose player entity is ent to issue a client command.
   // How it works is that clients all have a g_xgv global string in their client DLL that
   // stores the command string; if ever that string is filled with characters, the client DLL
   // sends it to the engine as a command to be executed. When the engine has executed that
   // command, this g_xgv string is reset to zero. Here is somehow a curious implementation of
   // ClientCommand: the engine sets the command it wants the client to issue in his g_xgv, then
   // the client DLL sends it back to the engine, the engine receives it then executes the
   // command therein. Don't ask me why we need all this complicated crap. Anyhow since bots have
   // no client DLL, be certain never to call this function upon a bot entity, else it will just
   // make the server crash. Since hordes of uncautious, not to say stupid, programmers don't
   // even imagine some players on their servers could be bots, this check is performed less than
   // sometimes actually by their side, that's why we strongly recommend to check it here too. In
   // case it's a bot asking for a client command, we handle it like we do for bot commands

   va_list ap;
   char buffer[MAX_PRINT_BUFFER];

   va_start (ap, format);
   _vsnprintf (buffer, cr::bufsize (buffer), format, ap);
   va_end (ap);

   if (ent && (ent->v.flags & (FL_FAKECLIENT | FL_DORMANT))) {
      if (bots.getBot (ent)) {
         engine.execBotCmd (ent, buffer);
      }

      if (g_gameFlags & GAME_METAMOD) {
         RETURN_META (MRES_SUPERCEDE); // prevent bots to be forced to issue client commands
      }
      return;
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnClientCommand (ent, buffer);
}

void pfnMessageBegin (int msgDest, int msgType, const float *origin, edict_t *ed) {
   // this function called each time a message is about to sent.
   
   // store the message type in our own variables, since the GET_USER_MSG_ID () will just do a lot of strcmp()'s...
   if ((g_gameFlags & GAME_METAMOD) && engine.getMessageId (NETMSG_MONEY) == -1) {
      engine.setMessageId (NETMSG_VGUI, GET_USER_MSG_ID (PLID, "VGUIMenu", nullptr));
      engine.setMessageId (NETMSG_SHOWMENU, GET_USER_MSG_ID (PLID, "ShowMenu", nullptr));
      engine.setMessageId (NETMSG_WEAPONLIST, GET_USER_MSG_ID (PLID, "WeaponList", nullptr));
      engine.setMessageId (NETMSG_CURWEAPON, GET_USER_MSG_ID (PLID, "CurWeapon", nullptr));
      engine.setMessageId (NETMSG_AMMOX, GET_USER_MSG_ID (PLID, "AmmoX", nullptr));
      engine.setMessageId (NETMSG_AMMOPICKUP, GET_USER_MSG_ID (PLID, "AmmoPickup", nullptr));
      engine.setMessageId (NETMSG_DAMAGE, GET_USER_MSG_ID (PLID, "Damage", nullptr));
      engine.setMessageId (NETMSG_MONEY, GET_USER_MSG_ID (PLID, "Money", nullptr));
      engine.setMessageId (NETMSG_STATUSICON, GET_USER_MSG_ID (PLID, "StatusIcon", nullptr));
      engine.setMessageId (NETMSG_DEATH, GET_USER_MSG_ID (PLID, "DeathMsg", nullptr));
      engine.setMessageId (NETMSG_SCREENFADE, GET_USER_MSG_ID (PLID, "ScreenFade", nullptr));
      engine.setMessageId (NETMSG_HLTV, GET_USER_MSG_ID (PLID, "HLTV", nullptr));
      engine.setMessageId (NETMSG_TEXTMSG, GET_USER_MSG_ID (PLID, "TextMsg", nullptr));
      engine.setMessageId (NETMSG_TEAMINFO, GET_USER_MSG_ID (PLID, "TeamInfo", nullptr));
      engine.setMessageId (NETMSG_BARTIME, GET_USER_MSG_ID (PLID, "BarTime", nullptr));
      engine.setMessageId (NETMSG_SENDAUDIO, GET_USER_MSG_ID (PLID, "SendAudio", nullptr));
      engine.setMessageId (NETMSG_SAYTEXT, GET_USER_MSG_ID (PLID, "SayText", nullptr));

      if (g_gameFlags & GAME_SUPPORT_BOT_VOICE) {
         engine.setMessageId (NETMSG_BOTVOICE, GET_USER_MSG_ID (PLID, "BotVoice", nullptr));
      }
   }
   engine.resetMessages ();

   if ((!(g_gameFlags & GAME_LEGACY) || (g_gameFlags & GAME_XASH_ENGINE)) && msgDest == MSG_SPEC && msgType == engine.getMessageId (NETMSG_HLTV)) {
      engine.setCurrentMessageId (NETMSG_HLTV);
   }
   engine.captureMessage (msgType, NETMSG_WEAPONLIST);

   if (!engine.isNullEntity (ed)) {
      int index = bots.index (ed);

      // is this message for a bot?
      if (index != -1 && !(ed->v.flags & FL_DORMANT)) {
         engine.setCurrentMessageOwner (index);

         // message handling is done in usermsg.cpp
         engine.captureMessage (msgType, NETMSG_VGUI);
         engine.captureMessage (msgType, NETMSG_CURWEAPON);
         engine.captureMessage (msgType, NETMSG_AMMOX);
         engine.captureMessage (msgType, NETMSG_AMMOPICKUP);
         engine.captureMessage (msgType, NETMSG_DAMAGE);
         engine.captureMessage (msgType, NETMSG_MONEY);
         engine.captureMessage (msgType, NETMSG_STATUSICON);
         engine.captureMessage (msgType, NETMSG_SCREENFADE);
         engine.captureMessage (msgType, NETMSG_BARTIME);
         engine.captureMessage (msgType, NETMSG_TEXTMSG);
         engine.captureMessage (msgType, NETMSG_SHOWMENU);
      }
   }
   else if (msgDest == MSG_ALL) {
      engine.captureMessage (msgType, NETMSG_TEAMINFO);
      engine.captureMessage (msgType, NETMSG_DEATH);
      engine.captureMessage (msgType, NETMSG_TEXTMSG);

      if (msgType == SVC_INTERMISSION) {
         for (int i = 0; i < engine.maxClients (); i++) {
            Bot *bot = bots.getBot (i);

            if (bot != nullptr) {
               bot->m_notKilled = false;
            }
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnMessageBegin (msgDest, msgType, origin, ed);
}

void pfnMessageEnd (void) {
   engine.resetMessages ();

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnMessageEnd ();

   // send latency fix
   bots.sendDeathMsgFix ();
}

void pfnMessageEnd_Post (void) {
   // send latency fix
   bots.sendDeathMsgFix ();

   RETURN_META (MRES_IGNORED);
}

void pfnWriteByte (int value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteByte (value);
}

void pfnWriteChar (int value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteChar (value);
}

void pfnWriteShort (int value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteShort (value);
}

void pfnWriteLong (int value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteLong (value);
}

void pfnWriteAngle (float value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteAngle (value);
}

void pfnWriteCoord (float value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteCoord (value);
}

void pfnWriteString (const char *sz) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)sz);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteString (sz);
}

void pfnWriteEntity (int value) {
   // if this message is for a bot, call the client message function...
   engine.processMessages ((void *)&value);

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnWriteEntity (value);
}

int pfnCmd_Argc (void) {
   // this function returns the number of arguments the current client command string has. Since
   // bots have no client DLL and we may want a bot to execute a client command, we had to
   // implement a g_xgv string in the bot DLL for holding the bots' commands, and also keep
   // track of the argument count. Hence this hook not to let the engine ask an unexistent client
   // DLL for a command we are holding here. Of course, real clients commands are still retrieved
   // the normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.isBotCmd ()) {
      if (g_gameFlags & GAME_METAMOD) {
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.botArgc ());
      }
      return engine.botArgc (); // if so, then return the argument count we know
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
   }
   return g_engfuncs.pfnCmd_Argc (); // ask the engine how many arguments there are
}

const char *pfnCmd_Args (void) {
   // this function returns a pointer to the whole current client command string. Since bots
   // have no client DLL and we may want a bot to execute a client command, we had to implement
   // a g_xgv string in the bot DLL for holding the bots' commands, and also keep track of the
   // argument count. Hence this hook not to let the engine ask an unexistent client DLL for a
   // command we are holding here. Of course, real clients commands are still retrieved the
   // normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.isBotCmd ()) {
      if (g_gameFlags & GAME_METAMOD) {
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.botArgs ());
      }
      return engine.botArgs (); // else return the whole bot client command string we know
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, nullptr);
   }
   return g_engfuncs.pfnCmd_Args (); // ask the client command string to the engine
}

const char *pfnCmd_Argv (int argc) {
   // this function returns a pointer to a certain argument of the current client command. Since
   // bots have no client DLL and we may want a bot to execute a client command, we had to
   // implement a g_xgv string in the bot DLL for holding the bots' commands, and also keep
   // track of the argument count. Hence this hook not to let the engine ask an unexistent client
   // DLL for a command we are holding here. Of course, real clients commands are still retrieved
   // the normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.isBotCmd ()) {
      if (g_gameFlags & GAME_METAMOD) {
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.botArgv (argc));
      }
      return engine.botArgv (argc); // if so, then return the wanted argument we know
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, nullptr);
   }
   return g_engfuncs.pfnCmd_Argv (argc); // ask the argument number "argc" to the engine
}

void pfnClientPrintf (edict_t *ent, PRINT_TYPE printType, const char *message) {
   // this function prints the text message string pointed to by message by the client side of
   // the client entity pointed to by ent, in a manner depending of printType (print_console,
   // print_center or print_chat). Be certain never to try to feed a bot with this function,
   // as it will crash your server. Why would you, anyway ? bots have no client DLL as far as
   // we know, right ? But since stupidity rules this world, we do a preventive check :)

   if (isFakeClient (ent)) {
      if (g_gameFlags & GAME_METAMOD) {
         RETURN_META (MRES_SUPERCEDE);
      }
      return;
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnClientPrintf (ent, printType, message);
}

void pfnSetClientMaxspeed (const edict_t *ent, float newMaxspeed) {
   Bot *bot = bots.getBot (const_cast <edict_t *> (ent));

   // check wether it's not a bot
   if (bot != nullptr) {
      bot->pev->maxspeed = newMaxspeed;
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnSetClientMaxspeed (ent, newMaxspeed);
}

int pfnRegUserMsg (const char *name, int size) {
   // this function registers a "user message" by the engine side. User messages are network
   // messages the game DLL asks the engine to send to clients. Since many MODs have completely
   // different client features (Counter-Strike has a radar and a timer, for example), network
   // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
   // "Hey, you have to know that I use a network message whose name is pszName and it is size
   // packets long". The engine books it, and returns the ID number under which he recorded that
   // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
   // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
   // know what to do, only for non-metamod version

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META_VALUE (MRES_IGNORED, 0);
   }
   int message = g_engfuncs.pfnRegUserMsg (name, size);

   if (strcmp (name, "VGUIMenu") == 0) {
      engine.setMessageId (NETMSG_VGUI, message);
   }
   else if (strcmp (name, "ShowMenu") == 0) {
      engine.setMessageId (NETMSG_SHOWMENU, message);
   }
   else if (strcmp (name, "WeaponList") == 0) {
      engine.setMessageId (NETMSG_WEAPONLIST, message);
   }
   else if (strcmp (name, "CurWeapon") == 0) {
      engine.setMessageId (NETMSG_CURWEAPON, message);
   }
   else if (strcmp (name, "AmmoX") == 0) {
      engine.setMessageId (NETMSG_AMMOX, message);
   }
   else if (strcmp (name, "AmmoPickup") == 0) {
      engine.setMessageId (NETMSG_AMMOPICKUP, message);
   }
   else if (strcmp (name, "Damage") == 0) {
      engine.setMessageId (NETMSG_DAMAGE, message);
   }
   else if (strcmp (name, "Money") == 0) {
      engine.setMessageId (NETMSG_MONEY, message);
   }
   else if (strcmp (name, "StatusIcon") == 0) {
      engine.setMessageId (NETMSG_STATUSICON, message);
   }
   else if (strcmp (name, "DeathMsg") == 0) {
      engine.setMessageId (NETMSG_DEATH, message);
   }
   else if (strcmp (name, "ScreenFade") == 0) {
      engine.setMessageId (NETMSG_SCREENFADE, message);
   }
   else if (strcmp (name, "HLTV") == 0) {
      engine.setMessageId (NETMSG_HLTV, message);
   }
   else if (strcmp (name, "TextMsg") == 0) {
      engine.setMessageId (NETMSG_TEXTMSG, message);
   }
   else if (strcmp (name, "TeamInfo") == 0) {
      engine.setMessageId (NETMSG_TEAMINFO, message);
   }
   else if (strcmp (name, "BarTime") == 0) {
      engine.setMessageId (NETMSG_BARTIME, message);
   }
   else if (strcmp (name, "SendAudio") == 0) {
      engine.setMessageId (NETMSG_SENDAUDIO, message);
   }
   else if (strcmp (name, "SayText") == 0) {
      engine.setMessageId (NETMSG_SAYTEXT, message);
   }
   else if (strcmp (name, "BotVoice") == 0) {
      engine.setMessageId (NETMSG_BOTVOICE, message);
   }
   return message;
}

void pfnAlertMessage (ALERT_TYPE alertType, char *format, ...) {
   va_list ap;
   char buffer[1024];

   va_start (ap, format);
   vsnprintf (buffer, cr::bufsize (buffer), format, ap);
   va_end (ap);

   if ((g_mapFlags & MAP_DE) && g_bombPlanted && strstr (buffer, "_Defuse_") != nullptr) {
      // notify all terrorists that CT is starting bomb defusing
      for (int i = 0; i < engine.maxClients (); i++) {
         Bot *bot = bots.getBot (i);

         if (bot != nullptr && bot->m_team == TEAM_TERRORIST && bot->m_notKilled) {
            bot->clearSearchNodes ();

            bot->m_position = waypoints.getBombPos ();
            bot->startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD) {
      RETURN_META (MRES_IGNORED);
   }
   g_engfuncs.pfnAlertMessage (alertType, buffer);
}

typedef void (*entity_func_t) (entvars_t *);
gamedll_funcs_t gameDLLFunc;

SHARED_LIBRARAY_EXPORT int GetEntityAPI2 (gamefuncs_t *functionTable, int *) {
   // this function is called right after FuncPointers_t() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code).

   memset (functionTable, 0, sizeof (gamefuncs_t));

   if (!(g_gameFlags & GAME_METAMOD)) {
      auto api_GetEntityAPI = g_gameLib->resolve<int (*) (gamefuncs_t *, int)> ("GetEntityAPI");

      // pass other DLLs engine callbacks to function table...
      if (api_GetEntityAPI (&g_functionTable, INTERFACE_VERSION) == 0) {
         logEntry (true, LL_FATAL, "GetEntityAPI2: ERROR - Not Initialized.");
         return FALSE; // error initializing function table!!!
      }
      gameDLLFunc.dllapi_table = &g_functionTable;
      gpGamedllFuncs = &gameDLLFunc;

      memcpy (functionTable, &g_functionTable, sizeof (gamefuncs_t));
   }

   functionTable->pfnGameInit = GameDLLInit;
   functionTable->pfnSpawn = Spawn;
   functionTable->pfnTouch = Touch;
   functionTable->pfnClientConnect = ClientConnect;
   functionTable->pfnClientDisconnect = ClientDisconnect;
   functionTable->pfnClientUserInfoChanged = ClientUserInfoChanged;
   functionTable->pfnClientCommand = ClientCommand;
   functionTable->pfnServerActivate = ServerActivate;
   functionTable->pfnServerDeactivate = ServerDeactivate;
   functionTable->pfnStartFrame = StartFrame;
   functionTable->pfnUpdateClientData = UpdateClientData;

   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEntityAPI2_Post (gamefuncs_t *functionTable, int *) {
   // this function is called right after FuncPointers_t() by the engine in the game DLL (or
   // what it BELIEVES to be the game DLL), in order to copy the list of MOD functions that can
   // be called by the engine, into a memory block pointed to by the functionTable pointer
   // that is passed into this function (explanation comes straight from botman). This allows
   // the Half-Life engine to call these MOD DLL functions when it needs to spawn an entity,
   // connect or disconnect a player, call Think() functions, Touch() functions, or Use()
   // functions, etc. The bot DLL passes its OWN list of these functions back to the Half-Life
   // engine, and then calls the MOD DLL's version of GetEntityAPI to get the REAL gamedll
   // functions this time (to use in the bot code). Post version, called only by metamod.

   memset (functionTable, 0, sizeof (gamefuncs_t));

   functionTable->pfnSpawn = Spawn_Post;
   functionTable->pfnStartFrame = StartFrame_Post;
   functionTable->pfnServerActivate = ServerActivate_Post;

   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetNewDLLFunctions (newgamefuncs_t *functionTable, int *interfaceVersion) {
   // it appears that an extra function table has been added in the engine to gamedll interface
   // since the date where the first enginefuncs table standard was frozen. These ones are
   // facultative and we don't hook them, but since some MODs might be featuring it, we have to
   // pass them too, else the DLL interfacing wouldn't be complete and the game possibly wouldn't
   // run properly.

   auto api_GetNewDLLFunctions = g_gameLib->resolve<int (*) (newgamefuncs_t *, int *)> ("GetNewDLLFunctions");

   if (api_GetNewDLLFunctions == nullptr) {
      return FALSE;
   }

   if (!api_GetNewDLLFunctions (functionTable, interfaceVersion)) {
      logEntry (true, LL_FATAL, "GetNewDLLFunctions: ERROR - Not Initialized.");
      return FALSE;
   }

   gameDLLFunc.newapi_table = functionTable;
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEngineFunctions (enginefuncs_t *functionTable, int *) {
   if (g_gameFlags & GAME_METAMOD) {
      memset (functionTable, 0, sizeof (enginefuncs_t));
   }

   functionTable->pfnChangeLevel = pfnChangeLevel;
   functionTable->pfnFindEntityByString = pfnFindEntityByString;
   functionTable->pfnEmitSound = pfnEmitSound;
   functionTable->pfnClientCommand = pfnClientCommand;
   functionTable->pfnMessageBegin = pfnMessageBegin;
   functionTable->pfnMessageEnd = pfnMessageEnd;
   functionTable->pfnWriteByte = pfnWriteByte;
   functionTable->pfnWriteChar = pfnWriteChar;
   functionTable->pfnWriteShort = pfnWriteShort;
   functionTable->pfnWriteLong = pfnWriteLong;
   functionTable->pfnWriteAngle = pfnWriteAngle;
   functionTable->pfnWriteCoord = pfnWriteCoord;
   functionTable->pfnWriteString = pfnWriteString;
   functionTable->pfnWriteEntity = pfnWriteEntity;
   functionTable->pfnRegUserMsg = pfnRegUserMsg;
   functionTable->pfnClientPrintf = pfnClientPrintf;
   functionTable->pfnCmd_Args = pfnCmd_Args;
   functionTable->pfnCmd_Argv = pfnCmd_Argv;
   functionTable->pfnCmd_Argc = pfnCmd_Argc;
   functionTable->pfnSetClientMaxspeed = pfnSetClientMaxspeed;
   functionTable->pfnAlertMessage = pfnAlertMessage;
   
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEngineFunctions_Post (enginefuncs_t *functionTable, int *) {

   memset (functionTable, 0, sizeof (enginefuncs_t));
   functionTable->pfnMessageEnd = pfnMessageEnd_Post;

   return TRUE;
}

SHARED_LIBRARAY_EXPORT int Server_GetBlendingInterface (int version, void **ppinterface, void *pstudio, float (*rotationmatrix)[3][4], float (*bonetransform)[128][3][4]) {
   // this function synchronizes the studio model animation blending interface (i.e, what parts
   // of the body move, which bones, which hitboxes and how) between the server and the game DLL.
   // some MODs can be using a different hitbox scheme than the standard one.

   auto api_GetBlendingInterface = g_gameLib->resolve<int (*) (int, void **, void *, float(*)[3][4], float(*)[128][3][4])> ("Server_GetBlendingInterface");

   if (api_GetBlendingInterface == nullptr) {
      return FALSE;
   }
   return api_GetBlendingInterface (version, ppinterface, pstudio, rotationmatrix, bonetransform);
}

SHARED_LIBRARAY_EXPORT int Meta_Query (char *, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs) {
   // this function is the first function ever called by metamod in the plugin DLL. Its purpose
   // is for metamod to retrieve basic information about the plugin, such as its meta-interface
   // version, for ensuring compatibility with the current version of the running metamod.

   gpMetaUtilFuncs = pMetaUtilFuncs;
   *pPlugInfo = &Plugin_info;

   return TRUE; // tell metamod this plugin looks safe
}

SHARED_LIBRARAY_EXPORT int Meta_Attach (PLUG_LOADTIME, metamod_funcs_t *functionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs) {
   // this function is called when metamod attempts to load the plugin. Since it's the place
   // where we can tell if the plugin will be allowed to run or not, we wait until here to make
   // our initialization stuff, like registering CVARs and dedicated server commands.

   // metamod engine & dllapi function tables
   static metamod_funcs_t metamodFunctionTable = {
      nullptr, // pfnGetEntityAPI ()
      nullptr, // pfnGetEntityAPI_Post ()
      GetEntityAPI2, // pfnGetEntityAPI2 ()
      GetEntityAPI2_Post, // pfnGetEntityAPI2_Post ()
      nullptr, // pfnGetNewDLLFunctions ()
      nullptr, // pfnGetNewDLLFunctions_Post ()
      GetEngineFunctions, // pfnGetEngineFunctions ()
      GetEngineFunctions_Post, // pfnGetEngineFunctions_Post ()
   };

   // keep track of the pointers to engine function tables metamod gives us
   gpMetaGlobals = pMGlobals;
   memcpy (functionTable, &metamodFunctionTable, sizeof (metamod_funcs_t));
   gpGamedllFuncs = pGamedllFuncs;

   return TRUE; // returning true enables metamod to attach this plugin
}

SHARED_LIBRARAY_EXPORT int Meta_Detach (PLUG_LOADTIME, PL_UNLOAD_REASON) {
   // this function is called when metamod unloads the plugin. A basic check is made in order
   // to prevent unloading the plugin if its processing should not be interrupted.

   bots.kickEveryone (true); // kick all bots off this server
   cleanupGarbage ();

   return TRUE;
}

SHARED_LIBRARAY_EXPORT void Meta_Init (void) {
   // this function is called by metamod, before any other interface functions. Purpose of this
   // function to give plugin a chance to determine is plugin running under metamod or not.

   g_gameFlags |= GAME_METAMOD;
}

Library *LoadCSBinary (void) {
   const char *modname = engine.getModName ();

   if (!modname)
      return nullptr;

#if defined(PLATFORM_WIN32)
   const char *libs[] = {"mp.dll", "cs.dll"};
#elif defined(PLATFORM_LINUX)
   const char *libs[] = {"cs.so", "cs_i386.so"};
#elif defined(PLATFORM_OSX)
   const char *libs[] = {"cs.dylib"};
#endif

   // search the libraries inside game dlls directory
   for (size_t i = 0; i < cr::arrsize (libs); i++) {
      auto *path = format ("%s/dlls/%s", modname, libs[i]);

      // if we can't read file, skip it
      if (!File::exists (path)) {
         continue;
      }

      // special case, czero is always detected first, as it's has custom directory
      if (strcmp (modname, "czero") == 0) {
         g_gameFlags |= (GAME_CZERO | GAME_SUPPORT_BOT_VOICE | GAME_SUPPORT_SVC_PINGS);

         if (g_gameFlags & GAME_METAMOD) {
            return nullptr;
         }
         return new Library (path);
      }
      else {
         Library *game = new Library (path);

         // try to load gamedll
         if (!game->isValid ()) {
            logEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", libs[i], modname);

            delete game;
            return nullptr;
         }
         // detect if we're running modern game
         auto entity = game->resolve<entity_func_t> ("weapon_famas");

         // detect xash engine
         if (g_engfuncs.pfnCVarGetPointer ("build") != nullptr) {
            g_gameFlags |= (GAME_LEGACY | GAME_XASH_ENGINE);

            if (entity != nullptr) {
               g_gameFlags |= GAME_SUPPORT_BOT_VOICE;
            }

            if (g_gameFlags & GAME_METAMOD) {
               delete game;
               return nullptr;
            }
            return game;
         }

         if (entity != nullptr) {
            g_gameFlags |= (GAME_CSTRIKE16 | GAME_SUPPORT_BOT_VOICE | GAME_SUPPORT_SVC_PINGS);
         }
         else {
            g_gameFlags |= GAME_LEGACY;
         }

         if (g_gameFlags & GAME_METAMOD) {
            delete game;
            return nullptr;
         }
         return game;
      }
   }
   return nullptr;
}

DLL_GIVEFNPTRSTODLL GiveFnptrsToDll (enginefuncs_t *functionTable, globalvars_t *pGlobals) {
   // this is the very first function that is called in the game DLL by the engine. Its purpose
   // is to set the functions interfacing up, by exchanging the functionTable function list
   // along with a pointer to the engine's global variables structure pGlobals, with the game
   // DLL. We can there decide if we want to load the normal game DLL just behind our bot DLL,
   // or any other game DLL that is present, such as Will Day's metamod. Also, since there is
   // a known bug on Win32 platforms that prevent hook DLLs (such as our bot DLL) to be used in
   // single player games (because they don't export all the stuff they should), we may need to
   // build our own array of exported symbols from the actual game DLL in order to use it as
   // such if necessary. Nothing really bot-related is done in this function. The actual bot
   // initialization stuff will be done later, when we'll be certain to have a multilayer game.

   // get the engine functions from the engine...
   memcpy (&g_engfuncs, functionTable, sizeof (enginefuncs_t));
   g_pGlobals = pGlobals;

   // register our cvars
   engine.pushRegStackToEngine ();

   // ensure we're have all needed directories
   {
      const char *mod = engine.getModName ();

      // create the needed paths
      File::pathCreate (const_cast <char *> (format ("%s/addons/yapb/conf/lang", mod)));
      File::pathCreate (const_cast <char *> (format ("%s/addons/yapb/data/learned", mod)));
   }

#ifdef PLATFORM_ANDROID
   g_gameFlags |= (GAME_LEGACY | GAME_XASH_ENGINE | GAME_MOBILITY | GAME_SUPPORT_BOT_VOICE);

   if (g_gameFlags & GAME_METAMOD) {
      return; // we should stop the attempt for loading the real gamedll, since metamod handle this for us
   }

#ifdef LOAD_HARDFP
   const char *serverDLL = "libserver_hardfp.so";
#else
   const char *serverDLL = "libserver.so";
#endif

   char gameDLLName[256];
   snprintf (gameDLLName, cr::bufsize (gameDLLName), "%s/%s", getenv ("XASH3D_GAMELIBDIR"), serverDLL);

   g_gameLib = new Library (gameDLLName);

   if (!g_gameLib->IsLoaded ()) {
      logEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", gameDLLName, engine.GetModName ());
      delete g_gameLib;
   }
#else
   g_gameLib = LoadCSBinary ();
   {
      if (g_gameLib == nullptr && !(g_gameFlags & GAME_METAMOD)) {
         logEntry (true, LL_FATAL | LL_IGNORE, "Mod that you has started, not supported by this bot (gamedir: %s)", engine.getModName ());
         return;
      }

      // print game detection info
      String gameVersionStr;

      if (g_gameFlags & GAME_LEGACY) {
         gameVersionStr.assign ("Legacy");
      }
      else if (g_gameFlags & GAME_CZERO) {
         gameVersionStr.assign ("Condition Zero");
      }
      else if (g_gameFlags & GAME_CSTRIKE16) {
         gameVersionStr.assign ("v1.6");
      }
      if (g_gameFlags & GAME_XASH_ENGINE) {
         gameVersionStr.append (" @ Xash3D Engine");

         if (g_gameFlags & GAME_MOBILITY) {
            gameVersionStr.append (" Mobile");
         }
         gameVersionStr.replace ("Legacy", "1.6 Limited");
      }

      if (g_gameFlags & GAME_SUPPORT_BOT_VOICE) {
         gameVersionStr.append (" (BV)");
      }

      if (g_gameFlags & GAME_SUPPORT_SVC_PINGS) {
         gameVersionStr.append (" (SVC)");
      }
      engine.print ("YaPB Bot has detect game version as Counter-Strike: %s", gameVersionStr.chars ());

      if (g_gameFlags & GAME_METAMOD) {
         return;
      }
   }
   
#endif

   auto api_GiveFnptrsToDll = g_gameLib->resolve<void(STD_CALL *) (enginefuncs_t *, globalvars_t *)> ("GiveFnptrsToDll");

   assert (api_GiveFnptrsToDll != nullptr);
   GetEngineFunctions (functionTable, nullptr);

   // give the engine functions to the other DLL...
   api_GiveFnptrsToDll (functionTable, pGlobals);
}

DLL_ENTRYPOINT {
   // dynamic library entry point, can be used for uninitialization stuff. NOT for initializing
   // anything because if you ever attempt to wander outside the scope of this function on a
   // DLL attach, LoadLibrary() will simply fail. And you can't do I/Os here either.

   // dynamic library detaching ??
   if (DLL_DETACHING) {
      cleanupGarbage (); // free everything that's freeable
      delete g_gameLib; // if dynamic link library of mod is load, free it
   }
   DLL_RETENTRY; // the return data type is OS specific too
}

void helper_LinkEntity (entity_func_t &addr, const char *name, entvars_t *pev) {
   if (addr == nullptr) {
      addr = g_gameLib->resolve<entity_func_t> (name);
   }

   if (addr == nullptr) {
      return;
   }
   addr (pev);
}

#define LINK_ENTITY(entityName)                              \
   SHARED_LIBRARAY_EXPORT void entityName (entvars_t *pev) { \
      static entity_func_t addr;                             \
      helper_LinkEntity (addr, #entityName, pev);            \
   }

// entities in counter-strike...
LINK_ENTITY (DelayedUse)
LINK_ENTITY (ambient_generic)
LINK_ENTITY (ammo_338magnum)
LINK_ENTITY (ammo_357sig)
LINK_ENTITY (ammo_45acp)
LINK_ENTITY (ammo_50ae)
LINK_ENTITY (ammo_556nato)
LINK_ENTITY (ammo_556natobox)
LINK_ENTITY (ammo_57mm)
LINK_ENTITY (ammo_762nato)
LINK_ENTITY (ammo_9mm)
LINK_ENTITY (ammo_buckshot)
LINK_ENTITY (armoury_entity)
LINK_ENTITY (beam)
LINK_ENTITY (bodyque)
LINK_ENTITY (button_target)
LINK_ENTITY (cycler)
LINK_ENTITY (cycler_prdroid)
LINK_ENTITY (cycler_sprite)
LINK_ENTITY (cycler_weapon)
LINK_ENTITY (cycler_wreckage)
LINK_ENTITY (env_beam)
LINK_ENTITY (env_beverage)
LINK_ENTITY (env_blood)
LINK_ENTITY (env_bombglow)
LINK_ENTITY (env_bubbles)
LINK_ENTITY (env_debris)
LINK_ENTITY (env_explosion)
LINK_ENTITY (env_fade)
LINK_ENTITY (env_funnel)
LINK_ENTITY (env_global)
LINK_ENTITY (env_glow)
LINK_ENTITY (env_laser)
LINK_ENTITY (env_lightning)
LINK_ENTITY (env_message)
LINK_ENTITY (env_rain)
LINK_ENTITY (env_render)
LINK_ENTITY (env_shake)
LINK_ENTITY (env_shooter)
LINK_ENTITY (env_snow)
LINK_ENTITY (env_sound)
LINK_ENTITY (env_spark)
LINK_ENTITY (env_sprite)
LINK_ENTITY (fireanddie)
LINK_ENTITY (func_bomb_target)
LINK_ENTITY (func_breakable)
LINK_ENTITY (func_button)
LINK_ENTITY (func_buyzone)
LINK_ENTITY (func_conveyor)
LINK_ENTITY (func_door)
LINK_ENTITY (func_door_rotating)
LINK_ENTITY (func_escapezone)
LINK_ENTITY (func_friction)
LINK_ENTITY (func_grencatch)
LINK_ENTITY (func_guntarget)
LINK_ENTITY (func_healthcharger)
LINK_ENTITY (func_hostage_rescue)
LINK_ENTITY (func_illusionary)
LINK_ENTITY (func_ladder)
LINK_ENTITY (func_monsterclip)
LINK_ENTITY (func_mortar_field)
LINK_ENTITY (func_pendulum)
LINK_ENTITY (func_plat)
LINK_ENTITY (func_platrot)
LINK_ENTITY (func_pushable)
LINK_ENTITY (func_rain)
LINK_ENTITY (func_recharge)
LINK_ENTITY (func_rot_button)
LINK_ENTITY (func_rotating)
LINK_ENTITY (func_snow)
LINK_ENTITY (func_tank)
LINK_ENTITY (func_tankcontrols)
LINK_ENTITY (func_tanklaser)
LINK_ENTITY (func_tankmortar)
LINK_ENTITY (func_tankrocket)
LINK_ENTITY (func_trackautochange)
LINK_ENTITY (func_trackchange)
LINK_ENTITY (func_tracktrain)
LINK_ENTITY (func_train)
LINK_ENTITY (func_traincontrols)
LINK_ENTITY (func_vehicle)
LINK_ENTITY (func_vehiclecontrols)
LINK_ENTITY (func_vip_safetyzone)
LINK_ENTITY (func_wall)
LINK_ENTITY (func_wall_toggle)
LINK_ENTITY (func_water)
LINK_ENTITY (func_weaponcheck)
LINK_ENTITY (game_counter)
LINK_ENTITY (game_counter_set)
LINK_ENTITY (game_end)
LINK_ENTITY (game_player_equip)
LINK_ENTITY (game_player_hurt)
LINK_ENTITY (game_player_team)
LINK_ENTITY (game_score)
LINK_ENTITY (game_team_master)
LINK_ENTITY (game_team_set)
LINK_ENTITY (game_text)
LINK_ENTITY (game_zone_player)
LINK_ENTITY (gibshooter)
LINK_ENTITY (grenade)
LINK_ENTITY (hostage_entity)
LINK_ENTITY (info_bomb_target)
LINK_ENTITY (info_hostage_rescue)
LINK_ENTITY (info_intermission)
LINK_ENTITY (info_landmark)
LINK_ENTITY (info_map_parameters)
LINK_ENTITY (info_null)
LINK_ENTITY (info_player_deathmatch)
LINK_ENTITY (info_player_start)
LINK_ENTITY (info_target)
LINK_ENTITY (info_teleport_destination)
LINK_ENTITY (info_vip_start)
LINK_ENTITY (infodecal)
LINK_ENTITY (item_airtank)
LINK_ENTITY (item_antidote)
LINK_ENTITY (item_assaultsuit)
LINK_ENTITY (item_battery)
LINK_ENTITY (item_healthkit)
LINK_ENTITY (item_kevlar)
LINK_ENTITY (item_longjump)
LINK_ENTITY (item_security)
LINK_ENTITY (item_sodacan)
LINK_ENTITY (item_suit)
LINK_ENTITY (item_thighpack)
LINK_ENTITY (light)
LINK_ENTITY (light_environment)
LINK_ENTITY (light_spot)
LINK_ENTITY (momentary_door)
LINK_ENTITY (momentary_rot_button)
LINK_ENTITY (monster_hevsuit_dead)
LINK_ENTITY (monster_mortar)
LINK_ENTITY (monster_scientist)
LINK_ENTITY (multi_manager)
LINK_ENTITY (multisource)
LINK_ENTITY (path_corner)
LINK_ENTITY (path_track)
LINK_ENTITY (player)
LINK_ENTITY (player_loadsaved)
LINK_ENTITY (player_weaponstrip)
LINK_ENTITY (soundent)
LINK_ENTITY (spark_shower)
LINK_ENTITY (speaker)
LINK_ENTITY (target_cdaudio)
LINK_ENTITY (test_effect)
LINK_ENTITY (trigger)
LINK_ENTITY (trigger_auto)
LINK_ENTITY (trigger_autosave)
LINK_ENTITY (trigger_camera)
LINK_ENTITY (trigger_cdaudio)
LINK_ENTITY (trigger_changelevel)
LINK_ENTITY (trigger_changetarget)
LINK_ENTITY (trigger_counter)
LINK_ENTITY (trigger_endsection)
LINK_ENTITY (trigger_gravity)
LINK_ENTITY (trigger_hurt)
LINK_ENTITY (trigger_monsterjump)
LINK_ENTITY (trigger_multiple)
LINK_ENTITY (trigger_once)
LINK_ENTITY (trigger_push)
LINK_ENTITY (trigger_relay)
LINK_ENTITY (trigger_teleport)
LINK_ENTITY (trigger_transition)
LINK_ENTITY (weapon_ak47)
LINK_ENTITY (weapon_aug)
LINK_ENTITY (weapon_awp)
LINK_ENTITY (weapon_c4)
LINK_ENTITY (weapon_deagle)
LINK_ENTITY (weapon_elite)
LINK_ENTITY (weapon_famas)
LINK_ENTITY (weapon_fiveseven)
LINK_ENTITY (weapon_flashbang)
LINK_ENTITY (weapon_g3sg1)
LINK_ENTITY (weapon_galil)
LINK_ENTITY (weapon_glock18)
LINK_ENTITY (weapon_hegrenade)
LINK_ENTITY (weapon_knife)
LINK_ENTITY (weapon_m249)
LINK_ENTITY (weapon_m3)
LINK_ENTITY (weapon_m4a1)
LINK_ENTITY (weapon_mac10)
LINK_ENTITY (weapon_mp5navy)
LINK_ENTITY (weapon_p228)
LINK_ENTITY (weapon_p90)
LINK_ENTITY (weapon_scout)
LINK_ENTITY (weapon_sg550)
LINK_ENTITY (weapon_sg552)
LINK_ENTITY (weapon_shield)
LINK_ENTITY (weapon_shieldgun)
LINK_ENTITY (weapon_smokegrenade)
LINK_ENTITY (weapon_tmp)
LINK_ENTITY (weapon_ump45)
LINK_ENTITY (weapon_usp)
LINK_ENTITY (weapon_xm1014)
LINK_ENTITY (weaponbox)
LINK_ENTITY (world_items)
LINK_ENTITY (worldspawn)