//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// external
#include <ystl/ystl.h>

#include <linkage/goldsrc.h>
#include <linkage/metamod.h>
#include <linkage/physint.h>

// plugin (amxx module) overrides for a bot
namespace bot {

struct Overrides {
  bool allow_move { true }; // movement allowed, false freezes the bot
  ystl::Vector look_at {}; // forced look target, empty means ai decides
};

// forwards
class Bot;
class Graph;
class Manager;
class TickManager;

} // namespace bot

#include "constant.h"
#include "product.h"
#include "module.h"

#include "chatlib.h"
#include "radio.h"
#include "trace.h"
#include "manager.h"
#include "buying.h"
#include "weapons.h"

#include "sounds.h"
#include "config.h"

#include "engine.h"
#include "rehlds.h"

#include "graph.h"
#include "vision.h"
#include "control.h"
#include "debug.h"

#include "clients.h"

#include "behavior.h"
#include "combat.h"
#include "navigate.h"
#include "tasks.h"

#include "support.h"
#include "hooks.h"
#include "message.h"
#include "cvars.h"
#include "planner.h"
#include "storage.h"
#include "analyze.h"
#include "fakeping.h"
#include "funmode.h"

namespace bot {

// main bot class
class Bot final : public WeaponsData,
                  public CombatData,
                  public NavigateData,
                  public VisionData,
                  public TasksData,
                  public ChatData,
                  public RadioData,
                  public BehaviorData,
                  public ystl::InlineListNode<Bot> {
public:
  friend class Manager;
  friend class TickManager;
  friend class Practice;
  friend class DebugPanel;

  friend struct NavigateHook;
  friend struct ChatHook;
  friend struct BuyHook;
  friend struct BehaviorHook;
  friend struct CombatHook;
  friend struct VisionHook;
  friend struct RadioHook;
  friend struct TasksHook;
  friend struct WeaponsHook;

private:
  ystl::RWrand rg {}; // all bots has own rng

  // weapon selection and purchasing
private:
  // select best weapon from array based on money available
  int PickBestWeapon (ystl::SmallArray<int> &vec, int money_save) const;

  // get index of best primary weapon currently carried
  int GetBestPrimaryCarriedIndex ();

  // get index of best secondary weapon currently carried
  int GetBestSecondaryCarriedIndex ();

  // get index of best weapon from all owned weapons
  int GetBestOwnedWeaponIndex () const;

  // get index of best pistol from owned weapons
  int GetBestOwnedPistolIndex () const;

  // get weapon id of best grenade currently carried
  Weapon GetBestGrenadeCarriedId () const;

  // check if bot can replace current weapon with better one
  bool CanReplaceWeapon ();

  // check if weapon is on the restricted list
  bool IsWeaponRestricted (Weapon wid) const;

  // check if weapon is restricted by amx plugin
  bool IsWeaponRestrictedAmx (Weapon wid) const;

  // check if weapon is eligible for purchase for team
  bool IsWeaponEligibleForPurchase (const WeaponInfo *weapon, Team team) const;

  // check if weapon passes economics budget check
  bool PassesEconomicsCheck (const WeaponInfo *weapon, int prostock, int disrespect_economics_pct) const;

  // get the prostock purchase limit
  int GetProstockLimit () const;

  // select primary weapon to buy from preference list
  WeaponInfo *SelectBuyPrimary (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab, int money_save);

  // select secondary weapon to buy from preference list
  WeaponInfo *SelectBuySecondary (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab);

  // issue buy command for specified weapon
  void IssueBuyCommand (const WeaponInfo *weapon);

  // buy primary weapon from preference list
  void BuyPrimaryWeapon (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab);

  // buy secondary weapon from preference list
  void BuySecondaryWeapon (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab);

  // buy armor (vest and vest+helmet)
  void BuyArmor ();

  // buy ammo for current weapons
  void BuyAmmo ();

  // buy grenades (flashbang, smoke, he)
  void BuyGrenades ();

  // buy defusal kit for ct
  void BuyDefusalKit ();

  // buy night vision goggles
  void BuyNightVision ();

  // combat and enemy detection
private:
  // count number of enemies within radius of origin
  int NumEnemiesNear (const ystl::Vector &origin, const float radius) const;

  // count number of friendly players within radius of origin
  int NumFriendsNear (const ystl::Vector &origin, const float radius) const;

  // calculate entity scale factor for hitbox adjustment
  float CalculateScaleFactor (edict_t *ent) const;

  // check if current enemy poses a threat
  bool IsEnemyThreat ();

  // check if bot has clear line of sight to enemy
  bool SeesEnemy (edict_t *player);

  // check if last known enemy position is shootable
  bool LastEnemyShootable ();

  // check if ground weapon is worth picking up
  bool RateGroundWeapon (edict_t *ent);

  // react to newly spotted enemy
  bool ReactOnEnemy ();

  // check if bot has any weapons available
  bool HasAnyWeapons () const;

  // check if current weapon has ammo in clip
  bool HasAnyAmmoInClip ();

  // check if another weapon (other than the given one) has bullets in its magazine
  bool HasAnotherWeaponWithAmmoInClip (Weapon except) const;

  // check if bot has any ammo for weapons
  bool HasAnyAmmo ();

  // check if bot is using knife only mode
  bool IsKnifeMode ();

  // check if server is in grenade war mode
  bool IsGrenadeWar ();

  // check if weapon is ineffective at given distance
  bool IsWeaponBadAtDistance (int weapon_index, float distance);

  // check if bot should pause firing at distance
  bool NeedToPauseFiring (float distance);

  // check if zoom is needed for target distance
  bool CheckZoom (float distance);

  // lookup and track visible enemies
  bool LookupEnemies ();

  // check if enemy is hiding behind cover
  bool IsEnemyHidden (edict_t *enemy);

  // check if enemy has god mode or invincibility
  bool IsEnemyInvincible (edict_t *enemy);

  // check if enemy has notarget flag set
  bool IsEnemyNoTarget (edict_t *enemy);

  // check if enemy is standing in dark area
  bool IsEnemyInDarkArea (edict_t *enemy) const;

  // throttled dark area check
  bool IsEnemyInDarkAreaCached (edict_t *enemy);

  // check if teammate is blocking line of fire
  bool IsFriendInLineOfFire (float distance) const;

  // check if multiple enemies are grouped together
  bool IsGroupOfEnemies (const ystl::Vector &location, float radius = 768.0f);

  // check if wall at destination is penetrable
  bool IsPenetrableObstacle (const ystl::Vector &dest);

  // throttled version of ispenetrableobstacle, re-runs trace cascade only if target moved or cache expired
  bool IsPenetrableObstacleCached (const ystl::Vector &dest);

  // randomized thru-wall chance: for skill values above 25% rolls anywhere in 25..pct range
  bool GetThruWallChance (int pct) const;

  // check penetrability using method 1 (trace line)
  bool IsPenetrableObstacle1 (const ystl::Vector &dest, int penetrate_power) const;

  // check penetrability using method 2 (hull trace)
  bool IsPenetrableObstacle2 (const ystl::Vector &dest, int penetrate_power) const;

  // check penetrability using method 3 (multiple traces)
  bool IsPenetrableObstacle3 (const ystl::Vector &dest, int penetrate_power) const;

  // check penetrability using method 4 (game-faithful FireBullets3 penetration)
  bool IsPenetrableObstacle4 (const ystl::Vector &dest, int penetrate_power) const;

  // check if enemy is using riot shield
  bool IsEnemyBehindShield (edict_t *enemy);

  // check if enemy is within visible sight cone
  bool IsEnemyInSight (ystl::Vector &end_pos);

  // check if enemy is close enough to notice
  bool IsEnemyNoticeable (float range);

  // check if bomb timer is about to expire
  bool IsOutOfBombTimer ();

  // check if body parts are visible for aiming
  bool CheckBodyParts (edict_t *target);

  // check body part visibility using offset method
  bool CheckBodyPartsWithOffsets (edict_t *target);

  // check body part visibility using hitbox method
  bool CheckBodyPartsWithHitboxes (edict_t *target);

  // combat actions and firing
private:
  // check if weapon needs reloading and reload
  void CheckReload ();

  // avoid incoming grenades by moving away
  void AvoidGrenades ();

  // check if grenades should be thrown at enemy
  void CheckGrenadesThrow ();

  // check if burst fire mode should be used
  void CheckBurstMode (float distance);

  // check if silencer should be attached/removed
  void CheckSilencer ();

  // handle strafing and movement during combat
  void AttackMovement ();

  // main weapon firing handler
  void FireWeapons ();

  // execute actual weapon firing logic
  void DoFireWeapons ();

  // handle weapon selection and firing based on distance
  void HandleWeapons (float distance, int index, Weapon id, int choosen);

  // keep aim focused on current enemy
  void FocusEnemy ();

  // select best available weapon for current situation
  void SelectBestWeapon ();

  // draw knife before long jump link for extra speed, remembering it was navigation-forced
  void DrawKnifeForJump (float jump_distance_sq, float height_diff);

  // return to best weapon once the navigation-forced jump knife is no longer needed
  void RestoreAfterJump ();

  // switch to secondary weapon
  void SelectSecondary ();

  // switch to weapon by weapon id
  void SelectWeaponById (Weapon id);

  // switch to weapon by inventory index
  void SelectWeaponByIndex (int index);

  // update enemy position tracking
  void TrackEnemies ();

  // update team voice commands based on situation
  void UpdateTeamCommands ();

  // decide whether to follow human teammate
  void DecideFollowUser ();

  // set reaction time based on difficulty
  void SetIdealReactionTimers (bool actual = false);

  // store last victim for revenge tracking
  void SetLastVictim (edict_t *victim);

  // clear all stored ammo information
  void ClearAmmoInfo ();

  // fix grenade velocity for proper throw trajectory
  edict_t *SetCorrectGrenadeVelocity (ystl::StringRef model);

  // get offset vector for enemy body aiming
  ystl::Vector GetEnemyBodyOffset ();

  // calculate throw trajectory vector
  ystl::Vector CalcThrow (const ystl::Vector &start, const ystl::Vector &stop);

  // calculate toss arc for grenade
  ystl::Vector CalcToss (const ystl::Vector &start, const ystl::Vector &stop);

  // calculate player jump velocity to reach a node, null if infeasible
  ystl::Vector CalcJumpVelocity (const ystl::Vector &start, const ystl::Vector &stop);

  // true when the ascent can be walked with engine step height
  bool IsWalkableAscent (const ystl::Vector &src, const ystl::Vector &dst);

  // get aiming error offset based on distance
  ystl::Vector GetBodyOffsetError (edict_t *target, float distance);

  // get custom height adjustment for aiming
  ystl::Vector GetCustomHeight (float distance) const;

  // navigation and pathfinding
private:
  // get random direction for camping
  int GetRandomCampDir ();

  // find node that provides best aim to target
  int FindAimingNode (const ystl::Vector &to, int &path_length);

  // find nearest graph node to bot position
  int FindNearestNode ();

  // random node fallback that skips nodes sealed inside mode walls
  int FindRandomNode (PointType type = PointType::Count);

  // farthest node fallback that skips parts sealed off by mode walls
  int FindFarestNode (const ystl::Vector &origin, float range = 32.0f);

  // find node closest to bomb site
  int FindBombNode ();

  // find node for taking cover
  int FindCoverNode (float max_distance);

  // find node for defending position
  int FindDefendNode (const ystl::Vector &origin);

  // find best goal node for current task
  int FindBestGoal ();

  // find best goal when bomb actions are needed
  int FindBestGoalWhenBombAction ();

  // find best goal when hostage actions are needed
  int FindBestGoalWhenHostageAction (ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive);

  // find goal node based on tactic type
  int FindGoalPost (GoalTactic tactic, ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive);

  // check if bot should rush the objective at the end of the round (personality/health/aggression gated)
  bool ShouldRushEndgameTime () const;

  // change current node index safely
  int ChangeNodeIndex (int index);

  // get estimated time to reach current node
  float GetEstimatedNodeReachTime ();

  // check if bot can duck under overhead obstacle
  bool CanDuckUnder (const ystl::Vector &normal);

  // check if bot can jump up onto ledge
  bool CanJumpUp (const ystl::Vector &normal);

  // check if forward movement is blocked (returns blocking entity or nullptr)
  edict_t *IsBlockedForward (const ystl::Vector &normal);

  // check if left strafe movement is possible
  bool CanStrafeLeft (Trace::Result *tr);

  // check if right strafe movement is possible
  bool CanStrafeRight (Trace::Result *tr);

  // check if left side is blocked
  bool IsBlockedLeft ();

  // check if right side is blocked
  bool IsBlockedRight ();

  // check for wall on left side at distance
  bool CheckWallOnLeft (float distance = 40.0f);

  // check for wall on right side at distance
  bool CheckWallOnRight (float distance = 40.0f);

  // check for wall behind bot at distance
  bool CheckWallOnBehind (float distance = 40.0f);

  // update navigation state and path following
  bool UpdateNavigation ();

  // check if bot has active goal node
  bool HasActiveGoal ();

  // advance movement along path to goal
  bool AdvanceMovement ();

  // check if bomb is currently being defused
  bool IsBombDefusing (const ystl::Vector &bomb_origin) const;

  // check if node is occupied by other bot
  bool IsOccupiedNode (int index, bool need_zero_velocity = false);

  // select best next node in path
  bool SelectBestNextNode ();

  // check if move to position is deadly (fall/drown)
  bool IsDeadlyMove (const ystl::Vector &to);

  // check if move to position is unsafe
  bool IsNotSafeToMove (const ystl::Vector &to);

  // check if node is reachable from current position
  bool IsReachableNode (int index);

  // update lift/elevator state handling
  bool UpdateLiftHandling ();

  // update lift state machine
  bool UpdateLiftStates ();

  // check if came from ladder recently
  bool IsPreviousLadder () const;

  // handle player avoidance steering
  void DoPlayerAvoidance (const ystl::Vector &normal);

  // select camp buttons based on node properties
  void SelectCampButtons (int index);

  // post process goal array into result
  void PostProcessGoals (const ystl::SmallArray<int32_t> &goals, int result[]);

  // check terrain for walkable surface
  void CheckTerrain (const ystl::Vector &dir_normal);

  // check if bot is falling and should jump
  void CheckFall ();

  // detect if bot is stuck and needs unstuck
  bool DetectStuckStatus (const ystl::Vector &dir_normal);

  // compute collision weights for response
  void ComputeCollisionWeights (const ystl::Vector &dir_normal, CollisionWeights &weights);

  // execute collision response movement
  void ExecuteCollisionResponse ();

  // find shortest path between nodes using dijkstra/floyd (builds into staging)
  void FindShortestPath (int src_index, int dest_index, PathWalk &staging, PathMeta &meta);

  // apply a freshly built path result to the bot
  void ApplyPathResult (const PathWalk &staging, const PathMeta &meta);

  // find path between nodes (fast or shortest)
  void FindPath (int src_index, int dest_index, FindPathType path_type = FindPathType::Fast);

  // reset collision avoidance state
  void ResetCollision ();

  // ignore collision for short period
  void IgnoreCollision ();

  // set raw strafe speed value
  void SetStrafeSpeedRaw (float strafe_speed);

  // set strafe speed based on normal
  void SetStrafeSpeed (const ystl::Vector &normal, float strafe_speed);

  // find valid node when current is unreachable
  void FindValidNode ();

  // set origin for path following
  void SetPathOrigin ();

  // compute desired distance squared for ladder navigation
  float ComputeLadderDesiredDistance ();

  // translate input to movement commands
  void TranslateInput ();

  // move bot along path to goal
  void MoveToGoal ();

  // reset all movement state
  void ResetMovement ();

  // lookup button entity by target name
  edict_t *LookupButton (ystl::StringRef target, bool blind_test = false);

  // check if bot is currently on ladder
  bool IsOnLadder () const {
    return pev->movetype == MOVETYPE_FLY;
  }

  // check if bot is on ground or partial ground
  bool IsOnFloor () const {
    return !!(pev->flags & (FL_ONGROUND | FL_PARTIALGROUND));
  }

  // check if bot is in water (swimming)
  bool IsInWater () const {
    return pev->waterlevel >= 2;
  }

  // check if bot is in narrow passage
  bool IsInNarrowPlace () const {
    return has_flag (path_flags_, NodeFlag::Narrow);
  }

  // ensure current node index is valid
  void EnsureCurrentNodeIndex () {
    if (current_node_index_ == kInvalidNodeIndex) {
      ChangeNodeIndex (FindNearestNode ());
    }
  }

  // check if path has jump travel flag
  bool HasJumpTravelFlag () const {
    return has_flag (current_travel_flags_, PathFlag::Jump);
  }

  // get skill percentage based on difficulty
  int Skill () const {
    return ystl::to_underlying (difficulty_) * 25;
  }

  // check if node index is valid for prediction
  bool IsNodeValidForPredict (const int index) const {
    return Graph::instance ().Exists (index) && index != current_node_index_;
  }

  // vision and looking
private:
  // get preferred camping direction for position
  ystl::Vector GetCampDirection (const ystl::Vector &dest);

  // check if destination is within field of view
  float IsInFov (const ystl::Vector &dest) const;

  // check if origin is within view cone angles
  bool IsInViewCone (const ystl::Vector &origin);

  // check if bot can see item classname at dest
  bool SeesItem (const ystl::Vector &dest, ystl::StringRef classname);

  // check if bot can see planted/dropped c4 at dest
  bool SeesC4 (const ystl::Vector &dest);

  // set aim direction for look angles
  void SetAimDirection ();

  // update look angles toward target
  void UpdateLookAngles ();

  // update body angles to match look direction
  void UpdateBodyAngles ();

  // check if bot is in dark area and needs light
  void CheckDarkness ();

  // update predicted node index for path
  void UpdatePredictedIndex ();

  // refresh enemy prediction data
  void RefreshEnemyPredict ();

  // check if bomb planting sound is audible
  ystl::Vector IsBombAudible ();

  // tasks and behavior states
private:
  // complete current active task
  void CompleteTask ();

  // execute all pending tasks by priority
  void ExecuteTasks ();

  // run logic during round freezetime
  void LogicDuringFreezetime ();

  // normal behavior task handler
  void TaskNormal ();

  // spraypaint graffiti task handler
  void TaskSpraypaint ();

  // hunt enemy task handler
  void TaskHunt ();

  // seek cover task handler
  void TaskSeekCover ();

  // attack enemy task handler
  void TaskAttack ();

  // pause/wait task handler
  void TaskPause ();

  // blind/flashbang recovery task handler
  void TaskBlind ();

  // camping task handler
  void TaskCamp ();

  // hide from enemy task handler
  void TaskHide ();

  // move to position task handler
  void TaskMoveTo ();

  // plant bomb task handler
  void TaskPlantBomb ();

  // defuse bomb task handler
  void TaskDefuseBomb ();

  // follow user task handler
  void TaskFollowUser ();

  // throw explosive grenade task handler
  void TaskThrowExplosive ();

  // throw flashbang grenade task handler
  void TaskThrowFlashbang ();

  // throw smoke grenade task handler
  void TaskThrowSmoke ();

  // double jump task handler
  void TaskDoubleJump ();

  // escape from bomb task handler
  void TaskEscapeFromBomb ();

  // pickup item task handler
  void TaskPickupItem ();

  // shoot breakable entity task handler
  void TaskShootBreakable ();

  // immutable task descriptor (handler, resumable, name), indexed by id
  static const ystl::Tuple<Task::Function, bool, ystl::StringRef> &TaskInfo (TaskId id);

  // task handler lookup by id
  static Task::Function TaskHandler (TaskId id);

  // default resume flag for a task id
  static bool TaskResumable (TaskId id);

  // task name lookup by id
  static ystl::StringRef TaskName (TaskId id);

  // chat and radio communication
private:
  // check if message matches chat keywords
  bool CheckChatKeywords (ystl::StringRef chat_text, ystl::String &reply);

  // check if bot is currently replying to chat
  bool IsReplyingToChat ();

  // send instant chatter message
  void InstantChatter (RadioChat type) const;

  // check and process radio message queue
  void CheckRadioQueue ();

  // handle chatter when task changes
  void HandleChatterTaskChange (TaskId tid);

  // handle chatter on player kill event
  void HandleChatterOnPlayerKill (bool is_team_kill);

  // handle chatter when enemy is killed
  void HandleChatterEnemyDown ();

  // execute chatter frame events and timeouts
  void ExecuteChatterFrameEvents ();

  // pickups and items
private:
  // get movement speed reduction factor
  float GetShiftSpeed ();

  // check if bot can run with heavy weight operations
  bool CanRunHeavyWeight ();

  // check if bot is marked as creature
  bool IsCreature () const;

  // check if item should be ignored
  bool IsIgnoredItem (edict_t *ent);

  // check if pickup is blocked by obstacle
  bool IsPickupBlocked ();

  // validate existing pickup entities in radius
  bool ValidateExistingPickup (const ystl::Array<InterestingEntity> &interesting, float radius_sq);

  // validate pickup by specific pickup type
  bool ValidatePickupByType (edict_t *ent, Pickup pickup_type);

  // handle team specific pickup logic
  bool HandleTeamSpecificPickups (edict_t *ent, const ystl::Vector &origin, Pickup pickup_type);

  // handle terrorist hostage pickup logic
  bool HandleTerroristHostagePickup (edict_t *ent, const ystl::Vector &origin);

  // handle terrorist bomb pickup logic
  bool HandleTerroristBombPickup (const ystl::Vector &origin);

  // handle ct hostage pickup logic
  bool HandleCtHostagePickup (edict_t *ent);

  // handle ct bomb pickup logic
  bool HandleCtBombPickup (const ystl::Vector &origin);

  // handle ct dropped c4 pickup logic
  bool HandleCtDroppedC4 (edict_t *ent, const ystl::Vector &origin);

  // classify entity as pickup type
  ystl::Twin<bool, Pickup> ClassifyPickupType (edict_t *ent);

  // check ammo and kit pickup by model
  Pickup CheckAmmoAndKitsPickup (ystl::StringRef model);

  // lookup breakable entity to shoot
  edict_t *LookupBreakable ();

  // main loop and frame handling
private:
  // main bot update function
  void Update ();

  // check spawn conditions and zones
  void CheckSpawnConditions ();

  // buy weapons in buy zone
  void BuyWeapons ();

  // check and process message queue
  void CheckMsgQueue ();

  // update hearing and sound tracking
  void UpdateHearing ();

  // update pickup item tracking
  void UpdatePickups ();

  // ensure pickup entities are cleared
  void EnsurePickupEntitiesClear ();

  // finalize pickup action
  void FinalizePickup ();

  // check parachute deployment
  void CheckParachute ();

  // print debug message to console
  void DebugMsgInternal (ystl::StringRef str);

  // main frame update handler
  void SlowFrame ();

  // cheat teleport to planted bomb sealed off by mode walls (official bots walk through, so do we)
  bool TryTeleportToSealedBomb ();

  // set bot condition flags
  void SetConditions ();

  // override condition flags
  void OverrideConditions ();

  // update emotional state machine
  void UpdateEmotions ();

  // update team join state machine
  void UpdateTeamJoin ();

  // refresh creature status in infobuffer
  void RefreshCreatureStatus (char *infobuffer);

  // donate c4 to human teammate
  void DonateC4ToHuman ();

  // inline helpers
private:
  // drop current weapon using drop command
  void DropCurrentWeapon () {
    IssueCommand ("drop");
  }

  // check if using sniper rifle
  bool UsesSniper () const {
    return weapon_type_ == WeaponType::Sniper;
  }

  // check if using rifle (including zoom rifle)
  bool UsesRifle () const {
    return UsesZoomableRifle () || weapon_type_ == WeaponType::Rifle;
  }

  // check if using zoomable rifle
  bool UsesZoomableRifle () const {
    return weapon_type_ == WeaponType::ZoomRifle;
  }

  // check if using pistol
  bool UsesPistol () const {
    return weapon_type_ == WeaponType::Pistol;
  }

  // check if using submachine gun
  bool UsesSubmachine () const {
    return weapon_type_ == WeaponType::SMG;
  }

  // check if using shotgun
  bool UsesShotgun () const {
    return weapon_type_ == WeaponType::Shotgun;
  }

  // check if using heavy weapon (m249)
  bool UsesHeavy () const {
    return weapon_type_ == WeaponType::Heavy;
  }

  // check if using weak weapon (shotgun or smg)
  bool UsesWeakWeapon () const {
    return UsesShotgun () || current_weapon_ == Weapon::UMP45 || current_weapon_ == Weapon::MAC10 || current_weapon_ == Weapon::TMP;
  }

  // check if using weapon suitable for camping
  bool UsesCampGun () const {
    return UsesSubmachine () || UsesRifle () || UsesSniper () || UsesHeavy ();
  }

  // check if using knife
  bool UsesKnife () const {
    return weapon_type_ == WeaponType::Melee;
  }

  // check if recoil punchangle is high
  bool IsRecoilHigh () const {
    return pev->punchangle.x < -1.45f;
  }

public:
  entvars_t *pev {};

  int index_ {}; // saved bot index
  CSTeam wanted_team_ {}; // player team bot wants select
  int wanted_skin_ {}; // player model bot wants to select
  Difficulty difficulty_ {}; // bots hard level

  int ping_base_ {}; // base ping level for randomizing
  ystl::Atomic<int> ping_ {}; // bot's acutal ping

  ystl::IntervalTimer spawn_timer_ {}; // elapsed since this bot spawned
  ystl::CountdownTimer slow_frame_timer_ {}; // time to per-second think
  float health_value_ {}; // clamped bot health
  ystl::CountdownTimer stay_timer_ {}; // stay time before reconnect
  float join_server_time_ {}; // time when bot joined the game
  float play_server_time_ {}; // time bot spent in the game

  int retry_join_ {}; // retry count for choosing team/class
  Msg start_action_ {}; // team/class selection state
  int vote_kick_index_ {}; // index of player to vote against
  int last_vote_kick_ {}; // last index
  int vote_map_ {}; // number of map to vote for
  Team team_ {}; // bot team

  bool is_vip_ {}; // bot is vip?
  bool is_alive_ {}; // has the player been killed or has he just respawned

  bool not_started_ {}; // team/class not chosen yet
  bool in_bomb_zone_ {}; // bot in the bomb zone or not
  bool in_buy_zone_ {}; // bot currently in buy zone
  bool in_escape_zone_ {}; // bot currently in escape zone
  bool in_rescue_zone_ {}; // bot currently in rescue zone
  bool in_vip_zone_ {}; // bot in the vip safety zone
  bool has_hostage_ {}; // does bot owns some hostages
  bool has_progress_bar_ {}; // has progress bar on a hud
  bool kicked_by_rotation_ {}; // is bot kicked due to rotation ?

  ystl::Atomic<bool> kick_me_from_server_ {}; // kick the bot off the server?
  bool is_stale_ {}; // bot is leaving server

  Personality personality_ {}; // bots type

public:
  // movement allowed, false freezes the bot
  bool IsMoveAllowed () const {
    return overrides_.allow_move;
  }

  // enable/disable movement
  void SetMoveAllowed (bool state) {
    overrides_.allow_move = state;
  }

  // forced look target, empty means ai decides
  ystl::Vector &ForcedLookAt () {
    return overrides_.look_at;
  }

public:
  Bot (edict_t *bot, Difficulty difficulty, Personality personality, Team team, int skin);
  ~Bot () = default;

public:
  // run logic that can skip frames
  void Logic ();

  // called when bot is spawned
  void Spawned ();

  // take blind damage from flashbang
  void TakeBlind (int alpha);

  // take damage from inflictor
  void TakeDamage (edict_t *inflictor, int damage, int armor, int bits);

  // show debug overlay for bot state
  void ShowDebugOverlay ();

  // called at start of new round
  void NewRound ();

  // reset path search type for new search
  void ResetPathSearchType ();

  // called when entering buy zone
  void EnteredBuyZone (BuyState buy_state);

  // push message to message queue
  void PushMsgQueue (Msg message);

  // prepare chat message for sending
  void PrepareChatMessage (ystl::StringRef message);

  // check if bot should send chat message
  void CheckForChat ();

  // show/hide chatter icon on hud
  void ShowChatterIcon (bool show, bool disconnect = false) const;

  // clear search nodes for path finding
  void ClearSearchNodes ();

  // mark the whole bombsite around a goal node as searched
  void MarkBombSiteVisited (int node);

  // check breakable entity on touch
  void CheckBreakable (edict_t *touch);

  // check for breakables around bot
  void CheckBreakablesAround ();

  // start new task with priority and data
  void StartTask (TaskId id, float desire, int data, float time, bool resume, bool preserve_goal = false);

  // plugin overrides (movement freeze, forced look target)
  Overrides overrides_ {};

  // clear specific task by id
  void ClearTask (TaskId id);

  // filter tasks by conditions
  void FilterTasks ();

  // clear all tasks
  void ClearTasks ();

  // check task priorities and select max desire task
  void CheckTaskPriorities ();

  // drop weapon for human user
  void DropWeaponForUser (edict_t *user, bool discard_c4);

  // send message to chat
  void SendToChat (ystl::StringRef message, bool team_only);

  // send legacy chat message
  void SendToChatLegacy (ystl::StringRef message, bool team_only);

  // push chat message to queue
  void PushChatMessage (Chat type, bool is_team_say = false);

  // push radio/chatter message to queue
  void PushRadioChat (RadioChat message);

  // try to look toward radio message source
  void TryHeadTowardRadioMessage ();

  // kill the bot
  void Kill ();

  // kick bot from server
  void Kick (bool silent = false);

  // reset double jump state
  void ResetDoubleJump ();

  // start double jump from entity
  void StartDoubleJump (edict_t *ent);

  // teleport bot to origin
  void SendBotToOrigin (const ystl::Vector &origin);

  // mark bot as stale (leaving)
  void MarkStale ();

  // set new difficulty level
  void SetNewDifficulty (Difficulty new_difficulty);

  // check if bot has hostage
  bool HasHostage ();

  // check if bot has primary weapon
  bool HasPrimaryWeapon () const;

  // check if bot has secondary weapon
  bool HasSecondaryWeapon () const;

  // check if bot has shield equipped
  bool HasShield ();

  // check if shield is currently drawn
  bool IsShieldDrawn ();

  // find next best node in path
  bool FindNextBestNode ();

  // find next best node with error handling
  bool FindNextBestNodeEx (const ystl::Array<int32_t> &data, bool handle_fails);

  // check if bot can see entity
  bool SeesEntity (const ystl::Vector &dest, bool from_body = false);

  // get total ammo count
  int GetAmmo () const;

  // get ammo count for weapon id
  int GetAmmo (Weapon id) const;

  // get nearest node to planted bomb
  int GetNearestToPlantedBomb ();

  // get connection time in seconds
  float GetConnectionTime ();

  // get current task pointer
  Task *Task ();

  // check if line of sight blocked by a smoke
  bool IsLineBlockedBySmoke (const ystl::Vector &from, const ystl::Vector &to);

public:
  // get ammo in current weapon clip
  int GetAmmoInClip () const {
    return ammo_in_clip_[current_weapon_];
  }

  // check if bot is ducking/crouching
  bool IsDucking () const {
    return !!(pev->flags & FL_DUCKING);
  }

  // get center of bot bounding box
  ystl::Vector GetCenter () const {
    return (pev->absmax + pev->absmin) * 0.5;
  };

  // get eye position of bot
  ystl::Vector GetEyesPos () const {
    return pev->origin + pev->view_ofs;
  };

  // get current task id
  TaskId GetTaskId () {
    return Task ()->id;
  }

  // tasks that must not be disturbed once started (plant/defuse, pickups, grenade throws)
  bool HasUninterruptibleTask () {
    const auto tid = GetTaskId ();

    return tid == TaskId::PlantBomb || tid == TaskId::DefuseBomb || tid == TaskId::PickupItem || tid == TaskId::ThrowExplosive ||
           tid == TaskId::ThrowFlashbang || tid == TaskId::ThrowSmoke;
  }

  // bomb handling tasks (plant/defuse pair)
  bool HasBombTask () {
    const auto tid = GetTaskId ();

    return tid == TaskId::PlantBomb || tid == TaskId::DefuseBomb;
  }

  // breakables the bot gave up on (unbreakable, blocked or timed out)
  bool IsIgnoredBreakable (edict_t *ent) {
    return !game.IsNullEntity (ent) && ignored_breakable_.contains (ent);
  }

  // no smashing decor while frozen or right after spawn
  bool IsBreakableAllowed () const {
    return pev->maxspeed >= 10.0f && spawn_timer_.greater_than (5.0f);
  }

  // get bot entity pointer
  edict_t *Ent () const {
    return pev->pContainingEntity;
  };

  // get bot array index
  int Index () const {
    return index_;
  }

  // get entity index with worldspawn offset
  int Entindex () const {
    return index_ + 1;
  }

  // get current node index in graph
  int GetCurrentNodeIndex () const {
    return current_node_index_;
  }

  // check if low on ammo for weapon
  bool IsLowOnAmmo (const Weapon id, const float factor) const;

  // print formatted debug message
  template <typename... Args> void DebugMsg (const char *fmt, Args &&...args) {
    // dynamic buffer, debug text is unbounded
    ystl::String formatted {};
    formatted.assignf (fmt, ystl::forward<Args> (args)...);

    DebugMsgInternal (formatted);
  }

  // execute client command with format
  template <typename... Args> void IssueCommand (const char *fmt, Args &&...args);
};

// execute client command helper
template <typename... Args> void Bot::IssueCommand (const char *fmt, Args &&...args) {
  game.Command (Ent (), fmt, ystl::forward<Args> (args)...);
}

} // namespace bot
