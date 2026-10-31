//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// fight style type
namespace bot {

enum class Fight : int32_t {
  None = 0,
  Strafe,
  Stay
};

// dodge type
enum class Dodge : int32_t {
  None = 0,
  Left,
  Right
};

// visibility flags
enum class Visibility : int8_t {
  Head = ystl::bit (1),
  Body = ystl::bit (2),
  Other = ystl::bit (3),
  None = 0
};
YSTL_ENABLE_ENUM_FLAGS (Visibility);

// current and remembered enemy, with the compound resets in one place
struct EnemyState {
  edict_t *current {}; // currently engaged enemy
  edict_t *last {}; // last enemy, kept after the current one is lost
  ystl::Vector last_origin {}; // last known position of the last enemy
  edict_t *aim_lock {}; // enemy the head/body aim lock was rolled for

  bool Has () const {
    return current != nullptr;
  }

  // drop the engaged target, keep the memory of the last enemy
  void Clear () {
    current = nullptr;
    aim_lock = nullptr;
  }

  // drop everything: new round, teleport or a rebuilt graph
  void Forget () {
    current = nullptr;
    last = nullptr;
    last_origin.clear ();
    aim_lock = nullptr;
  }

  // drop the remembered enemy only
  void ForgetLast () {
    last = nullptr;
    last_origin.clear ();
  }

  // forget any reference to a freed entity, keep memory of unrelated enemies
  void DropEntity (edict_t *ent) {
    if (current == ent) {
      current = nullptr;
    }
    if (aim_lock == ent) {
      aim_lock = nullptr;
    }
    if (last == ent) {
      ForgetLast ();
    }
  }

  // remember where an enemy was last seen
  void Remember (edict_t *ent, const ystl::Vector &origin) {
    last = ent;
    last_origin = origin;
  }
};

// throttled, cached result of a visibility probe against a world point
struct ProbeCache {
  ystl::CountdownTimer timer {}; // throttles the check
  ystl::Vector origin {}; // last position the check was made against
  bool result {}; // cached result
};

// bot combat data mixin
class CombatData {
  friend class DebugPanel;
  friend struct TestHook;
  friend struct CombatHook;
  friend struct BehaviorHook;
  friend struct VisionHook;
  friend struct TasksHook;
  friend struct WeaponsHook;

protected:
  AimFlags aim_flags_ {}; // aiming conditions

  int kills_count_ {}; // the kills count of a bot

  ystl::CountdownTimer fight_style_check_timer_ {}; // time checked style
  ystl::CountdownTimer strafe_set_timer_ {}; // time strafe direction was set
  ystl::CountdownTimer shoot_at_dead_timer_ {}; // time to shoot at dying players
  float old_combat_desire_ {}; // holds old desire for filtering
  ystl::IntervalTimer last_victim_timer_ {}; // time when bot killed an enemy
  float kills_interval_ {}; // interval between kills

  bool defuse_notified_ {}; // bot is notified about bomb defusion

  Visibility enemy_parts_ {}; // visibility flags
  Fight fight_style_ {}; // combat style to use

  ystl::Vector enemy_origin_ {}; // target origin chosen for shooting
  ystl::CountdownTimer forget_last_victim_timer_ {}; // time to forget last victim position ?
  ystl::CountdownTimer thru_wall_hold_timer_ {}; // holds thru-wall engagement without re-rolling chance
  ystl::CountdownTimer thru_wall_reroll_timer_ {}; // backoff between discrete thru-wall chance rolls
  ProbeCache penetration_ {}; // cached penetrable-obstacle check
  ProbeCache dark_area_ {}; // cached dark-area check
  edict_t *dark_area_enemy_ {}; // enemy the dark-area check was made for

  edict_t *stab_unaware_enemy_ {}; // enemy the unaware-stab decision was made for

  bool stab_unaware_ {}; // cached decision: go for the knife against current enemy
  ystl::CountdownTimer stab_unaware_timer_ {}; // re-evaluate the unaware-stab decision periodically

  ystl::UniquePtr<class PlayerHitboxEnumerator> hitbox_enumerator_ {};

public:
  ystl::CountdownTimer enemy_update_timer_ {}; // time to check for new enemies

  // noticing progress per visible player, meter 1.0 means noticed
  struct NoticeProgress {
    float meter {};
    float seen_time {};
  };
  ystl::HashMap<edict_t *, NoticeProgress, EdictHash> notice_progress_ {};
  ystl::CountdownTimer enemy_reachable_timer_ {}; // time to recheck if enemy reachable
  ystl::CountdownTimer enemy_ignore_timer_ {}; // ignore enemy for some time
  ystl::IntervalTimer see_enemy_timer_ {}; // time bot sees enemy
  ystl::CountdownTimer enemy_surprise_timer_ {}; // time of surprise
  float ideal_reaction_time_ {}; // time of base reaction
  float actual_reaction_time_ {}; // time of current reaction time

  bool is_enemy_reachable_ {}; // direct line to enemy
  bool fire_hurts_friend_ {}; // firing at enemy will hurt our friend?

  int death_count_ {}; // number of bot deaths
  int num_enemies_left_ {}; // number of enemies alive left on map
  int num_friends_left_ {}; // number of friend alive left on map

  float kpd_ratio_ {}; // kill per death ratio

  EnemyState enemy_ {};
  edict_t *last_victim_ {}; // pointer to killed entity
  ystl::CountdownTimer head_roll_timer_ {}; // gates re-rolling the head/body aim decision
  float recoil_compensation_ {}; // low-passed punch compensation; smooths per-shot aim jitter

  ystl::Vector last_victim_origin_ {}; // last victim origin to watch it
};

} // namespace bot
