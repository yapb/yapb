//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

Manager::Manager () {
  // this is a bot manager class constructor

  for (auto &td : team_data_) {
    td.leader_choosen = false;
    td.positive_eco = true;
    td.last_radio_slot = RadioChat::Invalid;
    td.last_radio_timestamp = 0.0f;
  }
  Reset ();

  add_requests_.clear ();
  killer_entity_ = nullptr;
}

Manager::~Manager () {
  // drop the links before the owned bots die with the member below
  bots_.clear ();
}

void Manager::CreateKillerEntity () {
  // this function creates single trigger_hurt for using in bot::kill, to reduce lags, when killing all the bots
  constexpr ystl::Vector kInfiniteOrigin = ystl::Vector (-kInfiniteDistance, -kInfiniteDistance, -kInfiniteDistance);

  killer_entity_ = engfuncs.pfnCreateNamedEntity ("trigger_hurt");

  killer_entity_->v.dmg = kInfiniteDistance;
  killer_entity_->v.dmg_take = 1.0f;
  killer_entity_->v.dmgtime = 2.0f;
  killer_entity_->v.effects |= EF_NODRAW;

  engfuncs.pfnSetOrigin (killer_entity_, kInfiniteOrigin);
  MDLL_Spawn (killer_entity_);
}

void Manager::DestroyKillerEntity () {
  if (!game.IsNullEntity (killer_entity_)) {
    engfuncs.pfnRemoveEntity (killer_entity_);
    killer_entity_ = nullptr;
  }
}

void Manager::TouchKillerEntity (Bot *bot) {

  // bot is already dead
  if (!bot->is_alive_) {
    return;
  }

  if (game.IsNullEntity (killer_entity_)) {
    CreateKillerEntity ();

    if (game.IsNullEntity (killer_entity_)) {
      MDLL_ClientKill (bot->Ent ());
      return;
    }
  }
  const auto &prop = conf.GetWeaponProp (bot->current_weapon_);

  killer_entity_->v.classname = prop.classname.chars ();
  killer_entity_->v.dmg_inflictor = bot->Ent ();
  killer_entity_->v.dmg = (bot->pev->health + bot->pev->armorvalue) * 4.0f;

  KeyValueData kv {};
  kv.szClassName = prop.classname.chars ();
  kv.szKeyName = "damagetype";
  kv.szValue = ystl::strings.format ("%d", ystl::bit (4));
  kv.fHandled = HLFalse;

  MDLL_KeyValue (killer_entity_, &kv);
  MDLL_Touch (killer_entity_, bot->Ent ());
}

void Manager::ExecGameEntity (edict_t *ent) {
  // this function calls gamedll player() function, in case to create player entity in game

  if (game.Is (GameFlags::Metamod)) {
    MUTIL_CallGameEntity (PLID, "player", &ent->v);
    return;
  }

  if (!entlink.CallPlayerFunction (ent)) {
    for (auto &bot : bots_) {
      if (bot.Ent () == ent) {
        bot.Kick ();
        break;
      }
    }
  }
}

void Manager::ForEach (ForEachBot handler) {
  for (auto &bot : bots_) {
    if (handler (&bot)) {
      return;
    }
  }
}

CreateResult Manager::Create (ystl::StringRef name, Difficulty difficulty, Personality personality, Team team, int skin) {
  // prepare bot entity for creation with team, difficulty and name

  edict_t *bot = nullptr;
  ystl::String result_name {};

  // do not allow create bots when there is no graph
  if (!graph.Length ()) {
    ctrl.Msg ("There is no graph found. Cannot create bot.");
    return CreateResult::GraphError;
  }

  // don't allow creating bots with changed graph (distance tables are messed up)
  else if (graph.HasChanged ()) {
    ctrl.Msg ("Graph has been changed. Load graph again...");
    return CreateResult::GraphError;
  }
  else if (team != Team::Invalid && IsTeamStacked (team - 1)) {
    ctrl.Msg ("Desired team is stacked. Unable to proceed with bot creation.");
    return CreateResult::TeamStacked;
  }

  if (difficulty < Difficulty::Noob || difficulty > Difficulty::Expert) {
    difficulty = static_cast<Difficulty> (cv_difficulty.As<int> ());

    if (difficulty < Difficulty::Noob || difficulty > Difficulty::Expert) {
      difficulty = static_cast<Difficulty> (ystl::rg (ystl::to_underlying (Difficulty::Hard), ystl::to_underlying (Difficulty::Expert)));
      cv_difficulty.Set (ystl::to_underlying (difficulty));
    }
  }

  // try to set proffered personality
  static ystl::HashMap<ystl::String, Personality> personality_map {
    { "normal",  Personality::Normal  },
    { "careful", Personality::Careful },
    { "rusher",  Personality::Rusher  },
  };

  // set personality if requested
  if (personality < Personality::Normal || personality > Personality::Careful) {

    // assign preferred if we're forced with cvar
    if (personality_map.exists (cv_preferred_personality.As<ystl::StringRef> ())) {
      personality = personality_map[cv_preferred_personality.As<ystl::StringRef> ()];
    }

    // do a holy random
    else {
      if (ystl::rg.chance (50)) {
        personality = Personality::Normal;
      }
      else {
        if (ystl::rg.chance (50)) {
          personality = Personality::Rusher;
        }
        else {
          personality = Personality::Careful;
        }
      }
    }
  }
  Name *bot_name = nullptr;

  // setup name
  if (name.empty ()) {
    bot_name = conf.PickBotName ();

    if (bot_name) {
      result_name = bot_name->name;
    }
    else {
      result_name.assignf ("%s_%d.%d", product.name_lower, ystl::rg (100, 10000), ystl::rg (100, 10000)); // just pick ugly random name
    }
  }
  else {
    result_name = name;
  }
  const bool has_name_prefix = !cv_name_prefix.As<ystl::StringRef> ().empty ();

  // disable save bots if prefix is enabled
  if (has_name_prefix && cv_save_bots) {
    cv_save_bots.Set (0);
  }

  if (has_name_prefix) {
    ystl::String prefixed {}; // temp buffer for storing modified name
    prefixed.assignf ("%s %s", cv_name_prefix.As<ystl::StringRef> (), result_name);

    // buffer has been modified, copy to real name
    result_name = ystl::move (prefixed);
  }
  bot = game.CreateFakeClient (result_name);

  if (game.IsNullEntity (bot)) {
    return CreateResult::MaxPlayersReached;
  }
  auto object = ystl::make_unique<Bot> (bot, difficulty, personality, team, skin);
  const auto index = object->Index ();

  // seed random number generator
  object->rg.seed (static_cast<uint64_t> (index) + static_cast<uint64_t> (game.Time ()));

  // assign owner of bot name
  if (bot_name != nullptr) {
    bot_name->used_by = index; // save by who name is used
  }
  else {
    conf.SetBotNameUsed (index, result_name);
  }
  bots_by_index_[index] = object.get ();
  owned_bots_.push (ystl::move (object));
  bots_.push_back (*bots_by_index_[index]);

  ctrl.Msg ("Connecting Bot...");

  return CreateResult::Success;
}

Bot *Manager::FindBotByIndex (int index) {
  // this function finds a bot specified by index, and then returns pointer to it (using direct lookup)

  if (index < 0 || index >= kGameMaxPlayers) {
    return nullptr;
  }
  return bots_by_index_[index];
}

Bot *Manager::FindBotByEntity (edict_t *ent) {
  // same as above, but using bot entity

  return FindBotByIndex (game.IndexOfPlayer (ent));
}

Bot *Manager::FindAliveBot () {
  // this function finds one bot, alive bot :)

  for (auto &bot : bots_) {
    if (bot.is_alive_) {
      return &bot;
    }
  }
  return nullptr;
}

void Manager::Frame () {
  // this function calls showframe function for all available at call moment bots

  // run the fun modes stuff
  fun_mode.Update ();

  for (auto &bot : bots_) {
    tickmgr.Frame (&bot);
  }
}

void TickManager::Frame (Bot *bot) {
  // ai runs at think rate, movement commands are capped at the same rate, like official bots do

  if (bot->think_timer_.elapsed ()) {
    bot->think_timer_.start (bot->think_interval_);

    bot->Update ();
  }

  if (bot->command_timer_.elapsed ()) {
    bot->command_timer_.start (bot->think_interval_);

    RunCommand (bot);
  }

  if (bot->slow_frame_timer_.elapsed ()) {
    bot->SlowFrame ();
  }
}

void TickManager::OnBotRound (Bot *bot) {
  auto think_interval = kBotThinkInterval;

  // allow to control bot tick rate via yb_think_fps only if unlocked with custom.cfg
  if (conf.FetchCustom ("UnlockThinkFPS").starts_with ("yes")) {
    think_interval = 1.0f / ystl::clamp (cv_think_fps.As<float> (), 10.0f, 90.0f);
  }

  if (IsFrameSkipDisabled ()) {
    think_interval = 0.0f;
  }
  bot->think_interval_ = think_interval;
  bot->full_think_interval_ = think_interval * (kBotFullThinkInterval / kBotThinkInterval);

  const auto phase = ystl::rg (0.0f, think_interval);

  bot->think_timer_.start (phase);
  bot->command_timer_.start (phase);
  bot->slow_frame_timer_.start (ystl::rg (0.0f, 0.5f));
  bot->heavy_timer_.start (ystl::rg (0.0f, 0.1f));
}

void TickManager::RunCommand (Bot *bot) {
  // issue the single runplayermove for the bot, based on currently calculated inputs

  const auto delta = game.Time () - bot->last_command_time_;

  // detect that the previous command was exhausted before we managed to issue the new one
  if (bot->last_command_msec_ > 0.0f && delta > bot->last_command_msec_ * 0.001f + 0.015f) {
    ++bot->starved_commands_;
  }

  const auto msec_val = ComputeMsec (bot);

  bot->last_command_time_ = game.Time ();
  bot->last_command_msec_ = static_cast<float> (msec_val);

  // flush queued button presses, so they won't be overwritten by engine
  if (bot->pending_buttons_ != 0) {
    bot->pev->button |= bot->pending_buttons_;
    bot->pending_buttons_ = 0;
  }

  bot->TranslateInput ();

  auto input_angles = [bot] () {
    if (bot->IsStuckState () || bot->GetTaskId () == TaskId::Attack || !bot->approaching_ladder_timer_.elapsed ()) {
      return bot->pev->v_angle;
    }
    return bot->move_angles_;
  };

  const auto input_buttons = static_cast<uint16_t> (bot->pev->button);
  const auto input_impulse = static_cast<uint8_t> (bot->pev->impulse);

  engfuncs.pfnRunPlayerMove (bot->Ent (), input_angles (), bot->move_speed_, bot->strafe_speed_, 0.0f, input_buttons, input_impulse, msec_val);

  // save our own copy of old buttons, since bot code is not running every frame now
  bot->old_buttons_ = bot->pev->button;
}

uint8_t TickManager::ComputeMsec (const Bot *bot) const {
  // estimate msec for this command from time since previous command

  const auto delta = game.Time () - bot->last_command_time_;
  const auto msec = static_cast<int32_t> (ystl::roundf (delta * 1000.0f));

  return static_cast<uint8_t> (ystl::clamp (msec, 1, 255));
}

void Manager::Addbot (ystl::StringRef name, Difficulty difficulty, Personality personality, Team team, int skin, bool manual) {
  // this function putting bot creation process to queue to prevent engine crashes

  Request request { .manual = manual, .skin = skin, .team = team, .personality = personality, .difficulty = difficulty, .name = name };

  QueueBotRequest (ystl::move (request));
}

void Manager::QueueBotRequest (Request &&request) {
  // restore bot data from previous level
  if (cv_save_bots && request.name.empty () && !saved_bots_.empty ()) {
    auto saved = saved_bots_.pop_front ();

    request.name = ystl::move (saved.name);

    // if full restore is enabled, restore all parameters
    if (cv_save_bots.As<int> () == 2) {
      request.difficulty = saved.difficulty;
      request.personality = saved.personality;
      request.team = saved.team;
      request.skin = saved.skin;
    }
  }

  // put to queue
  add_requests_.emplace_last (ystl::move (request));
}

void Manager::Addbot (
  ystl::StringRef name, ystl::StringRef difficulty, ystl::StringRef personality, ystl::StringRef team, ystl::StringRef skin, bool manual) {
  // this function is same as the function above, but accept as parameters string instead of integers

  constexpr ystl::StringRef any = "*";

  auto handle_param = [&any] (ystl::StringRef value) {
    return value.empty () || value == any ? -1 : value.as<int> ();
  };

  Request request { .manual = manual,
    .skin = handle_param (skin),
    .team = util.ConvertFromCsTeam (static_cast<CSTeam> (handle_param (team))),
    .personality = static_cast<Personality> (handle_param (personality)),
    .difficulty = static_cast<Difficulty> (handle_param (difficulty)),
    .name = name.empty () || name == any ? ystl::StringRef ("\0") : name };

  QueueBotRequest (ystl::move (request));
}

void Manager::MaintainQuota () {
  // keep bot count up to date and block creation while busy

  if (graph.Length () < 1 || graph.HasChanged ()) {
    if (cv_quota.As<int> () > 0) {
      ctrl.Msg ("There is no graph found. Cannot create bot.");
    }
    cv_quota.Set (0);
    return;
  }

  if (analyzer.IsAnalyzing ()) {
    ctrl.Msg ("Can't create bot during map analysis process.");
    return;
  }
  const int max_clients = game.MaxClients ();
  const int bots_in_game = GetBotCount ();

  // bot's creation update, always process manual requests regardless of hold timer
  if (!add_requests_.empty () && maintain_timer_.elapsed ()) {
    const auto &request = add_requests_.pop_front ();

    // guard manual add against server capacity
    if (request.manual && bots_in_game + GetHumansCount () >= max_clients) {
      ctrl.Msg ("Maximum players reached (%d/%d). Unable to create Bot.", max_clients, max_clients);
      add_requests_.clear ();
      maintain_timer_.start (0.1f);

      return;
    }
    const auto create_result = Create (request.name, request.difficulty, request.personality, request.team, request.skin);

    // only increment quota for manual add on successful creation
    if (request.manual && create_result == CreateResult::Success) {
      cv_quota.Set (ystl::min (cv_quota.As<int> () + 1, max_clients));
    }

    // on failure just clear pending requests, never modify cv_quota
    if (create_result == CreateResult::GraphError) {
      add_requests_.clear (); // something wrong with graph, reset tab of creation
    }
    else if (create_result == CreateResult::MaxPlayersReached) {
      ctrl.Msg ("Maximum players reached (%d/%d). Unable to create Bot.", max_clients, max_clients);
      add_requests_.clear ();
    }
    else if (create_result == CreateResult::TeamStacked) {
      ctrl.Msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and "
                "restart the round)");
      add_requests_.clear ();
    }

    // hold automatic quota management on non-manual failures
    if (create_result != CreateResult::Success && !request.manual) {
      hold_quota_management_timer_.start (5.0f);
    }
    maintain_timer_.start (0.1f);

    // do not process quota management later, if created manually
    if (request.manual && create_result == CreateResult::Success) {
      return;
    }
  }

  // hold timer only gates automatic quota balancing below
  if (!hold_quota_management_timer_.elapsed ()) {
    return;
  }

  // now keep bot number up to date
  if (!quota_maintain_timer_.elapsed ()) {
    return;
  }
  int desired_bot_count = cv_quota.As<int> ();

  // only assign if out of range
  if (desired_bot_count < 0 || desired_bot_count > max_clients) {
    cv_quota.Set (ystl::clamp (desired_bot_count, 0, max_clients));
  }
  const int total_humans_in_game = GetHumansCount ();
  const int human_players_in_game = GetHumansCount (true);

  if (!game.IsDedicatedServer () && !total_humans_in_game) {
    return;
  }

  if (cv_quota_mode.As<ystl::StringRef> () == "fill") {
    desired_bot_count = ystl::max (0, desired_bot_count - human_players_in_game);
  }
  else if (cv_quota_mode.As<ystl::StringRef> () == "match") {
    const int detect_quota_match = cv_quota_match.As<int> () == 0 ? cv_quota.As<int> () : cv_quota_match.As<int> ();

    desired_bot_count = ystl::max (0, detect_quota_match * human_players_in_game);
  }

  if (cv_join_after_player && human_players_in_game == 0) {
    desired_bot_count = 0;
  }

  if (cv_autovacate) {
    const auto keep_slots = cv_autovacate_keep_slots.As<int> ();

    if (cv_kick_after_player_connect) {
      desired_bot_count = ystl::min (desired_bot_count, max_clients - (total_humans_in_game + keep_slots));
    }
    else {
      desired_bot_count = ystl::min (desired_bot_count, max_clients - (human_players_in_game + keep_slots));
    }
  }
  else {
    desired_bot_count = ystl::min (desired_bot_count, max_clients - human_players_in_game);
  }
  auto max_spawn_count = game.GetSpawnCount (Team::Terrorist) + game.GetSpawnCount (Team::CT) - human_players_in_game;

  // if has some custom spawn points, max out spawn point counter
  if (desired_bot_count >= bots_in_game && HasCustomCsdmSpawnEntities ()) {
    max_spawn_count = max_clients + 1;
  }

  // disable spawn control
  if (conf.FetchCustom ("DisableSpawnControl").starts_with ("yes")) {
    max_spawn_count = max_clients + 1;
  }

  // sent message only to console from here
  ctrl.SetFromConsole (true);

  // add bots if necessary
  if (desired_bot_count > bots_in_game && bots_in_game < max_spawn_count) {
    CreateRandom ();
  }
  else if (desired_bot_count < bots_in_game) {
    BalancedKickRandom (false);
  }
  else {
    // clear the saved bots when quota balancing ended
    if (cv_save_bots && !saved_bots_.empty ()) {
      saved_bots_.clear ();
    }
  }
  quota_maintain_timer_.start (0.4f);
}

void Manager::MaintainLeaders () {
  if (game.Is (GameFlags::FreeForAll)) {
    return;
  }

  // select leader each team somewhere in round start
  if (game_state.GetRoundStartTime () + ystl::rg (1.5f, 3.0f) < game.Time ()) {
    for (auto team = Team::Terrorist; team < Team::Num; ++team) {
      SelectLeaders (team, false);
    }
  }
}

void Manager::MaintainRoundRestart () {
  if (!cv_first_human_restart || !game.IsDedicatedServer ()) {
    return;
  }
  const int total_humans = GetHumansCount (true);
  const int total_bots = GetBotCount ();

  if (total_humans > 0 && num_previous_players_ == 0 && total_humans == 1 && total_bots > 0 && !game_state.IsResetHud ()) {

    static ConVarRef sv_restartround ("sv_restartround");

    if (sv_restartround.Exists ()) {
      sv_restartround.Set ("1");
    }
  }
  num_previous_players_ = total_humans;
  game_state.SetResetHud (false);
}

void Manager::MaintainAutoKill () {
  const float kill_delay = cv_autokill_delay.As<float> ();

  if (kill_delay < 1.0f || game_state.IsRoundOver ()) {
    return;
  }

  // check if we're reached the delay, so kill out bots
  if (auto_kill_timer_.started () && auto_kill_timer_.elapsed ()) {
    KillAllBots (Team::Invalid, true);
    auto_kill_timer_.invalidate ();

    return;
  }
  int alive_bots = 0;

  // do not interrupt bomb-defuse scenario
  if (game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted ()) {
    return;
  }
  const int total_humans = GetHumansCount (true); // we're ignore spectators intentionally

  // if we're have no humans in teams do not bother to proceed
  if (!total_humans) {
    return;
  }

  for (const auto &bot : bots_) {
    if (bot.is_alive_) {
      ++alive_bots;

      // do not interrupt assassination scenario, if vip is a bot
      if (game.MapIs (MapFlags::Assassination) && game.IsPlayerVip (bot.Ent ())) {
        return;
      }
    }
  }
  const int alive_humans = GetAliveHumansCount ();

  // check if we're have no alive players and some alive bots, and start autokill timer
  if (!alive_humans && alive_bots > 0 && !auto_kill_timer_.started ()) {
    auto_kill_timer_.start (kill_delay);
  }
}

void Manager::Reset () {
  plant_search_update_timer_.invalidate ();
  last_chat_timer_.invalidate ();
  bomb_say_status_ = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;
}

void Manager::DecrementQuota (int by) {
  if (by != 0) {
    cv_quota.Set (ystl::clamp<int> (cv_quota.As<int> () - by, 0, cv_quota.As<int> ()));
    return;
  }
  cv_quota.Set (0);
}

void Manager::InitQuota () {
  maintain_timer_.start (cv_join_delay.As<float> ());
  quota_maintain_timer_.start (cv_join_delay.As<float> ());

  add_requests_.clear ();
}

void Manager::ServerFill (CSTeam team, Personality personality, Difficulty difficulty, int num_to_add) {
  // this function fill server with bots, with specified team & personality

  // always keep one slot
  const int max_clients = cv_autovacate
                            ? game.MaxClients () - cv_autovacate_keep_slots.As<int> () - (game.IsDedicatedServer () ? 0 : GetHumansCount ())
                            : game.MaxClients ();

  if (GetBotCount () >= max_clients - GetHumansCount ()) {
    return;
  }
  if (team == CSTeam::Terrorist || team == CSTeam::CT) {
    mp_limitteams.Set (0);
    mp_autoteambalance.Set (0);
  }
  else {
    team = CSTeam::Any;
  }
  const auto max_to_add = max_clients - (GetHumansCount () + GetBotCount ());

  constexpr char kTeams[ystl::to_underlying (CSTeam::Any) + 1][12] = {
    "",
    { "Terrorists" },
    { "CTs" },
    "",
    "",
    { "Random" },
  };
  auto to_add = num_to_add == -1 ? max_to_add : num_to_add;

  // limit manually added count as well
  if (to_add > max_to_add - 1) {
    to_add = max_to_add - 1;
  }

  for (int i = 0; i < to_add; ++i) {
    Addbot ("", difficulty, personality, util.ConvertFromCsTeam (team), -1, true);
  }
  ctrl.Msg ("Fill server with %s bots...", &kTeams[ystl::to_underlying (team)][0]);
}

void Manager::KickEveryone (bool instant, bool zero_quota, bool silent) {
  // this function drops all bot clients from server (this function removes only yapb's)

  if (!silent && cv_quota && HasBotsOnline ()) {
    ctrl.Msg ("Bots are removed from server.");
  }

  if (zero_quota) {
    DecrementQuota (0);
  }

  // if everyone is kicked, clear the saved bots
  if (cv_save_bots && !saved_bots_.empty ()) {
    saved_bots_.clear ();
  }

  if (instant) {
    for (auto &bot : bots_) {
      if (!game.IsNullEntity (bot.Ent ())) {
        bot.Kick (true);
      }
    }
  }
  add_requests_.clear ();

  // do not process with quota magement few moments
  if (instant) {
    hold_quota_management_timer_.start (1.0f);
  }
}

void Manager::KickFromTeam (Team team, bool remove_all) {
  // this function remove random bot from specified team (if removeall value = 1 then removes all players from team)

  if (remove_all) {
    const auto &[ts, cts] = CountTeamPlayers ();

    quota_maintain_timer_.start (3.0f);
    add_requests_.clear ();

    if (team == Team::Terrorist) {
      DecrementQuota (ts);
    }
    else {
      DecrementQuota (cts);
    }
  }

  for (auto &bot : bots_) {
    if (team == game.GetRealPlayerTeam (bot.Ent ())) {
      bot.Kick (remove_all);

      if (!remove_all) {
        DecrementQuota ();
        break;
      }
    }
  }
}

void Manager::KillAllBots (Team team, bool silent) {
  // this function kills all bots on server (only this dll controlled bots)

  for (auto &bot : bots_) {
    if (team != Team::Invalid && game.GetRealPlayerTeam (bot.Ent ()) != team) {
      continue;
    }
    bot.Kill ();
  }

  if (!silent) {
    ctrl.Msg ("All bots died...");
  }
}

void Manager::KickBot (int index) {
  auto bot = FindBotByIndex (index);

  if (bot) {
    bot->Kick ();
    DecrementQuota ();
  }
}

bool Manager::KickRandom (bool dec_quota, Team from_team) {
  // this function removes random bot from server (only yapb's)

  // if forteam is unassigned, that means random team
  bool dead_bot_found = false;

  auto update_quota = [&] () {
    if (dec_quota) {
      DecrementQuota ();
    }
  };

  auto belongs_team = [&] (const Bot *bot) {
    if (from_team == Team::Unassigned) {
      return true;
    }
    return game.GetRealPlayerTeam (bot->Ent ()) == from_team;
  };

  // first try to kick the bot that is currently dead
  for (auto &bot : bots_) {

    // is this slot used?
    if (!bot.is_alive_ && belongs_team (&bot)) {
      update_quota ();
      bot.Kick ();

      dead_bot_found = true;
      break;
    }
  }

  if (dead_bot_found) {
    return true;
  }

  // if no dead bots found try to find one with lowest amount of frags
  Bot *selected = nullptr;
  float score = kInfiniteDistance;

  // search bots in this team
  for (auto &bot : bots_) {
    if (bot.pev->frags < score && belongs_team (&bot)) {
      selected = &bot;
      score = bot.pev->frags;
    }
  }

  // if found some bots
  if (selected != nullptr) {
    update_quota ();
    selected->Kick ();

    return true;
  }
  ystl::Array<Bot *> kickable {};

  // worst case, just kick some random bot
  for (auto &bot : bots_) {

    // is this slot used?
    if (belongs_team (&bot)) {
      kickable.push (&bot);
    }
  }

  // kick random from collected
  if (!kickable.empty ()) {
    auto bot = kickable.random ();

    if (bot) {
      update_quota ();
      bot->Kick ();

      return true;
    }
  }
  return false;
}

bool Manager::BalancedKickRandom (bool dec_quota) {
  const auto &[ts, cts] = CountTeamPlayers ();
  bool is_kicked = false;

  if (ts > cts) {
    is_kicked = KickRandom (dec_quota, Team::Terrorist);
  }
  else if (ts < cts) {
    is_kicked = KickRandom (dec_quota, Team::CT);
  }
  else {
    is_kicked = KickRandom (dec_quota, Team::Unassigned);
  }

  // if we can't kick player from correct team, just kick any random to keep quota control work
  if (!is_kicked) {
    is_kicked = KickRandom (dec_quota, Team::Unassigned);
  }
  return is_kicked;
}

bool Manager::HasCustomCsdmSpawnEntities () {
  if (!game.Is (GameFlags::CSDM | GameFlags::FreeForAll)) {
    return false;
  }
  auto custom_spawn_class = conf.FetchCustom ("CustomCSDMSpawnPoint");

  // check for custom entity
  return game.HasEntityInGame (custom_spawn_class);
}

void Manager::SetLastWinner (Team winner) {
  last_winner_ = winner;
  game_state.SetRoundOver (true);

  if (cv_radio_mode.As<int> () != 2) {
    return;
  }
  auto notify = FindAliveBot ();

  if (notify) {
    if (notify->team_ == winner) {
      if (game_state.GetRoundMidTime () > game.Time ()) {
        notify->PushRadioChat (RadioChat::QuickWonRound);
      }
      else {
        notify->PushRadioChat (RadioChat::WonTheRound);
      }
    }
  }
}

void Manager::CheckBotModel (edict_t *ent, char *infobuffer) {
  for (auto &bot : bots) {
    if (bot.Ent () == ent) {
      bot.RefreshCreatureStatus (infobuffer);
      break;
    }
  }
}

void Manager::CheckNeedsToBeKicked () {
  // kick bots leaving on pathfinding error and hold quota management

  for (auto &bot : bots) {
    if (bot.kick_me_from_server_) {
      hold_quota_management_timer_.start (10.0f);
      add_requests_.clear ();

      bot.Kick (); // kick bot from server if requested
    }
  }
}

void Manager::RefreshCreatureStatus () {
  if (!game.Is (GameFlags::ZombieMod)) {
    return;
  }

  for (auto &bot : bots) {
    if (bot.is_alive_) {
      bot.RefreshCreatureStatus (nullptr);
    }
  }
}

void Manager::SetWeaponMode (int selection) {
  // this function sets bots weapon mode

  selection--;

  if (selection < 0 || selection > 6) {
    selection = 6; // default to standard
  }

  // weapon team abbreviations for readability
  constexpr auto n = WeaponTeam::None;
  constexpr auto t = WeaponTeam::Terrorist;
  constexpr auto c = WeaponTeam::CT;
  constexpr auto b = WeaponTeam::Both;

  constexpr WeaponTeam kStdMaps[7][kNumWeapons] = {
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // knife only
    { n, n, n, c, c, t, t, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // pistols only
    { n, n, n, n, n, n, n, c, c, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // shotgun only
    { n, n, n, n, n, n, n, n, n, b, c, b, t, b, n, n, n, n, n, n, n, n, n, n, b, n }, // machine guns only
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, t, t, c, t, c, c, n, n, n, n, n, n }, // rifles only
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, c, c, t, b, n }, // snipers only
    { n, n, n, b, b, t, c, b, b, b, c, b, t, b, t, t, c, t, c, c, b, b, t, c, b, c }  // standard
  };

  constexpr WeaponTeam kAsMaps[7][kNumWeapons] = {
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // knife only
    { n, n, n, c, c, t, t, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // pistols only
    { n, n, n, n, n, n, n, t, t, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n }, // shotgun only
    { n, n, n, n, n, n, n, n, n, c, t, c, b, c, n, n, n, n, n, n, n, n, n, n, t, n }, // machine guns only
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, t, n, c, t, b, b, n, n, n, n, n, n }, // rifles only
    { n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, n, t, t, n, b, n }, // snipers only
    { n, n, n, c, c, t, c, t, t, c, t, c, b, c, t, n, c, t, b, b, t, t, n, t, b, b }  // standard
  };

  constexpr char kModes[7][12] = { { "Knife" }, { "Pistol" }, { "Shotgun" }, { "Machine Gun" }, { "Rifle" }, { "Sniper" }, { "Standard" } };

  // get the weapons array
  auto &tab = conf.GetWeapons ();

  // set the correct weapon mode
  for (int i = 0; i < kNumWeapons; ++i) {
    tab[i].team_standard = kStdMaps[selection][i];
    tab[i].team_as = kAsMaps[selection][i];
  }
  cv_jasonmode.Set (selection == 0 ? 1 : 0);

  ctrl.Msg ("%s weapon mode selected.", &kModes[selection][0]);
}

void Manager::ListBots () {
  // this function list's bots currently playing on the server

  ctrl.Msg ("%-3.5s\t%-19.16s\t%-10.12s\t%-3.4s\t%-3.4s\t%-3.4s\t%-3.6s\t%-3.5s\t%-3.8s", "index", "name", "personality", "team", "difficulty",
    "frags", "deaths", "alive", "timeleft");

  auto bot_team = [] (edict_t *ent) -> ystl::StringRef {
    const auto team = game.GetRealPlayerTeam (ent);

    switch (team) {
    case Team::CT:
      return "CT";

    case Team::Terrorist:
      return "TE";

    case Team::Unassigned:
    default:
      return "UN";

    case Team::Spectator:
      return "SP";
    }
  };

  for (auto &bot : bots) {
    auto timelimit_str = cv_rotate_bots ? ystl::strings.format ("%-3.0f secs", bot.stay_timer_.remaining_time ()) : "unlimited";

    ctrl.Msg ("[%-2.1d]\t%-22.16s\t%-10.12s\t%-3.4s\t%-3.1d\t%-3.1d\t%-3.1d\t%-3.4s\t%s", bot.Index (), bot.pev->netname.chars (),
      bot.personality_ == Personality::Rusher   ? "rusher"
      : bot.personality_ == Personality::Normal ? "normal"
                                                : "careful",

      bot_team (bot.Ent ()), bot.difficulty_, static_cast<int> (bot.pev->frags),

      bot.death_count_, bot.is_alive_ ? "yes" : "no",

      timelimit_str);
  }
  ctrl.Msg ("%d bots", bots_.size ());
}

float Manager::GetConnectionTimes (ystl::StringRef name, float original) {
  // this function get's fake bot player time

  for (auto &bot : bots_) {
    if (name.starts_with (bot.pev->netname.chars ())) {
      return bot.GetConnectionTime ();
    }
  }
  return original;
}

float Manager::GetAverageTeamKpd (bool calc_for_bots) {
  ystl::Twin<float, int32_t> calc {};

  for (const auto &client : clients) {
    if (!client.IsUsed ()) {
      continue;
    }
    auto bot = bots[client.ent];

    if (calc_for_bots && bot) {
      calc.first += client.ent->v.frags;
      calc.second += ystl::max (static_cast<int32_t> (bot->death_count_), 1);
    }
    else if (!calc_for_bots && !bot) {
      calc.first += client.ent->v.frags;
      calc.second++;
    }
  }

  if (calc.second > 0) {
    return calc.first / static_cast<float> (calc.second);
  }
  return 0.0f;
}

ystl::Twin<int, int> Manager::CountTeamPlayers () {
  int ts = 0, cts = 0;

  for (const auto &client : clients) {
    if (client.IsUsed ()) {
      if (client.team2 == Team::Terrorist) {
        ++ts;
      }
      else if (client.team2 == Team::CT) {
        ++cts;
      }
    }
  }
  return { ts, cts };
}

Bot *Manager::FindHighestFragBot (Team team) {
  ystl::Twin<int32_t, float> best { -1, 0.0f };

  // search bots in this team
  for (const auto &bot : bots) {
    if (bot.is_alive_ && game.GetRealPlayerTeam (bot.Ent ()) == team) {
      if (best.first == -1 || bot.pev->frags > best.second) {
        best.first = bot.Index ();
        best.second = bot.pev->frags;
      }
    }
  }
  return best.first == -1 ? nullptr : FindBotByIndex (best.first);
}

void Manager::UpdateTeamEconomics (Team team, bool set_true) {
  // decide if team can buy primaries from poor player share

  auto &eco_status = team_data_[team].positive_eco;

  if (set_true || !cv_economics_rounds) {
    eco_status = true;
    return; // don't check economics while economics disable
  }
  int num_poor_players = 0;
  int num_team_players = 0;

  // start calculating
  for (const auto &bot : bots_) {
    if (bot.team_ == team) {
      if (bot.money_amount_ <= conf.GetEconLimit (EcoLimit::PrimaryGreater)) {
        ++num_poor_players;
      }
      ++num_team_players; // update count of team
    }
  }
  eco_status = true;

  if (num_team_players <= 1) {
    return;
  }
  // if 80 percent of team have no enough money to purchase primary weapon
  if ((num_team_players * 80) / 100 <= num_poor_players) {
    eco_status = false;
  }

  // winner must buy something!
  if (last_winner_ == team) {
    eco_status = true;
  }
}

void Manager::UpdateBotDifficulties () {
  // if min/max difficulty is specified  this should not have effect
  if (cv_difficulty_min.As<int> () != ystl::to_underlying (Difficulty::Invalid) ||
      cv_difficulty_max.As<int> () != ystl::to_underlying (Difficulty::Invalid) || cv_difficulty_auto) {
    return;
  }
  const auto difficulty = static_cast<Difficulty> (cv_difficulty.As<int> ());

  if (difficulty != last_difficulty_) {

    // sets new difficulty for all bots
    for (auto &bot : bots_) {
      bot.SetNewDifficulty (difficulty);
    }
    last_difficulty_ = difficulty;
  }
}

void Manager::BalanceBotDifficulties () {
  // difficulty changing once per round (time)
  auto update_difficulty = [] (Bot *bot, int32_t offset) {
    bot->SetNewDifficulty (ystl::clamp (bot->difficulty_ + offset, Difficulty::Noob, Difficulty::Expert));
  };

  // with nightmare difficulty, there is no balance
  if (cv_whose_your_daddy) {
    return;
  }
  const int auto_mode = cv_difficulty_auto.As<int> ();

  if (auto_mode && difficulty_balance_timer_.elapsed ()) {
    // determine which teams have human players
    bool has_human_t = false;
    bool has_human_ct = false;

    for (const auto &client : clients) {
      if (!client.IsUsed ()) {
        continue;
      }
      // check if this is a human player (not a bot)
      if (client.IsHuman ()) {
        if (client.team2 == Team::Terrorist) {
          has_human_t = true;
        }
        else if (client.team2 == Team::CT) {
          has_human_ct = true;
        }
      }
    }

    // mode 2: only balance bots in team with humans
    const bool human_team_only = (auto_mode == 2);

    // calculate team-specific kpd ratios
    const auto ratio_player = GetAverageTeamKpd (false);
    const auto ratio_bots = GetAverageTeamKpd (true);

    // calculate for each the bot
    for (auto &bot : bots_) {
      // in human-team-only mode, skip bots on teams without humans
      if (human_team_only) {
        const bool bot_on_t = (bot.team_ == Team::Terrorist);
        const bool bot_on_ct = (bot.team_ == Team::CT);

        // skip bots on teams that have no human players
        if ((bot_on_t && !has_human_t) || (bot_on_ct && !has_human_ct)) {
          continue;
        }
      }
      const float score = bot.kpd_ratio_;

      // if kd ratio is going to go to low, we need to try to set higher difficulty
      if (score < 0.7f || (score <= 1.05f && ratio_bots < ratio_player)) {
        update_difficulty (&bot, +1);
      }
      else if (score > 4.0f || (score >= 2.65f && ratio_bots > ratio_player)) {
        update_difficulty (&bot, -1);
      }
    }
    difficulty_balance_timer_.start (cv_difficulty_auto_balance_interval.As<float> ());
  }
}

void Manager::Destroy () {
  // this function free all bots slots (used on server shutdown)

  hold_quota_management_timer_.invalidate (); // restart quota manager
  last_winner_ = Team::Invalid; // no winner on fresh map / shutdown

  for (auto &bot : bots_) {
    bot.MarkStale ();
  }
  bots_.clear ();
  owned_bots_.clear ();

  ystl::memzero (bots_by_index_, sizeof (bots_by_index_));
}

Bot::Bot (edict_t *bot, Difficulty difficulty, Personality personality, Team team, int skin) {
  // this function does core operation of creating bot, it's called by addbot, when bot setup completed,

  const int client_index = game.IndexOfEntity (bot);
  pev = &bot->v;

  // create the player entity by calling mod's player function
  bots.ExecGameEntity (bot);

  // set all info buffer keys for this bot
  auto buffer = engfuncs.pfnGetInfoKeyBuffer (bot);

  engfuncs.pfnSetClientKeyValue (client_index, buffer, "_vgui_menus", "0");
  engfuncs.pfnSetClientKeyValue (client_index, buffer, "_ah", "0");

  if (!game.Is (GameFlags::Legacy)) {
    if (cv_show_latency.As<int> () == 1) {
      engfuncs.pfnSetClientKeyValue (client_index, buffer, "*bot", "1");
    }
    const auto &avatar = conf.GetRandomAvatar ();

    if (cv_show_avatars && !avatar.empty ()) {
      engfuncs.pfnSetClientKeyValue (client_index, buffer, "*sid", avatar.chars ());
    }
  }

  char reject[128] = {
    0,
  };
  MDLL_ClientConnect (bot, bot->v.netname.chars (), ystl::strings.format ("127.0.0.%d", client_index + 100), reject);

  if (!ystl::strings.is_empty (reject)) {
    ystl::logger.error ("Server refused '%s' connection (%s)", bot->v.netname.chars (), reject);

    // kick the bot player if the server refused it
    if (!rehlds.DropClient (game.IndexOfEntity (bot), reject)) {
      game.ServerCommand ("kick \"%s\"", bot->v.netname.chars ());
    }

    bot->v.flags |= FL_KILLME;
    return;
  }

  MDLL_ClientPutInServer (bot);
  bot->v.flags |= FL_CLIENT | FL_FAKECLIENT; // set this player as fake client

  // initialize all the variables for this bot
  not_started_ = true; // hasn't joined game yet
  force_radio_ = false;

  index_ = client_index - 1;
  start_action_ = Msg::None;
  retry_join_ = 0;
  money_amount_ = 0;
  logo_decal_index_ = conf.GetRandomLogoDecalIndex ();

  if (cv_rotate_bots) {
    stay_timer_.start (rg (cv_rotate_stay_min.As<float> (), cv_rotate_stay_max.As<float> ()));
  }
  else {
    stay_timer_.start (kInfiniteDistance);
  }

  // assign how talkative this bot will be
  say_text_buffer_.chat_delay = rg (3.8f, 10.0f);
  say_text_buffer_.chat_probability = rg (50, 100); // high floor, so keyword replies trigger often

  is_alive_ = false;
  weapon_burst_mode_ = BurstMode::Off;
  SetNewDifficulty (ystl::clamp (static_cast<Difficulty> (difficulty), Difficulty::Noob, Difficulty::Expert));

  auto min_difficulty = cv_difficulty_min.As<int> ();
  auto max_difficulty = cv_difficulty_max.As<int> ();

  // if we're have min/max difficulty specified, choose value from they
  if (min_difficulty != ystl::to_underlying (Difficulty::Invalid) && max_difficulty != ystl::to_underlying (Difficulty::Invalid)) {
    if (min_difficulty > max_difficulty) {
      ystl::swap (max_difficulty, min_difficulty);
    }
    SetNewDifficulty (static_cast<Difficulty> (rg (min_difficulty, max_difficulty)));
  }
  ping_base_ = fakeping.RandomBase ();
  ping_ = fakeping.RandomBase ();

  previous_think_time_ = game.Time () - 0.1f;
  frame_interval_ = 0.1f;
  last_command_time_ = game.Time ();
  kpd_ratio_ = 0.0f;
  death_count_ = 0;

  ignored_items_.reserve (64);
  chat_buffer_.reserve (256);

  // stuff from jk_botti
  play_server_time_ = 60.0f * rg (30.0f, 240.0f);
  join_server_time_ = ystl::plat.seconds () - play_server_time_ * ystl::rg (0.2f, 0.8f);

  switch (personality) {
  case Personality::Rusher:
    personality_ = Personality::Rusher;
    base_agression_level_ = rg (0.7f, 1.0f);
    base_fear_level_ = rg (0.0f, 0.4f);
    break;

  case Personality::Careful:
    personality_ = Personality::Careful;
    base_agression_level_ = rg (0.2f, 0.5f);
    base_fear_level_ = rg (0.7f, 1.0f);
    break;

  default:
    personality_ = Personality::Normal;
    base_agression_level_ = rg (0.4f, 0.7f);
    base_fear_level_ = rg (0.4f, 0.7f);
    break;
  }
  ClearAmmoInfo ();

  current_weapon_ = Weapon::Invalid; // current weapon is not assigned at start
  weapon_type_ = WeaponType::None; // current weapon type is not assigned at start

  voice_pitch_ = rg (85, 115); // assign voice pitch

  // copy them over to the temp level variables
  agression_level_ = base_agression_level_;
  fear_level_ = base_fear_level_;
  emotion_update_timer_.start (0.5f);
  health_value_ = bot->v.health;

  // just to be sure
  msg_queue_.clear ();

  // init async planner
  planner_ = ystl::make_unique<AStarAlgo> (graph.Length ());

  // init path walker; a path can visit every node, so size it for the whole graph
  path_walk_.Init (static_cast<size_t> (graph.Length ()));
  build_path_.Init (static_cast<size_t> (graph.Length ()));

  // presize the goal history, so it never grows during the bot's lifetime
  goal_history_.reserve (kMaxNodeLinks);

  // presize the last used chat sentences, so the first chat doesn't grow the array
  say_text_buffer_.last_used_sentences.reserve (8);

  // presize the chat reply buffer, so keyword replies never grow it during the game
  reply_buffer_.reserve (255);

  // presize the message queue, so the first queued messages don't grow the deque
  msg_queue_.reserve (8);

  // init player models parts enumerator
  if (cv_use_hitbox_enemy_targeting) {
    hitbox_enumerator_ = ystl::make_unique<PlayerHitboxEnumerator> ();
  }

  // bot is not kicked by rotation
  kicked_by_rotation_ = false;

  // assign team and class
  wanted_team_ = util.ConvertFromCsTeam (team);
  wanted_skin_ = skin;

  tasks_.Reserve (ystl::to_underlying (TaskId::Num));

  NewRound ();
}

void Bot::ClearAmmoInfo () {
  ystl::memzero (&ammo_in_clip_, sizeof (ammo_in_clip_));
  ystl::memzero (&ammo_, sizeof (ammo_));
}

float Bot::GetConnectionTime () {
  const auto current = ystl::plat.seconds ();

  if (current - join_server_time_ > play_server_time_ || current - join_server_time_ <= 0.0f) {
    play_server_time_ = 60.0f * rg (30.0f, 240.0f);
    join_server_time_ = current - play_server_time_ * rg (0.2f, 0.8f);
  }
  return current - join_server_time_;
}

int Manager::GetHumansCount (bool ignore_spectators) {
  // this function returns number of humans playing on the server

  int count = 0;

  for (const auto &client : clients) {
    if (client.IsUsed () && client.IsHuman ()) {
      if (ignore_spectators && client.team2 != Team::Terrorist && client.team2 != Team::CT) {
        continue;
      }
      ++count;
    }
  }
  return count;
}

int Manager::GetAliveHumansCount () {
  // this function returns number of humans playing on the server

  int count = 0;

  for (const auto &client : clients) {
    if (client.IsUsedAndAlive () && client.IsHuman ()) {
      ++count;
    }
  }
  return count;
}

int Manager::GetPlayerPriority (edict_t *ent) {
  constexpr auto kHighPriority = 1024;

  // always check for only our own bots
  auto bot = bots[ent];

  // if player just return high prio
  if (!bot) {
    return game.IndexOfEntity (ent) + kHighPriority * 2;
  }

  // give bots some priority
  if (bot->has_c4_ || bot->is_vip_ || bot->has_hostage_ || has_flag (bot->current_travel_flags_, PathFlag::Jump)) {
    return bot->Entindex () + kHighPriority;
  }
  const auto task = bot->GetTaskId ();

  // higher priority if important task
  if (task == TaskId::MoveTo || task == TaskId::SeekCover || task == TaskId::Camp || task == TaskId::Hide) {

    return bot->Entindex () + kHighPriority;
  }
  return bot->Entindex ();
}

bool Manager::IsTeamStacked (Team team) {
  if (team != Team::CT && team != Team::Terrorist) {
    return false;
  }
  const int limit_teams = mp_limitteams.As<int> ();

  if (!limit_teams) {
    return false;
  }
  ystl::FixedArray<int32_t, ystl::to_underlying (Team::Num)> team_counts = { 0 };

  for (const auto &client : clients) {
    if (client.IsUsed () && client.team2 != Team::Unassigned && client.team2 != Team::Spectator) {
      ++team_counts[client.team2];
    }
  }
  return team_counts[team] + 1 > team_counts[team == Team::CT ? Team::Terrorist : Team::CT] + limit_teams;
}

void Manager::DisconnectBot (Bot *bot) {
  if (bot == nullptr) [[unlikely]] {
    return;
  }
  bots_by_index_[bot->index_] = nullptr;
  bot->MarkStale ();

  if (!bot->kicked_by_rotation_ && cv_save_bots) {
    Request saved {};
    saved.name = bot->pev->netname.str ();

    // if full restore is enabled, save all parameters
    if (cv_save_bots.As<int> () == 2) {
      saved.difficulty = bot->difficulty_;
      saved.personality = bot->personality_;
      saved.team = bot->team_;
      saved.skin = bot->wanted_skin_;
    }
    saved_bots_.emplace_last (ystl::move (saved));
  }

  // unlink from the live set first, the owned object dies with the array below
  bots_.unlink (*bot);

  for (size_t i = 0; i < owned_bots_.size (); ++i) {
    if (owned_bots_[i].get () == bot) {
      owned_bots_.erase (i, 1);
      break;
    }
  }
}

void Manager::HandleDeath (edict_t *killer, edict_t *victim) {
  const auto killer_team = game.GetRealPlayerTeam (killer);
  const auto victim_team = game.GetRealPlayerTeam (victim);

  if (cv_radio_mode.As<int> () == 2) {
    // need to send congrats on well placed shot
    for (auto &notify : bots) {
      if (notify.is_alive_ && killer_team == notify.team_ && killer_team != victim_team && killer != notify.Ent () &&
          notify.SeesEntity (victim->v.origin)) {

        if (!(killer->v.flags & FL_FAKECLIENT)) {
          notify.PushRadioChat (RadioChat::NiceShotCommander);
        }
        else if (ystl::rg.chance (notify.radio_percent_) && ystl::rg.chance (50)) {
          notify.PushRadioChat (RadioChat::NiceShotPall);
        }
        break;
      }
    }
  }
  Bot *killer_bot = nullptr;
  Bot *victim_bot = nullptr;

  // notice nearby to victim teammates, that attacker is near
  for (auto &notify : bots) {
    if (notify.difficulty_ >= Difficulty::Hard && killer_team != victim_team && notify.see_enemy_timer_.greater_than (2.0f) &&
        notify.is_alive_ && notify.team_ == victim_team && game.IsNullEntity (notify.enemy_) && game.IsNullEntity (notify.last_enemy_) &&
        util.IsVisible (killer->v.origin, notify.Ent ())) {

      // make bot look at last enemy position
      notify.actual_reaction_time_ = 0.0f;
      notify.see_enemy_timer_.start ();
      notify.enemy_ = killer;
      notify.last_enemy_ = killer;
      notify.last_enemy_origin_ = killer->v.origin;
    }

    if (notify.Ent () == killer) {
      killer_bot = &notify;
    }
    else if (notify.Ent () == victim) {
      victim_bot = &notify;
    }
  }

  // is this message about a bot who killed somebody?
  if (killer_bot != nullptr) {
    killer_bot->SetLastVictim (victim);
  }

  // mark bot as "spawned", and reset it to new-round state when it dead (for csdm/zombie only)
  if (victim_bot != nullptr) {
    victim_bot->Spawned ();

    victim_bot->is_alive_ = false;

    // clear victim as last enemy for all bots
    for (auto &bot : bots) {
      auto ve = victim_bot->Ent ();

      if (bot.enemy_ == ve || bot.last_enemy_ == ve) {
        bot.enemy_ = nullptr;
        bot.enemy_origin_.clear ();

        bot.last_enemy_ = nullptr;
        bot.last_enemy_origin_.clear ();
      }
    }
    victim_bot->hostages_.clear ();
  }

  // did a human kill a bot on his team?
  else {
    if (victim_bot != nullptr) {
      if (killer_team == victim_bot->team_) {
        victim_bot->vote_kick_index_ = game.IndexOfEntity (killer);

        for (auto &notify : bots) {
          if (notify.SeesEntity (victim->v.origin)) {
            notify.PushRadioChat (RadioChat::TeamKill);
          }
        }
      }
      victim_bot->is_alive_ = false;
    }
  }
}

void Bot::NewRound () {
  // this function initializes a bot after creation & at the start of each round

  // delete all allocated path nodes
  ClearSearchNodes ();

  path_origin_.clear ();
  dest_origin_.clear ();

  path_ = nullptr;
  current_travel_flags_ = PathFlag::None;
  desired_velocity_.clear ();
  current_node_index_ = kInvalidNodeIndex;
  prev_goal_index_ = kInvalidNodeIndex;
  chosen_goal_index_ = kInvalidNodeIndex;
  loosed_bomb_node_index_ = kInvalidNodeIndex;
  planted_bomb_node_index_ = kInvalidNodeIndex;

  move_to_c4_ = false;
  defuse_notified_ = false;
  duck_defuse_ = false;
  duck_defuse_check_timer_.invalidate ();
  near_bomb_timer_.invalidate ();
  defuse_watch_timer_.invalidate ();
  debug_update_timer_.invalidate ();
  last_damage_timestamp_ = 0.0f;

  num_friends_left_ = 0;
  num_enemies_left_ = 0;
  old_buttons_ = pev->button;
  rechoice_goal_count_ = 0;

  ystl::fill (previous_nodes_, kInvalidNodeIndex);
  nav_timer_.start ();
  team_ = game.GetPlayerTeam (Ent ());

  ResetPathSearchType ();

  // clear all states & tasks
  states_ = Sense::Invalid;
  ClearTasks ();

  is_leader_ = false;
  has_progress_bar_ = false;
  can_set_aim_direction_ = true;

  team_order_timer_.invalidate ();
  ask_check_timer_.start (rg (30.0f, 90.0f));
  min_speed_ = 260.0f;
  prev_speed_ = 0.0f;
  prev_origin_ = ystl::Vector (kInfiniteDistance, kInfiniteDistance, kInfiniteDistance);
  prev_timer_.start (0.0f);
  look_update_time_ = game.Time ();
  look_yaw_.Reset ();
  look_pitch_.Reset ();
  ideal_angles_ = pev->v_angle;

  change_view_timer_.start (rg.chance (25) ? mp_freezetime.As<float> () : 0.0f);
  aim_error_timer_.start (0.0f);

  view_distance_ = Frustum::kMaxViewDistance;
  max_view_distance_ = Frustum::kMaxViewDistance;

  lift_entity_ = nullptr;
  pickup_item_ = nullptr;
  item_check_timer_.invalidate ();
  no_ammo_pickup_timer_.invalidate ();
  ignored_items_.clear ();

  breakable_entity_ = nullptr;
  breakable_origin_.clear ();
  breakable_shoot_timer_.invalidate ();
  last_breakable_ = nullptr;

  door_open_timer_.invalidate ();
  door_hit_timer_.invalidate ();

  for (auto &fall : fall_down_point_) {
    fall.clear ();
  }
  is_fall_down_ = false;

  ResetCollision ();
  ResetDoubleJump ();

  enemy_ = nullptr;
  last_victim_ = nullptr;
  last_enemy_ = nullptr;
  last_enemy_origin_.clear ();
  last_victim_origin_.clear ();
  tracking_edict_ = nullptr;
  nav_look_at_.clear ();
  nav_look_at_node_ = kInvalidNodeIndex;
  vertical_move_hold_.invalidate ();
  danger_glance_node_ = kInvalidNodeIndex;
  danger_glance_.invalidate ();
  danger_glance_gap_.invalidate ();

  // clear stale look targets from previous round, else during freezetime bot aims at outdated position
  look_at_.clear ();
  look_at_safe_.clear ();
  look_at_predict_.clear ();

  entity_.clear ();
  enemy_body_part_set_ = nullptr;
  next_tracking_timer_.invalidate ();
  last_predict_index_ = kInvalidNodeIndex;
  last_predict_length_ = kInfiniteDistanceLong;
  predict_cache_ = {};

  button_push_timer_.invalidate ();
  enemy_update_timer_.invalidate ();
  see_enemy_timer_.invalidate ();
  shoot_at_dead_timer_.invalidate ();
  old_combat_desire_ = 0.0f;
  lift_usage_timer_.invalidate ();
  breakable_timer_.invalidate ();

  cover_search_timer_.invalidate ();
  predict_enqueue_timer_.invalidate ();
  thru_wall_hold_timer_.invalidate ();
  thru_wall_reroll_timer_.invalidate ();
  penetration_check_timer_.invalidate ();
  penetration_check_origin_.clear ();
  penetration_result_ = false;

  dark_area_check_timer_.invalidate ();
  dark_area_check_enemy_ = nullptr;
  dark_area_check_origin_.clear ();
  dark_area_result_ = false;

  avoid_grenade_ = nullptr;
  need_avoid_grenade_ = 0;

  last_damage_type_ = -1;
  vote_kick_index_ = 0;
  last_vote_kick_ = 0;
  vote_map_ = 0;
  try_open_door_ = 0;

  aim_flags_ = AimFlags::Invalid;
  lift_state_ = LiftState::None;

  aim_last_error_.clear ();
  position_.clear ();
  lift_travel_pos_.clear ();

  SetIdealReactionTimers (true);

  target_entity_ = nullptr;
  follow_wait_timer_.invalidate ();

  hostages_.clear ();

  if (cv_use_hitbox_enemy_targeting) {
    if (hitbox_enumerator_) {
      hitbox_enumerator_->Reset ();
    }
    else {
      hitbox_enumerator_ = ystl::make_unique<PlayerHitboxEnumerator> ();
    }
  }
  ShowChatterIcon (false);

  approaching_ladder_timer_.invalidate ();
  forget_last_victim_timer_.invalidate ();
  lost_reachable_node_timer_.invalidate ();
  fix_fall_timer_.invalidate ();
  repath_timer_.invalidate ();

  ystl::fill (chatter_times_, kMaxChatterRepeatInterval);
  RefreshCreatureStatus (nullptr);

  reload_data_.Reset ();
  shoot_time_ = game.Time ();
  fire_pause_ = 0.0f;
  last_fired_timer_.invalidate ();

  sniper_stop_timer_.invalidate ();
  last_weapon_switch_timer_.invalidate ();
  grenade_check_timer_.invalidate ();
  is_using_grenade_ = false;
  bomb_search_overridden_ = false;
  fire_hurts_friend_ = false;

  blind_button_ = 0;
  blind_timer_.invalidate ();
  avoid_flash_timer_.invalidate ();
  avoid_flash_ent_ = nullptr;
  jump_time_ = 0.0f;
  jump_finished_ = false;
  jump_knife_drawn_ = false;
  jump_knife_restore_timer_.invalidate ();
  stuck_timer_.invalidate ();

  say_text_buffer_.time_next_chat = game.Time ();
  say_text_buffer_.entity_index = -1;
  say_text_buffer_.say_text.clear ();

  buy_state_ = BuyState::PrimaryWeapon;
  buy_context_ = BuyContext::None;

  last_equip_timer_.invalidate ();

  // setup radio percent each round
  const auto bad_morale = fear_level_ > agression_level_ ? rg.chance (75) : rg.chance (35);

  switch (personality_) {
  case Personality::Normal:
  default:
    radio_percent_ = bad_morale ? rg (50, 75) : rg (25, 50);
    break;

  case Personality::Rusher:
    radio_percent_ = bad_morale ? rg (35, 50) : rg (15, 35);
    break;

  case Personality::Careful:
    radio_percent_ = bad_morale ? rg (70, 90) : rg (50, 70);
    break;
  }

  // if bot died, clear all weapon stuff and force buying again
  if (!is_alive_) {
    ClearAmmoInfo ();

    current_weapon_ = Weapon::Invalid;
    weapon_type_ = WeaponType::None;
  }
  flash_level_ = 100;
  check_dark_timer_.invalidate ();

  knife_attack_timer_.start (rg (1.3f, 2.6f));

  if (mp_freezetime.As<float> () <= 1.0f) {
    next_buy_timer_.start (rg (0.2f, 0.6f));
  }
  else {
    next_buy_timer_.start (rg (0.6f, 2.0f));
  }

  buy_pending_ = false;
  in_bomb_zone_ = false;
  ignore_buy_delay_ = false;
  has_c4_ = false;
  has_hostage_ = false;

  fall_down_timer_.invalidate ();
  shield_check_timer_.invalidate ();
  zoom_check_timer_.invalidate ();
  strafe_set_timer_.invalidate ();
  dodge_strafe_dir_ = Dodge::None;
  fight_style_ = Fight::None;
  fight_style_check_timer_.invalidate ();

  check_weapon_switch_ = true;
  check_knife_switch_ = true;
  buying_finished_ = false;

  radio_entity_ = nullptr;
  radio_order_ = RadioChat::InvalidSelect;
  defended_bomb_ = false;
  escaped_from_bomb_ = false;
  defend_hostage_ = false;
  headed_timer_.invalidate ();

  logo_spray_timer_.start (rg (5.0f, 30.0f));
  spawn_timer_.start ();
  last_chat_timer_.start ();

  time_camping_ = 0.0f;
  camp_direction_ = 0;
  next_camp_dir_timer_.invalidate ();
  camp_buttons_ = 0;

  sound_update_timer_.invalidate ();
  heard_sound_timer_.start ();
  sound_memory_head_ = 0;
  sound_memory_ = {};

  msg_queue_.clear ();
  goal_history_.clear ();
  ignored_breakable_.clear ();
  dropped_dry_weapons_mask_ = 0;

  // ignore enemies for some time if needed
  if (cv_ignore_enemies_after_spawn_time.As<float> () > 0.0f) {
    enemy_ignore_timer_.start (cv_ignore_enemies_after_spawn_time.As<float> ());
  }
  else {
    enemy_ignore_timer_.invalidate ();
  }

  // and put buying into its message queue
  PushMsgQueue (Msg::Buy);
  StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);

  // restore fake client bit, just in case
  pev->flags |= FL_CLIENT | FL_FAKECLIENT;

  if (rg.chance (50)) {
    PushRadioChat (RadioChat::NewRound);
  }
  tickmgr.OnBotRound (this);
}

void Bot::ResetPathSearchType () {
  const auto morale = fear_level_ > agression_level_ ? rg.chance (30) : rg.chance (70);

  switch (personality_) {
  default:
  case Personality::Normal:
    path_type_ = morale ? FindPathType::Optimal : FindPathType::Fast;
    break;

  case Personality::Rusher:
    path_type_ = morale ? FindPathType::Fast : FindPathType::Optimal;
    break;

  case Personality::Careful:
    path_type_ = morale ? FindPathType::Optimal : FindPathType::Safe;
    break;
  }

  // if debug goal - set the fastest
  if (cv_debug_goal.As<int> () != kInvalidNodeIndex) {
    path_type_ = FindPathType::Fast;
  }

  // no need to be safe on csdm
  if (game.Is (GameFlags::CSDM)) {
    path_type_ = FindPathType::Fast;
    return;
  }
}

void Bot::Kill () {
  // this function kills a bot base code courtesy of lazy

  bots.TouchKillerEntity (this);
}

void Bot::Kick (bool silent) {
  // this function kick off one bot from the server
  auto username = pev->netname.chars ();

  if (!(pev->flags & FL_CLIENT) || (pev->flags & FL_DORMANT) || ystl::strings.is_empty (username)) {
    return;
  }
  MarkStale ();

  // prefer direct SV_DropClient on rehlds (the engine reports the drop
  // itself), kick command stays as fallback
  if (!rehlds.DropClient (game.IndexOfEntity (Ent ()), "Kicked")) {
    game.ServerCommand ("kick \"%s\"", username);

    if (!silent) {
      ctrl.Msg ("Bot '%s' kicked.", username);
    }
  }
}

void Bot::MarkStale () {
  // switch chatter icon off
  ShowChatterIcon (false, true);

  // reset bots ping to default
  fakeping.Reset (Ent ());

  // mark bot as leaving
  is_stale_ = true;

  // clear the bot name
  conf.ClearUsedName (this);

  // clear fakeclient bit
  pev->flags &= ~FL_FAKECLIENT;

  // make as not receiving any messages
  pev->flags |= FL_DORMANT;
}

void Bot::SetNewDifficulty (Difficulty new_difficulty) {
  if (new_difficulty < Difficulty::Noob || new_difficulty > Difficulty::Expert) {
    new_difficulty = Difficulty::Hard;
  }
  difficulty_ = new_difficulty;
  difficulty_data_ = conf.GetDifficultyTweaks (new_difficulty);
}

void Bot::UpdateTeamJoin () {
  // this function handles the selection of teams & class

  if (!not_started_) {
    return;
  }
  const auto bot_team = game.GetRealPlayerTeam (Ent ());

  // cs prior beta 7.0 uses hud-based motd, so press fire once
  if (game.Is (GameFlags::Legacy)) {
    pev->button |= IN_ATTACK;
  }

  // check if something has assigned team to us
  else if (bot_team == Team::Terrorist || bot_team == Team::CT) {
    not_started_ = false;
  }
  else if (bot_team == Team::Unassigned && retry_join_ > 2) {
    start_action_ = Msg::TeamSelect;
  }

  // if bot was unable to join team, and no menus pop-ups, check for stacked team
  if (start_action_ == Msg::None) {
    if (++retry_join_ > 3 && bots.IsTeamStacked (util.ConvertFromCsTeam (wanted_team_))) {
      retry_join_ = 0;

      ctrl.Msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and "
                "restart the round).");
      Kick ();

      return;
    }
  }

  // handle counter-strike stuff here
  if (start_action_ == Msg::TeamSelect) {
    start_action_ = Msg::None; // switch back to idle

    if (wanted_team_ == CSTeam::Invalid) {
      char team_join = cv_join_team.As<ystl::StringRef> ()[0];

      if (team_join == 'C' || team_join == 'c') {
        wanted_team_ = CSTeam::CT;
      }
      else if (team_join == 'T' || team_join == 't') {
        wanted_team_ = CSTeam::Terrorist;
      }
    }

    if (wanted_team_ != CSTeam::Terrorist && wanted_team_ != CSTeam::CT) {
      const auto &[ts, cts] = bots.CountTeamPlayers ();

      // balance teams manually as enforced skins break auto select

      if (ts > cts) {
        wanted_team_ = CSTeam::CT;
      }
      else if (ts < cts) {
        wanted_team_ = CSTeam::Terrorist;
      }
      else {
        wanted_team_ = rg.chance (50) ? CSTeam::CT : CSTeam::Terrorist;
      }
    }

    // select the team the bot wishes to join
    IssueCommand ("menuselect %d", wanted_team_);
  }
  else if (start_action_ == Msg::ClassSelect) {
    start_action_ = Msg::None; // switch back to idle

    // czero has additional models
    const auto max_choice = game.Is (GameFlags::ConditionZero) ? 5 : 4;
    auto enforced_skin = 0;

    // setup enforced skin based on selected team
    if (wanted_team_ == CSTeam::Terrorist || bot_team == Team::Terrorist) {
      enforced_skin = cv_botskin_t.As<int> ();
    }
    else if (wanted_team_ == CSTeam::Terrorist || bot_team == Team::CT) {
      enforced_skin = cv_botskin_ct.As<int> ();
    }
    enforced_skin = ystl::clamp (enforced_skin, 0, max_choice);

    // try to choice manually
    if (wanted_skin_ < 1 || wanted_skin_ > max_choice) {
      wanted_skin_ = rg (1, max_choice); // use random if invalid
    }

    // and set enforced if any
    if (enforced_skin > 0) {
      wanted_skin_ = enforced_skin;
    }

    // select the class the bot wishes to use
    IssueCommand ("menuselect %d", wanted_skin_);

    // bot has now joined the game (doesn't need to be started)
    not_started_ = false;

    // check for greeting other players, since we connected
    if (rg.chance (20)) {
      need_to_send_welcome_chat_ = true;
    }
  }
}

void Manager::CaptureChatRadio (ystl::StringRef cmd, ystl::StringRef arg, edict_t *ent) {
  if (game.IsBotCmd ()) {
    return;
  }

  if (cmd.starts_with ("say")) {
    const bool alive = game.IsAliveEntity (ent);
    auto team = Team::Invalid;

    if (cmd.ends_with ("team")) {
      team = game.GetRealPlayerTeam (ent);
    }

    for (const auto &client : clients) {
      if (!client.IsUsed () || (team != Team::Invalid && team != client.team2) || alive != game.IsAliveEntity (client.ent)) {
        continue;
      }
      auto target = bots[client.ent];

      if (target != nullptr) {
        target->say_text_buffer_.entity_index = game.IndexOfPlayer (ent);

        if (ystl::strings.is_empty (engfuncs.pfnCmd_Args ())) {
          continue;
        }
        target->say_text_buffer_.say_text = engfuncs.pfnCmd_Args ();
        target->say_text_buffer_.time_next_chat = game.Time () + target->say_text_buffer_.chat_delay;
      }
    }
  }
  auto &target = clients[ent];

  // check if this player alive, and issue something
  if (has_flag (target.flags, ClientFlags::Alive) && target.radio != RadioChat::InvalidSelect && cmd.starts_with ("menuselect")) {
    auto menuselect = static_cast<RadioChat> (arg.as<int> ());

    if (menuselect != RadioChat::InvalidSelect) {
      menuselect += 10 * (target.radio - 1);

      if (menuselect != RadioChat::RogerThat && menuselect != RadioChat::Negative && menuselect != RadioChat::ReportingIn) {
        for (auto &bot : bots) {

          // validate bot
          if (bot.team_ == target.team && ent != bot.Ent () && bot.radio_order_ == RadioChat::InvalidSelect) {
            bot.radio_order_ = menuselect;
            bot.radio_entity_ = ent;
          }
        }
      }
      bots.SetLastRadioTimestamp (target.team, game.Time ());
    }
    target.radio = RadioChat::InvalidSelect;
  }
  else if (cmd.starts_with ("radio")) {
    target.radio = static_cast<RadioChat> (cmd.substr (5).as<int> ());
  }
}

void Manager::NotifyBombDefuse () {
  // notify all terrorists that ct is starting bomb defusing

  const auto &bomb_pos = game_state.GetBombOrigin ();

  for (auto &bot : bots) {
    const auto task = bot.GetTaskId ();

    if (!bot.defuse_notified_ && bot.is_alive_ && task != TaskId::MoveTo && task != TaskId::DefuseBomb && task != TaskId::EscapeFromBomb) {

      if (bot.team_ == Team::Terrorist && bot.pev->origin.distance_sq (bomb_pos) < ystl::sqrf (1024.0f)) {
        bot.ClearSearchNodes ();

        bot.path_type_ = FindPathType::Fast;
        bot.position_ = bomb_pos;
        bot.defuse_notified_ = true;

        bot.StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
      }
    }
  }
}

void Manager::SelectLeaders (Team team, bool reset) {
  auto &leader_choosen = team_data_[team].leader_choosen;

  if (reset) {
    leader_choosen = false;
    return;
  }

  if (leader_choosen) {
    return;
  }
  auto &leader_choosen_t = team_data_[Team::Terrorist].leader_choosen;
  auto &leader_choosen_ct = team_data_[Team::CT].leader_choosen;

  if (game.MapIs (MapFlags::Assassination)) {
    if (team == Team::CT && !leader_choosen_ct) {
      for (auto &bot : bots_) {
        if (bot.is_vip_) {
          bot.is_leader_ = true; // vip bot is the leader

          if (ystl::rg.chance (50)) {
            bot.PushRadioChat (RadioChat::FollowMe);
            bot.camp_buttons_ = 0;
          }
        }
      }
      leader_choosen_ct = true;
    }
    else if (team == Team::Terrorist && !leader_choosen_t) {
      auto bot = bots.FindHighestFragBot (team);

      if (bot != nullptr && bot->is_alive_) {
        bot->is_leader_ = true;

        if (ystl::rg.chance (45)) {
          bot->PushRadioChat (RadioChat::FollowMe);
        }
      }
      leader_choosen_t = true;
    }
  }
  else if (game.MapIs (MapFlags::Demolition)) {
    if (team == Team::Terrorist && !leader_choosen_t) {
      for (auto &bot : bots_) {
        if (bot.has_c4_) {
          // bot carrying the bomb is the leader
          bot.is_leader_ = true;

          // terrorist carrying a bomb needs to have some company
          if (ystl::rg.chance (75)) {
            if (cv_radio_mode.As<int> () == 2) {
              bot.PushRadioChat (RadioChat::GoingToPlantBomb);
            }
            else {
              bot.PushRadioChat (RadioChat::FollowMe);
            }
            bot.camp_buttons_ = 0;
          }
        }
      }
      leader_choosen_t = true;
    }
    else if (!leader_choosen_ct) {
      if (auto bot = bots.FindHighestFragBot (team)) {
        bot->is_leader_ = true;

        if (ystl::rg.chance (30)) {
          bot->PushRadioChat (RadioChat::FollowMe);
        }
      }
      leader_choosen_ct = true;
    }
  }
  else if (game.MapIs (MapFlags::Escape | MapFlags::KnifeArena | MapFlags::FightYard)) {
    auto bot = bots.FindHighestFragBot (team);

    if (!leader_choosen && bot) {
      bot->is_leader_ = true;

      if (ystl::rg.chance (30)) {
        bot->PushRadioChat (RadioChat::FollowMe);
      }
      leader_choosen = true;
    }
  }
  else {
    auto bot = bots.FindHighestFragBot (team);

    if (!leader_choosen && bot) {
      bot->is_leader_ = true;

      if (ystl::rg.chance (team == Team::Terrorist ? 30 : 40)) {
        bot->PushRadioChat (RadioChat::FollowMe);
      }
      leader_choosen = true;
    }
  }
}

void Manager::InitRound () {
  // this is called at the start of each round

  // check team economics
  for (auto team = Team::Terrorist; team < Team::Num; ++team) {
    UpdateTeamEconomics (team);
    SelectLeaders (team, true);

    team_data_[team].last_radio_timestamp = 0.0f;
  }
  Reset ();

  // notify all bots about new round arrived
  for (auto &bot : bots) {
    bot.NewRound ();
  }

  // reset current radio message for all client
  for (auto &client : clients) {
    client.radio = RadioChat::InvalidSelect;
  }
  graph.ClearVisited ();

  bomb_say_status_ = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;
  plant_search_update_timer_.invalidate ();
  auto_kill_timer_.invalidate ();
  enemy_spotted_ = false;

  practice.Update (); // update practice data on round start
}

void ThreadWorker::Shutdown () {
  if (!Available ()) {
    return;
  }

  if (game.IsDeveloperMode ()) {
    game.Print ("Shutting down bot thread worker.");
  }
  pool_->shutdown ();

  // re-start pool completely
  pool_.reset ();
}

void ThreadWorker::Startup (int workers) {
  ystl::StringRef disable_worker_env = ystl::plat.env (ystl::strings.format ("%s_SINGLE_THREADED", product.logtag));

  // disable on legacy games
  const bool is_legacy_game = game.Is (GameFlags::Legacy);

  // do not do any threading when timescale enabled
  ConVarRef timescale ("sys_timescale");

  // disable worker if requested via env variable or workers are disabled
  if (is_legacy_game || workers == 0 || timescale.Value () > 0 || (!disable_worker_env.empty () && disable_worker_env == "1")) {
    return;
  }
  pool_ = ystl::make_unique<ystl::ThreadPool> ();

  // define worker threads
  const auto count = pool_->thread_count ();

  if (count > 0) {
    ystl::logger.error ("Tried to start thread pool with existing %d threads in pool.", count);
    return;
  }
  const auto max_threads = ystl::plat.hardware_concurrency ();
  auto requested_threads = workers;

  if (requested_threads < 0 || requested_threads >= max_threads) {
    requested_threads = 1;
  }
  requested_threads = ystl::clamp (requested_threads, 1, max_threads - 1);

  // notify user
  if (game.IsDeveloperMode ()) {
    game.Print ("Starting up bot thread worker with %d threads.", requested_threads);
  }

  // start up the worker
  pool_->startup (static_cast<size_t> (requested_threads));
}

bool TickManager::IsFrameSkipDisabled () {
  if (game.Is (GameFlags::Legacy)) {
    return true;
  }

  if (game.Is (GameFlags::Xash3D) && cv_think_fps_disable) {
    static ConVarRef sys_ticrate ("sys_ticrate");

    // ignore think_fps_disable if fps is more than 100 on xash dedicated server
    if (game.IsDedicatedServer () && sys_ticrate.Value () > 100.0f) {
      cv_think_fps_disable.Set (0);

      return false;
    }
    return true;
  }
  return false;
}

} // namespace bot
