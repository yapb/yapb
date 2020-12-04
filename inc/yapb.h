//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

#include <hlsdk/extdll.h>
#include <hlsdk/metamod.h>

#include <crlib/cr-complete.h>

// use all the cr-library
using namespace cr;

#include <product.h>
#include <module.h>

// forwards
class Bot;
class BotGraph;
class BotManager;

// defines bots tasks
CR_DECLARE_SCOPED_ENUM (Task,
   Normal = 0,
   Pause,
   MoveToPosition,
   FollowUser,
   PickupItem,
   Camp,
   PlantBomb,
   DefuseBomb,
   Attack,
   Hunt,
   SeekCover,
   ThrowExplosive,
   ThrowFlashbang,
   ThrowSmoke,
   DoubleJump,
   EscapeFromBomb,
   ShootBreakable,
   Hide,
   Blind,
   Spraypaint,
)

// bot menu ids
CR_DECLARE_SCOPED_ENUM (Menu,
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
   Commands,
   NodeMainPage1,
   NodeMainPage2,
   NodeRadius,
   NodeType,
   NodeFlag,
   NodeAutoPath,
   NodePath,
   KickPage1,
   KickPage2,
   KickPage3,
   KickPage4,
)

// bomb say string
CR_DECLARE_SCOPED_ENUM (BombPlantedSay,
  ChatSay = cr::bit (1),
  Chatter = cr::bit (2)
)

// chat types id's
CR_DECLARE_SCOPED_ENUM (Chat,
   Kill = 0, // id to kill chat array
   Dead, // id to dead chat array
   Plant, // id to bomb chat array
   TeamAttack, // id to team-attack chat array
   TeamKill, // id to team-kill chat array
   Hello, // id to welcome chat array
   NoKeyword, // id to no keyword chat array
   Count // number for array
)

// personalities defines
CR_DECLARE_SCOPED_ENUM (Personality,
   Normal = 0,
   Rusher,
   Careful,
   Invalid = -1
)

// bot difficulties
CR_DECLARE_SCOPED_ENUM (Difficulty,
   Noob,
   Easy,
   Normal,
   Hard,
   Expert,
   Invalid = -1
)

// collision states
CR_DECLARE_SCOPED_ENUM (CollisionState,
   Undecided,
   Probing,
   NoMove,
   Jump,
   Duck,
   StrafeLeft,
   StrafeRight
)

// counter-strike team id's
CR_DECLARE_SCOPED_ENUM (Team,
   Terrorist = 0,
   CT,
   Spectator,
   Unassigned,
   Invalid = -1
)

// item status for StatusIcon message
CR_DECLARE_SCOPED_ENUM (ItemStatus,
   Nightvision = cr::bit (0),
   DefusalKit = cr::bit (1)
)

// client flags
CR_DECLARE_SCOPED_ENUM (ClientFlags,
   Used = cr::bit (0),
   Alive = cr::bit (1),
   Admin = cr::bit (2),
   Icon = cr::bit (3)
)

// bot create status
CR_DECLARE_SCOPED_ENUM (BotCreateResult,
   Success,
   MaxPlayersReached,
   GraphError,
   TeamStacked
)

// radio messages
CR_DECLARE_SCOPED_ENUM (Radio,
   CoverMe = 1,
   YouTakeThePoint = 2,
   HoldThisPosition = 3,
   RegroupTeam = 4,
   FollowMe = 5,
   TakingFireNeedAssistance = 6,
   GoGoGo = 11,
   TeamFallback = 12,
   StickTogetherTeam = 13,
   GetInPositionAndWaitForGo = 14,
   StormTheFront = 15,
   ReportInTeam = 16,
   RogerThat = 21,
   EnemySpotted = 22,
   NeedBackup = 23,
   SectorClear = 24,
   ImInPosition = 25,
   ReportingIn = 26,
   ShesGonnaBlow = 27,
   Negative = 28,
   EnemyDown = 29
)

// chatter system (extending enum above, messages 30-39 is reserved)
CR_DECLARE_SCOPED_ENUM (Chatter,
   SpotTheBomber = 40,
   FriendlyFire,
   DiePain,
   Blind,
   GoingToPlantBomb,
   RescuingHostages,
   GoingToCamp,
   HeardNoise,
   TeamAttack,
   ReportingIn,
   GuardingDroppedC4,
   Camping,
   PlantingBomb,
   DefusingBomb,
   InCombat,
   SeekingEnemies,
   Nothing,
   EnemyDown,
   UsingHostages,
   FoundC4,
   WonTheRound,
   ScaredEmotion,
   HeardTheEnemy,
   SniperWarning,
   SniperKilled,
   VIPSpotted,
   GuardingVIPSafety,
   GoingToGuardVIPSafety,
   QuickWonRound,
   OneEnemyLeft,
   TwoEnemiesLeft,
   ThreeEnemiesLeft,
   NoEnemiesLeft,
   FoundC4Plant,
   WhereIsTheC4,
   DefendingBombsite,
   BarelyDefused,
   NiceShotCommander,
   NiceShotPall,
   GoingToGuardHostages,
   GoingToGuardDroppedC4,
   OnMyWay,
   LeadOnSir,
   PinnedDown,
   GottaFindC4,
   YouHeardTheMan,
   LostCommander,
   NewRound,
   CoverMe,
   BehindSmoke,
   BombsiteSecured,
   Count
)

// counter strike weapon classes (types)
CR_DECLARE_SCOPED_ENUM (WeaponType,
   None,
   Melee,
   Pistol,
   Shotgun,
   ZoomRifle,
   Rifle,
   SMG,
   Sniper,
   Heavy
)

// counter-strike weapon id's
CR_DECLARE_SCOPED_ENUM (Weapon,
   P228 = 1,
   Shield = 2,
   Scout = 3,
   Explosive = 4,
   XM1014 = 5,
   C4 = 6,
   MAC10 = 7,
   AUG = 8,
   Smoke = 9,
   Elite = 10,
   FiveSeven = 11,
   UMP45 = 12,
   SG550 = 13,
   Galil = 14,
   Famas = 15,
   USP = 16,
   Glock18 = 17,
   AWP = 18,
   MP5 = 19,
   M249 = 20,
   M3 = 21,
   M4A1 = 22,
   TMP = 23,
   G3SG1 = 24,
   Flashbang = 25,
   Deagle = 26,
   SG552 = 27,
   AK47 = 28,
   Knife = 29,
   P90 = 30,
   Armor = 31,
   ArmorHelm = 32,
   Defuser = 33
)

// buy counts
CR_DECLARE_SCOPED_ENUM (BuyState,
   PrimaryWeapon = 0,
   ArmorVestHelm,
   SecondaryWeapon,
   Ammo,
   DefusalKit,
   Grenades,
   NightVision,
   Done
)

// economics limits
CR_DECLARE_SCOPED_ENUM (EcoLimit,
   PrimaryGreater = 0,
   SmgCTGreater,
   SmgTEGreater,
   ShotgunGreater,
   ShotgunLess,
   HeavyGreater,
   HeavyLess,
   ProstockNormal,
   ProstockRusher,
   ProstockCareful,
   ShieldGreater
)

// defines for pickup items
CR_DECLARE_SCOPED_ENUM (Pickup,
   None = 0,
   Weapon,
   DroppedC4,
   PlantedC4,
   Hostage,
   Button,
   Shield,
   DefusalKit
)

// fight style type
CR_DECLARE_SCOPED_ENUM (Fight,
   None = 0,
   Strafe,
   Stay
)

// dodge type
CR_DECLARE_SCOPED_ENUM (Dodge,
   None = 0,
   Left,
   Right
)

// reload state
CR_DECLARE_SCOPED_ENUM (Reload,
   None = 0, // no reload state currently
   Primary, // primary weapon reload state
   Secondary  // secondary weapon reload state
)

// collision probes
CR_DECLARE_SCOPED_ENUM (CollisionProbe,
   Jump = cr::bit (0), // probe jump when colliding
   Duck = cr::bit (1), // probe duck when colliding
   Strafe = cr::bit (2) // probe strafing when colliding
)

// game start messages for counter-strike...
CR_DECLARE_SCOPED_ENUM (BotMsg,
   None = 1,
   TeamSelect = 2,
   ClassSelect = 3,
   Buy = 100,
   Radio = 200,
   Say = 10000,
   SayTeam = 10001
)

// sensing states
CR_DECLARE_SCOPED_ENUM (Sense,
   SeeingEnemy = cr::bit (0), // seeing an enemy
   HearingEnemy = cr::bit (1), // hearing an enemy
   SuspectEnemy = cr::bit (2), // suspect enemy behind obstacle
   PickupItem = cr::bit (3), // pickup item nearby
   ThrowExplosive = cr::bit (4), // could throw he grenade
   ThrowFlashbang = cr::bit (5), // could throw flashbang
   ThrowSmoke = cr::bit (6) // could throw smokegrenade
)

// positions to aim at
CR_DECLARE_SCOPED_ENUM (AimFlags,
   Nav = cr::bit (0), // aim at nav point
   Camp = cr::bit (1), // aim at camp vector
   PredictPath = cr::bit (2), // aim at predicted path
   LastEnemy = cr::bit (3), // aim at last enemy
   Entity = cr::bit (4), // aim at entity like buttons, hostages
   Enemy = cr::bit (5), // aim at enemy
   Grenade = cr::bit (6), // aim for grenade throw
   Override = cr::bit (7), // overrides all others (blinded)
   Danger = cr::bit (8) // additional danger falg
)

// famas/glock burst mode status + m4a1/usp silencer
CR_DECLARE_SCOPED_ENUM (BurstMode,
   On = cr::bit (0),
   Off = cr::bit (1)
)

// visibility flags
CR_DECLARE_SCOPED_ENUM (Visibility,
   Head = cr::bit (1),
   Body = cr::bit (2),
   Other = cr::bit (3),
   None = 0
)

// channel for skip traceline
CR_DECLARE_SCOPED_ENUM (TraceChannel,
   Enemy = 0,
   Body,
   Num
)

// frustum sides
CR_DECLARE_SCOPED_ENUM (FrustumSide,
   Top = 0,
   Bottom,
   Left,
   Right,
   Near,
   Far,
   Num
)

// some hardcoded desire defines used to override calculated ones
namespace TaskPri {
   static constexpr float Normal { 35.0f };
   static constexpr float Pause { 36.0f };
   static constexpr float Camp { 37.0f };
   static constexpr float Spraypaint { 38.0f };
   static constexpr float FollowUser { 39.0f };
   static constexpr float MoveToPosition { 50.0f };
   static constexpr float DefuseBomb { 89.0f };
   static constexpr float PlantBomb { 89.0f };
   static constexpr float Attack { 90.0f };
   static constexpr float SeekCover { 91.0f };
   static constexpr float Hide { 92.0f };
   static constexpr float Throw { 99.0f };
   static constexpr float DoubleJump { 99.0f };
   static constexpr float Blind { 100.0f };
   static constexpr float ShootBreakable { 100.0f };
   static constexpr float EscapeFromBomb { 100.0f };
}

// storage file magic
constexpr char kPodbotMagic[8] = "PODWAY!";

constexpr int32 kStorageMagic = 0x59415042; // storage magic for yapb-data files
constexpr int32 kStorageMagicUB = 0x544f4255; //support also the fork format (merged back into yapb)

constexpr float kInfiniteDistance = 9999999.0f;
constexpr float kGrenadeCheckTime = 2.15f;
constexpr float kSprayDistance = 260.0f;
constexpr float kDoubleSprayDistance = kSprayDistance * 2;
constexpr float kMaxChatterRepeatInteval = 99.0f;

constexpr int kInfiniteDistanceLong = static_cast <int> (kInfiniteDistance);
constexpr int kMaxNodeLinks = 8;
constexpr int kMaxPracticeDamageValue = 2040;
constexpr int kMaxPracticeGoalValue = 2040;
constexpr int kMaxNodes = 2048;
constexpr int kMaxWeapons = 32;
constexpr int kNumWeapons = 26;
constexpr int kMaxCollideMoves = 3;
constexpr int kGameMaxPlayers = 32;
constexpr int kGameTeamNum = 2;
constexpr int kInvalidNodeIndex = -1;

constexpr int kMaxBucketSize = static_cast <int> (kMaxNodes * 0.65);
constexpr int kMaxBucketsInsidePos = kMaxNodes * 8 / kMaxBucketSize + 1;
constexpr int kMaxNodesInsideBucket = kMaxBucketSize / kMaxBucketsInsidePos;

// weapon masks
constexpr auto kPrimaryWeaponMask = (cr::bit (Weapon::XM1014) | cr::bit (Weapon::M3) | cr::bit (Weapon::MAC10) | cr::bit (Weapon::UMP45) | cr::bit (Weapon::MP5) | cr::bit (Weapon::TMP) | cr::bit (Weapon::P90) | cr::bit (Weapon::AUG) | cr::bit (Weapon::M4A1) | cr::bit (Weapon::SG552) | cr::bit (Weapon::AK47) | cr::bit (Weapon::Scout) | cr::bit (Weapon::SG550) | cr::bit (Weapon::AWP) | cr::bit (Weapon::G3SG1) | cr::bit (Weapon::M249) | cr::bit (Weapon::Famas) | cr::bit (Weapon::Galil));
constexpr auto kSecondaryWeaponMask = (cr::bit (Weapon::P228) | cr::bit (Weapon::Elite) | cr::bit (Weapon::USP) | cr::bit (Weapon::Glock18) | cr::bit (Weapon::Deagle) | cr::bit (Weapon::FiveSeven));

// links keywords and replies together
struct Keywords {
   StringArray keywords;
   StringArray replies;
   StringArray usedReplies;

public:
   Keywords () = default;

   Keywords (const StringArray &keywords, const StringArray &replies) {
      this->keywords.clear ();
      this->replies.clear ();
      this->usedReplies.clear ();

      this->keywords.insert (0, keywords);
      this->replies.insert (0, replies);
   }
};

// tasks definition
struct BotTask {
   using Function = void (Bot::*) ();

public:
   Function func; // corresponding exec function in bot class
   Task id; // major task/action carried out
   float desire; // desire (filled in) for this task
   int data; // additional data (node index)
   float time; // time task expires
   bool resume; // if task can be continued if interrupted

public:
   BotTask (Function func, Task id, float desire, int data, float time, bool resume) : func (func), id (id), desire (desire), data (data), time (time), resume (resume) { }
};

// weapon properties structure
struct WeaponProp {
   String classname;
   int ammo1; // ammo index for primary ammo
   int ammo1Max; // max primary ammo
   int slot; // HUD slot (0 based)
   int pos; // slot position
   int id; // weapon ID
   int flags; // flags???
};

// weapon info structure
struct WeaponInfo {
   int id; // the weapon id value
   const char *name; // name of the weapon when selecting it
   const char *model; // model name to separate cs weapons
   int price; // price when buying
   int minPrimaryAmmo; // minimum primary ammo
   int teamStandard; // used by team (number) (standard map)
   int teamAS; // used by team (as map)
   int buyGroup; // group in buy menu (standard map)
   int buySelect; // select item in buy menu (standard map)
   int buySelectT; // for counter-strike v1.6
   int buySelectCT; // for counter-strike v1.6
   int penetratePower; // penetrate power
   int maxClip; // max ammo in clip
   int type; // weapon class
   bool primaryFireHold; // hold down primary fire button to use?

public:
   WeaponInfo (int id, 
      const char *name,
      const char *model,
      int price,
      int minPriAmmo,
      int teamStd,
      int teamAs,
      int buyGroup,
      int buySelect,
      int buySelectT,
      int buySelectCT, 
      int penetratePower,
      int maxClip,
      int type,
      bool fireHold) :  id (id), name (name), model (model), price (price), minPrimaryAmmo (minPriAmmo), teamStandard (teamStd), 
      teamAS (teamAs), buyGroup (buyGroup), buySelect (buySelect), buySelectT (buySelectT), buySelectCT (buySelectCT),
      penetratePower (penetratePower), maxClip (maxClip), type (type), primaryFireHold (fireHold)
   { }
};

// clients noise
struct ClientNoise {
   Vector pos;
   float dist;
   float last;
};

// defines frustum data for bot
struct FrustumPlane {
   Vector normal;
   Vector point;
   float result;
};

// array of clients struct
struct Client {
   edict_t *ent; // pointer to actual edict
   Vector origin; // position in the world
   int team; // bot team
   int team2; // real bot team in free for all mode (csdm)
   int flags; // client flags
   int radio; // radio orders
   int menu; // identifier to openen menu
   int ping; // when bot latency is enabled, client ping stored here
   int iconFlags[kGameMaxPlayers]; // flag holding chatter icons
   float iconTimestamp[kGameMaxPlayers]; // timers for chatter icons
   bool pingUpdate; // update ping ?
   ClientNoise noise;
};

// define chatting collection structure
struct ChatCollection {
   int chatProbability;
   float chatDelay;
   float timeNextChat;
   int entityIndex;
   String sayText;
   StringArray lastUsedSentences;
};

// include bot graph stuff
#include <graph.h>

// main bot class
class Bot final {
private:
   using RouteTwin = Twin <int, float>;

public:
   friend class BotManager;

private:
   uint32 m_states {}; // sensing bitstates
   uint32 m_collideMoves[kMaxCollideMoves] {}; // sorted array of movements
   uint32 m_collisionProbeBits {}; // bits of possible collision moves
   uint32 m_collStateIndex {}; // index into collide moves
   uint32 m_aimFlags {}; // aiming conditions
   uint32 m_currentTravelFlags {}; // connection flags like jumping

   int m_traceSkip[TraceChannel::Num] {}; // trace need to be skipped?
   int m_messageQueue[32] {}; // stack for messages

   int m_oldButtons {}; // our old buttons
   int m_reloadState {}; // current reload state
   int m_voicePitch; // bot voice pitch
   int m_loosedBombNodeIndex {}; // nearest to loosed bomb node
   int m_plantedBombNodeIndex {}; // nearest to planted bomb node
   int m_currentNodeIndex {}; // current node index
   int m_travelStartIndex {}; // travel start index to double jump action
   int m_previousNodes[5] {}; // previous node indices from node find
   int m_pathFlags {}; // current node flags
   int m_needAvoidGrenade {}; // which direction to strafe away
   int m_campDirection {}; // camp Facing direction
   int m_campButtons {}; // buttons to press while camping
   int m_tryOpenDoor {}; // attempt's to open the door
   int m_liftState {}; // state of lift handling
   int m_radioSelect {}; // radio entry
   
   float m_headedTime {};
   float m_prevTime {}; // time previously checked movement speed
   float m_heavyTimestamp; // is it time to execute heavy-weight functions
   float m_prevSpeed {}; // speed some frames before
   float m_timeDoorOpen {}; // time to next door open check
   float m_lastChatTime {}; // time bot last chatted
   float m_timeLogoSpray {}; // time bot last spray logo
   float m_knifeAttackTime {}; // time to rush with knife (at the beginning of the round)
   float m_duckDefuseCheckTime {}; // time to check for ducking for defuse
   float m_frameInterval; // bot's frame interval
   float m_lastCommandTime; // time bot last thinked
   float m_reloadCheckTime {}; // time to check reloading
   float m_zoomCheckTime {}; // time to check zoom again
   float m_shieldCheckTime {}; // time to check shiled drawing again
   float m_grenadeCheckTime {}; // time to check grenade usage
   float m_sniperStopTime {}; // bot switched to other weapon?
   float m_lastEquipTime {}; // last time we equipped in buyzone
   float m_duckTime {}; // time to duck
   float m_jumpTime {}; // time last jump happened
   float m_soundUpdateTime {}; // time to update the sound
   float m_heardSoundTime {}; // last time noise is heard
   float m_buttonPushTime {}; // time to push the button
   float m_liftUsageTime {}; // time to use lift
   float m_askCheckTime {}; // time to ask team
   float m_collideTime {}; // time last collision
   float m_firstCollideTime {}; // time of first collision
   float m_probeTime {}; // time of probing different moves
   float m_lastCollTime {}; // time until next collision check
   float m_lookYawVel {}; // look yaw velocity
   float m_lookPitchVel {}; // look pitch velocity
   float m_lookUpdateTime {}; // lookangles update time
   float m_aimErrorTime {}; // time to update error vector
   float m_nextCampDirTime {}; // time next camp direction change
   float m_lastFightStyleCheck {}; // time checked style
   float m_strafeSetTime {}; // time strafe direction was set
   float m_randomizeAnglesTime {}; // time last randomized location
   float m_playerTargetTime {}; // time last targeting
   float m_timeCamping {}; // time to camp
   float m_lastUsedNodesTime {}; // last time bot followed nodes
   float m_shootAtDeadTime {}; // time to shoot at dying players
   float m_followWaitTime {}; // wait to follow time
   float m_chatterTimes[Chatter::Count] {}; // chatter command timers
   float m_navTimeset {}; // time node chosen by Bot
   float m_moveSpeed {}; // current speed forward/backward
   float m_strafeSpeed {}; // current speed sideways
   float m_minSpeed {}; // minimum speed in normal mode
   float m_oldCombatDesire {}; // holds old desire for filtering
   float m_itemCheckTime {}; // time next search for items needs to be done
   float m_joinServerTime; // time when bot joined the game
   float m_playServerTime; // time bot spent in the game
   float m_changeViewTime {}; // timestamp to change look at while at freezetime

   bool m_moveToGoal {}; // bot currently moving to goal??
   bool m_isStuck {}; // bot is stuck
   bool m_isReloading {}; // bot is reloading a gun
   bool m_forceRadio; // should bot use radio anyway?
   bool m_defendedBomb {}; // defend action issued
   bool m_defendHostage {}; // defend action issued
   bool m_duckDefuse {}; // should or not bot duck to defuse bomb
   bool m_checkKnifeSwitch {}; // is time to check switch to knife action
   bool m_checkWeaponSwitch {}; // is time to check weapon switch
   bool m_isUsingGrenade {}; // bot currently using grenade??
   bool m_bombSearchOverridden {}; // use normal node if applicable when near the bomb
   bool m_wantsToFire {}; // bot needs consider firing
   bool m_jumpFinished {}; // has bot finished jumping
   bool m_isLeader {}; // bot is leader of his team
   bool m_checkTerrain {}; // check for terrain
   bool m_moveToC4 {}; // ct is moving to bomb
   bool m_grenadeRequested {}; // bot requested change to grenade

   Pickup m_pickupType; // type of entity which needs to be used/picked up
   PathWalk m_pathWalk; // pointer to current node from path
   Dodge m_combatStrafeDir; // direction to strafe
   Fight m_fightStyle; // combat style to use
   CollisionState m_collisionState; // collision State
   FindPath m_pathType; // which pathfinder to use
   uint8 m_enemyParts {}; // visibility flags
   TraceResult m_lastTrace[TraceChannel::Num]; // last trace result

   edict_t *m_pickupItem {}; // pointer to entity of item to use/pickup
   edict_t *m_itemIgnore {}; // pointer to entity to ignore for pickup
   edict_t *m_liftEntity {}; // pointer to lift entity
   edict_t *m_breakableEntity {}; // pointer to breakable entity
   edict_t *m_targetEntity {}; // the entity that the bot is trying to reach
   edict_t *m_avoidGrenade {}; // pointer to grenade entity to avoid

   Vector m_liftTravelPos; // lift travel position
   Vector m_moveAngles; // bot move angles
   Vector m_idealAngles; // angle wanted
   Vector m_randomizedIdealAngles; // angle wanted with noise
   Vector m_angularDeviation; // angular deviation from current to ideal angles
   Vector m_aimSpeed; // aim speed calculated
   Vector m_aimLastError; // last calculated aim error
   Vector m_prevOrigin; // origin some frames before
   Vector m_lookAt; // vector bot should look at
   Vector m_throw; // origin of node to throw grenades
   Vector m_enemyOrigin; // target origin chosen for shooting
   Vector m_grenade; // calculated vector for grenades
   Vector m_entity; // origin of entities like buttons etc.
   Vector m_camp; // aiming vector when camping.
   Vector m_desiredVelocity; // desired velocity for jump nodes
   Vector m_breakableOrigin; // origin of breakable

   Array <edict_t *> m_hostages; // pointer to used hostage entities
   Array <Route> m_routes; // pointer

   BinaryHeap <RouteTwin> m_routeQue;
   Path *m_path {}; // pointer to the current path node
   String m_chatBuffer; // space for strings (say text...)
   FrustumPlane m_frustum[FrustumSide::Num]  {};

private:
   int pickBestWeapon (int *vec, int count, int moneySave);
   int findCampingDirection ();
   int findAimingNode (const Vector &to);
   int findNearestNode ();
   int findBombNode ();
   int findCoverNode (float maxDistance);
   int findDefendNode (const Vector &origin);
   int findBestGoal ();
   int findGoalPost (int tactic, IntArray *defensive, IntArray *offsensive);
   int bestPrimaryCarried ();
   int bestSecondaryCarried ();
   int bestGrenadeCarried ();
   int bestWeaponCarried ();
   int changePointIndex (int index);
   int numEnemiesNear (const Vector &origin, float radius);
   int numFriendsNear (const Vector &origin, float radius);

   float getBombTimeleft ();
   float getReachTime ();
   float isInFOV (const Vector &dest);
   float getShiftSpeed ();
   float getEnemyBodyOffsetCorrection (float distance);

   bool canReplaceWeapon ();
   bool canDuckUnder (const Vector &normal);
   bool canJumpUp (const Vector &normal);
   bool doneCanJumpUp (const Vector &normal, const Vector &right);
   bool cantMoveForward (const Vector &normal, TraceResult *tr);
   bool canStrafeLeft (TraceResult *tr);
   bool canStrafeRight (TraceResult *tr);
   bool isBlockedLeft ();
   bool isBlockedRight ();
   bool checkWallOnLeft ();
   bool checkWallOnRight ();
   bool updateNavigation ();
   bool isEnemyThreat ();
   bool isWeaponRestricted (int weaponIndex);
   bool isWeaponRestrictedAMX (int weaponIndex);
   bool isInViewCone (const Vector &origin);
   bool checkBodyParts (edict_t *target);
   bool seesEnemy (edict_t *player, bool ignoreFOV = false);
   bool doPlayerAvoidance (const Vector &normal);
   bool hasActiveGoal ();
   bool advanceMovement ();
   bool isBombDefusing (const Vector &bombOrigin);
   bool isOccupiedNode (int index, bool needZeroVelocity = false);
   bool seesItem (const Vector &dest, const char *classname);
   bool lastEnemyShootable ();
   bool rateGroundWeapon (edict_t *ent);
   bool reactOnEnemy ();
   bool selectBestNextNode ();
   bool hasAnyWeapons ();
   bool isDeadlyMove (const Vector &to);
   bool isOutOfBombTimer ();
   bool isWeaponBadAtDistance (int weaponIndex, float distance);
   bool needToPauseFiring (float distance);
   bool lookupEnemies ();
   bool isEnemyHidden (edict_t *enemy);
   bool isEnemyInvincible (edict_t *enemy);
   bool isFriendInLineOfFire (float distance);
   bool isGroupOfEnemies (const Vector &location, int numEnemies = 1, float radius = 256.0f);
   bool isPenetrableObstacle (const Vector &dest);
   bool isPenetrableObstacle2 (const Vector &dest);
   bool isEnemyBehindShield (edict_t *enemy);
   bool isEnemyInFrustum (edict_t *enemy);
   bool checkChatKeywords (String &reply);
   bool isReplyingToChat ();
   bool isReachableNode (int index);
   bool updateLiftHandling ();
   bool updateLiftStates ();
   bool canRunHeavyWeight ();
   bool isEnemyInSight (Vector &endPos);

   void markStale ();
   void instantChatter (int type);
   void update ();
   void runMovement ();
   void checkSpawnConditions ();
   void buyStuff ();
   void changePitch (float speed);
   void changeYaw (float speed);
   void checkMsgQueue ();
   void checkRadioQueue ();
   void checkReload ();
   void avoidGrenades ();
   void checkGrenadesThrow ();
   void checkBurstMode (float distance);
   void checkSilencer ();
   void updateAimDir ();
   void updateLookAngles ();
   void updateBodyAngles ();
   void updateLookAnglesNewbie (const Vector &direction, float delta);
   void setIdealReactionTimers (bool actual = false);
   void updateHearing ();
   void postprocessGoals (const IntArray &goals, int result[]);
   void updatePickups ();
   void checkTerrain (float movedDistance, const Vector &dirNormal);
   void checkDarkness ();
   void checkParachute ();
   void getCampDirection (Vector *dest);
   void updatePracticeValue (int damage);
   void updatePracticeDamage (edict_t *attacker, int damage);
   void findShortestPath (int srcIndex, int destIndex);
   void calculateFrustum ();

   void findPath (int srcIndex, int destIndex, FindPath pathType = FindPath::Fast);
   void clearRoute ();
   void debugMsgInternal (const char *str);
   void frame ();
   void resetCollision ();
   void ignoreCollision ();
   void setConditions ();
   void overrideConditions ();
   void updateEmotions ();
   void setStrafeSpeed (const Vector &moveDir, float strafeSpeed);
   void updateTeamJoin ();
   void updateTeamCommands ();
   void decideFollowUser ();
   void attackMovement ();
   void findValidNode ();
   void fireWeapons ();
   void selectWeapons (float distance, int index, int id, int choosen);
   void focusEnemy ();
   void selectBestWeapon ();
   void selectSecondary ();
   void selectWeaponByName (const char *name);
   void selectWeaponById (int num);

   void completeTask ();
   void executeTasks ();
   void trackEnemies ();
   void choiceFreezetimeEntity ();

   void normal_ ();
   void spraypaint_ ();
   void huntEnemy_ ();
   void seekCover_ ();
   void attackEnemy_ ();
   void pause_ ();
   void blind_ ();
   void camp_ ();
   void hide_ ();
   void moveToPos_ ();
   void plantBomb_ ();
   void defuseBomb_ ();
   void followUser_ ();
   void throwExplosive_ ();
   void throwFlashbang_ ();
   void throwSmoke_ ();
   void doublejump_ ();
   void escapeFromBomb_ ();
   void pickupItem_ ();
   void shootBreakable_ ();

   edict_t *lookupButton (const char *targetName);
   edict_t *lookupBreakable ();
   edict_t *correctGrenadeVelocity (const char *model);

   const Vector &getEnemyBodyOffset ();
   Vector calcThrow (const Vector &start, const Vector &stop);
   Vector calcToss (const Vector &start, const Vector &stop);
   Vector isBombAudible ();
   Vector getBodyOffsetError (float distance);

   uint8 computeMsec ();

private:
   bool isOnLadder () const {
      return pev->movetype == MOVETYPE_FLY;
   }

   bool isOnFloor () const {
      return (pev->flags & (FL_ONGROUND | FL_PARTIALGROUND)) != 0;
   }

   bool isInWater () const {
      return pev->waterlevel >= 2;
   }

   bool isInNarrowPlace () const {
      return (m_pathFlags & NodeFlag::Narrow);
   }

public:
   entvars_t *pev;

   int m_index; // saved bot index
   int m_wantedTeam; // player team bot wants select
   int m_wantedSkin; // player model bot wants to select
   int m_difficulty; // bots hard level
   int m_moneyAmount; // amount of money in bot's bank

   float m_spawnTime {}; // time this bot spawned
   float m_timeTeamOrder {}; // time of last radio command
   float m_slowFrameTimestamp; // time to per-second think
   float m_nextBuyTime {}; // next buy time
   float m_checkDarkTime {}; // check for darkness time
   float m_preventFlashing {}; // bot turned away from flashbang
   float m_flashLevel {}; // flashlight level
   float m_blindTime {}; // time when bot is blinded
   float m_blindMoveSpeed {}; // mad speeds when bot is blind
   float m_blindSidemoveSpeed {}; // mad side move speeds when bot is blind
   float m_fallDownTime {}; // time bot started to fall 
   float m_duckForJump {}; // is bot needed to duck for double jump
   float m_baseAgressionLevel; // base aggression level (on initializing)
   float m_baseFearLevel; // base fear level (on initializing)
   float m_agressionLevel; // dynamic aggression level (in game)
   float m_fearLevel; // dynamic fear level (in game)
   float m_nextEmotionUpdate; // next time to sanitize emotions
   float m_updateTime {}; // skip some frames in bot thinking
   float m_updateInterval {}; // interval between frames
   float m_goalValue {}; // ranking value for this node
   float m_viewDistance {}; // current view distance
   float m_maxViewDistance {}; // maximum view distance
   float m_retreatTime {}; // time to retreat?
   float m_enemyUpdateTime {}; // time to check for new enemies
   float m_enemyReachableTimer {}; // time to recheck if enemy reachable
   float m_enemyIgnoreTimer {}; // ignore enemy for some time
   float m_seeEnemyTime {}; // time bot sees enemy
   float m_enemySurpriseTime {}; // time of surprise
   float m_idealReactionTime {}; // time of base reaction
   float m_actualReactionTime {}; // time of current reaction time
   float m_timeNextTracking {}; // time node index for tracking player is recalculated
   float m_firePause {}; // time to pause firing
   float m_shootTime {}; // time to shoot
   float m_timeLastFired {}; // time to last firing
   float m_difficultyChange {}; // time when auto-difficulty was last applied to this bot
   float m_kpdRatio; // kill per death ratio
   float m_healthValue; // clamped bot health 

   int m_basePing; // base ping for bot
   int m_numEnemiesLeft {}; // number of enemies alive left on map
   int m_numFriendsLeft {}; // number of friend alive left on map
   int m_retryJoin; // retry count for chosing team/class
   int m_startAction; // team/class selection state
   int m_voteKickIndex {}; // index of player to vote against
   int m_lastVoteKick {}; // last index
   int m_voteMap {}; // number of map to vote for
   int m_logotypeIndex; // index for logotype
   int m_buyState {}; // current count in buying
   int m_blindButton {}; // buttons bot press, when blind
   int m_radioOrder {}; // actual command
   int m_prevGoalIndex {}; // holds destination goal node
   int m_chosenGoalIndex {}; // used for experience, same as above
   int m_lastDamageType {}; // stores last damage
   int m_team {}; // bot team
   int m_currentWeapon; // one current weapon for each bot
   int m_weaponType; // current weapon type
   int m_ammoInClip[kMaxWeapons] {}; // ammo in clip for each weapons
   int m_ammo[MAX_AMMO_SLOTS] {}; // total ammo amounts

   bool m_isVIP {}; // bot is vip?
   bool m_notKilled; // has the player been killed or has he just respawned
   bool m_notStarted; // team/class not chosen yet
   bool m_ignoreBuyDelay {}; // when reaching buyzone in the middle of the round don't do pauses
   bool m_inBombZone {}; // bot in the bomb zone or not
   bool m_inBuyZone {}; // bot currently in buy zone
   bool m_inVIPZone {}; // bot in the vip satefy zone
   bool m_buyingFinished {}; // done with buying
   bool m_buyPending {}; // bot buy is pending
   bool m_hasDefuser {}; // does bot has defuser
   bool m_hasNVG {}; // does bot has nightvision goggles
   bool m_usesNVG {}; // does nightvision goggles turned on
   bool m_hasC4 {}; // does bot has c4 bomb
   bool m_hasProgressBar {}; // has progress bar on a HUD
   bool m_jumpReady {}; // is double jump ready
   bool m_canChooseAimDirection {}; // can choose aiming direction
   bool m_isEnemyReachable {}; // direct line to enemy

   edict_t *m_doubleJumpEntity {}; // pointer to entity that request double jump
   edict_t *m_radioEntity {}; // pointer to entity issuing a radio command
   edict_t *m_enemy {}; // pointer to enemy entity
   edict_t *m_lastEnemy {}; // pointer to last enemy entity
   edict_t *m_lastVictim {}; // pointer to killed entity
   edict_t *m_trackingEdict {}; // pointer to last tracked player when camping/hiding

   Vector m_pathOrigin; // origin of node
   Vector m_destOrigin; // origin of move destination
   Vector m_position; // position to move to in move to position task
   Vector m_doubleJumpOrigin; // origin of double jump
   Vector m_lastEnemyOrigin; // vector to last enemy origin
  
   ChatCollection m_sayTextBuffer; // holds the index & the actual message of the last unprocessed text message of a player
   BurstMode m_weaponBurstMode; // bot using burst mode? (famas/glock18, but also silencer mode)
   Personality m_personality; // bots type
   Array <BotTask> m_tasks;
   Deque <int32> m_msgQueue;

public:
   Bot (edict_t *bot, int difficulty, int personality, int team, int skin);
   ~Bot () = default;

public:
   void logic (); /// the things that can be executed while skipping frames
   void spawned ();
   void takeBlind (int alpha);
   void takeDamage (edict_t *inflictor, int damage, int armor, int bits);
   void showDebugOverlay ();
   void newRound ();
   void enteredBuyZone (int buyState);
   void pushMsgQueue (int message);
   void prepareChatMessage (StringRef message);
   void checkForChat ();
   void showChaterIcon (bool show);
   void clearSearchNodes ();
   void checkBreakable (edict_t *touch);
   void checkBreakablesAround ();
   void startTask (Task id, float desire, int data, float time, bool resume);
   void clearTask (Task id);
   void filterTasks ();
   void clearTasks ();
   void dropWeaponForUser (edict_t *user, bool discardC4);
   void sendToChat (StringRef message, bool teamOnly);
   void pushChatMessage (int type, bool isTeamSay = false);
   void pushRadioMessage (int message);
   void pushChatterMessage (int message);
   void tryHeadTowardRadioMessage ();
   void kill ();
   void kick ();
   void resetDoubleJump ();
   void startDoubleJump (edict_t *ent);
   void sendBotToOrigin (const Vector &origin);

   bool hasHostage ();
   bool usesRifle ();
   bool usesPistol ();
   bool usesSniper ();
   bool usesSubmachine ();
   bool usesShotgun ();
   bool usesHeavy ();
   bool usesZoomableRifle ();
   bool usesBadWeapon ();
   bool usesCampGun ();
   bool usesKnife ();
   bool hasPrimaryWeapon ();
   bool hasSecondaryWeapon ();
   bool hasShield ();
   bool isShieldDrawn ();
   bool findBestNearestNode ();
   bool seesEntity (const Vector &dest, bool fromBody = false);
   bool canSkipNextTrace (TraceChannel channel);

   int getAmmo ();
   int getNearestToPlantedBomb ();

   float getFrameInterval ();
   float getConnectionTime ();
   BotTask *getTask ();

public:
   int getAmmoInClip () const {
      return m_ammoInClip[m_currentWeapon];
   }
   
   Vector getCenter () const {
      return (pev->absmax + pev->absmin) * 0.5;
   };

   Vector getEyesPos () const {
      return pev->origin + pev->view_ofs;
   };

   Task getCurrentTaskId ()  {
      return getTask ()->id;
   }

   edict_t *ent () {
      return pev->pContainingEntity;
   };

   // bots array index
   int index () const {
      return m_index;
   }

   // entity index with worldspawn shift
   int entindex () const {
      return m_index + 1;
   }

   // get the current node index 
   int getCurrentNodeIndex () const {
      return m_currentNodeIndex;
   }

   // set the last bot trace result
   void setLastTraceResult (TraceChannel channel, TraceResult *traceResult) {
      m_lastTrace[channel] = *traceResult;
   }

   // get the bot last trace result
   TraceResult *getLastTraceResult (TraceChannel channel) {
      return &m_lastTrace[channel];
   }

   // prints debug message
   template <typename ...Args> void debugMsg (const char *fmt, Args &&...args) {
      debugMsgInternal (strings.format (fmt, cr::forward <Args> (args)...));
   }

   // execute client command helper
   template <typename ...Args> void issueCommand (const char *fmt, Args &&...args);
};

#include <config.h>
#include <support.h>
#include <message.h>
#include <engine.h>
#include <manager.h>
#include <control.h>

// very global convars
extern ConVar cv_jasonmode;
extern ConVar cv_radio_mode;
extern ConVar cv_ignore_enemies;
extern ConVar cv_chat;
extern ConVar cv_language;
extern ConVar cv_show_latency;
extern ConVar cv_enable_query_hook;
extern ConVar cv_whose_your_daddy;
extern ConVar cv_chatter_path;
extern ConVar cv_quota;
extern ConVar cv_difficulty;
extern ConVar cv_attack_monsters;
extern ConVar cv_pickup_custom_items;

// execute client command helper
template <typename ...Args> void Bot::issueCommand (const char *fmt, Args &&...args) {
   game.botCommand (ent (), strings.format (fmt, cr::forward <Args> (args)...));
}
