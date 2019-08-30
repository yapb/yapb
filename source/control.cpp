//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_display_menu_text ("yb_display_menu_text", "1", "Enables or disables display menu text, when players asks for menu. Useful only for Android.");
ConVar yb_password ("yb_password", "", "The value (password) for the setinfo key, if user set's correct password, he's gains access to bot commands and menus.", false, 0.0f, 0.0f, Var::Password);
ConVar yb_password_key ("yb_password_key", "_ybpw", "The name of setinfo key used to store password to bot commands and menus", false);

int BotControl::cmdAddBot () {
   enum args { alias = 1, difficulty, personality, team, model, name, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // this is duplicate error as in main bot creation code, but not to be silent
   if (!graph.length () || graph.hasChanged ()) {
      ctrl.msg ("There is no graph found or graph is changed. Cannot create bot.");
      return BotCommandResult::Handled;
   }

   // if team is specified, modify args to set team
   if (m_args[alias].find ("_ct", 0) != String::InvalidIndex) {
      m_args.set (team, "2");
   }
   else if (m_args[alias].find ("_t", 0) != String::InvalidIndex) {
      m_args.set (team, "1");
   }

   // if highskilled bot is requsted set personality to rusher and maxout difficulty
   if (m_args[alias].find ("hs", 0) != String::InvalidIndex) {
      m_args.set (difficulty, "4");
      m_args.set (personality, "1");
   }
   bots.addbot (m_args[name], m_args[difficulty], m_args[personality], m_args[team], m_args[model], true);

   return BotCommandResult::Handled;
}

int BotControl::cmdKickBot () {
   enum args { alias = 1, team, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if team is specified, kick from specified tram
   if (m_args[alias].find ("_ct", 0) != String::InvalidIndex || getInt (team) == 2 || getStr (team) == "ct") {
      bots.kickFromTeam (Team::CT);
   }
   else if (m_args[alias].find ("_t", 0) != String::InvalidIndex || getInt (team) == 1 || getStr (team) == "t") {
      bots.kickFromTeam (Team::Terrorist);
   }
   else {
      bots.kickRandom ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdKickBots () {
   enum args { alias = 1, instant, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // check if we're need to remove bots instantly
   auto kickInstant = getStr (instant) == "instant";

   // kick the bots
   bots.kickEveryone (kickInstant);

   return BotCommandResult::Handled;
}

int BotControl::cmdKillBots () {
   enum args { alias = 1, team, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if team is specified, kick from specified tram
   if (m_args[alias].find ("_ct", 0) != String::InvalidIndex || getInt (team) == 2 || getStr (team) == "ct") {
      bots.killAllBots (Team::CT);
   }
   else if (m_args[alias].find ("_t", 0) != String::InvalidIndex || getInt (team) == 1 || getStr (team) == "t") {
      bots.killAllBots (Team::Terrorist);
   }
   else {
      bots.killAllBots ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdFill () {
   enum args { alias = 1, team, count, difficulty, personality, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (team)) {
      return BotCommandResult::BadFormat;
   }
   bots.serverFill (getInt (team), hasArg (personality) ? getInt (personality) : -1, hasArg (difficulty) ? getInt (difficulty) : -1, hasArg (count) ? getInt (count) : -1);

   return BotCommandResult::Handled;
}

int BotControl::cmdVote () {
   enum args { alias = 1, mapid, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (mapid)) {
      return BotCommandResult::BadFormat;
   }
   int mapID = getInt (mapid);

   // loop through all players
   for (const auto &bot : bots) {
      bot->m_voteMap = mapID;
   }
   msg ("All dead bots will vote for map #%d", mapID);

   return BotCommandResult::Handled;
}

int BotControl::cmdWeaponMode () {
   enum args { alias = 1, type, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (type)) {
      return BotCommandResult::BadFormat;
   }
   Dictionary <String, int> modes;

   modes.push ("kinfe", 1);
   modes.push ("pistol", 2);
   modes.push ("shotgun", 3);
   modes.push ("smg", 4);
   modes.push ("rifle", 5);
   modes.push ("sniper", 6);
   modes.push ("standard", 7);

   auto mode = getStr (type);

   // check if selected mode exists
   if (!modes.exists (mode)) {
      return BotCommandResult::BadFormat;
   }
   bots.setWeaponMode (modes[mode]);

   return BotCommandResult::Handled;
}

int BotControl::cmdVersion () {
   auto hash = String (PRODUCT_GIT_HASH).substr (0, 8);
   auto author = String (PRODUCT_GIT_COMMIT_AUTHOR);

   // if no hash specified, set local one
   if (hash.startsWith ("unspe")) {
      hash = "local";
   }

   // if no commit author, set local one
   if (author.startsWith ("unspe")) {
      author = PRODUCT_EMAIL;
   }

   msg ("%s v%s (build %u)", PRODUCT_NAME, PRODUCT_VERSION, util.buildNumber ());
   msg ("  compiled: %s %s by %s", __DATE__, __TIME__, author.chars ());

   if (!hash.startsWith ("local")) {
      msg ("  commit: %scommit/%s", PRODUCT_COMMENTS, hash.chars ());
   }
   msg ("  url: %s", PRODUCT_URL);

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeMenu () {
   enum args { alias = 1, max };

   // graph editor is available only with editor
   if (!graph.hasEditor ()) {
      msg ("Unable to open graph editor without setting the editor player.");
      return BotCommandResult::Handled;
   }
   showMenu (Menu::NodeMainPage1);

   return BotCommandResult::Handled;
}

int BotControl::cmdMenu () {
   enum args { alias = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // reset the current menu
   showMenu (Menu::None);

   if (getStr (cmd) == "cmd" && util.isAlive (m_ent)) {
      showMenu (Menu::Commands);
   }
   else {
      showMenu (Menu::Main);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdList () {
   enum args { alias = 1, max };

   bots.listBots ();
   return BotCommandResult::Handled;
}

int BotControl::cmdCvars () {
   enum args { alias = 1, pattern, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   const auto &match = getStr (pattern);
   const bool isSave = match == "save";

   File cfg;

   // if save requested, dump cvars to yapb.cfg
   if (isSave) {
      cfg.open (strings.format ("%s/addons/yapb/conf/yapb.cfg", game.getModName ()), "wt");

      cfg.puts ("// Configuration file for %s\n\n", PRODUCT_SHORT_NAME);
   }

   for (const auto &cvar : game.getCvars ()) {
      if (cvar.info.empty ()) {
         continue;
      }

      if (!isSave && match != "empty" && !strstr (cvar.reg.name, match.chars ())) {
         continue;
      }

      // float value ?
      bool isFloat = !strings.isEmpty (cvar.self->str ()) && strstr (cvar.self->str (), ".");

      if (isSave) {
         cfg.puts ("//\n");
         cfg.puts ("// %s\n", String::join (cvar.info.split ("\n"), "\n//  ").chars ());
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
         game.print ("info: %s", cvar.info.chars ());

         game.print (" ");
      }
   }

   if (isSave) {
      ctrl.msg ("Bots cvars has been written to file.");


      cfg.close ();
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNode () {
   enum args { root, alias, cmd, cmd2, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // graph editor supported only with editor
   if (game.isDedicated () && !graph.hasEditor () && getStr (cmd) != "acquire_editor") {
      msg ("Unable to use graph edit commands without setting graph editor player. Please use \"graph acquire_editor\" to acquire rights for graph editing.");
      return BotCommandResult::Handled;
   }

   // should be moved to class?
   static Dictionary <String, BotCmd> commands;
   static StringArray descriptions;

   // fill only once
   if (descriptions.empty ()) {

      // separate function
      auto pushGraphCmd = [&] (String cmd, String format, String help, Handler handler) -> void {
         BotCmd botCmd (cr::move (cmd), cr::move (format), cr::move (help), cr::move (handler));

         commands.push (cmd, cr::move (botCmd));
         descriptions.push (cmd);
      };

      // add graph commands
      pushGraphCmd ("on", "on [display|auto|noclip|models]", "Enables displaying of graph, nodes, noclip cheat", &BotControl::cmdNodeOn);
      pushGraphCmd ("off", "off [display|auto|noclip|models]", "Disables displaying of graph, auto adding nodes, noclip cheat", &BotControl::cmdNodeOff);
      pushGraphCmd ("menu", "menu [noarguments]", "Opens and displays bots graph edtior.", &BotControl::cmdNodeMenu);
      pushGraphCmd ("add", "add [noarguments]", "Opens and displays graph node add menu.", &BotControl::cmdNodeAdd);
      pushGraphCmd ("addbasic", "menu [noarguments]", "Adds basic nodes such as player spawn points, goals and ladders.", &BotControl::cmdNodeAddBasic);
      pushGraphCmd ("save", "save [noarguments]", "Save graph file to disk.", &BotControl::cmdNodeSave);
      pushGraphCmd ("load", "load [noarguments]", "Load graph file from disk.", &BotControl::cmdNodeLoad);
      pushGraphCmd ("erase", "erase [iamsure]", "Erases the graph file from disk.", &BotControl::cmdNodeErase);
      pushGraphCmd ("delete", "delete [nearest|index]", "Deletes single graph node from map.", &BotControl::cmdNodeDelete);
      pushGraphCmd ("check", "check [noarguments]", "Check if graph working correctly.", &BotControl::cmdNodeCheck);
      pushGraphCmd ("cache", "cache [nearest|index]", "Caching node for future use.", &BotControl::cmdNodeCache);
      pushGraphCmd ("clean", "clean [all|nearest|index]", "Clean useless path connections from all or single node.", &BotControl::cmdNodeClean);
      pushGraphCmd ("setradius", "setradius [radius] [nearest|index]", "Sets the radius for node.", &BotControl::cmdNodeSetRadius);
      pushGraphCmd ("flags", "flags [noarguments]", "Open and displays menu for modifying flags for nearest point.", &BotControl::cmdNodeSetFlags);
      pushGraphCmd ("teleport", "teleport [index]", "Teleports player to specified node index.", &BotControl::cmdNodeTeleport);
      pushGraphCmd ("upload", "upload [id]", "Uploads created graph to graph database.", &BotControl::cmdNodeUpload);

      // add path commands
      pushGraphCmd ("path_create", "path_create [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathCreate);
      pushGraphCmd ("path_create_in", "path_create_in [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathCreate);
      pushGraphCmd ("path_create_out", "path_create_out [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathCreate);
      pushGraphCmd ("path_create_both", "path_create_both [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathCreate);
      pushGraphCmd ("path_delete", "path_create_both [noarguments]", "Opens and displays path creation menu.", &BotControl::cmdNodePathDelete);
      pushGraphCmd ("path_set_autopath", "path_set_autoath [max_distance]", "Opens and displays path creation menu.", &BotControl::cmdNodePathSetAutoDistance);

      // camp points iterator
      pushGraphCmd ("iterate_camp", "iterate_camp [begin|end|next]", "Allows to go through all camp points on map.", &BotControl::cmdNodeIterateCamp);

      // remote graph editing stuff
      if (game.isDedicated ()) {
         pushGraphCmd ("acquire_editor", "acquire_editor [max_distance]", "Acquires rights to edit graph on dedicated server.", &BotControl::cmdNodeAcquireEditor);
         pushGraphCmd ("release_editor", "acquire_editor [max_distance]", "Releases graph editing rights.", &BotControl::cmdNodeAcquireEditor);
      }
   }
   if (commands.exists (getStr (cmd))) {
      auto item = commands[getStr (cmd)];

      // graph have only bad format return status
      int status = (this->*item.handler) ();

      if (status == BotCommandResult::BadFormat) {
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
         msg ("Currently Graph Status %s", graph.hasEditFlag (GraphEdit::On) ? "Enabled" : "Disabled");
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeOn () {
   enum args { alias = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // enable various features of editor
   if (getStr (option) == "empty" || getStr (option) == "display" || getStr (option) == "models") {
      graph.setEditFlag (GraphEdit::On);
      enableDrawModels (true);

      msg ("Graph editor has been enabled.");
   }
   else if (getStr (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_NOCLIP;

      graph.setEditFlag (GraphEdit::On | GraphEdit::Noclip);
      enableDrawModels (true);

      msg ("Graph editor has been enabled with noclip mode.");
   }
   else if (getStr (option) == "auto") {
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
   enum args { graph_cmd = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // enable various features of editor
   if (getStr (option) == "empty" || getStr (option) == "display") {
      graph.clearEditFlag (GraphEdit::On | GraphEdit::Auto | GraphEdit::Noclip);
      enableDrawModels (false);

      msg ("Graph editor has been disabled.");
   }
   else if (getStr (option) == "models") {
      enableDrawModels (false);

      msg ("Graph editor has disabled spawn points highlighting.");
   }
   else if (getStr (option) == "noclip") {
      m_ent->v.movetype = MOVETYPE_WALK;
      graph.clearEditFlag (GraphEdit::Noclip);

      msg ("Graph editor has disabled noclip mode.");
   }
   else if (getStr (option) == "auto") {
      graph.clearEditFlag (GraphEdit::Auto);
      msg ("Graph editor has disabled auto add node mode.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAdd () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // show the menu
   showMenu (Menu::NodeType);
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAddBasic () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   graph.addBasic ();
   msg ("Basic graph nodes was added.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSave () {
   enum args { graph_cmd = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // if no check is set save anyway
   if (getStr (option) == "nocheck") {
      graph.saveGraphData ();

      msg ("All nodes has been saved and written to disk (IGNORING QUALITY CONTROL).");
   }
   else if (getStr (option) == "old") {
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
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

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
   enum args { graph_cmd = 1, cmd, iamsure, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // prevent accidents when graph are deleted unintentionally
   if (getStr (iamsure) == "iamsure") {
      graph.eraseFromDisk ();
   }
   else {
      msg ("Please, append \"iamsure\" as parameter to get graph erased from the disk.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeDelete () {
   enum args { graph_cmd = 1, cmd, nearest, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (getStr (nearest) == "empty" || getStr (nearest) == "nearest") {
      graph.erase (kInvalidNodeIndex);
   }
   else {
      int index = getInt (nearest);

      // check for existence
      if (graph.exists (index)) {
         graph.erase (index);
         msg ("Node #%d has beed deleted.", index);
      }
      else {
         msg ("Could not delete node #%d.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeCheck () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // check if nodes are ok
   if (graph.checkNodes (true)) {
      msg ("Graph seems to be OK.");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeCache () {
   enum args { graph_cmd = 1, cmd, nearest, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "neareset" or nothing passed delete neareset, else delete by index
   if (getStr (nearest) == "empty" || getStr (nearest) == "nearest") {
      graph.cachePoint (kInvalidNodeIndex);

      msg ("Nearest node has been put into the memory.");
   }
   else {
      int index = getInt (nearest);

      // check for existence
      if (graph.exists (index)) {
         graph.cachePoint (index);
         msg ("Node #%d has been put into the memory.", index);
      }
      else {
         msg ("Could not put node #%d into the memory.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeClean () {
   enum args { graph_cmd = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // if "all" passed clean up all the paths
   if (getStr (option) == "all") {
      int removed = 0;

      for (int i = 0; i < graph.length (); ++i) {
         removed += graph.clearConnections (i);
      }
      msg ("Done. Processed %d nodes. %d useless paths was cleared.", graph.length (), removed);
   }
   else if (getStr (option) == "empty" || getStr (option) == "nearest") {
      int removed = graph.clearConnections (graph.getEditorNeareset ());

      msg ("Done. Processed node #%d. %d useless paths was cleared.", graph.getEditorNeareset (), removed);
   }
   else {
      int index = getInt (option);

      // check for existence
      if (graph.exists (index)) {
         int removed = graph.clearConnections (index);

         msg ("Done. Processed node #%d. %d useless paths was cleared.", index, removed);
      }
      else {
         msg ("Could not process node #%d clearance.", index);
      }
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSetRadius () {
   enum args { graph_cmd = 1, cmd, radius, index, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // radius is a must
   if (!hasArg (radius)) {
      return BotCommandResult::BadFormat;
   }
   int radiusIndex = kInvalidNodeIndex;

   if (getStr (index) == "empty" || getStr (index) == "nearest") {
      radiusIndex = graph.getEditorNeareset ();
   }
   else {
      radiusIndex = getInt (index);
   }
   float value = getStr (radius).float_ ();

   graph.setRadius (radiusIndex, value);
   msg ("Node #%d has been set to radius %.2f.", radiusIndex, value);

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeSetFlags () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   //show the flag menu
   showMenu (Menu::NodeFlag);
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeTeleport () {
   enum args { graph_cmd = 1, cmd, teleport_index, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (teleport_index)) {
      return BotCommandResult::BadFormat;
   }
   int index = getInt (teleport_index);

   // check for existence
   if (graph.exists (index)) {
      engfuncs.pfnSetOrigin (graph.getEditor (), graph[index].origin);

      msg ("You have been teleported to node #%d.", index);

      // turn graph on
      graph.setEditFlag (GraphEdit::On | GraphEdit::Noclip);
   }
   else {
      msg ("Could not teleport to node #%d.", index);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathCreate () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // choose the direction for path creation
   if (m_args[cmd].find ("_both", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Bidirectional);
   }
   else if (m_args[cmd].find ("_in", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Incoming);
   }
   else if (m_args[cmd].find ("_out", 0) != String::InvalidIndex) {
      graph.pathCreate (PathConnection::Outgoing);
   }
   else {
      showMenu (Menu::NodePath);
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathDelete () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // delete the patch
   graph.erasePath ();

   return BotCommandResult::Handled;
}

int BotControl::cmdNodePathSetAutoDistance () {
   enum args { graph_cmd = 1, cmd, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);
   showMenu (Menu::NodeAutoPath);

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeAcquireEditor () {
   enum args { graph_cmd = 1, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (game.isNullEntity (m_ent)) {
      msg ("This command should not be executed from HLDS console.");
      return BotCommandResult::Handled;
   }

   if (graph.hasEditor ()) {
      msg ("Sorry, players \"%s\" already acquired rights to edit graph on this server.", STRING (graph.getEditor ()->v.netname));
      return BotCommandResult::Handled;
   }
   graph.setEditor (m_ent);
   msg ("You're acquired rights to edit graph on this server. You're now able to use graph commands.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeReleaseEditor () {
   enum args { graph_cmd = 1, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!graph.hasEditor ()) {
      msg ("No one is currently has rights to edit. Nothing to release.");
      return BotCommandResult::Handled;
   }
   graph.setEditor (nullptr);
   msg ("Graph editor rights freed. You're now not able to use graph commands.");

   return BotCommandResult::Handled;
}

int BotControl::cmdNodeUpload () {
   enum args { graph_cmd = 1, cmd, id, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   if (!hasArg (id)) {
      return BotCommandResult::BadFormat;
   }

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

   extern ConVar yb_graph_url;

   // try to upload the file
   if (http.uploadFile (strings.format ("%s/", yb_graph_url.str ()), strings.format ("%sgraph/%s.graph", graph.getDataDirectory (false), game.getMapName ()))) {
      msg ("Graph file was uploaded and Issue Request has been created for review.");
      msg ("As soon as database administrator review your upload request, your graph will");
      msg ("be available to download for all YaPB users. You can check your issue request at:");
      msg ("URL: https://github.com/jeefo/yapb-graph/issues");
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
      msg ("Something went wrong with uploading. Come back later. (%s)", status.chars ());
      msg ("\n");
      if (code == HttpClientResult::Forbidden) {
         msg ("You should create issue-request manually for this graph");
         msg ("as it's already exists in database, can't overwrite. Sorry...");
      }
      else {
         msg ("There is an internal error, or somethingis totally wrong with");
         msg ("your files, and they are not passed sanity checks. Sorry...");
      }
      msg ("\n");
   }
   return BotCommandResult::Handled;
}

int BotControl::cmdNodeIterateCamp () {
   enum args { graph_cmd = 1, cmd, option, max };

   // adding more args to args array, if not enough passed
   fixMissingArgs (max);

   // turn graph on
   graph.setEditFlag (GraphEdit::On);

   // get the option descriping operation
   auto op = getStr (option);

   if (op != "begin" && op != "end" && op != "next") {
      return BotCommandResult::BadFormat;
   }

   if ((op == "next" || op == "end") && m_campPointsIndex == kInvalidNodeIndex) {
      msg ("Before calling for 'next' / 'end' camp point, you should hit 'begin'.");
      return BotCommandResult::Handled;
   }
   else if (op == "begin" && m_campPointsIndex != kInvalidNodeIndex) {
      msg ("Before calling for 'begin' camp point, you should hit 'end'.");
      return BotCommandResult::Handled;
   }
   
   if (op == "end") {
      m_campPointsIndex = kInvalidNodeIndex;
      m_campPoints.clear ();
   }
   else if (op == "next") {
      if (m_campPointsIndex < static_cast <int> (m_campPoints.length ())) {
         Vector origin = graph[m_campPoints[m_campPointsIndex]].origin;

         if (graph[m_campPoints[m_campPointsIndex]].flags & NodeFlag::Crouch) {
            origin.z += 23.0f;
         }
         engfuncs.pfnSetOrigin (m_ent, origin);
      }
      else {
         m_campPoints.clear ();
         m_campPointsIndex = kInvalidNodeIndex;

         msg ("Finished iterating camp spots.");
      }
      ++m_campPointsIndex;
   }
   else if (op == "begin") {
      for (int i = 0; i < graph.length (); ++i) {
         if (graph[i].flags & NodeFlag::Camp) {
            m_campPoints.push (i);
         }
      }
      m_campPointsIndex = 0;
   }
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
      extern ConVar yb_debug;
      yb_debug.set (yb_debug.int_ () ^ 1);

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
   case 1: {
      int terrPoints = 0;
      int ctPoints = 0;
      int goalPoints = 0;
      int rescuePoints = 0;
      int campPoints = 0;
      int sniperPoints = 0;
      int noHostagePoints = 0;

      for (int i = 0; i < graph.length (); ++i) {
         const Path &path = graph[i];

         if (path.flags & NodeFlag::TerroristOnly) {
            ++terrPoints;
         }

         if (path.flags & NodeFlag::CTOnly) {
            ++ctPoints;
         }

         if (path.flags & NodeFlag::Goal) {
            ++goalPoints;
         }

         if (path.flags & NodeFlag::Rescue) {
            ++rescuePoints;
         }

         if (path.flags & NodeFlag::Camp) {
            ++campPoints;
         }

         if (path.flags & NodeFlag::Sniper) {
            ++sniperPoints;
         }

         if (path.flags & NodeFlag::NoHostage) {
            ++noHostagePoints;
         }
      }
      msg ("Nodes: %d - T Points: %d\n"
         "CT Points: %d - Goal Points: %d\n"
         "Rescue Points: %d - Camp Points: %d\n"
         "Block Hostage Points: %d - Sniper Points: %d\n",
         graph.length (), terrPoints, ctPoints, goalPoints, rescuePoints, campPoints, noHostagePoints, sniperPoints);

      showMenu (Menu::NodeMainPage2);
   } break;

   case 2:
      graph.setEditFlag (GraphEdit::On);

      if (graph.hasEditFlag (GraphEdit::Auto)) {
         graph.clearEditFlag (GraphEdit::Auto);
      }
      else {
         graph.setEditFlag (GraphEdit::Auto);
      }
      msg ("Auto-Add-Nodes %s", graph.hasEditFlag (GraphEdit::Auto) ? "Enabled" : "Disabled");

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
      graph.setEditFlag (GraphEdit::On | GraphEdit::Noclip);
      showMenu (Menu::NodeMainPage2);
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
   float result = 0.0f;

   if (item >= 1 && item <= 7) {
      result = distances[item - 1];
      graph.setAutoPathDistance (result);
   }

   if (cr::fzero (result)) {
      msg ("Autopathing is now disabled.");
   }
   else {
      msg ("Autopath distance is set to %.2f.", result);
   }
   showMenu (Menu::NodeAutoPath);

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

   // handle only "yb" and "yapb" commands
   if (m_args[0] != "yb" && m_args[0] != "yapb") {
      return false;
   }
   Client &client = util.getClient (game.indexOfPlayer (m_ent));

   // do not allow to execute stuff for non admins
   if (m_ent != game.getLocalEntity () && !(client.flags & ClientFlags::Admin)) {
      msg ("Access to %s commands is restricted.", PRODUCT_SHORT_NAME);
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
   if (m_args.length () > 0 && m_args[1] == "help") {
      for (auto &item : m_cmds) {
         if (aliasMatch (item.name, m_args[2], cmd)) {
            msg ("Command: \"%s %s\"\nFormat: %s\nHelp: %s", m_args[0].chars (), cmd.chars (), item.format.chars (), item.help.chars ());

            String aliases;

            for (auto &alias : item.name.split ("/")) {
               aliases.appendf ("%s, ", alias.chars ());
            }
            aliases.rtrim (", ");
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
         msg ("   %s - %s", item.name.split ("/")[0].chars (), item.help.chars ());
      }
      return true;
   }

   // first search for a actual cmd
   for (auto &item : m_cmds) {
      auto root = m_args[0].chars ();

      if (aliasMatch (item.name, m_args[1], cmd)) {
         auto alias = cmd.chars ();

         switch ((this->*item.handler) ()) {
         case BotCommandResult::Handled:
         default:
            break;

         case BotCommandResult::ListenServer:
            msg ("Command \"%s %s\" is only available from the listenserver console.", root, alias);
            break;

         case BotCommandResult::BadFormat:
            msg ("Incorrect usage of \"%s %s\" command. Correct usage is:\n\n\t%s\n\nPlease type \"%s help %s\" to get more information.", root, alias, item.format.chars (), root, alias);
            break;
         }

         m_isFromConsole = false;
         return true;
      }
   }
   msg ("Unrecognized command: %s", m_args[1].chars ());
   return true;
}

bool BotControl::executeMenus () {
   if (!util.isPlayer (m_ent) || game.isBotCmd ()) {
      return false;
   }
   auto &issuer = util.getClient (game.indexOfPlayer (m_ent));

   // check if it's menu select, and some key pressed
   if (getStr (0) != "menuselect" || getStr (1).empty () || issuer.menu == Menu::None) {
      return false;
   }

   // let's get handle
   for (auto &menu : m_menus) {
      if (menu.ident == issuer.menu) {
         return (this->*menu.handler) (getStr (1).int_ ());
      }
   }
   return false;
}

void BotControl::showMenu (int id) {
   static bool s_menusParsed = false;

   // make menus looks like we need only once
   if (!s_menusParsed) {
      for (auto &parsed : m_menus) {
         const String &translated = conf.translate (parsed.text.chars ());

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
         auto text = (game.is (GameFlags::Xash3D | GameFlags::Mobility) && !yb_display_menu_text.bool_ ()) ? " " : display.text.chars ();
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

   String menus;
   menus.assignf ("\\yBots Remove Menu (%d/4):\\w\n\n", page);

   int menuKeys = (page == 4) ? cr::bit (9) : (cr::bit (8) | cr::bit (9));
   int menuKey = (page - 1) * 8;

   for (int i = menuKey; i < page * 8; ++i) {
      auto bot = bots[i];

      // check for fakeclient bit, since we're clear it upon kick, but actual bot struct destroyed after client disconnected
      if (bot != nullptr && (bot->pev->flags & FL_FAKECLIENT)) {
         menuKeys |= cr::bit (cr::abs (i - menuKey));
         menus.appendf ("%1.1d. %s%s\n", i - menuKey + 1, STRING (bot->pev->netname), bot->m_team == Team::CT ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else {
         menus.appendf ("\\d %1.1d. Not a Bot\\w\n", i - menuKey + 1);
      }
   }
   menus.appendf ("\n%s 0. Back", (page == 4) ? "" : " 9. More...\n");

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
   const String &key = yb_password_key.str ();
   const String &password = yb_password.str ();

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
            if (strings.isEmpty (yb_password_key.str ()) && strings.isEmpty (yb_password.str ())) {
               client.flags &= ~ClientFlags::Admin;
            }
            else if (!!strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ())))) {
               client.flags &= ~ClientFlags::Admin;
               game.print ("Player %s had lost remote access to %s.", STRING (player->v.netname), PRODUCT_SHORT_NAME);
            }
         }
         else if (!(client.flags & ClientFlags::Admin) && !strings.isEmpty (yb_password_key.str ()) && !strings.isEmpty (yb_password.str ())) {
            if (strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ()))) == 0) {
               client.flags |= ClientFlags::Admin;
               game.print ("Player %s had gained full remote access to %s.", STRING (player->v.netname), PRODUCT_SHORT_NAME);
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
   m_campPointsIndex = kInvalidNodeIndex;

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
