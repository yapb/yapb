//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

int Control::DebugLevel () const {
  return cv_debug.As<int> ();
}

bool Control::IsDebug (int level) const {
  return DebugLevel () >= level;
}

CommandResult Control::CmdAddBot () {
  enum args {
    alias = 1,
    difficulty,
    personality,
    team,
    model,
    name,
    max
  };

  // this is duplicate error as in main bot creation code, but not to be silent
  if (!graph.Length () || graph.HasChanged ()) {
    Msg ("There is no graph found or graph is changed. Cannot create bot.");
    return CommandResult::Handled;
  }

  // give a chance to use additional args
  args_.resize (max);

  // if team is specified, modify args to set team
  if (Arg<ystl::StringRef> (alias).ends_with ("_ct")) {
    args_.set (team, "2");
  }
  else if (Arg<ystl::StringRef> (alias).ends_with ("_t")) {
    args_.set (team, "1");
  }

  // if high-skilled bot is requested set personality to rusher and max-out difficulty
  if (Arg<ystl::StringRef> (alias).contains ("addhs")) {
    args_.set (difficulty, "4");
    args_.set (personality, "1");
  }
  bots.Addbot (Arg<ystl::StringRef> (name), Arg<ystl::StringRef> (difficulty), Arg<ystl::StringRef> (personality), Arg<ystl::StringRef> (team),
    Arg<ystl::StringRef> (model), true);

  return CommandResult::Handled;
}

CommandResult Control::CmdKickBot () {
  enum args {
    alias = 1,
    team
  };

  // if team is specified, kick from specified tram
  if (Arg<ystl::StringRef> (alias).ends_with ("_ct") || Arg<int> (team) == 2 || Arg<ystl::StringRef> (team) == "ct") {
    bots.KickFromTeam (Team::CT);
  }
  else if (Arg<ystl::StringRef> (alias).ends_with ("_t") || Arg<int> (team) == 1 || Arg<ystl::StringRef> (team) == "t") {
    bots.KickFromTeam (Team::Terrorist);
  }
  else {
    bots.BalancedKickRandom (true);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdKickBots () {
  enum args {
    alias = 1,
    instant,
    team
  };

  // check if we're need to remove bots instantly
  const auto kick_instant = Arg<ystl::StringRef> (instant) == "instant";

  // if team is specified, kick from specified tram
  if (Arg<ystl::StringRef> (alias).ends_with ("_ct") || Arg<int> (team) == 2 || Arg<ystl::StringRef> (team) == "ct") {
    bots.KickFromTeam (Team::CT, true);
  }
  else if (Arg<ystl::StringRef> (alias).ends_with ("_t") || Arg<int> (team) == 1 || Arg<ystl::StringRef> (team) == "t") {
    bots.KickFromTeam (Team::Terrorist, true);
  }
  else {
    bots.KickEveryone (kick_instant);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdKillBots () {
  enum args {
    alias = 1,
    team,
    silent,
    max
  };

  // do not issue any messages
  bool silent_kill = HasArg (silent) && Arg<ystl::StringRef> (silent).starts_with ("si");

  // if team is specified, kick from specified tram
  if (Arg<ystl::StringRef> (alias).ends_with ("_ct") || Arg<int> (team) == 2 || Arg<ystl::StringRef> (team) == "ct") {
    bots.KillAllBots (Team::CT, silent_kill);
  }
  else if (Arg<ystl::StringRef> (alias).ends_with ("_t") || Arg<int> (team) == 1 || Arg<ystl::StringRef> (team) == "t") {
    bots.KillAllBots (Team::Terrorist, silent_kill);
  }
  else {
    bots.KillAllBots (Team::Invalid, silent_kill);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdFill () {
  enum args {
    alias = 1,
    team,
    count,
    difficulty,
    personality
  };

  if (!HasArg (team)) {
    return CommandResult::BadFormat;
  }
  bots.ServerFill (Arg<CSTeam> (team), HasArg (personality) ? Arg<Personality> (personality) : Personality::Invalid,
    HasArg (difficulty) ? Arg<Difficulty> (difficulty) : Difficulty::Invalid, HasArg (count) ? Arg<int> (count) : -1);

  return CommandResult::Handled;
}

CommandResult Control::CmdVote () {
  enum args {
    alias = 1,
    mapid
  };

  if (!HasArg (mapid)) {
    return CommandResult::BadFormat;
  }
  const int map_id = Arg<int> (mapid);

  // loop through all players
  for (auto &bot : bots) {
    bot.vote_map_ = map_id;
  }
  Msg ("All dead bots will vote for map #%d.", map_id);

  return CommandResult::Handled;
}

CommandResult Control::CmdWeaponMode () {
  enum args {
    alias = 1,
    type
  };

  if (!HasArg (type)) {
    return CommandResult::BadFormat;
  }
  static ystl::HashMap<ystl::String, int> modes {
    { "knife",    1 },
    { "pistol",   2 },
    { "shotgun",  3 },
    { "smg",      4 },
    { "rifle",    5 },
    { "sniper",   6 },
    { "standard", 7 }
  };
  auto mode = Arg<ystl::StringRef> (type);

  // check if selected mode exists
  if (!modes.exists (mode)) {
    return CommandResult::BadFormat;
  }
  bots.SetWeaponMode (modes[mode]);

  return CommandResult::Handled;
}

CommandResult Control::CmdFun () {
  enum args {
    alias = 1,
    modeName
  };

  if (!HasArg (modeName)) {
    return CommandResult::BadFormat;
  }
  const auto mode = fun_mode.ParseMode (Arg<ystl::StringRef> (modeName));

  // check if selected mode exists
  if (mode == FunModeId::Invalid) {
    return CommandResult::BadFormat;
  }
  fun_mode.SetMode (mode);
  Msg (fun_mode.MessageOf (mode).chars ());

  return CommandResult::Handled;
}

CommandResult Control::CmdVersion () {
  constexpr auto &bi = product.bi;

  Msg ("%s v%s (ID %s)", product.name, product.version, bi.id);
  Msg ("   by %s (%s)", product.author, product.email);
  Msg ("   %s", product.url);
  Msg ("compiled: %s on %s with %s", product.dtime, bi.machine, bi.compiler);

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeMenu () {
  enum args {
    alias = 1
  };

  // graph editor is available only with editor
  if (!graph.HasEditor ()) {
    Msg ("Unable to open graph editor without setting the editor player.");
    return CommandResult::Handled;
  }
  ShowMenu (MenuId::NodeMainPage1);

  return CommandResult::Handled;
}

CommandResult Control::CmdMenu () {
  enum args {
    alias = 1,
    cmd
  };

  // reset the current menu
  CloseMenu ();

  if (Arg<ystl::StringRef> (cmd) == "cmd" && game.IsAliveEntity (ent_)) {
    ShowMenu (MenuId::Commands);
  }
  else {
    ShowMenu (MenuId::Main);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdList () {
  enum args {
    alias = 1
  };

  bots.ListBots ();
  return CommandResult::Handled;
}

CommandResult Control::CmdCvars () {
  enum args {
    alias = 1,
    pattern
  };

  auto match = Arg<ystl::StringRef> (pattern);

  // stop printing if executed once more
  print_queue_.clear ();

  // revert all the cvars to their default values
  if (match == "defaults") {
    Msg ("Bots cvars has been reverted to their default values.");

    for (const auto &cvar : game.GetCvars ()) {
      if (!cvar.self || !cvar.self->ptr || cvar.type == Var::GameRef) {
        continue;
      }

      // set depending on cvar type
      if (cvar.bounded) {
        cvar.self->Set (cvar.initial);
      }
      else {
        cvar.self->Set (cvar.init.chars ());
      }
    }
    cv_quota.Revert (); // quota should be reverted instead of regval

    return CommandResult::Handled;
  }

  const bool is_save_main = match == "save";
  const bool is_save_map = match == "save_map";

  const bool is_save = is_save_main || is_save_map;

  ystl::File cfg {};

  // if save requested, dump cvars to main config
  if (is_save) {
    auto cfg_path =
      ystl::strings.join_path (bstor.GetRunningPath (), folders.config, ystl::strings.format ("%s.%s", product.name_lower, kConfigExtension));

    if (is_save_map) {
      cfg_path = ystl::strings.join_path (
        bstor.GetRunningPath (), folders.config, "maps", ystl::strings.format ("%s.%s", game.GetMapName (), kConfigExtension));
    }
    cfg.open (cfg_path, "wt");

    if (!cfg) {
      Msg ("Unable to write cvars to config file. ystl::File not accessible");
      return CommandResult::Handled;
    }
    cfg.puts ("//\n");
    cfg.puts ("// @package: %s\n", product.name);
    cfg.puts ("// @version: %s\n", product.version);
    cfg.puts ("// @author: %s\n", product.author);
    cfg.puts ("// @filename: %s.cfg\n", is_save_map ? game.GetMapName () : product.name_lower);
    cfg.puts ("// \n");
    cfg.puts ("// %s configuration file for %s. Can be executed using the 'exec' command.\n", is_save_map ? "Map" : "Main", product.name);
    cfg.puts ("//\n");
  }
  else {
    SetRapidOutput (true);
  }

  for (const auto &cvar : game.GetCvars ()) {
    if (cvar.info.empty () || !cvar.self || !cvar.self->ptr) {
      continue;
    }

    if (!is_save && !match.empty () && !ystl::StringRef (cvar.reg.name).contains (match)) {
      continue;
    }

    auto val = cvar.self->As<ystl::StringRef> ();

    // float value ?
    bool is_float = !val.empty () && val.find (".") != ystl::StringRef::InvalidIndex;

    if (is_save) {
      cfg.puts ("//\n");
      cfg.puts ("// %s\n", ystl::String::join (cvar.info.split ("\n"), "\n//  "));
      cfg.puts ("// ---\n");

      if (cvar.bounded) {
        if (is_float) {
          cfg.puts ("// Default: \"%.1f\", Min: \"%.1f\", Max: \"%.1f\"\n", cvar.initial, cvar.min, cvar.max);
        }
        else {
          cfg.puts ("// Default: \"%i\", Min: \"%i\", Max: \"%i\"\n", static_cast<int> (cvar.initial), static_cast<int> (cvar.min),
            static_cast<int> (cvar.max));
        }
      }
      else {
        cfg.puts ("// Default: \"%s\"\n", cvar.self->As<ystl::StringRef> ());
      }
      cfg.puts ("// \n");

      if (cvar.bounded) {
        if (is_float) {
          cfg.puts ("%s \"%.1f\"\n", cvar.reg.name, cvar.self->As<float> ());
        }
        else {
          cfg.puts ("%s \"%i\"\n", cvar.reg.name, cvar.self->As<int> ());
        }
      }
      else {
        cfg.puts ("%s \"%s\"\n", cvar.reg.name, cvar.self->As<ystl::StringRef> ());
      }
      cfg.puts ("\n");
    }
    else {
      Msg ("name: %s", cvar.reg.name);
      Msg ("info: %s", conf.Translate (cvar.info));

      Msg (" ");
    }
  }
  SetRapidOutput (false);

  if (is_save) {
    Msg ("Bots cvars has been written to file.");
    cfg.close ();
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdShowCustom () {
  enum args {
    alias = 1
  };

  conf.ShowCustomValues ();

  return CommandResult::Handled;
}

CommandResult Control::CmdNode () {
  enum args {
    root,
    alias,
    cmd
  };

  static constexpr ystl::FixedArray<ystl::StringRef, 12> allowed_on_hlds { "acquire_editor", "upload", "save", "load", "help", "erase",
    "erase_training", "fileinfo", "check", "import", "export", "stats" };

  auto is_allowed_on_hlds = [] (ystl::StringRef str) -> bool {
    return !!ystl::find (allowed_on_hlds, str);
  };

  // graph editor supported only with editor
  if (game.IsDedicatedServer () && !graph.HasEditor () && !is_allowed_on_hlds (Arg<ystl::StringRef> (cmd))) {
    Msg ("Unable to use graph edit commands without setting graph editor player. Please use \"graph acquire_editor\" to acquire rights for "
         "graph editing.");
    return CommandResult::Handled;
  }

  // find the graph command in m_cmds to get its subcommands
  for (auto &item : cmds_) {
    if (item.HasSubcommands () && [&] {
          const ystl::StringRef aliases (item.name.chars (), item.name.size ());
          size_t start = 0;

          while (start <= aliases.size ()) {
            const auto end = aliases.find ("/", start);
            const auto seg_end = end == ystl::String::InvalidIndex ? aliases.size () : end;

            if (seg_end - start == args_[alias].size () && aliases.substr (start, seg_end - start) == args_[alias]) {
              return true;
            }
            if (end == ystl::String::InvalidIndex) {
              break;
            }
            start = end + 1;
          }
          return false;
        }()) {
      auto result = DispatchSubcommand (item, 2);

      // add graph status message when listing commands
      if (!HasArg (cmd) || (Arg<ystl::StringRef> (cmd) != "help" && !item.subcommands.empty ())) {
        bool found_match = false;

        for (const auto &subcmd : item.subcommands) {
          for (auto &alias : subcmd.name.split ("/")) {
            if (alias == Arg<ystl::StringRef> (cmd)) {
              found_match = true;
              break;
            }
          }
          if (found_match)
            break;
        }
        if (!found_match && !HasArg (cmd)) {
          Msg ("Currently Graph Status %s", graph.HasEditFlag (GraphEdit::On) ? "Enabled" : "Disabled");
        }
      }
      return result;
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeOn () {
  enum args {
    alias = 1,
    cmd,
    option
  };

  // enable various features of editor
  if (Arg<ystl::StringRef> (option).empty () || Arg<ystl::StringRef> (option) == "display" || Arg<ystl::StringRef> (option) == "models") {
    graph.SetEditFlag (GraphEdit::On);
    EnableDrawModels (true);

    Msg ("Graph editor has been enabled.");
  }
  else if (Arg<ystl::StringRef> (option) == "noclip") {
    if (!game.IsNullEntity (ent_)) {
      ent_->v.movetype = MOVETYPE_NOCLIP;
    }

    if (graph.HasEditFlag (GraphEdit::On)) {
      graph.SetEditFlag (GraphEdit::Noclip);

      Msg ("Noclip mode enabled.");
    }
    else {
      graph.SetEditFlag (GraphEdit::On | GraphEdit::Noclip);
      EnableDrawModels (true);

      Msg ("Graph editor has been enabled with noclip mode.");
    }
  }
  else if (Arg<ystl::StringRef> (option) == "auto") {
    if (graph.HasEditFlag (GraphEdit::On)) {
      graph.SetEditFlag (GraphEdit::Auto);

      Msg ("Enabled auto nodes placement.");
    }
    else {
      graph.SetEditFlag (GraphEdit::On | GraphEdit::Auto);
      EnableDrawModels (true);

      Msg ("Graph editor has been enabled with auto add node mode.");
    }
  }

  if (graph.HasEditFlag (GraphEdit::On)) {
    auto store_cvar_value = [&] (ConVar &var) {
      game_cvar_holder_[var.Name ()] = var.As<float> ();
      var.Set (0);
    };

    store_cvar_value (mp_roundtime);
    store_cvar_value (mp_freezetime);
    store_cvar_value (mp_timelimit);

    if (game.Is (GameFlags::ReGameDLL)) {
      ConVarRef mp_round_infinite ("mp_round_infinite");

      if (mp_round_infinite.Exists ()) {
        mp_round_infinite.Set ("1");
      }
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeOff () {
  enum args {
    graph_cmd = 1,
    cmd,
    option
  };

  // enable various features of editor
  if (Arg<ystl::StringRef> (option).empty () || Arg<ystl::StringRef> (option) == "display") {
    graph.ClearEditFlag (GraphEdit::On | GraphEdit::Auto | GraphEdit::Noclip);
    EnableDrawModels (false);

    // revert cvars back to their values
    auto restore_cvar_value = [&] (ConVar &var) {
      var.Set (game_cvar_holder_[var.Name ()]);
    };
    restore_cvar_value (mp_roundtime);
    restore_cvar_value (mp_freezetime);
    restore_cvar_value (mp_timelimit);

    if (game.Is (GameFlags::ReGameDLL)) {
      ConVarRef mp_round_infinite ("mp_round_infinite");

      if (mp_round_infinite.Exists ()) {
        mp_round_infinite.Set ("0");
      }
    }
    Msg ("Graph editor has been disabled.");
  }
  else if (Arg<ystl::StringRef> (option) == "models") {
    EnableDrawModels (false);

    Msg ("Graph editor has disabled spawn points highlighting.");
  }
  else if (Arg<ystl::StringRef> (option) == "noclip") {
    if (!game.IsNullEntity (ent_)) {
      ent_->v.movetype = MOVETYPE_WALK;
    }
    graph.ClearEditFlag (GraphEdit::Noclip);

    Msg ("Graph editor has disabled noclip mode.");
  }
  else if (Arg<ystl::StringRef> (option) == "auto") {
    graph.ClearEditFlag (GraphEdit::Auto);
    Msg ("Graph editor has disabled auto add node mode.");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeAdd () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // show the menu
  ShowMenu (MenuId::NodeType);
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeAddBasic () {
  enum args {
    graph_cmd = 1,
    cmd
  };
  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  graph.SeedBasicNodes ();
  Msg ("Basic graph nodes was added.");

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeSave () {
  enum args {
    graph_cmd = 1,
    cmd,
    option
  };

  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }

  // if no check is set save anyway
  if (Arg<ystl::StringRef> (option) == "nocheck") {
    graph.SaveGraphData ();

    Msg ("All nodes has been saved and written to disk (IGNORING QUALITY CONTROL).");
  }
  else if (Arg<ystl::StringRef> (option) == "old" || Arg<ystl::StringRef> (option) == "oldformat") {
    if (graph.Length () >= 1024) {
      Msg ("Unable to save POD-Bot Format waypoint file. Number of nodes exceeds 1024.");

      return CommandResult::Handled;
    }
    graph.SaveOldFormat ();

    Msg ("All nodes has been saved and written to disk (POD-Bot Format (.pwf)).");
  }
  else {
    if (graph.CheckNodes (false)) {
      graph.SaveGraphData ();
      Msg ("All nodes has been saved and written to disk.\n*** Please don't forget to share your work by typing \"%s g upload\". Thank you! "
           "***",
        product.cmd_pri);
    }
    else {
      Msg ("Could not save nodes to disk. Graph check has failed.");
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeLoad () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }

  // just save graph on request
  if (graph.LoadGraphData ()) {
    Msg ("Graph successfully loaded.");
  }
  else {
    Msg ("Could not load Graph. See console...");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeErase () {
  enum args {
    graph_cmd = 1,
    cmd,
    iamsure
  };

  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }

  // prevent accidents when graph are deleted unintentionally
  if (Arg<ystl::StringRef> (iamsure) == "iamsure") {
    bstor.UnlinkFromDisk (false, false);
  }
  else {
    Msg ("Please, append \"iamsure\" as parameter to get graph erased from the disk.");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeRefresh () {
  enum args {
    graph_cmd = 1,
    cmd,
    iamsure
  };

  if (!graph.CanDownload ()) {
    Msg ("Can't sync graph with database while graph url is not set.");

    return CommandResult::Handled;
  }

  // prevent accidents when graph are deleted unintentionally
  if (Arg<ystl::StringRef> (iamsure) == "iamsure") {
    bstor.UnlinkFromDisk (false, false);
    graph.LoadGraphData ();
  }
  else {
    Msg ("Please, append \"iamsure\" as parameter to get graph refreshed from the graph database.");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeEraseTraining () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  bstor.UnlinkFromDisk (true, false);

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeDelete () {
  enum args {
    graph_cmd = 1,
    cmd,
    nearest
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // if "nearest" or nothing passed delete nearest, else delete by index
  if (Arg<ystl::StringRef> (nearest).empty () || Arg<ystl::StringRef> (nearest) == "nearest") {
    graph.Erase (kInvalidNodeIndex);
  }
  else {
    const auto index = Arg<int> (nearest);

    // check for existence
    if (graph.Exists (index)) {
      graph.Erase (index);
      Msg ("Node %d has been deleted.", index);
    }
    else {
      Msg ("Could not delete node %d.", index);
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeCheck () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // check if nodes are ok
  if (graph.CheckNodes (true)) {
    Msg ("Graph seems to be OK.");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeCache () {
  enum args {
    graph_cmd = 1,
    cmd,
    nearest
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // if "nearest" or nothing passed delete nearest, else delete by index
  if (Arg<ystl::StringRef> (nearest).empty () || Arg<ystl::StringRef> (nearest) == "nearest") {
    graph.CachePoint (kInvalidNodeIndex);
  }
  else {
    const int index = Arg<int> (nearest);

    // check for existence
    if (graph.Exists (index)) {
      graph.CachePoint (index);
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeClean () {
  enum args {
    graph_cmd = 1,
    cmd,
    option
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // if "all" passed clean up all the paths
  if (Arg<ystl::StringRef> (option) == "all") {
    int removed = 0;

    graph.SetMessageSilence (true); // per-link chatter, summary is logged once below

    for (auto i = 0; i < graph.Length (); ++i) {
      removed += graph.ClearConnections (i);
    }
    graph.SetMessageSilence (false);
    Msg ("Done. Processed %d nodes. %d useless paths was cleared.", graph.Length (), removed);
  }
  else if (Arg<ystl::StringRef> (option).empty () || Arg<ystl::StringRef> (option) == "nearest") {
    int removed = graph.ClearConnections (graph.GetEditorNearest ());

    Msg ("Done. Processed node %d. %d useless paths was cleared.", graph.GetEditorNearest (), removed);
  }
  else {
    const int index = Arg<int> (option);

    // check for existence
    if (graph.Exists (index)) {
      const int removed = graph.ClearConnections (index);

      Msg ("Done. Processed node %d. %d useless paths was cleared.", index, removed);
    }
    else {
      Msg ("Could not process node %d clearance.", index);
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeSetRadius () {
  enum args {
    graph_cmd = 1,
    cmd,
    radius,
    node_index
  };

  // radius is a must
  if (!HasArg (radius)) {
    return CommandResult::BadFormat;
  }
  int radius_index = kInvalidNodeIndex;

  if (Arg<ystl::StringRef> (node_index).empty () || Arg<ystl::StringRef> (node_index) == "nearest") {
    radius_index = graph.GetEditorNearest ();
  }
  else {
    radius_index = Arg<int> (node_index);
  }
  graph.SetRadius (radius_index, Arg<ystl::StringRef> (radius).as<float> ());

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeSetFlags () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // show the flag menu
  ShowMenu (MenuId::NodeFlag);
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeTeleport () {
  enum args {
    graph_cmd = 1,
    cmd,
    teleport_index
  };

  if (!HasArg (teleport_index)) {
    return CommandResult::BadFormat;
  }
  int node_index = Arg<int> (teleport_index);

  // check for existence
  if (graph.Exists (node_index)) {
    engfuncs.pfnSetOrigin (graph.GetEditor (), graph[node_index].origin);

    Msg ("You have been teleported to node %d.", node_index);

    // turn graph on
    graph.SetEditFlag (GraphEdit::On | GraphEdit::Noclip);
  }
  else {
    Msg ("Could not teleport to node %d.", node_index);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodePathCreate () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // choose the direction for path creation
  if (Arg<ystl::StringRef> (cmd).ends_with ("_jump")) {
    graph.PathCreate (PathConnection::Jumping);
  }
  else if (Arg<ystl::StringRef> (cmd).ends_with ("_both")) {
    graph.PathCreate (PathConnection::Bidirectional);
  }
  else if (Arg<ystl::StringRef> (cmd).ends_with ("_in")) {
    graph.PathCreate (PathConnection::Incoming);
  }
  else if (Arg<ystl::StringRef> (cmd).ends_with ("_out")) {
    graph.PathCreate (PathConnection::Outgoing);
  }
  else {
    ShowMenu (MenuId::NodePath);
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodePathDelete () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // delete the path
  graph.ErasePath ();

  return CommandResult::Handled;
}

CommandResult Control::CmdNodePathSetAutoDistance () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);
  ShowMenu (MenuId::NodeAutoPath);

  return CommandResult::Handled;
}

CommandResult Control::CmdNodePathCleanAll () {
  enum args {
    graph_cmd = 1,
    cmd,
    node_index
  };

  auto requested_node = kInvalidNodeIndex;

  if (HasArg (node_index)) {
    requested_node = Arg<int> (node_index);
  }
  graph.ResetPath (requested_node);

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeAcquireEditor () {
  enum args {
    graph_cmd = 1
  };

  if (game.IsNullEntity (ent_)) {
    Msg ("This command should not be executed from HLDS console.");
    return CommandResult::Handled;
  }

  if (graph.HasEditor ()) {
    Msg ("Sorry, players \"%s\" already acquired rights to edit graph on this server.", graph.GetEditor ()->v.netname.chars ());
    return CommandResult::Handled;
  }
  graph.SetEditor (ent_);
  Msg ("You're acquired rights to edit graph on this server. You're now able to use graph commands.");

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeReleaseEditor () {
  enum args {
    graph_cmd = 1
  };

  if (!graph.HasEditor ()) {
    Msg ("No one is currently has rights to edit. Nothing to release.");
    return CommandResult::Handled;
  }
  graph.SetEditor (nullptr);
  Msg ("Graph editor rights freed. You're now not able to use graph commands.");

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeUpload () {
  enum args {
    graph_cmd = 1,
    cmd
  };

  // do not allow to upload analyzed graphs
  if (graph.IsAnalyzed ()) {
    Msg ("Sorry, unable to upload graph that was generated automatically.");
    return CommandResult::Handled;
  }

  // do not allow to upload bad graph
  if (!graph.CheckNodes (false)) {
    Msg ("Sorry, unable to upload graph file that contains errors. Please type \"graph check\" to verify graph consistency.");
    return CommandResult::Handled;
  }
  ystl::String upload_url = graph_urls.UploadUrl ();

  if (upload_url.empty ()) {
    Msg ("Graph upload is disabled (graph_url_upload is empty).");
    return CommandResult::Handled;
  }

  Msg ("\n");
  Msg ("WARNING!");
  Msg ("Graph uploaded to graph database in synchronous mode. That means if graph is big enough");
  Msg ("you may notice the game freezes a bit during upload and issue request creation. Please, be patient.");
  Msg ("\n");

  // try to upload the file
  if (ystl::http.upload_file (upload_url, bstor.BuildPath (StorageFile::Graph))) {
    const auto download_base = graph_urls.DownloadBase ();

    Msg ("Graph file was successfully validated and uploaded to the %s Graph DB (%s).", product.name,
      download_base.empty () ? product.download.chars () : download_base.chars ());
    Msg ("It will be available for download for all %s users in a few minutes.", product.name);
    Msg ("\n");
    Msg ("Thank you.");
    Msg ("\n");
  }
  else {
    ystl::String status {};
    auto code = ystl::http.get_last_status_code ();

    if (code == HttpClientResult::Forbidden) {
      status = "AlreadyExists";
    }
    else if (code == HttpClientResult::NotFound) {
      status = "AccessDenied";
    }
    else if (code == HttpClientResult::HttpOnly) {
      status = graph_urls.HttpsUnsupportedText ();
    }
    else {
      status.assignf ("%d", code);
    }
    Msg ("Something went wrong with uploading. Come back later. (%s)", status);
    Msg ("\n");

    if (code == HttpClientResult::Forbidden) {
      Msg ("You should create issue-request manually for this graph");
      Msg ("as it's already exists in database, can't overwrite. Sorry...");
    }
    else {
      Msg ("There is an internal error, or something is totally wrong with");
      Msg ("your files, and they are not passed sanity checks. Sorry...");
    }
    Msg ("\n");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeIterateCamp () {
  enum args {
    graph_cmd = 1,
    cmd,
    option
  };

  // turn graph on
  graph.SetEditFlag (GraphEdit::On);

  // get the option describing operation
  auto op = Arg<ystl::StringRef> (option);

  if (op != "begin" && op != "end" && op != "next") {
    return CommandResult::BadFormat;
  }

  if ((op == "next" || op == "end") && camp_iterator_.empty ()) {
    Msg ("Before calling for 'next' / 'end' camp point, you should hit 'begin'.");
    return CommandResult::Handled;
  }
  else if (op == "begin" && !camp_iterator_.empty ()) {
    Msg ("Before calling for 'begin' camp point, you should hit 'end'.");
    return CommandResult::Handled;
  }

  if (op == "end") {
    camp_iterator_.clear ();
  }
  else if (op == "next") {
    if (!camp_iterator_.empty ()) {
      ystl::Vector origin = graph[camp_iterator_.first ()].origin;

      if (has_flag (graph[camp_iterator_.first ()].flags, NodeFlag::Crouch)) {
        origin.z += 23.0f;
      }
      if (!game.IsNullEntity (ent_)) {
        engfuncs.pfnSetOrigin (ent_, origin);
      }

      // go to next
      camp_iterator_.shift ();

      if (camp_iterator_.empty ()) {
        Msg ("Finished iterating camp spots.");
      }
    }
  }
  else if (op == "begin") {
    for (const auto &path : graph) {
      if (has_flag (path.flags, NodeFlag::Camp)) {
        camp_iterator_.push (path.number);
      }
    }
    if (!camp_iterator_.empty ()) {
      Msg ("Ready for iteration. Type 'next' to go to first camp node.");
      return CommandResult::Handled;
    }
    Msg ("Unable to begin iteration, camp points is not set.");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeShowStats () {
  graph.ShowStats ();

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeFileInfo () {
  graph.ShowFileInfo ();

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeExport () {
  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }
  graph.ExportGraphText ();

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeImport () {
  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }

  // the text file lives next to the binary graph file
  const auto file_path = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.data, folders.graph,
    ystl::strings.format ("%s.graph.txt", ystl::String (game.GetMapName ()).lowercase ().chars ()));

  graph.ImportGraphText (file_path);

  return CommandResult::Handled;
}

CommandResult Control::CmdNodeApply () {
  // prevent some commands while analyzing graph
  if (analyzer.IsAnalyzing ()) {
    Msg ("This command is unavailable while map analysis is ongoing.");

    return CommandResult::Handled;
  }
  if (graph.Length () < 1) {
    Msg ("There is no graph to apply.");

    return CommandResult::Handled;
  }
  if (!graph.CheckNodes (false)) {
    Msg ("Could not apply graph text. Graph check has failed.");

    return CommandResult::Handled;
  }
  if (!graph.SaveGraphData ()) {
    Msg ("Could not save the graph, see console...");

    return CommandResult::Handled;
  }
  if (graph.LoadGraphData ()) {
    Msg ("Graph applied: saved and reloaded, bots can be added now.");
  }
  else {
    Msg ("Could not load graph. See console...");
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdNodeAdjustHeight () {
  enum args {
    graph_cmd = 1,
    cmd,
    offset
  };

  if (!HasArg (offset)) {
    return CommandResult::BadFormat;
  }
  auto height_offset = Arg<float> (offset);

  // adjust the height for all the nodes (negative values possible)
  for (auto &path : graph) {
    path.origin.z += height_offset;
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdDebug () {
  // find the debug command in m_cmds to get its subcommands
  for (auto &item : cmds_) {
    if (item.name.contains ("debug") && item.HasSubcommands ()) {
      return DispatchSubcommand (item, 2);
    }
  }
  return CommandResult::Handled;
}

edict_t *Control::ResolveDebugTarget (int arg_index) {
  if (!HasArg (static_cast<size_t> (arg_index))) {
    return nullptr;
  }

  auto target_str = Arg<ystl::StringRef> (static_cast<size_t> (arg_index));

  if (target_str == "host") {
    if (game.IsDedicatedServer ()) {
      Msg ("'host' target is only available on listenserver.");
      return nullptr;
    }
    return game.GetLocalEntity ();
  }

  // try to parse as integer (player index)
  if (!target_str.empty () && (isdigit (target_str[0]) || (target_str.size () > 1 && target_str[0] == '-'))) {
    auto index = target_str.as<int> ();
    auto ent = game.PlayerOfIndex (index - 1);

    if (game.IsNullEntity (ent)) {
      Msg ("Player with index %d not found.", index);
      return nullptr;
    }
    return ent;
  }

  // search by name (partial match)
  edict_t *match = nullptr;

  for (int i = 0; i < game.MaxClients (); ++i) {
    auto player = game.PlayerOfIndex (i);

    if (game.IsNullEntity (player)) {
      continue;
    }

    if (ystl::StringRef (player->v.netname.chars ()).contains (target_str)) {
      if (match) {
        Msg ("Multiple players match \"%s\". Be more specific.", target_str);
        Msg ("Matches: \"%s\", \"%s\"", match->v.netname.chars (), player->v.netname.chars ());
        return nullptr;
      }
      match = player;
    }
  }

  if (match) {
    return match;
  }
  Msg ("Player with name \"%s\" not found.", target_str);

  return nullptr;
}

CommandResult Control::DispatchSubcommand (const Cmd &parent_cmd, int arg_offset) {

  // slash-separated aliases matched in place, without building an array<string> per lookup
  auto alias_match = [] (const ystl::String &test, ystl::StringRef cmd) -> bool {
    const auto view = ystl::StringRef (test.chars (), test.size ());
    size_t start = 0;

    while (start <= view.size ()) {
      const auto end = view.find ("/", start);
      const auto seg_end = end == ystl::String::InvalidIndex ? view.size () : end;

      if (seg_end - start == cmd.size () && view.substr (start, seg_end - start) == cmd) {
        return true;
      }

      if (end == ystl::String::InvalidIndex) {
        break;
      }
      start = end + 1;
    }
    return false;
  };

  // first alias of a slash-separated name,
  auto first_alias = [] (ystl::StringRef name) -> ystl::String {
    const auto end = name.find ("/");

    return ystl::String (name.chars (), end == ystl::String::InvalidIndex ? name.size () : end);
  };

  auto subcommand_arg = Arg<ystl::StringRef> (static_cast<size_t> (arg_offset));

  // handle help request
  if (subcommand_arg == "help" && HasArg (static_cast<size_t> (arg_offset + 1))) {
    auto help_cmd = Arg<ystl::StringRef> (static_cast<size_t> (arg_offset + 1));

    for (const auto &subcmd : parent_cmd.subcommands) {
      if (alias_match (subcmd.name, help_cmd)) {
        Msg ("Command: \"%s %s %s\"", args_[0], args_[1], first_alias (subcmd.name));
        Msg ("Format: %s", subcmd.format);
        Msg ("Help: %s", conf.Translate (subcmd.help));

        return CommandResult::Handled;
      }
    }
  }

  // try to find matching subcommand
  for (const auto &subcmd : parent_cmd.subcommands) {
    if (alias_match (subcmd.name, subcommand_arg)) {
      auto status = (this->*subcmd.handler) ();

      if (status == CommandResult::BadFormat) {
        Msg ("Incorrect usage of \"%s %s %s\" command. Correct usage is:", args_[0], args_[1], first_alias (subcmd.name));
        Msg ("\n\t%s\n", subcmd.format);
        Msg ("Please use correct format.");
      }
      return status;
    }
  }

  // no match found, list available subcommands
  for (const auto &subcmd : parent_cmd.subcommands) {
    Msg ("   %s - %s", first_alias (subcmd.name), conf.Translate (subcmd.help));
  }
  return CommandResult::Handled;
}

CommandResult Control::CmdDebugSlay () {
  enum args {
    root,
    alias,
    cmd,
    target
  };

  auto target_ent = ResolveDebugTarget (target);

  if (!target_ent) {
    return HasArg (target) ? CommandResult::Handled : CommandResult::BadFormat;
  }

  if (!game.IsAliveEntity (target_ent)) {
    Msg ("Player \"%s\" is not alive.", target_ent->v.netname.chars ());
    return CommandResult::Handled;
  }

  Msg ("Slaying player \"%s\".", target_ent->v.netname.chars ());
  MDLL_ClientKill (target_ent);

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugSlap () {
  enum args {
    root,
    alias,
    cmd,
    target,
    damage
  };

  auto target_ent = ResolveDebugTarget (target);

  if (!target_ent) {
    return HasArg (target) ? CommandResult::Handled : CommandResult::BadFormat;
  }

  if (!game.IsAliveEntity (target_ent)) {
    Msg ("Player \"%s\" is not alive.", target_ent->v.netname.chars ());
    return CommandResult::Handled;
  }

  // get slap damage, default to 0 if not specified
  int slap_damage = HasArg (damage) ? Arg<int> (damage) : 0;

  if (slap_damage > 0) {
    target_ent->v.health -= static_cast<float> (slap_damage);

    // check if player died from the slap
    if (target_ent->v.health <= 0) {
      Msg ("Player \"%s\" was killed by slap.", target_ent->v.netname.chars ());
      MDLL_ClientKill (target_ent);

      return CommandResult::Handled;
    }
  }

  // push the player up
  target_ent->v.velocity.z += 256.0f;

  Msg ("Slapped player \"%s\" (damage: %d, health: %d).", target_ent->v.netname.chars (), slap_damage, static_cast<int> (target_ent->v.health));

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugGod () {
  enum args {
    root,
    alias,
    cmd,
    target
  };

  edict_t *target_ent = nullptr;

  // without target argument default to the listenserver host entity
  if (HasArg (target)) {
    target_ent = ResolveDebugTarget (target);

    if (!target_ent) {
      return CommandResult::Handled;
    }
  }
  else {
    if (game.IsDedicatedServer ()) {
      Msg ("'god' without target is only available on listenserver. Use \"god <id|name>\" instead.");
      return CommandResult::BadFormat;
    }

    target_ent = game.GetLocalEntity ();

    if (game.IsNullEntity (target_ent)) {
      Msg ("Host entity not found on listenserver.");
      return CommandResult::Handled;
    }
  }

  if (target_ent->v.flags & FL_GODMODE) {
    target_ent->v.flags &= ~FL_GODMODE;
    Msg ("God mode disabled for player \"%s\".", target_ent->v.netname.chars ());
  }
  else {
    target_ent->v.flags |= FL_GODMODE;
    Msg ("God mode enabled for player \"%s\".", target_ent->v.netname.chars ());
  }

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugNotarget () {
  enum args {
    root,
    alias,
    cmd,
    target
  };

  auto target_ent = ResolveDebugTarget (target);

  if (!target_ent) {
    return HasArg (target) ? CommandResult::Handled : CommandResult::BadFormat;
  }

  if (target_ent->v.flags & FL_NOTARGET) {
    target_ent->v.flags &= ~FL_NOTARGET;
    Msg ("Notarget disabled for player \"%s\".", target_ent->v.netname.chars ());
  }
  else {
    target_ent->v.flags |= FL_NOTARGET;
    Msg ("Notarget enabled for player \"%s\".", target_ent->v.netname.chars ());
  }

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugExec () {
  enum args {
    root,
    alias,
    cmd,
    target,
    command
  };

  if (!HasArg (command)) {
    return CommandResult::BadFormat;
  }

  auto target_ent = ResolveDebugTarget (target);

  if (!target_ent) {
    return HasArg (target) ? CommandResult::Handled : CommandResult::BadFormat;
  }

  auto bot = bots.FindBotByEntity (target_ent);

  if (bot) {
    bot->IssueCommand (Arg<ystl::StringRef> (command).chars ());
  }
  else {
    engfuncs.pfnClientCommand (target_ent, "%s\n", Arg<ystl::StringRef> (command).chars ());
  }
  Msg ("Executed command on player \"%s\".", target_ent->v.netname.chars ());

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugMemory () {
#if defined(YSTL_DEBUG)
  // keep report state across calls, rendering lives in ystl
  static ystl::mem::MemoryDebugger::State report_state {};

  ystl::mem::MemoryDebugger::print (report_state, game.Time (), [&] (ystl::StringRef line) {
    Msg ("%s", line);
  });
#else
  Msg ("Unsupported on release builds.");
#endif

  return CommandResult::Handled;
}

CommandResult Control::CmdDebugTranslate () {
  enum args {
    root,
    alias,
    cmd,
    subcommand
  };

  if (HasArg (subcommand) && Arg<ystl::StringRef> (subcommand) == "reset") {
    conf.ResetMissingTranslations ();
    Msg ("Missing translations list cleared.");
    return CommandResult::Handled;
  }

  if (HasArg (subcommand) && Arg<ystl::StringRef> (subcommand) == "write") {
    const auto written = conf.WriteMissingTranslations ();

    if (written < 0) {
      Msg ("Failed to write the language configuration file, see the log for details.");
    }
    else if (written == 0) {
      Msg ("No missing translations to write.");
    }
    else {
      Msg ("Appended %d entries to lang/%s_lang.cfg, originals are used as translations for now.", written,
        cv_language.As<ystl::StringRef> ().chars ());
    }
    return CommandResult::Handled;
  }

  // snapshot the list before printing, msg () runs translations and may insert into the same map
  ystl::Array<uint32_t> hashes {};
  ystl::Array<ystl::String> originals {};

  for (const auto &[hash, original] : conf.MissingTranslations ()) {
    hashes.push (hash);
    originals.push (original);
  }
  if (originals.empty ()) {
    Msg ("No missing translations found yet. Play for a while, then run this command again.");
    return CommandResult::Handled;
  }
  const auto language = cv_language.As<ystl::StringRef> ();

  Msg ("Found %d untranslated strings (language: '%s', hash of original is the lookup key):", originals.size (), language.chars ());

  for (size_t i = 0; i < originals.size (); ++i) {
    Msg ("  %08x %s", hashes[i], originals[i].chars ());
  }
  Msg ("Translate these strings into lang/%s_lang.cfg, then run 'yb debug translate reset'.", language.chars ());

  return CommandResult::Handled;
}

CommandResult Control::MenuMain (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    is_menu_fill_command_ = false;
    ShowMenu (MenuId::Control);
    break;

  case 2:
    ShowMenu (MenuId::Features);
    break;

  case 3:
    is_menu_fill_command_ = true;
    ShowMenu (MenuId::TeamSelect);
    break;

  case 4:
    if (game.Is (GameFlags::ReGameDLL) && !cv_bots_kill_on_endround) {
      game.ServerCommand ("endround");
    }
    else {
      bots.KillAllBots ();
    }
    break;

  case 10:
    CloseMenu ();
    break;

  default:
    ShowMenu (MenuId::Main);
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuFeatures (int item) {
  CloseMenu (); // reset menu display

  auto auto_acquire_editor_rights = [&] () {
    if (!graph.HasEditor ()) {
      graph.SetEditor (ent_);
    }
    return graph.HasEditor () && graph.GetEditor () == ent_ ? MenuId::NodeMainPage1 : MenuId::Features;
  };

  switch (item) {
  case 1:
    ShowMenu (MenuId::WeaponMode);
    break;

  case 2:
    ShowMenu (auto_acquire_editor_rights ());
    break;

  case 3:
    ShowMenu (MenuId::Personality);
    break;

  case 4:
    cv_debug.Set (DebugLevel () ^ 1);

    ShowMenu (MenuId::Features);
    break;

  case 5:
    if (game.IsAliveEntity (ent_)) {
      ShowMenu (MenuId::Commands);
    }
    else {
      CloseMenu (); // reset menu display
      Msg ("You're dead, and have no access to this menu");
    }
    break;

  case 6:
    ShowMenu (MenuId::FunModeId);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuControl (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    bots.CreateRandom (true);
    ShowMenu (MenuId::Control);
    break;

  case 2:
    ShowMenu (MenuId::Difficulty);
    break;

  case 3:
    bots.KickRandom ();
    ShowMenu (MenuId::Control);
    break;

  case 4:
    bots.KickEveryone ();
    break;

  case 5:
    KickBotByMenu (1);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuWeaponMode (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
    bots.SetWeaponMode (item);
    ShowMenu (MenuId::WeaponMode);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuFunMode (int item) {
  CloseMenu (); // reset menu display

  constexpr FunModeId modes[7] = { FunModeId::Off, FunModeId::Tron, FunModeId::NewYear, FunModeId::Haunted, FunModeId::Dark, FunModeId::Stoned,
    FunModeId::Mars };

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
    fun_mode.SetMode (modes[item - 1]);
    Msg (fun_mode.MessageOf (modes[item - 1]).chars ());
    ShowMenu (MenuId::FunModeId);
    break;

  case 10:
    CloseMenu ();
    break;

  default:
    ShowMenu (MenuId::Features);
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuPersonality (int item) {
  if (is_menu_fill_command_) {
    CloseMenu (); // reset menu display

    switch (item) {
    case 1:
    case 2:
    case 3:
    case 4:
      bots.ServerFill (
        static_cast<CSTeam> (menu_server_fill_team_), static_cast<Personality> (item - 2), static_cast<Difficulty> (inter_menu_data_[0]));
      CloseMenu ();
      break;

    case 10:
      CloseMenu ();
      break;
    }
    return CommandResult::Handled;
  }
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
    inter_menu_data_[3] = item - 2;
    ShowMenu (MenuId::TeamSelect);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuDifficulty (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    inter_menu_data_[0] = 0;
    break;

  case 2:
    inter_menu_data_[0] = 1;
    break;

  case 3:
    inter_menu_data_[0] = 2;
    break;

  case 4:
    inter_menu_data_[0] = 3;
    break;

  case 5:
    inter_menu_data_[0] = 4;
    break;

  case 10:
    CloseMenu ();
    break;
  }
  ShowMenu (MenuId::Personality);

  return CommandResult::Handled;
}

CommandResult Control::MenuTeamSelect (int item) {
  if (is_menu_fill_command_) {
    CloseMenu (); // reset menu display

    if (item < 3) {
      // turn off cvars if specified team
      mp_limitteams.Set (0);
      mp_autoteambalance.Set (0);
    }

    switch (item) {
    case 1:
    case 2:
    case 5:
      menu_server_fill_team_ = item;
      ShowMenu (MenuId::Difficulty);
      break;

    case 10:
      CloseMenu ();
      break;
    }
    return CommandResult::Handled;
  }
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
  case 5:
    inter_menu_data_[1] = item;

    if (item == 5) {
      inter_menu_data_[2] = item;

      bots.Addbot ("", static_cast<Difficulty> (inter_menu_data_[0]), static_cast<Personality> (inter_menu_data_[3]),
        util.ConvertFromCsTeam (static_cast<CSTeam> (inter_menu_data_[1])), inter_menu_data_[2], true);
    }
    else if (game.Is (GameFlags::ConditionZero)) {
      ShowMenu (item == 1 ? MenuId::TerroristSelectCZ : MenuId::CTSelectCZ);
    }
    else {
      ShowMenu (item == 1 ? MenuId::TerroristSelect : MenuId::CTSelect);
    }
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuClassSelect (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
    inter_menu_data_[2] = item;
    bots.Addbot ("", static_cast<Difficulty> (inter_menu_data_[0]), static_cast<Personality> (inter_menu_data_[3]),
      util.ConvertFromCsTeam (static_cast<CSTeam> (inter_menu_data_[1])), inter_menu_data_[2], true);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuCommands (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
    if (const auto found = util.FindNearestBot ({ .origin = ent_, .distance = 600.0f, .same_team = true, .alive = true, .visible = true })) {
      djump_ = *found;

      if (!djump_->has_c4_ && !djump_->has_hostage_) {
        if (item == 1) {
          djump_->StartDoubleJump (ent_);
        }
        else {
          if (djump_) {
            djump_->ResetDoubleJump ();
            djump_ = nullptr;
          }
        }
      }
    }
    ShowMenu (MenuId::Commands);
    break;

  case 3:
  case 4:
    if (const auto nearest = util.FindNearestBot (
          { .origin = ent_, .distance = 600.0f, .same_team = true, .alive = true, .visible = true, .skip_c4 = item != 4 })) {
      (*nearest)->DropWeaponForUser (ent_, item == 4 ? false : true);
    }
    ShowMenu (MenuId::Commands);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphPage1 (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    if (graph.HasEditFlag (GraphEdit::On)) {
      graph.ClearEditFlag (GraphEdit::On);
      EnableDrawModels (false);

      Msg ("Graph editor has been disabled.");
    }
    else {
      graph.SetEditFlag (GraphEdit::On);
      EnableDrawModels (true);

      Msg ("Graph editor has been enabled.");
    }
    ShowMenu (MenuId::NodeMainPage1);
    break;

  case 2:
    graph.SetEditFlag (GraphEdit::On);
    graph.CachePoint (kInvalidNodeIndex);

    ShowMenu (MenuId::NodeMainPage1);
    break;

  case 3:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodePath);
    break;

  case 4:
    graph.SetEditFlag (GraphEdit::On);
    graph.ErasePath ();

    ShowMenu (MenuId::NodeMainPage1);
    break;

  case 5:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodeType);
    break;

  case 6:
    graph.SetEditFlag (GraphEdit::On);
    graph.Erase (kInvalidNodeIndex);

    ShowMenu (MenuId::NodeMainPage1);
    break;

  case 7:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodeAutoPath);
    break;

  case 8:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodeRadius);
    break;

  case 9:
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphPage2 (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodeDebug);
    break;

  case 2:
    graph.SetEditFlag (GraphEdit::On);

    if (graph.HasEditFlag (GraphEdit::Auto)) {
      graph.ClearEditFlag (GraphEdit::Auto);
    }
    else {
      graph.SetEditFlag (GraphEdit::Auto);
    }

    if (graph.HasEditFlag (GraphEdit::Auto)) {
      Msg ("Enabled auto nodes placement.");
    }
    else {
      Msg ("Disabled auto nodes placement.");
    }
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 3:
    graph.SetEditFlag (GraphEdit::On);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 4:
    if (graph.CheckNodes (true)) {
      graph.SaveGraphData ();
      Msg ("Graph successfully saved.");
    }
    else {
      Msg ("Graph not saved. There are errors, see console...");
    }
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 5:
    if (graph.SaveGraphData ()) {
      Msg ("Graph successfully saved.");
    }
    else {
      Msg ("Could not save Graph. See console...");
    }
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 6:
    if (graph.LoadGraphData ()) {
      Msg ("Graph successfully loaded.");
    }
    else {
      Msg ("Could not load Graph. See console...");
    }
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 7:
    if (graph.CheckNodes (true)) {
      Msg ("Nodes works fine");
    }
    else {
      Msg ("There are errors, see console");
    }
    ShowMenu (MenuId::NodeMainPage2);
    break;

  case 8:
    graph.SetEditFlag (GraphEdit::On);

    if (graph.HasEditFlag (GraphEdit::Noclip)) {
      graph.ClearEditFlag (GraphEdit::Noclip);
      Msg ("Noclip mode disabled.");
    }
    else {
      graph.SetEditFlag (GraphEdit::Noclip);
      Msg ("Noclip mode enabled.");
    }
    ShowMenu (MenuId::NodeMainPage2);

    // update editor movetype based on flag
    if (!game.IsNullEntity (ent_)) {
      ent_->v.movetype = graph.HasEditFlag (GraphEdit::Noclip) ? MOVETYPE_NOCLIP : MOVETYPE_WALK;
    }

    break;

  case 9:
    ShowMenu (MenuId::NodeMainPage1);
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphRadius (int item) {
  CloseMenu (); // reset menu display
  graph.SetEditFlag (GraphEdit::On); // turn graph on in case

  if (item >= 1 && item <= 9) {
    constexpr float kRadiusValues[] = { 0.0f, 8.0f, 16.0f, 32.0f, 48.0f, 64.0f, 80.0f, 96.0f, 128.0f };

    graph.SetRadius (kInvalidNodeIndex, kRadiusValues[item - 1]);
    ShowMenu (MenuId::NodeRadius);
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphType (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7:
    graph.Add (static_cast<NodeAddFlag> (item - 1));
    ShowMenu (MenuId::NodeType);
    break;

  case 8:
    graph.Add (NodeAddFlag::Goal);
    ShowMenu (MenuId::NodeType);
    break;

  case 9:
    graph.StartLearnJump ();
    ShowMenu (MenuId::NodeType);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphDebug (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    cv_debug_goal.Set (graph.GetEditorNearest ());
    if (cv_debug_goal.As<int> () != kInvalidNodeIndex) {
      Msg ("Debug goal is set to node %d.", cv_debug_goal.As<int> ());
    }
    else {
      Msg ("Cannot find the node. Debug goal is disabled.");
    }
    ShowMenu (MenuId::NodeDebug);
    break;

  case 2:
    cv_debug_goal.Set (graph.GetFacingIndex ());
    if (cv_debug_goal.As<int> () != kInvalidNodeIndex) {
      Msg ("Debug goal is set to node %d.", cv_debug_goal.As<int> ());
    }
    else {
      Msg ("Cannot find the node. Debug goal is disabled.");
    }
    ShowMenu (MenuId::NodeDebug);
    break;

  case 3:
    cv_debug_goal.Set (kInvalidNodeIndex);
    Msg ("Debug goal is disabled.");
    ShowMenu (MenuId::NodeDebug);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphFlag (int item) {
  CloseMenu (); // reset menu display
  int nearest = graph.GetEditorNearest ();

  switch (item) {
  case 1:
    graph.ToggleFlags (NodeFlag::NoHostage);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 2:
    if (has_flag (graph[nearest].flags, NodeFlag::CTOnly)) {
      graph.ToggleFlags (NodeFlag::CTOnly);
      graph.ToggleFlags (NodeFlag::TerroristOnly);
    }
    else {
      graph.ToggleFlags (NodeFlag::TerroristOnly);
    }
    ShowMenu (MenuId::NodeFlag);
    break;

  case 3:
    if (has_flag (graph[nearest].flags, NodeFlag::TerroristOnly)) {
      graph.ToggleFlags (NodeFlag::TerroristOnly);
      graph.ToggleFlags (NodeFlag::CTOnly);
    }
    else {
      graph.ToggleFlags (NodeFlag::CTOnly);
    }
    ShowMenu (MenuId::NodeFlag);
    break;

  case 4:
    graph.ToggleFlags (NodeFlag::Lift);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 5:
    graph.ToggleFlags (NodeFlag::Sniper);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 6:
    graph.ToggleFlags (NodeFlag::Goal);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 7:
    graph.ToggleFlags (NodeFlag::Rescue);
    ShowMenu (MenuId::NodeFlag);
    break;

  case 8:
    if (!has_flag (graph[nearest].flags, NodeFlag::Crouch)) {
      graph.ToggleFlags (NodeFlag::Crouch);
      graph[nearest].origin.z += -18.0f;
    }
    else {
      graph.ToggleFlags (NodeFlag::Crouch);
      graph[nearest].origin.z += 18.0f;
    }

    ShowMenu (MenuId::NodeFlag);
    break;

  case 9:
    // if the node doesn't have a camp flag, set it and open the camp directions selection menu
    if (!has_flag (graph[nearest].flags, NodeFlag::Camp)) {
      graph.ToggleFlags (NodeFlag::Camp);
      ShowMenu (MenuId::CampDirections);
      break;
    }
    // otherwise remove the flag, and don't show the camp directions selection menu
    else {
      graph.ToggleFlags (NodeFlag::Camp);
      ShowMenu (MenuId::NodeFlag);
      break;
    }
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuCampDirections (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    graph.Add (NodeAddFlag::Camp);
    ShowMenu (MenuId::CampDirections);
    break;

  case 2:
    graph.Add (NodeAddFlag::CampEnd);
    ShowMenu (MenuId::CampDirections);
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuAutoPathDistance (int item) {
  CloseMenu (); // reset menu display

  if (item >= 1 && item <= 7) {
    constexpr float kDistanceValues[] = { 0.0f, 100.0f, 130.0f, 160.0f, 190.0f, 220.0f, 250.0f };

    graph.SetAutoPathDistance (kDistanceValues[item - 1]);
  }

  switch (item) {
  default:
    ShowMenu (MenuId::NodeAutoPath);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuKick (int item) {
  CloseMenu ();

  // get current page for this client
  auto &page = kick_pages_[game.IndexOfEntity (ent_)];

  if (page < 1) {
    page = 1;
  }

  // collect all active bot indices
  ystl::SmallArray<int32_t> bot_indices {};
  bot_indices.reserve (static_cast<size_t> (game.MaxClients () + 1));

  for (int i = 0; i < kGameMaxPlayers; ++i) {
    auto bot = bots[i];

    if (bot != nullptr && !(bot->pev->flags & FL_DORMANT)) {
      bot_indices.push (i);
    }
  }
  constexpr int kBotsPerPage = 7;

  const int total_bots = static_cast<int> (bot_indices.size ());
  const int total_pages = total_bots > 0 ? (total_bots + kBotsPerPage - 1) / kBotsPerPage : 1;

  if (page > total_pages) {
    page = total_pages;
  }

  switch (item) {
  case 1:
  case 2:
  case 3:
  case 4:
  case 5:
  case 6:
  case 7: {
    const int select_index = (page - 1) * kBotsPerPage + (item - 1);

    if (select_index < total_bots) {
      const auto bot_index = bot_indices[select_index];
      auto bot = bots[bot_index];

      if (bot != nullptr) {
        // selectindex is a position in botindices, erase by position
        bot_indices.erase (static_cast<size_t> (select_index), 1);

        bot->Kick ();
        bots.DecrementQuota ();

        const int new_total_bots = static_cast<int> (bot_indices.size ());
        const int new_total_pages = new_total_bots > 0 ? (new_total_bots + kBotsPerPage - 1) / kBotsPerPage : 1;

        if (page > new_total_pages && page > 1) {
          --page;
        }
      }
    }
    KickBotByMenu (page);
    break;
  }

  case 8: // more (next page)
    if (page < total_pages) {
      KickBotByMenu (++page);
    }
    break;

  case 9: // back (previous page)
    if (page > 1) {
      KickBotByMenu (--page);
    }
    break;

  case 10: // exit
    ShowMenu (MenuId::Control);
    break;
  }
  return CommandResult::Handled;
}

CommandResult Control::MenuGraphPath (int item) {
  CloseMenu (); // reset menu display

  switch (item) {
  case 1:
    graph.PathCreate (PathConnection::Outgoing);
    ShowMenu (MenuId::NodePath);
    break;

  case 2:
    graph.PathCreate (PathConnection::Incoming);
    ShowMenu (MenuId::NodePath);
    break;

  case 3:
    graph.PathCreate (PathConnection::Bidirectional);
    ShowMenu (MenuId::NodePath);
    break;

  case 4:
    graph.PathCreate (PathConnection::Jumping);
    ShowMenu (MenuId::NodePath);
    break;

  case 10:
    CloseMenu ();
    break;
  }
  return CommandResult::Handled;
}

bool Control::ExecuteCommands () {
  if (args_.empty ()) {
    return false;
  }
  const auto &prefix = args_.first ();

  // no handling if not for us
  if (prefix != product.cmd_pri && prefix != product.cmd_sec) {
    return false;
  }
  const auto &client = clients[ent_];

  // do not allow to execute stuff for non admins
  if (ent_ != game.GetLocalEntity () && !has_flag (client.flags, ClientFlags::Admin)) {
    Msg ("Access to %s commands is restricted.", product.name);

    // reset issuer, but returns "true" to suppress "unknown command" message
    SetIssuer (nullptr);

    return true;
  }

  // matches command against slash separated aliases without splitting into temporary arrays
  auto alias_match = [] (ystl::StringRef test, ystl::StringRef cmd, ystl::String &alias_name) -> bool {
    size_t start = 0;

    while (start <= test.size ()) {
      const auto end = test.find ("/", start);
      const auto segment = end == ystl::String::InvalidIndex ? test.substr (start) : test.substr (start, end - start);

      if (segment == cmd) {
        alias_name = segment;
        return true;
      }

      if (end == ystl::String::InvalidIndex) {
        break;
      }
      start = end + 1;
    }
    return false;
  };

  // returns the first alias of slash separated names, as a null-terminated copy
  auto first_alias = [] (ystl::StringRef test) -> ystl::String {
    const auto end = test.find ("/");
    return ystl::String (test.chars (), end == ystl::String::InvalidIndex ? test.size () : end);
  };

  ystl::String cmd {};

  // give some help
  if (HasArg (1) && Arg<ystl::StringRef> (1) == "help") {
    const auto has_second_arg = HasArg (2);

    for (auto &item : cmds_) {
      if (!has_second_arg) {
        cmd = first_alias (item.name);
      }

      if (!has_second_arg || alias_match (item.name, Arg<ystl::StringRef> (2), cmd)) {
        Msg ("Command: \"%s %s\"", prefix, cmd);
        Msg ("Format: %s", item.format);
        Msg ("Help: %s", conf.Translate (item.help));

        auto aliases = item.name.split ("/");

        if (aliases.size () > 1) {
          Msg ("Aliases: %s", ystl::String::join (aliases, ", "));
        }

        if (has_second_arg) {
          return true;
        }
        else {
          Msg ("\n");
        }
      }
    }

    if (!has_second_arg) {
      return true;
    }
    else {
      Msg ("No help found for \"%s\"", Arg<ystl::StringRef> (2));
    }
    return true;
  }
  cmd.clear ();

  // if no args passed just print all the commands
  if (args_.size () == 1) {
    Msg ("usage %s <command> [arguments]", prefix);
    Msg ("valid commands are: ");

    for (auto &item : cmds_) {
      if (!item.visible) {
        continue;
      }
      Msg ("  %-14.11s - %s", first_alias (item.name), ystl::String (conf.Translate (item.help)).lowercase ());
    }
    return true;
  }

  // first search for a actual cmd
  for (auto &item : cmds_) {
    if (alias_match (item.name, args_[1], cmd)) {
      switch ((this->*item.handler) ()) {
      case CommandResult::Handled:
      default:
        break;

      case CommandResult::ListenServer:
        Msg ("Command \"%s %s\" is only available from the listenserver console.", prefix, cmd);
        break;

      case CommandResult::BadFormat:
        Msg ("Incorrect usage of \"%s %s\" command. Correct usage is:", prefix, cmd);
        Msg ("\n\t%s\n", item.format);
        Msg ("Please type \"%s help %s\" to get more information.", prefix, cmd);
        break;
      }

      is_from_console_ = false;
      return true;
    }
  }
  Msg ("Unknown command: %s", args_[1]);

  // clear all the arguments upon finish
  args_.clear ();

  return true;
}

bool Control::ExecuteMenus () {
  if (!game.IsPlayerEntity (ent_) || game.IsBotCmd ()) {
    return false;
  }
  const auto &issuer = clients[ent_];

  // check if it's menu select, and some key pressed
  if (Arg<ystl::StringRef> (0) != "menuselect" || Arg<ystl::StringRef> (1).empty () || issuer.MenuId == MenuId::None) {
    return false;
  }

  // let's get handle
  for (auto &menu : menus_) {
    if (menu.ident == issuer.MenuId) {
      return (this->*menu.handler) (Arg<ystl::StringRef> (1).as<int> ()) == CommandResult::Handled;
    }
  }
  return false;
}

void Control::ShowMenu (MenuId id) {
  static bool menus_parsed = false;

  // make menus looks like we need only once
  if (!menus_parsed) {
    ignore_translate_ = false; // always translate menus

    for (auto &parsed : menus_) {
      ystl::StringRef translated = conf.Translate (parsed.text);

      // translate all the things
      parsed.text = translated;

      // make menu looks best
      if (!game.Is (GameFlags::Legacy)) {
        for (int j = 0; j < 10; ++j) {
          parsed.text.replace (ystl::strings.format ("%d.", j), ystl::strings.format ("\\r%d.\\w", j));
        }
      }
    }
    menus_parsed = true;
  }

  if (!game.IsPlayerEntity (ent_)) {
    return;
  }
  auto &client = clients[ent_];

  auto send_menu = [&] (int32_t slots, bool last, ystl::StringRef text) {
    MessageWriter (MSG_ONE, msgs.Id (NetMsg::ShowMenu), nullptr, ent_)
      .WriteShort (slots)
      .WriteChar (-1)
      .WriteByte (last ? HLFalse : HLTrue)
      .WriteString (text.chars ());
  };

  constexpr size_t kMaxMenuSentLength = 140;

  for (const auto &display : menus_) {
    if (display.ident == id) {
      ystl::String text = (game.Is (GameFlags::Xash3D | GameFlags::Mobility) && !cv_display_menu_text) ? " " : display.text.chars ();

      // split if needed
      if (text.size () > kMaxMenuSentLength) {

        // send in chunks, using a stack buffer instead of materializing chunk strings
        for (size_t offset = 0; offset < text.size (); offset += kMaxMenuSentLength) {
          const auto len = ystl::min (kMaxMenuSentLength, text.size () - offset);

          ystl::FixedArray<char, kMaxMenuSentLength + 1> chunk {};
          memcpy (chunk.data (), text.chars () + offset, len);

          chunk[len] = ystl::kNullChar;

          send_menu (display.slots, offset + len >= text.size (), ystl::StringRef (chunk.data (), len));
        }
      }
      else {
        send_menu (display.slots, true, text);
      }

      client.MenuId = id;
      engfuncs.pfnClientCommand (ent_, "speak \"player/geiger1\"\n"); // stops others from hearing menu sounds

      break;
    }
  }
}

void Control::CloseMenu () {
  if (!game.IsPlayerEntity (ent_)) {
    return;
  }
  auto &client = clients[ent_];

  // do not reset menu if already none
  if (client.MenuId == MenuId::None) {
    return;
  }
  MessageWriter (MSG_ONE, msgs.Id (NetMsg::ShowMenu), nullptr, ent_).WriteShort (0).WriteChar (0).WriteByte (0).WriteString ("");

  client.MenuId = MenuId::None;
}

void Control::KickBotByMenu (int requested_page) {
  static constexpr ystl::StringRef kHeaderTitleKey = "Bot Removal Menu";
  static constexpr ystl::StringRef kExitKey = "Exit";
  static constexpr ystl::StringRef kMoreKey = "More";
  static constexpr ystl::StringRef kBackKey = "Back";

  static constexpr int kBotsPerPage = 7;

  // collect all active bot indices
  ystl::SmallArray<int> bot_indices {};
  bot_indices.reserve (static_cast<size_t> (game.MaxClients () + 1));

  for (int i = 0; i < kGameMaxPlayers; ++i) {
    auto bot = bots[i];

    if (bot != nullptr && !(bot->pev->flags & FL_DORMANT)) {
      bot_indices.push (i);
    }
  }
  const int total_bots = static_cast<int> (bot_indices.size ());

  // no bots left - return to control menu
  if (total_bots == 0) {
    kick_pages_[game.IndexOfEntity (ent_)] = 0;
    ShowMenu (MenuId::Control);

    return;
  }
  const int total_pages = (total_bots + kBotsPerPage - 1) / kBotsPerPage;

  // clamp page to valid range
  int page = ystl::clamp (requested_page, 1, total_pages);

  // store current page for this client
  kick_pages_[game.IndexOfEntity (ent_)] = page;

  // build menu content
  ystl::String menu_text {};
  menu_text.assignf ("\\y%s (%d/%d):\\w\n\n", conf.Translate (kHeaderTitleKey), page, total_pages);

  // calculate which keys are available
  const bool is_first_page = (page == 1);
  const bool is_last_page = (page == total_pages);

  // key 0 (exit) is always available
  uint32_t menu_keys = ystl::bit (9u);

  // key 8 (more) only if not last page
  if (!is_last_page) {
    menu_keys |= ystl::bit (7u);
  }

  // key 9 (back) only if not first page
  if (!is_first_page) {
    menu_keys |= ystl::bit (8u);
  }
  menu_keys |= ystl::bit (10u);

  const int start_index = (page - 1) * kBotsPerPage;
  const int end_index = ystl::min (start_index + kBotsPerPage, total_bots);

  // add bot entries for current page
  for (int i = start_index; i < end_index; ++i) {
    const int bot_index = bot_indices[i];
    const int menu_slot = i - start_index;
    auto bot = bots[bot_index];

    if (bot == nullptr) {
      continue;
    }
    menu_keys |= static_cast<uint32_t> (ystl::bit (menu_slot));

    const ystl::StringRef team_suffix = (bot->team_ == Team::CT) ? " \\y(CT)\\w" : " \\r(T)\\w";
    menu_text.appendf ("\\r%1.1d. \\w%s%s\n", menu_slot + 1, bot->pev->netname.chars (), team_suffix);
  }
  menu_text.append ("\n");

  // add navigation options
  if (!is_last_page) {
    menu_text.appendf ("\\r8. \\w%s...\n", conf.Translate (kMoreKey));
  }

  if (!is_first_page) {
    menu_text.appendf ("\\r9. \\w%s\n", conf.Translate (kBackKey));
  }
  menu_text.appendf ("\n\\r0. \\w%s", conf.Translate (kExitKey));

  // clear current menu and update with new content
  CloseMenu ();

  // update the kick menu entry
  for (auto &menu : menus_) {
    if (menu.ident == MenuId::Kick) {
      menu.slots = static_cast<int> (menu_keys);
      menu.text = menu_text;
      break;
    }
  }
  ShowMenu (MenuId::Kick);
}

void Control::AssignAdminRights (edict_t *ent, char *infobuffer) {
  if (!game.IsDedicatedServer () || game.IsFakeClientEntity (ent)) {
    return;
  }
  ystl::StringRef key = cv_password_key.As<ystl::StringRef> ();
  ystl::StringRef password = cv_password.As<ystl::StringRef> ();

  if (!key.empty () && !password.empty ()) {
    auto &client = clients[ent];

    if (SecureCompare (password, engfuncs.pfnInfoKeyValue (infobuffer, key.chars ()))) {
      client.flags |= ClientFlags::Admin;
    }
    else {
      client.flags &= ~ClientFlags::Admin;
    }
  }
}

bool Control::SecureCompare (ystl::StringRef a, ystl::StringRef b) {
  auto len = a.size ();

  if (len != b.size ()) {
    return false;
  }
  volatile uint8_t result = 0;

  for (size_t i = 0; i < len; ++i) {
    result = static_cast<uint8_t> (result | (static_cast<uint8_t> (a[i]) ^ static_cast<uint8_t> (b[i])));
  }
  return result == 0;
}

void Control::MaintainAdminRights () {
  if (!game.IsDedicatedServer ()) {
    return;
  }

  ystl::StringRef key = cv_password_key.As<ystl::StringRef> ();
  ystl::StringRef password = cv_password.As<ystl::StringRef> ();

  for (auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }
    auto ent = client.ent;

    if (has_flag (client.flags, ClientFlags::Admin)) {
      if (key.empty () || password.empty ()) {
        client.flags &= ~ClientFlags::Admin;
      }
      else if (!SecureCompare (password, engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), key.chars ()))) {
        client.flags &= ~ClientFlags::Admin;
        Msg ("Player %s had lost remote access to %s.", ent->v.netname.chars (), product.name);
      }
    }
    else if (!has_flag (client.flags, ClientFlags::Admin) && !key.empty () && !password.empty ()) {
      if (SecureCompare (password, engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), key.chars ()))) {
        client.flags |= ClientFlags::Admin;
        Msg ("Player %s had gained full remote access to %s.", ent->v.netname.chars (), product.name);
      }
    }
  }
}

void Control::FlushPrintQueue () {
  if (!print_flush_timer_.elapsed () || print_queue_.empty ()) {
    return;
  }
  auto printable = print_queue_.pop_front ();

  // send to needed destination (text is already translated and formatted by msg())
  if (printable.dest == PrintQueueDest::ServerConsole) {
    game.SendServerMessage (printable.text.chars ());
  }
  else if (!game.IsNullEntity (printable.ent)) {
    game.SendClientMessage (true, printable.ent, printable.text.chars ());
  }
  print_flush_timer_.start (0.05f);
}

Control::Control () {
  ent_ = nullptr;
  djump_ = nullptr;

  deny_commands_ = true;
  ignore_translate_ = false;
  is_from_console_ = false;
  is_menu_fill_command_ = false;
  rapid_output_ = false;
  menu_server_fill_team_ = 5;
  print_flush_timer_.invalidate ();

  // declare the menus
  CreateMenus ();
}

void Control::RegisterCommands () {
  // build graph subcommands
  ystl::Array<Cmd> graph_subcommands {};

  graph_subcommands.emplace ("on", "on [display|auto|noclip|models]", "Enables displaying of graph, nodes, noclip cheat", &Control::CmdNodeOn);

  graph_subcommands.emplace (
    "off", "off [display|auto|noclip|models]", "Disables displaying of graph, auto adding nodes, noclip cheat", &Control::CmdNodeOff);

  graph_subcommands.emplace ("menu", "menu [noarguments]", "Opens and displays bots graph editor.", &Control::CmdNodeMenu);
  graph_subcommands.emplace ("add", "add [noarguments]", "Opens and displays graph node add menu.", &Control::CmdNodeAdd);
  graph_subcommands.emplace (
    "addbasic", "addbasic [noarguments]", "Adds basic nodes such as player spawn points, goals and ladders.", &Control::CmdNodeAddBasic);

  graph_subcommands.emplace ("save", "save [noarguments]", "Save graph file to disk.", &Control::CmdNodeSave);

  graph_subcommands.emplace ("load", "load [noarguments]", "Load graph file from disk.", &Control::CmdNodeLoad);
  graph_subcommands.emplace ("erase", "erase [iamsure]", "Erases the graph file from disk.", &Control::CmdNodeErase);
  graph_subcommands.emplace (
    "erase_training", "erase_training", "Erases the training data leaving graph files.", &Control::CmdNodeEraseTraining);

  graph_subcommands.emplace ("delete", "delete [nearest|index]", "Deletes single graph node from map.", &Control::CmdNodeDelete);
  graph_subcommands.emplace ("check", "check [noarguments]", "Check if graph working correctly.", &Control::CmdNodeCheck);
  graph_subcommands.emplace ("cache", "cache [nearest|index]", "Caching node for future use.", &Control::CmdNodeCache);

  graph_subcommands.emplace (
    "clean", "clean [all|nearest|index]", "Clean useless path connections from all or single node.", &Control::CmdNodeClean);

  graph_subcommands.emplace ("setradius", "setradius [radius] [nearest|index]", "Sets the radius for node.", &Control::CmdNodeSetRadius);

  graph_subcommands.emplace (
    "flags", "flags [noarguments]", "Open and displays menu for modifying flags for nearest point.", &Control::CmdNodeSetFlags);

  graph_subcommands.emplace ("teleport", "teleport [index]", "Teleports player to specified node index.", &Control::CmdNodeTeleport);
  graph_subcommands.emplace ("upload", "upload", "Uploads created graph to graph database.", &Control::CmdNodeUpload);
  graph_subcommands.emplace ("stats", "stats [noarguments]", "Shows the stats about node types on the map.", &Control::CmdNodeShowStats);
  graph_subcommands.emplace ("fileinfo", "fileinfo [noarguments]", "Shows basic information about graph file.", &Control::CmdNodeFileInfo);
  graph_subcommands.emplace (
    "export", "export [noarguments]", "Exports the graph into a text file (conf format), for version control.", &Control::CmdNodeExport);
  graph_subcommands.emplace (
    "import", "import [noarguments]", "Imports the graph from the text file, replacing the current one.", &Control::CmdNodeImport);
  graph_subcommands.emplace (
    "apply", "apply [noarguments]", "Saves the current graph and reloads it, so bots can be added after an import.", &Control::CmdNodeApply);

  graph_subcommands.emplace ("adjust_height", "adjust_height [height offset]",
    "Modifies all the graph nodes height (z-component) with specified offset.", &Control::CmdNodeAdjustHeight);

  graph_subcommands.emplace (
    "refresh", "refresh [noarguments]", "Deletes a current graph and downloads one from graph database.", &Control::CmdNodeRefresh);

  graph_subcommands.emplace ("path_create", "path_create [noarguments]", "Opens and displays path creation menu.", &Control::CmdNodePathCreate);

  graph_subcommands.emplace ("path_create_in", "path_create_in [noarguments]", "Creates incoming path connection from faced to nearest node.",
    &Control::CmdNodePathCreate);

  graph_subcommands.emplace ("path_create_out", "path_create_out [noarguments]", "Creates outgoing path connection from nearest to faced node.",
    &Control::CmdNodePathCreate);

  graph_subcommands.emplace ("path_create_both", "path_create_both [noarguments]",
    "Creates both-ways path connection between faced and nearest node.", &Control::CmdNodePathCreate);

  graph_subcommands.emplace ("path_create_jump", "path_create_jump [noarguments]", "Creates jumping path connection from nearest to faced node.",
    &Control::CmdNodePathCreate);

  graph_subcommands.emplace (
    "path_delete", "path_delete [noarguments]", "Deletes path from nearest to faced node.", &Control::CmdNodePathDelete);

  graph_subcommands.emplace ("path_set_autopath", "path_set_autopath [max_distance]", "Opens menu for setting autopath maximum distance.",
    &Control::CmdNodePathSetAutoDistance);

  graph_subcommands.emplace (
    "path_clean", "path_clean [index]", "Clears connections of all types from the node.", &Control::CmdNodePathCleanAll);

  graph_subcommands.emplace (
    "iterate_camp", "iterate_camp [begin|end|next]", "Allows to go through all camp points on map.", &Control::CmdNodeIterateCamp);

  if (game.IsDedicatedServer ()) {
    graph_subcommands.emplace (
      "acquire_editor", "acquire_editor [noarguments]", "Acquires rights to edit graph on dedicated server.", &Control::CmdNodeAcquireEditor);

    graph_subcommands.emplace (
      "release_editor", "release_editor [noarguments]", "Releases graph editing rights.", &Control::CmdNodeReleaseEditor);
  }

  // build debug subcommands
  ystl::Array<Cmd> debug_subcommands {};

  debug_subcommands.emplace ("slay", "slay [id|name|host]", "Kills a player by id or name (or host for listenserver).", &Control::CmdDebugSlay);

  debug_subcommands.emplace (
    "slap", "slap [id|name|host] [damage]", "Slaps a player by id or name (or host for listenserver).", &Control::CmdDebugSlap);

  debug_subcommands.emplace ("god", "god [id|name|host]", "Toggles god mode on host entity (listenserver) or a player.", &Control::CmdDebugGod);
  debug_subcommands.emplace ("notarget", "notarget [id|name|host]", "Toggles notarget on a player.", &Control::CmdDebugNotarget);
  debug_subcommands.emplace ("exec", "exec [id|name|host] [command]", "Executes a client command on a player.", &Control::CmdDebugExec);
  debug_subcommands.emplace ("memory", "memory", "Displays memory allocation statistics.", &Control::CmdDebugMemory);
  debug_subcommands.emplace ("translate", "translate [reset|write]",
    "Lists untranslated strings collected during play; 'write' appends them to the language config, 'reset' clears the list.",
    &Control::CmdDebugTranslate);

  // build main commands - use emplace to construct in-place
  cmds_.emplace ("add/addbot/add_ct/addbot_ct/add_t/addbot_t/addhs/addhs_t/addhs_ct", "add [difficulty] [personality] [team] [model] [name]",
    "Adding specific bot into the game.", &Control::CmdAddBot);

  cmds_.emplace (
    "kick/kickone/kick_ct/kick_t/kickbot_ct/kickbot_t", "kick [team]", "Kicks off the random bot from the game.", &Control::CmdKickBot);

  cmds_.emplace ("removebots/kickbots/kickall/kickall_ct/kickall_t", "removebots [instant] [team]", "Kicks all the bots from the game.",
    &Control::CmdKickBots);

  cmds_.emplace (
    "kill/killbots/killall/kill_ct/kill_t", "kill [team] [silent]", "Kills the specified team / all the bots.", &Control::CmdKillBots);

  cmds_.emplace ("fill/fillserver", "fill [team] [count] [difficulty] [personality]", "Fill the server (add bots) with specified parameters.",
    &Control::CmdFill);

  cmds_.emplace ("vote/votemap", "vote [map_id]", "Forces all the bots to vote for the specified map.", &Control::CmdVote);

  cmds_.emplace ("weapons/weaponmode", "weapons [knife|pistol|shotgun|smg|rifle|sniper|standard]", "Sets the bots' weapon mode to use.",
    &Control::CmdWeaponMode);

  cmds_.emplace ("fun/funmode/omg", "fun [imsober|tronisback|itsnewyear|imhaunted|itstoodark|stonedagain|imonmars]",
    "Sets the classic fun mode for everyone to enjoy.", &Control::CmdFun);

  cmds_.emplace ("menu/botmenu", "menu [cmd]", "Opens the main bot menu, or command menu if specified.", &Control::CmdMenu);
  cmds_.emplace ("version/ver/about", "version [no arguments]", "Displays version information about bot build.", &Control::CmdVersion);
  cmds_.emplace ("graphmenu/wpmenu/wptmenu", "graphmenu [noarguments]", "Opens and displays bots graph editor.", &Control::CmdNodeMenu);
  cmds_.emplace ("list/listbots", "list [noarguments]", "Lists the bots currently playing on server.", &Control::CmdList);

  cmds_.emplace (
    "graph/g/w/wp/wpt/waypoint", "graph [help]", "Handles graph operations.", &Control::CmdNode, true, ystl::move (graph_subcommands));

  cmds_.emplace ("cvars", "cvars [save|save_map|cvar|defaults]", "Display all the cvars with their descriptions.", &Control::CmdCvars);
  cmds_.emplace ("show_custom", "show_custom [noarguments]", "Shows the current values from custom.cfg.", &Control::CmdShowCustom, false);
  cmds_.emplace ("debug", "debug [help]", "Debug commands for players.", &Control::CmdDebug, false, ystl::move (debug_subcommands));
}

void Control::HandleEngineCommands () {
  if (deny_commands_) {
    return;
  }

  CollectArgs ();
  SetIssuer (game.GetLocalEntity ());

  SetFromConsole (true);
  ExecuteCommands ();
}

bool Control::HandleClientSideCommandsWrapper (edict_t *ent, bool is_menus) {
  if (deny_commands_) {
    return false;
  }

  // bots should never reach the command dispatcher or menus,
  if (game.IsFakeClientEntity (ent)) {
    return false;
  }

  CollectArgs ();
  SetIssuer (ent);

  SetFromConsole (!is_menus);
  auto result = is_menus ? ExecuteMenus () : ExecuteCommands ();

  if (!result) {
    SetIssuer (nullptr);
  }
  return result;
}

bool Control::HandleClientCommands (edict_t *ent) {
  return HandleClientSideCommandsWrapper (ent, false);
}

bool Control::HandleMenuCommands (edict_t *ent) {
  return HandleClientSideCommandsWrapper (ent, true);
}

void Control::EnableDrawModels (bool enable) {
  constexpr ystl::FixedArray<ystl::StringRef, 3> entities { "info_player_start", "info_player_deathmatch", "info_vip_start" };

  if (enable) {
    game.SetPlayerStartDrawModels ();
  }

  for (auto &entity : entities) {
    game.SearchEntities ("classname", entity, [&enable] (edict_t *ent) {
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

void Control::CreateMenus () {
  auto keys = [] (int num_keys) -> int {
    int result = 0;

    for (int i = 0; i < num_keys; ++i) {
      result |= ystl::bit (i);
    }
    result |= ystl::bit (9);

    return result;
  };

  // bots main menu
  menus_.emplace (MenuId::Main, keys (4),
    "\\yMain Menu\\w\n\n"
    "1. Control bots\n"
    "2. Features\n\n"
    "3. Fill server\n"
    "4. End round\n\n"
    "0. Exit",
    &Control::MenuMain);

  // bots features menu
  menus_.emplace (MenuId::Features, keys (6),
    "\\yBots Features\\w\n\n"
    "1. Weapon mode menu\n"
    "2. Graph editor\n"
    "3. Select personality\n\n"
    "4. Toggle debug mode\n"
    "5. Command menu\n\n"
    "6. Fun mode menu\n\n"
    "0. Exit",
    &Control::MenuFeatures);

  // bot control menu
  menus_.emplace (MenuId::Control, keys (5),
    "\\yBots Control Menu\\w\n\n"
    "1. Quick add bot\n"
    "2. Add specific bot\n\n"
    "3. Remove random bot\n"
    "4. Remove all bots\n\n"
    "5. Bot removal menu\n\n"
    "0. Exit",
    &Control::MenuControl);

  // weapon mode select menu
  menus_.emplace (MenuId::WeaponMode, keys (7),
    "\\yBots Weapon Mode\\w\n\n"
    "1. Knives only\n"
    "2. Pistols only\n"
    "3. Shotguns only\n"
    "4. Machine guns only\n"
    "5. Rifles only\n"
    "6. Sniper weapons only\n"
    "7. All weapons\n\n"
    "0. Exit",
    &Control::MenuWeaponMode);

  // fun mode select menu
  menus_.emplace (MenuId::FunModeId, keys (7),
    "\\yBots Fun Mode\\w\n\n"
    "1. Im sober\n"
    "2. Tron is back\n"
    "3. It's new year\n"
    "4. Im haunted\n"
    "5. It's too dark\n"
    "6. Stoned again\n"
    "7. Im on mars\n\n"
    "0. Exit",
    &Control::MenuFunMode);

  // personality select menu
  menus_.emplace (MenuId::Personality, keys (4),
    "\\yBots Personality\\w\n\n"
    "1. Random\n"
    "2. Normal\n"
    "3. Aggressive\n"
    "4. Careful\n\n"
    "0. Exit",
    &Control::MenuPersonality);

  // difficulty select menu
  menus_.emplace (MenuId::Difficulty, keys (5),
    "\\yBots Difficulty Level\\w\n\n"
    "1. Newbie\n"
    "2. Average\n"
    "3. Normal\n"
    "4. Professional\n"
    "5. Godlike\n\n"
    "0. Exit",
    &Control::MenuDifficulty);

  // team select menu
  menus_.emplace (MenuId::TeamSelect, keys (5),
    "\\ySelect a Team\\w\n\n"
    "1. Terrorist Force\n"
    "2. Counter-Terrorist Force\n\n"
    "5. Auto-select\n\n"
    "0. Exit",
    &Control::MenuTeamSelect);

  // terrorist model select menu
  menus_.emplace (MenuId::TerroristSelect, keys (5),
    "\\ySelect an Appearance\\w\n\n"
    "1. Phoenix Connexion\n"
    "2. L337 Krew\n"
    "3. Arctic Avengers\n"
    "4. Guerilla Warfare\n\n"
    "5. Auto-select\n\n"
    "0. Exit",
    &Control::MenuClassSelect);

  // counter-terrorist model select menu
  menus_.emplace (MenuId::CTSelect, keys (5),
    "\\ySelect an Appearance\\w\n\n"
    "1. Seal Team 6 (DEVGRU)\n"
    "2. German GSG-9\n"
    "3. UK SAS\n"
    "4. French GIGN\n\n"
    "5. Auto-select\n\n"
    "0. Exit",
    &Control::MenuClassSelect);

  // condition zero terrorist model select menu
  menus_.emplace (MenuId::TerroristSelectCZ, keys (6),
    "\\ySelect an Appearance\\w\n\n"
    "1. Phoenix Connexion\n"
    "2. L337 Krew\n"
    "3. Arctic Avengers\n"
    "4. Guerilla Warfare\n"
    "5. Midwest Militia\n\n"
    "6. Auto-select\n\n"
    "0. Exit",
    &Control::MenuClassSelect);

  // condition zero counter-terrorist model select menu
  menus_.emplace (MenuId::CTSelectCZ, keys (6),
    "\\ySelect an Appearance\\w\n\n"
    "1. Seal Team 6 (DEVGRU)\n"
    "2. German GSG-9\n"
    "3. UK SAS\n"
    "4. French GIGN\n"
    "5. Russian Spetsnaz\n\n"
    "6. Auto-select\n\n"
    "0. Exit",
    &Control::MenuClassSelect);

  // command menu
  menus_.emplace (MenuId::Commands, keys (4),
    "\\yBot Command Menu\\w\n\n"
    "1. Make double jump\n"
    "2. Finish double jump\n\n"
    "3. Drop the C4 bomb\n"
    "4. Drop the weapon\n\n"
    "0. Exit",
    &Control::MenuCommands);

  // main node menu
  menus_.emplace (MenuId::NodeMainPage1, keys (9),
    "\\yGraph Editor (Page 1)\\w\n\n"
    "1. Show/Hide nodes\n"
    "2. Cache node\n"
    "3. Create path\n"
    "4. Delete path\n"
    "5. Add node\n"
    "6. Delete node\n"
    "7. Set autopath distance\n"
    "8. Set radius\n\n"
    "9. Next...\n\n"
    "0. Exit",
    &Control::MenuGraphPage1);

  // main node menu (page 2)
  menus_.emplace (MenuId::NodeMainPage2, keys (9),
    "\\yGraph Editor (Page 2)\\w\n\n"
    "1. Debug goal\n"
    "2. Auto node placement on/off\n"
    "3. Set flags\n"
    "4. Save graph\n"
    "5. Save without checking\n"
    "6. Load graph\n"
    "7. Check graph\n"
    "8. Noclip cheat on/off\n\n"
    "9. Previous...\n\n"
    "0. Exit",
    &Control::MenuGraphPage2);

  // select nodes radius menu
  menus_.emplace (MenuId::NodeRadius, keys (9),
    "\\yNode Radius\\w\n\n"
    "1. 0 units\n"
    "2. 8 units\n"
    "3. 16 units\n"
    "4. 32 units\n"
    "5. 48 units\n"
    "6. 64 units\n"
    "7. 80 units\n"
    "8. 96 units\n"
    "9. 128 units\n\n"
    "0. Exit",
    &Control::MenuGraphRadius);

  // nodes add menu
  menus_.emplace (MenuId::NodeType, keys (9),
    "\\yNode Type\\w\n\n"
    "1. Normal\n"
    "\\r2. Terrorist important\n"
    "3. Counter-Terrorist important\n"
    "\\w4. Block with hostage / Ladder\n"
    "\\y5. Rescue zone\n"
    "\\w6. Camping\n"
    "7. Camp end\n"
    "\\r8. Map goal\n"
    "\\w9. Jump\n\n"
    "0. Exit",
    &Control::MenuGraphType);

  // debug goal menu
  menus_.emplace (MenuId::NodeDebug, keys (3),
    "\\yDebug Goal\\w\n\n"
    "1. Debug nearest node\n"
    "2. Debug facing node\n"
    "3. Stop debugging\n\n"
    "0. Exit",
    &Control::MenuGraphDebug);

  // set node flag menu
  menus_.emplace (MenuId::NodeFlag, keys (9),
    "\\yToggle Node Flags\\w\n\n"
    "1. Block with hostage\n"
    "2. Terrorists specific\n"
    "3. CTs specific\n"
    "4. Use elevator\n"
    "5. Sniper point (\\yfor camp points only!\\w)\n"
    "6. Map goal\n"
    "7. Rescue zone\n"
    "8. Crouch down\n"
    "9. Camp point\n\n"
    "0. Exit",
    &Control::MenuGraphFlag);

  // set camp directions menu
  menus_.emplace (MenuId::CampDirections, keys (2),
    "\\ySet Camp Point Directions\\w\n\n"
    "1. Camp start\n"
    "2. Camp end\n\n"
    "0. Exit",
    &Control::MenuCampDirections);

  // auto-path max distance
  menus_.emplace (MenuId::NodeAutoPath, keys (7),
    "\\yAutoPath Distance\\w\n\n"
    "1. 0 units\n"
    "2. 100 units\n"
    "3. 130 units\n"
    "4. 160 units\n"
    "5. 190 units\n"
    "6. 220 units\n"
    "7. 250 units (default)\n\n"
    "0. Exit",
    &Control::MenuAutoPathDistance);

  // path connections
  menus_.emplace (MenuId::NodePath, keys (4),
    "\\yCreate Path (Choose Direction)\\w\n\n"
    "1. Outgoing path\n"
    "2. Incoming path\n"
    "3. Bidirectional (both ways)\n"
    "4. Jumping path\n\n"
    "0. Exit",
    &Control::MenuGraphPath);

  // kick menu (dynamic, single entry with pagination)
  menus_.emplace (MenuId::Kick, 0x0, "", &Control::MenuKick);
}

} // namespace bot
