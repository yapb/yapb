//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#include <core.h>

// console vars
ConVar yb_password ("yb_password", "", VT_PASSWORD);
ConVar yb_password_key ("yb_password_key", "_ybpw");

ConVar yb_language ("yb_language", "en");
ConVar yb_version ("yb_version", PRODUCT_VERSION, VT_READONLY);

ConVar mp_startmoney ("mp_startmoney", nullptr, VT_NOREGISTER);

int BotCommandHandler (edict_t *ent, const char *arg0, const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *arg5, const char *self)
{
   // adding one bot with random parameters to random team
   if (stricmp (arg0, "addbot") == 0 || stricmp (arg0, "add") == 0)
      bots.AddBot (arg4, arg1, arg2, arg3, arg5);

   // adding one bot with high difficulty parameters to random team
   else if (stricmp (arg0, "addbot_hs") == 0 || stricmp (arg0, "addhs") == 0)
      bots.AddBot (arg4, "4", "1", arg3, arg5);

   // adding one bot with random parameters to terrorist team
   else if (stricmp (arg0, "addbot_t") == 0 || stricmp (arg0, "add_t") == 0)
      bots.AddBot (arg4, arg1, arg2, "1", arg5);

   // adding one bot with random parameters to counter-terrorist team
   else if (stricmp (arg0, "addbot_ct") == 0 || stricmp (arg0, "add_ct") == 0)
      bots.AddBot (arg4, arg1, arg2, "2", arg5);
         
   // kicking off one bot from the terrorist team
   else if (stricmp (arg0, "kickbot_t") == 0 || stricmp (arg0, "kick_t") == 0)
      bots.RemoveFromTeam (TERRORIST);

   // kicking off one bot from the counter-terrorist team
   else if (stricmp (arg0, "kickbot_ct") == 0 || stricmp (arg0, "kick_ct") == 0)
      bots.RemoveFromTeam (CT);

   // kills all bots on the terrorist team
   else if (stricmp (arg0, "killbots_t") == 0 || stricmp (arg0, "kill_t") == 0)
      bots.KillAll (TERRORIST);

   // kills all bots on the counter-terrorist team
   else if (stricmp (arg0, "killbots_ct") == 0 || stricmp (arg0, "kill_ct") == 0)
      bots.KillAll (CT);

   // list all bots playeing on the server
   else if (stricmp (arg0, "listbots") == 0 || stricmp (arg0, "list") == 0)
      bots.ListBots ();

   // kick off all bots from the played server
   else if (stricmp (arg0, "kickbots") == 0 || stricmp (arg0, "kickall") == 0)
      bots.RemoveAll ();

   // kill all bots on the played server
   else if (stricmp (arg0, "killbots") == 0 || stricmp (arg0, "killall") == 0)
      bots.KillAll ();

   // kick off one random bot from the played server
   else if (stricmp (arg0, "kickone") == 0 || stricmp (arg0, "kick") == 0)
      bots.RemoveRandom ();

   // fill played server with bots
   else if (stricmp (arg0, "fillserver") == 0 || stricmp (arg0, "fill") == 0)
      bots.FillServer (atoi (arg1), IsNullString (arg2) ? -1 : atoi (arg2), IsNullString (arg3) ? -1 : atoi (arg3), IsNullString (arg4) ? -1 : atoi (arg4));

   // select the weapon mode for bots
   else if (stricmp (arg0, "weaponmode") == 0 || stricmp (arg0, "wmode") == 0)
   {
      int selection = atoi (arg1);

      // check is selected range valid
      if (selection >= 1 && selection <= 7)
         bots.SetWeaponMode (selection);
      else
         engine.ClientPrintf (ent, "Choose weapon from 1 to 7 range");
   }

   // force all bots to vote to specified map
   else if (stricmp (arg0, "votemap") == 0)
   {
      if (!IsNullString (arg1))
      {
         int nominatedMap = atoi (arg1);

         // loop through all players
         for (int i = 0; i < engine.MaxClients (); i++)
         {
            Bot *bot = bots.GetBot (i);

            if (bot != nullptr)
               bot->m_voteMap = nominatedMap;
         }
         engine.ClientPrintf (ent, "All dead bots will vote for map #%d", nominatedMap);
      }
   }

   // displays version information
   else if (stricmp (arg0, "version") == 0 || stricmp (arg0, "ver") == 0)
   {
      char versionData[] =
         "------------------------------------------------\n"
         "Name: %s\n"
         "Version: %s (Build: %u)\n"
         "Compiled: %s, %s\n"
         "Git Hash: %s\n"
         "Git Commit Author: %s\n"
         "------------------------------------------------";

      engine.ClientPrintf (ent, versionData, PRODUCT_NAME, PRODUCT_VERSION, GenerateBuildNumber (), __DATE__, __TIME__, PRODUCT_GIT_HASH, PRODUCT_GIT_COMMIT_AUTHOR);
   }

   // display some sort of help information
   else if (stricmp (arg0, "?") == 0 || stricmp (arg0, "help") == 0)
   {
      engine.ClientPrintf (ent, "Bot Commands:");
      engine.ClientPrintf (ent, "%s version\t - display version information.", self);
      engine.ClientPrintf (ent, "%s add\t - create a bot in current game.", self);
      engine.ClientPrintf (ent, "%s fill\t - fill the server with random bots.", self);
      engine.ClientPrintf (ent, "%s kickall\t - disconnects all bots from current game.", self);
      engine.ClientPrintf (ent, "%s killbots\t - kills all bots in current game.", self);
      engine.ClientPrintf (ent, "%s kick\t - disconnect one random bot from game.", self);
      engine.ClientPrintf (ent, "%s weaponmode\t - select bot weapon mode.", self);
      engine.ClientPrintf (ent, "%s votemap\t - allows dead bots to vote for specific map.", self);
      engine.ClientPrintf (ent, "%s cmenu\t - displaying bots command menu.", self);

      if (stricmp (arg1, "full") == 0 || stricmp (arg1, "f") == 0 || stricmp (arg1, "?") == 0)
      {
         engine.ClientPrintf (ent, "%s add_t\t - creates one random bot to terrorist team.", self);
         engine.ClientPrintf (ent, "%s add_ct\t - creates one random bot to ct team.", self);
         engine.ClientPrintf (ent, "%s kick_t\t - disconnect one random bot from terrorist team.", self);
         engine.ClientPrintf (ent, "%s kick_ct\t - disconnect one random bot from ct team.", self);
         engine.ClientPrintf (ent, "%s kill_t\t - kills all bots on terrorist team.", self);
         engine.ClientPrintf (ent, "%s kill_ct\t - kills all bots on ct team.", self);
         engine.ClientPrintf (ent, "%s list\t - display list of bots currently playing.", self);
         engine.ClientPrintf (ent, "%s order\t - execute specific command on specified bot.", self);
         engine.ClientPrintf (ent, "%s time\t - displays current time on server.", self);
         engine.ClientPrintf (ent, "%s deletewp\t - erase waypoint file from hard disk (permanently).", self);

          if (!engine.IsDedicatedServer ())
          {
             engine.Printf ("%s autowp\t - toggle autowaypointing.", self);
             engine.Printf ("%s wp\t - toggle waypoint showing.", self);
             engine.Printf ("%s wp on noclip\t - enable noclip cheat", self);
             engine.Printf ("%s wp save nocheck\t - save waypoints without checking.", self);
             engine.Printf ("%s wp add\t - open menu for waypoint creation.", self);
             engine.Printf ("%s wp menu\t - open main waypoint menu.", self);
             engine.Printf ("%s wp addbasic\t - creates basic waypoints on map.", self);
             engine.Printf ("%s wp find\t - show direction to specified waypoint.", self);
             engine.Printf ("%s wp load\t - load the waypoint file from hard disk.", self);
             engine.Printf ("%s wp check\t - checks if all waypoints connections are valid.", self);
             engine.Printf ("%s wp cache\t - cache nearest waypoint.", self);
             engine.Printf ("%s wp teleport\t - teleport hostile to specified waypoint.", self);
             engine.Printf ("%s wp setradius\t - manually sets the wayzone radius for this waypoint.", self);
             engine.Printf ("%s path autodistance - opens menu for setting autopath maximum distance.", self);
             engine.Printf ("%s path cache\t - remember the nearest to player waypoint.", self);
             engine.Printf ("%s path create\t - opens menu for path creation.", self);
             engine.Printf ("%s path delete\t - delete path from cached to nearest waypoint.", self);
             engine.Printf ("%s path create_in\t - creating incoming path connection.", self);
             engine.Printf ("%s path create_out\t - creating outgoing path connection.", self);
             engine.Printf ("%s path create_both\t - creating both-ways path connection.", self);
             engine.Printf ("%s exp save\t - save the experience data.", self);
          }
      }
   }
   else if (stricmp (arg0, "bot_takedamage") == 0 && !IsNullString (arg1))
   {
      bool isOn = !!(atoi (arg1) == 1);

      for (int i = 0; i < engine.MaxClients (); i++)
      {
         Bot *bot = bots.GetBot (i);

         if (bot != nullptr)
         {
            bot->pev->takedamage = isOn ? 0.0f : 1.0f;
         }
      }
   }

   // displays main bot menu
   else if (stricmp (arg0, "botmenu") == 0 || stricmp (arg0, "menu") == 0)
      DisplayMenuToClient (ent, BOT_MENU_MAIN);

   // display command menu
   else if (stricmp (arg0, "cmdmenu") == 0 || stricmp (arg0, "cmenu") == 0)
   {
      if (IsAlive (ent))
         DisplayMenuToClient (ent, BOT_MENU_COMMANDS);
      else
      {
         DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display
         engine.CenterPrintf ("You're dead, and have no access to this menu");
      }
   }

   // waypoint manimupulation (really obsolete, can be edited through menu) (supported only on listen server)
   else if (stricmp (arg0, "waypoint") == 0 || stricmp (arg0, "wp") == 0 || stricmp (arg0, "wpt") == 0)
   {
      if (engine.IsDedicatedServer () || engine.IsNullEntity (g_hostEntity))
         return 2;

      // enables or disable waypoint displaying
      if (stricmp (arg1, "on") == 0)
      {
         g_waypointOn = true;
         engine.Printf ("Waypoint Editing Enabled");

         // enables noclip cheat
         if (stricmp (arg2, "noclip") == 0)
         {
            if (g_editNoclip)
            {
               g_hostEntity->v.movetype = MOVETYPE_WALK;
               engine.Printf ("Noclip Cheat Disabled");
            }
            else
            {
               g_hostEntity->v.movetype = MOVETYPE_NOCLIP;
               engine.Printf ("Noclip Cheat Enabled");
            }
            g_editNoclip = !g_editNoclip; // switch on/off (XOR it!)
         }
         engine.IssueCmd ("yapb wp mdl on");
      }

      // switching waypoint editing off
      else if (stricmp (arg1, "off") == 0)
      {
         g_waypointOn = false;
         g_editNoclip = false;
         g_hostEntity->v.movetype = MOVETYPE_WALK;

         engine.Printf ("Waypoint Editing Disabled");
         engine.IssueCmd ("yapb wp mdl off");
      }

      // toggles displaying player models on spawn spots
      else if (stricmp (arg1, "mdl") == 0 || stricmp (arg1, "models") == 0)
      {
         edict_t *spawnEntity = nullptr;

         if (stricmp (arg2, "on") == 0)
         {
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_player_start")))
               spawnEntity->v.effects &= ~EF_NODRAW;
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_player_deathmatch")))
               spawnEntity->v.effects &= ~EF_NODRAW;
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_vip_start")))
               spawnEntity->v.effects &= ~EF_NODRAW;

            engine.IssueCmd ("mp_roundtime 9"); // reset round time to maximum
            engine.IssueCmd ("mp_timelimit 0"); // disable the time limit
            engine.IssueCmd ("mp_freezetime 0"); // disable freezetime
         }
         else if (stricmp (arg2, "off") == 0)
         {
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_player_start")))
               spawnEntity->v.effects |= EF_NODRAW;
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_player_deathmatch")))
               spawnEntity->v.effects |= EF_NODRAW;
            while (!engine.IsNullEntity (spawnEntity = FIND_ENTITY_BY_CLASSNAME (spawnEntity, "info_vip_start")))
               spawnEntity->v.effects |= EF_NODRAW;
         }
      }

      // show direction to specified waypoint
      else if (stricmp (arg1, "find") == 0)
         waypoints.SetFindIndex (atoi (arg2));

      // opens adding waypoint menu
      else if (stricmp (arg1, "add") == 0)
      {
         g_waypointOn = true;  // turn waypoints on
         DisplayMenuToClient (g_hostEntity, BOT_MENU_WAYPOINT_TYPE);
      }

      // creates basic waypoints on the map (ladder/spawn points/goals)
      else if (stricmp (arg1, "addbasic") == 0)
      {
         waypoints.CreateBasic ();
         engine.CenterPrintf ("Basic waypoints was Created");
      }

      // delete nearest to host edict waypoint
      else if (stricmp (arg1, "delete") == 0)
      {
         g_waypointOn = true; // turn waypoints on
         waypoints.Delete ();
      }

      // save waypoint data into file on hard disk
      else if (stricmp (arg1, "save") == 0)
      {
         char *waypointSaveMessage = engine.TraslateMessage ("Waypoints Saved");

         if (FStrEq (arg2, "nocheck"))
         {
            waypoints.Save ();
            engine.Printf (waypointSaveMessage);
         }
         else if (waypoints.NodesValid ())
         {
            waypoints.Save ();
            engine.Printf (waypointSaveMessage);
         }
      }

      // remove waypoint and all corresponding files from hard disk
      else if (stricmp (arg1, "erase") == 0)
         waypoints.EraseFromHardDisk ();

      // load all waypoints again (overrides all changes, that wasn't saved)
      else if (stricmp (arg1, "load") == 0)
      {
         if (waypoints.Load ())
            engine.Printf ("Waypoints loaded");
      }

      // check all nodes for validation
      else if (stricmp (arg1, "check") == 0)
      {
         if (waypoints.NodesValid ())
            engine.CenterPrintf ("Nodes work Fine");
      }

      // opens menu for setting (removing) waypoint flags
      else if (stricmp (arg1, "flags") == 0)
         DisplayMenuToClient (g_hostEntity, BOT_MENU_WAYPOINT_FLAG);

      // setting waypoint radius
      else if (stricmp (arg1, "setradius") == 0)
         waypoints.SetRadius (atoi (arg2));

      // remembers nearest waypoint
      else if (stricmp (arg1, "cache") == 0)
         waypoints.CacheWaypoint ();

      // teleport player to specified waypoint
      else if (stricmp (arg1, "teleport") == 0)
      {
         int teleportPoint = atoi (arg2);

         if (teleportPoint < g_numWaypoints)
         {
            Path *path = waypoints.GetPath (teleportPoint);

            (*g_engfuncs.pfnSetOrigin) (g_hostEntity, path->origin);
            g_waypointOn = true;

            engine.Printf ("Player '%s' teleported to waypoint #%d (x:%.1f, y:%.1f, z:%.1f)", STRING (g_hostEntity->v.netname), teleportPoint, path->origin.x, path->origin.y, path->origin.z); //-V807
            g_editNoclip = true;
         }
      }

      // displays waypoint menu
      else if (stricmp (arg1, "menu") == 0)
         DisplayMenuToClient (g_hostEntity, BOT_MENU_WAYPOINT_MAIN_PAGE1);

      // otherwise display waypoint current status
      else
         engine.Printf ("Waypoints are %s", g_waypointOn == true ? "Enabled" : "Disabled");
   }

   // path waypoint editing system (supported only on listen server)
   else if (stricmp (arg0, "pathwaypoint") == 0 || stricmp (arg0, "path") == 0 || stricmp (arg0, "pwp") == 0)
   {
      if (engine.IsDedicatedServer () || engine.IsNullEntity (g_hostEntity))
         return 2;

      // opens path creation menu
      if (stricmp (arg1, "create") == 0)
         DisplayMenuToClient (g_hostEntity, BOT_MENU_WAYPOINT_PATH);

      // creates incoming path from the cached waypoint
      else if (stricmp (arg1, "create_in") == 0)
         waypoints.CreatePath (CONNECTION_INCOMING);

      // creates outgoing path from current waypoint
      else if (stricmp (arg1, "create_out") == 0)
         waypoints.CreatePath (CONNECTION_OUTGOING);

      // creates bidirectional path from cahed to current waypoint
      else if (stricmp (arg1, "create_both") == 0)
         waypoints.CreatePath (CONNECTION_BOTHWAYS);

      // delete special path
      else if (stricmp (arg1, "delete") == 0)
         waypoints.DeletePath ();

      // sets auto path maximum distance
      else if (stricmp (arg1, "autodistance") == 0)
         DisplayMenuToClient (g_hostEntity, BOT_MENU_WAYPOINT_AUTOPATH);
   }

   // automatic waypoint handling (supported only on listen server)
   else if (stricmp (arg0, "autowaypoint") == 0 || stricmp (arg0, "autowp") == 0)
   {
      if (engine.IsDedicatedServer () || engine.IsNullEntity (g_hostEntity))
         return 2;

      // enable autowaypointing
      if (stricmp (arg1, "on") == 0)
      {
         g_autoWaypoint = true;
         g_waypointOn = true; // turn this on just in case
      }

      // disable autowaypointing
      else if (stricmp (arg1, "off") == 0)
         g_autoWaypoint = false;

      // display status
      engine.Printf ("Auto-Waypoint %s", g_autoWaypoint ? "Enabled" : "Disabled");
   }

   // experience system handling (supported only on listen server)
   else if (stricmp (arg0, "experience") == 0 || stricmp (arg0, "exp") == 0)
   {
      if (engine.IsDedicatedServer () || engine.IsNullEntity (g_hostEntity))
         return 2;

      // write experience table (and visibility table) to hard disk
      if (stricmp (arg1, "save") == 0)
      {
         waypoints.SaveExperienceTab ();
         waypoints.SaveVisibilityTab ();

         engine.Printf ("Experience tab saved");
      }
   }
   else
      return 0; // command is not handled by bot

   return 1; // command was handled by bot
}

void CommandHandler (void)
{
   // this function is the dedicated server command handler for the new yapb server command we
   // registered at game start. It will be called by the engine each time a server command that
   // starts with "yapb" is entered in the server console. It works exactly the same way as
   // ClientCommand() does, using the CmdArgc() and CmdArgv() facilities of the engine. Argv(0)
   // is the server command itself (here "yapb") and the next ones are its arguments. Just like
   // the stdio command-line parsing in C when you write "long main (long argc, char **argv)".

   // check status for dedicated server command
   if (BotCommandHandler (g_hostEntity, IsNullString (CMD_ARGV (1)) ? "help" : CMD_ARGV (1), CMD_ARGV (2), CMD_ARGV (3), CMD_ARGV (4), CMD_ARGV (5), CMD_ARGV (6), CMD_ARGV (0)) == 0)
      engine.Printf ("Unknown command: %s", CMD_ARGV (1));
}

void ParseVoiceEvent (const String &base, int type, float timeToRepeat)
{
   // this function does common work of parsing single line of voice chatter

   Array <String> temp = String (base).Split (',');
   ChatterItem chatterItem;

   FOR_EACH_AE (temp, i)
   {
      temp[i].Trim ().TrimQuotes ();

      if (engine.GetWaveLength (temp[i]) <= 0.0f)
         continue;

      chatterItem.name = temp[i];
      chatterItem.repeatTime = timeToRepeat;

      g_chatterFactory[type].Push (chatterItem);
    }
   temp.RemoveAll ();
}

// forwards for MemoryFile
MemoryFile::MF_Loader MemoryFile::Loader = nullptr;
MemoryFile::MF_Unloader MemoryFile::Unloader = nullptr;

void InitConfig (void)
{
   if (!MemoryFile::Loader && !MemoryFile::Unloader)
   {
      MemoryFile::Loader = reinterpret_cast <MemoryFile::MF_Loader> (g_engfuncs.pfnLoadFileForMe);
      MemoryFile::Unloader = reinterpret_cast <MemoryFile::MF_Unloader> (g_engfuncs.pfnFreeFile);
   }

   MemoryFile fp;
   char line[512];

   KeywordFactory replyKey;

   // fixes for crashing if configs couldn't be accessed
   g_chatFactory.SetSize (CHAT_TOTAL);
   g_chatterFactory.SetSize (Chatter_Total);

   #define SKIP_COMMENTS() if (line[0] == '/' || line[0] == '\r' || line[0] == '\n' || line[0] == 0 || line[0] == ' ' || line[0] == '\t' || line[0] == ';') continue

   // NAMING SYSTEM INITIALIZATION
   if (OpenConfig ("names.cfg", "Name configuration file not found.", &fp , true))
   {
      g_botNames.RemoveAll ();

      while (fp.GetBuffer (line, 255))
      {
         SKIP_COMMENTS ();

         Array <String> pair = String (line).Split ("\t\t");

         if (pair.GetElementNumber () > 1)
            strncpy (line, pair[0].Trim ().GetBuffer (), SIZEOF_CHAR (line));

         String::TrimExternalBuffer (line);
         line[32] = 0;

         BotName item;
         memset (&item, 0, sizeof (item));

         item.name = line;
         item.used = false;

         if (pair.GetElementNumber () > 1)
            item.steamId = pair[1].Trim ();

         g_botNames.Push (item);
      }
      fp.Close ();
   }

   // CHAT SYSTEM CONFIG INITIALIZATION
   if (OpenConfig ("chat.cfg", "Chat file not found.", &fp, true))
   {
      char section[80];
      int chatType = -1;

      while (fp.GetBuffer (line, 255))
      {
         SKIP_COMMENTS ();
         strncpy (section, Engine::ExtractSingleField (line, 0, 1), SIZEOF_CHAR (section));

         if (strcmp (section, "[KILLED]") == 0)
         {
            chatType = 0;
            continue;
         }
         else if (strcmp (section, "[BOMBPLANT]") == 0)
         {
            chatType = 1;
            continue;
         }
         else if (strcmp (section, "[DEADCHAT]") == 0)
         {
            chatType = 2;
            continue;
         }
         else if (strcmp (section, "[REPLIES]") == 0)
         {
            chatType = 3;
            continue;
         }
         else if (strcmp (section, "[UNKNOWN]") == 0)
         {
            chatType = 4;
            continue;
         }
         else if (strcmp (section, "[TEAMATTACK]") == 0)
         {
            chatType = 5;
            continue;
         }
         else if (strcmp (section, "[WELCOME]") == 0)
         {
            chatType = 6;
            continue;
         }
         else if (strcmp (section, "[TEAMKILL]") == 0)
         {
            chatType = 7;
            continue;
         }

         if (chatType != 3)
            line[79] = 0;

         String::TrimExternalBuffer (line);

         switch (chatType)
         {
         case 0:
            g_chatFactory[CHAT_KILLING].Push (line);
            break;

         case 1:
            g_chatFactory[CHAT_BOMBPLANT].Push (line);
            break;

         case 2:
            g_chatFactory[CHAT_DEAD].Push (line);
            break;

         case 3:
            if (strstr (line, "@KEY") != nullptr)
            {
               if (!replyKey.keywords.IsEmpty () && !replyKey.replies.IsEmpty ())
               {
                  g_replyFactory.Push (replyKey);
                  replyKey.replies.RemoveAll ();
               }

               replyKey.keywords.RemoveAll ();
               replyKey.keywords = String (&line[4]).Split (',');

               FOR_EACH_AE (replyKey.keywords, i)
                  replyKey.keywords[i].Trim ().TrimQuotes ();
            }
            else if (!replyKey.keywords.IsEmpty ())
               replyKey.replies.Push (line);

            break;

         case 4:
            g_chatFactory[CHAT_NOKW].Push (line);
            break;

         case 5:
            g_chatFactory[CHAT_TEAMATTACK].Push (line);
            break;

         case 6:
            g_chatFactory[CHAT_WELCOME].Push (line);
            break;

         case 7:
            g_chatFactory[CHAT_TEAMKILL].Push (line);
            break;
         }
      }
      fp.Close ();
   }
   else
   {
      extern ConVar yb_chat;
      yb_chat.SetInt (0);
   }
  
   // GENERAL DATA INITIALIZATION
   if (OpenConfig ("general.cfg", "General configuration file not found. Loading defaults", &fp))
   {
      while (fp.GetBuffer (line, 255))
      {
         SKIP_COMMENTS ();

         Array <String> pair = String (line).Split ('=');

         if (pair.GetElementNumber () != 2)
            continue;

         pair[0].Trim ().Trim ();
         pair[1].Trim ().Trim ();

         Array <String> splitted = pair[1].Split (',');

         if (pair[0] == "MapStandard")
         {
            if (splitted.GetElementNumber () != NUM_WEAPONS)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < NUM_WEAPONS; i++)
               g_weaponSelect[i].teamStandard = splitted[i].ToInt ();
         }
         else if (pair[0] == "MapAS")
         {
            if (splitted.GetElementNumber () != NUM_WEAPONS)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < NUM_WEAPONS; i++)
               g_weaponSelect[i].teamAS = splitted[i].ToInt ();
         }
         else if (pair[0] == "GrenadePercent")
         {
            if (splitted.GetElementNumber () != 3)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < 3; i++)
               g_grenadeBuyPrecent[i] = splitted[i].ToInt ();
         }
         else if (pair[0] == "Economics")
         {
            if (splitted.GetElementNumber () != 11)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < 11; i++)
               g_botBuyEconomyTable[i] = splitted[i].ToInt ();
         }
         else if (pair[0] == "PersonalityNormal")
         {
            if (splitted.GetElementNumber () != NUM_WEAPONS)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < NUM_WEAPONS; i++)
               g_normalWeaponPrefs[i] = splitted[i].ToInt ();
         }
         else if (pair[0] == "PersonalityRusher")
         {
            if (splitted.GetElementNumber () != NUM_WEAPONS)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < NUM_WEAPONS; i++)
               g_rusherWeaponPrefs[i] = splitted[i].ToInt ();
         }
         else if (pair[0] == "PersonalityCareful")
         {
            if (splitted.GetElementNumber () != NUM_WEAPONS)
               AddLogEntry (true, LL_FATAL, "%s entry in general config is not valid.", pair[0].GetBuffer ());

            for (int i = 0; i < NUM_WEAPONS; i++)
               g_carefulWeaponPrefs[i] = splitted[i].ToInt ();
         }
      }
      fp.Close ();
   }

   // CHATTER SYSTEM INITIALIZATION
   if (OpenConfig ("chatter.cfg", "Couldn't open chatter system configuration", &fp) && !(g_gameFlags & GAME_LEGACY) && yb_communication_type.GetInt () == 2)
   {
      Array <String> array;

      extern ConVar yb_chatter_path;

      while (fp.GetBuffer (line, 511))
      {
         SKIP_COMMENTS ();

         if (strncmp (line, "RewritePath", 11) == 0)
            yb_chatter_path.SetString (String (&line[12]).Trim ());
         else if (strncmp (line, "Event", 5) == 0)
         {
            array = String (&line[6]).Split ('=');

            if (array.GetElementNumber () != 2)
               AddLogEntry (true, LL_FATAL, "Error in chatter config file syntax... Please correct all Errors.");

            FOR_EACH_AE (array, i)
               array[i].Trim ().Trim (); // double trim

            // just to be more unique :)
            array[1].TrimLeft ('(');
            array[1].TrimRight (';');
            array[1].TrimRight (')');

            #define PARSE_CHATTER_ITEM(type, timeToRepeatAgain) { if (strcmp (array[0], #type) == 0) ParseVoiceEvent (array[1], type, timeToRepeatAgain); }
            #define PARSE_CHATTER_ITEM_NR(type) PARSE_CHATTER_ITEM(type, 99999.0f)

            // radio system
            PARSE_CHATTER_ITEM_NR (Radio_CoverMe);
            PARSE_CHATTER_ITEM_NR (Radio_YouTakePoint);
            PARSE_CHATTER_ITEM_NR (Radio_HoldPosition);
            PARSE_CHATTER_ITEM_NR (Radio_RegroupTeam);
            PARSE_CHATTER_ITEM_NR (Radio_FollowMe);
            PARSE_CHATTER_ITEM_NR (Radio_TakingFire);
            PARSE_CHATTER_ITEM_NR (Radio_GoGoGo);
            PARSE_CHATTER_ITEM_NR (Radio_Fallback);
            PARSE_CHATTER_ITEM_NR (Radio_StickTogether);
            PARSE_CHATTER_ITEM_NR (Radio_GetInPosition);
            PARSE_CHATTER_ITEM_NR (Radio_StormTheFront);
            PARSE_CHATTER_ITEM_NR (Radio_ReportTeam);
            PARSE_CHATTER_ITEM_NR (Radio_Affirmative);
            PARSE_CHATTER_ITEM_NR (Radio_EnemySpotted);
            PARSE_CHATTER_ITEM_NR (Radio_NeedBackup);
            PARSE_CHATTER_ITEM_NR (Radio_SectorClear);
            PARSE_CHATTER_ITEM_NR (Radio_InPosition);
            PARSE_CHATTER_ITEM_NR (Radio_ReportingIn);
            PARSE_CHATTER_ITEM_NR (Radio_ShesGonnaBlow);
            PARSE_CHATTER_ITEM_NR (Radio_Negative);
            PARSE_CHATTER_ITEM_NR (Radio_EnemyDown);

            // voice system
            PARSE_CHATTER_ITEM (Chatter_SpotTheBomber, 4.3f);
            PARSE_CHATTER_ITEM (Chatter_VIPSpotted, 5.3f);
            PARSE_CHATTER_ITEM (Chatter_FriendlyFire, 2.1f);
            PARSE_CHATTER_ITEM_NR (Chatter_DiePain);
            PARSE_CHATTER_ITEM (Chatter_GotBlinded, 5.0f);
            PARSE_CHATTER_ITEM_NR (Chatter_GoingToPlantBomb);
            PARSE_CHATTER_ITEM_NR (Chatter_GoingToGuardVIPSafety);
            PARSE_CHATTER_ITEM_NR (Chatter_RescuingHostages);
            PARSE_CHATTER_ITEM_NR (Chatter_GoingToCamp);
            PARSE_CHATTER_ITEM_NR (Chatter_TeamKill);
            PARSE_CHATTER_ITEM_NR (Chatter_ReportingIn);
            PARSE_CHATTER_ITEM (Chatter_GuardDroppedC4, 3.0f);
            PARSE_CHATTER_ITEM_NR (Chatter_Camp);
            PARSE_CHATTER_ITEM_NR (Chatter_GuardingVipSafety);
            PARSE_CHATTER_ITEM_NR (Chatter_PlantingC4);
            PARSE_CHATTER_ITEM (Chatter_DefusingC4, 3.0f);
            PARSE_CHATTER_ITEM_NR (Chatter_InCombat);
            PARSE_CHATTER_ITEM_NR (Chatter_SeeksEnemy);
            PARSE_CHATTER_ITEM_NR (Chatter_Nothing);
            PARSE_CHATTER_ITEM_NR (Chatter_EnemyDown);
            PARSE_CHATTER_ITEM_NR (Chatter_UseHostage);
            PARSE_CHATTER_ITEM (Chatter_FoundC4, 5.5f);
            PARSE_CHATTER_ITEM_NR (Chatter_WonTheRound);
            PARSE_CHATTER_ITEM (Chatter_ScaredEmotion, 6.1f);
            PARSE_CHATTER_ITEM (Chatter_HeardEnemy, 12.2f);
            PARSE_CHATTER_ITEM (Chatter_SniperWarning, 4.3f);
            PARSE_CHATTER_ITEM (Chatter_SniperKilled, 2.1f);
            PARSE_CHATTER_ITEM_NR (Chatter_QuicklyWonTheRound);
            PARSE_CHATTER_ITEM (Chatter_OneEnemyLeft, 2.5f);
            PARSE_CHATTER_ITEM (Chatter_TwoEnemiesLeft, 2.5f);
            PARSE_CHATTER_ITEM (Chatter_ThreeEnemiesLeft, 2.5f);
            PARSE_CHATTER_ITEM_NR (Chatter_NoEnemiesLeft);
            PARSE_CHATTER_ITEM_NR (Chatter_FoundBombPlace);
            PARSE_CHATTER_ITEM_NR (Chatter_WhereIsTheBomb);
            PARSE_CHATTER_ITEM_NR (Chatter_DefendingBombSite);
            PARSE_CHATTER_ITEM_NR (Chatter_BarelyDefused);
            PARSE_CHATTER_ITEM_NR (Chatter_NiceshotCommander);
            PARSE_CHATTER_ITEM (Chatter_NiceshotPall, 2.0);
            PARSE_CHATTER_ITEM (Chatter_GoingToGuardHostages, 3.0f);
            PARSE_CHATTER_ITEM (Chatter_GoingToGuardDoppedBomb, 3.0f);
            PARSE_CHATTER_ITEM (Chatter_OnMyWay, 1.5f);
            PARSE_CHATTER_ITEM (Chatter_LeadOnSir, 5.0f);
            PARSE_CHATTER_ITEM (Chatter_Pinned_Down, 5.0f);
            PARSE_CHATTER_ITEM (Chatter_GottaFindTheBomb, 3.0f);
            PARSE_CHATTER_ITEM (Chatter_You_Heard_The_Man, 3.0f);
            PARSE_CHATTER_ITEM (Chatter_Lost_The_Commander, 4.5f);
            PARSE_CHATTER_ITEM (Chatter_NewRound, 3.5f);
            PARSE_CHATTER_ITEM (Chatter_CoverMe, 3.5f);
            PARSE_CHATTER_ITEM (Chatter_BehindSmoke, 3.5f);
            PARSE_CHATTER_ITEM (Chatter_BombSiteSecured, 3.5f);
         }
      }
      fp.Close ();
   }
   else
   {
      yb_communication_type.SetInt (1);
      AddLogEntry (true, LL_DEFAULT, "Chatter Communication disabled.");
   }

   // LOCALIZER INITITALIZATION
   if (OpenConfig ("lang.cfg", "Specified language not found", &fp, true) && !(g_gameFlags & GAME_LEGACY))
   {
      if (engine.IsDedicatedServer ())
         return; // dedicated server will use only english translation

      enum Lang { Lang_Original, Lang_Translate } langState = static_cast <Lang> (2);

      char buffer[1024];
      TranslatorPair temp = {"", ""};

      while (fp.GetBuffer (line, 255))
      {
         if (strncmp (line, "[ORIGINAL]", 10) == 0)
         {
            langState = Lang_Original;

            if (!IsNullString (buffer))
            {
               String::TrimExternalBuffer (buffer);
               temp.translated = _strdup (buffer);
               buffer[0] = 0x0;
            }

            if (!IsNullString (temp.translated) && !IsNullString (temp.original))
               engine.PushTranslationPair (temp);
         }
         else if (strncmp (line, "[TRANSLATED]", 12) == 0)
         {
            String::TrimExternalBuffer (buffer);
            temp.original = _strdup (buffer);
            buffer[0] = 0x0;

            langState = Lang_Translate;
         }
         else
         {
            switch (langState)
            {
            case Lang_Original:
               strncat (buffer, line, 1024 - 1 - strlen (buffer));
               break;

            case Lang_Translate:
               strncat (buffer, line, 1024 - 1 - strlen (buffer));
               break;
            }
         }
      }
      fp.Close ();
   }
   else if (g_gameFlags & GAME_LEGACY)
      AddLogEntry (true, LL_DEFAULT, "Multilingual system disabled, due to your Counter-Strike Version!");
   else if (strcmp (yb_language.GetString (), "en") != 0)
      AddLogEntry (true, LL_ERROR, "Couldn't load language configuration");

   // set personality weapon pointers here
   g_weaponPrefs[PERSONALITY_NORMAL] = reinterpret_cast <int *> (&g_normalWeaponPrefs);
   g_weaponPrefs[PERSONALITY_RUSHER] = reinterpret_cast <int *> (&g_rusherWeaponPrefs);
   g_weaponPrefs[PERSONALITY_CAREFUL] = reinterpret_cast <int *> (&g_carefulWeaponPrefs);

   g_timePerSecondUpdate = 0.0f;
}

void GameDLLInit (void)
{
   // this function is a one-time call, and appears to be the second function called in the
   // DLL after GiveFntprsToDll() has been called. Its purpose is to tell the MOD DLL to
   // initialize the game before the engine actually hooks into it with its video frames and
   // clients connecting. Note that it is a different step than the *server* initialization.
   // This one is called once, and only once, when the game process boots up before the first
   // server is enabled. Here is a good place to do our own game session initialization, and
   // to register by the engine side the server commands we need to administrate our bots.

   // register server command(s)
   engine.RegisterCmd ("yapb", CommandHandler);   
   engine.RegisterCmd ("yb", CommandHandler);

   // execute main config
   engine.IssueCmd ("exec addons/yapb/conf/yapb.cfg");

   // set correct version string
   yb_version.SetString (FormatBuffer ("%d.%d.%d", PRODUCT_VERSION_DWORD_INTERNAL, GenerateBuildNumber ()));

   // register fake metamod command handler if we not! under mm
   if (!(g_gameFlags & GAME_METAMOD))
   {
      engine.RegisterCmd ("meta", [] (void)
      {
         engine.Printf ("You're launched standalone version of yapb. Metamod is not installed or not enabled!");
      });
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnGameInit) ();
}

void Touch (edict_t *pentTouched, edict_t *pentOther)
{
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

   if (!engine.IsNullEntity (pentOther) && (pentOther->v.flags & FL_FAKECLIENT))
   {
      Bot *bot = bots.GetBot (pentOther);

      if (bot != nullptr)
         bot->VerifyBreakable (pentTouched);
   }
   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnTouch) (pentTouched, pentOther);
}

int Spawn (edict_t *ent)
{
   // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
   // world, in other words to 'display') the entity pointed to by ent in the game. The
   // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
   // and any MOD is supposed to implement one for each of its entities.

   // for faster access
   const char *entityClassname = STRING (ent->v.classname);

   if (strcmp (entityClassname, "worldspawn") == 0)
   {
      engine.Precache (ent);
      engine.PushRegisteredConVarsToEngine (true);

      PRECACHE_SOUND (ENGINE_STR ("weapons/xbow_hit1.wav"));      // waypoint add
      PRECACHE_SOUND (ENGINE_STR ("weapons/mine_activate.wav"));  // waypoint delete
      PRECACHE_SOUND (ENGINE_STR ("common/wpn_hudoff.wav"));      // path add/delete start
      PRECACHE_SOUND (ENGINE_STR ("common/wpn_hudon.wav"));       // path add/delete done
      PRECACHE_SOUND (ENGINE_STR ("common/wpn_moveselect.wav"));  // path add/delete cancel
      PRECACHE_SOUND (ENGINE_STR ("common/wpn_denyselect.wav"));  // path add/delete error

      RoundInit ();
      g_mapType = 0; // reset map type as worldspawn is the first entity spawned

      // detect official csbots here, as they causing crash in linkent code when active for some reason
      if (!(g_gameFlags & GAME_LEGACY) && g_engfuncs.pfnCVarGetPointer ("bot_stop") != nullptr)
         g_gameFlags |= GAME_OFFICIAL_CSBOT;
   }
   else if (strcmp (entityClassname, "player_weaponstrip") == 0)
   {
      if ((g_gameFlags & GAME_LEGACY) && (STRING (ent->v.target))[0] == '0')
         ent->v.target = ent->v.targetname = ALLOC_STRING ("fake");
      else
      {
         REMOVE_ENTITY (ent);

         if (g_gameFlags & GAME_METAMOD)
            RETURN_META_VALUE (MRES_SUPERCEDE, 0);

         return 0;
      }
   }
#ifndef XASH_CSDM
   else if (strcmp (entityClassname, "info_player_start") == 0)
   {
      SET_MODEL (ent, ENGINE_STR ("models/player/urban/urban.mdl"));

      ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
      ent->v.renderamt = 127; // set its transparency amount
      ent->v.effects |= EF_NODRAW;
   }
   else if (strcmp (entityClassname, "info_player_deathmatch") == 0)
   {
      SET_MODEL (ent, ENGINE_STR ("models/player/terror/terror.mdl"));

      ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
      ent->v.renderamt = 127; // set its transparency amount
      ent->v.effects |= EF_NODRAW;
   }

   else if (strcmp (entityClassname, "info_vip_start") == 0)
   {
      SET_MODEL (ent, ENGINE_STR ("models/player/vip/vip.mdl"));

      ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
      ent->v.renderamt = 127; // set its transparency amount
      ent->v.effects |= EF_NODRAW;
   }
#endif
   else if (strcmp (entityClassname, "func_vip_safetyzone") == 0 || strcmp (STRING (ent->v.classname), "info_vip_safetyzone") == 0)
      g_mapType |= MAP_AS; // assassination map

   else if (strcmp (entityClassname, "hostage_entity") == 0)
      g_mapType |= MAP_CS; // rescue map

   else if (strcmp (entityClassname, "func_bomb_target") == 0 || strcmp (STRING (ent->v.classname), "info_bomb_target") == 0)
      g_mapType |= MAP_DE; // defusion map

   else if (strcmp (entityClassname, "func_escapezone") == 0)
      g_mapType |= MAP_ES;

   // next maps doesn't have map-specific entities, so determine it by name
   else if (strncmp (engine.GetMapName (), "fy_", 3) == 0) // fun map
      g_mapType |= MAP_FY;
   else if (strncmp (engine.GetMapName (), "ka_", 3) == 0) // knife arena map
      g_mapType |= MAP_KA;

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, 0);

   int result = (*g_functionTable.pfnSpawn) (ent); // get result

   if (ent->v.rendermode == kRenderTransTexture)
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents

   return result;
}

void UpdateClientData (const struct edict_s *ent, int sendweapons, struct clientdata_s *cd)
{
   extern ConVar yb_latency_display;

   if (!(g_gameFlags & GAME_LEGACY) && yb_latency_display.GetInt () == 2)
      bots.SendPingDataOffsets (const_cast <edict_t *> (ent));

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnUpdateClientData) (ent, sendweapons, cd);
}

int ClientConnect (edict_t *ent, const char *name, const char *addr, char rejectReason[128])
{
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
   if (strcmp (addr, "loopback") == 0)
      g_hostEntity = ent; // save the edict of the listen server client...

   bots.AdjustQuota (true, ent);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, 0);

   return (*g_functionTable.pfnClientConnect) (ent, name, addr, rejectReason);
}

void ClientDisconnect (edict_t *ent)
{
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

   bots.AdjustQuota (false, ent);

   int i = engine.IndexOfEntity (ent) - 1;

   InternalAssert (i >= 0 && i < MAX_ENGINE_PLAYERS);

   Bot *bot = bots.GetBot (i);

   // check if its a bot
   if (bot != nullptr)
   {
      if (bot->pev == &ent->v)
      {
         bot->EnableChatterIcon (false);
         bot->ReleaseUsedName ();

         bots.Free (i);
      }
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnClientDisconnect) (ent);
}

void ClientUserInfoChanged (edict_t *ent, char *infobuffer)
{
   // this function is called when a player changes model, or changes team. Occasionally it
   // enforces rules on these changes (for example, some MODs don't want to allow players to
   // change their player model). But most commonly, this function is in charge of handling
   // team changes, recounting the teams population, etc...

   if (engine.IsDedicatedServer () && !IsValidBot (ent))
   {
      const char *passwordField = yb_password_key.GetString ();
      const char *password = yb_password.GetString ();

      if (!IsNullString (passwordField) || !IsNullString (password))
      {
         int clientIndex = engine.IndexOfEntity (ent) - 1;

         if (strcmp (password, INFOKEY_VALUE (infobuffer, const_cast <char *> (passwordField))) == 0)
            g_clients[clientIndex].flags |= CF_ADMIN;
         else
            g_clients[clientIndex].flags &= ~CF_ADMIN;
      }
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnClientUserInfoChanged) (ent, infobuffer);
}

void ClientCommand (edict_t *ent)
{
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

   const char *command = CMD_ARGV (0);
   const char *arg1 = CMD_ARGV (1);

   static int fillServerTeam = 5;
   static bool fillCommand = false;

   int issuerPlayerIndex = engine.IndexOfEntity (ent) - 1;

   if (!engine.IsBotCommand () && (ent == g_hostEntity || (g_clients[issuerPlayerIndex].flags & CF_ADMIN)))
   {
      if (stricmp (command, "yapb") == 0 || stricmp (command, "yb") == 0)
      {
         int state = BotCommandHandler (ent, IsNullString (arg1) ? "help" : arg1, CMD_ARGV (2), CMD_ARGV (3), CMD_ARGV (4), CMD_ARGV (5), CMD_ARGV (6), CMD_ARGV (0));

         switch (state)
         {
         case 0:
            engine.ClientPrintf (ent, "Unknown command: %s", arg1);
            break;

         case 2:
            engine.ClientPrintf (ent, "Command %s, can only be executed from server console.", arg1);
            break;
         }
         if (g_gameFlags & GAME_METAMOD)
            RETURN_META (MRES_SUPERCEDE);

         return;
      }
      else if (stricmp (command, "menuselect") == 0 && !IsNullString (arg1) && g_clients[issuerPlayerIndex].menu != BOT_MENU_IVALID)
      {
         Client *client = &g_clients[issuerPlayerIndex];
         int selection = atoi (arg1);

         if (client->menu == BOT_MENU_WAYPOINT_TYPE)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               waypoints.Add (selection - 1);
               break;

            case 8:
               waypoints.Add (100);
               break;

            case 9:
               waypoints.SetLearnJumpWaypoint ();
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_FLAG)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               waypoints.ToggleFlags (FLAG_NOHOSTAGE);
               break;

            case 2:
               waypoints.ToggleFlags (FLAG_TF_ONLY);
               break;

            case 3:
               waypoints.ToggleFlags (FLAG_CF_ONLY);
               break;

            case 4:
               waypoints.ToggleFlags (FLAG_LIFT);
               break;

            case 5:
               waypoints.ToggleFlags (FLAG_SNIPER);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_MAIN_PAGE1)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               if (g_waypointOn)
                  engine.IssueCmd ("yapb waypoint off");
               else
                  engine.IssueCmd ("yapb waypoint on");
               break;

            case 2:
               g_waypointOn = true;
               waypoints.CacheWaypoint ();
               break;

            case 3:
               g_waypointOn = true;
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_PATH);
               break;

            case 4:
               g_waypointOn = true;
               waypoints.DeletePath ();
               break;

            case 5:
               g_waypointOn = true;
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_TYPE);
               break;

            case 6:
               g_waypointOn = true;
               waypoints.Delete ();
               break;

            case 7:
               g_waypointOn = true;
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_AUTOPATH);
               break;

            case 8:
               g_waypointOn = true;
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_RADIUS);
               break;

            case 9:
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_MAIN_PAGE2);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_MAIN_PAGE2)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               {
                  int terrPoints = 0;
                  int ctPoints = 0;
                  int goalPoints = 0;
                  int rescuePoints = 0;
                  int campPoints = 0;
                  int sniperPoints = 0;
                  int noHostagePoints = 0;

                  for (int i = 0; i < g_numWaypoints; i++)
                  {
                     Path *path = waypoints.GetPath (i);

                     if (path->flags & FLAG_TF_ONLY)
                        terrPoints++;

                     if (path->flags & FLAG_CF_ONLY)
                        ctPoints++;

                     if (path->flags & FLAG_GOAL)
                        goalPoints++;

                     if (path->flags & FLAG_RESCUE)
                        rescuePoints++;

                     if (path->flags & FLAG_CAMP)
                        campPoints++;

                     if (path->flags & FLAG_SNIPER)
                        sniperPoints++;

                     if (path->flags & FLAG_NOHOSTAGE)
                        noHostagePoints++;
                  }
                  engine.Printf ("Waypoints: %d - T Points: %d\n"
                                    "CT Points: %d - Goal Points: %d\n"
                                    "Rescue Points: %d - Camp Points: %d\n"
                                    "Block Hostage Points: %d - Sniper Points: %d\n", g_numWaypoints, terrPoints, ctPoints, goalPoints, rescuePoints, campPoints, noHostagePoints, sniperPoints);
               }
               break;

            case 2:
               g_waypointOn = true;
               g_autoWaypoint &= 1;
               g_autoWaypoint ^= 1;

               engine.CenterPrintf ("Auto-Waypoint %s", g_autoWaypoint ? "Enabled" : "Disabled");
               break;

            case 3:
               g_waypointOn = true;
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_FLAG);
               break;

            case 4:
               if (waypoints.NodesValid ())
                  waypoints.Save ();
               else
                  engine.CenterPrintf ("Waypoint not saved\nThere are errors, see console");
               break;

            case 5:
               waypoints.Save ();
               break;

            case 6:
               waypoints.Load ();
               break;

            case 7:
               if (waypoints.NodesValid ())
                  engine.CenterPrintf ("Nodes works fine");
               else
                  engine.CenterPrintf ("There are errors, see console");
               break;

            case 8:
               engine.IssueCmd ("yapb wp on noclip");
               break;

            case 9:
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_RADIUS)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            g_waypointOn = true;  // turn waypoints on in case

            const int radiusValue[] = {0, 8, 16, 32, 48, 64, 80, 96, 128};

            if ((selection >= 1) && (selection <= 9))
               waypoints.SetRadius (radiusValue[selection - 1]);

            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_MAIN)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               fillCommand = false;
               DisplayMenuToClient (ent, BOT_MENU_CONTROL);
               break;

            case 2:
               DisplayMenuToClient (ent, BOT_MENU_FEATURES);
               break;

            case 3:
               fillCommand = true;
               DisplayMenuToClient (ent, BOT_MENU_TEAM_SELECT);
               break;

            case 4:
               bots.KillAll ();
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;

            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_CONTROL)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               bots.AddRandom ();
               break;

            case 2:
               DisplayMenuToClient (ent, BOT_MENU_DIFFICULTY);
               break;

            case 3:
               bots.RemoveRandom ();
               break;

            case 4:
               bots.RemoveAll ();
               break;

            case 5:
               bots.RemoveMenu (ent, 1);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_FEATURES)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               DisplayMenuToClient (ent, BOT_MENU_WEAPON_MODE);
               break;

            case 2:
               DisplayMenuToClient (ent, BOT_MENU_WAYPOINT_MAIN_PAGE1);
               break;

            case 3:
               DisplayMenuToClient (ent, BOT_MENU_PERSONALITY);
               break;

            case 4:
               extern ConVar yb_debug;
               yb_debug.SetInt (yb_debug.GetInt () ^ 1);

               break;

            case 5:
               if (IsAlive (ent))
                  DisplayMenuToClient (ent, BOT_MENU_COMMANDS);
               else
               {
                  DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display
                  engine.CenterPrintf ("You're dead, and have no access to this menu");
               }
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_COMMANDS)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display
            Bot *bot = nullptr;

            switch (selection)
            {
            case 1:
            case 2:
               if (FindNearestPlayer (reinterpret_cast <void **> (&bot), client->ent, 300.0f, true, true, true))
               {
                  if (!bot->m_hasC4 && !bot->HasHostage () )
                  {
                     if (selection == 1)
                     {
                        bot->ResetDoubleJumpState ();

                        bot->m_doubleJumpOrigin = client->ent->v.origin;
                        bot->m_doubleJumpEntity = client->ent;

                        bot->PushTask (TASK_DOUBLEJUMP, TASKPRI_DOUBLEJUMP, -1, engine.Time (), true);
                        bot->TeamSayText (FormatBuffer ("Ok %s, i will help you!", STRING (ent->v.netname)));
                     }
                     else if (selection == 2)
                        bot->ResetDoubleJumpState ();

                     break;
                  }
               }
               break;

            case 3:
            case 4:
               if (FindNearestPlayer (reinterpret_cast <void **> (&bot), ent, 300.0f, true, true, true))
                  bot->DiscardWeaponForUser (ent, selection == 4 ? false : true);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu ==BOT_MENU_WAYPOINT_AUTOPATH)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            const float autoDistanceValue[] = {0.0f, 100.0f, 130.0f, 160.0f, 190.0f, 220.0f, 250.0f };

            if (selection >= 1 && selection <= 7)
               g_autoPathDistance = autoDistanceValue[selection - 1];

            if (g_autoPathDistance == 0.0f)
               engine.CenterPrintf ("AutoPath disabled");
            else
               engine.CenterPrintf ("AutoPath maximum distance set to %.2f", g_autoPathDistance);

            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WAYPOINT_PATH)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
               waypoints.CreatePath (CONNECTION_OUTGOING);
               break;

            case 2:
               waypoints.CreatePath (CONNECTION_INCOMING);
               break;

            case 3:
               waypoints.CreatePath (CONNECTION_BOTHWAYS);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }

            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_DIFFICULTY)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            client->menu = BOT_MENU_PERSONALITY;

            switch (selection)
            {
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
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }

            if (client->menu == BOT_MENU_PERSONALITY)
               DisplayMenuToClient (ent, BOT_MENU_PERSONALITY);

            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_TEAM_SELECT && fillCommand)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
               // turn off cvars if specified team
               CVAR_SET_STRING ("mp_limitteams", "0");
               CVAR_SET_STRING ("mp_autoteambalance", "0");

            case 5:
               fillServerTeam = selection;
               DisplayMenuToClient (ent, BOT_MENU_DIFFICULTY);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_PERSONALITY && fillCommand)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
               bots.FillServer (fillServerTeam, selection - 2, g_storeAddbotVars[0]);

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_TEAM_SELECT)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 5:
               g_storeAddbotVars[1] = selection;
               if (selection == 5)
               {
                  g_storeAddbotVars[2] = 5;
                  bots.AddBot ("", g_storeAddbotVars[0], g_storeAddbotVars[3], g_storeAddbotVars[1], g_storeAddbotVars[2]);
               }
               else
               {
                  if (selection == 1)
                     DisplayMenuToClient (ent, BOT_MENU_TERRORIST_SELECT);
                  else
                     DisplayMenuToClient (ent, BOT_MENU_CT_SELECT);
               }
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_PERSONALITY)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
               g_storeAddbotVars[3] = selection - 2;
               DisplayMenuToClient (ent, BOT_MENU_TEAM_SELECT);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_TERRORIST_SELECT || client->menu == BOT_MENU_CT_SELECT)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
               g_storeAddbotVars[2] = selection;
               bots.AddBot ("", g_storeAddbotVars[0], g_storeAddbotVars[3], g_storeAddbotVars[1], g_storeAddbotVars[2]);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_WEAPON_MODE)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
               bots.SetWeaponMode (selection);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_IVALID);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_1)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.GetBot (selection - 1)->Kick ();
               break;

            case 9:
               bots.RemoveMenu (ent, 2);
               break;

            case 10:
               DisplayMenuToClient (ent, BOT_MENU_CONTROL);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_2)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.GetBot (selection + 8 - 1)->Kick ();
               break;

            case 9:
               bots.RemoveMenu (ent, 3);
               break;

            case 10:
               bots.RemoveMenu (ent, 1);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_3)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.GetBot (selection + 16 - 1)->Kick ();
               break;

            case 9:
               bots.RemoveMenu (ent, 4);
               break;

            case 10:
               bots.RemoveMenu (ent, 2);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
         else if (client->menu == BOT_MENU_KICK_PAGE_4)
         {
            DisplayMenuToClient (ent, BOT_MENU_IVALID); // reset menu display

            switch (selection)
            {
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 6:
            case 7:
            case 8:
               bots.GetBot (selection + 24 - 1)->Kick ();
               break;

            case 10:
               bots.RemoveMenu (ent, 3);
               break;
            }
            if (g_gameFlags & GAME_METAMOD)
               RETURN_META (MRES_SUPERCEDE);

            return;
         }
      }
   }

   if (!engine.IsBotCommand () && (stricmp (command, "say") == 0 || stricmp (command, "say_team") == 0))
   {
      Bot *bot = nullptr;

      if (FStrEq (arg1, "dropme") || FStrEq (arg1, "dropc4"))
      {
         if (FindNearestPlayer (reinterpret_cast <void **> (&bot), ent, 300.0f, true, true, true))
            bot->DiscardWeaponForUser (ent, IsNullString (strstr (arg1, "c4")) ? false : true);

         return;
      }

      bool isAlive = IsAlive (ent);
      int team = -1;

      if (FStrEq (command, "say_team"))
         team = engine.GetTeam (ent);

      for (int i = 0; i < engine.MaxClients (); i++)
      {
         const Client &client = g_clients[i];

         if (!(client.flags & CF_USED) || (team != -1 && team != client.team) || isAlive != IsAlive (client.ent))
            continue;

         Bot *target = bots.GetBot (i);

         if (target != nullptr)
         {
            target->m_sayTextBuffer.entityIndex = engine.IndexOfEntity (ent);

            if (IsNullString (CMD_ARGS ()))
               continue;

            strncpy (target->m_sayTextBuffer.sayText, CMD_ARGS (), SIZEOF_CHAR (target->m_sayTextBuffer.sayText));
            target->m_sayTextBuffer.timeNextChat = engine.Time () + target->m_sayTextBuffer.chatDelay;
         }
      }
   }

   
   int clientIndex = engine.IndexOfEntity (ent) - 1;
   const Client &radioTarget = g_clients[clientIndex];

   // check if this player alive, and issue something
   if ((radioTarget.flags & CF_ALIVE) && g_radioSelect[clientIndex] != 0 && strncmp (command, "menuselect", 10) == 0)
   {
      int radioCommand = atoi (arg1);

      if (radioCommand != 0)
      {
         radioCommand += 10 * (g_radioSelect[clientIndex] - 1);

         if (radioCommand != Radio_Affirmative && radioCommand != Radio_Negative && radioCommand != Radio_ReportingIn)
         {
            for (int i = 0; i < engine.MaxClients (); i++)
            {
               Bot *bot = bots.GetBot (i);

               // validate bot
               if (bot != nullptr && bot->m_team == radioTarget.team && ent != bot->GetEntity () && bot->m_radioOrder == 0)
               {
                  bot->m_radioOrder = radioCommand;
                  bot->m_radioEntity = ent;
               }
            }
         }
         g_lastRadioTime[radioTarget.team] = engine.Time ();
      }
      g_radioSelect[clientIndex] = 0;
   }
   else if (strncmp (command, "radio", 5) == 0)
      g_radioSelect[clientIndex] = atoi (&command[5]);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnClientCommand) (ent);
}

void ServerActivate (edict_t *pentEdictList, int edictCount, int clientMax)
{
   // this function is called when the server has fully loaded and is about to manifest itself
   // on the network as such. Since a mapchange is actually a server shutdown followed by a
   // restart, this function is also called when a new map is being loaded. Hence it's the
   // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
   // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
   // Once this function has been called, the server can be considered as "running".

   FreeLibraryMemory ();
   InitConfig (); // initialize all config files

   // do level initialization stuff here...
   waypoints.Init ();
   waypoints.Load ();

   // create global killer entity
   bots.CreateKillerEntity ();

   // execute main config
   engine.IssueCmd ("exec addons/yapb/conf/yapb.cfg");

   if (File::Accessible (FormatBuffer ("%s/maps/%s_yapb.cfg", engine.GetModName (), engine.GetMapName ())))
   {
      engine.IssueCmd ("exec maps/%s_yapb.cfg", engine.GetMapName ());
      engine.Printf ("Executing Map-Specific config file");
   }
   bots.InitQuota ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnServerActivate) (pentEdictList, edictCount, clientMax);

   waypoints.InitializeVisibility ();
}

void ServerDeactivate (void)
{
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
   waypoints.SaveExperienceTab ();
   waypoints.SaveVisibilityTab ();

   // destroy global killer entity
   bots.DestroyKillerEntity ();

   FreeLibraryMemory ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnServerDeactivate) ();
}

void StartFrame (void)
{
   // this function starts a video frame. It is called once per video frame by the engine. If
   // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
   // placing a hook on it, we have a good place to do things that should be done continuously
   // during the game, for example making the bots think (yes, because no Think() function exists
   // for the bots by the MOD side, remember). Also here we have control on the bot population,
   // for example if a new player joins the server, we should disconnect a bot, and if the
   // player population decreases, we should fill the server with other bots.

   // run periodic update of bot states
   bots.PeriodicThink ();

   // record some stats of all players on the server
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      edict_t *player = engine.EntityOfIndex (i + 1);
      Client &storeClient = g_clients[i];

      if (!engine.IsNullEntity (player) && (player->v.flags & FL_CLIENT))
      {
         storeClient.ent = player;
         storeClient.flags |= CF_USED;

         if (IsAlive (player))
            storeClient.flags |= CF_ALIVE;
         else
            storeClient.flags &= ~CF_ALIVE;

         if (storeClient.flags & CF_ALIVE)
         {
            // keep the clipping mode enabled, or it can be turned off after new round has started
            if (g_hostEntity == player && g_editNoclip)
               g_hostEntity->v.movetype = MOVETYPE_NOCLIP;

            storeClient.origin = player->v.origin;
            SoundSimulateUpdate (i);
         }
      }
      else
      {
         storeClient.flags &= ~(CF_USED | CF_ALIVE);
         storeClient.ent = nullptr;
      }
   }

   if (!engine.IsDedicatedServer () && !engine.IsNullEntity (g_hostEntity))
   {
      if (g_waypointOn)
         waypoints.Think ();

      CheckWelcomeMessage ();
   }
   bots.SetDeathMsgState (false);

   if (g_timePerSecondUpdate < engine.Time ())
   {
      for (int i = 0; i < engine.MaxClients (); i++)
      {
         edict_t *player = engine.EntityOfIndex (i + 1);

         // code below is executed only on dedicated server
         if (engine.IsDedicatedServer () && !engine.IsNullEntity (player) && (player->v.flags & FL_CLIENT) && !(player->v.flags & FL_FAKECLIENT))
         {
            Client &client = g_clients[i];

            if (client.flags & CF_ADMIN)
            {
               if (IsNullString (yb_password_key.GetString ()) && IsNullString (yb_password.GetString ()))
                  client.flags &= ~CF_ADMIN;
               else if (strcmp (yb_password.GetString (), INFOKEY_VALUE (GET_INFOKEYBUFFER (client.ent), const_cast <char *> (yb_password_key.GetString ()))))
               {
                  client.flags &= ~CF_ADMIN;
                  engine.Printf ("Player %s had lost remote access to yapb.", STRING (player->v.netname));
               }
            }
            else if (!(client.flags & CF_ADMIN) && !IsNullString (yb_password_key.GetString ()) && !IsNullString (yb_password.GetString ()))
            {
               if (strcmp (yb_password.GetString (), INFOKEY_VALUE (GET_INFOKEYBUFFER (client.ent), const_cast <char *> (yb_password_key.GetString ()))) == 0)
               {
                  client.flags |= CF_ADMIN;
                  engine.Printf ("Player %s had gained full remote access to yapb.", STRING (player->v.netname));
               }
            }
         }
      }
      bots.CalculatePingOffsets ();

      // select the leader each team
      for (int team = TERRORIST; team < SPECTATOR; team++)
         bots.SelectLeaderEachTeam (team, false);

      if (g_gameFlags & GAME_METAMOD)
      {
         static cvar_t *csdm_active;
         static cvar_t *mp_freeforall;

         if (csdm_active == nullptr)
            csdm_active = CVAR_GET_POINTER ("csdm_active");

         if (mp_freeforall == nullptr)
            mp_freeforall = CVAR_GET_POINTER ("mp_freeforall");

         if (csdm_active != nullptr && csdm_active->value > 0)
            yb_csdm_mode.SetInt (mp_freeforall != nullptr && mp_freeforall->value > 0 ? 2 : 1);
      }
      g_timePerSecondUpdate = engine.Time () + 1.0f;
   }

   // keep track of grenades on map
   bots.UpdateActiveGrenades ();

   // keep bot number up to date
   bots.MaintainBotQuota ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_functionTable.pfnStartFrame) ();

   // **** AI EXECUTION STARTS ****
   bots.Think ();
   // **** AI EXECUTION FINISH ****
}

void StartFrame_Post (void)
{
   // this function starts a video frame. It is called once per video frame by the engine. If
   // you run Half-Life at 90 fps, this function will then be called 90 times per second. By
   // placing a hook on it, we have a good place to do things that should be done continuously
   // during the game, for example making the bots think (yes, because no Think() function exists
   // for the bots by the MOD side, remember).  Post version called only by metamod.

   // **** AI EXECUTION STARTS ****
   bots.Think ();
   // **** AI EXECUTION FINISH ****

   RETURN_META (MRES_IGNORED);
}

int Spawn_Post (edict_t *ent)
{
   // this function asks the game DLL to spawn (i.e, give a physical existence in the virtual
   // world, in other words to 'display') the entity pointed to by ent in the game. The
   // Spawn() function is one of the functions any entity is supposed to have in the game DLL,
   // and any MOD is supposed to implement one for each of its entities. Post version called
   // only by metamod.

   // solves the bots unable to see through certain types of glass bug.
   if (ent->v.rendermode == kRenderTransTexture)
      ent->v.flags &= ~FL_WORLDBRUSH; // clear the FL_WORLDBRUSH flag out of transparent ents

   RETURN_META_VALUE (MRES_IGNORED, 0);
}

void ServerActivate_Post (edict_t *, int, int)
{
   // this function is called when the server has fully loaded and is about to manifest itself
   // on the network as such. Since a mapchange is actually a server shutdown followed by a
   // restart, this function is also called when a new map is being loaded. Hence it's the
   // perfect place for doing initialization stuff for our bots, such as reading the BSP data,
   // loading the bot profiles, and drawing the world map (ie, filling the navigation hashtable).
   // Once this function has been called, the server can be considered as "running". Post version
   // called only by metamod.

   waypoints.InitializeVisibility ();

   RETURN_META (MRES_IGNORED);
}

void pfnChangeLevel (char *s1, char *s2)
{
   // the purpose of this function is to ask the engine to shutdown the server and restart a
   // new one running the map whose name is s1. It is used ONLY IN SINGLE PLAYER MODE and is
   // transparent to the user, because it saves the player state and equipment and restores it
   // back in the new level. The "changelevel trigger point" in the old level is linked to the
   // new level's spawn point using the s2 string, which is formatted as follows: "trigger_name
   // to spawnpoint_name", without spaces (for example, "tr_1atotr_2lm" would tell the engine
   // the player has reached the trigger point "tr_1a" and has to spawn in the next level on the
   // spawn point named "tr_2lm".

   // save collected experience on map change
   waypoints.SaveExperienceTab ();
   waypoints.SaveVisibilityTab ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   CHANGE_LEVEL (s1, s2);
}

edict_t *pfnFindEntityByString (edict_t *edictStartSearchAfter, const char *field, const char *value)
{
   // round starts in counter-strike 1.5
   if (strcmp (value, "info_map_parameters") == 0)
      RoundInit ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, 0);

   return FIND_ENTITY_BY_STRING (edictStartSearchAfter, field, value);
}

void pfnEmitSound (edict_t *entity, int channel, const char *sample, float volume, float attenuation, int flags, int pitch)
{
   // this function tells the engine that the entity pointed to by "entity", is emitting a sound
   // which fileName is "sample", at level "channel" (CHAN_VOICE, etc...), with "volume" as
   // loudness multiplicator (normal volume VOL_NORM is 1.0), with a pitch of "pitch" (normal
   // pitch PITCH_NORM is 100.0), and that this sound has to be attenuated by distance in air
   // according to the value of "attenuation" (normal attenuation ATTN_NORM is 0.8 ; ATTN_NONE
   // means no attenuation with distance). Optionally flags "fFlags" can be passed, which I don't
   // know the heck of the purpose. After we tell the engine to emit the sound, we have to call
   // SoundAttachToThreat() to bring the sound to the ears of the bots. Since bots have no client DLL
   // to handle this for them, such a job has to be done manually.

   SoundAttachToClients (entity, sample, volume);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_engfuncs.pfnEmitSound) (entity, channel, sample, volume, attenuation, flags, pitch);
}

void pfnClientCommand (edict_t *ent, char const *format, ...)
{
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
   _vsnprintf (buffer, SIZEOF_CHAR (buffer), format, ap);
   va_end (ap);

   // is the target entity an official bot, or a third party bot ?
   if (ent->v.flags & FL_FAKECLIENT)
   {
      if (g_gameFlags & GAME_METAMOD)
         RETURN_META (MRES_SUPERCEDE); // prevent bots to be forced to issue client commands

      return;
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   CLIENT_COMMAND (ent, buffer);
}

void pfnMessageBegin (int msgDest, int msgType, const float *origin, edict_t *ed)
{
   // this function called each time a message is about to sent.

   // store the message type in our own variables, since the GET_USER_MSG_ID () will just do a lot of strcmp()'s...
   if ((g_gameFlags & GAME_METAMOD) && engine.FindMessageId (NETMSG_MONEY) == -1)
   {
      engine.AssignMessageId (NETMSG_VGUI, GET_USER_MSG_ID (PLID, "VGUIMenu", nullptr));
      engine.AssignMessageId (NETMSG_SHOWMENU, GET_USER_MSG_ID (PLID, "ShowMenu", nullptr));
      engine.AssignMessageId (NETMSG_WEAPONLIST, GET_USER_MSG_ID (PLID, "WeaponList", nullptr));
      engine.AssignMessageId (NETMSG_CURWEAPON, GET_USER_MSG_ID (PLID, "CurWeapon", nullptr));
      engine.AssignMessageId (NETMSG_AMMOX, GET_USER_MSG_ID (PLID, "AmmoX", nullptr));
      engine.AssignMessageId (NETMSG_AMMOPICKUP, GET_USER_MSG_ID (PLID, "AmmoPickup", nullptr));
      engine.AssignMessageId (NETMSG_DAMAGE, GET_USER_MSG_ID (PLID, "Damage", nullptr));
      engine.AssignMessageId (NETMSG_MONEY, GET_USER_MSG_ID (PLID, "Money", nullptr));
      engine.AssignMessageId (NETMSG_STATUSICON, GET_USER_MSG_ID (PLID, "StatusIcon", nullptr));
      engine.AssignMessageId (NETMSG_DEATH, GET_USER_MSG_ID (PLID, "DeathMsg", nullptr));
      engine.AssignMessageId (NETMSG_SCREENFADE, GET_USER_MSG_ID (PLID, "ScreenFade", nullptr));
      engine.AssignMessageId (NETMSG_HLTV, GET_USER_MSG_ID (PLID, "HLTV", nullptr));
      engine.AssignMessageId (NETMSG_TEXTMSG, GET_USER_MSG_ID (PLID, "TextMsg", nullptr));
      engine.AssignMessageId (NETMSG_SCOREINFO, GET_USER_MSG_ID (PLID, "ScoreInfo", nullptr));
      engine.AssignMessageId (NETMSG_BARTIME, GET_USER_MSG_ID (PLID, "BarTime", nullptr));
      engine.AssignMessageId (NETMSG_SENDAUDIO, GET_USER_MSG_ID (PLID, "SendAudio", nullptr));
      engine.AssignMessageId (NETMSG_SAYTEXT, GET_USER_MSG_ID (PLID, "SayText", nullptr));

      if (!(g_gameFlags & GAME_LEGACY))
         engine.AssignMessageId (NETMSG_BOTVOICE, GET_USER_MSG_ID (PLID, "BotVoice", nullptr));
   }
   engine.ResetMessageCapture ();

   if ((!(g_gameFlags & GAME_LEGACY) || (g_gameFlags & GAME_XASH)) && msgDest == MSG_SPEC && msgType == engine.FindMessageId (NETMSG_HLTV))
      engine.SetOngoingMessageId (NETMSG_HLTV);

   engine.TryCaptureMessage (msgType, NETMSG_WEAPONLIST);

   if (!engine.IsNullEntity (ed))
   {
      int index = bots.GetIndex (ed);

      // is this message for a bot?
      if (index != -1 && !(ed->v.flags & FL_DORMANT))
      {
         engine.SetOngoingMessageReceiver (index);

         // message handling is done in usermsg.cpp
         engine.TryCaptureMessage (msgType, NETMSG_VGUI);
         engine.TryCaptureMessage (msgType, NETMSG_CURWEAPON);
         engine.TryCaptureMessage (msgType, NETMSG_AMMOX);
         engine.TryCaptureMessage (msgType, NETMSG_AMMOPICKUP);
         engine.TryCaptureMessage (msgType, NETMSG_DAMAGE);
         engine.TryCaptureMessage (msgType, NETMSG_MONEY);
         engine.TryCaptureMessage (msgType, NETMSG_STATUSICON);
         engine.TryCaptureMessage (msgType, NETMSG_SCREENFADE);
         engine.TryCaptureMessage (msgType, NETMSG_BARTIME);
         engine.TryCaptureMessage (msgType, NETMSG_TEXTMSG);
         engine.TryCaptureMessage (msgType, NETMSG_SHOWMENU);
      }
   }
   else if (msgDest == MSG_ALL)
   {
      engine.TryCaptureMessage (msgType, NETMSG_SCOREINFO);
      engine.TryCaptureMessage (msgType, NETMSG_DEATH);
      engine.TryCaptureMessage (msgType, NETMSG_TEXTMSG);

      if (msgType == SVC_INTERMISSION)
      {
         for (int i = 0; i < engine.MaxClients (); i++)
         {
            Bot *bot = bots.GetBot (i);

            if (bot != nullptr)
               bot->m_notKilled = false;
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   MESSAGE_BEGIN (msgDest, msgType, origin, ed);
}

void pfnMessageEnd (void)
{
   engine.ResetMessageCapture ();

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   MESSAGE_END ();

   // send latency fix
   bots.SendDeathMsgFix ();
}

void pfnMessageEnd_Post (void)
{
   // send latency fix
   bots.SendDeathMsgFix ();

   RETURN_META (MRES_IGNORED);
}

void pfnWriteByte (int value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_BYTE (value);
}

void pfnWriteChar (int value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_CHAR (value);
}

void pfnWriteShort (int value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_SHORT (value);
}

void pfnWriteLong (int value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_LONG (value);
}

void pfnWriteAngle (float value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_ANGLE (value);
}

void pfnWriteCoord (float value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_COORD (value);
}

void pfnWriteString (const char *sz)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) sz);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_STRING (sz);
}

void pfnWriteEntity (int value)
{
   // if this message is for a bot, call the client message function...
   engine.ProcessMessageCapture ((void *) &value);

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   WRITE_ENTITY (value);
}

int pfnCmd_Argc (void)
{
   // this function returns the number of arguments the current client command string has. Since
   // bots have no client DLL and we may want a bot to execute a client command, we had to
   // implement a g_xgv string in the bot DLL for holding the bots' commands, and also keep
   // track of the argument count. Hence this hook not to let the engine ask an unexistent client
   // DLL for a command we are holding here. Of course, real clients commands are still retrieved
   // the normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.IsBotCommand ())
   {
      if (g_gameFlags & GAME_METAMOD)
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.GetOverrideArgc ());

      return engine.GetOverrideArgc (); // if so, then return the argument count we know
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, 0);

   return CMD_ARGC (); // ask the engine how many arguments there are
}

const char *pfnCmd_Args (void)
{
   // this function returns a pointer to the whole current client command string. Since bots
   // have no client DLL and we may want a bot to execute a client command, we had to implement
   // a g_xgv string in the bot DLL for holding the bots' commands, and also keep track of the
   // argument count. Hence this hook not to let the engine ask an unexistent client DLL for a
   // command we are holding here. Of course, real clients commands are still retrieved the
   // normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.IsBotCommand ())
   {
      if (g_gameFlags & GAME_METAMOD)
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.GetOverrideArgs ());

      return engine.GetOverrideArgs (); // else return the whole bot client command string we know
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, nullptr);

   return CMD_ARGS (); // ask the client command string to the engine
}

const char *pfnCmd_Argv (int argc)
{
   // this function returns a pointer to a certain argument of the current client command. Since
   // bots have no client DLL and we may want a bot to execute a client command, we had to
   // implement a g_xgv string in the bot DLL for holding the bots' commands, and also keep
   // track of the argument count. Hence this hook not to let the engine ask an unexistent client
   // DLL for a command we are holding here. Of course, real clients commands are still retrieved
   // the normal way, by asking the engine.

   // is this a bot issuing that client command?
   if (engine.IsBotCommand ())
   {
      if (g_gameFlags & GAME_METAMOD)
         RETURN_META_VALUE (MRES_SUPERCEDE, engine.GetOverrideArgv (argc));

      return engine.GetOverrideArgv (argc); // if so, then return the wanted argument we know
   }
   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, nullptr);

   return CMD_ARGV (argc); // ask the argument number "argc" to the engine
}

void pfnClientPrintf (edict_t *ent, PRINT_TYPE printType, const char *message)
{
   // this function prints the text message string pointed to by message by the client side of
   // the client entity pointed to by ent, in a manner depending of printType (print_console,
   // print_center or print_chat). Be certain never to try to feed a bot with this function,
   // as it will crash your server. Why would you, anyway ? bots have no client DLL as far as
   // we know, right ? But since stupidity rules this world, we do a preventive check :)

   if (IsValidBot (ent))
   {
      if (g_gameFlags & GAME_METAMOD)
         RETURN_META (MRES_SUPERCEDE);

      return;
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   CLIENT_PRINTF (ent, printType, message);
}

void pfnSetClientMaxspeed (const edict_t *ent, float newMaxspeed)
{
   Bot *bot = bots.GetBot (const_cast <edict_t *> (ent));

   // check wether it's not a bot
   if (bot != nullptr)
      bot->pev->maxspeed = newMaxspeed;

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_engfuncs.pfnSetClientMaxspeed) (ent, newMaxspeed);
}

int pfnRegUserMsg (const char *name, int size)
{
   // this function registers a "user message" by the engine side. User messages are network
   // messages the game DLL asks the engine to send to clients. Since many MODs have completely
   // different client features (Counter-Strike has a radar and a timer, for example), network
   // messages just can't be the same for every MOD. Hence here the MOD DLL tells the engine,
   // "Hey, you have to know that I use a network message whose name is pszName and it is size
   // packets long". The engine books it, and returns the ID number under which he recorded that
   // custom message. Thus every time the MOD DLL will be wanting to send a message named pszName
   // using pfnMessageBegin (), it will know what message ID number to send, and the engine will
   // know what to do, only for non-metamod version

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META_VALUE (MRES_IGNORED, 0);

   int message = REG_USER_MSG (name, size);

   if (strcmp (name, "VGUIMenu") == 0)
      engine.AssignMessageId (NETMSG_VGUI, message);
   else if (strcmp (name, "ShowMenu") == 0)
      engine.AssignMessageId (NETMSG_SHOWMENU, message);
   else if (strcmp (name, "WeaponList") == 0)
      engine.AssignMessageId (NETMSG_WEAPONLIST, message);
   else if (strcmp (name, "CurWeapon") == 0)
      engine.AssignMessageId (NETMSG_CURWEAPON, message);
   else if (strcmp (name, "AmmoX") == 0)
      engine.AssignMessageId (NETMSG_AMMOX, message);
   else if (strcmp (name, "AmmoPickup") == 0)
      engine.AssignMessageId (NETMSG_AMMOPICKUP, message);
   else if (strcmp (name, "Damage") == 0)
      engine.AssignMessageId (NETMSG_DAMAGE, message);
   else if (strcmp (name, "Money") == 0)
      engine.AssignMessageId (NETMSG_MONEY, message);
   else if (strcmp (name, "StatusIcon") == 0)
      engine.AssignMessageId (NETMSG_STATUSICON, message);
   else if (strcmp (name, "DeathMsg") == 0)
      engine.AssignMessageId (NETMSG_DEATH, message);
   else if (strcmp (name, "ScreenFade") == 0)
      engine.AssignMessageId (NETMSG_SCREENFADE, message);
   else if (strcmp (name, "HLTV") == 0)
      engine.AssignMessageId (NETMSG_HLTV, message);
   else if (strcmp (name, "TextMsg") == 0)
      engine.AssignMessageId (NETMSG_TEXTMSG, message);
   else if (strcmp (name, "ScoreInfo") == 0)
      engine.AssignMessageId (NETMSG_SCOREINFO, message);
   else if (strcmp (name, "BarTime") == 0)
      engine.AssignMessageId (NETMSG_BARTIME, message);
   else if (strcmp (name, "SendAudio") == 0)
      engine.AssignMessageId (NETMSG_SENDAUDIO, message);
   else if (strcmp (name, "SayText") == 0)
      engine.AssignMessageId (NETMSG_SAYTEXT, message);
   else if (strcmp (name, "BotVoice") == 0)
      engine.AssignMessageId (NETMSG_BOTVOICE, message);

   return message;
}

void pfnAlertMessage (ALERT_TYPE alertType, char *format, ...)
{
   va_list ap;
   char buffer[1024];

   va_start (ap, format);
   vsnprintf (buffer, SIZEOF_CHAR (buffer), format, ap);
   va_end (ap);

   if ((g_mapType & MAP_DE) && g_bombPlanted && strstr (buffer, "_Defuse_") != nullptr)
   {
      // notify all terrorists that CT is starting bomb defusing
      for (int i = 0; i < engine.MaxClients (); i++)
      {
         Bot *bot = bots.GetBot (i);

         if (bot != nullptr && bot->m_team == TERRORIST && bot->m_notKilled)
         {
            bot->DeleteSearchNodes ();

            bot->m_position = waypoints.GetBombPosition ();
            bot->PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);
         }
      }
   }

   if (g_gameFlags & GAME_METAMOD)
      RETURN_META (MRES_IGNORED);

   (*g_engfuncs.pfnAlertMessage) (alertType, buffer);
}

gamedll_funcs_t gameDLLFunc;

SHARED_LIBRARAY_EXPORT int GetEntityAPI2 (gamefuncs_t *functionTable, int *)
{
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

   if (!(g_gameFlags & GAME_METAMOD))
   {
      auto api_GetEntityAPI = g_gameLib->GetFuncAddr <EntityAPI_t> ("GetEntityAPI");

      // pass other DLLs engine callbacks to function table...
      if (api_GetEntityAPI (&g_functionTable, INTERFACE_VERSION) == 0)
      {
         AddLogEntry (true, LL_FATAL, "GetEntityAPI2: ERROR - Not Initialized.");
         return FALSE;  // error initializing function table!!!
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

SHARED_LIBRARAY_EXPORT int GetEntityAPI2_Post (gamefuncs_t *functionTable, int *)
{
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

SHARED_LIBRARAY_EXPORT int GetNewDLLFunctions (newgamefuncs_t *functionTable, int *interfaceVersion)
{
   // it appears that an extra function table has been added in the engine to gamedll interface
   // since the date where the first enginefuncs table standard was frozen. These ones are
   // facultative and we don't hook them, but since some MODs might be featuring it, we have to
   // pass them too, else the DLL interfacing wouldn't be complete and the game possibly wouldn't
   // run properly.

   auto api_GetNewDLLFunctions = g_gameLib->GetFuncAddr <NewEntityAPI_t> ("GetNewDLLFunctions");

   if (api_GetNewDLLFunctions == nullptr)
      return FALSE;

   if (!api_GetNewDLLFunctions (functionTable, interfaceVersion))
   {
      AddLogEntry (true, LL_FATAL, "GetNewDLLFunctions: ERROR - Not Initialized.");
      return FALSE;
   }

   gameDLLFunc.newapi_table = functionTable;
   return TRUE;
}

SHARED_LIBRARAY_EXPORT int GetEngineFunctions (enginefuncs_t *functionTable, int *)
{
   if (g_gameFlags & GAME_METAMOD)
      memset (functionTable, 0, sizeof (enginefuncs_t));
  
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

SHARED_LIBRARAY_EXPORT int GetEngineFunctions_Post (enginefuncs_t *functionTable, int *)
{
   memset (functionTable, 0, sizeof (enginefuncs_t));

   functionTable->pfnMessageEnd = pfnMessageEnd_Post;

   return TRUE;
}

SHARED_LIBRARAY_EXPORT int Server_GetBlendingInterface (int version, void **ppinterface, void *pstudio, float (*rotationmatrix)[3][4], float (*bonetransform)[128][3][4])
{
   // this function synchronizes the studio model animation blending interface (i.e, what parts
   // of the body move, which bones, which hitboxes and how) between the server and the game DLL.
   // some MODs can be using a different hitbox scheme than the standard one.

   auto api_GetBlendingInterface = g_gameLib->GetFuncAddr <BlendAPI_t> ("Server_GetBlendingInterface");

   if (api_GetBlendingInterface == nullptr)
      return FALSE;

   return api_GetBlendingInterface (version, ppinterface, pstudio, rotationmatrix, bonetransform);
}

SHARED_LIBRARAY_EXPORT int Meta_Query (char *, plugin_info_t **pPlugInfo, mutil_funcs_t *pMetaUtilFuncs)
{
   // this function is the first function ever called by metamod in the plugin DLL. Its purpose
   // is for metamod to retrieve basic information about the plugin, such as its meta-interface
   // version, for ensuring compatibility with the current version of the running metamod.

   gpMetaUtilFuncs = pMetaUtilFuncs;
   *pPlugInfo = &Plugin_info;

   return TRUE; // tell metamod this plugin looks safe
}

SHARED_LIBRARAY_EXPORT int Meta_Attach (PLUG_LOADTIME, metamod_funcs_t *functionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
   // this function is called when metamod attempts to load the plugin. Since it's the place
   // where we can tell if the plugin will be allowed to run or not, we wait until here to make
   // our initialization stuff, like registering CVARs and dedicated server commands.


   // metamod engine & dllapi function tables
   static metamod_funcs_t metamodFunctionTable =
   {
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

SHARED_LIBRARAY_EXPORT int Meta_Detach (PLUG_LOADTIME, PL_UNLOAD_REASON)
{
   // this function is called when metamod unloads the plugin. A basic check is made in order
   // to prevent unloading the plugin if its processing should not be interrupted.

   bots.RemoveAll (); // kick all bots off this server
   FreeLibraryMemory ();

   return TRUE;
}

SHARED_LIBRARAY_EXPORT void Meta_Init (void)
{
   // this function is called by metamod, before any other interface functions. Purpose of this
   // function to give plugin a chance to determine is plugin running under metamod or not.

   g_gameFlags |= GAME_METAMOD;
}

Library *LoadCSBinary (void)
{
   const char *modname = engine.GetModName ();

   if (!modname)
      return nullptr;

#if defined (PLATFORM_WIN32)
   const char *libs[] = { "mp.dll", "cs.dll" };
#elif defined (PLATFORM_LINUX)
   const char *libs[] = { "cs.so", "cs_i386.so" };
#elif defined (PLATFORM_OSX)
   const char *libs[] = { "cs.dylib" };
#endif

   // search the libraries inside game dlls directory
   for (int i = 0; i < ARRAYSIZE_HLSDK (libs); i++)
   {
      char path[256];
      sprintf (path, "%s/dlls/%s", modname, libs[i]);

      // if we can't read file, skip it
      if (!File::Accessible (path))
         continue;

      // special case, czero is always detected first, as it's has custom directory
      if (strcmp (modname, "czero") == 0)
      {
         g_gameFlags |= GAME_CZERO;

         if (g_gameFlags & GAME_METAMOD)
            return nullptr;

         return new Library (path);
      }
      else
      {
         Library *game = new Library (path);

         // try to load gamedll
         if (!game->IsLoaded ())
         {
            AddLogEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", libs[i], modname);
            return nullptr;
         }

         // detect xash engine
         if (g_engfuncs.pfnCVarGetPointer ("build") != nullptr)
         {
            g_gameFlags |= (GAME_LEGACY | GAME_XASH);

            if (g_gameFlags & GAME_METAMOD)
            {
               delete game;
               return nullptr;
            }
            return game;
         }

         // detect if we're running modern game
         EntityPtr_t entity = game->GetFuncAddr <EntityPtr_t> ("weapon_famas");

         if (entity != nullptr)
            g_gameFlags |= GAME_CSTRIKE16;
         else
            g_gameFlags |= GAME_LEGACY;

         if (g_gameFlags & GAME_METAMOD)
         {
            delete game;
            return nullptr;
         }
         return game;
      }
   }
   return nullptr;
}

DLL_GIVEFNPTRSTODLL GiveFnptrsToDll (enginefuncs_t *functionTable, globalvars_t *pGlobals)
{
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
   engine.PushRegisteredConVarsToEngine ();

   // ensure we're have all needed directories
   {
      const char *mod = engine.GetModName ();

      // create the needed paths
      File::CreatePath (const_cast <char *> (FormatBuffer ("%s/addons/yapb/conf/lang", mod)));
      File::CreatePath (const_cast <char *> (FormatBuffer ("%s/addons/yapb/data/learned", mod)));
   }

#ifdef PLATFORM_ANDROID
   g_gameFlags |= (GAME_LEGACY | GAME_XASH | GAME_MOBILITY);

   if (g_gameFlags & GAME_METAMOD)
      return;  // we should stop the attempt for loading the real gamedll, since metamod handle this for us

#ifdef LOAD_HARDFP
   const char *serverDLL = "libserver_hardfp.so";
#else
   const char *serverDLL = "libserver.so";
#endif

   char gameDLLName[256];
   snprintf (gameDLLName, SIZEOF_CHAR (gameDLLName), "%s/%s", getenv ("XASH3D_GAMELIBDIR"), serverDLL);

   g_gameLib = new Library (gameDLLName);

   if (!g_gameLib->IsLoaded ())
      AddLogEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", gameDLLName, engine.GetModName ());
#else
   g_gameLib = LoadCSBinary ();
   {
      if (g_gameLib == nullptr && !(g_gameFlags & GAME_METAMOD))
      {
         AddLogEntry (true, LL_FATAL | LL_IGNORE, "Mod that you has started, not supported by this bot (gamedir: %s)", engine.GetModName ());
         return;
      }

      // print game detection info
      String gameVersionStr;

      if (g_gameFlags & GAME_LEGACY)
         gameVersionStr.Assign ("Legacy");

      else if (g_gameFlags & GAME_CZERO)
         gameVersionStr.Assign ("Condition Zero");

      else if (g_gameFlags & GAME_CSTRIKE16)
         gameVersionStr.Assign ("v1.6");

      if (g_gameFlags & GAME_XASH)
      {
         gameVersionStr.Append (" @ Xash3D Engine");

         if (g_gameFlags & GAME_MOBILITY)
            gameVersionStr.Append (" Mobile");

         gameVersionStr.Replace ("Legacy", "1.6 Limited");
      }
      engine.Printf ("YaPB Bot has detect game version as Counter-Strike: %s", gameVersionStr.GetBuffer ());

      if (g_gameFlags & GAME_METAMOD)
         return;
   }
#endif

   auto api_GiveFnptrsToDll = g_gameLib->GetFuncAddr <FuncPointers_t> ("GiveFnptrsToDll");

   if (!api_GiveFnptrsToDll)
      TerminateOnMalloc ();

   GetEngineFunctions (functionTable, nullptr);
   
   // give the engine functions to the other DLL...
   api_GiveFnptrsToDll (functionTable, pGlobals);
}

DLL_ENTRYPOINT
{
   // dynamic library entry point, can be used for uninitialization stuff. NOT for initializing
   // anything because if you ever attempt to wander outside the scope of this function on a
   // DLL attach, LoadLibrary() will simply fail. And you can't do I/Os here either.

   // dynamic library detaching ??
   if (DLL_DETACHING)
   {
      FreeLibraryMemory (); // free everything that's freeable
      delete g_gameLib; // if dynamic link library of mod is load, free it
   }
   DLL_RETENTRY; // the return data type is OS specific too
}

void LinkEntity_Helper (EntityPtr_t &addr, const char *name, entvars_t *pev)
{
   if (addr == nullptr)
      addr = g_gameLib->GetFuncAddr <EntityPtr_t > (name);

   if (addr == nullptr)
      return;

   addr (pev);
}

#define LINK_ENTITY(entityName) \
SHARED_LIBRARAY_EXPORT void entityName (entvars_t *pev) \
{ \
   static EntityPtr_t addr; \
   LinkEntity_Helper (addr, #entityName, pev); \
} \

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

#ifdef XASH_CSDM
LINK_ENTITY (aiscripted_sequence)
LINK_ENTITY (cine_blood)
LINK_ENTITY (deadplayer_entity)
LINK_ENTITY (func_headq)
LINK_ENTITY (info_node)
LINK_ENTITY (info_node_air)
LINK_ENTITY (info_player_csdm)
LINK_ENTITY (monster_c4)
LINK_ENTITY (monster_cine2_hvyweapons)
LINK_ENTITY (monster_cine2_scientist)
LINK_ENTITY (monster_cine2_slave)
LINK_ENTITY (monster_cine3_barney)
LINK_ENTITY (monster_cine3_scientist)
LINK_ENTITY (monster_cine_barney)
LINK_ENTITY (monster_cine_panther)
LINK_ENTITY (monster_cine_scientist)
LINK_ENTITY (monster_cockroach)
LINK_ENTITY (monster_furniture)
LINK_ENTITY (monster_osprey)
LINK_ENTITY (monster_rat)
LINK_ENTITY (monster_tentacle)
LINK_ENTITY (monster_tentaclemaw)
LINK_ENTITY (monstermaker)
LINK_ENTITY (node_viewer)
LINK_ENTITY (node_viewer_fly)
LINK_ENTITY (node_viewer_human)
LINK_ENTITY (node_viewer_large)
LINK_ENTITY (scripted_sequence)
LINK_ENTITY (testhull)
LINK_ENTITY (xen_hair)
LINK_ENTITY (xen_hull)
LINK_ENTITY (xen_plantlight)
LINK_ENTITY (xen_spore_large)
LINK_ENTITY (xen_spore_medium)
LINK_ENTITY (xen_spore_small)
LINK_ENTITY (xen_tree)
LINK_ENTITY (xen_ttrigger)
#endif
