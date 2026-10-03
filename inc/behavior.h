//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// game start messages for counter-strike
namespace bot {

enum class Msg : int32_t {
  None = 1,
  TeamSelect = 2,
  ClassSelect = 3,
  Buy = 100,
  Radio = 200,
  Say = 10000,
  SayTeam = 10001
};

// sensing states
enum class Sense : uint32_t {
  Invalid = 0, // invalid sense flag
  SeeingEnemy = ystl::bit (0), // seeing an enemy
  HearingEnemy = ystl::bit (1), // hearing an enemy
  SuspectEnemy = ystl::bit (2), // suspect enemy behind obstacle
  PickupItem = ystl::bit (3), // pickup item nearby
  ThrowExplosive = ystl::bit (4), // could throw he grenade
  ThrowFlashbang = ystl::bit (5), // could throw flashbang
  ThrowSmoke = ystl::bit (6) // could throw smokegrenade
};
YSTL_ENABLE_ENUM_FLAGS (Sense);

// positions to aim at
enum class AimFlags : uint32_t {
  Invalid = 0, // default aim state
  Nav = ystl::bit (0), // aim at nav point
  Camp = ystl::bit (1), // aim at camp vector
  PredictPath = ystl::bit (2), // aim at predicted path
  LastEnemy = ystl::bit (3), // aim at last enemy
  Entity = ystl::bit (4), // aim at entity like buttons, hostages
  Enemy = ystl::bit (5), // aim at enemy
  Grenade = ystl::bit (6), // aim for grenade throw
  Override = ystl::bit (7), // overrides all others (blinded)
  Danger = ystl::bit (8), // additional danger flag
  Flash = ystl::bit (9) // look away from a popping flashbang
};
YSTL_ENABLE_ENUM_FLAGS (AimFlags);

// bot core behavior data mixin
class BehaviorData {
  friend class DebugPanel;
  friend struct TestHook;
  friend struct ChatHook;
  friend struct BuyHook;
  friend struct BehaviorHook;
  friend struct CombatHook;
  friend struct ManagerHook;
  friend struct RadioHook;
  friend struct TasksHook;
  friend struct WeaponsHook;

protected:
  Sense states_ {}; // sensing bitstates

  int old_buttons_ {}; // our old buttons
  int pending_buttons_ {}; // button presses queued for the next movement command
  uint32_t model_mask_ {}; // model mask bits

  float frame_interval_ {}; // bot's frame interval
  ystl::CountdownTimer command_timer_ {}; // next movement command time
  float last_command_msec_ {}; // msec of the last issued movement command
  float last_command_time_ {}; // time of the last issued movement command
  int starved_commands_ {}; // count of starved movement commands (debug)
  ystl::IntervalTimer headed_timer_ {}; // last time followed by radio entity
  ystl::CountdownTimer prev_timer_ {}; // time previously checked movement speed
  ystl::CountdownTimer heavy_timer_ {}; // is it time to execute heavy-weight functions
  float prev_speed_ {}; // speed some frames before
  float previous_think_time_ {}; // time bot last thinked
  ystl::CountdownTimer knife_attack_timer_ {}; // time to rush with knife (at the beginning of the round)
  ystl::CountdownTimer duck_defuse_check_timer_ {}; // time to check for ducking for defuse
  ystl::CountdownTimer near_bomb_timer_ {}; // falls back to direct defuse after hanging around the bomb
  ystl::CountdownTimer defuse_watch_timer_ {}; // gives up the defuse if the bomb can't actually be reached
  ystl::CountdownTimer cover_search_timer_ {}; // cooldown before next seek cover attempt
  ystl::CountdownTimer logo_spray_timer_ {}; // time bot last spray logo
  ystl::CountdownTimer sound_update_timer_ {}; // time to update the sound
  ystl::IntervalTimer heard_sound_timer_ {}; // last time noise is heard
  ystl::CountdownTimer ask_check_timer_ {}; // time to ask team
  ystl::CountdownTimer follow_wait_timer_ {}; // wait to follow time
  float move_speed_ {}; // current speed forward/backward
  float strafe_speed_ {}; // current speed sideways
  float min_speed_ {}; // minimum speed in normal mode
  ystl::CountdownTimer item_check_timer_ {}; // time next search for items needs to be done
  ystl::IntervalTimer last_equip_timer_ {}; // last time we equipped in buyzone
  ystl::CountdownTimer duck_timer_ {}; // time to duck
  ystl::CountdownTimer breakable_timer_ {}; // breakable acquired time
  ystl::CountdownTimer breakable_shoot_timer_ {}; // time when bot started shooting breakable
  ystl::CountdownTimer debug_update_timer_ {}; // time to update last debug timestamp
  float last_damage_timestamp_ {}; // last damage from take damage fn
  float moved_distance_ {}; // bot moved distance

  bool defended_bomb_ {}; // defend action issued
  bool escaped_from_bomb_ {}; // already ran away from the ticking bomb, don't re-enter
  bool defend_hostage_ {}; // defend action issued
  bool duck_defuse_ {}; // should or not bot duck to defuse bomb
  bool is_leader_ {}; // bot is leader of his team
  bool move_to_c4_ {}; // ct is moving to bomb
  bool is_creature_ {}; // bot is not a player, but something else ? zombie ?
  bool is_on_infected_team_ {}; // bot is zombie (this assumes bot is a creature)
  bool infected_enemy_team_ {}; // the enemy is a zombie (assumed to be a hostile creature)

  edict_t *breakable_entity_ {}; // pointer to breakable entity
  edict_t *last_breakable_ {}; // last acquired breakable
  edict_t *target_entity_ {}; // the entity that the bot is trying to reach
  edict_t *heard_enemy_ {}; // the heard enemy

  // sound memory ring buffer for tactical awareness
  static constexpr int kSoundMemorySize = 8;
  int sound_memory_head_ {};
  ystl::FixedArray<SoundMemoryEntry, kSoundMemorySize> sound_memory_ {};

  ystl::Vector prev_origin_ {}; // origin some frames before
  ystl::Vector breakable_origin_ {}; // origin of breakable

  ystl::Array<edict_t *> ignored_breakable_ {}; // list of ignored breakables
  ystl::Array<edict_t *> hostages_ {}; // pointer to used hostage entities

  DifficultyData *difficulty_data_ {};
  ystl::Deque<Msg> msg_queue_ {};

  ystl::CountdownTimer think_timer_ {}; // next think time
  float think_interval_ {}; // bot think interval
  float full_think_interval_ {}; // bot full think interval, heavy weight operations, 1.0 / think_fps

public:
  ystl::CountdownTimer next_buy_timer_ {}; // next buy time
  ystl::CountdownTimer avoid_flash_timer_ {}; // look away until the flash pops
  edict_t *avoid_flash_ent_ {}; // flashbang the reaction was rolled for
  float avoid_flash_dmgtime_ {}; // detonation time of the rolled flashbang
  ystl::Vector avoid_flash_look_ {}; // level point away from the flashbang
  ystl::CountdownTimer blind_timer_ {}; // time when bot is blinded
  float blind_move_speed_ {}; // mad speeds when bot is blind
  float blind_side_move_speed_ {}; // mad side move speeds when bot is blind
  ystl::CountdownTimer change_view_timer_ {}; // timestamp to change look at while at freezetime
  float base_agression_level_ {}; // base aggression level (on initializing)
  float base_fear_level_ {}; // base fear level (on initializing)
  float agression_level_ {}; // dynamic aggression level (in game)
  float fear_level_ {}; // dynamic fear level (in game)
  ystl::CountdownTimer emotion_update_timer_ {}; // next time to sanitize emotions

  BuyState buy_state_ {}; // current count in buying
  BuyContext buy_context_ {}; // context during buying

  int last_damage_type_ {}; // stores last damage
  int money_amount_ {}; // amount of money in bot's bank
  int blind_node_index_ { kInvalidNodeIndex }; // node index to cover when blind (invalid until takeBlind assigns cover)
  int blind_button_ {}; // buttons bot press, when blind
  int logo_decal_index_ {}; // index for logotype

  bool ignore_buy_delay_ {}; // when reaching buyzone in the middle of the round don't do pauses
  bool buying_finished_ {}; // done with buying
  bool buy_pending_ {}; // bot buy is pending

  ystl::Vector position_ {}; // position to move to in move to position task
};

} // namespace bot
