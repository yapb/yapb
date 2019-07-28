//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <engine/extdll.h>
#include <engine/meta_api.h>

#include <crlib/cr-complete.h>

// use all the cr-library
using namespace cr;

#include <resource.h>

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
);

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
);

// bomb say string
CR_DECLARE_SCOPED_ENUM (BombPlantedSay,
  ChatSay = cr::bit (1),
  Chatter = cr::bit (2)
);

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
);

// personalities defines
CR_DECLARE_SCOPED_ENUM (Personality,
   Normal = 0,
   Rusher,
   Careful,
   Invalid = -1
);

// bot difficulties
CR_DECLARE_SCOPED_ENUM (Difficulty,
   VeryEasy,
   Easy,
   Normal,
   Hard,
   Extreme,
   Invalid = -1
);

// collision states
CR_DECLARE_SCOPED_ENUM (CollisionState,
   Undecided,
   Probing,
   NoMove,
   Jump,
   Duck,
   StrafeLeft,
   StrafeRight
);

// counter-strike team id's
CR_DECLARE_SCOPED_ENUM (Team,
   Terrorist = 0,
   CT,
   Spectator,
   Unassigned,
   Invalid = -1
);

// item status for StatusIcon message
CR_DECLARE_SCOPED_ENUM (ItemStatus,
   Nightvision = cr::bit (0),
   DefusalKit = cr::bit (1)
);

// client flags
CR_DECLARE_SCOPED_ENUM (ClientFlags,
   Used = cr::bit (0),
   Alive = cr::bit (1),
   Admin = cr::bit (2),
   Icon = cr::bit (3)
);

// bot create status
CR_DECLARE_SCOPED_ENUM (BotCreateResult,
   Success,
   MaxPlayersReached,
   GraphError,
   TeamStacked
);

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
);

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
);

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
);

// buy counts
CR_DECLARE_SCOPED_ENUM (BuyState,
   PrimaryWeapon = 0,
   ArmorVestHelm ,
   SecondaryWeapon,
   Grenades,
   DefusalKit,
   Ammo,
   NightVision,
   Done
);

// economics limits
CR_DECLARE_SCOPED_ENUM (EcoLimit,
   PrimaryGreater = 0,
   SmgCTGreater,
   SmgTEGreater,
   ShotgunGreater,
   ShotgunLess,
   HeavyGreater ,
   HeavyLess,
   ProstockNormal,
   ProstockRusher,
   ProstockCareful,
   ShieldGreater
);

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
);

// fight style type
CR_DECLARE_SCOPED_ENUM (Fight,
   None = 0,
   Strafe,
   Stay
);

// dodge type
CR_DECLARE_SCOPED_ENUM (Dodge,
   None = 0,
   Left,
   Right
);

// reload state
CR_DECLARE_SCOPED_ENUM (Reload,
   None = 0, // no reload state currently
   Primary, // primary weapon reload state
   Secondary  // secondary weapon reload state
);

// collision probes
CR_DECLARE_SCOPED_ENUM (CollisionProbe,
   Jump = cr::bit (0), // probe jump when colliding
   Duck = cr::bit (1), // probe duck when colliding
   Strafe = cr::bit (2) // probe strafing when colliding
);

// vgui menus (since latest steam updates is obsolete, but left for old cs)
CR_DECLARE_SCOPED_ENUM (GuiMenu,
   TeamSelect = 2, // menu select team
   TerroristSelect = 26, // terrorist select menu
   CTSelect = 27  // ct select menu
);

// lift usage states
CR_DECLARE_SCOPED_ENUM (LiftState,
   None = 0,
   LookingButtonOutside,
   WaitingFor,
   EnteringIn,
   WaitingForTeammates,
   LookingButtonInside,
   TravelingBy,
   Leaving
);

// game start messages for counter-strike...
CR_DECLARE_SCOPED_ENUM (BotMsg,
   None = 1,
   TeamSelect = 2,
   ClassSelect = 3,
   Buy = 100,
   Radio = 200,
   Say = 10000,
   SayTeam = 10001
);

// sensing states
CR_DECLARE_SCOPED_ENUM (Sense,
   SeeingEnemy = cr::bit (0), // seeing an enemy
   HearingEnemy = cr::bit (1), // hearing an enemy
   SuspectEnemy = cr::bit (2), // suspect enemy behind obstacle
   PickupItem = cr::bit (3), // pickup item nearby
   ThrowExplosive = cr::bit (4), // could throw he grenade
   ThrowFlashbang = cr::bit (5), // could throw flashbang
   ThrowSmoke = cr::bit (6) // could throw smokegrenade
);

// positions to aim at
CR_DECLARE_SCOPED_ENUM (AimFlags,
   Nav = cr::bit (0), // aim at nav point
   Camp = cr::bit (1), // aim at camp vector
   PredictPath = cr::bit (2), // aim at predicted path
   LastEnemy = cr::bit (3), // aim at last enemy
   Entity = cr::bit (4), // aim at entity like buttons, hostages
   Enemy = cr::bit (5), // aim at enemy
   Grenade = cr::bit (6), // aim for grenade throw
   Override = cr::bit (7) // overrides all others (blinded)
);

// famas/glock burst mode status + m4a1/usp silencer
CR_DECLARE_SCOPED_ENUM (BurstMode,
   On = cr::bit (0),
   Off = cr::bit (1)
);

// visibility flags
CR_DECLARE_SCOPED_ENUM (Visibility,
   Head = cr::bit (1),
   Body = cr::bit (2),
   Other = cr::bit (3)
);

// command handler status
CR_DECLARE_SCOPED_ENUM (BotCommandResult,
   Handled = 0, // command successfully handled 
   ListenServer, // command is only avaialble on listen server
   BadFormat // wrong params
);

// defines for nodes flags field (32 bits are available)
CR_DECLARE_SCOPED_ENUM (NodeFlag,
   Lift = cr::bit (1), // wait for lift to be down before approaching this node
   Crouch = cr::bit (2), // must crouch to reach this node
   Crossing = cr::bit (3), // a target node
   Goal = cr::bit (4), // mission goal point (bomb, hostage etc.)
   Ladder = cr::bit (5), // node is on ladder
   Rescue = cr::bit (6), // node is a hostage rescue point
   Camp = cr::bit (7), // node is a camping point
   NoHostage = cr::bit (8), // only use this node if no hostage
   DoubleJump = cr::bit (9), // bot help's another bot (requster) to get somewhere (using djump)
   Sniper = cr::bit (28), // it's a specific sniper point
   TerroristOnly = cr::bit (29), // it's a specific terrorist point
   CTOnly = cr::bit (30),  // it's a specific ct point
);

// defines for node connection flags field (16 bits are available)
CR_DECLARE_SCOPED_ENUM_TYPE (PathFlag, uint16,
   Jump = cr::bit (0) // must jump for this connection
);

// enum pathfind search type
CR_DECLARE_SCOPED_ENUM (FindPath,
   Fast = 0,
   Optimal,
   Safe
);

// defines node connection types
CR_DECLARE_SCOPED_ENUM (PathConnection,
   Outgoing = 0,
   Incoming,
   Bidirectional
);

// defines node add commands
CR_DECLARE_SCOPED_ENUM (GraphAdd,
   Normal = 0,
);

// a* route state
CR_DECLARE_SCOPED_ENUM (RouteState,
   Open = 0,
   Closed,
   New
);

// node edit states
CR_DECLARE_SCOPED_ENUM (GraphEdit,
   On = cr::bit (1),
   Noclip = cr::bit (2),
   Auto = cr::bit (3)
);

CR_DECLARE_SCOPED_ENUM (StorageOption,
   Practice = cr::bit (0), // this is practice (experience) file
   Matrix = cr::bit (1), // this is floyd warshal path & distance matrix
   Vistable = cr::bit (2), // this is vistable data
   Graph = cr::bit (3), // this is a node graph data
   Official = cr::bit (4), // this is additional flag for graph indicates graph are official
   Recovered = cr::bit (5), // this is additional flag indicates graph converted from podbot and was bad
   Author = cr::bit (6) // this is additional flag indicates that there's author info
);

CR_DECLARE_SCOPED_ENUM (StorageVersion,
   Graph = 1,
   Practice = 1,
   Vistable = 1,
   Matrix = 1,
   Podbot = 7
);

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
constexpr int32 kStorageMagic = 0x59415042;

constexpr float kGrenadeCheckTime = 2.15f;
constexpr float kSprayDistance = 260.0f;
constexpr float kDoubleSprayDistance = kSprayDistance * 2;
constexpr float kMaxChatterRepeatInteval = 99.0f;

constexpr int kMaxNodeLinks = 8;
constexpr int kMaxPracticeDamageValue = 2040;
constexpr int kMaxPracticeGoalValue = 2040;
constexpr int kMaxNodes = 2048;
constexpr int kMaxRouteLength = kMaxNodes / 2;
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

// a* route
struct Route {
   float g, f;
   int parent;
   RouteState state;
};

// general stprage header information structure
struct StorageHeader {
   int32 magic;
   int32 version;
   int32 options;
   int32 length;
   int32 compressed;
   int32 uncompressed;
};

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
   Task id; // major task/action carried out
   float desire; // desire (filled in) for this task
   int data; // additional data (node index)
   float time; // time task expires
   bool resume; // if task can be continued if interrupted

public:
   BotTask (Task id, float desire, int data, float time, bool resume) : id (id), desire (desire), data (data), time (time), resume (resume) { }
};

// botname structure definition
struct BotName {
   String name = "";
   int usedBy = -1;

public:
   BotName () = default;
   BotName (String &name, int usedBy) : name (cr::move (name)), usedBy (usedBy) { }
};

// voice config structure definition
struct ChatterItem {
   String name;
   float repeat;
   float duration;

public:
   ChatterItem (String name, float repeat, float duration) : name (cr::move (name)), repeat (repeat), duration (duration) { }
};

// weapon properties structure
struct WeaponProp {
   char classname[64];
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
   int newBuySelectT; // for counter-strike v1.6
   int newBuySelectCT; // for counter-strike v1.6
   int penetratePower; // penetrate power
   int maxClip; // max ammo in clip
   bool primaryFireHold; // hold down primary fire button to use?

public:
   WeaponInfo (int id, const char *name, const char *model, int price, int minPriAmmo, int teamStd,
      int teamAs, int buyGroup, int buySelect, int newBuySelectT, int newBuySelectCT, int penetratePower, 
      int maxClip, bool fireHold) {

      this->id = id;
      this->name = name;
      this->model = model;
      this->price = price;
      this->minPrimaryAmmo = minPriAmmo;
      this->teamStandard = teamStd;
      this->teamAS = teamAs;
      this->buyGroup = buyGroup;
      this->buySelect = buySelect;
      this->newBuySelectCT = newBuySelectCT;
      this->newBuySelectT = newBuySelectT;
      this->penetratePower = penetratePower;
      this->maxClip = maxClip;
      this->primaryFireHold = fireHold;
   }
};

// array of clients struct
struct Client {
   edict_t *ent; // pointer to actual edict
   Vector origin; // position in the world
   Vector sound; // position sound was played
   int team; // bot team
   int team2; // real bot team in free for all mode (csdm)
   int flags; // client flags
   int radio; // radio orders
   int menu; // identifier to openen menu
   int ping; // when bot latency is enabled, client ping stored here
   float hearingDistance; // distance this sound is heared
   float timeSoundLasting; // time sound is played/heared
   int iconFlags[kGameMaxPlayers]; // flag holding chatter icons
   float iconTimestamp[kGameMaxPlayers]; // timers for chatter icons
   bool pingUpdate; // update ping ?
};

// bot creation tab
struct CreateQueue {
   bool manual;
   int difficulty;
   int team;
   int member;
   int personality;
   String name;
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

// general waypoint header information structure
struct PODGraphHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   char mapName[32];
   char author[32];
};

// floyd-warshall matrices
struct Matrix {
   int16 dist;
   int16 index;
};

// experience data hold in memory while playing
struct Practice {
   int16 damage[kGameTeamNum];
   int16 index[kGameTeamNum];
   int16 value[kGameTeamNum];
};

// defines linked waypoints
struct PathLink {
   Vector velocity;
   int32 distance;
   uint16 flags;
   int16 index;
};

// defines visibility count
struct PathVis {
   uint16 stand, crouch;
};

// define graph path structure for yapb
struct Path {
   int32 number, flags;
   Vector origin, start, end;
   float radius, light, display;   
   PathLink links[kMaxNodeLinks];
   PathVis vis;
};

// define waypoint structure for podbot  (will convert on load)
struct PODPath {
   int32 number, flags;
   Vector origin;
   float radius, csx, csy, cex, cey;
   int16 index[kMaxNodeLinks];
   uint16 conflags[kMaxNodeLinks];
   Vector velocity[kMaxNodeLinks];
   int32 distance[kMaxNodeLinks];
   PathVis vis;
};

// this structure links nodes returned from pathfinder
class PathWalk : public IntArray {
public:
   explicit PathWalk () {
      clear ();
   }

   ~PathWalk () = default;
public:
   int &next () {
      return at (1);
   }

   int &first () {
      return at (0);
   }

   bool hasNext () const {
      if (empty ()) {
         return false;
      }
      return length () > 1;
   }
};

// main bot class
class Bot final {
private:
   using RouteTwin = Twin <int, float>;

public:
   friend class BotManager;

private:
   uint32 m_states; // sensing bitstates
   uint32 m_collideMoves[kMaxCollideMoves]; // sorted array of movements
   uint32 m_collisionProbeBits; // bits of possible collision moves
   uint32 m_collStateIndex; // index into collide moves
   uint32 m_aimFlags; // aiming conditions
   uint32 m_currentTravelFlags; // connection flags like jumping

   int m_messageQueue[32]; // stack for messages
   int m_oldButtons; // our old buttons
   int m_reloadState; // current reload state
   int m_voicePitch; // bot voice pitch
   int m_rechoiceGoalCount; // multiple failed goals?
   int m_loosedBombNodeIndex; // nearest to loosed bomb node
   int m_plantedBombNodeIndex; // nearest to planted bomb node
   int m_currentNodeIndex; // current node index
   int m_travelStartIndex; // travel start index to double jump action
   int m_previousNodes[5]; // previous node indices from node find
   int m_pathFlags; // current node flags
   int m_needAvoidGrenade; // which direction to strafe away
   int m_campDirection; // camp Facing direction
   int m_campButtons; // buttons to press while camping
   int m_tryOpenDoor; // attempt's to open the door
   int m_liftState; // state of lift handling
   int m_radioSelect; // radio entry

   float m_headedTime;
   float m_prevTime; // time previously checked movement speed
   float m_prevSpeed; // speed some frames before
   float m_timeDoorOpen; // time to next door open check
   float m_lastChatTime; // time bot last chatted
   float m_timeLogoSpray; // time bot last spray logo
   float m_knifeAttackTime; // time to rush with knife (at the beginning of the round)
   float m_duckDefuseCheckTime; // time to check for ducking for defuse
   float m_frameInterval; // bot's frame interval
   float m_lastCommandTime; // time bot last thinked
   float m_reloadCheckTime; // time to check reloading
   float m_zoomCheckTime; // time to check zoom again
   float m_shieldCheckTime; // time to check shiled drawing again
   float m_grenadeCheckTime; // time to check grenade usage
   float m_sniperStopTime; // bot switched to other weapon?
   float m_lastEquipTime; // last time we equipped in buyzone
   float m_duckTime; // time to duck
   float m_jumpTime; // time last jump happened
   float m_soundUpdateTime; // time to update the sound
   float m_heardSoundTime; // last time noise is heard
   float m_buttonPushTime; // time to push the button
   float m_liftUsageTime; // time to use lift
   float m_askCheckTime; // time to ask team
   float m_collideTime; // time last collision
   float m_firstCollideTime; // time of first collision
   float m_probeTime; // time of probing different moves
   float m_lastCollTime; // time until next collision check
   float m_jumpStateTimer; // timer for jumping collision check
   float m_lookYawVel; // look yaw velocity
   float m_lookPitchVel; // look pitch velocity
   float m_lookUpdateTime; // lookangles update time
   float m_aimErrorTime; // time to update error vector
   float m_nextCampDirTime; // time next camp direction change
   float m_lastFightStyleCheck; // time checked style
   float m_strafeSetTime; // time strafe direction was set
   float m_randomizeAnglesTime; // time last randomized location
   float m_playerTargetTime; // time last targeting
   float m_timeCamping; // time to camp
   float m_lastUsedNodesTime; // last time bot followed nodes
   float m_shootAtDeadTime; // time to shoot at dying players
   float m_followWaitTime; // wait to follow time
   float m_chatterTimes[Chatter::Count]; // chatter command timers
   float m_navTimeset; // time node chosen by Bot
   float m_moveSpeed; // current speed forward/backward
   float m_strafeSpeed; // current speed sideways
   float m_minSpeed; // minimum speed in normal mode
   float m_oldCombatDesire; // holds old desire for filtering
   float m_avoidTime; // time to avoid players around
   float m_itemCheckTime; // time next search for items needs to be done

   bool m_moveToGoal; // bot currently moving to goal??
   bool m_isStuck; // bot is stuck
   bool m_isReloading; // bot is reloading a gun
   bool m_forceRadio; // should bot use radio anyway?
   bool m_defendedBomb; // defend action issued
   bool m_defendHostage; // defend action issued
   bool m_duckDefuse; // should or not bot duck to defuse bomb
   bool m_checkKnifeSwitch; // is time to check switch to knife action
   bool m_checkWeaponSwitch; // is time to check weapon switch
   bool m_isUsingGrenade; // bot currently using grenade??
   bool m_bombSearchOverridden; // use normal node if applicable when near the bomb
   bool m_wantsToFire; // bot needs consider firing
   bool m_jumpFinished; // has bot finished jumping
   bool m_isLeader; // bot is leader of his team
   bool m_checkTerrain; // check for terrain
   bool m_moveToC4; // ct is moving to bomb
   bool m_grenadeRequested; // bot requested change to grenade

   Pickup m_pickupType; // type of entity which needs to be used/picked up
   PathWalk m_pathWalk; // pointer to current node from path
   Dodge m_combatStrafeDir; // direction to strafe
   Fight m_fightStyle; // combat style to use
   CollisionState m_collisionState; // collision State
   FindPath m_pathType; // which pathfinder to use
   uint8 m_visibility; // visibility flags

   edict_t *m_pickupItem; // pointer to entity of item to use/pickup
   edict_t *m_itemIgnore; // pointer to entity to ignore for pickup
   edict_t *m_liftEntity; // pointer to lift entity
   edict_t *m_breakableEntity; // pointer to breakable entity
   edict_t *m_targetEntity; // the entity that the bot is trying to reach
   edict_t *m_avoidGrenade; // pointer to grenade entity to avoid
   edict_t *m_avoid; // avoid players on our way

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
   Path *m_path; // pointer to the current path node
   String m_chatBuffer; // space for strings (say text...)

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
   int getMsgQueue ();
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
   bool doneCanJumpUp (const Vector &normal);
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
   bool checkBodyParts (edict_t *target, Vector *origin, uint8 *bodyPart);
   bool seesEnemy (edict_t *player, bool ignoreFOV = false);
   bool doPlayerAvoidance (const Vector &normal);
   bool hasActiveGoal ();
   bool advanceMovement ();
   bool isBombDefusing (const Vector &bombOrigin);
   bool isOccupiedPoint (int index);
   bool seesItem (const Vector &dest, const char *itemName);
   bool lastEnemyShootable ();
   bool isShootableBreakable (edict_t *ent);
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
   bool isFriendInLineOfFire (float distance);
   bool isGroupOfEnemies (const Vector &location, int numEnemies = 1, float radius = 256.0f);
   bool isPenetrableObstacle (const Vector &dest);
   bool isPenetrableObstacle2 (const Vector &dest);
   bool isEnemyBehindShield (edict_t *enemy);
   bool checkChatKeywords (String &reply);
   bool isReplyingToChat ();

   void instantChatter (int type);
   void runAI ();
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
   void postprocessGoals (const IntArray &goals, int *result);
   void updatePickups ();
   void checkTerrain (float movedDistance, const Vector &dirNormal);
   void checkDarkness ();
   void checkParachute ();
   void getCampDirection (Vector *dest);
   void updatePracticeValue (int damage);
   void updatePracticeDamage (edict_t *attacker, int damage);
   void findShortestPath (int srcIndex, int destIndex);
   void findPath (int srcIndex, int destIndex, FindPath pathType = FindPath::Fast);
   void clearRoute ();
   void sayDebug (const char *format, ...);
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
   void bombDefuse_ ();
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

public:
   entvars_t *pev;

   int m_index; // saved bot index
   int m_wantedTeam; // player team bot wants select
   int m_wantedClass; // player model bot wants to select
   int m_difficulty; // bots hard level
   int m_moneyAmount; // amount of money in bot's bank

   float m_spawnTime; // time this bot spawned
   float m_timeTeamOrder; // time of last radio command
   float m_slowFrameTimestamp; // time to per-second think
   float m_nextBuyTime; // next buy time
   float m_checkDarkTime; // check for darkness time
   float m_preventFlashing; // bot turned away from flashbang
   float m_flashLevel; // flashlight level
   float m_blindTime; // time when bot is blinded
   float m_blindMoveSpeed; // mad speeds when bot is blind
   float m_blindSidemoveSpeed; // mad side move speeds when bot is blind
   float m_fallDownTime; // time bot started to fall 
   float m_duckForJump; // is bot needed to duck for double jump
   float m_baseAgressionLevel; // base aggression level (on initializing)
   float m_baseFearLevel; // base fear level (on initializing)
   float m_agressionLevel; // dynamic aggression level (in game)
   float m_fearLevel; // dynamic fear level (in game)
   float m_nextEmotionUpdate; // next time to sanitize emotions
   float m_thinkFps; // skip some frames in bot thinking
   float m_thinkInterval; // interval between frames
   float m_goalValue; // ranking value for this node
   float m_viewDistance; // current view distance
   float m_maxViewDistance; // maximum view distance
   float m_retreatTime; // time to retreat?
   float m_enemyUpdateTime; // time to check for new enemies
   float m_enemyReachableTimer; // time to recheck if enemy reachable
   float m_enemyIgnoreTimer; // ignore enemy for some time
   float m_seeEnemyTime; // time bot sees enemy
   float m_enemySurpriseTime; // time of surprise
   float m_idealReactionTime; // time of base reaction
   float m_actualReactionTime; // time of current reaction time
   float m_timeNextTracking; // time node index for tracking player is recalculated
   float m_firePause; // time to pause firing
   float m_shootTime; // time to shoot
   float m_timeLastFired; // time to last firing

   int m_basePing; // base ping for bot
   int m_numEnemiesLeft; // number of enemies alive left on map
   int m_numFriendsLeft; // number of friend alive left on map
   int m_retryJoin; // retry count for chosing team/class
   int m_startAction; // team/class selection state
   int m_voteKickIndex; // index of player to vote against
   int m_lastVoteKick; // last index
   int m_voteMap; // number of map to vote for
   int m_logotypeIndex; // index for logotype
   int m_buyState; // current count in buying
   int m_blindButton; // buttons bot press, when blind
   int m_radioOrder; // actual command
   int m_actMessageIndex; // current processed message
   int m_pushMessageIndex; // offset for next pushed message
   int m_prevGoalIndex; // holds destination goal node
   int m_chosenGoalIndex; // used for experience, same as above
   int m_lastDamageType; // stores last damage
   int m_team; // bot team
   int m_currentWeapon; // one current weapon for each bot
   int m_ammoInClip[kMaxWeapons]; // ammo in clip for each weapons
   int m_ammo[MAX_AMMO_SLOTS]; // total ammo amounts

   bool m_isVIP; // bot is vip?
   bool m_notKilled; // has the player been killed or has he just respawned
   bool m_notStarted; // team/class not chosen yet
   bool m_ignoreBuyDelay; // when reaching buyzone in the middle of the round don't do pauses
   bool m_inBombZone; // bot in the bomb zone or not
   bool m_inBuyZone; // bot currently in buy zone
   bool m_inVIPZone; // bot in the vip satefy zone
   bool m_buyingFinished; // done with buying
   bool m_buyPending; // bot buy is pending
   bool m_hasDefuser; // does bot has defuser
   bool m_hasNVG; // does bot has nightvision goggles
   bool m_usesNVG; // does nightvision goggles turned on
   bool m_hasC4; // does bot has c4 bomb
   bool m_hasProgressBar; // has progress bar on a HUD
   bool m_jumpReady; // is double jump ready
   bool m_canChooseAimDirection; // can choose aiming direction
   bool m_isEnemyReachable; // direct line to enemy

   edict_t *m_doubleJumpEntity; // pointer to entity that request double jump
   edict_t *m_radioEntity; // pointer to entity issuing a radio command
   edict_t *m_enemy; // pointer to enemy entity
   edict_t *m_lastEnemy; // pointer to last enemy entity
   edict_t *m_lastVictim; // pointer to killed entity
   edict_t *m_trackingEdict; // pointer to last tracked player when camping/hiding

   Vector m_pathOrigin; // origin of node
   Vector m_destOrigin; // origin of move destination
   Vector m_position; // position to move to in move to position task
   Vector m_doubleJumpOrigin; // origin of double jump
   Vector m_lastEnemyOrigin; // vector to last enemy origin
  
   ChatCollection m_sayTextBuffer; // holds the index & the actual message of the last unprocessed text message of a player
   BurstMode m_weaponBurstMode; // bot using burst mode? (famas/glock18, but also silencer mode)
   Personality m_personality; // bots type
   Array <BotTask> m_tasks;

public:
   Bot (edict_t *bot, int difficulty, int personality, int team, int member);
   ~Bot () = default;

public:
   void slowFrame (); // the main Lambda that decides intervals of running bot ai
   void fastFrame (); /// the things that can be executed while skipping frames
   void processBlind (int alpha);
   void processDamage (edict_t *inflictor, int damage, int armor, int bits);
   void showDebugOverlay ();
   void newRound ();
   void processBuyzoneEntering (int buyState);
   void pushMsgQueue (int message);
   void prepareChatMessage (const String &message);
   void checkForChat ();
   void showChaterIcon (bool show);
   void clearSearchNodes ();
   void processBreakables (edict_t *touch);
   void avoidIncomingPlayers (edict_t *touch);
   void startTask (Task id, float desire, int data, float time, bool resume);
   void clearTask (Task id);
   void filterTasks ();
   void clearTasks ();
   void dropWeaponForUser (edict_t *user, bool discardC4);
   void say (const char *text);
   void sayTeam (const char *text);
   void pushChatMessage (int type, bool isTeamSay = false);
   void pushRadioMessage (int message);
   void pushChatterMessage (int message);
   void processChatterMessage (const char *tempMessage);
   void tryHeadTowardRadioMessage ();
   void kill ();
   void kick ();
   void resetDoubleJump ();
   void startDoubleJump (edict_t *ent);

   bool hasHostage ();
   bool usesRifle ();
   bool usesPistol ();
   bool usesSniper ();
   bool usesSubmachine ();
   bool usesZoomableRifle ();
   bool usesBadWeapon ();
   bool usesCampGun ();
   bool hasPrimaryWeapon ();
   bool hasSecondaryWeapon ();
   bool hasShield ();
   bool isShieldDrawn ();
   bool findBestNearestNode ();
   bool seesEntity (const Vector &dest, bool fromBody = false);

   int getAmmo ();
   int getNearestToPlantedBomb ();

   float getFrameInterval ();
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

   int index () const {
      return m_index;
   }

   int entindex () const {
      return m_index + 1;
   }
};

// manager class
class BotManager final : public Singleton <BotManager> {
public:
   using ForEachBot = Lambda <bool (Bot *)>;
   using UniqueBot = UniquePtr <Bot>;

private:
   float m_timeRoundStart;
   float m_timeRoundEnd;
   float m_timeRoundMid;

   float m_maintainTime; // time to maintain bot creation
   float m_quotaMaintainTime; // time to maintain bot quota
   float m_grenadeUpdateTime; // time to update active grenades
   float m_entityUpdateTime; // time to update intresting entities
   float m_plantSearchUpdateTime; // time to update for searching planted bomb
   float m_lastChatTime; // global chat time timestamp
   float m_timeBombPlanted; // time the bomb were planted
   float m_lastRadioTime[kGameTeamNum]; // global radio time

   int m_lastWinner; // the team who won previous round
   int m_lastDifficulty; // last bots difficulty
   int m_bombSayStatus; // some bot is issued whine about bomb
   int m_lastRadio[kGameTeamNum]; // last radio message for team

   bool m_leaderChoosen[kGameTeamNum]; // is team leader choose theese round
   bool m_economicsGood[kGameTeamNum]; // is team able to buy anything
   bool m_bombPlanted;
   bool m_botsCanPause;
   bool m_roundEnded;

   Array <edict_t *> m_activeGrenades; // holds currently active grenades on the map
   Array <edict_t *> m_intrestingEntities;  // holds currently intresting entities on the map
   Array <CreateQueue> m_creationTab; // bot creation tab
   Array <BotTask> m_filters; // task filters

   Array <UniqueBot> m_bots; // all available bots
   edict_t *m_killerEntity; // killer entity for bots

protected:
   BotCreateResult create (const String &name, int difficulty, int personality, int team, int member);

public:
   BotManager ();
   ~BotManager () = default;

public:
   Twin <int, int> countTeamPlayers ();

   Bot *findBotByIndex (int index);
   Bot *findBotByEntity (edict_t *ent);

   Bot *findAliveBot ();
   Bot *findHighestFragBot (int team);

   int getHumansCount (bool ignoreSpectators = false);
   int getAliveHumansCount ();
   int getBotCount ();

   void setBombPlanted (bool isPlanted);
   void slowFrame ();
   void frame ();
   void createKillerEntity ();
   void destroyKillerEntity ();
   void touchKillerEntity (Bot *bot);
   void destroy ();
   void addbot (const String &name, int difficulty, int personality, int team, int member, bool manual);
   void addbot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual);
   void serverFill (int selection, int personality = Personality::Normal, int difficulty = -1, int numToAdd = -1);
   void kickEveryone (bool instant = false,  bool zeroQuota = true);
   bool kickRandom (bool decQuota = true, Team fromTeam = Team::Unassigned);
   void kickBot (int index);
   void kickFromTeam (Team team, bool removeAll = false);
   void killAllBots (int team = -1);
   void maintainQuota ();
   void initQuota ();
   void initRound ();
   void decrementQuota (int by = 1);
   void selectLeaders (int team, bool reset);
   void listBots ();
   void setWeaponMode (int selection);
   void updateTeamEconomics (int team, bool setTrue = false);
   void updateBotDifficulties ();
   void reset ();
   void initFilters ();
   void resetFilters ();
   void updateActiveGrenade ();
   void updateIntrestingEntities ();
   void captureChatRadio (const char *cmd, const char *arg, edict_t *ent);
   void notifyBombDefuse ();
   void execGameEntity (entvars_t *vars);
   void forEach (ForEachBot handler);
   void erase (Bot *bot);
   
   bool isTeamStacked (int team);

public:
   Array <edict_t *> &searchActiveGrenades () {
      return m_activeGrenades;
   }

   Array <edict_t *> &searchIntrestingEntities () {
      return m_intrestingEntities;
   }

   bool hasActiveGrenades () const {
      return !m_activeGrenades.empty ();
   }

   bool hasIntrestingEntities () const {
      return !m_intrestingEntities.empty ();
   }

   bool checkTeamEco (int team) const {
      return m_economicsGood[team];
   }

   int getLastWinner () const {
      return m_lastWinner;
   }

   void setLastWinner (int winner) {
      m_lastWinner = winner;
   }

   // get the list of filters
   Array <BotTask> &getFilters () {
      return m_filters;
   }

   void createRandom (bool manual = false) {
      addbot ("", -1, -1, -1, -1, manual);
   }

   bool isBombPlanted () const {
      return m_bombPlanted;
   }

   float getTimeBombPlanted () const {
      return m_timeBombPlanted;
   }

   float getRoundStartTime () const {
      return m_timeRoundStart;
   }

   float getRoundMidTime () const {
      return m_timeRoundMid;
   }

   float getRoundEndTime () const {
      return m_timeRoundEnd;
   }

   bool isRoundOver () const {
      return m_roundEnded;
   }

   void setRoundOver (const bool over) {
      m_roundEnded = over;
   }

   bool canPause () const {
      return m_botsCanPause;
   }

   void setCanPause (const bool pause) {
      m_botsCanPause = pause;
   }

   bool hasBombSay (int type) {
      return (m_bombSayStatus & type) == type;
   }

   void clearBombSay (int type) {
      m_bombSayStatus &= ~type;
   }

   void setPlantedBombSearchTimestamp (const float timestamp) {
      m_plantSearchUpdateTime = timestamp;
   }

   float getPlantedBombSearchTimestamp () const {
      return m_plantSearchUpdateTime;
   }

   void setLastRadioTimestamp (const int team, const float timestamp) {
      if (team == Team::CT || team == Team::Terrorist) {
         m_lastRadioTime[team] = timestamp;
      }
   }

   float getLastRadioTimestamp (const int team) const {
      if (team == Team::CT || team == Team::Terrorist) {
         return m_lastRadioTime[team];
      }
      return 0.0f;
   }

   void setLastRadio (const int team, const int radio) {
      m_lastRadio[team] = radio;
   }

   int getLastRadio (const int team) const {
      return m_lastRadio[team];
   }

   void setLastChatTimestamp (const float timestamp) {
      m_lastChatTime = timestamp;
   }

   float getLastChatTimestamp () const {
      return m_lastChatTime;
   }

   // some bots are online ?
   bool hasBotsOnline () {
      return getBotCount () > 0;
   }

public:
   Bot *operator [] (int index) {
      return findBotByIndex (index);
   }

   Bot *operator [] (edict_t *ent) {
      return findBotByEntity (ent);
   }

public:
   UniqueBot *begin () {
      return m_bots.begin ();
   }

   UniqueBot *begin () const {
      return m_bots.begin ();
   }

   UniqueBot *end () {
      return m_bots.end ();
   }

   UniqueBot *end () const {
      return m_bots.end ();
   }
};

// graph operation class
class BotGraph final : public Singleton <BotGraph> {
public:
   friend class Bot;

private:
   struct Bucket {
      int x, y, z;
   };

   int m_editFlags;
   int m_loadAttempts;
   int m_cacheNodeIndex;
   int m_lastJumpNode;
   int m_findWPIndex;
   int m_facingAtIndex;
   int m_highestDamage[kGameTeamNum];

   float m_timeJumpStarted;
   float m_autoPathDistance;
   float m_pathDisplayTime;
   float m_arrowDisplayTime;

   bool m_isOnLadder;
   bool m_endJumpPoint;
   bool m_jumpLearnNode;
   bool m_hasChanged;
   bool m_needsVisRebuild;

   Vector m_learnVelocity;
   Vector m_learnPosition;
   Vector m_bombPos;
   Vector m_lastNode;

   IntArray m_terrorPoints;
   IntArray m_ctPoints;
   IntArray m_goalPoints;
   IntArray m_campPoints;
   IntArray m_sniperPoints;
   IntArray m_rescuePoints;
   IntArray m_visitedGoals;

   Array <int32, ReservePolicy::PlusOne> m_buckets[kMaxBucketsInsidePos][kMaxBucketsInsidePos][kMaxBucketsInsidePos];
   Array <Matrix, ReservePolicy::PlusOne> m_matrix;
   Array <Practice, ReservePolicy::PlusOne> m_practice;
   Array <Path, ReservePolicy::PlusOne> m_paths;
   Array <uint8, ReservePolicy::PlusOne> m_vistable;

   String m_tempStrings;
   edict_t *m_editor;

public:
   BotGraph ();
   ~BotGraph () = default;

public:

   int getFacingIndex ();
   int getFarest (const Vector &origin, float maxDistance = 32.0);
   int getNearest (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   int getNearestNoBuckets (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   int getEditorNeareset ();
   int getDangerIndex (int team, int start, int goal);
   int getDangerValue (int team, int start, int goal);
   int getDangerDamage (int team, int start, int goal);
   int getPathDist (int srcIndex, int destIndex);
   int clearConnections (int index);

   float calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin);

   bool convertOldFormat ();
   bool isVisible (int srcIndex, int destIndex);
   bool isStandVisible (int srcIndex, int destIndex);
   bool isDuckVisible (int srcIndex, int destIndex);
   bool isConnected (int a, int b);
   bool isConnected (int index);
   bool isReachable (Bot *bot, int index);
   bool isNodeReacheable (const Vector &src, const Vector &destination);
   bool checkNodes (bool teleportPlayer);
   bool loadPathMatrix ();
   bool isVisited (int index);

   bool saveGraphData ();
   bool loadGraphData ();

   template <typename U> bool saveStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, const Array <U, ReservePolicy::PlusOne> &data, uint8 *blob);
   template <typename U> bool loadStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, Array <U, ReservePolicy::PlusOne> &data, uint8 *blob, int32 *outOptions);

   void saveOldFormat ();
   void initGraph ();
   void frame ();
   void loadPractice ();
   void loadVisibility ();
   void initNodesTypes ();
   void initLightLevels ();
   void addPath (int addIndex, int pathIndex, float distance);
   void add (int type, const Vector &pos = nullvec);
   void erase (int target);
   void toggleFlags (int toggleFlag);
   void setRadius (int index, float radius);
   void rebuildVisibility ();
   void pathCreate (char dir);
   void erasePath ();
   void cachePoint (int index);
   void calculatePathRadius (int index);
   void savePractice ();
   void saveVisibility ();
   void addBasic ();
   void eraseFromDisk ();
   void savePathMatrix ();
   void setSearchIndex (int index);
   void startLearnJump ();
   void setVisited (int index);
   void clearVisited ();
   void initBuckets ();
   void addToBucket (const Vector &pos, int index);
   void eraseFromBucket (const Vector &pos, int index);
   void setBombPos (bool reset = false, const Vector &pos = nullvec);
   void updateGlobalPractice ();
   void unassignPath (int from, int to);
   void setDangerValue (int team, int start, int goal, int value);
   void setDangerDamage (int team, int start, int goal, int value);
   void convertFromPOD (Path &path, const PODPath &pod);
   void converToPOD (const Path &path, PODPath &pod);
   void convertCampDirection (Path &path);

   const char *getDataDirectory (bool isMemoryFile = false);
   const char *getOldFormatGraphName (bool isMemoryFile = false);

   Bucket locateBucket (const Vector &pos);
   IntArray searchRadius (float radius, const Vector &origin, int maxCount = -1);
   const Array <int32, ReservePolicy::PlusOne> &getNodesInBucket (const Vector &pos);

public:
   int getHighestDamageForTeam (int team) const {
      return m_highestDamage[team];
   }

   void setHighestDamageForTeam (int team, int value) {
      m_highestDamage[team] = value;
   }

   const char *getAuthor () const {
      return m_tempStrings.chars ();
   }

   bool hasChanged () const {
      return m_hasChanged;
   }

   bool hasEditFlag (int flag) const {
      return !!(m_editFlags & flag);
   }

   void setEditFlag (int flag) {
      m_editFlags |= flag;
   }

   void clearEditFlag (int flag) {
      m_editFlags &= ~flag;
   }

   void setAutoPathDistance (const float distance) {
      m_autoPathDistance = distance;
   }

   const Vector &getBombPos () const {
      return m_bombPos;
   }

   // access paths
   Path &operator [] (int index) {
      return m_paths[index];
   }

   // check nodes range
   bool exists (int index) const {
      return index >= 0 && index < static_cast <int> (m_paths.length ());
   }

   // get real nodes num
   int length () const {
      return m_paths.length ();
   }

   // check if has editor
   bool hasEditor () const {
      return !!m_editor;
   }

   // set's the node editor
   void setEditor (edict_t *ent) {
      m_editor = ent;
   }

   // get the current node editor
   edict_t *getEditor () {
      return m_editor;
   }
};

// mostly config stuff, and some stuff dealing with menus
class BotConfig final : public Singleton <BotConfig> {
private:
   Array <StringArray> m_chat;
   Array <Array <ChatterItem>> m_chatter;

   Array <BotName> m_botNames;
   Array <Keywords> m_replies;
   Array <WeaponInfo> m_weapons;
   Array <WeaponProp> m_weaponProps;

   StringArray m_logos;
   StringArray m_avatars;

   // default tables for personality weapon preferences, overridden by general.cfg
   int m_normalWeaponPrefs[kNumWeapons] = { 0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 18, 21, 17, 19, 15, 17, 9, 14, 16 };
   int m_rusherWeaponPrefs[kNumWeapons] = { 0, 2, 1, 4, 5, 6, 3, 24, 19, 22, 23, 20, 21, 10, 12, 13, 7, 8, 11, 9, 18, 17, 19, 25, 15, 16 };
   int m_carefulWeaponPrefs[kNumWeapons] = { 0, 2, 1, 4, 25, 6, 3, 7, 8, 12, 10, 13, 11, 9, 24, 18, 14, 17, 16, 15, 19, 20, 21, 22, 23, 5 };
   int m_grenadeBuyPrecent[kNumWeapons - 23] = { 95, 85, 60 };
   int m_botBuyEconomyTable[kNumWeapons - 15] = { 1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000 };
   int *m_weaponPrefs[3] = { m_normalWeaponPrefs, m_rusherWeaponPrefs, m_carefulWeaponPrefs };

public:
   BotConfig ();
   ~BotConfig () = default;

public:

   // load the configuration files
   void loadConfigs ();

   // loads main config file
   void loadMainConfig ();

   // loads bot names
   void loadNamesConfig ();

   // loads weapons config
   void loadWeaponsConfig ();

   // loads chatter config
   void loadChatterConfig ();

   // loads chat config
   void loadChatConfig (); 

   // loads language config
   void loadLanguageConfig ();

   // load bots logos config
   void loadLogosConfig ();

   // load bots avatars config
   void loadAvatarsConfig ();

   // sets memfile to use engine functions
   void setupMemoryFiles ();

   // picks random bot name
   BotName *pickBotName ();

   // remove bot name from used list
   void clearUsedName (Bot *bot);

   // initialize weapon info
   void initWeapons ();

   // fix weapon prices (ie for elite)
   void adjustWeaponPrices ();

   WeaponInfo &findWeaponById (int id);

private:
   bool isCommentLine (const String &line) {
      const char ch = line.at (0);
      return ch == '#' || ch == '/' || ch == '\r' || ch == ';' || ch == 0 || ch == ' ';
   };

public:

   // checks whether chat banks contains messages
   bool hasChatBank (int chatType) const {
      return !m_chat[chatType].empty ();
   }

   // checks whether chatter banks contains messages
   bool hasChatterBank (int chatterType) const {
      return !m_chatter[chatterType].empty ();
   }

   // pick random phrase from chat bank
   const String &pickRandomFromChatBank (int chatType) {
      return m_chat[chatType].random ();
   }

   // pick random phrase from chatter bank
   const ChatterItem &pickRandomFromChatterBank (int chatterType) {
      return m_chatter[chatterType].random ();
   }

   // gets chatter repeat-interval
   float getChatterMessageRepeatInterval (int chatterType) const {
      return m_chatter[chatterType][0].repeat;
   }

   // get's the replies array
   Array <Keywords> &getReplies () {
      return m_replies;
   }

   // get's the weapon info data
   Array <WeaponInfo> &getWeapons () {
      return m_weapons;
   }

   // get's raw weapon info
   WeaponInfo *getRawWeapons () {
      return m_weapons.begin ();
   }

   // set's the weapon properties
   void setWeaponProp (const WeaponProp &prop) {
      m_weaponProps[prop.id] = prop;
   }

   // get's the weapons prop
   const WeaponProp &getWeaponProp (int id) const {
      return m_weaponProps[id];
   }

   // get's weapon preferences for personality
   int *getWeaponPrefs (int personality) const {
      return m_weaponPrefs[personality];
   }

   // get economics value
   int *getEconLimit () {
      return m_botBuyEconomyTable;
   }

   // get's grenade buy percents
   bool chanceToBuyGrenade (int grenadeType) const {
      return rg.chance (m_grenadeBuyPrecent[grenadeType]);
   }

   // get's random avatar for player (if any)
   String getRandomAvatar () const {
      if (!m_avatars.empty ()) {
         return m_avatars.random ();
      }
      return "";
   }

   // get's random logo index
   int getRandomLogoIndex () const {
      return m_logos.index (m_logos.random ());
   }

   // get random name by index
   const String &getRandomLogoName (int index) const {
      return m_logos[index];
   }
};

class BotUtils final : public Singleton <BotUtils> {
private:
   bool m_needToSendWelcome;
   float m_welcomeReceiveTime;

   StringArray m_sentences;
   Array <Client> m_clients;
   Array <Twin <String, String>> m_tags;

public:
   BotUtils ();
   ~BotUtils () = default;

public:
   // need to send welcome message ?
   void checkWelcome ();

   // gets the weapon alias as hlsting, maybe move to config...
   int getWeaponAlias (bool needString, const char *weaponAlias, int weaponIndex = -1);

   // gets the build number of bot
   int buildNumber ();

   // gets the shooting cone deviation
   float getShootingCone (edict_t *ent, const Vector &position);

   // check if origin is visible from the entity side
   bool isVisible (const Vector &origin, edict_t *ent);

   // check if entity is alive
   bool isAlive (edict_t *ent);

   // check if origin is inside view cone of entity
   bool isInViewCone (const Vector &origin, edict_t *ent);

   // checks if entitiy is fakeclient
   bool isFakeClient (edict_t *ent);

   // check if entitiy is a player
   bool isPlayer (edict_t *ent);

   // check if entity is a vip
   bool isPlayerVIP (edict_t *ent);

   // opens config helper
   bool openConfig (const char *fileName, const char *errorIfNotExists, MemFile *outFile, bool languageDependant = false);

   // nearest player search helper
   bool findNearestPlayer (void **holder, edict_t *to, float searchDistance = 4096.0, bool sameTeam = false, bool needBot = false, bool needAlive = false, bool needDrawn = false, bool needBotWithC4 = false);

   // tracing decals for bots spraying logos
   void traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex);

   // attaches sound to client struct
   void attachSoundsToClients (edict_t *ent, const char *sample, float volume);

   // simulate sound for players
   void simulateSoundUpdates (int playerIndex);

   // update stats on clients
   void updateClients ();

   // chat helper to strip the clantags out of the string
   void stripTags (String &line);

   // chat helper to make player name more human-like
   void humanizePlayerName (String &playerName);

   // chat helper to add errors to the bot chat string
   void addChatErrors (String &line);

   // chat helper to find keywords for given string
   bool checkKeywords (const String &line, String &reply);

   // generates ping bitmask for SVC_PINGS message
   int getPingBitmask (edict_t *ent, int loss, int ping);

   // calculate our own pings for all the players
   void calculatePings ();

   // send modified pings to all the clients
   void sendPings (edict_t *to);

public:

   // re-show welcome after changelevel ?
   void setNeedForWelcome (bool need) {
      m_needToSendWelcome = need;
   }

   // get array of clients
   Array <Client> &getClients () {
      return m_clients;
   }

   // get clients as const-reference
   const Array <Client> &getClients () const {
      return m_clients;
   }

   // get single client as ref
   Client &getClient (const int index) {
      return m_clients[index];
   }

   // checks if string is not empty
   bool isEmptyStr (const char *input) const {
      if (input == nullptr) {
         return true;
      }
      return *input == '\0';
   }
};

// bot command manager
class BotControl final : public Singleton <BotControl> {
public:
   using Handler = int (BotControl::*) ();
   using MenuHandler = int (BotControl::*) (int);

public:
   // generic bot command
   struct BotCmd {
      String name, format, help;
      Handler handler = nullptr;

   public:
      BotCmd () = default;
      BotCmd (String name, String format, String help, Handler handler) : name (cr::move (name)), format (cr::move (format)), help (cr::move (help)), handler (cr::move (handler)) { }
   };

   // single bot menu
   struct BotMenu {
      int ident, slots;
      String text;
      MenuHandler handler;

   public:
      BotMenu (int ident, int slots, String text, MenuHandler handler) : ident (ident), slots (slots), text (cr::move (text)), handler (cr::move (handler)) { }
   };

private:
   StringArray m_args;
   Array <BotCmd> m_cmds;
   Array <BotMenu> m_menus;

   edict_t *m_ent;

   bool m_isFromConsole;
   bool m_rapidOutput;
   bool m_isMenuFillCommand;
   int m_menuServerFillTeam;
   int m_interMenuData[4] = { 0, };

public:
   BotControl ();
   ~BotControl () = default;

private:
   int cmdAddBot ();
   int cmdKickBot ();
   int cmdKickBots ();
   int cmdKillBots ();
   int cmdFill ();
   int cmdVote ();
   int cmdWeaponMode ();
   int cmdVersion ();
   int cmdNodeMenu ();
   int cmdMenu ();
   int cmdList ();
   int cmdNode ();
   int cmdNodeOn ();
   int cmdNodeOff ();
   int cmdNodeAdd ();
   int cmdNodeAddBasic ();
   int cmdNodeSave ();
   int cmdNodeLoad ();
   int cmdNodeErase ();
   int cmdNodeDelete ();
   int cmdNodeCheck ();
   int cmdNodeCache ();
   int cmdNodeClean ();
   int cmdNodeSetRadius ();
   int cmdNodeSetFlags ();
   int cmdNodeTeleport ();
   int cmdNodePathCreate ();
   int cmdNodePathDelete ();
   int cmdNodePathSetAutoDistance ();
   int cmdNodeAcquireEditor ();
   int cmdNodeReleaseEditor ();
   int cmdNodeUpload ();

private:
   int menuMain (int item);
   int menuFeatures (int item);
   int menuControl (int item);
   int menuWeaponMode (int item);
   int menuPersonality (int item);
   int menuDifficulty (int item);
   int menuTeamSelect (int item);
   int menuClassSelect (int item);
   int menuCommands (int item);
   int menuGraphPage1 (int item);
   int menuGraphPage2 (int item);
   int menuGraphRadius (int item);
   int menuGraphType (int item);
   int menuGraphFlag (int item);
   int menuGraphPath (int item);
   int menuAutoPathDistance (int item);
   int menuKickPage1 (int item);
   int menuKickPage2 (int item);
   int menuKickPage3 (int item);
   int menuKickPage4 (int item);

private:
   void enableDrawModels (bool enable);
   void createMenus ();

public:
   bool executeCommands ();
   bool executeMenus ();

   void showMenu (int id);
   void kickBotByMenu (int page);
   void assignAdminRights (edict_t *ent, char *infobuffer);
   void maintainAdminRights ();

public:
   void setFromConsole (bool console) {
      m_isFromConsole = console;
   }

   void setRapidOutput (bool force) {
      m_rapidOutput = force;
   }

   void setIssuer (edict_t *ent) {
      m_ent = ent;
   }

   void fixMissingArgs (size_t num) {
      if (num < m_args.length ()) {
         return;
      }
      m_args.resize (num);
   }

   int getInt (size_t arg) const {
      if (!hasArg (arg)) {
         return 0;
      }
      return m_args[arg].int_ ();
   }

   const String &getStr (size_t arg) {
      static String empty ("empty");

      if (!hasArg (arg) || m_args[arg].empty ()) {
         return empty;
      }
      return m_args[arg];
   }

   bool hasArg (size_t arg) const {
      return arg < m_args.length ();
   }

   void collectArgs () {
      m_args.clear ();

      for (int i = 0; i < engfuncs.pfnCmd_Argc (); i++) {
         m_args.push (engfuncs.pfnCmd_Argv (i));
      }
   }

   // global heloer for sending message to correct channel
   template <typename ...Args> void msg (const char *fmt, Args ...args);

public:

   // for the server commands
   static void handleEngineCommands ();

   // for the client commands
   bool handleClientCommands (edict_t *ent);

   // for the client menu commands
   bool handleMenuCommands (edict_t *ent);
};

#include <engine.h>

// expose bot super-globals
static auto &graph = BotGraph::get ();
static auto &bots = BotManager::get ();
static auto &conf = BotConfig::get ();
static auto &util = BotUtils::get ();
static auto &ctrl = BotControl::get ();
static auto &game = Game::get ();
static auto &illum = LightMeasure::get ();

// very global convars
extern ConVar yb_jasonmode;
extern ConVar yb_radio_mode;
extern ConVar yb_ignore_enemies;
extern ConVar yb_chat;
extern ConVar yb_language;
extern ConVar yb_show_latency;

inline int Game::getTeam (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return Team::Unassigned;
   }
   return util.getClient (indexOfPlayer (ent)).team;
}

// global heloer for sending message to correct channel
template <typename ...Args> inline void BotControl::msg (const char *fmt, Args ...args) {
   auto result = strings.format (fmt, cr::forward <Args> (args)...);

   // if no receiver or many message have to appear, just print to server console
   if (game.isNullEntity (m_ent) || m_rapidOutput) {
      game.print (result); 
      return;
   }

   if (m_isFromConsole || strlen (result) > 48) {
      game.clientPrint (m_ent, result);
   }
   else {
      game.centerPrint (m_ent, result);
      game.clientPrint (m_ent, result);
   }
}
