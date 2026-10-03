//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// command handler status
namespace bot {

enum class CommandResult : int32_t {
  Handled = 0, // command successfully handled
  ListenServer, // command is only available on listen server
  BadFormat // wrong params
};

// print queue destination
enum class PrintQueueDest : int32_t {
  ServerConsole, // use server console
  ClientConsole // use client console
};

// bot menu ids
enum class MenuId : int32_t {
  None = 0,
  Main,
  Features,
  Control,
  WeaponMode,
  Personality,
  Difficulty,
  TeamSelect,
  TerroristSelect,
  CTSelect,
  TerroristSelectCZ,
  CTSelectCZ,
  Commands,
  NodeMainPage1,
  NodeMainPage2,
  NodeRadius,
  NodeType,
  NodeFlag,
  NodeAutoPath,
  NodePath,
  NodeDebug,
  CampDirections,
  FunModeId,
  Kick
};

// bot command manager
class Control final : public ystl::Singleton<Control> {
public:
  using Handler = CommandResult (Control::*) ();
  using MenuHandler = CommandResult (Control::*) (int);

public:
  // generic bot command
  struct Cmd {
    ystl::String name {}, format {}, help {};
    Handler handler = nullptr;
    bool visible = true;
    ystl::Array<Cmd> subcommands {};

  public:
    Cmd () : name {}, format {}, help {}, handler (nullptr), visible (true), subcommands {} {}

    Cmd (ystl::StringRef name, ystl::StringRef format, ystl::StringRef help, Handler handler, bool visible = true) :
      name (name), format (format), help (help), handler (ystl::move (handler)), visible (visible), subcommands {} {}

    Cmd (ystl::StringRef name, ystl::StringRef format, ystl::StringRef help, Handler handler, bool visible, ystl::Array<Cmd> &&subcommands) :
      name (name), format (format), help (help), handler (ystl::move (handler)), visible (visible), subcommands (ystl::move (subcommands)) {}

    bool HasSubcommands () const {
      return !subcommands.empty ();
    }
  };

  // single bot menu
  struct Menu {
    MenuId ident {};
    int32_t slots {};
    ystl::String text {};
    MenuHandler handler {};

  public:
    explicit Menu (MenuId ident, int slots, ystl::StringRef text, MenuHandler handler) :
      ident (ident), slots (slots), text (text), handler (ystl::move (handler)) {}
  };
  ystl::FixedArray<int, kGameMaxPlayers + 1> kick_pages_ {};

  // queued text message to prevent overflow with rapid output
  struct PrintQueue {
    PrintQueueDest dest {};
    ystl::String text {};
    edict_t *ent {};

  public:
    explicit PrintQueue () = default;

    PrintQueue (PrintQueueDest dest, ystl::StringRef text, edict_t *ent = nullptr) : dest (dest), text (text), ent (ent) {}
  };

  // save old values of changed cvars to revert them back when editing turned off
  ystl::HashMap<ystl::String, float> game_cvar_holder_ {};

private:
  ystl::Array<ystl::String> args_ {};
  ystl::Array<Cmd> cmds_ {};
  ystl::Array<Menu> menus_ {};
  ystl::Deque<PrintQueue> print_queue_ {};
  ystl::Array<int32_t> camp_iterator_ {};

  edict_t *ent_ {};
  Bot *djump_ {};

  bool is_from_console_ {};
  bool rapid_output_ {};
  bool is_menu_fill_command_ {};
  bool ignore_translate_ {};
  bool deny_commands_ {};

  int menu_server_fill_team_ {};
  int inter_menu_data_[4] = {};

  ystl::CountdownTimer print_flush_timer_ {};

public:
  Control ();
  ~Control () = default;

private:
  CommandResult CmdAddBot ();
  CommandResult CmdKickBot ();
  CommandResult CmdKickBots ();
  CommandResult CmdKillBots ();
  CommandResult CmdFill ();
  CommandResult CmdVote ();
  CommandResult CmdWeaponMode ();
  CommandResult CmdFun ();
  CommandResult CmdVersion ();
  CommandResult CmdNodeMenu ();
  CommandResult CmdMenu ();
  CommandResult CmdList ();
  CommandResult CmdCvars ();
  CommandResult CmdShowCustom ();
  CommandResult CmdNode ();
  CommandResult CmdNodeOn ();
  CommandResult CmdNodeOff ();
  CommandResult CmdNodeAdd ();
  CommandResult CmdNodeAddBasic ();
  CommandResult CmdNodeSave ();
  CommandResult CmdNodeLoad ();
  CommandResult CmdNodeErase ();
  CommandResult CmdNodeRefresh ();
  CommandResult CmdNodeExport ();
  CommandResult CmdNodeImport ();
  CommandResult CmdNodeApply ();
  CommandResult CmdNodeEraseTraining ();
  CommandResult CmdNodeDelete ();
  CommandResult CmdNodeCheck ();
  CommandResult CmdNodeCache ();
  CommandResult CmdNodeClean ();
  CommandResult CmdNodeSetRadius ();
  CommandResult CmdNodeSetFlags ();
  CommandResult CmdNodeTeleport ();
  CommandResult CmdNodePathCreate ();
  CommandResult CmdNodePathDelete ();
  CommandResult CmdNodePathSetAutoDistance ();
  CommandResult CmdNodePathCleanAll ();
  CommandResult CmdNodeAcquireEditor ();
  CommandResult CmdNodeReleaseEditor ();
  CommandResult CmdNodeUpload ();
  CommandResult CmdNodeIterateCamp ();
  CommandResult CmdNodeShowStats ();
  CommandResult CmdNodeFileInfo ();
  CommandResult CmdNodeAdjustHeight ();
  CommandResult CmdDebug ();
  CommandResult CmdDebugSlay ();
  CommandResult CmdDebugSlap ();
  CommandResult CmdDebugGod ();
  CommandResult CmdDebugNotarget ();
  CommandResult CmdDebugExec ();
  CommandResult CmdDebugMemory ();

  CommandResult CmdDebugTranslate ();
  edict_t *ResolveDebugTarget (int arg_index);
  CommandResult DispatchSubcommand (const Cmd &parent_cmd, int arg_offset);

private:
  CommandResult MenuMain (int item);
  CommandResult MenuFeatures (int item);
  CommandResult MenuControl (int item);
  CommandResult MenuWeaponMode (int item);
  CommandResult MenuFunMode (int item);
  CommandResult MenuPersonality (int item);
  CommandResult MenuDifficulty (int item);
  CommandResult MenuTeamSelect (int item);
  CommandResult MenuClassSelect (int item);
  CommandResult MenuCommands (int item);
  CommandResult MenuGraphPage1 (int item);
  CommandResult MenuGraphPage2 (int item);
  CommandResult MenuGraphRadius (int item);
  CommandResult MenuGraphType (int item);
  CommandResult MenuGraphDebug (int item);
  CommandResult MenuGraphFlag (int item);
  CommandResult MenuGraphPath (int item);
  CommandResult MenuCampDirections (int item);
  CommandResult MenuAutoPathDistance (int item);

  CommandResult MenuKick (int item);

private:
  void CreateMenus ();

public:
  void RegisterCommands ();
  bool ExecuteCommands ();
  bool ExecuteMenus ();

  void ShowMenu (MenuId id);
  void CloseMenu ();

  void KickBotByMenu (int page);
  void AssignAdminRights (edict_t *ent, char *infobuffer);
  bool SecureCompare (ystl::StringRef a, ystl::StringRef b);
  void MaintainAdminRights ();
  void FlushPrintQueue ();
  void EnableDrawModels (bool enable);

public:
  void SetFromConsole (bool console) {
    is_from_console_ = console;
  }

  void SetRapidOutput (bool force) {
    rapid_output_ = force;
  }

  void SetDenyCommands (bool deny) {
    deny_commands_ = deny;
  }

  void SetIssuer (edict_t *ent) {
    ent_ = ent;
  }

  void ResetFlushTimestamp () {
    print_flush_timer_.invalidate ();
  }

  template <typename U> constexpr U Arg (const size_t index) const {
    if constexpr (ystl::is_same_v<U, float>) {
      if (!HasArg (index)) {
        return 0.0f;
      }
      return args_[index].as<float> ();
    }
    else if constexpr (ystl::is_same_v<U, int>) {
      if (!HasArg (index)) {
        return 0;
      }
      return args_[index].as<int> ();
    }
    else if constexpr (ystl::is_same_v<U, Personality>) {
      if (!HasArg (index)) {
        return Personality::Invalid;
      }
      return static_cast<Personality> (args_[index].as<int> ());
    }
    else if constexpr (ystl::is_same_v<U, CSTeam>) {
      if (!HasArg (index)) {
        return CSTeam::Invalid;
      }
      return static_cast<CSTeam> (args_[index].as<int> ());
    }
    else if constexpr (ystl::is_same_v<U, Team>) {
      if (!HasArg (index)) {
        return Team::Invalid;
      }
      return static_cast<Team> (args_[index].as<int> ());
    }
    else if constexpr (ystl::is_same_v<U, Difficulty>) {
      if (!HasArg (index)) {
        return Difficulty::Invalid;
      }
      return static_cast<Difficulty> (args_[index].as<int> ());
    }
    else if constexpr (ystl::is_same_v<U, ystl::StringRef>) {
      if (!HasArg (index)) {
        return "";
      }
      return args_[index];
    }
  }

  bool HasArg (size_t arg) const {
    return arg < args_.size ();
  }

  bool IgnoreTranslate () const {
    return ignore_translate_;
  }

  void CollectArgs () {
    args_.clear ();

    for (auto i = 0; i < engfuncs.pfnCmd_Argc (); ++i) {
      ystl::String arg = engfuncs.pfnCmd_Argv (i);

      // only make case-insensetive command itself and first argument
      if (i < 2) {
        arg = arg.lowercase ();
      }
      args_.emplace (arg);
    }
  }

  edict_t *GetIssuer () {
    return ent_;
  }

  // global helper for sending message to correct channel
  template <typename... Args> void Msg (const char *fmt, Args &&...args);

  // current debug level (yb_debug 0-4)
  int DebugLevel () const;

  // check if debug output for given level is enabled (default: any level >= 1)
  bool IsDebug (int level = 1) const;

  // unified debug output: forwards to msg () only when yb_debug >= level
  template <typename... Args> void Debug (const char *fmt, Args &&...args);
  template <typename... Args> void Debug (int level, const char *fmt, Args &&...args);

public:
  // for the server commands
  void HandleEngineCommands ();

  // wrapper for menus and commands
  bool HandleClientSideCommandsWrapper (edict_t *ent, bool is_menus);

  // for the client commands
  bool HandleClientCommands (edict_t *ent);

  // for the client menu commands
  bool HandleMenuCommands (edict_t *ent);
};

// global helper for sending message to correct channel
template <typename... Args> inline void Control::Msg (const char *fmt, Args &&...args) {
  ignore_translate_ = game.Is (GameFlags::Legacy);

  auto result = ystl::strings.format (conf.Translate (fmt), ystl::forward<Args> (args)...);
  auto result_len = strnlen (result, ystl::Strings::StaticBufferSize);
  auto message = ystl::strings.concat (result, "\n", ystl::Strings::StaticBufferSize);

  // if no receiver or many message have to appear, just print to server console
  if (game.IsNullEntity (ent_)) {

    if (rapid_output_) {
      print_queue_.emplace_last (PrintQueueDest::ServerConsole, message);
    }
    else {
      game.SendServerMessage (message);
    }
    return;
  }

  if (is_from_console_ || result_len > 96 || rapid_output_) {
    if (rapid_output_) {
      print_queue_.emplace_last (PrintQueueDest::ClientConsole, message, ent_);
    }
    else {
      game.SendClientMessage (true, ent_, message);
    }
  }
  else {
    game.SendClientMessage (false, ent_, message);
    game.SendClientMessage (true, ent_, message);
  }
}

// unified debug output: forwards to msg () only when yb_debug >= level
template <typename... Args> inline void Control::Debug (const char *fmt, Args &&...args) {
  Debug (1, fmt, ystl::forward<Args> (args)...);
}

template <typename... Args> inline void Control::Debug (int level, const char *fmt, Args &&...args) {
  if (IsDebug (level)) {
    Msg (fmt, ystl::forward<Args> (args)...);
  }
}

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Control, ctrl);

} // namespace bot
