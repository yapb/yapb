//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

namespace bot {

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

// shared cvar description: const char* for designated init, StringRef for registration, String for the registry copy
template <typename S> struct ConVarDescT {
  S name {};
  S value {};
  S info { "" };
  bool bounded {};
  float min {};
  float max { 1.0f };
  Var type { Var::NoServer };
  bool reg_missing {};
  S reg_val {};
};

using ConVarDesc = ConVarDescT<const char *>;
using ConVarSpec = ConVarDescT<ystl::StringRef>;

// registry entry: owned description plus engine-facing registration data
struct ConVarReg final : ConVarDescT<ystl::String> {
  ConVarEngineReg reg {};
  class ConVar *self {};
  float initial {};
};

// reference some game/mod cvars for access
class ConVarRef final : public ystl::NonCopyable {
private:
  cvar_t *ptr_ {};
  char name_[128] {};
  bool checked_ {};

public:
  ConVarRef (ystl::StringRef name) {
    ystl::strings.copy (name_, name.chars (), ystl::bufsize (name_));
  }
  ~ConVarRef () = default;

public:
  bool Exists () {
    if (checked_ && !ptr_) {
      return false;
    }
    checked_ = true;
    ptr_ = engfuncs.pfnCVarGetPointer (name_);

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

// constexpr described console variable, registered only from engine-driven code
class ConVar final {
public:
  cvar_t *ptr { nullptr };

private:
  char reg_name_[64] {};
  char reg_value_[512] {};
  char reg_refval_[64] {};
  const char *info_ {};
  bool bounded_ {};
  float min_ {};
  float max_ {};
  Var type_ { Var::NoServer };
  bool reg_missing_ {};

public:
  ConVar () = delete;
  ConVar (const ConVar &) = delete;
  ConVar &operator= (const ConVar &) = delete;
  ~ConVar () = default;

public:
  ConVar (const ConVarDesc &desc);

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
    return reg_name_;
  }

  const char *RegName () const {
    return reg_name_;
  }

  const char *RegValue () const {
    return reg_value_;
  }

  const char *RegRefValue () const {
    return reg_refval_;
  }

  constexpr ConVarSpec Spec () const {
    return { reg_name_, reg_value_, info_, bounded_, min_, max_, type_, reg_missing_, reg_refval_ };
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
  // revert cvar to default value
  void Revert ();

private:
  void SetRegistrationData (const ConVarDesc &desc) {
    if (desc.type == Var::GameRef) {
      ystl::strings.copy (reg_name_, desc.name, ystl::bufsize (reg_name_));
    }
    else {
      ystl::strings.copy (reg_name_, product.cmd_pri.chars (), ystl::bufsize (reg_name_));
      ystl::strings.concat (reg_name_, "_", ystl::bufsize (reg_name_));
      ystl::strings.concat (reg_name_, desc.name, ystl::bufsize (reg_name_));
    }

    ystl::strings.copy (reg_value_, desc.value, ystl::bufsize (reg_value_));
    ystl::strings.copy (reg_refval_, desc.reg_val, ystl::bufsize (reg_refval_));

    info_ = desc.info;
    bounded_ = desc.bounded;
    min_ = desc.min;
    max_ = desc.max;
    type_ = desc.type;
    reg_missing_ = desc.reg_missing;
  }
};

// single engine-driven registration point for all global cvars
class CvarRegistry final : public ystl::Singleton<CvarRegistry> {
public:
  CvarRegistry () = default;
  ~CvarRegistry () = default;

public:
  // called from the ConVar constructor, no engine access here
  void Add (ConVar *cv);

  // push pending cvars and register them with the engine
  void Register (bool game_vars = false);

  // check the cvar bounds
  void CheckBounds ();

  // modify cvar description
  void SetDescription (const ConVar &cv, ystl::StringRef info);

  // registered cvars (mutable: config load stores parsed values back)
  ystl::SmallArray<ConVarReg> &Get () {
    return registry_;
  }

private:
  void Push (const ConVarSpec &spec, ConVar *self);

private:
  ystl::SmallArray<ConVarReg> registry_ {};
  ystl::SmallArray<ConVar *> pending_ {};
};

// all bot console variables live here, created in one place
// clang-format off

// behavior
inline ConVar cv_debug {
  {
    .name = "debug",
    .value = "0",
    .info = "Enables or disables useful messages about bot states. Not required for end users.",
    .bounded = true,
    .min = 0.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_debug_goal {
  {
    .name = "debug_goal",
    .value = "-1",
    .info = "Forces all alive bots to build a path and go to the graph node specified here.",
    .bounded = true,
    .min = -1.0f,
    .max = kMaxNodes,
  }
};

inline ConVar cv_debug_overlay_color {
  {
    .name = "debug_overlay_color",
    .value = "255 255 255",
    .info = "Specifies the RGB color of the debug overlay text, e.g. '255 255 255'.",
  }
};

inline ConVar cv_user_follow_percent {
  {
    .name = "user_follow_percent",
    .value = "20",
    .info = "Specifies the percent of bots that can follow a leader at each round start.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

inline ConVar cv_user_max_followers {
  {
    .name = "user_max_followers",
    .value = "1",
    .info = "Specifies how many bots can follow a single user.",
    .bounded = true,
    .min = 0.0f,
    .max = static_cast<float> (kGameMaxPlayers / 4),
  }
};

inline ConVar cv_jasonmode {
  {
    .name = "jasonmode",
    .value = "0",
    .info = "If enabled, all bots will be forced to use only the knife, skipping weapon buying routines.",
    .bounded = true,
  }
};

inline ConVar cv_radio_mode {
  {
    .name = "radio_mode",
    .value = "2",
    .info = "Allows bots to use radio or chatter.\nAllowed values: '0', '1', '2'.\nIf '0', radio and chatter is disabled.\nIf '1', only radio messages.\nIf '2', only chatter.",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

inline ConVar cv_economics_rounds {
  {
    .name = "economics_rounds",
    .value = "1",
    .info = "Specifies whether bots are able to use team economics, like not buying any weapons for the whole team to keep money for better guns.",
    .bounded = true,
  }
};

inline ConVar cv_economics_disrespect_percent {
  {
    .name = "economics_disrespect_percent",
    .value = "25",
    .info = "Allows bots to ignore economics and buy weapons with disrespect to economics.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

inline ConVar cv_check_darkness {
  {
    .name = "check_darkness",
    .value = "1",
    .info = "Allows or disallows the bot to check the environment for darkness, thus allowing or not allowing the use of flashlights or NVG.",
    .bounded = true,
  }
};

inline ConVar cv_avoid_grenades {
  {
    .name = "avoid_grenades",
    .value = "1",
    .info = "Allows bots to partially avoid grenades.",
    .bounded = true,
  }
};

inline ConVar cv_move_during_throw {
  {
    .name = "move_during_throw",
    .value = "-",
    .info = "Allows bots to keep moving while throwing grenades.",
    .bounded = true,
  }
};

inline ConVar cv_tkpunish {
  {
    .name = "tkpunish",
    .value = "1",
    .info = "Allows or disallows bots to take revenge on teamkillers/team attacks.",
    .bounded = true,
  }
};

inline ConVar cv_freeze_bots {
  {
    .name = "freeze_bots",
    .value = "0",
    .info = "If enabled, the bot's think function is disabled, so bots will not move anywhere from their spawn spots.",
    .bounded = true,
  }
};

inline ConVar cv_spraypaints {
  {
    .name = "spraypaints",
    .value = "30",
    .info = "Specifies the percent chance for bots to use spray paints.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

inline ConVar cv_botbuy {
  {
    .name = "botbuy",
    .value = "1",
    .info = "Allows or disallows bots weapon buying routines.",
    .bounded = true,
  }
};

inline ConVar cv_destroy_breakables_around {
  {
    .name = "destroy_breakables_around",
    .value = "1",
    .info = "Allows bots to destroy breakables around them, even without touching them.",
    .bounded = true,
  }
};

inline ConVar cv_object_pickup_radius {
  {
    .name = "object_pickup_radius",
    .value = "450.0",
    .info = "The radius within which the bot searches the world for new objects, items, and weapons.",
    .bounded = true,
    .min = 64.0f,
    .max = 1024.0f,
  }
};

inline ConVar cv_object_destroy_radius {
  {
    .name = "object_destroy_radius",
    .value = "400.0",
    .info = "The radius within which the bot destroys breakables around it, when not touching them.",
    .bounded = true,
    .min = 64.0f,
    .max = 1024.0f,
  }
};

inline ConVar cv_chatter_path {
  {
    .name = "chatter_path",
    .value = "sound/radio/bot",
    .info = "Specifies the path for the bot chatter sound files.",
  }
};

inline ConVar cv_attack_monsters {
  {
    .name = "attack_monsters",
    .value = "0",
    .info = "Allows or disallows bots to attack monsters.",
    .bounded = true,
  }
};

inline ConVar cv_pickup_custom_items {
  {
    .name = "pickup_custom_items",
    .value = "0",
    .info = "Allows or disallows bots to pick up custom items.",
    .bounded = true,
  }
};

inline ConVar cv_pickup_ammo_and_kits {
  {
    .name = "pickup_ammo_and_kits",
    .value = "0",
    .info = "Allows bots to pick up mod items like ammo, health kits, and suits.",
    .bounded = true,
  }
};

inline ConVar cv_pickup_best {
  {
    .name = "pickup_best",
    .value = "1",
    .info = "Allows or disallows bots to pick up the best weapons.",
    .bounded = true,
  }
};

inline ConVar cv_ignore_objectives {
  {
    .name = "ignore_objectives",
    .value = "0",
    .info = "Allows or disallows bots to do map objectives, i.e. plant/defuse bombs, and save hostages.",
    .bounded = true,
  }
};

// manager
inline ConVar cv_autovacate {
  {
    .name = "autovacate",
    .value = "1",
    .info = "Kicks bots to automatically make room for human players.",
    .bounded = true,
  }
};

inline ConVar cv_autovacate_keep_slots {
  {
    .name = "autovacate_keep_slots",
    .value = "1",
    .info = "How many slots the autovacate feature should keep for human players.",
    .bounded = true,
    .min = 1.0f,
    .max = 8.0f,
  }
};

inline ConVar cv_kick_after_player_connect {
  {
    .name = "kick_after_player_connect",
    .value = "1",
    .info = "Kicks the bot immediately when a human player joins the server (yb_autovacate must be enabled).",
    .bounded = true,
  }
};

inline ConVar cv_quota {
  {
    .name = "quota",
    .value = "9",
    .info = "Specifies the number of bots to be added to the game.",
    .bounded = true,
    .min = 0.0f,
    .max = static_cast<float> (kGameMaxPlayers),
  }
};

inline ConVar cv_quota_mode {
  {
    .name = "quota_mode",
    .value = "normal",
    .info = "Specifies the type of quota.\nAllowed values: 'normal', 'fill', and 'match'.\nIf 'fill', the server will adjust bots to keep N players in the game.\nIf 'match', the server will maintain a certain number of bots relative to humans.",
  }
};

inline ConVar cv_quota_match {
  {
    .name = "quota_match",
    .value = "0",
    .info = "Number of players to match if yb_quota_mode is set to 'match'.",
    .bounded = true,
    .min = 0.0f,
    .max = static_cast<float> (kGameMaxPlayers),
  }
};

inline ConVar cv_think_fps {
  {
    .name = "think_fps",
    .value = "40.0",
    .info = "Specifies how many times per second the bot code will run.",
    .bounded = true,
    .min = 10.0f,
    .max = 90.0f,
  }
};

inline ConVar cv_think_fps_disable {
  {
    .name = "think_fps_disable",
    .value = "1",
    .info = "Allows to completely disable think fps on Xash3D.",
    .bounded = true,
    .min = 0.0f,
    .max = 1.0f,
    .type = Var::Xash3D,
  }
};

inline ConVar cv_autokill_delay {
  {
    .name = "autokill_delay",
    .value = "0.0",
    .info = "Specifies the amount of time in seconds after which bots will be killed if no humans are left alive.",
    .bounded = true,
    .min = 0.0f,
    .max = 90.0f,
  }
};

inline ConVar cv_first_human_restart {
  {
    .name = "first_human_restart",
    .value = "0",
    .info = "Restart the game if the first human player joins a bot game.",
    .bounded = true,
  }
};

inline ConVar cv_join_after_player {
  {
    .name = "join_after_player",
    .value = "0",
    .info = "Specifies whether bots should join the server only when at least one human player is in the game.",
    .bounded = true,
  }
};

inline ConVar cv_join_team {
  {
    .name = "join_team",
    .value = "any",
    .info = "Forces all bots to join the team specified here.",
  }
};

inline ConVar cv_join_delay {
  {
    .name = "join_delay",
    .value = "5.0",
    .info = "Specifies after how many seconds bots should start to join the game after the changelevel.",
    .bounded = true,
    .min = 0.0f,
    .max = 30.0f,
  }
};

inline ConVar cv_name_prefix {
  {
    .name = "name_prefix",
    .value = "",
    .info = "All bot names will be prefixed with the string specified by this cvar.",
  }
};

inline ConVar cv_difficulty {
  {
    .name = "difficulty",
    .value = "3",
    .info = "All bots difficulty level. Changing at runtime will affect already created bots.",
    .bounded = true,
    .min = 0.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_difficulty_min {
  {
    .name = "difficulty_min",
    .value = "-1",
    .info = "Lower bound of random difficulty on bot creation. Only affects newly created bots. -1 means only yb_difficulty is used.",
    .bounded = true,
    .min = -1.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_difficulty_max {
  {
    .name = "difficulty_max",
    .value = "-1",
    .info = "Upper bound of random difficulty on bot creation. Only affects newly created bots. -1 means only yb_difficulty is used.",
    .bounded = true,
    .min = -1.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_difficulty_auto {
  {
    .name = "difficulty_auto",
    .value = "0",
    .info = "Allows each bot to balance its own difficulty based on the kd-ratio of the team.\nAllowed values: '0', '1', '2'.\nIf '0', difficulty auto-balancing is disabled.\nIf '1', all bots balance their difficulty.\nIf '2', only bots in the team with human players balance their difficulty (bot-only team stays at configured difficulty).",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

inline ConVar cv_difficulty_auto_balance_interval {
  {
    .name = "difficulty_auto_balance_interval",
    .value = "30",
    .info = "Interval at which bots will balance their difficulty.",
    .bounded = true,
    .min = 30.0f,
    .max = 240.0f,
  }
};

inline ConVar cv_difficulty_lock {
  {
    .name = "difficulty_lock",
    .value = "1",
    .info = "Keeps the difficulty of manually added bots fixed, ignoring yb_difficulty and yb_difficulty_auto changes.",
    .bounded = true,
    .min = 0.0f,
    .max = 1.0f,
  }
};

inline ConVar cv_show_avatars {
  {
    .name = "show_avatars",
    .value = "0",
    .info = "Enables or disables displaying bot avatars in front of their names in the scoreboard. Note that currently you can only see avatars of your teammates.",
    .bounded = true,
    .min = 0.0f,
    .max = 1.0f,
  }
};

inline ConVar cv_show_latency {
  {
    .name = "show_latency",
    .value = "0",
    .info = "Enables latency display in the scoreboard.\nAllowed values: '0', '1', '2'.\nIf '0', there is nothing displayed.\nIf '1', there is a 'BOT' string.\nIf '2', fake ping is displayed.",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

inline ConVar cv_save_bots {
  {
    .name = "save_bots",
    .value = "1",
    .info = "Allows saving bot data upon changelevel, so bots will be restored after a map change.\nAllowed values: '0', '1', '2'.\nIf '0', nothing is saved.\nIf '1', only quota is saved.\nIf '2', full bot data is saved.",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

inline ConVar cv_botskin_t {
  {
    .name = "botskin_t",
    .value = "0",
    .info = "Specifies the bot's wanted skin for the Terrorist team.",
    .bounded = true,
    .min = 0.0f,
    .max = 5.0f,
  }
};

inline ConVar cv_botskin_ct {
  {
    .name = "botskin_ct",
    .value = "0",
    .info = "Specifies the bot's wanted skin for the CT team.",
    .bounded = true,
    .min = 0.0f,
    .max = 5.0f,
  }
};

inline ConVar cv_preferred_personality {
  {
    .name = "preferred_personality",
    .value = "none",
    .info = "Sets the default personality when creating bots with quota management.\nAllowed values: 'none', 'normal', 'careful', 'rusher'.\nIf 'none' is set, random personality is chosen.",
  }
};

inline ConVar cv_language {
  {
    .name = "language",
    .value = "en",
    .info = "Specifies the language for bot messages and menus.",
  }
};

inline ConVar cv_rotate_bots {
  {
    .name = "rotate_bots",
    .value = "0",
    .info = "Randomly disconnects and connects bots, simulating players joining/quitting.",
    .bounded = true,
  }
};

inline ConVar cv_rotate_stay_min {
  {
    .name = "rotate_stay_min",
    .value = "360.0",
    .info = "Specifies the minimum amount of seconds a bot stays connected, if rotation is active.",
    .bounded = true,
    .min = 120.0f,
    .max = 7200.0f,
  }
};

inline ConVar cv_rotate_stay_max {
  {
    .name = "rotate_stay_max",
    .value = "3600.0",
    .info = "Specifies the maximum amount of seconds a bot stays connected, if rotation is active.",
    .bounded = true,
    .min = 1800.0f,
    .max = 14400.0f,
  }
};

inline ConVar cv_restricted_weapons {
  {
    .name = "restricted_weapons",
    .value = "",
    .info = "",
  }
};

inline ConVar cv_fun_mode {
  {
    .name = "fun_mode",
    .value = "imsober",
    .info = "Enables classic podbot fun modes for everyone on the server.\nAllowed values: 'imsober', 'off', 'tronisback', 'itsnewyear', 'imhaunted', 'itstoodark', 'stonedagain', 'imonmars'.",
  }
};

// support
inline ConVar cv_display_welcome_text {
  {
    .name = "display_welcome_text",
    .value = "1",
    .info = "Enables or disables showing a welcome message to the host entity on game start.",
    .bounded = true,
  }
};

inline ConVar cv_enable_query_hook {
  {
    .name = "enable_query_hook",
    .value = "0",
    .info = "Enables or disables fake server query responses, which show bots as real players in the server browser.",
    .bounded = true,
  }
};

inline ConVar cv_enable_fake_steamids {
  {
    .name = "enable_fake_steamids",
    .value = "0",
    .info = "Allows or disallows bots to return a fake Steam ID.",
    .bounded = true,
  }
};

inline ConVar cv_smoke_grenade_checks {
  {
    .name = "smoke_grenade_checks",
    .value = "2",
    .info = "Affects the bot's vision by smoke clouds.",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

inline ConVar cv_smoke_grenade_radius {
  {
    .name = "smoke_grenade_radius",
    .value = "240",
    .info = "Radius to check for smoke clouds around a detonated grenade.",
    .bounded = true,
    .min = 32.0f,
    .max = 320.0f,
  }
};

// combat
inline ConVar cv_shoots_thru_walls {
  {
    .name = "shoots_thru_walls",
    .value = "4",
    .info = "Specifies whether bots are able to fire at enemies behind the wall, if they hear or suspect them.",
    .bounded = true,
    .min = 0.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_ignore_enemies {
  {
    .name = "ignore_enemies",
    .value = "0",
    .info = "Enables or disables searching the world for enemies.",
    .bounded = true,
  }
};

inline ConVar cv_check_enemy_rendering {
  {
    .name = "check_enemy_rendering",
    .value = "0",
    .info = "Enables or disables checking enemy rendering flags. Useful for some mods.",
    .bounded = true,
  }
};

inline ConVar cv_check_enemy_invincibility {
  {
    .name = "check_enemy_invincibility",
    .value = "0",
    .info = "Enables or disables checking enemy invincibility. Useful for some mods.",
    .bounded = true,
  }
};

inline ConVar cv_stab_close_enemies {
  {
    .name = "stab_close_enemies",
    .value = "1",
    .info = "Enables or disables the bot's ability to stab the enemy with the knife if the bot is in good condition.",
    .bounded = true,
  }
};

inline ConVar cv_use_engine_pvs_check {
  {
    .name = "use_engine_pvs_check",
    .value = "1",
    .info = "Uses the engine to check the potential visibility of an enemy.",
    .bounded = true,
  }
};

inline ConVar cv_use_hitbox_enemy_targeting {
  {
    .name = "use_hitbox_enemy_targeting",
    .value = "0",
    .info = "Uses hitbox-based enemy targeting, instead of offset-based. Use with yb_use_engine_pvs_check enabled to reduce CPU usage.",
    .bounded = true,
  }
};

inline ConVar cv_aim_trace_consider_glass {
  {
    .name = "aim_trace_consider_glass",
    .value = "0",
    .info = "Bots will consider glass when deciding to shoot enemies. Required for very special maps only.",
    .bounded = true,
  }
};

inline ConVar cv_dont_shoot {
  {
    .name = "dont_shoot",
    .value = "0",
    .info = "If enabled, bots are not allowed to press the attack button, thus forbidding them from shooting.",
    .bounded = true,
  }
};

inline ConVar cv_enable_bullet_spread {
  {
    .name = "enable_bullet_spread",
    .value = "0",
    .info = "Enables or disables bullet spread for bots on servers running without metamod.",
    .bounded = true,
  }
};

// chatlib
inline ConVar cv_chat {
  {
    .name = "chat",
    .value = "1",
    .info = "Enables or disables bot chat functionality.",
    .bounded = true,
  }
};

inline ConVar cv_chat_percent {
  {
    .name = "chat_percent",
    .value = "30",
    .info = "Bot's chance to send random dead chat when killed.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

// tasks
inline ConVar cv_walking_allowed {
  {
    .name = "walking_allowed",
    .value = "1",
    .info = "Specifies whether bots are able to use 'shift' if they think that an enemy is near.",
    .bounded = true,
  }
};

inline ConVar cv_camping_allowed {
  {
    .name = "camping_allowed",
    .value = "1",
    .info = "Allows or disallows bots to camp. Doesn't affect bomb/hostage defending tasks.",
    .bounded = true,
  }
};

inline ConVar cv_camping_time_min {
  {
    .name = "camping_time_min",
    .value = "15.0",
    .info = "Lower bound of time from which the time for camping is calculated.",
    .bounded = true,
    .min = 5.0f,
    .max = 90.0f,
  }
};

inline ConVar cv_camping_time_max {
  {
    .name = "camping_time_max",
    .value = "45.0",
    .info = "Upper bound of time until which the time for camping is calculated.",
    .bounded = true,
    .min = 15.0f,
    .max = 120.0f,
  }
};

inline ConVar cv_random_knife_attacks {
  {
    .name = "random_knife_attacks",
    .value = "1",
    .info = "Allows or disallows the ability for random knife attacks when the bot is rushing and no enemy is nearby.",
    .bounded = true,
  }
};

// graph
inline ConVar cv_graph_fixcamp {
  {
    .name = "graph_fixcamp",
    .value = "0",
    .info = "Specifies whether the bot should not 'fix' camp directions of camp waypoints when loading the old PWF format.",
    .bounded = true,
  }
};

inline ConVar cv_graph_url {
  {
    .name = "graph_url",
    .value = "@github",
    .info = "Specifies the base URL from which bots download a missing graph. The 'graph/<map>.graph' path is appended automatically. Set to empty to disable downloads.\nAllowed values: '@github', '@russia', '@http', or any full base URL.\nIf '@github', CDN raw over https (unavailable on builds without TLS).\nIf '@russia', sourcecraft download over https, for regions with github issues (unavailable on builds without TLS).\nIf '@http' (or '@legacy'), legacy server over plain http.",
  }
};

inline ConVar cv_graph_url_upload {
  {
    .name = "graph_url_upload",
    .value = "@workers",
    .info = "Specifies the base URL to which bots upload graphs. The file is POSTed to the base URL as-is. Set to empty to disable uploads.\nAllowed values: '@workers', '@russia', '@http', or any full base URL.\nIf '@workers', cloudflare worker over https (falls back to '@http' on builds without TLS).\nIf '@russia', yandex worker copy over https (falls back to '@http' on builds without TLS).\nIf '@http' (or '@legacy'), legacy server over plain http.\nAutomatic collection is requested from the server root and requires https, otherwise it's skipped.",
  }
};

inline ConVar cv_graph_auto_save_count {
  {
    .name = "graph_auto_save_count",
    .value = "15",
    .info = "Every N graph nodes placed on the map, the graph will be saved automatically (without checks).",
    .bounded = true,
    .min = 0.0f,
    .max = kMaxNodes,
  }
};

inline ConVar cv_graph_draw_distance {
  {
    .name = "graph_draw_distance",
    .value = "400",
    .info = "Maximum distance to draw graph nodes from the editor viewport.",
    .bounded = true,
    .min = 64.0f,
    .max = 3072.0f,
  }
};

inline ConVar cv_graph_auto_collect_db {
  {
    .name = "graph_auto_collect_db",
    .value = "1",
    .info = "Allows bots to exchange your graph files with the graph database automatically.",
    .bounded = true,
  }
};

// analyze
inline ConVar cv_graph_analyze_auto_start {
  {
    .name = "graph_analyze_auto_start",
    .value = "1",
    .info = "Autostart analyzer if all other cases fail.",
    .bounded = true,
  }
};

inline ConVar cv_graph_analyze_auto_save {
  {
    .name = "graph_analyze_auto_save",
    .value = "1",
    .info = "Auto save results of analysis to graph file and re-add bots.",
    .bounded = true,
  }
};

inline ConVar cv_graph_analyze_distance {
  {
    .name = "graph_analyze_distance",
    .value = "64",
    .info = "The minimum distance to keep nodes from each other.",
    .bounded = true,
    .min = 42.0f,
    .max = 128.0f,
  }
};

inline ConVar cv_graph_analyze_max_jump_height {
  {
    .name = "graph_analyze_max_jump_height",
    .value = "44",
    .info = "Max jump height to test if the next node will be unreachable.",
    .bounded = true,
    .min = 44.0f,
    .max = 64.0f,
  }
};

inline ConVar cv_graph_analyze_slice_ms {
  {
    .name = "graph_analyze_slice_ms",
    .value = "4.0",
    .info = "Max milliseconds of analysis work per server frame. Bounds analysis hitch, prevents hard freezes.",
    .bounded = true,
    .min = 1.0f,
    .max = 25.0f,
  }
};

inline ConVar cv_graph_analyze_on_finish {
  {
    .name = "graph_analyze_on_finish",
    .value = "all",
    .info = "Finish steps as bitmask (1 optimize, 2 clean, 4 goals, 8 camps, 16 teams) or names (optimize,clean,goals,camps,teams,all,none).",
  }
};

// planner
inline ConVar cv_path_heuristic_mode {
  {
    .name = "path_heuristic_mode",
    .value = "0",
    .info = "Selects the heuristic function mode. For debug purposes only.",
    .bounded = true,
    .min = 0.0f,
    .max = 4.0f,
  }
};

inline ConVar cv_path_floyd_memory_limit {
  {
    .name = "path_floyd_memory_limit",
    .value = "6",
    .info = "Limits the maximum Floyd-Warshall memory (megabytes). Uses Dijkstra if memory is exceeded.",
    .bounded = true,
    .min = 0.0,
    .max = 32.0f,
  }
};

inline ConVar cv_path_dijkstra_simple_distance {
  {
    .name = "path_dijkstra_simple_distance",
    .value = "1",
    .info = "Uses simple distance path calculation instead of running a full Dijkstra path cycle. Used only when Floyd matrices are unavailable due to memory limits.",
    .bounded = true,
    .min = 0.0f,
    .max = 1.0f,
  }
};

inline ConVar cv_path_astar_post_smooth {
  {
    .name = "path_astar_post_smooth",
    .value = "0",
    .info = "Enables post-smoothing for A*. Reduces zig-zags on paths at the cost of some CPU cycles.",
    .bounded = true,
  }
};

inline ConVar cv_path_turn_penalty {
  {
    .name = "path_turn_penalty",
    .value = "0.5",
    .info = "Penalty weight for sharp turns in pathfinding (0=off, higher=smoother paths).",
    .bounded = true,
    .min = 0.0f,
    .max = 2.0f,
  }
};

// vision
inline ConVar cv_max_nodes_for_predict {
  {
    .name = "max_nodes_for_predict",
    .value = "22",
    .info = "Maximum number of path nodes to predict the enemy.",
    .bounded = true,
    .min = 15.0f,
    .max = 256.0f,
  }
};

inline ConVar cv_whose_your_daddy {
  {
    .name = "whose_your_daddy",
    .value = "0",
    .info = "Enables or disables extra hard difficulty for bots.",
    .bounded = true,
  }
};

// fakeping
inline ConVar cv_ping_base_min {
  {
    .name = "ping_base_min",
    .value = "5",
    .info = "Lower bound for base bot ping shown in the scoreboard upon creation.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

inline ConVar cv_ping_base_max {
  {
    .name = "ping_base_max",
    .value = "20",
    .info = "Upper bound for base bot ping shown in the scoreboard upon creation.",
    .bounded = true,
    .min = 0.0f,
    .max = 100.0f,
  }
};

inline ConVar cv_ping_count_real_players {
  {
    .name = "ping_count_real_players",
    .value = "1",
    .info = "Count player pings when calculating the average ping for bots. If not, a random ping is chosen for bots.",
    .bounded = true,
  }
};

inline ConVar cv_ping_updater_interval {
  {
    .name = "ping_updater_interval",
    .value = "1.25",
    .info = "Interval at which the fake ping gets updated in the scoreboard.",
    .bounded = true,
    .min = 0.1f,
    .max = 10.0f,
  }
};

// linkage
inline ConVar cv_version {
  {
    .name = "version",
    .value = product.version.chars (),
    .type = Var::ReadOnly,
  }
};

// config
inline ConVar cv_bind_menu_key {
  {
    .name = "bind_menu_key",
    .value = "=",
    .info = "Binds the specified key for opening the bot menu.",
  }
};

inline ConVar cv_ignore_cvars_on_changelevel {
  {
    .name = "ignore_cvars_on_changelevel",
    .value = "yb_quota,yb_autovacate",
    .info = "Specifies a comma separated list of bot cvars that will not be overwritten by the config on changelevel.",
  }
};

// engine
inline ConVar cv_csdm_mode {
  {
    .name = "csdm_mode",
    .value = "0",
    .info = "Enables or disables CSDM / FFA mode for bots.\nAllowed values: '0', '1', '2', '3'.\nIf '0', CSDM / FFA mode is auto-detected.\nIf '1', CSDM / FFA mode is enabled.\nIf '2', CSDM / FFA mode is disabled.\nIf '3', FFA mode is enabled.",
    .bounded = true,
    .min = 0.0f,
    .max = 3.0f,
  }
};

inline ConVar cv_ignore_map_prefix_game_mode {
  {
    .name = "ignore_map_prefix_game_mode",
    .value = "0",
    .info = "If enabled, bots will not apply game modes based on map name prefix (fy_ and ka_ specifically).",
    .bounded = true,
  }
};

inline ConVar cv_threadpool_workers {
  {
    .name = "threadpool_workers",
    .value = "-1",
    .info = "Maximum number of threads the bot will run to process some tasks. -1 means half of the CPU cores are used.",
    .bounded = true,
    .min = -1.0f,
    .max = 128.0f,
  }
};

inline ConVar cv_grenadier_mode {
  {
    .name = "grenadier_mode",
    .value = "0",
    .info = "If enabled, bots will not apply throwing conditions on grenades.",
    .bounded = true,
  }
};

inline ConVar cv_ignore_enemies_after_spawn_time {
  {
    .name = "ignore_enemies_after_spawn_time",
    .value = "0",
    .info = "Makes bots ignore enemies for a specified time in seconds on a new round. Useful for Zombie Plague mods.",
  }
};

inline ConVar cv_breakable_health_limit {
  {
    .name = "breakable_health_limit",
    .value = "500.0",
    .info = "Specifies the maximum health of a breakable object that the bot will consider destroying.",
    .bounded = true,
    .min = 1.0f,
    .max = 3000.0,
  }
};

// navigate
inline ConVar cv_has_team_semiclip {
  {
    .name = "has_team_semiclip",
    .value = "0",
    .info = "When enabled, bots will not try to avoid teammates on their way. Assumes that some semiclip plugins are in use.",
    .bounded = true,
  }
};

inline ConVar cv_graph_slope_height {
  {
    .name = "graph_slope_height",
    .value = "18.0",
    .info = "Determines the maximum slope height change between the current and next node to consider the current link as a jump link. Only for analyzer.",
    .bounded = true,
    .min = 18.0f,
    .max = 45.0f,
  }
};

// control
inline ConVar cv_display_menu_text {
  {
    .name = "display_menu_text",
    .value = "1",
    .info = "Enables or disables display menu text, when players asks for menu. Useful only for Android.",
    .bounded = true,
    .min = 0.0f,
    .max = 1.0f,
    .type = Var::Xash3D,
  }
};

inline ConVar cv_password {
  {
    .name = "password",
    .value = "",
    .info = "The value (password) for the setinfo key. If the user sets the correct password, he gains access to bot commands and menus.",
    .type = Var::Password,
  }
};

inline ConVar cv_password_key {
  {
    .name = "password_key",
    .value = "_ybpw",
    .info = "The name of the setinfo key used to store the password for bot commands and menus.",
  }
};

inline ConVar cv_bots_kill_on_endround {
  {
    .name = "bots_kill_on_endround",
    .value = "0",
    .info = "Allows the use of classic bot kill when issuing the end-round command in menus, instead of the gamedll endround.",
  }
};

// game console variables (references to game/mod cvars)
inline ConVar mp_c4timer {
  {
    .name = "mp_c4timer",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_buytime {
  {
    .name = "mp_buytime",
    .value = nullptr,
    .type = Var::GameRef,
    .reg_missing = true,
    .reg_val = "1",
  }
};

inline ConVar mp_startmoney {
  {
    .name = "mp_startmoney",
    .value = nullptr,
    .type = Var::GameRef,
    .reg_missing = true,
    .reg_val = "800",
  }
};

inline ConVar mp_footsteps {
  {
    .name = "mp_footsteps",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_limitteams {
  {
    .name = "mp_limitteams",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_autoteambalance {
  {
    .name = "mp_autoteambalance",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_roundtime {
  {
    .name = "mp_roundtime",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_timelimit {
  {
    .name = "mp_timelimit",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_freezetime {
  {
    .name = "mp_freezetime",
    .value = nullptr,
    .type = Var::GameRef,
    .reg_missing = true,
    .reg_val = "0",
  }
};

inline ConVar mp_friendlyfire {
  {
    .name = "mp_friendlyfire",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_flashlight {
  {
    .name = "mp_flashlight",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar mp_maxmoney {
  {
    .name = "mp_maxmoney",
    .value = nullptr,
    .type = Var::GameRef,
    .reg_missing = true,
    .reg_val = "16000",
  }
};

inline ConVar sv_gravity {
  {
    .name = "sv_gravity",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar sv_stepsize {
  {
    .name = "sv_stepsize",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar sv_skycolor_r {
  {
    .name = "sv_skycolor_r",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar sv_skycolor_g {
  {
    .name = "sv_skycolor_g",
    .value = nullptr,
    .type = Var::GameRef,
  }
};

inline ConVar sv_skycolor_b {
  {
    .name = "sv_skycolor_b",
    .value = nullptr,
    .type = Var::GameRef,
  }
};
// clang-format on

YSTL_EXPOSE_GLOBAL_SINGLETON (CvarRegistry, cvars);

} // namespace bot
