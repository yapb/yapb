//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// bomb say string
namespace bot {

enum class BombPlantedSay : int32_t {
  ChatSay = ystl::bit (1),
  Chatter = ystl::bit (2)
};
YSTL_ENABLE_ENUM_FLAGS (BombPlantedSay);

// bot create status
enum class CreateResult : int32_t {
  Success,
  MaxPlayersReached,
  GraphError,
  TeamStacked
};

// shared team data for bot
struct TeamData {
  bool leader_choosen {}; // is team leader choose thees round
  bool positive_eco {}; // is team able to buy anything
  float last_radio_timestamp {}; // global radio time
  RadioChat last_radio_slot = { RadioChat::Invalid }; // last radio message for team
};

// bot difficulty data
struct DifficultyData {
  float reaction[2] {};
  int32_t headshot_pct {};
  int32_t seen_thru_pct {};
  int32_t hear_thru_pct {};
  int32_t max_recoil {};
  ystl::Vector aim_error {};
};

// bot creation tab
struct Request {
  bool manual {};
  int skin {};
  Team team {};
  Personality personality {};
  Difficulty difficulty {};
  ystl::String name {};
};

// manager class
class Manager final : public ystl::Singleton<Manager> {
public:
  using ForEachBot = const ystl::Lambda<bool (Bot *)> &;
  using UniqueBot = ystl::UniquePtr<Bot>;

private:
  ystl::CountdownTimer difficulty_balance_timer_ {}; // time to balance difficulties ?
  ystl::CountdownTimer auto_kill_timer_ {}; // time to kill all the bots ?
  ystl::CountdownTimer maintain_timer_ {}; // time to maintain bot creation
  ystl::CountdownTimer quota_maintain_timer_ {}; // time to maintain bot quota
  ystl::CountdownTimer plant_search_update_timer_ {}; // time to update for searching planted bomb
  ystl::IntervalTimer last_chat_timer_ {}; // global chat time timestamp

  Team last_winner_ { Team::Invalid }; // the team who won previous round
  Difficulty last_difficulty_ {}; // last bots difficulty
  BombPlantedSay bomb_say_status_ {}; // some bot is issued whine about bomb
  int num_previous_players_ {}; // number of players in game im previous player check

  bool enemy_spotted_ {}; // any bot has spotted an enemy this round ?

  ystl::Deque<Request> saved_bots_ {}; // bot data that persists upon changelevel
  ystl::Deque<Request> add_requests_ {}; // bot creation tab

private:
  // queues the bot creation request, restoring saved bot data if needed
  void QueueBotRequest (Request &&request);

  ystl::InlineList<Bot> bots_ {}; // live bots, intrusive links stay valid (no array invalidation)

  // owns the bot objects; heap bots keep stable addresses across growth, unlink from bots_ first
  ystl::Array<UniqueBot> owned_bots_ {};

  Bot *bots_by_index_[kGameMaxPlayers] {}; // direct lookup by player index

  edict_t *killer_entity_ {}; // killer entity for bots
  ystl::FixedArray<TeamData, ystl::to_underlying (Team::Num)> team_data_ {}; // teams shared data

  ystl::CountdownTimer hold_quota_management_timer_ {}; // prevent from running quota management for some time

protected:
  CreateResult Create (ystl::StringRef name, Difficulty difficulty, Personality personality, Team team, int skin);

public:
  Manager ();
  ~Manager ();

public:
  ystl::Twin<int, int> CountTeamPlayers ();

  Bot *FindBotByIndex (int index);
  Bot *FindBotByEntity (edict_t *ent);

  Bot *FindAliveBot ();
  Bot *FindHighestFragBot (Team team);

  int GetHumansCount (bool ignore_spectators = false);
  int GetAliveHumansCount ();
  int GetPlayerPriority (edict_t *ent);

  float GetConnectionTimes (ystl::StringRef name, float original);
  float GetAverageTeamKpd (bool calc_for_bots);

  void Frame ();
  void CreateKillerEntity ();
  void DestroyKillerEntity ();
  void TouchKillerEntity (Bot *bot);
  void Destroy ();
  void Addbot (ystl::StringRef name, Difficulty difficulty, Personality personality, Team team, int skin, bool manual);
  void Addbot (
    ystl::StringRef name, ystl::StringRef difficulty, ystl::StringRef personality, ystl::StringRef team, ystl::StringRef skin, bool manual);
  void ServerFill (CSTeam team, Personality personality = Personality::Normal, Difficulty difficulty = Difficulty::Invalid, int num_to_add = -1);
  void KickEveryone (bool instant = false, bool zero_quota = true, bool silent = false);
  void KickBot (int index);
  void KickFromTeam (Team team, bool remove_all = false);
  void KillAllBots (Team team = Team::Invalid, bool silent = false);
  void MaintainQuota ();
  void MaintainAutoKill ();
  void MaintainLeaders ();
  void MaintainRoundRestart ();
  void InitQuota ();
  void InitRound ();
  void DecrementQuota (int by = 1);
  void SelectLeaders (Team team, bool reset);
  void ListBots ();
  void SetWeaponMode (int selection);
  void UpdateTeamEconomics (Team team, bool set_true = false);
  void UpdateBotDifficulties ();
  void BalanceBotDifficulties ();
  void Reset ();
  void CaptureChatRadio (ystl::StringRef cmd, ystl::StringRef arg, edict_t *ent);
  void NotifyBombDefuse ();
  void ExecGameEntity (edict_t *ent);
  void ForEach (ForEachBot handler);
  void DisconnectBot (Bot *bot);
  void HandleDeath (edict_t *killer, edict_t *victim);
  void SetLastWinner (Team winner);
  void CheckBotModel (edict_t *ent, char *infobuffer);
  void CheckNeedsToBeKicked ();
  void RefreshCreatureStatus ();

  bool IsTeamStacked (Team team);
  bool KickRandom (bool dec_quota = true, Team from_team = Team::Unassigned);
  bool BalancedKickRandom (bool dec_quota);
  bool HasCustomCsdmSpawnEntities ();

public:
  bool GetTeamEconomics (Team team) const {
    return team_data_[team].positive_eco;
  }

  Team GetLastWinner () const {
    return last_winner_;
  }

  int32_t GetBotCount () const {
    return bots_.size<int32_t> ();
  }

  void CreateRandom (bool manual = false) {
    Addbot ("", Difficulty::Invalid, Personality::Invalid, Team::Invalid, -1, manual);
  }

  bool EnemySpotted () const {
    return enemy_spotted_;
  }

  void SetEnemySpotted (const bool spotted) {
    enemy_spotted_ = spotted;
  }

  bool HasBombSay (BombPlantedSay type) const {
    return has_flag (bomb_say_status_, type);
  }

  void ClearBombSay (BombPlantedSay type) {
    bomb_say_status_ &= ~type;
  }

  void SetPlantedBombSearchCooldown (const float duration) {
    plant_search_update_timer_.start (duration);
  }

  bool HasPlantedBombSearchCooldown () const {
    return !plant_search_update_timer_.elapsed ();
  }

  void SetLastRadioTimestamp (const Team team, const float timestamp) {
    if (team == Team::CT || team == Team::Terrorist) {
      team_data_[team].last_radio_timestamp = timestamp;
    }
  }

  float GetLastRadioTimestamp (const Team team) const {
    if (team == Team::CT || team == Team::Terrorist) {
      return team_data_[team].last_radio_timestamp;
    }
    return 0.0f;
  }

  void SetLastRadio (const Team team, const RadioChat radio) {
    team_data_[team].last_radio_slot = radio;
  }

  RadioChat GetLastRadio (const Team team) const {
    return team_data_[team].last_radio_slot;
  }

  void MarkLastChatTime () {
    last_chat_timer_.start ();
  }

  float GetLastChatElapsedTime () const {
    return last_chat_timer_.elapsed_time ();
  }

  // some bots are online ?
  bool HasBotsOnline () const {
    return GetBotCount () > 0;
  }

  void DisconnectAll () {
    if (HasBotsOnline ()) {
      KickEveryone (true);
    }
  }

public:
  Bot *operator[] (int index) {
    return FindBotByIndex (index);
  }

  Bot *operator[] (edict_t *ent) {
    return FindBotByEntity (ent);
  }

public:
  ystl::InlineList<Bot>::iterator begin () {
    return bots_.begin ();
  }

  ystl::InlineList<Bot>::const_iterator begin () const {
    return bots_.begin ();
  }

  ystl::InlineList<Bot>::iterator end () {
    return bots_.end ();
  }

  ystl::InlineList<Bot>::const_iterator end () const {
    return bots_.end ();
  }
};

// bot async worker wrapper
class ThreadWorker final : public ystl::Singleton<ThreadWorker> {
private:
  ystl::UniquePtr<ystl::ThreadPool> pool_ {};

public:
  explicit ThreadWorker () = default;
  ~ThreadWorker () = default;

public:
  void Shutdown ();
  void Startup (int workers);

public:
  template <typename F> void Enqueue (F &&fn) {
    if (!Available ()) {
      fn (); // no threads, no fun, just run task in current thread
      return;
    }
    pool_->enqueue (ystl::move (fn));
  }

public:
  bool Available () {
    return pool_ && pool_->thread_count () > 0;
  }
};

// bot tick scheduler, owns think rate and movement command issuing
class TickManager final : public ystl::Singleton<TickManager> {
public:
  explicit TickManager () = default;
  ~TickManager () = default;

public:
  void Frame (Bot *bot);
  void OnBotRound (Bot *bot);
  void RunCommand (Bot *bot);

private:
  bool IsFrameSkipDisabled ();
  uint8_t ComputeMsec (const Bot *bot) const;
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Manager, bots);

// expose async worker
YSTL_EXPOSE_GLOBAL_SINGLETON (ThreadWorker, worker);

// expose tick scheduler
YSTL_EXPOSE_GLOBAL_SINGLETON (TickManager, tickmgr);

} // namespace bot
