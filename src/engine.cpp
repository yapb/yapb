//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

Game::Game () {
  start_entity_ = nullptr;
  local_entity_ = nullptr;

  precached_ = false;

  game_flags_ = GameFlags::None;
  map_flags_ = MapFlags::None;

  one_second_timer_.invalidate ();
  half_second_timer_.invalidate ();

  cvars_.clear ();
}

void Game::Precache () {
  if (precached_) {
    return;
  }
  precached_ = true;

  draw_models_[DrawLineType::Simple] = engine_wrap_.PrecacheModel ("sprites/laserbeam.spr");
  draw_models_[DrawLineType::Arrow] = engine_wrap_.PrecacheModel ("sprites/arrow1.spr");

  engine_wrap_.PrecacheSound ("weapons/xbow_hit1.wav"); // node add
  engine_wrap_.PrecacheSound ("weapons/mine_activate.wav"); // node delete
  engine_wrap_.PrecacheSound ("common/wpn_hudon.wav"); // path add/delete done

  map_flags_ = MapFlags::None; // reset map type as worldspawn is the first entity spawned
  RegisterCvars (true);
}

void Game::LevelInitialize (edict_t *entities, int max) {
  // this function precaches needed models and initialize class variables

  // set the global timer function
  ystl::timer_source.set_time_address (&globals->time);

  // enable command handling
  ctrl.SetDenyCommands (false);

  // re-initialize bot's array
  bots.Destroy ();

  // startup threaded worker
  worker.Startup (cv_threadpool_workers.As<int> ());

  // clear breakable validity before initialization (slots recycle across maps)
  checked_breakables_.zap ();
  checked_breakables_.reserve (128);
  game_state.SetHasBreakables (false);

  // initialize all config files
  conf.LoadConfigs ();

  // update worldmodel
  illum.ResetWorldModel ();

  // execute main config
  conf.LoadMainConfig ();

  // ensure the server admin is confident about features he's using
  EnsureHealthyGameEnvironment ();

  // load map-specific config
  conf.LoadMapSpecificConfig ();

  // do level initialization stuff here
  graph.LoadGraphData ();

  // initialize quota management
  bots.InitQuota ();

  // install the sendto hook to fake queries
  fakequeries.Init ();

  // flush any print queue
  ctrl.ResetFlushTimestamp ();

  // restart the fakeping timer, so it'll start working after mapchange
  fakeping.RestartTimer ();

  // go thru the all entities on map, and do whatever we're want
  for (int i = 0; i < max; ++i) {
    auto ent = entities + i;

    // only valid entities
    if (!ent || ent->v.classname == 0) {
      continue;
    }
    auto classname = ent->v.classname.str ();

    if (classname == "worldspawn") {
      start_entity_ = ent;
    }
    else if (classname == "player_weaponstrip") {
      if (Is (GameFlags::Legacy) && ystl::strings.is_empty (ent->v.target.chars ())) {
        ent->v.target = ent->v.targetname = engfuncs.pfnAllocString ("fake");
      }
      else if (!Is (GameFlags::ReGameDLL)) {
        engfuncs.pfnRemoveEntity (ent);
      }
    }
    else if (classname == "info_player_start" || classname == "info_vip_start") {
      ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
      ent->v.renderamt = 127; // set its transparency amount
      ent->v.effects |= EF_NODRAW;
    }
    else if (classname == "info_player_deathmatch") {
      ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
      ent->v.renderamt = 127; // set its transparency amount
      ent->v.effects |= EF_NODRAW;
    }
    else if (classname == "func_vip_safetyzone" || classname == "info_vip_safetyzone") {
      map_flags_ |= MapFlags::Assassination; // assassination map
    }
    else if (IsHostageEntity (ent)) {
      map_flags_ |= MapFlags::HostageRescue; // rescue map
    }
    else if (classname == "func_bomb_target" || classname == "info_bomb_target") {
      map_flags_ |= MapFlags::Demolition; // defusion map
    }
    else if (classname == "func_escapezone") {
      map_flags_ |= MapFlags::Escape;

      // strange thing on some es maps, where hostage entity present there
      if (MapIs (MapFlags::HostageRescue)) {
        map_flags_ &= ~MapFlags::HostageRescue;
      }
    }
    else if (IsDoorEntity (ent)) {
      map_flags_ |= MapFlags::HasDoors;
    }
    else if (classname.starts_with ("func_button")) {
      map_flags_ |= MapFlags::HasButtons;
    }
    else if (IsBreakableEntity (ent, true)) {

      // seed validity, absence means excluded (impulse gate or spawn hook below)
      if (ent->v.impulse <= 0) {
        checked_breakables_.insert (IndexOfEntity (ent));
      }
      game_state.SetHasBreakables (true);
    }
  }

  // next maps doesn't have map-specific entities, so determine it by name
  if (!cv_ignore_map_prefix_game_mode) {
    ystl::StringRef prefix = GetMapName ();

    if (prefix.starts_with ("fy_")) {
      map_flags_ |= MapFlags::FightYard;
    }
    else if (prefix.starts_with ("ka_")) {
      map_flags_ |= MapFlags::KnifeArena;
    }
    else if (prefix.starts_with ("he_")) {
      map_flags_ |= MapFlags::GrenadeWar;
    }
  }

  // reset some timers
  one_second_timer_.invalidate ();
  half_second_timer_.invalidate ();
}

void Game::OnSpawnEntity (edict_t *ent) {
  constexpr auto kEntityInfoPlayerStart = ystl::StringRef::fnv1a32 ("info_player_start");
  constexpr auto kEntityInfoVIPStart = ystl::StringRef::fnv1a32 ("info_vip_start");
  constexpr auto kEntityInfoPlayerDeathmatch = ystl::StringRef::fnv1a32 ("info_player_deathmatch");
  constexpr auto kEntityWorldspawn = ystl::StringRef::fnv1a32 ("worldspawn");

  if (!ent || ent->free || ent->v.classname == 0) {
    return;
  }
  const auto class_name_hash = ent->v.classname.str ().hash ();

  // keep worldspawn edict as base since it spawns first
  if (class_name_hash == kEntityWorldspawn) {
    start_entity_ = ent;
  }
  else if (class_name_hash == kEntityInfoPlayerStart || class_name_hash == kEntityInfoVIPStart) {
    ++spawn_count_[Team::CT];
  }
  else if (class_name_hash == kEntityInfoPlayerDeathmatch) {
    ++spawn_count_[Team::Terrorist];
  }
}

void Game::LevelShutdown () {
  // save collected practice on shutdown
  practice.Save ();

  // stop thread pool
  worker.Shutdown ();

  // destroy global killer entity
  bots.DestroyKillerEntity ();

  // ensure players are off on xash3d
  if (Is (GameFlags::Xash3D) || Is (GameFlags::Xash3DLegacy)) {
    bots.KickEveryone (true, false, true);
  }

  // set state to unprecached
  SetUnprecached ();

  // enable lightstyle animations on level change
  illum.EnableAnimation (true);

  // reset fun mode timers, game time is about to be reset
  fun_mode.ResetTimers ();

  // send message on new map
  util.SetNeedForWelcome (false);

  // clear local entity
  SetLocalEntity (nullptr);

  // invalidate the worldspawn edict, as the engine is about to free up the edicts pool
  start_entity_ = nullptr;

  // reset graph state
  graph.Reset ();

  // drop cached mode walls, map ents are freed
  mode_walls.Reset ();

  // suspend any analyzer tasks
  analyzer.Suspend ();

  // disable command handling
  ctrl.SetDenyCommands (true);

  // reset spawn counts
  ystl::fill (spawn_count_, 0);
}

void Game::DrawLine (edict_t *ent, const ystl::Vector &start, const ystl::Vector &end, int width, int noise, const ystl::Color &color,
  int brightness, int speed, int life, DrawLineType type) const {
  // this function draws a arrow visible from the client side of the player whose player entity
  // is pointed to by ent, from the vector location start to the vector location end,
  // which is supposed to last life tenths seconds, and having the color defined by RGB

  if (!IsPlayerEntity (ent)) {
    return; // reliability check
  }

  MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, ent)
    .WriteByte (TE_BEAMPOINTS)
    .WriteCoord (end.x)
    .WriteCoord (end.y)
    .WriteCoord (end.z)
    .WriteCoord (start.x)
    .WriteCoord (start.y)
    .WriteCoord (start.z)
    .WriteShort (draw_models_[type])
    .WriteByte (0) // framestart
    .WriteByte (10) // framerate
    .WriteByte (life) // life in 0.1's
    .WriteByte (width) // width
    .WriteByte (noise) // noise
    .WriteByte (color.red) // r, g, b
    .WriteByte (color.green) // r, g, b
    .WriteByte (color.blue) // r, g, b
    .WriteByte (brightness) // brightness
    .WriteByte (speed); // speed
}

void Game::DrawBox (edict_t *ent, const ystl::Vector &bbmin, const ystl::Vector &bbmax, int width, int noise, const ystl::Color &color,
  int brightness, int speed, int life) const {
  // draws an axis-aligned box outline as twelve edges, wrapping drawLine

  if (!IsPlayerEntity (ent)) {
    return; // reliability check
  }

  // eight corners
  const ystl::Vector c[8] {
    { bbmin.x, bbmin.y, bbmin.z },
    { bbmax.x, bbmin.y, bbmin.z },
    { bbmax.x, bbmax.y, bbmin.z },
    { bbmin.x, bbmax.y, bbmin.z },
    { bbmin.x, bbmin.y, bbmax.z },
    { bbmax.x, bbmin.y, bbmax.z },
    { bbmax.x, bbmax.y, bbmax.z },
    { bbmin.x, bbmax.y, bbmax.z },
  };

  // bottom, top and vertical edges
  static constexpr int kEdges[12][2] {
    { 0, 1 },
    { 1, 2 },
    { 2, 3 },
    { 3, 0 }, // bottom square
    { 4, 5 },
    { 5, 6 },
    { 6, 7 },
    { 7, 4 }, // top square
    { 0, 4 },
    { 1, 5 },
    { 2, 6 },
    { 3, 7 }, // verticals
  };

  for (const auto &edge : kEdges) {
    DrawLine (ent, c[edge[0]], c[edge[1]], width, noise, color, brightness, speed, life);
  }
}

bool Game::IsDedicatedServer () {
  // return true if server is dedicated server, false otherwise
  static const bool dedicated = engfuncs.pfnIsDedicatedServer () > 0;

  return dedicated;
}

const char *Game::GetRunningModName () {
  // this function returns mod name without path

  static ystl::String name {};

  if (!name.empty ()) {
    return name.chars ();
  }

  char engine_mod_name[ystl::Strings::StaticBufferSize] {};
  engfuncs.pfnGetGameDir (engine_mod_name);

  name = engine_mod_name;
  size_t slash = name.find_last_of ("\\/");

  if (slash != ystl::String::InvalidIndex) {
    name = name.substr (slash + 1);
  }
  name = name.trim (" \\/");
  return name.chars ();
}

const char *Game::GetMapName () {
  // this function gets the map name and store it in the map_name global string variable

  return ystl::strings.format ("%s", globals->mapname.chars ());
}

ystl::Vector Game::GetEntityOrigin (edict_t *ent) {
  // this expanded function returns the vector origin of a bounded entity, assuming that any
  // entity that has a bounding box has its center at the center of the bounding box itself

  if (IsNullEntity (ent)) {
    return nullptr;
  }

  if (ent->v.origin.empty ()) {
    return ent->v.absmin + ent->v.size * 0.5;
  }
  return ent->v.origin;
}

void Game::RegisterEngineCommand (const char *command, void func ()) {
  // this function tells the engine that a new server command is being declared, in addition
  // to the standard ones, whose name is command_name. The engine is thus supposed to be aware
  // that for every "command_name" server command it receives, it should call the function
  // pointed to by "function" in order to handle it

  // check for hl pre 1.1.0.4, as it's doesn't have pfnaddservercommand and many more stuff we need to work
  if (!ystl::plat.is_valid_ptr (engfuncs.pfnAddServerCommand)) {
    ystl::logger.fatal (
      "%s's minimum HL engine version is 1.1.0.4 and minimum Counter-Strike is Beta 6.5. Please update your engine / game version.",
      product.name);
  }
  else {
    engfuncs.pfnAddServerCommand (command, func);
  }
}

void Game::PlaySound (edict_t *ent, const char *sound) {
  if (IsNullEntity (ent)) {
    return;
  }
  engfuncs.pfnEmitSound (ent, CHAN_WEAPON, sound, 1.0f, ATTN_NORM, 0, 100);
}

Team Game::GetPlayerTeam (edict_t *ent) const {
  // gets the player team

  if (IsNullEntity (ent)) {
    return Team::Unassigned;
  }
  return clients[IndexOfPlayer (ent)].team;
}

Team Game::GetRealPlayerTeam (edict_t *ent) const {
  // gets the player team (real in ffa)

  if (IsNullEntity (ent)) {
    return Team::Unassigned;
  }
  return clients[IndexOfPlayer (ent)].team2;
}

void Game::SetPlayerStartDrawModels () {
  static ystl::HashMap<ystl::String, ystl::String> models {
    { "info_player_start",      "models/player/urban/urban.mdl"   },
    { "info_player_deathmatch", "models/player/terror/terror.mdl" },
    { "info_vip_start",         "models/player/vip/vip.mdl"       }
  };

  for (const auto &pair : models) {
    SearchEntities ("classname", pair.first, [&] (edict_t *ent) {
      engine_wrap_.SetModel (ent, pair.second.chars ());
      return EntitySearchResult::Continue;
    });
  }
}

bool Game::CheckVisibility (edict_t *ent, uint8_t *set) {
  if (!set) {
    return true;
  }

  if (ent->headnode < 0) {
    for (int i = 0; i < ent->num_leafs; ++i) {
      const auto leaf = ent->leafnums[i];

      if (set[leaf >> 3] & ystl::bit (leaf & 7)) {
        return true;
      }
    }
    return false;
  }

  for (int i = 0; i < MAX_ENT_LEAFS; ++i) {
    const auto leaf = ent->leafnums[i];

    if (leaf == -1) {
      break;
    }

    if (set[leaf >> 3] & ystl::bit (leaf & 7)) {
      return true;
    }
  }
  return engfuncs.pfnCheckVisibility (ent, set) > 0;
}

uint8_t *Game::GetVisibilitySet (Bot *bot, bool pvs) const {
  if (Is (GameFlags::Xash3DLegacy)) {
    return nullptr;
  }
  auto eyes = bot->GetEyesPos ();

  if (bot->IsDucking ()) {
    eyes += VEC_HULL_MIN - VEC_DUCK_HULL_MIN;
  }
  return pvs ? engfuncs.pfnSetFatPVS (eyes) : engfuncs.pfnSetFatPAS (eyes);
}

void Game::SendClientMessage (bool console, edict_t *ent, ystl::StringRef message) {
  // helper to sending the client message

  // do not send messages to fake clients
  if (!IsPlayerEntity (ent) || IsFakeClientEntity (ent)) {
    return;
  }

  // if console message and destination is listenserver entity, just print via server message instead of through unreliable channel
  if (console && ent == GetLocalEntity ()) {
    SendServerMessage (message);
    return;
  }

  // used to split messages
  auto send_text_msg = [&console, &ent] (ystl::StringRef text) {
    MessageWriter (MSG_ONE_UNRELIABLE, msgs.Id (NetMsg::TextMsg), nullptr, ent)
      .WriteByte (console ? HUD_PRINTCONSOLE : HUD_PRINTCENTER)
      .WriteString (text.chars ());
  };

  // do not excess limit
  constexpr size_t kMaxSendLength = 125;

  // split up the string into utf-8 safe chunks
  if (message.size () > kMaxSendLength) {
    ystl::utf8tools.for_each_chunk (message, kMaxSendLength, [&] (ystl::StringRef chunk) {
      send_text_msg (ystl::String (chunk));
    });
    return;
  }
  send_text_msg (message);
}

void Game::SendServerMessage (ystl::StringRef message) {
  // helper to sending the server message

  // do not excess limit
  constexpr size_t kMaxSendLength = 175;

  // split up the string into utf-8 safe chunks
  if (message.size () > kMaxSendLength) {
    ystl::utf8tools.for_each_chunk (message, kMaxSendLength, [&] (ystl::StringRef chunk) {
      engfuncs.pfnServerPrint (ystl::String (chunk).chars ());
    });
    return;
  }
  engfuncs.pfnServerPrint (message.chars ());
}

void Game::SendHudMessage (edict_t *ent, const hudtextparms_t &htp, ystl::StringRef message) {
  constexpr size_t kMaxSendLength = 512;

  if (IsNullEntity (ent)) {
    return;
  }
  MessageWriter msg (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, ent);

  msg.WriteByte (TE_TEXTMESSAGE);
  msg.WriteByte (htp.channel & 0xff);
  msg.WriteShort (MessageWriter::Fs16 (htp.x, 13.0f));
  msg.WriteShort (MessageWriter::Fs16 (htp.y, 13.0f));
  msg.WriteByte (htp.effect);
  msg.WriteByte (htp.r1);
  msg.WriteByte (htp.g1);
  msg.WriteByte (htp.b1);
  msg.WriteByte (htp.a1);
  msg.WriteByte (htp.r2);
  msg.WriteByte (htp.g2);
  msg.WriteByte (htp.b2);
  msg.WriteByte (htp.a2);
  msg.WriteShort (MessageWriter::Fu16 (htp.fadeinTime, 8.0f));
  msg.WriteShort (MessageWriter::Fu16 (htp.fadeoutTime, 8.0f));
  msg.WriteShort (MessageWriter::Fu16 (htp.holdTime, 8.0f));

  if (htp.effect == 2) {
    msg.WriteShort (MessageWriter::Fu16 (htp.fxTime, 8.0f));
  }

  // truncate only when needed, so short messages go out without a temporary copy
  if (message.size () > kMaxSendLength) {
    msg.WriteString (message.substr (0, kMaxSendLength).chars ());
  }
  else {
    msg.WriteString (message.chars ());
  }
}

void Game::PrepareBotArgs (edict_t *ent) {
  // the purpose of this function is to provide fakeclients (bots) with the same client
  // command-scripting advantages (putting multiple commands in one line between semicolons)
  // as real players. It is an improved version of botman's FakeClientCommand, in which you
  // supply directly the whole string as if you were typing it in the bot's "console". It
  // is supposed to work exactly like the pfnClientCommand (server-sided client command)

  auto &cmd = bot_cmd_;
  cmd.Reset ();

  // parts of the command, separated by semicolons
  ystl::Tokenizer scanner { ystl::StringRef (cmd.buffer, cmd.length) };

  // splits a single part into tokens and executes it
  const auto execute_part = [&] (ystl::StringRef part) {
    ystl::Tokenizer words { part };

    const auto command = words.read_until (' ');
    const auto say_command = command.starts_with ("say");

    size_t tail_length = 0;

    if (words.accept (' ') && words.peek () == '"') {
      // quoted value: the rest of the part is a single argument
      words.advance (); // skip the opening quote
      words.skip_while ([] (char ch) {
        return ch == '"';
      });

      const auto value = words.rest ();

      cmd.args[0] = command;
      cmd.args[1] = value;
      cmd.arg_count = 2;

      // args string for the gamedll: the message for say commands, the joined part for the rest
      if (say_command) {
        tail_length = value.size ();
        memcpy (cmd.tail_buffer, value.chars (), tail_length);
      }
      else {
        tail_length = command.size () + 1 + value.size ();
        memcpy (cmd.tail_buffer, command.chars (), command.size ());

        cmd.tail_buffer[command.size ()] = ' ';
        memcpy (cmd.tail_buffer + command.size () + 1, value.chars (), value.size ());
      }
    }
    else if (words.eof ()) {
      // part without arguments is a single token
      cmd.args[0] = command;
      cmd.arg_count = 1;

      if (!say_command) {
        tail_length = part.size ();
        memcpy (cmd.tail_buffer, part.chars (), tail_length);
      }
    }
    else {
      // whitespace separated arguments
      cmd.args[0] = command;

      size_t count = 1;

      while (count < Command::kMaxArgs) {
        words.skip_while ([] (char ch) {
          return ch == ' ';
        });

        if (words.eof ()) {
          break;
        }
        cmd.args[count++] = words.read_until (' ');
      }
      cmd.arg_count = count;

      if (say_command) {
        // trim the say text in place, without a temporary string
        const auto tail_start = ystl::min (command.size () + 1, part.size ());
        const auto raw = ystl::StringRef (part.chars () + tail_start, part.size () - tail_start);
        const auto tail = ystl::Tokenizer::trim (raw, " ");

        tail_length = tail.size ();
        memcpy (cmd.tail_buffer, tail.chars (), tail_length);
      }
      else {
        tail_length = part.size ();
        memcpy (cmd.tail_buffer, part.chars (), tail_length);
      }
    }

    // the gamedll reads tokens as c-strings, terminate the token spans in place
    for (size_t i = 0; i < cmd.arg_count; ++i) {
      cmd.Terminate (cmd.args[i]);
    }
    // ... and the args string as well, it holds stale bytes from the previous command
    cmd.tail_buffer[tail_length] = ystl::kNullChar;
    cmd.tail = ystl::StringRef (cmd.tail_buffer, tail_length);

    MDLL_ClientCommand (ent);
    cmd.Reset (); // clear space for next cmd
  };

  while (!scanner.eof ()) {
    const auto part = ystl::Tokenizer::trim (scanner.read_until (';'), " \t\r\n\"");

    scanner.accept (';'); // consume the separator when present

    if (part.empty ()) {
      continue; // empty part, nothing to execute
    }
    execute_part (part);
  }
}

bool Game::IsSoftwareRenderer () {
  // xash always use "hw" structures
  if (Is (GameFlags::Xash3D)) {
    return false;
  }

  // dedicated server (except xash) always use "sw" structures
  if (IsDedicatedServer ()) {
    return true;
  }
  auto model = illum.GetWorldModel ();

  if (!model) {
    return false;
  }

  if (model->nodes[0].parent != nullptr) {
    return false;
  }
  const auto child = model->nodes[0].children[0];

  if (child < model->nodes || child > model->nodes + model->numnodes) {
    return false;
  }

  if (child->parent != &model->nodes[0]) {
    return false;
  }

  // and on only windows version you can use software-render game. linux, macos always defaults to opengl
  if (ystl::plat.win) {
    return ystl::SharedLibrary::has_module ("sw");
  }
  return false;
}

bool Game::Is25thAnniversaryUpdate () {
  static ConVarRef host_hl25_extended_structs ("host_hl25_extended_structs");

  // xash3d ships its own hl25 marker, so never probe the steam cvar there
  if (host_hl25_extended_structs.Exists ()) {
    return host_hl25_extended_structs.Value () > 0.0f;
  }

  static ConVarRef sv_use_steam_networking ("sv_use_steam_networking");
  return sv_use_steam_networking.Exists ();
}

bool Game::IsGoldClientListenServer () {
  static ConVarRef sv_userinfo_transmitted_fields ("sv_userinfo_transmitted_fields");
  static ConVarRef sv_rcon_whitelist_address ("sv_rcon_whitelist_address");

  return !IsDedicatedServer () && sv_userinfo_transmitted_fields.Exists () && sv_rcon_whitelist_address.Exists ();
}

void Game::DetectXashEngine () {
  if (engfuncs.pfnCVarGetPointer ("host_ver") != nullptr) {
    game_flags_ |= GameFlags::Xash3D;
  }
  // legacy xash3d branches have no host_ver, only the old build cvar
  else if (engfuncs.pfnCVarGetPointer ("build") != nullptr) {
    game_flags_ |= GameFlags::Xash3DLegacy;
  }
}

void Game::PushConVar (const ConVarSpec &spec, ConVar *self) {
  // this function adds globally defined variable to registration stack

  ConVarReg reg {};

  // engine-visible pointers aim at ConVar members: static objects, assigned once
  // in the constructor, never relocated or freed afterwards (unlike registry copies,
  // which store_var_value reassigns on every config load)
  reg.reg.name = spec.name.chars ();
  reg.reg.string = spec.init.chars ();
  reg.reg.desc = spec.info.chars ();
  reg.name = spec.name;
  reg.missing = spec.missing;
  reg.init = spec.init;
  reg.info = spec.info;
  reg.bounded = spec.bounded;

  if (!spec.regval.empty ()) {
    reg.regval = spec.regval;
  }

  if (spec.bounded) {
    reg.min = spec.min;
    reg.max = spec.max;
    reg.initial = spec.init.as<float> ();
  }
  int eflags = FCVAR_EXTDLL;

  if (spec.type == Var::Normal) {
    eflags |= FCVAR_SERVER;
  }
  else if (spec.type == Var::ReadOnly) {
    eflags |= FCVAR_SERVER | FCVAR_SPONLY | FCVAR_PRINTABLEONLY;
  }
  else if (spec.type == Var::Password) {
    eflags |= FCVAR_PROTECTED;
  }

  reg.reg.flags = eflags;
  reg.self = self;
  reg.type = spec.type;

  cvars_.push (ystl::move (reg));
}

void ConVar::Revert () {
  if (!ptr) {
    return;
  }
  const auto &cvars = game.GetCvars ();

  for (const auto &var : cvars) {
    if (var.name == ptr->name) {
      Set (var.init.chars ());
      break;
    }
  }
}

void ConVar::SetPrefix (ystl::StringRef name, Var type) {
  if (type == Var::GameRef) {
    name_ = name;
    return;
  }
  name_.assignf ("%s_%s", product.cmd_pri, name);
}

void Game::CheckCvarsBounds () {
  for (const auto &var : cvars_) {
    if (!var.self || !var.self->ptr) {
      continue;
    }

    // read only cvar is not changeable
    if (var.type == Var::ReadOnly && !var.init.empty ()) {
      if (var.init != var.self->As<ystl::StringRef> ()) {
        var.self->Set (var.init.chars ());
      }
      continue;
    }

    if (!var.bounded || !var.self) {
      continue;
    }
    auto value = var.self->As<float> ();
    const auto str = var.self->As<ystl::StringRef> ();

    // check the bounds and set default if out of bounds
    if (value > var.max || value < var.min || (!str.empty () && isalpha (str[0]))) {
      var.self->Set (var.initial);

      // notify about that
      ctrl.Msg ("Bogus value for cvar '%s', min is '%.1f' and max is '%.1f', and we're got '%s', value reverted to default '%.1f'.", var.name,
        var.min, var.max, str, var.initial);
      continue;
    }

    /// prevent min/max problems
    const auto max_pos = var.name.find ("_max");

    if (max_pos != ystl::StringRef::InvalidIndex) {
      // compare via non-owning views, so no temporary strings get constructed per cvar
      const auto var_view = ystl::StringRef (var.name.chars (), var.name.size ());

      for (auto &mv : cvars_) {
        // match the "_min" twin cvar without building a temporary string
        if (mv.name.size () != var.name.size ()) {
          continue;
        }
        const auto min_pos = mv.name.find ("_min");

        if (min_pos != max_pos) {
          continue;
        }
        const auto mv_view = ystl::StringRef (mv.name.chars (), mv.name.size ());

        if (mv_view.substr (0, max_pos) == var_view.substr (0, max_pos) && mv_view.substr (max_pos + 4) == var_view.substr (max_pos + 4)) {
          const auto min_value = mv.self->As<float> ();

          if (min_value > value) {
            var.self->Set (min_value);
            mv.self->Set (value);

            // notify about that
            ctrl.Msg ("Bogus value for min/max cvar '%s' can't be higher than '%s'. Values swapped.", mv.name, var.name);
          }
        }
      }
    }
  }

  // force simulating on xash3d so startframe runs without players
  if (Is (GameFlags::Xash3DLegacy)) {
    static ConVarRef sv_forcesimulating ("sv_forcesimulating");

    if (sv_forcesimulating.Exists () && !ystl::fequal (sv_forcesimulating.Value (), 1.0f)) {
      Print ("Force-enable Xash3D sv_forcesimulating cvar.");
      sv_forcesimulating.Set ("1.0");
    }
  }
}

void Game::SetCvarDescription (const ConVar &cv, ystl::StringRef info) {
  for (auto &var : cvars_) {
    if (var.name == cv.Name ()) {
      var.info = info;
      break;
    }
  }
}

void Game::RegisterCvars (bool game_vars) {
  // this function pushes all added global variables to engine registration

  for (auto &var : cvars_) {
    ConVar &self = *var.self;
    ConVarEngineReg &reg = var.reg;

    if (var.type != Var::GameRef) {
      if (var.type == Var::Xash3D && !Is (GameFlags::Xash3D)) {
        continue;
      }

      if (!self.ptr) {
        // xash3d extended cvar marker, sized against the pointer width
        constexpr uintptr_t kSentinel = sizeof (void *) == 8 ? 0xDEADBEEFDEADBEEFULL : 0xDEADBEEFu;

        // xash3d picks the description up when next holds the sentinel
        if (Is (GameFlags::Xash3D) && !ystl::strings.is_empty (reg.desc)) {
          reg.next = reinterpret_cast<void *> (kSentinel);
        }

        // fix metamod' memlocs not found
        if (Is (GameFlags::Metamod)) {
          static ConVarEngineReg metamod_rg {};
          metamod_rg = var.reg;

          engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&metamod_rg));
        }
        else {
          engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&var.reg));
        }
      }
      self.ptr = engfuncs.pfnCVarGetPointer (reg.name);
    }
    else if (game_vars) {
      self.ptr = engfuncs.pfnCVarGetPointer (reg.name);

      if (var.missing && !self.ptr) {
        if (var.init.empty () && !var.regval.empty ()) {
          reg.string = var.regval.chars ();
          reg.flags |= FCVAR_SERVER;
        }
        engfuncs.pfnCVarRegister (reinterpret_cast<cvar_t *> (&var.reg));
        self.ptr = engfuncs.pfnCVarGetPointer (reg.name);
      }

      if (!self.ptr) {
        ystl::logger.error ("Got nullptr on cvar %s!", reg.name);
      }
    }
  }
}

void Game::ConstructCsBinaryName (ystl::SmallArray<ystl::String> &libs) {
  // platform -> binary suffix; checked top-to-bottom, first match wins
  struct Rule {
    ystl::StringRef suffix;
    bool (*match) ();
  };

  // clang-format off
   static constexpr Rule kCsBinaryRules[] = {
      { "_android_arm64",  [] { return ystl::plat.android && ystl::plat.x64; } },
      { "_android_armv7l", [] { return ystl::plat.android && ystl::plat.arm; } },
      { "_psvita",         [] { return ystl::plat.psvita; } },
      { "_arm64",          [] { return ystl::plat.x64 && ystl::plat.arm; } },
      { "_ppc64le",        [] { return ystl::plat.x64 && ystl::plat.ppc; } },
      { "_riscv64d",       [] { return ystl::plat.x64 && ystl::plat.riscv; } },
      { "_amd64",          [] { return ystl::plat.x64; } },
      { "_armv7hf",        [] { return ystl::plat.arm; } },
      { "_i386",           [] { return !ystl::plat.nix && !ystl::plat.win && !ystl::plat.macos; } },
   };
  // clang-format on

  ystl::String suffix {};
  for (const Rule &rule : kCsBinaryRules) {
    if (rule.match ()) {
      suffix = rule.suffix;
      break;
    }
  }

  if (ystl::plat.android) {
    // only "libcs" with suffix (no "mp", and must have "lib" prefix)
    libs.push ("libcs" + suffix);
  }
  else {
    // standard: "cs" and "mp" with suffix
    libs.push ("cs" + suffix);
    libs.push ("mp" + suffix);
  }
}

bool Game::LoadCsBinary () {
  ystl::StringRef modname = GetRunningModName ();

  if (modname.empty ()) {
    return false;
  }

  ystl::SmallArray<ystl::String> libs {};
  ConstructCsBinaryName (libs);

  auto lib_check = [&] (ystl::StringRef mod, ystl::StringRef dll) {
    // try to load gamedll
    if (!game_lib_) {
      ystl::logger.fatal ("Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", dll, mod);
    }
    auto ent = game_lib_.resolve<EntityProto> ("trigger_random_unique");

    // detect regamedll by addon entity they provide
    if (ent != nullptr) {
      game_flags_ |= GameFlags::ReGameDLL;
    }
    return true;
  };

  // search the libraries inside game dlls directory
  for (const auto &lib : libs) {
    ystl::String path {};

    if (ystl::plat.android) {
      // this will be removed as soon as mod downloader will be implemented on engine side
      auto gamelibdir = ystl::plat.env ("XASH3D_GAMELIBDIR");
      path = ystl::strings.join_path (gamelibdir, lib) + kLibrarySuffix;

      // if we can't read file, skip it
      if (!ystl::plat.file_exists (path.chars ())) {
        path = "";
      }
    }

    if (ystl::plat.emscripten) {
      path = ystl::String (ystl::plat.env ("XASH3D_GAMELIBPATH")); // defined by launcher
    }

    if (path.empty ()) {
      path = ystl::strings.join_path (modname, "dlls", lib) + kLibrarySuffix;

      // if we can't read file, skip it
      if (!ystl::plat.file_exists (path.chars ())) {
        continue;
      }
    }

    // enable fake pings on supported platforms only
    auto enable_fake_pings = [&] () {
      if (!has_flag (game_flags_, GameFlags::Xash3D | GameFlags::Xash3DLegacy)) {
        game_flags_ |= GameFlags::HasFakePings;
      }
    };

    // special case, czero is always detected first, as it's has custom directory
    if (modname == "czero") {
      game_flags_ |= (GameFlags::ConditionZero | GameFlags::HasBotVoice);

      // no fake pings on xash3d
      enable_fake_pings ();

      if (Is (GameFlags::Metamod)) {
        return false;
      }
      game_lib_.load (path);

      // verify dll is ok
      return lib_check (modname, lib);
    }
    else {
      game_lib_.load (path);

      // verify dll is ok
      if (!lib_check (modname, lib)) {
        return false;
      }

      // detect if we're running modern game
      auto entity = game_lib_.resolve<EntityProto> ("weapon_famas");

      // detect xash engine
      if (Is (GameFlags::Xash3D)) {
        game_flags_ |= GameFlags::Modern;

        if (entity != nullptr) {
          game_flags_ |= GameFlags::HasBotVoice;
        }

        if (Is (GameFlags::Metamod)) {
          return false;
        }
        return true;
      }

      if (entity != nullptr) {
        game_flags_ |= (GameFlags::Modern | GameFlags::HasBotVoice);

        // no fake pings on xash3d
        enable_fake_pings ();
      }
      else {
        game_flags_ |= GameFlags::Legacy;

        // clear modern flag just in case
        game_flags_ &= ~GameFlags::Modern;
      }

      // allow to enable hitbox-based aiming on fresh games
      if (Is (GameFlags::Modern)) {
        game_flags_ |= GameFlags::HasStudioModels;
      }

      if (Is (GameFlags::Metamod)) {
        return false;
      }
      return true;
    }
  }
  return false;
}

bool Game::Postload () {
  bstor.CheckInstallLocation (); // check if installed just as in manual

  // register logger
  ystl::logger.initialize (bstor.BuildPath (StorageFile::LogFile), [] (const char *msg) {
    // log lines are runtime data, not translatable language keys, so they bypass the translation
    game.SendServerMessage (ystl::strings.format ("%s\n", msg));
  });

  auto ensure_bot_path_exists = [] (ystl::StringRef dir1, ystl::StringRef dir2) {
    ystl::File::make_path (ystl::strings.join_path (bstor.GetRunningPath (), dir1, dir2).chars ());
  };

  // ensure we're have all needed directories
  ensure_bot_path_exists (folders.config, folders.lang);
  ensure_bot_path_exists (folders.data, folders.train);
  ensure_bot_path_exists (folders.data, folders.graph);
  ensure_bot_path_exists (folders.data, folders.logs);
  ensure_bot_path_exists (folders.data, folders.podbot);

  // set out user agent for http stuff
  ystl::http.set_user_agent (ystl::strings.format ("%s/%s", product.name, product.version));

#if defined(YSTL_WITH_TLS)
  // tls certificate bundle shipped with the package, without it https is unverified
  if (!ystl::http.set_ca_file (ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, folders.extra, "cacert.pem"))) {
    ystl::logger.error ("https ca bundle is missing, tls connections will be unverified");
  }
#endif

  // set the app name
  ystl::plat.set_app_name (product.name.chars ());

  DetectXashEngine ();

  // register bot cvars
  RegisterCvars ();

  // register bot commands
  ctrl.RegisterCommands ();

  // handle prefixes
  constexpr ystl::FixedArray<ystl::StringRef, 2> prefixes { product.cmd_pri, product.cmd_sec };

  // register all our handlers
  for (const auto &prefix : prefixes) {
    RegisterEngineCommand (prefix.chars (), [] () {
      ctrl.HandleEngineCommands ();
    });
  }

  // register fake metamod command handler if we not! under mm
  if (!Is (GameFlags::Metamod)) {
    RegisterEngineCommand ("meta", [] () {
      game.Print ("You're launched standalone version of %s. Metamod is not installed or not enabled!", product.name);
    });
  }

  // is 25th anniversary
  if (Is25thAnniversaryUpdate ()) {
    game_flags_ |= GameFlags::AnniversaryHL25;
  }

  // is goldclient engine
  if (IsGoldClientListenServer ()) {
    game_flags_ |= GameFlags::GoldClient;
  }

  // initialize weapons
  conf.InitWeapons ();

  // set custom cvar descriptions (needs the weapon table initialized)
  util.SetCustomCvarDescriptions ();

  // register engine lib handle
  engine_lib_.locate (reinterpret_cast<void *> (engfuncs.pfnPrecacheModel));

  // probe rehlds fast path for dropping clients
  if (rehlds.Probe ()) {
    game_flags_ |= GameFlags::ReHLDS;
  }

  if (ystl::plat.android || ystl::plat.emscripten) {
    game_flags_ |= (GameFlags::Xash3D | GameFlags::Mobility | GameFlags::HasBotVoice | GameFlags::ReGameDLL);

    if (Is (GameFlags::Metamod)) {
      return true; // we should stop the attempt for loading the real gamedll, since metamod handle this for us
    }
  }
  const bool binary_loaded = LoadCsBinary ();

  if (!binary_loaded && !Is (GameFlags::Metamod)) {
    ystl::StringRef modname = GetRunningModName ();
    ystl::SmallArray<ystl::String> libs {};

    ConstructCsBinaryName (libs);

    // append library suffix to each library name
    for (auto &lib : libs) {
      lib += kLibrarySuffix;
    }
    auto tried_libs = ystl::String::join (libs, ", ");

    ystl::logger.fatal ("Failed to load game library for mod '%s'. Tried: [%s]. Supported mods: czero, cstrike, valve. Get help: %s or email %s",
      modname.chars (), tried_libs.chars (), product.url, product.email);
  }

  if (Is (GameFlags::Metamod)) {
    game_lib_.unload ();
    return true;
  }

  return false;
}

void Game::ApplyGameModes () {
  if (!Is (GameFlags::Metamod | GameFlags::ReGameDLL)) {
    return;
  }

  // handle cvar cases
  switch (cv_csdm_mode.As<int> ()) {
  default:
  case 0:
    break;

    // force csdm mode
  case 1:
    game_flags_ |= GameFlags::CSDM;
    game_flags_ &= ~GameFlags::FreeForAll;
    return;

    // force csdm ffa mode
  case 2:
    game_flags_ |= GameFlags::CSDM | GameFlags::FreeForAll;
    return;

    // force disable everything
  case 3:
    game_flags_ &= ~(GameFlags::CSDM | GameFlags::FreeForAll);
    return;
  }

  static ystl::StringRef csdm_active_cvar_name = conf.FetchCustom ("CSDMDetectCvar");
  static ystl::StringRef zm_active_cvar_name = conf.FetchCustom ("ZMDetectCvar");
  static ystl::StringRef zm_delay_cvar_name = conf.FetchCustom ("ZMDelayCvar");

  static ConVarRef csdm_active (csdm_active_cvar_name);
  static ConVarRef csdm_version ("csdm_version");
  static ConVarRef redm_active ("redm_active");
  static ConVarRef mp_freeforall ("mp_freeforall");

  // csdm is only with amxx and metamod
  if (csdm_active.Exists () || redm_active.Exists () || csdm_version.Exists ()) {
    if (csdm_active.Value () > 0.0f || redm_active.Value () > 0.0f) {
      game_flags_ |= GameFlags::CSDM;
    }
    else if (Is (GameFlags::CSDM)) {
      game_flags_ &= ~GameFlags::CSDM;
    }
  }

  // but this can be provided by regamedll
  if (mp_freeforall.Exists ()) {
    if (mp_freeforall.Value () > 0.0f) {
      game_flags_ |= (GameFlags::FreeForAll | GameFlags::CSDM);
    }
    else if (Is (GameFlags::FreeForAll)) {
      game_flags_ &= ~(GameFlags::FreeForAll | GameFlags::CSDM);
    }
  }

  // does zombie mod is in use
  static ystl::SmallArray<ystl::String> zm_detects {};

  // initialize cvars to check, as different zombie mods are providing different cvars
  if (zm_detects.empty ()) {
    for (const auto &name : zm_active_cvar_name.split (",")) {
      if (!name.empty () && name.find_first_not_of (" \t") != ystl::StringRef::InvalidIndex) {
        zm_detects.emplace (ystl::String (name).trim ());
      }
    }
  }

  static float zm_scanned_at = -1.0f;
  static bool zm_active = false;

  const float round_start = game_state.GetRoundStartTime ();

  if (!ystl::fequal (zm_scanned_at, round_start)) {
    zm_scanned_at = round_start;
    zm_active = false;

    for (const auto &name : zm_detects) {
      if (engfuncs.pfnCVarGetPointer (name.chars ())) {
        zm_active = true;
        break;
      }
    }
  }

  // do a some little support for zombie plague
  if (zm_active) {
    static ConVarRef zm_delay (zm_delay_cvar_name);

    // update our ignore timer if zp_delay exists
    if (zm_delay.Exists () && zm_delay.Value () > 0.0f) {
      cv_ignore_enemies_after_spawn_time.Set (zm_delay.Value () + 1.5f);
    }
    game_flags_ |= GameFlags::ZombieMod;
  }
  else {
    game_flags_ &= ~GameFlags::ZombieMod;
  }
}

void Game::SlowFrame () {
  const auto next_update = ystl::clamp (75.0f * globals->frametime, 0.5f, 1.0f);

  // run something that is should run more
  if (half_second_timer_.elapsed ()) {

    // refresh bomb origin in case some plugin moved it out
    game_state.SetBombOrigin ();

    // rescan mode 2x2 walls
    mode_walls.Frame ();

    // ensure the server admin is confident about features he's using
    EnsureHealthyGameEnvironment ();

    // maintain round restart for first human join
    bots.MaintainRoundRestart ();

    // update next update time
    half_second_timer_.start (next_update * 0.25f);
  }

  if (!one_second_timer_.elapsed ()) {
    return;
  }
  ctrl.MaintainAdminRights ();

  // update bot difficulties to newly selected from cvar
  bots.UpdateBotDifficulties ();

  // check if we're need to autokill bots
  bots.MaintainAutoKill ();

  // maintain leaders selection upon round start
  bots.MaintainLeaders ();

  // initialize light levels
  graph.InitLightLevels ();

  // initialize corridors
  graph.InitNarrowPlaces ();

  // detect csdm
  ApplyGameModes ();

  // check the cvar bounds
  CheckCvarsBounds ();

  // display welcome message
  util.CheckWelcome ();

  // kick failed bots
  bots.CheckNeedsToBeKicked ();

  // refresh bot infection (creature) status
  bots.RefreshCreatureStatus ();

  // update client pings
  fakeping.Calculate ();

  // update next update time
  one_second_timer_.start (next_update);
}

void Game::Frame () {
  // update lightstyle animations
  illum.AnimateLight ();

  if (graph.HasEditFlag (GraphEdit::On) && graph.HasEditor ()) {
    graph.Frame ();
  }

  // update analyzer if needed
  analyzer.Update ();

  // run stuff periodically
  SlowFrame ();

  // rebuild vistable if needed
  vistab.Rebuild ();

  if (bots.HasBotsOnline ()) {
    // keep track of grenades on map
    game_state.UpdateActiveGrenade ();

    // keep track of interesting entities
    game_state.UpdateInterestingEntities ();
  }

  // keep bot number up to date
  bots.MaintainQuota ();

  // balance bot difficulties
  bots.BalanceBotDifficulties ();

  // flush print queue to users
  ctrl.FlushPrintQueue ();

  // on metamod skip the game dll call, it runs through the metamod chain instead,
  // and the bot ai below runs in the post hook (GetEntityApiPost)
  if (Is (GameFlags::Metamod)) {
    RETURN_META (MRES_IGNORED);
  }
  ::dllapi.pfnStartFrame ();

  // run the bot ai
  bots.Frame ();

  // refresh clients after bot ai, so bot input is fresh for the noise simulation
  clients.Update ();
}

void Game::SearchEntities (ystl::StringRef field, ystl::StringRef value, EntitySearch functor) {
  edict_t *ent = nullptr;

  while (!IsNullEntity (ent = engfuncs.pfnFindEntityByString (ent, field.chars (), value.chars ()))) {
    if (ent->v.flags & FL_CLIENT) {
      continue;
    }

    if (functor (ent) == EntitySearchResult::Break) {
      break;
    }
  }
}

void Game::SearchEntities (const ystl::Vector &position, float radius, EntitySearch functor) const {
  edict_t *ent = nullptr;

  if (!start_entity_ && position.empty ()) {
    return;
  }
  const ystl::Vector &pos = position.empty () ? start_entity_->v.origin : position;

  while (!IsNullEntity (ent = engfuncs.pfnFindEntityInSphere (ent, pos, radius))) {
    if (ent->v.flags & FL_CLIENT) {
      continue;
    }

    if (functor (ent) == EntitySearchResult::Break) {
      break;
    }
  }
}

bool Game::HasEntityInGame (ystl::StringRef classname) const {
  return !IsNullEntity (engfuncs.pfnFindEntityByString (nullptr, "classname", classname.chars ()));
}

void Game::PrintBotVersion () const {
  ystl::String game_version_str {};
  ystl::Array<ystl::String> bot_runtime_flags {};

  // base game version (legacy builds report as "1.6 limited" under xash3d)
  if (Is (GameFlags::Legacy)) {
    game_version_str.assign (Is (GameFlags::Xash3D) ? "1.6 Limited" : "Legacy");
  }
  else if (Is (GameFlags::ConditionZero)) {
    game_version_str.assign ("Condition Zero");
  }
  else if (Is (GameFlags::Modern)) {
    game_version_str.assign ("v1.6");
  }

  if (Is (GameFlags::Xash3D)) {
    game_version_str.append (Is (GameFlags::Xash3DLegacy) ? " @ Xash3D-NG" : " @ Xash3D FWGS");

    if (Is (GameFlags::Mobility)) {
      game_version_str.append (" Mobile");
    }
  }

  // collect runtime flags
  struct FlagLabel {
    GameFlags flag;
    ystl::StringRef label;
  };

  static constexpr FlagLabel kRuntimeFlags[] = {
    { GameFlags::HasBotVoice,     "BotVoice"   },
    { GameFlags::ReGameDLL,       "ReGameDLL"  },
    { GameFlags::ReHLDS,          "ReHLDS"     },
    { GameFlags::HasFakePings,    "FakePing"   },
    { GameFlags::Metamod,         "Metamod"    },
    { GameFlags::AnniversaryHL25, "HL25"       },
    { GameFlags::GoldClient,      "GoldClient" },
  };
  for (const FlagLabel &entry : kRuntimeFlags) {
    if (Is (entry.flag)) {
      bot_runtime_flags.push (entry.label);
    }
  }

  // print if we're using sse 4.x / neon / rvv instructions
  if (ystl::plat.simd && (ystl::cpuflags.sse41 || ystl::cpuflags.sse42 || ystl::cpuflags.neon || ystl::cpuflags.rvv)) {
    struct SimdLevel {
      bool enabled;
      ystl::StringRef label;
    };
    const SimdLevel k_simd_levels[] = {
      { ystl::cpuflags.sse41, "4.1"  },
      { ystl::cpuflags.sse42, "4.2"  },
      { ystl::cpuflags.neon,  "Neon" },
      { ystl::cpuflags.rvv,   "RVV"  },
    };
    ystl::Array<ystl::String> simd_levels {};

    for (const SimdLevel &level : k_simd_levels) {
      if (level.enabled) {
        simd_levels.push (level.label);
      }
    }
    bot_runtime_flags.push (ystl::strings.format ("SIMD: %s", ystl::String::join (simd_levels, " & ")));
  }

  if (bot_runtime_flags.empty ()) {
    bot_runtime_flags.push ("None");
  }

  ctrl.Msg ("\n%s v%s successfully loaded for game: Counter-Strike %s.\n\tFlags: %s.\n", product.name, product.version, game_version_str,
    ystl::String::join (bot_runtime_flags, ", "));
}

void Game::EnsureHealthyGameEnvironment () {
  const bool dedicated = IsDedicatedServer ();

  if (!dedicated || Is (GameFlags::Legacy | GameFlags::Xash3D)) {
    if (!dedicated) {

      // force enable pings on listen servers if disabled at all
      if (Is (GameFlags::Modern) && cv_show_latency.As<int> () == 0) {
        cv_show_latency.Set (2);
      }
    }
    return; // listen servers doesn't care about it at all
  }

  // magic string that's enables the features
  constexpr auto kAllowHash = ystl::StringRef::fnv1a32 ("i'm confident for what i'm doing");
  constexpr auto kAllowHash2 = ystl::StringRef::fnv1a32 ("\"i'm confident for what i'm doing\"");

  // fetch custom variable, so fake features are explicitly enabled
  static auto enable_fake_features = ystl::StringRef::fnv1a32 (conf.FetchCustom ("EnableFakeBotFeatures").chars ());

  // if string matches, do not affect the cvars
  if (enable_fake_features == kAllowHash || enable_fake_features == kAllowHash2) {
    return;
  }

  auto notify_peaceful_revert = [] (const ConVar &cv) {
    game.Print ("Cvar \"%s\" reverted to peaceful value.", cv.Name ());
  };

  // disable fake latency
  if (cv_show_latency.As<int> () > 1) {
    cv_show_latency.Set (0);

    notify_peaceful_revert (cv_show_latency);
  }

  // disable fake avatars
  if (cv_show_avatars) {
    cv_show_avatars.Set (0);

    notify_peaceful_revert (cv_show_avatars);
  }

  // disable fake queries
  if (cv_enable_query_hook) {
    cv_enable_query_hook.Set (0);

    notify_peaceful_revert (cv_enable_query_hook);
  }
}

edict_t *Game::CreateFakeClient (ystl::StringRef name) {
  auto ent = engfuncs.pfnCreateFakeClient (name.chars ());

  if (IsNullEntity (ent)) {
    return nullptr;
  }
  auto netname = ent->v.netname;
  ent->v = {}; // reset entire the entvars structure (fix from regamedll)

  // restore containing entity, name and client flags
  ent->v.pContainingEntity = ent;
  ent->v.flags = FL_FAKECLIENT | FL_CLIENT;
  ent->v.netname = netname;

  if (ent->pvPrivateData != nullptr) {
    engfuncs.pfnFreeEntPrivateData (ent);
  }
  ent->pvPrivateData = nullptr;

  return ent;
}

void Game::MarkBreakableAsInvalid (edict_t *ent) {
  checked_breakables_.erase (IndexOfEntity (ent));
}

bool Game::IsDeveloperMode () const {
  static ConVarRef developer { "developer" };

  return developer.Exists () && developer.Value () > 0.0f;
}

bool Game::IsAliveEntity (edict_t *ent) const {
  if (IsNullEntity (ent)) {
    return false;
  }
  return ent->v.deadflag == DEAD_NO && ent->v.health > 0.0f && ent->v.movetype != MOVETYPE_NOCLIP;
}

bool Game::IsPlayerEntity (edict_t *ent) const {
  if (IsNullEntity (ent)) {
    return false;
  }

  if (ent->v.flags & FL_PROXY) {
    return false;
  }

  if ((ent->v.flags & (FL_CLIENT | FL_FAKECLIENT)) || bots[ent] != nullptr) {
    return !ystl::strings.is_empty (ent->v.netname.chars ());
  }
  return false;
}

bool Game::IsBombEntity (edict_t *ent) const {
  if (!game.MapIs (MapFlags::Demolition) || IsNullEntity (ent)) {
    return false;
  }
  const auto classname = ent->v.classname.str ();
  const auto model = ent->v.model.str (9);

  return (classname.starts_with ("grenade") && conf.GetBombModelName () == model) ||
         (classname.starts_with ("weaponbox") && model == "backpack.mdl");
}

bool Game::IsMonsterEntity (edict_t *ent) const {
  if (IsNullEntity (ent)) {
    return false;
  }

  if (~ent->v.flags & FL_MONSTER) {
    return false;
  }

  if (IsHostageEntity (ent)) {
    return false;
  }
  return true;
}

bool Game::IsItemEntity (edict_t *ent) const {
  return !IsNullEntity (ent) && ent->v.classname.str ().contains ("item_");
}

bool Game::IsPlayerVip (edict_t *ent) const {
  if (!MapIs (MapFlags::Assassination)) {
    return false;
  }

  if (!IsPlayerEntity (ent)) {
    return false;
  }
  return *(engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), "model")) == 'v';
}

bool Game::IsDoorEntity (edict_t *ent) const {
  if (IsNullEntity (ent)) {
    return false;
  }
  const auto class_hash = ent->v.classname.str ().hash ();

  constexpr auto kFuncDoor = ystl::StringRef::fnv1a32 ("func_door");
  constexpr auto kFuncDoorRotating = ystl::StringRef::fnv1a32 ("func_door_rotating");

  return class_hash == kFuncDoor || class_hash == kFuncDoorRotating;
}

bool Game::IsHostageEntity (edict_t *ent) const {
  if (IsNullEntity (ent)) {
    return false;
  }
  const auto class_hash = ent->v.classname.str ().hash ();

  constexpr auto kHostageEntity = ystl::StringRef::fnv1a32 ("hostage_entity");
  constexpr auto kMonsterScientist = ystl::StringRef::fnv1a32 ("monster_scientist");

  return class_hash == kHostageEntity || class_hash == kMonsterScientist;
}

bool Game::IsBreakableEntity (edict_t *ent, bool initial_seed) const {
  if (!initial_seed) {
    if (!HasBreakables ()) {
      return false;
    }
  }

  if (IsNullEntity (ent) || ent == GetStartEntity () || (!initial_seed && !game.IsBreakableValid (ent))) {
    return false;
  }
  const auto limit = cv_breakable_health_limit.As<float> ();

  // not shoot-able
  if (ent->v.health < 1.0f || ent->v.health >= limit) {
    return false;
  }
  constexpr auto kFuncBreakable = ystl::StringRef::fnv1a32 ("func_breakable");
  constexpr auto kFuncPushable = ystl::StringRef::fnv1a32 ("func_pushable");
  constexpr auto kFuncWall = ystl::StringRef::fnv1a32 ("func_wall");

  if (ent->v.takedamage > 0.0f && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY)) {
    const auto class_hash = ent->v.classname.str ().hash ();

    if (class_hash == kFuncBreakable || (class_hash == kFuncPushable && (ent->v.spawnflags & SF_PUSH_BREAKABLE)) || class_hash == kFuncWall) {
      return ent->v.movetype == MOVETYPE_PUSH || ent->v.movetype == MOVETYPE_PUSHSTEP;
    }
  }
  return false;
}

bool Game::HasBreakables () const {
  return game_state.HasBreakables ();
}

bool Game::IsFakeClientEntity (edict_t *ent) const {
  return bots[ent] != nullptr || (!IsNullEntity (ent) && (ent->v.flags & FL_FAKECLIENT));
}

bool Game::IsEntityModelMatches (const edict_t *ent, ystl::StringRef model) const {
  return model.starts_with (ent->v.model.chars (9));
}

void LightMeasure::InitializeLightstyles () {
  // this function initializes lighting information

  // reset all light styles
  for (auto &ls : lightstyle_) {
    ls.length = 0;
    ls.map[0] = ystl::kNullChar;
  }

  ystl::fill (lightstyle_value_, 264);
}

void LightMeasure::AnimateLight () {
  // this function performs light animations

  if (!do_animation_) {
    return;
  }

  // 'm' is normal light, 'a' is no light, 'z' is double bright
  const auto index = static_cast<int> (game.Time () * 10.0f);

  for (auto j = 0; j < MAX_LIGHTSTYLES; ++j) {
    if (!lightstyle_[j].length) {
      lightstyle_value_[j] = MAX_LIGHTSTYLEVALUE;
      continue;
    }
    lightstyle_value_[j] = static_cast<uint32_t> (lightstyle_[j].map[index % lightstyle_[j].length] - 'a') * 22u;
  }
}

void LightMeasure::UpdateLight (int style, char *value) {
  if (!do_animation_) {
    return;
  }

  if (style >= MAX_LIGHTSTYLES) {
    return;
  }

  if (ystl::strings.is_empty (value)) {
    lightstyle_[style].length = 0u;
    lightstyle_[style].map[0] = ystl::kNullChar;

    return;
  }
  const auto copy_limit = sizeof (lightstyle_[style].map) - sizeof (ystl::kNullChar);
  ystl::strings.copy (lightstyle_[style].map, value, copy_limit);

  lightstyle_[style].map[copy_limit] = ystl::kNullChar;
  lightstyle_[style].length = static_cast<int> (strlen (lightstyle_[style].map));
}

template <typename S, typename M> bool LightMeasure::RecursiveLightPoint (const M *node, const ystl::Vector &start, const ystl::Vector &end) {
  if (!node || node->contents < 0) {
    return false;
  }

  // determine which side of the node plane our points are on, fixme: optimize for axial
  const auto plane = node->plane;

  const float front = (start | plane->normal) - plane->dist;
  const float back = (end | plane->normal) - plane->dist;

  const int side = front < 0.0f;

  // if they're both on the same side of the plane, don't bother to split just check the appropriate child
  if ((back < 0.0f) == side) {
    return RecursiveLightPoint<S, M> (reinterpret_cast<M *> (node->children[side]), start, end);
  }

  // calculate mid point
  const float frac = front / (front - back);
  auto mid = start + (end - start) * frac;

  // go down front side
  if (RecursiveLightPoint<S, M> (reinterpret_cast<M *> (node->children[side]), start, mid)) {
    return true; // hit something
  }

  // blow it off if it doesn't split the plane
  if ((back < 0.0f) == !!side) {
    return false; // didn't hit anything
  }

  // check for impact on this node lightspot = mid; lightplane = plane;
  auto surf = reinterpret_cast<S *> (world_model_->surfaces) + node->firstsurface;

  for (int i = 0; i < node->numsurfaces; ++i, ++surf) {
    if (surf->flags & SURF_DRAWTILED) {
      continue; // no lightmaps
    }
    const auto tex = surf->texinfo;

    // see where in lightmap space our intersection point is
    const int s = static_cast<int> ((mid | ystl::Vector (tex->vecs[0])) + tex->vecs[0][3]);
    const int t = static_cast<int> ((mid | ystl::Vector (tex->vecs[1])) + tex->vecs[1][3]);

    // not in the bounds of our lightmap? punt
    if (s < surf->texturemins[0] || t < surf->texturemins[1]) {
      continue;
    }

    // fixme: assume square lightmap and punt if outside rectangle
    int ds = s - surf->texturemins[0];
    int dt = t - surf->texturemins[1];

    if (ds > surf->extents[0] || dt > surf->extents[1]) {
      continue;
    }

    if (!surf->samples) {
      return true;
    }
    ds >>= 4;
    dt >>= 4;

    point_.reset (); // reset point color

    const int smax = (surf->extents[0] >> 4) + 1;
    const int tmax = (surf->extents[1] >> 4) + 1;
    const int size = smax * tmax;

    auto lightmap = surf->samples + dt * smax + ds;

    // compute the lightmap color at a particular point
    for (int maps = 0; maps < MAX_LIGHTMAPS && surf->styles[maps] != 255; ++maps) {
      const auto scale = static_cast<int32_t> (lightstyle_value_[surf->styles[maps]]);

      point_.red += lightmap->r * scale;
      point_.green += lightmap->g * scale;
      point_.blue += lightmap->b * scale;

      lightmap += size; // skip to next lightmap
    }
    point_.red >>= 8u;
    point_.green >>= 8u;
    point_.blue >>= 8u;

    return true;
  }
  return RecursiveLightPoint<S, M> (reinterpret_cast<M *> (node->children[!side]), mid, end); // go down back side
}

template <typename S, typename M> bool LightMeasure::LightPointProc (LightMeasure *self, const ystl::Vector &start, const ystl::Vector &end) {
  return self->RecursiveLightPoint<S, M> (reinterpret_cast<M *> (self->world_model_->nodes), start, end);
}

float LightMeasure::GetLightLevel (const ystl::Vector &point) {
  if (game.Is (GameFlags::Legacy)) {
    return kInvalidLightLevel;
  }

  if (!world_model_) {
    return kInvalidLightLevel;
  }

  if (!world_model_->lightdata) {
    return 255.0f;
  }

  ystl::Vector end_point (point);
  end_point.z -= 2048.0f;

  using LightProc = bool (*) (LightMeasure *, const ystl::Vector &, const ystl::Vector &);

  static const LightProc light_proc = [] () -> LightProc {
    if (game.IsSoftwareRenderer ()) {
      return LightPointProc<msurface_t, mnode_t>;
    }
    if (game.Is (GameFlags::AnniversaryHL25)) {
      return LightPointProc<msurface_hw_hl25_t, mnode_hw_t>;
    }
    if (game.Is (GameFlags::GoldClient)) {
      return LightPointProc<msurface_gc_t, mnode_hw_t>;
    }
    return LightPointProc<msurface_hw_t, mnode_hw_t>;
  }();

  if (!light_proc (this, point, end_point)) {
    return kInvalidLightLevel;
  }
  return 100.0f * ystl::sqrtf (ystl::min (75.0f, static_cast<float> (point_.avg ())) / 75.0f);
}

float LightMeasure::GetSkyColor () {
  return static_cast<float> (ystl::Color (sv_skycolor_r.As<int> (), sv_skycolor_g.As<int> (), sv_skycolor_b.As<int> ()).avg ());
}

ystl::Vector PlayerHitboxEnumerator::Get (edict_t *ent, PlayerPart part, float update_timestamp) {
  const auto index = game.IndexOfPlayer (ent);

  if (index < 0 || index >= kGameMaxPlayers) {
    return nullptr;
  }
  auto parts = &parts_[index];

  if (game.Time () > parts->updated) {
    Update (ent);
    parts->updated = game.Time () + update_timestamp;
  }

  switch (part) {
  default:
  case PlayerPart::Head:
    return parts->head;

  case PlayerPart::Stomach:
    return parts->stomach;

  case PlayerPart::LeftArm:
    return parts->left;

  case PlayerPart::RightArm:
    return parts->right;

  case PlayerPart::Feet:
    return parts->feet;

  case PlayerPart::RightLeg:
    return { parts->right.x, parts->right.y, parts->feet.z };

  case PlayerPart::LeftLeg:
    return { parts->left.x, parts->left.y, parts->feet.z };
  }
}

void PlayerHitboxEnumerator::Update (edict_t *ent) {
  if (!game.IsAliveEntity (ent)) {
    return;
  }
  // get info about player
  const auto index = game.IndexOfPlayer (ent);

  if (index < 0 || index >= kGameMaxPlayers) {
    return;
  }
  auto parts = &parts_[index];

  // set the feet without bones
  parts->feet = ent->v.origin;

  constexpr auto kStandFeet = 34.0f;
  constexpr auto kCrouchFeet = 14.0f;

  // legs position isn't calculated to reduce cpu usage, just use some universal feet spot
  if (ent->v.flags & FL_DUCKING) {
    parts->feet.z = ent->v.origin.z - kCrouchFeet;
  }
  else {
    parts->feet.z = ent->v.origin.z - kStandFeet;
  }

  auto get_hitbox = [&] (studiohdr_t *hdr, mstudiobbox_t *bb, PlayerPart part) {
    auto hitbox = PlayerPart::Invalid;

    for (auto i = 0; i < hdr->numhitboxes; ++i) {
      const auto set = &bb[i];

      if (set->group != ystl::to_underlying (part)) {
        continue;
      }
      hitbox = static_cast<PlayerPart> (i);
      break;
    }
    return hitbox;
  };
  auto model = engfuncs.pfnGetModelPtr (ent);
  auto studiohdr = reinterpret_cast<studiohdr_t *> (model);

  // this can be null ?
  if (model && studiohdr) {
    auto bboxset = reinterpret_cast<mstudiobbox_t *> (reinterpret_cast<uint8_t *> (studiohdr) + studiohdr->hitboxindex);

    // resolve a single body part into the corresponding bone origin
    auto resolve_part = [&] (PlayerPart part, ystl::Vector &dest) {
      const auto hitbox = get_hitbox (studiohdr, bboxset, part);

      if (hitbox != PlayerPart::Invalid) {
        engfuncs.pfnGetBonePosition (ent, bboxset[ystl::to_underlying (hitbox)].bone, dest, nullptr);
      }
    };

    // get the head
    const auto head_hitbox = get_hitbox (studiohdr, bboxset, PlayerPart::Head);

    if (head_hitbox != PlayerPart::Invalid) {
      const auto head_index = ystl::to_underlying (head_hitbox);

      engfuncs.pfnGetBonePosition (ent, bboxset[head_index].bone, parts->head, nullptr);

      parts->head.z += bboxset[head_index].bbmax.z;
      parts->head = { ent->v.origin.x, ent->v.origin.y, parts->head.z };
    }

    // get the body (stomach)
    resolve_part (PlayerPart::Stomach, parts->stomach);

    // get the left (arm)
    resolve_part (PlayerPart::LeftArm, parts->left);

    // get the right (arm)
    resolve_part (PlayerPart::RightArm, parts->right);
    return;
  }
  else {
    game.ClearGameFlag (GameFlags::HasStudioModels); // yes, only a single fail will disable this
  }

  parts->head = ent->v.origin + ent->v.view_ofs;
  parts->stomach = ent->v.origin;

  parts->left = parts->head;
  parts->right = parts->head;
}

void PlayerHitboxEnumerator::Reset () {
  for (auto &part : parts_) {
    part = {};
  }
}

void GameState::SetBombOrigin (bool reset, const ystl::Vector &pos) {
  // this function stores the bomb position as a vector

  if (!game.MapIs (MapFlags::Demolition) || !game_state.IsBombPlanted ()) {
    return;
  }

  if (reset) {
    bomb_origin_.clear ();
    bomb_entity_ = nullptr;
    SetBombPlanted (false);

    return;
  }

  if (!pos.empty ()) {
    bomb_origin_ = pos;
    bomb_entity_ = nullptr;
    return;
  }
  bool was_found = false;
  auto bomb_model = conf.GetBombModelName ();

  game.SearchEntities ("classname", "grenade", [&] (edict_t *ent) {
    if (game.IsEntityModelMatches (ent, bomb_model)) {
      bomb_origin_ = game.GetEntityOrigin (ent);
      bomb_entity_ = ent;

      was_found = true;

      return EntitySearchResult::Break;
    }
    return EntitySearchResult::Continue;
  });

  if (!was_found) {
    bomb_origin_.clear ();
    bomb_entity_ = nullptr;

    SetBombPlanted (false);
  }
}

void GameState::RoundStart () {
  round_over_ = false;
  time_bomb_planted_ = 0.0f;

  // tell the bots
  bots.InitRound ();
  SetBombOrigin (true);

  // calculate the round mid/end in world time
  time_round_start_ = game.Time () + mp_freezetime.As<float> ();
  time_round_mid_ = time_round_start_ + mp_roundtime.As<float> () * 60.0f * 0.5f;
  time_round_end_ = time_round_start_ + mp_roundtime.As<float> () * 60.0f;

  interesting_entities_.clear ();
  active_grenades_.clear ();

  active_grenades_update_time_.reset ();
  interesting_entities_update_time_.reset ();

  mode_walls.OnRoundStart ();

  sgtrack.Clear ();
}

bool GameState::IsEarlyRound (const float timestamp) const {
  return time_round_start_ + mp_freezetime.As<float> () + timestamp > game.Time ();
}

float GameState::GetBombTimeLeft () const {
  if (!bomb_planted_) {
    return 0.0f;
  }
  return ystl::max (time_bomb_planted_ + mp_c4timer.As<float> () - game.Time (), 0.0f);
}

void GameState::SetBombPlanted (bool is_planted) {
  if (cv_ignore_objectives) {
    bomb_planted_ = false;
    return;
  }

  if (is_planted) {
    time_bomb_planted_ = game.Time ();
  }
  bomb_planted_ = is_planted;
}

void GameState::UpdateActiveGrenade () {
  constexpr auto kUpdateTime = 0.25f;

  if (active_grenades_update_time_.less_than (kUpdateTime)) {
    return;
  }
  active_grenades_.clear (); // clear previously stored grenades

  // need to ignore bomb model in active grenades
  auto bomb_model = conf.GetBombModelName ();

  // search the map for any type of grenade
  game.SearchEntities ("classname", "grenade", [&] (edict_t *e) {
    // do not count c4 as a grenade
    if (!game.IsEntityModelMatches (e, bomb_model)) {
      const auto model = e->v.model.str (9);

      auto kind = GrenadeKind::Other;

      if (model == kFlashbangModelName) {
        kind = GrenadeKind::Flash;
      }
      else if (model == kExplosiveModelName) {
        kind = GrenadeKind::Explosive;
      }
      else if (model == kSmokeModelName) {
        kind = GrenadeKind::Smoke;
      }
      active_grenades_.push (ActiveGrenade { e, kind });
    }
    return EntitySearchResult::Continue; // continue iteration
  });
  active_grenades_update_time_.start ();
}

void GameState::UpdateInterestingEntities () {
  constexpr auto kUpdateTime = 0.5f;

  if (interesting_entities_update_time_.less_than (kUpdateTime)) {
    return;
  }
  interesting_entities_.clear (); // clear previously stored entities
  has_breakables_ = false;

  // search the map for interesting entities, kind is fixed here so consumers compare enums
  game.SearchEntities (nullptr, kInfiniteDistance, [&] (edict_t *e) {
    auto classname = e->v.classname.str ();

    if (classname.starts_with ("weaponbox") || classname.starts_with ("grenade") || classname.starts_with ("weapon_shield") ||
        game.IsItemEntity (e) || classname.starts_with ("armoury")) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Pickup });
    }

    // pickup some hostage if on cs_ maps
    else if (game.MapIs (MapFlags::HostageRescue) && game.IsHostageEntity (e)) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Hostage });
    }

    // add buttons
    else if (game.MapIs (MapFlags::HasButtons) && classname.starts_with ("func_button")) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Button });
    }

    // pickup some csdm stuff if we're running csdm
    else if (game.Is (GameFlags::CSDM) && classname.starts_with ("csdm")) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Csdm });
    }

    else if (cv_attack_monsters && game.IsMonsterEntity (e)) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Monster });
    }

    // breakables keep their own branch: unlike above they may duplicate a pickup entry,
    // and they qualify only when the validity map allows them (seeded at load or spawn hook)
    if (game.IsBreakableEntity (e, true) && game.IsBreakableValid (e)) {
      interesting_entities_.push (InterestingEntity { e, EntityKind::Breakable });
      has_breakables_ = true;
    }

    // continue iteration
    return EntitySearchResult::Continue;
  });
  interesting_entities_update_time_.start ();
}

bool Trace::IsCacheable (const Result &result) {
  return result.hit == nullptr || result.hit == game.GetStartEntity ();
}

Trace::CacheEntry *Trace::FindInCache (const CacheKey &key, float now) {
  CacheEntry *free = nullptr;
  CacheEntry *oldest = nullptr;
  uint32_t oldest_used = ystl::numeric_limits<uint32_t>::max ();

  for (auto &entry : cache_) {
    if (entry.valid && entry.key.Matches (key) && now - entry.timestamp < kCacheTTL) {
      entry.last_used = ++lru_counter_;
      return &entry;
    }

    if (!free && (!entry.valid || now - entry.timestamp >= kCacheTTL)) {
      free = &entry;
    }

    if (entry.last_used < oldest_used) {
      oldest_used = entry.last_used;
      oldest = &entry;
    }
  }
  return free ? free : oldest;
}

void Trace::StoreInCache (CacheEntry *entry, const CacheKey &key, const Result &result, float now) {
  if (entry) {
    entry->key = key;
    entry->result = result;
    entry->timestamp = now;
    entry->last_used = ++lru_counter_;
    entry->valid = true;
  }
}

void Trace::Line (const ystl::Vector &start, const ystl::Vector &end, int ignore_flags, edict_t *ignore_entity, Result *ptr) {
  CacheKey key { start.x, start.y, start.z, end.x, end.y, end.z, ignore_flags, -1, ignore_entity };

  const auto now = game.Time ();
  auto entry = FindInCache (key, now);

  if (entry && entry->valid && entry->key.Matches (key) && now - entry->timestamp < kCacheTTL && IsCacheable (entry->result)) {
    *ptr = entry->result;
    ++hits_;
    return;
  }
  ++misses_;

  auto engine_flags = 0;

  if (ignore_flags & TraceIgnore::Monsters) {
    engine_flags = 1;
  }

  if (ignore_flags & TraceIgnore::Glass) {
    engine_flags |= 0x100;
  }
  engfuncs.pfnTraceLine (start, end, engine_flags, ignore_entity, reinterpret_cast<TraceResult *> (ptr));

  if (IsCacheable (*ptr)) {
    StoreInCache (entry, key, *ptr, now);
  }
  else if (entry) {
    entry->valid = false;
  }
}

void Trace::Model (const ystl::Vector &start, const ystl::Vector &end, int hull_number, edict_t *ent_to_hit, Result *ptr) {
  // this function traces a line between start and end against the BSP model of a specific entity
  // (entToHit), using the specified hull size for the collision test. Unlike TraceLine and TraceHull,
  // which test against the entire world and all entities in the path, TraceModel only tests against
  // the BSP model of the single specified entity, ignoring everything else. The hullNumber parameter
  // specifies the hull type to use for the trace (point_hull, human_hull, head_hull, or large_hull)
  // This is useful for checking whether a specific entity (such as a door, breakable, or pushable)
  // would block movement between two points, without interference from walls or other entities
  // The results are returned in the TraceResult structure ptr, similar to TraceLine and TraceHull

  engfuncs.pfnTraceModel (start, end, hull_number, ent_to_hit, reinterpret_cast<TraceResult *> (ptr));
}

void Trace::Hull (const ystl::Vector &start, const ystl::Vector &end, int ignore_flags, int hull_number, edict_t *ignore_entity, Result *ptr) {
  CacheKey key { start.x, start.y, start.z, end.x, end.y, end.z, ignore_flags, static_cast<int16_t> (hull_number), ignore_entity };

  const auto now = game.Time ();
  auto entry = FindInCache (key, now);

  if (entry && entry->valid && entry->key.Matches (key) && now - entry->timestamp < kCacheTTL && IsCacheable (entry->result)) {
    *ptr = entry->result;
    ++hits_;
    return;
  }
  ++misses_;

  engfuncs.pfnTraceHull (
    start, end, !!(ignore_flags & TraceIgnore::Monsters), hull_number, ignore_entity, reinterpret_cast<TraceResult *> (ptr));

  if (IsCacheable (*ptr)) {
    StoreInCache (entry, key, *ptr, now);
  }
  else if (entry) {
    entry->valid = false;
  }
}

bool Trace::IsEndpointClear (const Result &result) const {
  // a completed trace (fraction >= 1.0) that did not start inside a solid is a clear line.
  // do not use inOpen/inWater here: on goldsrc/xash those mean "the trace start was in open
  // space / water", not "the endpoint is clear", so gating on them rejects valid sightlines
  return result.fraction >= 1.0f && !result.start_solid && !result.all_solid;
}

} // namespace bot
