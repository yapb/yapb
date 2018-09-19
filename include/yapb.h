//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

#include <extdll.h>
#include <memory.h>
#include <stdio.h>
#include <math.h>
#include <meta_api.h>

using namespace cr::types;
using namespace cr::classes;

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

// defines bots tasks
enum TaskID {
   TASK_NORMAL,
   TASK_PAUSE,
   TASK_MOVETOPOSITION,
   TASK_FOLLOWUSER,
   TASK_PICKUPITEM,
   TASK_CAMP,
   TASK_PLANTBOMB,
   TASK_DEFUSEBOMB,
   TASK_ATTACK,
   TASK_HUNTENEMY,
   TASK_SEEKCOVER,
   TASK_THROWHEGRENADE,
   TASK_THROWFLASHBANG,
   TASK_THROWSMOKE,
   TASK_DOUBLEJUMP,
   TASK_ESCAPEFROMBOMB,
   TASK_SHOOTBREAKABLE,
   TASK_HIDE,
   TASK_BLINDED,
   TASK_SPRAY,
   TASK_MAX
};

// supported cs's
enum GameFlags {
   GAME_CSTRIKE16 = (1 << 0), // counter-strike 1.6 and above
   GAME_XASH_ENGINE = (1 << 1), // counter-strike 1.6 under the xash engine (additional flag)
   GAME_CZERO = (1 << 2), // counter-strike: condition zero
   GAME_LEGACY = (1 << 3), // counter-strike 1.3-1.5 with/without steam
   GAME_MOBILITY = (1 << 4), // additional flag that bot is running on android (additional flag)
   GAME_OFFICIAL_CSBOT = (1 << 5), // additional flag that indicates official cs bots are in game
   GAME_METAMOD = (1 << 6), // game running under metamod
   GAME_CSDM = (1 << 7), // csdm mod currently in use
   GAME_CSDM_FFA = (1 << 8), // csdm mod with ffa mode
   GAME_SUPPORT_SVC_PINGS = (1 << 9), // on that game version we can fake bots pings
   GAME_SUPPORT_BOT_VOICE = (1 << 10) // on that game version we can use chatter
};

// bot menu ids
enum MenuId {
   BOT_MENU_INVALID = 0,
   BOT_MENU_MAIN,
   BOT_MENU_FEATURES,
   BOT_MENU_CONTROL,
   BOT_MENU_WEAPON_MODE,
   BOT_MENU_PERSONALITY,
   BOT_MENU_DIFFICULTY,
   BOT_MENU_TEAM_SELECT,
   BOT_MENU_TERRORIST_SELECT,
   BOT_MENU_CT_SELECT,
   BOT_MENU_COMMANDS,
   BOT_MENU_WAYPOINT_MAIN_PAGE1,
   BOT_MENU_WAYPOINT_MAIN_PAGE2,
   BOT_MENU_WAYPOINT_RADIUS,
   BOT_MENU_WAYPOINT_TYPE,
   BOT_MENU_WAYPOINT_FLAG,
   BOT_MENU_WAYPOINT_AUTOPATH,
   BOT_MENU_WAYPOINT_PATH,
   BOT_MENU_KICK_PAGE_1,
   BOT_MENU_KICK_PAGE_2,
   BOT_MENU_KICK_PAGE_3,
   BOT_MENU_KICK_PAGE_4,
   BOT_MENU_TOTAL_MENUS
};

// log levels
enum LogLevel {
   LL_DEFAULT = 1, // default log message
   LL_WARNING = 2, // warning log message
   LL_ERROR = 3, // error log message
   LL_IGNORE = 4, // additional flag
   LL_FATAL = 5  // fatal error log message (terminate the game!)
};

// chat types id's
enum ChatType {
   CHAT_KILLING = 0, // id to kill chat array
   CHAT_DEAD, // id to dead chat array
   CHAT_BOMBPLANT, // id to bomb chat array
   CHAT_TEAMATTACK, // id to team-attack chat array
   CHAT_TEAMKILL, // id to team-kill chat array
   CHAT_WELCOME, // id to welcome chat array
   CHAT_NOKW, // id to no keyword chat array
   CHAT_TOTAL // number for array
};

// personalities defines
enum Personality {
   PERSONALITY_NORMAL = 0,
   PERSONALITY_RUSHER,
   PERSONALITY_CAREFUL
};

// bot difficulties
enum Difficulty : int {
   DIFFICULTY_VERY_EASY,
   DIFFICULTY_EASY,
   DIFFICULTY_NORMAL,
   DIFFICULTY_HARD,
   DIFFICULTY_VERY_HARD
};

// collision states
enum CollisionState {
   COLLISION_NOTDECICED,
   COLLISION_PROBING,
   COLLISION_NOMOVE,
   COLLISION_JUMP,
   COLLISION_DUCK,
   COLLISION_STRAFELEFT,
   COLLISION_STRAFERIGHT
};

// counter-strike team id's
enum Team {
   TEAM_TERRORIST = 0,
   TEAM_COUNTER,
   TEAM_SPECTATOR,
   TEAM_UNASSIGNED
};

// client flags
enum ClientFlags {
   CF_USED = (1 << 0),
   CF_ALIVE = (1 << 1),
   CF_ADMIN = (1 << 2),
   CF_ICON = (1 << 3)
};

// bot create status
enum BotCreationResult {
   BOT_RESULT_CREATED,
   BOT_RESULT_MAX_PLAYERS_REACHED,
   BOT_RESULT_NAV_ERROR,
   BOT_RESULT_TEAM_STACKED
};

// radio messages
enum pushRadioMessage {
   Radio_CoverMe = 1,
   Radio_YouTakePoint = 2,
   Radio_HoldPosition = 3,
   Radio_RegroupTeam = 4,
   Radio_FollowMe = 5,
   Radio_TakingFire = 6,
   Radio_GoGoGo = 11,
   Radio_Fallback = 12,
   Radio_StickTogether = 13,
   Radio_GetInPosition = 14,
   Radio_StormTheFront = 15,
   Radio_ReportTeam = 16,
   Radio_Affirmative = 21,
   Radio_EnemySpotted = 22,
   Radio_NeedBackup = 23,
   Radio_SectorClear = 24,
   Radio_InPosition = 25,
   Radio_ReportingIn = 26,
   Radio_ShesGonnaBlow = 27,
   Radio_Negative = 28,
   Radio_EnemyDown = 29
};

// voice system (extending enum above, messages 30-39 is reserved)
enum pushChatterMessage {
   Chatter_SpotTheBomber = 40,
   Chatter_FriendlyFire,
   Chatter_DiePain,
   Chatter_GotBlinded,
   Chatter_GoingToPlantBomb,
   Chatter_RescuingHostages,
   Chatter_GoingToCamp,
   Chatter_HearSomething,
   Chatter_TeamKill,
   Chatter_ReportingIn,
   Chatter_GuardDroppedC4,
   Chatter_Camp,
   Chatter_PlantingC4,
   Chatter_DefusingC4,
   Chatter_InCombat,
   Chatter_SeeksEnemy,
   Chatter_Nothing,
   Chatter_EnemyDown,
   Chatter_UseHostage,
   Chatter_FoundC4,
   Chatter_WonTheRound,
   Chatter_ScaredEmotion,
   Chatter_HeardEnemy,
   Chatter_SniperWarning,
   Chatter_SniperKilled,
   Chatter_VIPSpotted,
   Chatter_GuardingVipSafety,
   Chatter_GoingToGuardVIPSafety,
   Chatter_QuicklyWonTheRound,
   Chatter_OneEnemyLeft,
   Chatter_TwoEnemiesLeft,
   Chatter_ThreeEnemiesLeft,
   Chatter_NoEnemiesLeft,
   Chatter_FoundBombPlace,
   Chatter_WhereIsTheBomb,
   Chatter_DefendingBombSite,
   Chatter_BarelyDefused,
   Chatter_NiceshotCommander,
   Chatter_NiceshotPall,
   Chatter_GoingToGuardHostages,
   Chatter_GoingToGuardDoppedBomb,
   Chatter_OnMyWay,
   Chatter_LeadOnSir,
   Chatter_Pinned_Down,
   Chatter_GottaFindTheBomb,
   Chatter_You_Heard_The_Man,
   Chatter_Lost_The_Commander,
   Chatter_NewRound,
   Chatter_CoverMe,
   Chatter_BehindSmoke,
   Chatter_BombSiteSecured,
   Chatter_Total

};

// counter-strike weapon id's
enum Weapon {
   WEAPON_P228 = 1,
   WEAPON_SHIELD = 2,
   WEAPON_SCOUT = 3,
   WEAPON_EXPLOSIVE = 4,
   WEAPON_XM1014 = 5,
   WEAPON_C4 = 6,
   WEAPON_MAC10 = 7,
   WEAPON_AUG = 8,
   WEAPON_SMOKE = 9,
   WEAPON_ELITE = 10,
   WEAPON_FIVESEVEN = 11,
   WEAPON_UMP45 = 12,
   WEAPON_SG550 = 13,
   WEAPON_GALIL = 14,
   WEAPON_FAMAS = 15,
   WEAPON_USP = 16,
   WEAPON_GLOCK = 17,
   WEAPON_AWP = 18,
   WEAPON_MP5 = 19,
   WEAPON_M249 = 20,
   WEAPON_M3 = 21,
   WEAPON_M4A1 = 22,
   WEAPON_TMP = 23,
   WEAPON_G3SG1 = 24,
   WEAPON_FLASHBANG = 25,
   WEAPON_DEAGLE = 26,
   WEAPON_SG552 = 27,
   WEAPON_AK47 = 28,
   WEAPON_KNIFE = 29,
   WEAPON_P90 = 30,
   WEAPON_ARMOR = 31,
   WEAPON_ARMORHELM = 32,
   WEAPON_DEFUSER = 33
};

// buy counts
enum BuyState {
   BUYSTATE_PRIMARY_WEAPON = 0,
   BUYSTATE_ARMOR_VESTHELM,
   BUYSTATE_SECONDARY_WEAPON,
   BUYSTATE_GRENADES,
   BUYSTATE_DEFUSER,
   BUYSTATE_AMMO,
   BUYSTATE_FINISHED
};

// economics limits
enum EconomyLimit {
   ECO_PRIMARY_GT = 0,
   ECO_SMG_GT_CT,
   ECO_SMG_GT_TE,
   ECO_SHOTGUN_GT,
   ECO_SHOTGUN_LT,
   ECO_HEAVY_GT,
   ECO_HEAVY_LT,
   ECO_PROSTOCK_NORMAL,
   ECO_PROSTOCK_RUSHER,
   ECO_PROSTOCK_CAREFUL,
   ECO_SHIELDGUN_GT
};

// defines for pickup items
enum PickupType {
   PICKUP_NONE,
   PICKUP_WEAPON,
   PICKUP_DROPPED_C4,
   PICKUP_PLANTED_C4,
   PICKUP_HOSTAGE,
   PICKUP_BUTTON,
   PICKUP_SHIELD,
   PICKUP_DEFUSEKIT
};

// fight style type
enum FightStyle {
   FIGHT_NONE,
   FIGHT_STRAFE,
   FIGHT_STAY
};

// dodge type
enum StrafeDir {
   STRAFE_DIR_NONE,
   STRAFE_DIR_LEFT,
   STRAFE_DIR_RIGHT
};

// reload state
enum ReloadState {
   RELOAD_NONE = 0, // no reload state currently
   RELOAD_PRIMARY = 1, // primary weapon reload state
   RELOAD_SECONDARY = 2  // secondary weapon reload state
};

// collision probes
enum CollisionProbe {
   PROBE_JUMP = (1 << 0), // probe jump when colliding
   PROBE_DUCK = (1 << 1), // probe duck when colliding
   PROBE_STRAFE = (1 << 2) // probe strafing when colliding
};

// vgui menus (since latest steam updates is obsolete, but left for old cs)
enum VGuiMenu {
   VMS_TEAM = 2, // menu select team
   VMS_TF = 26, // terrorist select menu
   VMS_CT = 27  // ct select menu
};

// lift usage states
enum LiftState {
   LIFT_NO_NEARBY = 0,
   LIFT_LOOKING_BUTTON_OUTSIDE,
   LIFT_WAITING_FOR,
   LIFT_ENTERING_IN,
   LIFT_WAIT_FOR_TEAMMATES,
   LIFT_LOOKING_BUTTON_INSIDE,
   LIFT_TRAVELING_BY,
   LIFT_LEAVING
};

// wayponit auto-downloader
enum WaypointDownloadError {
   WDE_SOCKET_ERROR,
   WDE_CONNECT_ERROR,
   WDE_NOTFOUND_ERROR,
   WDE_NOERROR
};

// game start messages for counter-strike...
enum GameMessage {
   GAME_MSG_NONE = 1,
   GAME_MSG_TEAM_SELECT = 2,
   GAME_MSG_CLASS_SELECT = 3,
   GAME_MSG_PURCHASE = 100,
   GAME_MSG_RADIO = 200,
   GAME_MSG_SAY_CMD = 10000,
   GAME_MSG_SAY_TEAM_MSG = 10001
};

// sensing states
enum SensingState {
   STATE_SEEING_ENEMY = (1 << 0), // seeing an enemy
   STATE_HEARING_ENEMY = (1 << 1), // hearing an enemy
   STATE_SUSPECT_ENEMY = (1 << 2), // suspect enemy behind obstacle
   STATE_PICKUP_ITEM = (1 << 3), // pickup item nearby
   STATE_THROW_HE = (1 << 4), // could throw he grenade
   STATE_THROW_FB = (1 << 5), // could throw flashbang
   STATE_THROW_SG = (1 << 6) // could throw smokegrenade
};

// positions to aim at
enum AimPosition {
   AIM_NAVPOINT = (1 << 0), // aim at nav point
   AIM_CAMP = (1 << 1), // aim at camp vector
   AIM_PREDICT_PATH = (1 << 2), // aim at predicted path
   AIM_LAST_ENEMY = (1 << 3), // aim at last enemy
   AIM_ENTITY = (1 << 4), // aim at entity like buttons, hostages
   AIM_ENEMY = (1 << 5), // aim at enemy
   AIM_GRENADE = (1 << 6), // aim for grenade throw
   AIM_OVERRIDE = (1 << 7)  // overrides all others (blinded)
};

// famas/glock burst mode status + m4a1/usp silencer
enum BurstMode {
   BM_ON = 1,
   BM_OFF = 2
};

// visibility flags
enum Visibility {
   VISIBLE_HEAD = (1 << 1),
   VISIBLE_BODY = (1 << 2),
   VISIBLE_OTHER = (1 << 3)
};

// defines map type
enum MapFlags {
   MAP_AS = (1 << 0),
   MAP_CS = (1 << 1),
   MAP_DE = (1 << 2),
   MAP_ES = (1 << 3),
   MAP_KA = (1 << 4),
   MAP_FY = (1 << 5),

   // additional flags
   MAP_HAS_DOORS = (1 << 6)
};

// defines for waypoint flags field (32 bits are available)
enum WaypointFlag {
   FLAG_LIFT = (1 << 1), // wait for lift to be down before approaching this waypoint
   FLAG_CROUCH = (1 << 2), // must crouch to reach this waypoint
   FLAG_CROSSING = (1 << 3), // a target waypoint
   FLAG_GOAL = (1 << 4), // mission goal point (bomb, hostage etc.)
   FLAG_LADDER = (1 << 5), // waypoint is on ladder
   FLAG_RESCUE = (1 << 6), // waypoint is a hostage rescue point
   FLAG_CAMP = (1 << 7), // waypoint is a camping point
   FLAG_NOHOSTAGE = (1 << 8), // only use this waypoint if no hostage
   FLAG_DOUBLEJUMP = (1 << 9), // bot help's another bot (requster) to get somewhere (using djump)
   FLAG_SNIPER = (1 << 28), // it's a specific sniper point
   FLAG_TF_ONLY = (1 << 29), // it's a specific terrorist point
   FLAG_CF_ONLY = (1 << 30)  // it's a specific ct point
};

// defines for waypoint connection flags field (16 bits are available)
enum PathFlag {
   PATHFLAG_JUMP = (1 << 0) // must jump for this connection
};

// enum pathfind search type
enum SearchPathType {
   SEARCH_PATH_FASTEST = 0,
   SEARCH_PATH_SAFEST_FASTER,
   SEARCH_PATH_SAFEST
};

// defines waypoint connection types
enum PathConnection {
   CONNECTION_OUTGOING = 0,
   CONNECTION_INCOMING,
   CONNECTION_BOTHWAYS
};

// a* route state
enum RouteState {
   ROUTE_OPEN,
   ROUTE_CLOSED,
   ROUTE_NEW
};

// bot known file headers
const char FH_WAYPOINT[] = "PODWAY!";
const char FH_EXPERIENCE[] = "PODEXP!";
const char FH_VISTABLE[] = "PODVIS!";
const char FH_MATRIX[] = "PODMAT!";

constexpr int FV_WAYPOINT = 7;
constexpr int FV_EXPERIENCE = 3;
constexpr int FV_VISTABLE = 2;
constexpr int FV_MATRIX = 2;

// some hardcoded desire defines used to override calculated ones
constexpr float TASKPRI_NORMAL = 35.0f;
constexpr float TASKPRI_PAUSE = 36.0f;
constexpr float TASKPRI_CAMP = 37.0f;
constexpr float TASKPRI_SPRAYLOGO = 38.0f;
constexpr float TASKPRI_FOLLOWUSER = 39.0f;
constexpr float TASKPRI_MOVETOPOSITION = 50.0f;
constexpr float TASKPRI_DEFUSEBOMB = 89.0f;
constexpr float TASKPRI_PLANTBOMB = 89.0f;
constexpr float TASKPRI_ATTACK = 90.0f;
constexpr float TASKPRI_SEEKCOVER = 91.0f;
constexpr float TASKPRI_HIDE = 92.0f;
constexpr float TASKPRI_THROWGRENADE = 99.0f;
constexpr float TASKPRI_DOUBLEJUMP = 99.0f;
constexpr float TASKPRI_BLINDED = 100.0f;
constexpr float TASKPRI_SHOOTBREAKABLE = 100.0f;
constexpr float TASKPRI_ESCAPEFROMBOMB = 100.0f;

constexpr float MAX_GRENADE_TIMER = 2.15f;
constexpr float MAX_SPRAY_DISTANCE = 260.0f;
constexpr float MAX_SPRAY_DISTANCE_X2 = MAX_SPRAY_DISTANCE * 2;
constexpr float MAX_CHATTER_REPEAT = 99.0f;

constexpr int MAX_PATH_INDEX = 8;
constexpr int MAX_DAMAGE_VALUE = 2040;
constexpr int MAX_GOAL_VALUE = 2040;
constexpr int MAX_KILL_HISTORY = 16;
constexpr int MAX_WAYPOINTS = 1024;
constexpr int MAX_ROUTE_LENGTH = MAX_WAYPOINTS / 2;
constexpr int MAX_WEAPONS = 32;
constexpr int NUM_WEAPONS = 26;
constexpr int MAX_COLLIDE_MOVES = 3;
constexpr int MAX_ENGINE_PLAYERS = 32;
constexpr int MAX_PRINT_BUFFER = 1024;
constexpr int MAX_TEAM_COUNT = 2;
constexpr int INVALID_WAYPOINT_INDEX = -1;

constexpr int MAX_WAYPOINT_BUCKET_SIZE = static_cast <int> (MAX_WAYPOINTS * 0.65);
constexpr int MAX_WAYPOINT_BUCKET_MAX = MAX_WAYPOINTS * 8 / MAX_WAYPOINT_BUCKET_SIZE;
constexpr int MAX_WAYPOINT_BUCKET_WPTS = MAX_WAYPOINT_BUCKET_SIZE / MAX_WAYPOINT_BUCKET_MAX;

// weapon masks
constexpr int WEAPON_PRIMARY = ((1 << WEAPON_XM1014) | (1 << WEAPON_M3) | (1 << WEAPON_MAC10) | (1 << WEAPON_UMP45) | (1 << WEAPON_MP5) | (1 << WEAPON_TMP) | (1 << WEAPON_P90) | (1 << WEAPON_AUG) | (1 << WEAPON_M4A1) | (1 << WEAPON_SG552) | (1 << WEAPON_AK47) | (1 << WEAPON_SCOUT) | (1 << WEAPON_SG550) | (1 << WEAPON_AWP) | (1 << WEAPON_G3SG1) | (1 << WEAPON_M249) | (1 << WEAPON_FAMAS) | (1 << WEAPON_GALIL));
constexpr int WEAPON_SECONDARY = ((1 << WEAPON_P228) | (1 << WEAPON_ELITE) | (1 << WEAPON_USP) | (1 << WEAPON_GLOCK) | (1 << WEAPON_DEAGLE) | (1 << WEAPON_FIVESEVEN));

// a* route
struct Route {
   float g, f;
   int parent;
   RouteState state;
};

// links keywords and replies together
struct KeywordFactory {
   StringArray keywords;
   StringArray replies;
   StringArray usedReplies;
};

// tasks definition
struct Task {
   TaskID id; // major task/action carried out
   float desire; // desire (filled in) for this task
   int data; // additional data (waypoint index)
   float time; // time task expires
   bool resume; // if task can be continued if interrupted
};

// botname structure definition
struct BotName {
   String steamId;
   String name;
   int usedBy;
};

// voice config structure definition
struct ChatterItem {
   String name;
   float repeat;
   float duration;
};

struct WeaponSelect {
   int id; // the weapon id value
   const char *weaponName; // name of the weapon when selecting it
   const char *modelName; // model name to separate cs weapons
   int price; // price when buying
   int minPrimaryAmmo; // minimum primary ammo
   int teamStandard; // used by team (number) (standard map)
   int teamAS; // used by team (as map)
   int buyGroup; // group in buy menu (standard map)
   int buySelect; // select item in buy menu (standard map)
   int newBuySelectT; // for counter-strike v1.6
   int newBuySelectCT; // for counter-strike v1.6
   int penetratePower; // penetrate power
   bool primaryFireHold; // hold down primary fire button to use?
};

// struct for menus
struct MenuText {
   MenuId id; // actual menu id
   int slots; //  together bits for valid keys
   String text; // ptr to actual string
};

// array of clients struct
struct Client {
   MenuId menu; // id to opened bot menu
   edict_t *ent; // pointer to actual edict
   Vector origin; // position in the world
   Vector soundPos; // position sound was played

   int team; // bot team
   int team2; // real bot team in free for all mode (csdm)
   int flags; // client flags

   float hearingDistance; // distance this sound is heared
   float timeSoundLasting; // time sound is played/heared

   int iconFlags[MAX_ENGINE_PLAYERS]; // flag holding chatter icons
   float iconTimestamp[MAX_ENGINE_PLAYERS]; // timers for chatter icons
};

// experience data hold in memory while playing
struct Experience {
   uint16 team0Damage;
   uint16 team1Damage;
   int16 team0DangerIndex;
   int16 team1DangerIndex;
   int16 team0Value;
   int16 team1Value;
};

// experience data when saving/loading
struct ExperienceSave {
   uint8 team0Damage;
   uint8 team1Damage;
   int8 team0Value;
   int8 team1Value;
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

// weapon properties structure
struct WeaponProperty {
   char className[64];
   int ammo1; // ammo index for primary ammo
   int ammo1Max; // max primary ammo
   int slotID; // HUD slot (0 based)
   int position; // slot position
   int id; // weapon ID
   int flags; // flags???
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
struct WaypointHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   char mapName[32];
   char author[32];
};

// general experience & vistable header information structure
struct ExtensionHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
};

// define general waypoint structure
struct Path {
   int32 pathNumber;
   int32 flags;
   Vector origin;
   float radius;

   float campStartX;
   float campStartY;
   float campEndX;
   float campEndY;

   int16 index[MAX_PATH_INDEX];
   uint16 connectionFlags[MAX_PATH_INDEX];
   Vector connectionVelocity[MAX_PATH_INDEX];
   int32 distances[MAX_PATH_INDEX];

   struct Vis {
      uint16 stand, crouch;
   } vis;
};

// this structure links waypoints returned from pathfinder
class PathWalk : public IntArray {
public:
   PathWalk (void) {
      reserve (MAX_ROUTE_LENGTH);
      clear ();
   }

   ~PathWalk (void) = default;
public:
   inline int &next (void) {
      return at (1);
   }

   inline int &first (void) {
      return at (0);
   }

   inline bool hasNext (void) const {
      if (empty ()) {
         return false;
      }
      return length () > 1;
   }
};

// main bot class
class Bot {
   friend class BotManager;

private:
   unsigned int m_states; // sensing bitstates

   float m_moveSpeed; // current speed forward/backward
   float m_strafeSpeed; // current speed sideways
   float m_minSpeed; // minimum speed in normal mode
   float m_oldCombatDesire; // holds old desire for filtering

   bool m_isLeader; // bot is leader of his team
   bool m_checkTerrain; // check for terrain
   bool m_moveToC4; // ct is moving to bomb
   bool m_grenadeRequested; // bot requested change to grenade

   float m_prevTime; // time previously checked movement speed
   float m_prevSpeed; // speed some frames before
   Vector m_prevOrigin; // origin some frames before

   int m_messageQueue[32]; // stack for messages
   String m_tempStrings; // space for strings (say text...)
   int m_radioSelect; // radio entry
   float m_headedTime;

   edict_t *m_avoid; // avoid players on our way
   float m_avoidTime; // time to avoid players around

   float m_itemCheckTime; // time next search for items needs to be done
   PickupType m_pickupType; // type of entity which needs to be used/picked up
   Vector m_breakableOrigin; // origin of breakable

   edict_t *m_pickupItem; // pointer to entity of item to use/pickup
   edict_t *m_itemIgnore; // pointer to entity to ignore for pickup
   edict_t *m_liftEntity; // pointer to lift entity
   edict_t *m_breakableEntity; // pointer to breakable entity

   float m_timeDoorOpen; // time to next door open check
   float m_lastChatTime; // time bot last chatted
   float m_timeLogoSpray; // time bot last spray logo
   float m_knifeAttackTime; // time to rush with knife (at the beginning of the round)
   bool m_defendedBomb; // defend action issued
   bool m_defendHostage;

   float m_askCheckTime; // time to ask team
   float m_collideTime; // time last collision
   float m_firstCollideTime; // time of first collision
   float m_probeTime; // time of probing different moves
   float m_lastCollTime; // time until next collision check
   float m_jumpStateTimer; // timer for jumping collision check

   unsigned int m_collisionProbeBits; // bits of possible collision moves
   unsigned int m_collideMoves[MAX_COLLIDE_MOVES]; // sorted array of movements
   unsigned int m_collStateIndex; // index into collide moves
   CollisionState m_collisionState; // collision State

   BinaryHeap <int, float, MAX_ROUTE_LENGTH> m_routeQue;
   Array <Route> m_routes; // pointer
   PathWalk m_path; // pointer to current node from path
   Path *m_currentPath; // pointer to the current path waypoint

   SearchPathType m_pathType; // which pathfinder to use
   uint8 m_visibility; // visibility flags

   int m_currentWaypointIndex; // current waypoint index
   int m_travelStartIndex; // travel start index to double jump action
   int m_prevWptIndex[5]; // previous waypoint indices from waypoint find
   int m_waypointFlags; // current waypoint flags

   int m_loosedBombWptIndex; // nearest to loosed bomb waypoint
   int m_plantedBombWptIndex; // nearest to planted bomb waypoint

   uint16 m_currentTravelFlags; // connection flags like jumping
   bool m_jumpFinished; // has bot finished jumping
   Vector m_desiredVelocity; // desired velocity for jump waypoints
   float m_navTimeset; // time waypoint chosen by Bot

   unsigned int m_aimFlags; // aiming conditions
   Vector m_lookAt; // vector bot should look at
   Vector m_throw; // origin of waypoint to throw grenades

   Vector m_enemyOrigin; // target origin chosen for shooting
   Vector m_grenade; // calculated vector for grenades
   Vector m_entity; // origin of entities like buttons etc.
   Vector m_camp; // aiming vector when camping.

   float m_timeWaypointMove; // last time bot followed waypoints
   bool m_wantsToFire; // bot needs consider firing
   float m_shootAtDeadTime; // time to shoot at dying players
   edict_t *m_avoidGrenade; // pointer to grenade entity to avoid
   int m_needAvoidGrenade; // which direction to strafe away

   float m_followWaitTime; // wait to follow time
   edict_t *m_targetEntity; // the entity that the bot is trying to reach
   Array <edict_t *> m_hostages; // pointer to used hostage entities

   bool m_moveToGoal; // bot currently moving to goal??
   bool m_isStuck; // bot is stuck
   bool m_isReloading; // bot is reloading a gun
   bool m_forceRadio; // should bot use radio anyway?

   int m_oldButtons; // our old buttons
   int m_reloadState; // current reload state
   int m_voicePitch; // bot voice pitch

   bool m_duckDefuse; // should or not bot duck to defuse bomb
   float m_duckDefuseCheckTime; // time to check for ducking for defuse

   float m_frameInterval; // bot's frame interval
   float m_lastCommandTime; // time bot last thinked

   float m_reloadCheckTime; // time to check reloading
   float m_zoomCheckTime; // time to check zoom again
   float m_shieldCheckTime; // time to check shiled drawing again
   float m_grenadeCheckTime; // time to check grenade usage
   float m_sniperStopTime; // bot switched to other weapon?
   float m_lastEquipTime; // last time we equipped in buyzone

   bool m_checkKnifeSwitch; // is time to check switch to knife action
   bool m_checkWeaponSwitch; // is time to check weapon switch
   bool m_isUsingGrenade; // bot currently using grenade??
   bool m_bombSearchOverridden; // use normal waypoint if applicable when near the bomb

   StrafeDir m_combatStrafeDir; // direction to strafe
   FightStyle m_fightStyle; // combat style to use
   float m_lastFightStyleCheck; // time checked style
   float m_strafeSetTime; // time strafe direction was set

   float m_timeCamping; // time to camp
   int m_campDirection; // camp Facing direction
   float m_nextCampDirTime; // time next camp direction change
   int m_campButtons; // buttons to press while camping
   int m_tryOpenDoor; // attempt's to open the door
   int m_liftState; // state of lift handling

   float m_duckTime; // time to duck
   float m_jumpTime; // time last jump happened
   float m_chatterTimes[Chatter_Total]; // voice command timers
   float m_soundUpdateTime; // time to update the sound
   float m_heardSoundTime; // last time noise is heard
   float m_buttonPushTime; // time to push the button
   float m_liftUsageTime; // time to use lift

   int m_pingOffset[2];
   int m_ping[3]; // bots pings in scoreboard

   Vector m_liftTravelPos; // lift travel position
   Vector m_moveAngles; // bot move angles
   Vector m_idealAngles; // angle wanted

   float m_lookYawVel; // look yaw velocity
   float m_lookPitchVel; // look pitch velocity
   float m_lookUpdateTime; // lookangles update time
   float m_aimErrorTime; // time to update error vector

   Vector m_randomizedIdealAngles; // angle wanted with noise
   Vector m_angularDeviation; // angular deviation from current to ideal angles
   Vector m_aimSpeed; // aim speed calculated
   Vector m_aimLastError; // last calculated aim error

   float m_randomizeAnglesTime; // time last randomized location
   float m_playerTargetTime; // time last targeting

   void instantChatter (int type);
   void ai (void);
   void checkSpawnConditions (void);
   void buyStuff (void);

   bool canReplaceWeapon (void);
   int pickBestWeapon (int *vec, int count, int moneySave);

   bool canDuckUnder (const Vector &normal);
   bool canJumpUp (const Vector &normal);
   bool doneCanJumpUp (const Vector &normal);
   bool cantMoveForward (const Vector &normal, TraceResult *tr);

#ifdef DEAD_CODE
   bool canStrafeLeft (TraceResult *tr);
   bool canStrafeRight (TraceResult *tr);

   bool isBlockedLeft (void);
   bool isBlockedRight (void);

   void changePitch (float speed);
   void changeYaw (float speed);
#endif

   void checkMsgQueue (void);
   void checkRadioQueue (void);
   void checkReload (void);
   int getMaxClip (int id);
   void avoidGrenades (void);
   void checkGrenadesThrow (void);
   void checkBurstMode (float distance);
   void checkSilencer (void);
   void updateAimDir (void);
   bool checkWallOnLeft (void);
   bool checkWallOnRight (void);
   int getBombPoint (void);

   bool processNavigation (void);
   bool isEnemyThreat (void);
   void processLookAngles (void);
   void processBodyAngles (void);
   void updateLookAnglesNewbie (const Vector &direction, const float delta);
   void setIdealReactionTimers (bool actual = false);
   bool isWeaponRestricted (int weaponIndex);
   bool isWeaponRestrictedAMX (int weaponIndex);

   bool isInViewCone (const Vector &origin);
   void processHearing (void);
   bool checkBodyParts (edict_t *target, Vector *origin, uint8 *bodyPart);
   bool seesEnemy (edict_t *player, bool ignoreFOV = false);

   edict_t *getNearestButton (const char *className);
   edict_t *lookupBreakable (void);
   int getCoverPoint (float maxDistance);
   int getDefendPoint (const Vector &origin);
   int searchGoal (void);
   int getGoalProcess (int tactic, IntArray *defensive, IntArray *offsensive);
   void filterGoals (const IntArray &goals, int *result);
   void processPickups (void);
   void checkTerrain (float movedDistance, const Vector &dirNormal);
   bool doPlayerAvoidance (const Vector &normal);

   void getCampDir (Vector *dest);
   void collectGoalExperience (int damage, int team);
   void collectDataExperience (edict_t *attacker, int damage);
   int getMsgQueue (void);
   bool hasActiveGoal (void);
   bool advanceMovement (void);
   float isInFOV (const Vector &dest);

   bool isBombDefusing (const Vector &bombOrigin);
   bool isOccupiedPoint (int index);

   inline bool isOnLadder (void) {
      return pev->movetype == MOVETYPE_FLY;
   }

   inline bool isOnFloor (void) {
      return (pev->flags & (FL_ONGROUND | FL_PARTIALGROUND)) != 0;
   }

   inline bool isInWater (void) {
      return pev->waterlevel >= 2;
   }
   float getShiftSpeed (void);

   bool seesItem (const Vector &dest, const char *itemName);
   bool lastEnemyShootable (void);
   void processTasks (void);

   // split big task list into separate functions
   void normal_ (void);
   void spraypaint_ (void);
   void huntEnemy_ (void);
   void seekCover_ (void);
   void attackEnemy_ (void);
   void pause_ (void);
   void blind_ (void);
   void camp_ (void);
   void hide_ (void);
   void moveToPos_ (void);
   void plantBomb_ (void);
   void bombDefuse_ (void);
   void followUser_ (void);
   void throwExplosive_ (void);
   void throwFlashbang_ (void);
   void throwSmoke_ (void);
   void doublejump_ (void);
   void escapeFromBomb_ (void);
   void pickupItem_ (void);
   void shootBreakable_ (void);

   bool isShootableBreakable (edict_t *ent);
   bool rateGroundWeapon (edict_t *ent);
   bool reactOnEnemy (void);
   void resetCollision (void);
   void ignoreCollision (void);
   void setConditions (void);
   void overrideConditions (void);
   void updateEmotions (void);
   void setStrafeSpeed (const Vector &moveDir, float strafeSpeed);
   void processTeamJoin (void);
   void completeTask (void);
   bool getNextBestPoint (void);

   int bestPrimaryCarried (void);
   int bestSecondaryCarried (void);
   int bestGrenadeCarried (void);
   int bestWeaponCarried (void);
   bool hasAnyWeapons (void);

   void processMovement (void);
   uint8 computeMsec (void);
   void getValidPoint (void);

   int changePointIndex (int index);
   int getNearestPoint (void);
   bool isDeadlyMove (const Vector &to);
   bool isOutOfBombTimer (void);

   edict_t *correctGrenadeVelocity (const char *model);
   Vector calcThrow (const Vector &spot1, const Vector &spot2);
   Vector calcToss (const Vector &spot1, const Vector &spot2);
   Vector isBombAudible (void);

   const Vector &getEnemyBodyOffset (void);
   Vector getBodyOffserError (float distance);
   float getEnemyBodyOffsetCorrection (float distance);

   void processTeamCommands (void);
   void decideFollowUser (void);
   void attackMovement (void);
   bool isWeaponBadAtDistance (int weaponIndex, float distance);
   bool throttleFiring (float distance);
   bool lookupEnemies (void);
   bool isEnemyHidden (edict_t *enemy);
   void fireWeapons (void);
   void selectWeapons (float distance, int index, int id, int choosen);
   void focusEnemy (void);

   void selectBestWeapon (void);
   void selectSecondary (void);
   bool isFriendInLineOfFire (float distance);
   bool isGroupOfEnemies (const Vector &location, int numEnemies = 1, float radius = 256.0f);

   bool isPenetrableObstacle (const Vector &dest);
   bool isPenetrableObstacle2 (const Vector &dest);

   int numEnemiesNear (const Vector &origin, float radius);
   int numFriendsNear (const Vector &origin, float radius);

   void selectWeaponByName (const char *name);
   void selectWeaponById (int num);

   bool isEnemyBehindShield (edict_t *enemy);
   bool processChatKeywords (char *reply);
   bool isReplyingToChat (void);
   float getBombTimeleft (void);
   float getReachTime (void);

   int searchCampDir (void);
   int searchAimingPoint (const Vector &to);

   void searchShortestPath (int srcIndex, int destIndex);
   void searchPath (int srcIndex, int destIndex, SearchPathType pathType = SEARCH_PATH_FASTEST);
   void clearRoute (void);
   void sayDebug (const char *format, ...);
   void frame (void);

public:
   entvars_t *pev;

   int m_wantedTeam; // player team bot wants select
   int m_wantedClass; // player model bot wants to select

   int m_difficulty;
   int m_moneyAmount; // amount of money in bot's bank

   Personality m_personality;
   float m_spawnTime; // time this bot spawned
   float m_timeTeamOrder; // time of last radio command
   float m_timePeriodicUpdate; // time to per-second think
   float m_timeRepotingInDelay; // time to delay report-in

   bool m_isVIP; // bot is vip?

   int m_numEnemiesLeft; // number of enemies alive left on map
   int m_numFriendsLeft; // number of friend alive left on map

   int m_retryJoin; // retry count for chosing team/class
   int m_startAction; // team/class selection state

   bool m_notKilled; // has the player been killed or has he just respawned
   bool m_notStarted; // team/class not chosen yet

   int m_voteKickIndex; // index of player to vote against
   int m_lastVoteKick; // last index
   int m_voteMap; // number of map to vote for
   int m_logotypeIndex; // index for logotype

   bool m_ignoreBuyDelay; // when reaching buyzone in the middle of the round don't do pauses
   bool m_inBombZone; // bot in the bomb zone or not
   int m_buyState; // current Count in Buying
   float m_nextBuyTime; // next buy time

   bool m_inBuyZone; // bot currently in buy zone
   bool m_inVIPZone; // bot in the vip satefy zone
   bool m_buyingFinished; // done with buying
   bool m_buyPending; // bot buy is pending
   bool m_hasDefuser; // does bot has defuser
   bool m_hasC4; // does bot has c4 bomb
   bool m_hasProgressBar; // has progress bar on a HUD
   bool m_jumpReady; // is double jump ready
   bool m_canChooseAimDirection; // can choose aiming direction
   float m_turnAwayFromFlashbang; // bot turned away from flashbang

   float m_blindTime; // time when bot is blinded
   float m_blindMoveSpeed; // mad speeds when bot is blind
   float m_blindSidemoveSpeed; // mad side move speeds when bot is blind
   int m_blindButton; // buttons bot press, when blind

   edict_t *m_doubleJumpEntity; // pointer to entity that request double jump
   edict_t *m_radioEntity; // pointer to entity issuing a radio command
   int m_radioOrder; // actual command

   float m_duckForJump; // is bot needed to duck for double jump
   float m_baseAgressionLevel; // base aggression level (on initializing)
   float m_baseFearLevel; // base fear level (on initializing)
   float m_agressionLevel; // dynamic aggression level (in game)
   float m_fearLevel; // dynamic fear level (in game)
   float m_nextEmotionUpdate; // next time to sanitize emotions
   float m_thinkFps; // skip some frames in bot thinking
   float m_thinkInterval; // interval between frames

   int m_actMessageIndex; // current processed message
   int m_pushMessageIndex; // offset for next pushed message

   int m_prevGoalIndex; // holds destination goal waypoint
   int m_chosenGoalIndex; // used for experience, same as above
   float m_goalValue; // ranking value for this waypoint

   Vector m_waypointOrigin; // origin of waypoint
   Vector m_destOrigin; // origin of move destination
   Vector m_position; // position to move to in move to position task
   Vector m_doubleJumpOrigin; // origin of double jump

   float m_viewDistance; // current view distance
   float m_maxViewDistance; // maximum view distance
   Vector m_lastEnemyOrigin; // vector to last enemy origin
   ChatCollection m_sayTextBuffer; // holds the index & the actual message of the last unprocessed text message of a player
   BurstMode m_weaponBurstMode; // bot using burst mode? (famas/glock18, but also silencer mode)

   edict_t *m_enemy; // pointer to enemy entity
   float m_retreatTime; // time to retreat?
   float m_enemyUpdateTime; // time to check for new enemies
   float m_enemyReachableTimer; // time to recheck if enemy reachable
   float m_enemyIgnoreTimer; // ignore enemy for some time
   bool m_isEnemyReachable; // direct line to enemy

   float m_seeEnemyTime; // time bot sees enemy
   float m_enemySurpriseTime; // time of surprise
   float m_idealReactionTime; // time of base reaction
   float m_actualReactionTime; // time of current reaction time

   edict_t *m_lastEnemy; // pointer to last enemy entity
   edict_t *m_lastVictim; // pointer to killed entity
   edict_t *m_trackingEdict; // pointer to last tracked player when camping/hiding
   float m_timeNextTracking; // time waypoint index for tracking player is recalculated

   float m_firePause; // time to pause firing
   float m_shootTime; // time to shoot
   float m_timeLastFired; // time to last firing
   int m_lastDamageType; // stores last damage

   int m_currentWeapon; // one current weapon for each bot
   int m_ammoInClip[MAX_WEAPONS]; // ammo in clip for each weapons
   int m_ammo[MAX_AMMO_SLOTS]; // total ammo amounts
   int m_team; // bot team
   Array<Task> m_tasks;

   Bot (edict_t *bot, int difficulty, int personality, int team, int member, const String &steamId);
   ~Bot (void);

   int ammo (void);
   inline int ammoClip (void) {
      return m_ammoInClip[m_currentWeapon];
   }

   inline edict_t *ent (void) {
      return pev->pContainingEntity;
   };
   int index (void);

   inline Vector centerPos (void) {
      return (pev->absmax + pev->absmin) * 0.5;
   };
   inline Vector eyePos (void) {
      return pev->origin + pev->view_ofs;
   };

   float calcThinkInterval (void);

   // the main function that decides intervals of running bot ai
   void framePeriodic (void);

   /// the things that can be executed while skipping frames
   void frameThink (void);

   void processBlind (int alpha);
   void processDamage (edict_t *inflictor, int damage, int armor, int bits);

   void showDebugOverlay (void);
   void newRound (void);
   void processBuyzoneEntering (int buyState);
   void pushMsgQueue (int message);
   void prepareChatMessage (char *text);
   bool searchOptimalPoint (void);
   bool seesEntity (const Vector &dest, bool fromBody = false);

   void showChaterIcon (bool show);
   void clearSearchNodes (void);
   void processBreakables (edict_t *touch);
   void avoidIncomingPlayers (edict_t *touch);

   void startTask (TaskID id, float desire, int data, float time, bool canContinue);
   void clearTask (TaskID id);
   void filterTasks (void);
   void clearTasks (void);

   Task *task (void);

   inline TaskID taskId (void) {
      return task ()->id;
   }

   void dropWeaponForUser (edict_t *user, bool discardC4);
   void clearUsedName (void);

   void say (const char *text);
   void sayTeam (const char *text);

   void pushChatMessage (int type, bool isTeamSay = false);
   void pushRadioMessage (int message);
   void pushChatterMessage (int message);
   void processChatterMessage (const char *sz);
   void tryHeadTowardRadioMessage (void);

   void kill (void);
   void kick (void);

   void resetDoubleJump (void);
   void startDoubleJump (edict_t *ent);

   int locatePlantedC4 (void);

   bool hasHostage (void);
   bool usesRifle (void);
   bool usesPistol (void);
   bool usesSniper (void);
   bool usesSubmachine (void);
   bool usesZoomableRifle (void);
   bool usesBadWeapon (void);
   bool usesCampGun (void);
   bool hasPrimaryWeapon (void);
   bool hasSecondaryWeapon (void);
   bool hasShield (void);
   bool isShieldDrawn (void);
};

// manager class
class BotManager : public Singleton<BotManager> {
private:
   Array <CreateQueue> m_creationTab; // bot creation tab

   Bot *m_bots[MAX_ENGINE_PLAYERS]; // all available bots

   float m_maintainTime; // time to maintain bot creation
   float m_quotaMaintainTime; // time to maintain bot quota
   float m_grenadeUpdateTime; // time to update active grenades
   float m_entityUpdateTime; // time to update intresting entities

   int m_lastWinner; // the team who won previous round

   bool m_leaderChoosen[MAX_TEAM_COUNT]; // is team leader choose theese round
   bool m_economicsGood[MAX_TEAM_COUNT]; // is team able to buy anything
   bool m_deathMsgSent; // for fake ping

   Array <edict_t *> m_activeGrenades; // holds currently active grenades on the map
   Array <edict_t *> m_intrestingEntities;  // holds currently intresting entities on the map
   edict_t *m_killerEntity; // killer entity for bots

protected:
   BotCreationResult create (const String &name, int difficulty, int personality, int team, int member);

public:
   BotManager (void);
   ~BotManager (void);

   inline bool checkTeamEco (int team) {
      return m_economicsGood[team];
   }

   bool isTeamStacked (int team);

   inline int getLastWinner (void) const {
      return m_lastWinner;
   }

   inline void setLastWinner (int winner) {
      m_lastWinner = winner;
   }
   void reset (void);

   int index (edict_t *ent);
   Bot *getBot (int index);
   Bot *getBot (edict_t *ent);
   Bot *getAliveBot (void);
   Bot *getHighfragBot (int team);

   int getHumansCount (bool ignoreSpectators = false);
   int getAliveHumansCount (void);
   int getBotCount (void);

   void framePeriodic (void);
   void frame (void);

   void createKillerEntity (void);
   void destroyKillerEntity (void);
   void touchKillerEntity (Bot *bot);

   void destroy (void);
   void destroy (int index);

   void createRandom (bool manual = false) {
      addbot ("", -1, -1, -1, -1, manual);
   }
   void addbot (const String &name, int difficulty, int personality, int team, int member, bool manual);
   void addbot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual);
   void serverFill (int selection, int personality = PERSONALITY_NORMAL, int difficulty = -1, int numToAdd = -1);

   void kickEveryone (bool instant = false,  bool zeroQuota = true);
   void kickRandom (bool decQuota = true);
   void kickBot (int index);
   void kickFromTeam (Team team, bool removeAll = false);

   void kickBotByMenu (edict_t *ent, int selection);
   void killAllBots (int team = -1);
   void maintainQuota (void);
   void initQuota (void);
   void decrementQuota (int by = 1);
   void selectLeaders (int team, bool reset);

   void listBots (void);
   void setWeaponMode (int selection);
   void updateTeamEconomics (int team, bool setTrue = false);

   static void execGameEntity (entvars_t *vars);

   // grenades
   void updateActiveGrenade (void);
   void updateIntrestingEntities (void);

   inline Array <edict_t *> &searchActiveGrenades (void) {
      return m_activeGrenades;
   }

   inline Array <edict_t *> &searchIntrestingEntities (void) {
      return m_intrestingEntities;
   }

   inline bool hasActiveGrenades (void) {
      return !m_activeGrenades.empty ();
   }

   inline bool hasIntrestingEntities (void) {
      return !m_intrestingEntities.empty ();
   }

public:
   void calculatePingOffsets (void);
   void sendPingOffsets (edict_t *to);
   void sendDeathMsgFix (void);

   inline void updateDeathMsgState (bool sent) {
      m_deathMsgSent = sent;
   }
};

// waypoint operation class
class Waypoint : public Singleton<Waypoint> {

public:
   friend class Bot;

private:
   struct Bucket {
      int x, y, z;
   };

   Path *m_paths[MAX_WAYPOINTS];

   bool m_waypointPaths;
   bool m_isOnLadder;
   bool m_endJumpPoint;
   bool m_learnJumpWaypoint;
   bool m_waypointsChanged;
   float m_timeJumpStarted;

   Vector m_learnVelocity;
   Vector m_learnPosition;
   Vector m_bombPos;

   int m_numWaypoints;
   int m_loadTries;
   int m_cacheWaypointIndex;
   int m_lastJumpWaypoint;
   int m_visibilityIndex;
   Vector m_lastWaypoint;
   uint8 m_visLUT[MAX_WAYPOINTS][MAX_WAYPOINTS / 4];

   float m_pathDisplayTime;
   float m_arrowDisplayTime;
   float m_waypointDisplayTime[MAX_WAYPOINTS];
   int m_findWPIndex;
   int m_facingAtIndex;
   char m_infoBuffer[256];

   int *m_distMatrix;
   int *m_pathMatrix;

   IntArray m_terrorPoints;
   IntArray m_ctPoints;
   IntArray m_goalPoints;
   IntArray m_campPoints;
   IntArray m_sniperPoints;
   IntArray m_rescuePoints;
   IntArray m_visitedGoals;

   IntArray m_buckets[MAX_WAYPOINT_BUCKET_MAX][MAX_WAYPOINT_BUCKET_MAX][MAX_WAYPOINT_BUCKET_MAX];

public:
   bool m_redoneVisibility;

   Waypoint (void);
   ~Waypoint (void);

   void init (void);
   void initExperience (void);
   void initVisibility (void);
   void initTypes (void);

   void addPath (int addIndex, int pathIndex, float distance);

   int getFacingIndex (void);
   int getFarest (const Vector &origin, float maxDistance = 32.0);
   int getNearest (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   int getNearestFallback (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   IntArray searchRadius (float radius, const Vector &origin, int maxCount = -1);

   void push (int flags, const Vector &waypointOrigin = Vector::null ());
   void erase (void);
   void toggleFlags (int toggleFlag);
   void setRadius (int radius);
   bool isConnected (int pointA, int pointB);
   bool isConnected (int num);
   void rebuildVisibility (void);
   void pathCreate (char dir);
   void erasePath (void);
   void cachePoint (void);

   float calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin);
   bool isVisible (int srcIndex, int destIndex);
   bool isStandVisible (int srcIndex, int destIndex);
   bool isDuckVisible (int srcIndex, int destIndex);
   void calculatePathRadius (int index);

   bool load (void);
   void save (void);
   void cleanupPathMemory (void);

   bool isReachable (Bot *bot, int index);
   bool isNodeReacheable (const Vector &src, const Vector &destination);
   void frame (void);
   bool checkNodes (void);
   void saveExperience (void);
   void saveVisibility (void);
   void addBasic (void);
   void eraseFromDisk (void);

   void initPathMatrix (void);
   void savePathMatrix (void);
   bool loadPathMatrix (void);

   int getPathDist (int srcIndex, int destIndex);
   const char *getInformation (int id);

   char *getAuthor (void) {
      return m_infoBuffer;
   }
   bool hasChanged (void) {
      return m_waypointsChanged;
   }

   void setSearchIndex (int index);
   void startLearnJump (void);

   bool isVisited (int index);
   void setVisited (int index);
   void clearVisited (void);

   const char *getDataDirectory (bool isMemoryFile = false);
   const char *getWaypointFilename (bool isMemoryFile = false);

   void setBombPos (bool reset = false, const Vector &pos = Vector::null ());

   inline const Vector &getBombPos (void) {
      return m_bombPos;
   }

   // access paths
   inline Path &operator [] (int index) {
      return *m_paths[index];
   }

   // check waypoints range
   inline bool exists (int index) const {
      return index >= 0 && index < m_numWaypoints;
   }

   // get real waypoint num
   inline int length (void) const {
      return m_numWaypoints;
   }

   // free's socket handle
   void closeSocket (int sock);

   // do actually downloading of waypoint file
   WaypointDownloadError downloadWaypoint (void);

   // initalize waypoint buckets
   void initBuckets (void);

   void addToBucket (const Vector &pos, int index);
   void eraseFromBucket (const Vector &pos, int index);

   Bucket locateBucket (const Vector &pos);
   IntArray &getWaypointsInBucket (const Vector &pos);
};

#include <engine.h>

// expose bot super-globals
static auto &waypoints = Waypoint::ref ();
static auto &bots = BotManager::ref ();
static auto &engine = Engine::ref ();
static auto &rng = RandomSequence::ref ();

// prototypes of bot functions...
extern int getWeaponData (bool isString, const char *weaponAlias, int weaponIndex = -1);
extern int getWeaponPenetrationPower (int id);
extern int buildNumber (void);
extern float getShootingConeDeviation (edict_t *ent, const Vector &position);

extern bool isVisible (const Vector &origin, edict_t *ent);
extern bool isAlive (edict_t *ent);
extern bool isInViewCone (const Vector &origin, edict_t *ent);
extern bool isFakeClient (edict_t *ent);
extern bool isPlayer (edict_t *ent);
extern bool isPlayerVIP (edict_t *ent);
extern bool openConfig (const char *fileName, const char *errorIfNotExists, MemFile *outFile, bool languageDependant = false);
extern bool findNearestPlayer (void **holder, edict_t *to, float searchDistance = 4096.0, bool sameTeam = false, bool needBot = false, bool needAlive = false, bool needDrawn = false, bool needBotWithC4 = false);

extern void cleanupGarbage (void);
extern void initRound (void);
extern void checkWelcome (void);
extern void logEntry (bool outputToConsole, int logLevel, const char *format, ...);
extern void showMenu (edict_t *ent, MenuId menu);
extern void traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex);
extern void attachSoundsToClients (edict_t *ent, const char *sample, float volume);
extern void simulateSoundUpdates (int playerIndex);

extern const char *format (const char *format, ...);

// very global convars
extern ConVar yb_jasonmode;
extern ConVar yb_communication_type;
extern ConVar yb_ignore_enemies;

#include <compress.h>
#include <globals.h>
#include <resource.h>

inline int Bot::index (void) {
   return engine.indexOfEntity (ent ());
}

static inline void makeVectors (const Vector &in) {
   in.makeVectors (&g_pGlobals->v_forward, &g_pGlobals->v_right, &g_pGlobals->v_up);
}