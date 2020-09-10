//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#include <yapb.h>

ConVar cv_display_menu_text ("yb_display_menu_text", "1", "Enables or disables display menu text, when players asks for menu. Useful only for Android.");
ConVar cv_password ("yb_password", "", "The value (password) for the setinfo key, if user set's correct password, he's gains access to bot commands and menus.", false, 0.0f, 0.0f, Var::Password);
ConVar cv_password_key ("yb_password_key", "_ubpw", "The name of setinfo key used to store password to bot commands and menus.", false);

int BotControl::cmdAddBot () {
   enum args { alias = 1, difficulty, personality, team, model, name, max };

   // this is duplicate error as in main bot creation code, but not to be silent
   if (!graph.length () || graph.hasChanged ()) {
      ctrl.msg ("There is no graph found or graph is changed. Cannot create bot.");
      return BotCommandResult::Handled;
   }

   // give a chance to use additional args
   m_args.resize (max);

   // if team is specified, modify args to set team
   if (strValue (alias).find ("_ct", 0) != String::InvalidIndex) {
      m_args.set (team, "2");
   }
   else if (strValue (alias).find ("_t", 0) != String::InvalidIndex) {
      m_args.set (team, "1");
   }

   // if highskilled bot is requsted set personality to rusher and maxout difficulty
   if (strValue (alias).find ("hs", 0) != String::InvalidIndex) {
      m_args.set (difficulty, "4");
      m_args.set (personality, "1");
   }
   bots.addbot (strValue (name), strValue (difficulty), strValue (personality), strValue (team), strValue (model), true);

   return BotCommandResult::Handled;
}

int BotControl::cmdKickBot () {
   enum args { alias = 1, team };

   // if team is specified, kick from specified tram
   if (strValue (alias).find ("_ct", 0) != String::InvalidIndex || intValue (team) == 2 || strValue (team) == "ct") {
      bots.kickFromTeam (Team::CT);
   }
   else if (strValue (alias).find ("_t", 0) != String::InvalidIndex || intValue (team) == 1 || strValue (team) == "t") {
      bots.kickFromTeam (Team::Terrorist);
   }
   else {
      bots.kickRandom ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdKickBots () {
   enum args { alias = 1, instant };

   // check if we're need to remove bots instantly
   auto kickInstant = strValue (instant) == "instant";

   // kick the bots
   bots.kickEveryone (kickInstant);

   return BotCommandResult::Handled;
}

int BotControl::cmdKillBots () {
   enum args { alias = 1, team, max };

   // if team is specified, kick from specified tram
   if (strValue (alias).find ("_ct", 0) != String::InvalidIndex || intValue (team) == 2 || strValue (team) == "ct") {
      bots.killAllBots (Team::CT);
   }
   else if (strValue (alias).find ("_t", 0) != String::InvalidIndex || intValue (team) == 1 || strValue (team) == "t") {
      bots.killAllBots (Team::Terrorist);
   }
   else {
      bots.killAllBots ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdFill () {
   enum args { alias = 1, team, count, difficulty, personality };

   if (!hasArg (team)) {
      return BotCommandResult::BadFormat;
   }
   bots.serverFill (intValue (team), hasArg (personality) ? intValue (personality) : -1, hasArg (difficulty) ? intValue (difficulty) : -1, hasArg (count) ? intValue (count) : -1);

   return BotCommandResult::Handled;
}

int BotControl::cmdVote () {
   enum args { alias = 1, mapid };

   if (!hasArg (mapid)) {
      return BotCommandResult::BadFormat;
   }
   int mapID = intValue (mapid);

   // loop through all players
   for (const auto &bot : bots) {
      bot->m_voteMap = mapID;
   }
   msg ("All dead bots will vote for map #%d.", mapID);

   return BotCommandResult::Handled;
}

int BotControl::cmdWeaponMode () {
   enum args { alias = 1, type };

   if (!hasArg (type)) {
      return BotCommandResult::BadFormat;
   }
   HashMap <String, int> modes;

   modes["knife"] = 1;
   modes["pistol"] = 2;
   modes["shotgun"] = 3;
   modes["smg"] = 4;
   modes["rifle"] = 5;
   modes["sniper"] = 6;
   modes["standard"] = 7;

   auto mode = strValue (type);

   // check if selected mode exists
   if (!modes.has (mode)) {
      return BotCommandResult::BadFormat;
   }
   bots.setWeaponMode (modes[mode]);

   return BotCommandResult::Handled;
}

int BotControl::cmdVersion () {
   auto &build = product.build;

   msg ("%s v%s.%s (ID %s)", product.name, product.version, build.count, build.id);
   msg ("   by %s (%s)", product.author, product.email);
   msg ("   %s", product.url);
   msg ("compiled: %s on %s with %s", product.dtime, build.machine, build.compiler);

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeMenu () {
   enum args { alias = 1 };

   // graph editor is available only with editor
   if (!graph.hasEditor ()) {
      msg ("Unable to open graph editor without setting the editor player.");
      return BotCommandResult::Handled;
   }
   showMenu (Menu::NodeMainPage1);

   return BotCommandResult::Handled;
}

int BotControl::cmdMenu () {
   enum args { alias = 1, cmd };

   // reset the current menu
   showMenu (Menu::None);

   if (strValue (cmd) == "cmd" && util.isAlive (m_ent)) {
      showMenu (Menu::Commands);
   }
   else {
      showMenu (Menu::Main);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdList () {
   enum args { alias = 1 };

   bots.listBots ();
   return BotCommandResult::Handled;
}

int BotControl::cmdCvars () {
   enum args { alias = 1, pattern };

   const auto &match = strValue (pattern);
   const bool isSave = match == "save";

   File cfg;

   // if save requested, dump cvars to yapb.cfg
   if (isSave) {
      cfg.open (strings.format ("%s/addons/%s/conf/%s.cfg", game.getRunningModName (), product.folder, product.folder), "wt");
      cfg.puts ("// Configuration file for %s\n\n", product.name);
   }

   for (const auto &cvar : game.getCvars ()) {
      if (cvar.info.empty ()) {
         continue;
      }

      if (!isSave && !match.empty () && !strstr (cvar.reg.name, match.chars ())) {
         continue;
      }

      // float value ?
      bool isFloat = !strings.isEmpty (cvar.self->str ()) && strchr (cvar.self->str (), '.');

      if (isSave) {
         cfg.puts ("//\n");
         cfg.puts ("// %s\n", String::join (cvar.info.split ("\n"), "\n//  "));
         cfg.puts ("// ---\n");

         if (cvar.bounded) {
            if (isFloat) {
               cfg.puts ("// Default: \"%.1f\", Min: \"%.1f\", Max: \"%.1f\"\n", cvar.initial, cvar.min, cvar.max);
            }
            else {
               cfg.puts ("// Default: \"%i\", Min: \"%i\", Max: \"%i\"\n", static_cast <int> (cvar.initial), static_cast <int> (cvar.min), static_cast <int> (cvar.max));
            }
         }
         else {
            cfg.puts ("// Default: \"%s\"\n", cvar.self->str ());
         }
         cfg.puts ("// \n");

         if (cvar.bounded) {
            if (isFloat) {
               cfg.puts ("%s \"%.1f\"\n", cvar.reg.name, cvar.self->float_ ());
            }
            else {
               cfg.puts ("%s \"%i\"\n", cvar.reg.name, cvar.self->int_ ());
            }
         }
         else {
            cfg.puts ("%s \"%s\"\n", cvar.reg.name, cvar.self->str ());
         }
         cfg.puts ("\n");
      }
      else {
         game.print ("cvar: %s", cvar.reg.name);
         game.print ("info: %s", conf.translate (cvar.info));

         game.print (" ");
      }
   }

   if (isSave) {
      msg ("Bots cvars has been written to file.");
      cfg.close ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNode () {
   enum args { root, alias, cmd, cmd2 };

   static Array <StringRef> allowedOnDedicatedServer {
      "acquire_editor",
      "upload",
      "save",
      "load",
      "help"
   };

   // check if cmd is allowed on dedicated server
   auto isAllowedOnDedicatedServer = [] (StringRef str) -> bool {
      for (const auto &test : allowedOnDedicatedServer) {
         if (test == str) {
            return true;
         }
      }
      return false;
   };

   // graph editor supported only with editor
   if (game.isDedicated () && !graph.hasEditor () && !isAllowedOnDedicatedServer (strValue (cmd))) {
      msg ("Unable to use graph edit commands without setting graph editor player. Please use \"graph acquire_editor\" to acquire rights for graph editing.");
      return BotCommandResult::Handled;
   }

   // should be moved to class?
   static HashMap <String, BotCmd> commands;
   static StringArray descriptions;

   // fill only once
   if (descriptions.empty ()) {

      // separate function
      auto addGraphCmd = [&] (String cmd, String format, String help, Handler handler) -> void {
         BotCmd botCmd { cmd, cr::move (format), cr::move (help), cr::move (handler) };

         commands[cmd] = cr::move (botCmd);
         descriptions.push (cmd);
      };

      // add graph commands
      addGraphCmd ("on", "on [display|auto|noclip|models]", "Enables displaying of graph, nodes, noclip cheat", &BotControl::cmdNodeOn);
      addGraphCmd ("off", "off [display|auto|noclip|models]", "Disables displaying of graph, auto adding nodes, noclip cheat", &BotControl::cmdNodeOff);
      addGraphCmd ("menu", "menu [noarguments]", "Opens and displays bots graph editor.", &BotControl::cmdNodeMenu);
      addGraphCmd ("add", "add [noarguments]", "Opens and displays graph node add menu.", &BotControl::cmdNodeAdd);
      addGraphCmd ("addbasic", "menu [noarguments]", "Adds basic nodes such as player spawn points, goals and ladders.", &BotControl::cmdNodeAddBasic);
      addGraphCmd ("save", "save [noarguments]", "Save graph file to disk.", &BotControl::cmdNodeSave);
      addGraphCmd ("load", "load [noarguments]", "Load graph file from disk.", &BotControl::cmdNodeLoad);
      addGraphCmd ("erase", "erase [iamsure]", "Erases the graph file from disk.", &BotControl::cmdNodeErase);
      addGraphCmd ("delete", "delete [nearest|index]", "Deletes single graph node from map.", &BotControl::cmdNodeDelete);
      addGraphCmd ("check", "check [noarguments]", "Check if graph working correctly.", &BotControl::cmdNodeCheck);
      addGraphCmd ("cache", "cache [nearest|index]", "Caching node for future use.", &BotControl::cmdNodeCache);
      addGraphCmd ("clean", "clean [all|nearest|index]", "Clean useless path connections from all or single node.", &BotControl::cmdNodeClean);
      addGraphCmd ("setradius", "setradius [radius] [nearest|index]", "Sets the radius for node.", &BotControl::cmdNodeSetRadius);
      addGraphCmd ("flags", "flags [noarguments]", "Open and displays menu for modifying flags for nearest point.", &BotControl::cmdNodeSetFlags);
      addGraphCmd ("teleport", "teleport [index]", "Teleports player to specified node index.", &BotControl::cmdNodeTeleport);
      addGraphCmd ("upload", "upload [id]", "Uploads created graph to graph database.", &BotControl::cmdNodeUpload);
      addGraphCmd ("stats", "[noarguments]", "Shows the stats about node types on the map.", &BotControl::cmdNodeShowStats);

      // add path commands
      addGraphCmd ("path_create", "path_create [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathCreate);
      addGraphCmd ("path_create_in", "path_create_in [noarguments]", "Creates incoming path connection from faced to nearest graph.", &BotControl::cmdNodePathCreate);
      addGraphCmd ("path_create_out", "path_create_out [noarguments]", "Creates outgoing path connection from faced to nearest graph.", &BotControl::cmdNodePathCreate);
      addGraphCmd ("path_create_both", "path_create_both [noarguments]", "Creates both-ways path connection from faced to nearest graph.", &BotControl::cmdNodePathCreate);
      addGraphCmd ("path_delete", "path_delete [noarguments]", "Deletes path from faced to nearest graph.", &BotControl::cmdNodePathDelete);
      addGraphCmd ("path_set_autopath", "path_set_autopath [max_distance]", "Opens menu for setting autopath maximum distance.", &BotControl::cmdNodePathSetAutoDistance);

      // camp points iterator
      addGraphCmd ("iterate_camp", "iterate_camp [begin|end|next]", "Allows to go through all camp points on map.", &BotControl::cmdNodeIterateCamp);

      // remote graph editing stuff
      if (game.isDedicated ()) {
         addGraphCmd ("acquire_editor", "acquire_editor", "Acquires rights to edit graph on dedicated server.", &BotControl::cmdNodeAcquireEditor);
         addGraphCmd ("release_editor", "acquire_editor", "Releases graph editing rights.", &BotControl::cmdNodeAcquireEditor);
      }
   }
   if (commands.has (strValue (cmd))) {
      auto item = commands[strValue (cmd)];

      // graph have only bad format return status
      int status = (this->*item.handler) ();

      if (status == BotCommandResult::BadFormat) {
         msg ("Incorrect usage of \"%s %s %s\" command. Correct usage is:\n\n\t%s\n\nPlease use correct format.", m_args[root], m_args[alias], item.name, item.format);
      }
   }
   else {
      if (strValue (cmd) == "help" && hasArg (cmd2) && commands.has (strValue (cmd2))) {
         auto &item = commands[strValue (cmd2)];

         msg ("Command: \"%s %s %s\"\nFormat: %s\nHelp: %s", m_args[root], m_args[alias], item.name, item.format, conf.translate (item.help));
      }
      else {
         for (auto &desc : descriptions) {
            auto &item = commands[desc];
            msg ("   %s - %s", item.name, conf.translate (item.help));
         }
         msg ("Currently Graph Status %s", graph.hasEditFlag (GraphEdit::On) ? "Enabled" : "Disabled");
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeOn () {
   enum args { alias = 1, cmd, option };

   // enable various features of editor
   if (strValue (option).empty () || strValue (option) == "display" || strValue (option) == "models") {
      graph.setEditFlag (GraphEdit::On);
      enableDrawModels (true);

      msg ("Graph editor has been enabled.");
   }
   else if (strValue (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_NOCLIP;

      graph.setEditFlag (GraphEdit::On | GraphEdit::Noclip);
      enableDrawModels (true);

      msg ("Graph editor has been enabled with noclip mode.");
   }
   else if (strValue (option) == "auto") {
      graph.setEditFlag (GraphEdit::On | GraphEdit::Auto);
      enableDrawModels (true);

      msg ("Graph editor has been enabled with auto add node mode.");
   }

   if (graph.hasEditFlag (GraphEdit::On)) {
      extern ConVar mp_roundtime, mp_freezetime, mp_timelimit;

      mp_roundtime.set (9);
      mp_freezetime.set (0);
      mp_timelimit.set (0);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeOff () {
   enum args { graph_cmd = 1, cmd, option };

   // enable various features of editor
   if (strValue (option).empty () || strValue (option) == "display") {
      graph.clearEditFlag (GraphEdit::On | GraphEdit::Auto | GraphEdit::Noclip);
      enableDrawModels (false);

      msg ("Graph editor has been disabled.");
   }
   else if (strValue (option) == "models") {
      enableDrawModels (false);

      msg ("Graph editor has disabled spawn points highlighting.");
   }
   else if (strValue (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_WALK;
      graph.clearEditFlag (GraphEdit::Noclip);

      msg ("Graph editor has disabled noclip mode.");
   }
   else if (strValue (option) == "auto") {
      graph.clearEditFlag (GraphEdit::Auto);
      msg ("Graph editor has disabled auto add node mode.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAdd () {
   enum args { graph_cmd = 1, cmd };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // show the menu
   showMenu (Menu::NodeType);
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAddBasic () {
   enum args { graph_cmd = 1, cmd };
   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   graph.addBasic ();
   msg ("Basic graph nodes was added.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSave () {
   enum args { graph_cmd = 1, cmd, option };

   // if no check is set save anyway
   if (strValue (option) == "nocheck") {
      graph.saveGraphData ();

      msg ("All nodes has been saved and written to disk (IGNORING QUALITY CONTROL).");
   }
   else if (strValue (option) == "old") {
      graph.saveOldFormat ();

      msg ("All nodes has been saved and written to disk (POD-Bot Format (.pwf)).");
   }
   else {
      if (graph.checkNodes (false)) {
         graph.saveGraphData ();
         msg ("All nodes has been saved and written to disk.");
      }
      else {
         msg ("Could not save save nodes to disk. Graph check has failed.");
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeLoad () {
   enum args { graph_cmd = 1, cmd };

   // just save graph on request
   if (graph.loadGraphData ()) {
      msg ("Graph successfully loaded.");
   }
   else {
      msg ("Could not load Graph. See console...");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeErase () {
   enum args { graph_cmd = 1, cmd, iamsure };

   // prevent accidents when graph are deleted unintentionally
   if (strValue (iamsure) == "iamsure") {
      graph.eraseFromDisk ();
   }
   else {
      msg ("Please, append \"iamsure\" as parameter to get graph erased from the disk.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeDelete () {
   enum args { graph_cmd = 1, cmd, nearest };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (strValue (nearest).empty () || strValue (nearest) == "nearest") {
      graph.erase (kInvalidNodeIndex);
   }
   else {
      int index = intValue (nearest);

      // check for existence
      if (graph.exists (index)) {
         graph.erase (index);
         msg ("Node %d has been deleted.", index);
      }
      else {
         msg ("Could not delete node %d.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeCheck () {
   enum args { graph_cmd = 1, cmd };

   // check if nodes are ok
   if (graph.checkNodes (true)) {
      msg ("Graph seems to be OK.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeCache () {
   enum args { graph_cmd = 1, cmd, nearest };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (strValue (nearest).empty () || strValue (nearest) == "nearest") {
      graph.cachePoint (kInvalidNodeIndex);

      msg ("Nearest node has been put into the memory.");
   }
   else {
      int index = intValue (nearest);

      // check for existence
      if (graph.exists (index)) {
         graph.cachePoint (index);
         msg ("Node %d has been put into the memory.", index);
      }
      else {
         msg ("Could not put node %d into the memory.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeClean () {
   enum args { graph_cmd = 1, cmd, option };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "all" passed clean up all the paths
   if (strValue (option) == "all") {
      int removed = 0;

      for (int i = 0; i < graph.length (); ++i) {
         removed += graph.clearConnections (i);
      }
      msg ("Done. Processed %d nodes. %d useless paths was cleared.", graph.length (), removed);
   }
   else if (strValue (option).empty () || strValue (option) == "nearest") {
      int removed = graph.clearConnections (graph.getEditorNeareset ());

      msg ("Done. Processed node %d. %d useless paths was cleared.", graph.getEditorNeareset (), removed);
   }
   else {
      int index = intValue (option);

      // check for existence
      if (graph.exists (index)) {
         int removed = graph.clearConnections (index);

         msg ("Done. Processed node %d. %d useless paths was cleared.", index, removed);
      }
      else {
         msg ("Could not process node %d clearance.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSetRadius () {
   enum args { graph_cmd = 1, cmd, radius, index };

   // radius is a must
   if (!hasArg (radius)) {
      return BotCommandResult::BadFormat;
   }
   int radiusIndex = kInvalidNodeIndex;

   if (strValue (index).empty () || strValue (index) == "nearest") {
      radiusIndex = graph.getEditorNeareset ();
   }
   else {
      radiusIndex = intValue (index);
   }
   graph.setRadius (radiusIndex, strValue (radius).float_ ());

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSetFlags () {
   enum args { graph_cmd = 1, cmd };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   //show the flag menu
   showMenu (Menu::NodeFlag);
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeTeleport () {
   enum args { graph_cmd = 1, cmd, teleport_index };

   if (!hasArg (teleport_index)) {
      return BotCommandResult::BadFormat;
   }
   int index = intValue (teleport_index);

   // check for existence
   if (graph.exists (index)) {
      engfuncs.pfnSetOrigin (graph.getEditor (), graph[index].origin);

      msg ("You have been teleported to node %d.", index);

      // turn graph on
      graph.setEditFlag (GraphEdit::On | GraphEdit::Noclip);
   }
   else {
      msg ("Could not teleport to node %d.", index);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathCreate () {
   enum args { graph_cmd = 1, cmd };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // choose the direction for path creation
   if (strValue (cmd).find ("_both", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Bidirectional);
   }
   else if (strValue (cmd).find ("_in", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Incoming);
   }
   else if (strValue (cmd).find ("_out", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Outgoing);
   }
   else {
      showMenu (Menu::NodePath);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathDelete () {
   enum args { graph_cmd = 1, cmd };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // delete the patch
   graph.erasePath ();

   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathSetAutoDistance () {
   enum args { graph_cmd = 1, cmd };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);
   showMenu (Menu::NodeAutoPath);

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAcquireEditor () {
   enum args { graph_cmd = 1 };

   if (game.isNullEntity (m_ent)) {
      msg ("This command should not be executed from HLDS console.");
      return BotCommandResult::Handled;
   }

   if (graph.hasEditor ()) {
      msg ("Sorry, players \"%s\" already acquired rights to edit graph on this server.", graph.getEditor ()->v.netname.chars ());
      return BotCommandResult::Handled;
   }
   graph.setEditor (m_ent);
   msg ("You're acquired rights to edit graph on this server. You're now able to use graph commands.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeReleaseEditor () {
   enum args { graph_cmd = 1 };

   if (!graph.hasEditor ()) {
      msg ("No one is currently has rights to edit. Nothing to release.");
      return BotCommandResult::Handled;
   }
   graph.setEditor (nullptr);
   msg ("Graph editor rights freed. You're now not able to use graph commands.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeUpload () {
   enum args { graph_cmd = 1, cmd };

   // do not allow to upload bad graph
   if (!graph.checkNodes (false)) {
      msg ("Sorry, unable to upload graph file that contains errors. Please type \"wp check\" to verify graph consistency.");
      return BotCommandResult::BadFormat;
   }

   msg ("\n");
   msg ("WARNING!");
   msg ("Graph uploaded to graph database in synchronous mode. That means if graph is big enough");
   msg ("you may notice the game freezes a bit during upload and issue request creation. Please, be patient.");
   msg ("\n");

   // six seconds is enough?
   http.setTimeout (6);

   // try to upload the file
   if (http.uploadFile (strings.format ("http://%s/graph", product.download), strings.format ("%sgraph/%s.graph", graph.getDataDirectory (false), game.getMapName ()))) {
      msg ("Graph file was successfully validated and uploaded to the YaPB Graph DB (%s).", product.download);
      msg ("It will be available for download for all YaPB users in a few minutes.");
      msg ("\n");
      msg ("Thank you.");
      msg ("\n");
   }
   else {
      String status;
      auto code = http.getLastStatusCode ();

      if (code == HttpClientResult::Forbidden) {
         status = "AlreadyExists";
      }
      else if (code == HttpClientResult::NotFound) {
         status = "AccessDenied";
      }
      else {
         status.assignf ("%d", code);
      }
      msg ("Something went wrong with uploading. Come back later. (%s)", status);
      msg ("\n");

      if (code == HttpClientResult::Forbidden) {
         msg ("You should create issue-request manually for this graph");
         msg ("as it's already exists in database, can't overwrite. Sorry...");
      }
      else {
         msg ("There is an internal error, or something is totally wrong with");
         msg ("your files, and they are not passed sanity checks. Sorry...");
      }
      msg ("\n");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeIterateCamp () {
   enum args { graph_cmd = 1, cmd, option };

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // get the option descriping operation
   auto op = strValue (option);

   if (op != "begin" && op != "end" && op != "next") {
      return BotCommandResult::BadFormat;
   }

   if ((op == "next" || op == "end") && m_campIterator.empty ()) {
      msg ("Before calling for 'next' / 'end' camp point, you should hit 'begin'.");
      return BotCommandResult::Handled;
   }
   else if (op == "begin" && !m_campIterator.empty ()) {
      msg ("Before calling for 'begin' camp point, you should hit 'end'.");
      return BotCommandResult::Handled;
   }
   
   if (op == "end") {
      m_campIterator.clear ();
   }
   else if (op == "next") {
      if (!m_campIterator.empty ()) {
         Vector origin = graph[m_campIterator.first ()].origin;

         if (graph[m_campIterator.first ()].flags & NodeFlag::Crouch) {
            origin.z += 23.0f;
         }
         engfuncs.pfnSetOrigin (m_ent, origin);

         // go to next
         m_campIterator.shift ();

         if (m_campIterator.empty ()) {
            msg ("Finished iterating camp spots.");
         }
      }
   }
   else if (op == "begin") {
      for (int i = 0; i < graph.length (); ++i) {
         if (graph[i].flags & NodeFlag::Camp) {
            m_campIterator.push (i);
         }
      }
      msg ("Ready for iteration. Type 'next' to go to first camp node.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeShowStats () {
   enum args { graph_cmd = 1 };

   graph.showStats ();

   return BotCommandResult::Handled;
}

int BotControl::menuMain (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      m_isMenuFillCommand = false;
      showMenu (Menu::Control);
      break;

   case 2:
      showMenu (Menu::Features);
      break;

   case 3:
      m_isMenuFillCommand = true;
      showMenu (Menu::TeamSelect);
      break;

   case 4:
      bots.killAllBots ();
      break;

   case 10:
      showMenu (Menu::None);
      break;

   default:
      showMenu (Menu::Main);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuFeatures (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      showMenu (Menu::WeaponMode);
      break;

   case 2:
      showMenu (graph.hasEditor () ? Menu::NodeMainPage1 : Menu::Features);
      break;

   case 3:
      showMenu (Menu::Personality);
      break;

   case 4:
      extern ConVar cv_debug;
      cv_debug.set (cv_debug.int_ () ^ 1);

      showMenu (Menu::Features);
      break;

   case 5:
      if (util.isAlive (m_ent)) {
         showMenu (Menu::Commands);
      }
      else {
         showMenu (Menu::None); // reset menu display
         msg ("You're dead, and have no access to this menu");
      }
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuControl (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      bots.createRandom (true);
      showMenu (Menu::Control);
      break;

   case 2:
      showMenu (Menu::Difficulty);
      break;

   case 3:
      bots.kickRandom ();
      showMenu (Menu::Control);
      break;

   case 4:
      bots.kickEveryone ();
      break;

   case 5:
      kickBotByMenu (1);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuWeaponMode (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
      bots.setWeaponMode (item);
      showMenu (Menu::WeaponMode);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuPersonality (int item) {
   if (m_isMenuFillCommand) {
      showMenu (Menu::None); // reset menu display

      switch (item) {
      case 1:
      case 2:
      case 3:
      case 4:
         bots.serverFill (m_menuServerFillTeam, item - 2, m_interMenuData[0]);
         showMenu (Menu::None);
         break;

      case 10:
         showMenu (Menu::None);
         break;
      }
      return BotCommandResult::Handled;
   }
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
      m_interMenuData[3] = item - 2;
      showMenu (Menu::TeamSelect);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuDifficulty (int item) {
   showMenu (Menu::None); // reset menu display

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
      showMenu (Menu::None);
      break;
   }
   showMenu (Menu::Personality);

   return BotCommandResult::Handled;
}

int BotControl::menuTeamSelect (int item) {
   if (m_isMenuFillCommand) {
      showMenu (Menu::None); // reset menu display

      if (item < 3) {
         extern ConVar mp_limitteams, mp_autoteambalance;

         // turn off cvars if specified team
         mp_limitteams.set (0);
         mp_autoteambalance.set (0);
      }

      switch (item) {
      case 1:
      case 2:
      case 5:
         m_menuServerFillTeam = item;
         showMenu (Menu::Difficulty);
         break;

      case 10:
         showMenu (Menu::None);
         break;
      }
      return BotCommandResult::Handled;
   }
   showMenu (Menu::None); // reset menu display

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
         showMenu (item == 1 ? Menu::TerroristSelect : Menu::CTSelect);
      }
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuClassSelect (int item) {
   showMenu (Menu::None); // reset menu display

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
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuCommands (int item) {
   showMenu (Menu::None); // reset menu display
   Bot *nearest = nullptr;

   switch (item) {
   case 1:
   case 2:
      if (util.findNearestPlayer (reinterpret_cast <void **> (&m_djump), m_ent, 600.0f, true, true, true, true, false) && !m_djump->m_hasC4 && !m_djump->hasHostage ()) {
         if (item == 1) {
            m_djump->startDoubleJump (m_ent);
         }
         else {
            if (m_djump) {
               m_djump->resetDoubleJump ();
               m_djump = nullptr;
            }
         }
      }
      showMenu (Menu::Commands);
      break;

   case 3:
   case 4:
      if (util.findNearestPlayer (reinterpret_cast <void **> (&nearest), m_ent, 600.0f, true, true, true, true, item == 4 ? false : true)) {
         nearest->dropWeaponForUser (m_ent, item == 4 ? false : true);
      }
      showMenu (Menu::Commands);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuGraphPage1 (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      if (graph.hasEditFlag (GraphEdit::On)) {
         graph.clearEditFlag (GraphEdit::On);
         enableDrawModels (false);

         msg ("Graph editor has been disabled.");
      }
      else {
         graph.setEditFlag (GraphEdit::On);
         enableDrawModels (true);

         msg ("Graph editor has been enabled.");
      }
      showMenu (Menu::NodeMainPage1);
      break;

   case 2:
      graph.setEditFlag (GraphEdit::On);
      graph.cachePoint (kInvalidNodeIndex);

      showMenu (Menu::NodeMainPage1);
      break;

   case 3:
      graph.setEditFlag (GraphEdit::On);
      showMenu (Menu::NodePath);
      break;

   case 4:
      graph.setEditFlag (GraphEdit::On);
      graph.erasePath ();
      
      showMenu (Menu::NodeMainPage1);
      break;

   case 5:
      graph.setEditFlag (GraphEdit::On);
      showMenu (Menu::NodeType);
      break;

   case 6:
      graph.setEditFlag (GraphEdit::On);
      graph.erase (kInvalidNodeIndex);

      showMenu (Menu::NodeMainPage1);
      break;

   case 7:
      graph.setEditFlag (GraphEdit::On);
      showMenu (Menu::NodeAutoPath);
      break;

   case 8:
      graph.setEditFlag (GraphEdit::On);
      showMenu (Menu::NodeRadius);
      break;

   case 9:
      showMenu (Menu::NodeMainPage2);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuGraphPage2 (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1: 
      graph.showStats ();
      showMenu (Menu::NodeMainPage2);

      break;

   case 2:
      graph.setEditFlag (GraphEdit::On);

      if (graph.hasEditFlag (GraphEdit::Auto)) {
         graph.clearEditFlag (GraphEdit::Auto);
      }
      else {
         graph.setEditFlag (GraphEdit::Auto);
      }

      if (graph.hasEditFlag (GraphEdit::Auto)) {
         msg ("Enabled auto nodes placement.");
      }
      else {
         msg ("Disabled auto nodes placement.");
      }
      showMenu (Menu::NodeMainPage2);
      break;

   case 3:
      graph.setEditFlag (GraphEdit::On);
      showMenu (Menu::NodeFlag);
      break;

   case 4:
      if (graph.checkNodes (true)) {
         graph.saveGraphData ();
      }
      else {
         msg ("Graph not saved\nThere are errors. See console...");
      }
      showMenu (Menu::NodeMainPage2);
      break;

   case 5:
      graph.saveGraphData ();
      showMenu (Menu::NodeMainPage2);
      break;

   case 6:
      graph.loadGraphData ();
      showMenu (Menu::NodeMainPage2);
      break;

   case 7:
      if (graph.checkNodes (true)) {
         msg ("Nodes works fine");
      }
      else {
         msg ("There are errors, see console");
      }
      showMenu (Menu::NodeMainPage2);
      break;

   case 8:
      graph.setEditFlag (GraphEdit::On);

      if (graph.hasEditFlag (GraphEdit::Noclip)) {
         graph.clearEditFlag (GraphEdit::Noclip);
      }
      else {
         graph.setEditFlag (GraphEdit::Noclip);
      }
      showMenu (Menu::NodeMainPage2);

      // update editor movetype based on flag
      m_ent->v.movetype = graph.hasEditFlag (GraphEdit::Noclip) ? MOVETYPE_NOCLIP : MOVETYPE_WALK;

      break;

   case 9:
      showMenu (Menu::NodeMainPage1);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuGraphRadius (int item) {
   showMenu (Menu::None); // reset menu display
   graph.setEditFlag (GraphEdit::On); // turn graph on in case

   constexpr float radius[] = { 0.0f, 8.0f, 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 128.0f };

   if (item >= 1 && item <= 9) {
      graph.setRadius (kInvalidNodeIndex, radius[item - 1]);
      showMenu (Menu::NodeRadius);
   }
   return BotCommandResult::Handled;
}

int BotControl::menuGraphType (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
   case 2:
   case 3:
   case 4:
   case 5:
   case 6:
   case 7:
      graph.add (item - 1);
      showMenu (Menu::NodeType);
      break;

   case 8:
      graph.add (100);
      showMenu (Menu::NodeType);
      break;

   case 9:
      graph.startLearnJump ();
      showMenu (Menu::NodeType);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuGraphFlag (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      graph.toggleFlags (NodeFlag::NoHostage);
      showMenu (Menu::NodeFlag);
      break;

   case 2:
      graph.toggleFlags (NodeFlag::TerroristOnly);
      showMenu (Menu::NodeFlag);
      break;

   case 3:
      graph.toggleFlags (NodeFlag::CTOnly);
      showMenu (Menu::NodeFlag);
      break;

   case 4:
      graph.toggleFlags (NodeFlag::Lift);
      showMenu (Menu::NodeFlag);
      break;

   case 5:
      graph.toggleFlags (NodeFlag::Sniper);
      showMenu (Menu::NodeFlag);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuAutoPathDistance (int item) {
   showMenu (Menu::None); // reset menu display

   constexpr float distances[] = { 0.0f, 100.0f, 130.0f, 160.0f, 190.0f, 220.0f, 250.0f };

   if (item >= 1 && item <= 7) {
      graph.setAutoPathDistance (distances[item - 1]);
   }

   switch (item) {
   default:
      showMenu (Menu::NodeAutoPath);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuKickPage1 (int item) {
   showMenu (Menu::None); // reset menu display

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
      showMenu (Menu::Control);
      break;
   }
   return BotCommandResult::Handled;
}

int BotControl::menuKickPage2 (int item) {
   showMenu (Menu::None); // reset menu display

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
   return BotCommandResult::Handled;
}

int BotControl::menuKickPage3 (int item) {
   showMenu (Menu::None); // reset menu display

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
   return BotCommandResult::Handled;
}

int BotControl::menuKickPage4 (int item) {
   showMenu (Menu::None); // reset menu display

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
   return BotCommandResult::Handled;
}

int BotControl::menuGraphPath (int item) {
   showMenu (Menu::None); // reset menu display

   switch (item) {
   case 1:
      graph.pathCreate (PathConnection::Outgoing);
      showMenu (Menu::NodePath);
      break;

   case 2:
      graph.pathCreate (PathConnection::Incoming);
      showMenu (Menu::NodePath);
      break;

   case 3:
      graph.pathCreate (PathConnection::Bidirectional);
      showMenu (Menu::NodePath);
      break;

   case 10:
      showMenu (Menu::None);
      break;
   }
   return BotCommandResult::Handled;
}

bool BotControl::executeCommands () {
   if (m_args.empty ()) {
      return false;
   }
   const auto &prefix = m_args[0];

   // no handling if not for us
   if (prefix != product.cmdPri && prefix != product.cmdSec) {
      return false;
   }
   Client &client = util.getClient (game.indexOfPlayer (m_ent));

   // do not allow to execute stuff for non admins
   if (m_ent != game.getLocalEntity () && !(client.flags & ClientFlags::Admin)) {
      msg ("Access to %s commands is restricted.", product.name);
      return true;
   }

   auto aliasMatch = [] (String &test, const String &cmd, String &aliasName) -> bool {
      for (auto &alias : test.split ("/")) {
         if (alias == cmd) {
            aliasName = alias;
            return true;
         }
      }
      return false;
   };
   String cmd;

   // give some help
   if (hasArg (1) && m_args[1] == "help") {
      const auto hasSecondArg = hasArg (2);

      for (auto &item : m_cmds) {
         if (!hasSecondArg) {
            cmd = item.name.split ("/")[0];
         }

         if ((hasSecondArg && aliasMatch (item.name, m_args[2], cmd)) || !hasSecondArg) {
            msg ("Command: \"%s %s\"\nFormat: %s\nHelp: %s", prefix, cmd, item.format, conf.translate (item.help));

            auto aliases = item.name.split ("/");

            if (aliases.length () > 1) {
               msg ("Aliases: %s", String::join (aliases, ", "));
            }

            if (hasSecondArg) {
               return true;
            }
            else {
               msg ("\n");
            }
         }
      }

      if (!hasSecondArg) {
         return true;
      }
      else {
         msg ("No help found for \"%s\"", m_args[2]);
      }
      return true;
   }
   cmd.clear ();

   // if no args passed just print all the commands
   if (m_args.length () == 1) {
      msg ("usage %s <command> [arguments]", prefix);
      msg ("valid commands are: ");

      for (auto &item : m_cmds) {
         msg ("   %s - %s", item.name.split ("/")[0], conf.translate (item.help));
      }
      return true;
   }

   // first search for a actual cmd
   for (auto &item : m_cmds) {
      if (aliasMatch (item.name, m_args[1], cmd)) {
         switch ((this->*item.handler) ()) {
         case BotCommandResult::Handled:
         default:
            break;

         case BotCommandResult::ListenServer:
            msg ("Command \"%s %s\" is only available from the listenserver console.", prefix, cmd);
            break;

         case BotCommandResult::BadFormat:
            msg ("Incorrect usage of \"%s %s\" command. Correct usage is:\n\n\t%s\n\nPlease type \"%s help %s\" to get more information.", prefix, cmd, item.format, prefix, cmd);
            break;
         }

         m_isFromConsole = false;
         return true;
      }
   }
   msg ("Unknown command: %s", m_args[1]);

   // clear all the arguments upon finish
   m_args.clear ();

   return true;
}

bool BotControl::executeMenus () {
   if (!util.isPlayer (m_ent) || game.isBotCmd ()) {
      return false;
   }
   auto &issuer = util.getClient (game.indexOfPlayer (m_ent));

   // check if it's menu select, and some key pressed
   if (strValue (0) != "menuselect" || strValue (1).empty () || issuer.menu == Menu::None) {
      return false;
   }

   // let's get handle
   for (auto &menu : m_menus) {
      if (menu.ident == issuer.menu) {
         return (this->*menu.handler) (strValue (1).int_ ());
      }
   }
   return false;
}

void BotControl::showMenu (int id) {
   static bool s_menusParsed = false;

   // make menus looks like we need only once
   if (!s_menusParsed) {
      for (auto &parsed : m_menus) {
         StringRef translated = conf.translate (parsed.text);

         // translate all the things
         parsed.text = translated;

          // make menu looks best
         if (!(game.is (GameFlags::Legacy))) {
            for (int j = 0; j < 10; ++j) {
               parsed.text.replace (strings.format ("%d.", j), strings.format ("\\r%d.\\w", j));
            }
         }
      }
      s_menusParsed = true;
   }

   if (!util.isPlayer (m_ent)) {
      return;
   }
   Client &client = util.getClient (game.indexOfPlayer (m_ent));

   if (id == Menu::None) {
      MessageWriter (MSG_ONE_UNRELIABLE, msgs.id (NetMsg::ShowMenu), nullptr, m_ent)
         .writeShort (0)
         .writeChar (0)
         .writeByte (0)
         .writeString ("");

      client.menu = id;
      return;
   }

   for (auto &display : m_menus) {
      if (display.ident == id) {
         auto text = (game.is (GameFlags::Xash3D | GameFlags::Mobility) && !cv_display_menu_text.bool_ ()) ? " " : display.text.chars ();
         MessageWriter msg;

         while (strlen (text) >= 64) {
            msg.start (MSG_ONE_UNRELIABLE, msgs.id (NetMsg::ShowMenu), nullptr, m_ent)
               .writeShort (display.slots)
               .writeChar (-1)
               .writeByte (1);

            for (int i = 0; i < 64; ++i) {
               msg.writeChar (text[i]);
            }
            msg.end ();
            text += 64;
         }

         MessageWriter (MSG_ONE_UNRELIABLE, msgs.id (NetMsg::ShowMenu), nullptr, m_ent)
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

   static StringRef headerTitle = conf.translate ("Bots Remove Menu");
   static StringRef notABot = conf.translate ("Not a Bot");
   static StringRef backKey = conf.translate ("Back");
   static StringRef moreKey = conf.translate ("More");

   String menus;
   menus.assignf ("\\y%s (%d/4):\\w\n\n", headerTitle, page);

   int menuKeys = (page == 4) ? cr::bit (9) : (cr::bit (8) | cr::bit (9));
   int menuKey = (page - 1) * 8;

   for (int i = menuKey; i < page * 8; ++i) {
      auto bot = bots[i];

      // check for fakeclient bit, since we're clear it upon kick, but actual bot struct destroyed after client disconnected
      if (bot != nullptr && (bot->pev->flags & FL_FAKECLIENT)) {
         menuKeys |= cr::bit (cr::abs (i - menuKey));
         menus.appendf ("%1.1d. %s%s\n", i - menuKey + 1, bot->pev->netname.chars (), bot->m_team == Team::CT ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else {
         menus.appendf ("\\d %1.1d. %s\\w\n", i - menuKey + 1, notABot);
      }
   }
   menus.appendf ("\n%s 0. %s", (page == 4) ? "" : strings.format (" 9. %s...\n", moreKey), backKey);

   // force to clear current menu
   showMenu (Menu::None);

   auto id = Menu::KickPage1 - 1 + page;

   for (auto &menu : m_menus) {
      if (menu.ident == id) {
         menu.slots = menuKeys & static_cast <uint32> (-1);
         menu.text = menus;

         break;
      }
   }
   showMenu (id);
}

void BotControl::assignAdminRights (edict_t *ent, char *infobuffer) {
   if (!game.isDedicated () || util.isFakeClient (ent)) {
      return;
   }
   StringRef key = cv_password_key.str ();
   StringRef password = cv_password.str ();

   if (!key.empty () && !password.empty ()) {
      auto &client = util.getClient (game.indexOfPlayer (ent));

      if (password == engfuncs.pfnInfoKeyValue (infobuffer, key.chars ())) {
         client.flags |= ClientFlags::Admin;
      }
      else {
         client.flags &= ~ClientFlags::Admin;
      }
   }
}

void BotControl::maintainAdminRights () {
   if (!game.isDedicated ()) {
      return;
   }

   for (int i = 0; i < game.maxClients (); ++i) {
      edict_t *player = game.playerOfIndex (i);

      // code below is executed only on dedicated server
      if (util.isPlayer (player) && !util.isFakeClient (player)) {
         Client &client = util.getClient (i);

         if (client.flags & ClientFlags::Admin) {
            if (strings.isEmpty (cv_password_key.str ()) && strings.isEmpty (cv_password.str ())) {
               client.flags &= ~ClientFlags::Admin;
            }
            else if (!!strcmp (cv_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (cv_password_key.str ())))) {
               client.flags &= ~ClientFlags::Admin;
               game.print ("Player %s had lost remote access to %s.", player->v.netname.chars (), product.name);
            }
         }
         else if (!(client.flags & ClientFlags::Admin) && !strings.isEmpty (cv_password_key.str ()) && !strings.isEmpty (cv_password.str ())) {
            if (strcmp (cv_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (cv_password_key.str ()))) == 0) {
               client.flags |= ClientFlags::Admin;
               game.print ("Player %s had gained full remote access to %s.", player->v.netname.chars (), product.name);
            }
         }
      }
   }
}

BotControl::BotControl () {
   m_ent = nullptr;
   m_djump = nullptr;

   m_isFromConsole = false;
   m_isMenuFillCommand = false;
   m_rapidOutput = false;
   m_menuServerFillTeam = 5;

   m_cmds.emplace ("add/addbot/add_ct/addbot_ct/add_t/addbot_t/addhs/addhs_t/addhs_ct", "add [difficulty[personality[team[model[name]]]]]", "Adding specific bot into the game.", &BotControl::cmdAddBot);
   m_cmds.emplace ("kick/kickone/kick_ct/kick_t/kickbot_ct/kickbot_t", "kick [team]", "Kicks off the random bot from the game.", &BotControl::cmdKickBot);
   m_cmds.emplace ("removebots/kickbots/kickall", "removebots [instant]", "Kicks all the bots from the game.", &BotControl::cmdKickBots);
   m_cmds.emplace ("kill/killbots/killall/kill_ct/kill_t", "kill [team]", "Kills the specified team / all the bots.", &BotControl::cmdKillBots);
   m_cmds.emplace ("fill/fillserver", "fill [team[count[difficulty[pesonality]]]]", "Fill the server (add bots) with specified parameters.", &BotControl::cmdFill);
   m_cmds.emplace ("vote/votemap", "vote [map_id]", "Forces all the bot to vote to specified map.", &BotControl::cmdVote);
   m_cmds.emplace ("weapons/weaponmode", "weapons [knife|pistol|shotgun|smg|rifle|sniper|standard]", "Sets the bots weapon mode to use", &BotControl::cmdWeaponMode);
   m_cmds.emplace ("menu/botmenu", "menu [cmd]", "Opens the main bot menu, or command menu if specified.", &BotControl::cmdMenu);
   m_cmds.emplace ("version/ver/about", "version [no arguments]", "Displays version information about bot build.", &BotControl::cmdVersion);
   m_cmds.emplace ("graphmenu/wpmenu/wptmenu", "graphmenu [noarguments]", "Opens and displays bots graph edtior.", &BotControl::cmdNodeMenu);
   m_cmds.emplace ("list/listbots", "list [noarguments]", "Lists the bots currently playing on server.", &BotControl::cmdList);
   m_cmds.emplace ("graph/g/wp/wpt/waypoint", "graph [help]", "Handles graph operations.", &BotControl::cmdNode);
   m_cmds.emplace ("cvars", "cvars [save|cvar]", "Display all the cvars with their descriptions.", &BotControl::cmdCvars);

   // declare the menus
   createMenus ();
}

void BotControl::handleEngineCommands () {
   collectArgs ();
   setIssuer (game.getLocalEntity ());

   setFromConsole (true);
   executeCommands ();
}

bool BotControl::handleClientCommands (edict_t *ent) {
   collectArgs ();
   setIssuer (ent);

   setFromConsole (true);
   return executeCommands ();
}

bool BotControl::handleMenuCommands (edict_t *ent) {
   collectArgs ();
   setIssuer (ent);

   setFromConsole (false);
   return ctrl.executeMenus ();
}

void BotControl::enableDrawModels (bool enable) {
   StringArray entities;

   entities.push ("info_player_start");
   entities.push ("info_player_deathmatch");
   entities.push ("info_vip_start");

   for (auto &entity : entities) {
      game.searchEntities ("classname", entity, [&enable] (edict_t *ent) {
         if (enable) {
            ent->v.effects &= ~EF_NODRAW;
         }
         else {
            ent->v.effects |= EF_NODRAW;
         }
         return EntitySearchResult::Continue;
      });
   }
}

void BotControl::createMenus () {
   auto keys = [] (int numKeys) -> int {
      int result = 0;

      for (int i = 0; i < numKeys; ++i) {
         result |= cr::bit (i);
      }
      result |= cr::bit (9);

      return result;
   };

   // bots main menu
   m_menus.emplace (
      Menu::Main, keys (4),
      "\\yMain Menu\\w\n\n"
      "1. Control Bots\n"
      "2. Features\n\n"
      "3. Fill Server\n"
      "4. End Round\n\n"
      "0. Exit",
      &BotControl::menuMain);


   // bots features menu
   m_menus.emplace (
      Menu::Features, keys (5),
      "\\yBots Features\\w\n\n"
      "1. Weapon Mode Menu\n"
      "2. Waypoint Menu\n"
      "3. Select Personality\n\n"
      "4. Toggle Debug Mode\n"
      "5. Command Menu\n\n"
      "0. Exit",
      &BotControl::menuFeatures);

   // bot control menu
   m_menus.emplace (
      Menu::Control, keys (5),
      "\\yBots Control Menu\\w\n\n"
      "1. Add a Bot, Quick\n"
      "2. Add a Bot, Specified\n\n"
      "3. Remove Random Bot\n"
      "4. Remove All Bots\n\n"
      "5. Remove Bot Menu\n\n"
      "0. Exit",
      &BotControl::menuControl);

   // weapon mode select menu
   m_menus.emplace (
      Menu::WeaponMode, keys (7),
      "\\yBots Weapon Mode\\w\n\n"
      "1. Knives only\n"
      "2. Pistols only\n"
      "3. Shotguns only\n"
      "4. Machine Guns only\n"
      "5. Rifles only\n"
      "6. Sniper Weapons only\n"
      "7. All Weapons\n\n"
      "0. Exit",
      &BotControl::menuWeaponMode);

   // personality select menu
   m_menus.emplace (
      Menu::Personality, keys (4),
      "\\yBots Personality\\w\n\n"
      "1. Random\n"
      "2. Normal\n"
      "3. Aggressive\n"
      "4. Careful\n\n"
      "0. Exit",
      &BotControl::menuPersonality);

   // difficulty select menu
   m_menus.emplace (
      Menu::Difficulty, keys (5),
      "\\yBots Difficulty Level\\w\n\n"
      "1. Newbie\n"
      "2. Average\n"
      "3. Normal\n"
      "4. Professional\n"
      "5. Godlike\n\n"
      "0. Exit",
      &BotControl::menuDifficulty);

   // team select menu
   m_menus.emplace (
      Menu::TeamSelect, keys (5),
      "\\ySelect a team\\w\n\n"
      "1. Terrorist Force\n"
      "2. Counter-Terrorist Force\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      &BotControl::menuTeamSelect);

   // terrorist model select menu
   m_menus.emplace (
      Menu::TerroristSelect, keys (5),
      "\\ySelect an appearance\\w\n\n"
      "1. Phoenix Connexion\n"
      "2. L337 Krew\n"
      "3. Arctic Avengers\n"
      "4. Guerilla Warfare\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      &BotControl::menuClassSelect);

   // counter-terrorist model select menu
   m_menus.emplace (
      Menu::CTSelect, keys (5),
      "\\ySelect an appearance\\w\n\n"
      "1. Seal Team 6 (DEVGRU)\n"
      "2. German GSG-9\n"
      "3. UK SAS\n"
      "4. French GIGN\n\n"
      "5. Auto-select\n\n"
      "0. Exit",
      &BotControl::menuClassSelect);

   // command menu
   m_menus.emplace (
      Menu::Commands, keys (4),
      "\\yBot Command Menu\\w\n\n"
      "1. Make Double Jump\n"
      "2. Finish Double Jump\n\n"
      "3. Drop the C4 Bomb\n"
      "4. Drop the Weapon\n\n"
      "0. Exit",
      &BotControl::menuCommands);

   // main waypoint menu
   m_menus.emplace (
      Menu::NodeMainPage1, keys (9),
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
      &BotControl::menuGraphPage1);

   // main waypoint menu (page 2)
   m_menus.emplace (
      Menu::NodeMainPage2, keys (9),
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
      &BotControl::menuGraphPage2);

   // select waypoint radius menu
   m_menus.emplace (
      Menu::NodeRadius, keys (9),
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
      &BotControl::menuGraphRadius);

   // waypoint add menu
   m_menus.emplace (
      Menu::NodeType, keys (9),
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
      &BotControl::menuGraphType);

   // set waypoint flag menu
   m_menus.emplace (
      Menu::NodeFlag, keys (5),
      "\\yToggle Waypoint Flags\\w\n\n"
      "1. Block with Hostage\n"
      "2. Terrorists Specific\n"
      "3. CTs Specific\n"
      "4. Use Elevator\n"
      "5. Sniper Point (\\yFor Camp Points Only!\\w)\n\n"
      "0. Exit",
      &BotControl::menuGraphFlag);

   // auto-path max distance
   m_menus.emplace (
      Menu::NodeAutoPath, keys (7),
      "\\yAutoPath Distance\\w\n\n"
      "1. Distance 0\n"
      "2. Distance 100\n"
      "3. Distance 130\n"
      "4. Distance 160\n"
      "5. Distance 190\n"
      "6. Distance 220\n"
      "7. Distance 250 (Default)\n\n"
      "0. Exit",
      &BotControl::menuAutoPathDistance);

   // path connections
   m_menus.emplace (
      Menu::NodePath, keys (3),
      "\\yCreate Path (Choose Direction)\\w\n\n"
      "1. Outgoing Path\n"
      "2. Incoming Path\n"
      "3. Bidirectional (Both Ways)\n\n"
      "0. Exit",
      &BotControl::menuGraphPath);

   // kick menus
   m_menus.emplace (Menu::KickPage1, 0x0, "", &BotControl::menuKickPage1);
   m_menus.emplace (Menu::KickPage2, 0x0, "", &BotControl::menuKickPage2);
   m_menus.emplace (Menu::KickPage3, 0x0, "", &BotControl::menuKickPage3);
   m_menus.emplace (Menu::KickPage4, 0x0, "", &BotControl::menuKickPage4);
}
