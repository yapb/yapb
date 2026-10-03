//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// line draw
namespace bot {

enum class DrawLineType : int32_t {
  Simple,
  Arrow,
  Num
};

// variable type
enum class Var : int32_t {
  Normal = 0,
  ReadOnly,
  Password,
  NoServer,
  GameRef,
  Xash3D // registrable only on xash3d engine
};

// supported cs's
enum class GameFlags : int32_t {
  None = 0,
  Modern = ystl::bit (0), // counter-strike 1.6 and above
  Xash3D = ystl::bit (1), // counter-strike 1.6 under the xash engine (additional flag)
  ConditionZero = ystl::bit (2), // counter-strike: condition zero
  Legacy = ystl::bit (3), // counter-strike 1.3-1.5 with/without steam
  Mobility = ystl::bit (4), // additional flag that bot is running on android (additional flag)
  ReHLDS = ystl::bit (5), // server engine is rehlds (SV_DropClient available)
  Metamod = ystl::bit (6), // game running under meta\mod
  CSDM = ystl::bit (7), // csdm mod currently in use
  FreeForAll = ystl::bit (8), // csdm mod with ffa mode
  ReGameDLL = ystl::bit (9), // server dll is a regamedll
  HasFakePings = ystl::bit (10), // on that game version we can fake bots pings
  HasBotVoice = ystl::bit (11), // on that game version we can use chatter
  AnniversaryHL25 = ystl::bit (12), // half-life 25th anniversary engine
  Xash3DLegacy = ystl::bit (13), // old xash3d-branch
  ZombieMod = ystl::bit (14), // zombie mod is active
  HasStudioModels = ystl::bit (15), // game supports studio models, so we can use hitbox-based aiming
  GoldClient = ystl::bit (16) // game is custom-built goldclient engine
};
YSTL_ENABLE_ENUM_FLAGS (GameFlags);

// defines map type
enum class MapFlags : int32_t {
  None = 0,
  Assassination = ystl::bit (0),
  HostageRescue = ystl::bit (1),
  Demolition = ystl::bit (2),
  Escape = ystl::bit (3),
  KnifeArena = ystl::bit (4),
  FightYard = ystl::bit (5),
  GrenadeWar = ystl::bit (6),
  HasDoors = ystl::bit (10), // additional flags
  HasButtons = ystl::bit (11) // map has buttons
};
YSTL_ENABLE_ENUM_FLAGS (MapFlags);

// recursive entity search
enum class EntitySearchResult : int32_t {
  Continue,
  Break
};

// player body parts
enum class PlayerPart : int32_t {
  Invalid = -1,
  Head = 1,
  Chest,
  Stomach,
  LeftArm,
  RightArm,
  LeftLeg,
  RightLeg,
  Feet // custom!
};

// registration block handed to the engine, prefix-compatible with cvar_t
struct ConVarEngineReg {
  const char *name {};
  const char *string {};
  int flags {};
  float value {};
  void *next {};
  const char *desc {};
  const char *def {};
};

static_assert (sizeof (ConVarEngineReg) == sizeof (cvar_t) + 2 * sizeof (void *));

// variable reg pair, string type selects storage: owned copies for logic,
// views into ConVar members for the transient registration call
template <typename S> struct ConVarRegT {
  S name {};
  S init {};
  S info {};
  bool bounded = false;
  float min = 0.0f;
  float max = 0.0f;
  Var type = Var::NoServer;
  bool missing = false;
  S regval {};
  ConVarEngineReg reg {};
  class ConVar *self {};
  float initial = 0.0f;
};

using ConVarReg = ConVarRegT<ystl::String>;
using ConVarSpec = ConVarRegT<ystl::StringRef>;

// entity prototype
using EntityProto = void (*) (entvars_t *);

// work around missing precache string allocation on original hlds
class EngineWrap final {
public:
  EngineWrap () = default;
  ~EngineWrap () = default;

private:
  const char *AllocStr (const char *str) const {
    return string_t::from (engfuncs.pfnAllocString (str));
  }

public:
  int32_t PrecacheModel (const char *model) const {
    return engfuncs.pfnPrecacheModel (AllocStr (model));
  }

  int32_t PrecacheSound (const char *sound) const {
    return engfuncs.pfnPrecacheSound (AllocStr (sound));
  }

  void SetModel (edict_t *ent, const char *model) {
    engfuncs.pfnSetModel (ent, AllocStr (model));
  }
};

// player model part info enumerator
class PlayerHitboxEnumerator final {
public:
  struct Info {
    float updated {};
    ystl::Vector head {};
    ystl::Vector stomach {};
    ystl::Vector feet {};
    ystl::Vector right {};
    ystl::Vector left {};
  } parts_[kGameMaxPlayers] {};

public:
  // get's the enemy part based on bone info
  ystl::Vector Get (edict_t *ent, PlayerPart part, float update_timestamp);

  // update bones positions for given player
  void Update (edict_t *ent);

  // reset all the poisitons
  void Reset ();
};

// fake client command state kept in one buffer with tokens into it
struct Command final {
  static constexpr size_t kMaxArgs = 80; // same argument limit as the engine
  static constexpr size_t kBufferSize = ystl::Strings::StaticBufferSize;

  char buffer[kBufferSize] {}; // formatted command text
  char tail_buffer[kBufferSize] {}; // args string for the gamedll (see tail)
  ystl::FixedArray<ystl::StringRef, kMaxArgs> args {}; // tokens into buffer
  ystl::StringRef tail {}; // pfncmd_args () result for the current part
  size_t length = 0; // formatted length of buffer
  size_t arg_count = 0;

  // drops tokens of the current part
  void Reset () {
    arg_count = 0;
    tail = {};
  }

  // true while a bot client command is being executed
  bool Active () const {
    return arg_count != 0;
  }

  // token by index, empty ref when out of bounds
  ystl::StringRef Argv (size_t index) const {
    return index < arg_count ? args[index] : ystl::StringRef {};
  }

  // null-terminates a token span in place, so that the gamedll can read it as a c-string
  void Terminate (ystl::StringRef span) {
    buffer[span.chars () - buffer + span.size ()] = ystl::kNullChar;
  }
};

// provides utility functions to not call original engine (less call-cost)
class Game final : public ystl::Singleton<Game> {
public:
  using EntitySearch = const ystl::Lambda<EntitySearchResult (edict_t *)> &;

private:
  ystl::FixedArray<int32_t, ystl::to_underlying (DrawLineType::Num)> draw_models_ {};
  ystl::FixedArray<int32_t, ystl::to_underlying (Team::Num)> spawn_count_ {};

  // bot client command
  Command bot_cmd_ {};

  edict_t *start_entity_ {};
  edict_t *local_entity_ {};

  ystl::HashSet<int32_t> checked_breakables_ {};

  ystl::SmallArray<ConVarReg> cvars_ {};
  ystl::SharedLibrary game_lib_ {};
  ystl::SharedLibrary engine_lib_ {};
  EngineWrap engine_wrap_ {};

  bool precached_ {};

  GameFlags game_flags_ {};
  MapFlags map_flags_ {};

  ystl::CountdownTimer one_second_timer_ {}; // per second updated
  ystl::CountdownTimer half_second_timer_ {}; // per half second update

public:
  Game ();
  ~Game () = default;

public:
  // preaches internal stuff
  void Precache ();

  // initialize levels
  void LevelInitialize (edict_t *entities, int max);

  // when entity spawns
  void OnSpawnEntity (edict_t *ent);

  // shutdown levels
  void LevelShutdown ();

  // display world line
  void DrawLine (edict_t *ent, const ystl::Vector &start, const ystl::Vector &end, int width, int noise, const ystl::Color &color,
    int brightness, int speed, int life, DrawLineType type = DrawLineType::Simple) const;

  // display world axis-aligned box outline
  void DrawBox (edict_t *ent, const ystl::Vector &bbmin, const ystl::Vector &bbmax, int width, int noise, const ystl::Color &color,
    int brightness, int speed, int life) const;

  // we are on dedicated server ?
  bool IsDedicatedServer ();

  // get stripped down mod name
  const char *GetRunningModName ();

  // get the valid mapname
  const char *GetMapName ();

  // get the "any" entity origin
  ystl::Vector GetEntityOrigin (edict_t *ent);

  // registers a server command
  void RegisterEngineCommand (const char *command, void func ());

  // play's sound to client
  void PlaySound (edict_t *ent, const char *sound);

  // sends bot command
  void PrepareBotArgs (edict_t *ent);

  // adds cvar to registration stack
  void PushConVar (const ConVarSpec &spec, ConVar *self);

  // check the cvar bounds
  void CheckCvarsBounds ();

  // modify cvar description
  void SetCvarDescription (const ConVar &cv, ystl::StringRef info);

  // sends local registration stack for engine registration
  void RegisterCvars (bool game_vars = false);

  // checks whether software rendering is enabled
  bool IsSoftwareRenderer ();

  // checks if this is 25th anniversary half-life update
  bool Is25thAnniversaryUpdate ();

  // check if engine is goldclient listenserver
  bool IsGoldClientListenServer ();

  // detect xash3d before cvar registration, the sentinel needs the flag early
  void DetectXashEngine ();

  // load the cs binary in non metamod mode
  bool LoadCsBinary ();

  void ConstructCsBinaryName (ystl::SmallArray<ystl::String> &libs);

  // do post-load stuff
  bool Postload ();

  // detects if csdm mod is in use
  void ApplyGameModes ();

  // executes stuff every 1 second
  void SlowFrame ();

  // runs full per-frame orchestration (startframe hook body)
  void Frame ();

  // search entities by variable field
  void SearchEntities (ystl::StringRef field, ystl::StringRef value, EntitySearch functor);

  // search entities in sphere
  void SearchEntities (const ystl::Vector &position, float radius, EntitySearch functor) const;

  // check if map has entity
  bool HasEntityInGame (ystl::StringRef classname) const;

  // print the version to server console on startup
  void PrintBotVersion () const;

  // ensure prosperous gaming environment as per: https://github.com/yapb/yapb/issues/575
  void EnsureHealthyGameEnvironment ();

  // creates a fake client's a nd resets all the entvars
  edict_t *CreateFakeClient (ystl::StringRef name);

  // mark breakable entity as invalid
  void MarkBreakableAsInvalid (edict_t *ent);

  // is developer mode ?
  bool IsDeveloperMode () const;

  // entity utils
public:
  // check if entity is alive
  bool IsAliveEntity (edict_t *ent) const;

  // checks if entity is fakeclient
  bool IsFakeClientEntity (edict_t *ent) const;

  // check if entity is a player
  bool IsPlayerEntity (edict_t *ent) const;

  // check if entity is a monster
  bool IsMonsterEntity (edict_t *ent) const;

  // check if entity is a item
  bool IsItemEntity (edict_t *ent) const;

  // check if entity is a hostage entity
  bool IsHostageEntity (edict_t *ent) const;

  // check if entity is a door entity
  bool IsDoorEntity (edict_t *ent) const;

  // check if entity is planted or dropped c4
  bool IsBombEntity (edict_t *ent) const;

  // this function is checking that pointed by ent pointer obstacle, can be destroyed
  bool IsBreakableEntity (edict_t *ent, bool initial_seed = false) const;

  // checks if same model omitting the models directory
  bool IsEntityModelMatches (const edict_t *ent, ystl::StringRef model) const;

  // check if entity is a vip
  bool IsPlayerVip (edict_t *ent) const;

  // public inlines
public:
  // get the current time on server
  float Time () const {
    return globals->time;
  }

  // get "maxplayers" limit on server
  int MaxClients () const {
    return globals->maxClients;
  }

  // get the fakeclient command interface
  bool IsBotCmd () const {
    return bot_cmd_.Active ();
  }

  // gets custom engine args for client command
  const char *BotArgs () const {
    return bot_cmd_.tail.chars ();
  }

  // gets custom engine argv for client command
  const char *BotArgv (int32_t index) const {
    return bot_cmd_.Argv (static_cast<size_t> (index)).chars ();
  }

  // gets custom engine argc for client command
  int32_t BotArgc () const {
    return static_cast<int32_t> (bot_cmd_.arg_count);
  }

  // gets edict pointer out of entity index
  YSTL_FORCE_INLINE edict_t *EntityOfIndex (const int index) const {
    if (!start_entity_) [[unlikely]] {
      return engfuncs.pfnPEntityOfEntIndex (index);
    }
    return static_cast<edict_t *> (start_entity_ + index);
  };

  // gets edict pointer out of entity index (player)
  YSTL_FORCE_INLINE edict_t *PlayerOfIndex (const int index) const {
    if (!start_entity_) [[unlikely]] {
      return engfuncs.pfnPEntityOfEntIndex (index + 1);
    }
    return EntityOfIndex (index) + 1;
  };

  // gets edict index out of it's pointer
  YSTL_FORCE_INLINE int IndexOfEntity (const edict_t *ent) const {
    return static_cast<int> (ent - start_entity_);
  };

  // gets edict index of it's pointer (player)
  YSTL_FORCE_INLINE int IndexOfPlayer (const edict_t *ent) const {
    return IndexOfEntity (ent) - 1;
  }

  // verify entity isn't null
  YSTL_FORCE_INLINE bool IsNullEntity (const edict_t *ent) const {
    return !start_entity_ || !ent || !IndexOfEntity (ent) || ent->free;
  }

  // get the worldspawn entity
  edict_t *GetStartEntity () const {
    return start_entity_;
  }

  // get spawn count for team
  int GetSpawnCount (Team team) const {
    return spawn_count_[team];
  }

  // gets the player team
  Team GetPlayerTeam (edict_t *ent) const;

  // gets the player team (real in ffa)
  Team GetRealPlayerTeam (edict_t *ent) const;

  // get real gamedll team (matches gamedll indices)
  Team GetPlayerTeamGame (edict_t *ent) const {
    return GetRealPlayerTeam (ent) + Team::CT;
  }

  // sets the precache to uninitialized
  void SetUnprecached () {
    precached_ = false;
  }

  // gets the local entity (host edict)
  edict_t *GetLocalEntity () {
    return local_entity_;
  }

  // sets the local entity (host edict)
  void SetLocalEntity (edict_t *ent) {
    local_entity_ = ent;
  }

  // sets player start entity draw models
  void SetPlayerStartDrawModels ();

  // check the engine visibility wrapper
  bool CheckVisibility (edict_t *ent, uint8_t *set);

  // get pvs/pas visibility set
  uint8_t *GetVisibilitySet (Bot *bot, bool pvs) const;

  // what kind of game engine / game dll / mod / tool we're running ?
  bool Is (const GameFlags type) const {
    return has_flag (game_flags_, type);
  }

  // adds game flag
  void AddGameFlag (const GameFlags type) {
    game_flags_ |= type;
  }

  // clears game flag
  void ClearGameFlag (const GameFlags type) {
    game_flags_ &= ~type;
  }

  // gets the map type
  bool MapIs (const MapFlags type) const {
    return has_flag (map_flags_, type);
  }

  // get loaded gamelib
  const ystl::SharedLibrary &Lib () {
    return game_lib_;
  }

  // get loaded engine lib
  const ystl::SharedLibrary &Elib () {
    return engine_lib_;
  }

  // get registered cvars list (mutable: config load stores parsed values back)
  ystl::SmallArray<ConVarReg> &GetCvars () {
    return cvars_;
  }

  // check if map has breakables (sweep-maintained flag, see GameState)
  bool HasBreakables () const;

  // is breakable entity is valid ?
  bool IsBreakableValid (edict_t *ent) {
    return checked_breakables_.contains (IndexOfEntity (ent));
    ;
  }

  // find variable value by variable name
  ystl::StringRef FindCvar (ystl::StringRef name) {
    return engfuncs.pfnCVarGetString (name.chars ());
  }

  // helper to sending the client message
  void SendClientMessage (bool console, edict_t *ent, ystl::StringRef message);

  // helper to sending the server message
  void SendServerMessage (ystl::StringRef message);

  // helper for sending hud messages to client
  void SendHudMessage (edict_t *ent, const hudtextparms_t &htp, ystl::StringRef message);

  // send server command
  template <typename... Args> void ServerCommand (const char *fmt, Args &&...args) {
    engfuncs.pfnServerCommand (
      ystl::strings.concat (ystl::strings.format (fmt, ystl::forward<Args> (args)...), "\n", ystl::Strings::StaticBufferSize));
  }

  // send a bot command
  template <typename... Args> void Command (edict_t *ent, const char *fmt, Args &&...args) {
    const auto result = ystl::fmtwrap ().exec (bot_cmd_.buffer, Command::kBufferSize, fmt, ystl::forward<Args> (args)...);

    // snprintf returns the would-be length on truncation, clamp to the real one
    bot_cmd_.length = result > 0 ? ystl::min (static_cast<size_t> (result), Command::kBufferSize - 1) : 0;
    PrepareBotArgs (ent);
  }

  // prints data to servers console
  template <typename... Args> void Print (const char *fmt, Args &&...args) {
    SendServerMessage (
      ystl::strings.concat (ystl::strings.format (conf.Translate (fmt), ystl::forward<Args> (args)...), "\n", ystl::Strings::StaticBufferSize));
  }

  // prints center message to specified player
  template <typename... Args> void ClientPrint (edict_t *ent, const char *fmt, Args &&...args) {
    if (IsNullEntity (ent)) {
      Print (fmt, ystl::forward<Args> (args)...);
      return;
    }
    SendClientMessage (true, ent,
      ystl::strings.concat (ystl::strings.format (conf.Translate (fmt), ystl::forward<Args> (args)...), "\n", ystl::Strings::StaticBufferSize));
  }

  // prints message to client console
  template <typename... Args> void CenterPrint (edict_t *ent, const char *fmt, Args &&...args) {
    if (IsNullEntity (ent)) {
      Print (fmt, ystl::forward<Args> (args)...);
      return;
    }
    SendClientMessage (false, ent,
      ystl::strings.concat (ystl::strings.format (conf.Translate (fmt), ystl::forward<Args> (args)...), "\n", ystl::Strings::StaticBufferSize));
  }
};

// reference some game/mod cvars for access
class ConVarRef final : public ystl::NonCopyable {
private:
  cvar_t *ptr_ {};
  ystl::String name_ {};
  bool checked_ {};

public:
  ConVarRef (ystl::StringRef name) : name_ (name) {}
  ~ConVarRef () = default;

public:
  bool Exists () {
    if (checked_ && !ptr_) {
      return false;
    }
    checked_ = true;
    ptr_ = engfuncs.pfnCVarGetPointer (name_.chars ());

    return ptr_ != nullptr;
  }

  template <typename U = float> U Value () {
    return Exists () ? static_cast<U> (ptr_->value) : static_cast<U> (0);
  }

  void Set (ystl::StringRef value) {
    if (Exists ()) {
      engfuncs.pfnCvar_DirectSet (ptr_, value.chars ());
    }
  }
};

// simplify access for console variables
class ConVar final : public ystl::NonCopyable {
public:
  cvar_t *ptr;

private:
  ystl::String name_ {};
  ystl::String initval_ {}; // owned once, engine-visible pointers aim here: never reassign after construction

public:
  ConVar () = delete;
  ~ConVar () = default;

public:
  ConVar (ystl::StringRef name, ystl::StringRef initval, Var type = Var::NoServer, bool reg_missing = false, ystl::StringRef reg_val = nullptr) :
    ptr (nullptr) {
    SetPrefix (name, type);
    initval_ = initval;
    Game::instance ().PushConVar ({ name_.chars (), initval_.chars (), {}, false, 0.0f, 0.0f, type, reg_missing, reg_val }, this);
  }

  ConVar (ystl::StringRef name, ystl::StringRef initval, ystl::StringRef info, bool bounded = true, float min = 0.0f, float max = 1.0f,
    Var type = Var::NoServer, bool reg_missing = false, ystl::StringRef reg_val = nullptr) : ptr (nullptr) {
    SetPrefix (name, type);
    initval_ = initval;

    Game::instance ().PushConVar ({ name_.chars (), initval_.chars (), info, bounded, min, max, type, reg_missing, reg_val }, this);
  }

public:
  template <typename U> constexpr U As () const {
    if constexpr (ystl::is_same_v<U, float>) {
      return ptr->value;
    }
    else if constexpr (ystl::is_same_v<U, bool>) {
      return ptr->value > 0.0f;
    }
    else if constexpr (ystl::is_same_v<U, int>) {
      return static_cast<U> (ptr->value);
    }
    else if constexpr (ystl::is_same_v<U, ystl::StringRef>) {
      return ptr->string;
    }
  }

public:
  operator bool () const {
    return As<bool> ();
  }

  operator float () const {
    return As<float> ();
  }

  operator int () const {
    return As<int> ();
  }

  operator ystl::StringRef () {
    return As<ystl::StringRef> ();
  }

public:
  ystl::StringRef Name () const {
    return ptr->name;
  }

  void Set (float val) {
    engfuncs.pfnCVarSetFloat (ptr->name, val);
  }

  void Set (int val) {
    Set (static_cast<float> (val));
  }

  void Set (const char *val) {
    if (ptr) {
      engfuncs.pfnCvar_DirectSet (ptr, val);
    }
  }

  // revet cvar to default value
  void Revert ();

  // set the cvar prefix if needed
  void SetPrefix (ystl::StringRef name, Var type);
};

class MessageWriter final {
private:
  bool auto_destruct_ { false };

public:
  MessageWriter () = default;

  MessageWriter (int dest, int type, const ystl::Vector &pos = nullptr, edict_t *to = nullptr) {
    Start (dest, type, pos, to);
    auto_destruct_ = true;
  }

  ~MessageWriter () {
    if (auto_destruct_) {
      end ();
    }
  }

public:
  MessageWriter &Start (int dest, int type, const ystl::Vector &pos = nullptr, edict_t *to = nullptr) {
    engfuncs.pfnMessageBegin (dest, type, pos, to);
    return *this;
  }

  void end () {
    engfuncs.pfnMessageEnd ();
  }

  MessageWriter &WriteByte (int val) {
    engfuncs.pfnWriteByte (val);
    return *this;
  }

  MessageWriter &WriteLong (int val) {
    engfuncs.pfnWriteLong (val);
    return *this;
  }

  MessageWriter &WriteChar (int val) {
    engfuncs.pfnWriteChar (val);
    return *this;
  }

  MessageWriter &WriteShort (int val) {
    engfuncs.pfnWriteShort (val);
    return *this;
  }

  MessageWriter &WriteCoord (float val) {
    engfuncs.pfnWriteCoord (val);
    return *this;
  }

  MessageWriter &WriteString (const char *val) {
    engfuncs.pfnWriteString (val);
    return *this;
  }

public:
  static uint16_t Fu16 (float value, float scale) {
    return static_cast<uint16_t> (
      ystl::clamp (value * ystl::bit (static_cast<short> (scale)), 0.0f, static_cast<float> (ystl::numeric_limits<uint16_t>::max ())));
  }

  static short Fs16 (float value, float scale) {
    return static_cast<short> (ystl::clamp (value * ystl::bit (static_cast<short> (scale)),
      static_cast<float> (-ystl::numeric_limits<short>::max ()), static_cast<float> (ystl::numeric_limits<short>::max ())));
  }
};

class LightMeasure final : public ystl::Singleton<LightMeasure> {
private:
  lightstyle_t lightstyle_[MAX_LIGHTSTYLES] {};
  uint32_t lightstyle_value_[MAX_LIGHTSTYLEVALUE] {};
  bool do_animation_ = false;

  ystl::Color point_;
  model_t *world_model_ = nullptr;

public:
  LightMeasure () {
    InitializeLightstyles ();
    point_.reset ();
  }

public:
  void InitializeLightstyles ();
  void AnimateLight ();
  void UpdateLight (int style, char *value);

  float GetLightLevel (const ystl::Vector &point);
  float GetSkyColor ();

private:
  template <typename S, typename M> bool RecursiveLightPoint (const M *node, const ystl::Vector &start, const ystl::Vector &end);
  template <typename S, typename M> static bool LightPointProc (LightMeasure *self, const ystl::Vector &start, const ystl::Vector &end);

public:
  void ResetWorldModel () {
    world_model_ = nullptr;
  }

  void SetWorldModel (model_t *model) {
    if (world_model_) {
      return;
    }
    world_model_ = model;
  }

  model_t *GetWorldModel () const {
    return world_model_;
  }

  void EnableAnimation (bool enable) {
    do_animation_ = enable;
  }
};

// tracked entity shape shared by both registries below. kept distinct types on purpose:
// update cadence and consumer sets differ, so no common container
template <typename Kind> struct TrackedEntity {
  edict_t *ent {};
  Kind kind {};
};

enum class EntityKind : uint8_t {
  Pickup,
  Hostage,
  Button,
  Csdm,
  Monster,
  Breakable
};

using InterestingEntity = TrackedEntity<EntityKind>;

enum class GrenadeKind : uint8_t {
  Flash,
  Explosive,
  Smoke,
  Other
};

using ActiveGrenade = TrackedEntity<GrenadeKind>;

// offload bot manager class from things it shouldn't do
class GameState final : public ystl::Singleton<GameState> {
private:
  bool bomb_planted_ {}; // is bomb planted ?
  bool round_over_ {}; // well, round is over>
  bool reset_hud_ {}; // reset hud is called for some one

  float time_bomb_planted_ {}; // time the bomb were planted
  float time_round_start_ {}; // time round has started
  float time_round_end_ {}; // time round ended
  float time_round_mid_ {}; // middle point timestamp of a round

  ystl::Vector bomb_origin_ {}; // stored bomb origin
  edict_t *bomb_entity_ {}; // stored bomb entity

  ystl::Array<ActiveGrenade> active_grenades_ {}; // holds currently active grenades on the map
  ystl::Array<InterestingEntity> interesting_entities_ {}; // holds currently interesting entities on the map

  bool has_breakables_ {}; // set by the sweep below, breakables change rarely

  ystl::IntervalTimer interesting_entities_update_time_ {}; // time to update interesting entities
  ystl::IntervalTimer active_grenades_update_time_ {}; // time to update active grenades

public:
  GameState () = default;
  ~GameState () = default;

public:
  const ystl::Vector &GetBombOrigin () const {
    return bomb_origin_;
  }

  edict_t *GetBombEntity () const {
    return bomb_entity_;
  }

  bool IsBombPlanted () const {
    return bomb_planted_;
  }

  float GetTimeBombPlanted () const {
    return time_bomb_planted_;
  }

  float GetRoundStartTime () const {
    return time_round_start_;
  }

  float GetRoundMidTime () const {
    return time_round_mid_;
  }

  float GetRoundEndTime () const {
    return time_round_end_;
  }

  float GetRoundTimeLeft () const {
    return time_round_end_ - Game::instance ().Time ();
  }

  bool IsRoundTimeLow (const float threshold = 30.0f) const {
    return time_round_end_ - Game::instance ().Time () <= threshold;
  }

  bool IsRoundOver () const {
    return round_over_;
  }

  bool IsResetHud () const {
    return reset_hud_;
  }

  void SetResetHud (bool reset_hud) {
    reset_hud_ = reset_hud;
  }

  void SetRoundOver (bool round_over) {
    round_over_ = round_over;
  }

  const ystl::Array<ActiveGrenade> &GetActiveGrenades () {
    return active_grenades_;
  }

  const ystl::Array<InterestingEntity> &GetInterestingEntities () {
    return interesting_entities_;
  }

  bool HasActiveGrenades () const {
    return !active_grenades_.empty ();
  }

  bool HasInterestingEntities () const {
    return !interesting_entities_.empty ();
  }

  bool HasBreakables () const {
    return has_breakables_;
  }

  void SetHasBreakables (bool has) {
    has_breakables_ = has;
  }

public:
  float GetBombTimeLeft () const;

  void SetBombPlanted (bool is_planted);
  void SetBombOrigin (bool reset = false, const ystl::Vector &pos = nullptr);
  void RoundStart ();
  void UpdateActiveGrenade ();
  void UpdateInterestingEntities ();
  bool IsEarlyRound (const float timestamp) const;
};

// sg detonation tracking
struct EdictHash {
  uint32_t operator() (const edict_t *key) const noexcept {
    return Game::instance ().IndexOfEntity (key);
  }
};

class SGDetonateTrack final : public ystl::Singleton<SGDetonateTrack> {
private:
  ystl::HashMap<edict_t *, ystl::Vector, EdictHash> positions_ {};

public:
  SGDetonateTrack () = default;
  ~SGDetonateTrack () = default;

public:
  void Acquire (edict_t *ent, const ystl::Vector &pos) {
    positions_[ent] = pos;
  }

  const ystl::Vector &Find (edict_t *ent) {
    return positions_[ent];
  }

  bool Has (edict_t *ent) const {
    return positions_.exists (ent);
  }

  void Clear () {
    positions_.clear ();
  }
};

// expose globals
YSTL_EXPOSE_GLOBAL_SINGLETON (Game, game);
YSTL_EXPOSE_GLOBAL_SINGLETON (GameState, game_state);
YSTL_EXPOSE_GLOBAL_SINGLETON (LightMeasure, illum);
YSTL_EXPOSE_GLOBAL_SINGLETON (SGDetonateTrack, sgtrack);

} // namespace bot
