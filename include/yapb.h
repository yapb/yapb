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
#include <meta_api.h>

using namespace cr::types;
using namespace cr::classes;

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <time.h>

#include <compress.h>
#include <resource.h>

// defines bots tasks
enum TaskID : int {
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

// bot menu ids
enum MenuId : int {
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

// bomb say string
enum BombSayStr : int {
   BSS_NEED_TO_FIND_CHAT = cr::bit (1),
   BSS_NEED_TO_FIND_CHATTER = cr::bit (2)
};

// log levels
enum LogLevel : int {
   LL_DEFAULT = cr::bit (0), // default log message
   LL_WARNING = cr::bit (1), // warning log message
   LL_ERROR = cr::bit (2), // error log message
   LL_IGNORE = cr::bit (3), // additional flag
   LL_FATAL = cr::bit (4) // fatal error log message (terminate the game!)
};

// chat types id's
enum ChatType : int {
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
enum Personality : int {
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
enum CollisionState : int {
   COLLISION_NOTDECICED,
   COLLISION_PROBING,
   COLLISION_NOMOVE,
   COLLISION_JUMP,
   COLLISION_DUCK,
   COLLISION_STRAFELEFT,
   COLLISION_STRAFERIGHT
};

// counter-strike team id's
enum Team : int {
   TEAM_TERRORIST = 0,
   TEAM_COUNTER,
   TEAM_SPECTATOR,
   TEAM_UNASSIGNED
};

// client flags
enum ClientFlags : int {
   CF_USED = cr::bit (0),
   CF_ALIVE = cr::bit (1),
   CF_ADMIN = cr::bit (2),
   CF_ICON = cr::bit (3)
};

// bot create status
enum BotCreationResult : int {
   BOT_RESULT_CREATED,
   BOT_RESULT_MAX_PLAYERS_REACHED,
   BOT_RESULT_NAV_ERROR,
   BOT_RESULT_TEAM_STACKED
};

// radio messages
enum RadioMessage : int {
   RADIO_COVER_ME = 1,
   RADIO_YOU_TAKE_THE_POINT = 2,
   RADIO_HOLD_THIS_POSITION = 3,
   RADIO_REGROUP_TEAM = 4,
   RADIO_FOLLOW_ME = 5,
   RADIO_TAKING_FIRE = 6,
   RADIO_GO_GO_GO = 11,
   RADIO_TEAM_FALLBACK = 12,
   RADIO_STICK_TOGETHER_TEAM = 13,
   RADIO_GET_IN_POSITION = 14,
   RADIO_STORM_THE_FRONT = 15,
   RADIO_REPORT_TEAM = 16,
   RADIO_AFFIRMATIVE = 21,
   RADIO_ENEMY_SPOTTED = 22,
   RADIO_NEED_BACKUP = 23,
   RADIO_SECTOR_CLEAR = 24,
   RADIO_IN_POSITION = 25,
   RADIO_REPORTING_IN = 26,
   RADIO_SHES_GONNA_BLOW = 27,
   RADIO_NEGATIVE = 28,
   RADIO_ENEMY_DOWN = 29
};

// chatter system (extending enum above, messages 30-39 is reserved)
enum ChatterMessage : int {
   CHATTER_SPOT_THE_BOMBER = 40,
   CHATTER_FRIENDLY_FIRE,
   CHATTER_PAIN_DIED,
   CHATTER_BLINDED,
   CHATTER_GOING_TO_PLANT_BOMB,
   CHATTER_RESCUING_HOSTAGES,
   CHATTER_GOING_TO_CAMP,
   CHATTER_HEARD_NOISE,
   CHATTER_TEAM_ATTACK,
   CHATTER_REPORTING_IN,
   CHATTER_GUARDING_DROPPED_BOMB,
   CHATTER_CAMP,
   CHATTER_PLANTING_BOMB,
   CHATTER_DEFUSING_BOMB,
   CHATTER_IN_COMBAT,
   CHATTER_SEEK_ENEMY,
   CHATTER_NOTHING,
   CHATTER_ENEMY_DOWN,
   CHATTER_USING_HOSTAGES,
   CHATTER_FOUND_BOMB,
   CHATTER_WON_THE_ROUND,
   CHATTER_SCARED_EMOTE,
   CHATTER_HEARD_ENEMY,
   CHATTER_SNIPER_WARNING,
   CHATTER_SNIPER_KILLED,
   CHATTER_VIP_SPOTTED,
   CHATTER_GUARDING_VIP_SAFETY,
   CHATTER_GOING_TO_GUARD_VIP_SAFETY,
   CHATTER_QUICK_WON_ROUND,
   CHATTER_ONE_ENEMY_LEFT,
   CHATTER_TWO_ENEMIES_LEFT,
   CHATTER_THREE_ENEMIES_LEFT,
   CHATTER_NO_ENEMIES_LEFT,
   CHATTER_FOUND_BOMB_PLACE,
   CHATTER_WHERE_IS_THE_BOMB,
   CHATTER_DEFENDING_BOMBSITE,
   CHATTER_BARELY_DEFUSED,
   CHATTER_NICESHOT_COMMANDER,
   CHATTER_NICESHOT_PALL,
   CHATTER_GOING_TO_GUARD_HOSTAGES,
   CHATTER_GOING_TO_GUARD_DROPPED_BOMB,
   CHATTER_ON_MY_WAY,
   CHATTER_LEAD_ON_SIR,
   CHATTER_PINNED_DOWN,
   CHATTER_GOTTA_FIND_BOMB,
   CHATTER_YOU_HEARD_THE_MAN,
   CHATTER_LOST_COMMANDER,
   CHATTER_NEW_ROUND,
   CHATTER_COVER_ME,
   CHATTER_BEHIND_SMOKE,
   CHATTER_BOMB_SITE_SECURED,
   CHATTER_MAX
};

// counter-strike weapon id's
enum Weapon : int {
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
enum BuyState : int {
   BUYSTATE_PRIMARY_WEAPON = 0,
   BUYSTATE_ARMOR_VESTHELM,
   BUYSTATE_SECONDARY_WEAPON,
   BUYSTATE_GRENADES,
   BUYSTATE_DEFUSER,
   BUYSTATE_AMMO,
   BUYSTATE_NIGHTVISION,
   BUYSTATE_FINISHED
};

// economics limits
enum EconomyLimit : int {
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
enum PickupType : int {
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
enum FightStyle : int {
   FIGHT_NONE,
   FIGHT_STRAFE,
   FIGHT_STAY
};

// dodge type
enum StrafeDir : int {
   STRAFE_DIR_NONE,
   STRAFE_DIR_LEFT,
   STRAFE_DIR_RIGHT
};

// reload state
enum ReloadState : int {
   RELOAD_NONE = 0, // no reload state currently
   RELOAD_PRIMARY = 1, // primary weapon reload state
   RELOAD_SECONDARY = 2  // secondary weapon reload state
};

// collision probes
enum CollisionProbe : int {
   PROBE_JUMP = cr::bit (0), // probe jump when colliding
   PROBE_DUCK = cr::bit (1), // probe duck when colliding
   PROBE_STRAFE = cr::bit (2) // probe strafing when colliding
};

// vgui menus (since latest steam updates is obsolete, but left for old cs)
enum VGuiMenu : int {
   VMS_TEAM = 2, // menu select team
   VMS_TF = 26, // terrorist select menu
   VMS_CT = 27  // ct select menu
};

// lift usage states
enum LiftState : int {
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
enum WaypointDownloadError : int {
   WDE_SOCKET_ERROR,
   WDE_CONNECT_ERROR,
   WDE_NOTFOUND_ERROR,
   WDE_NOERROR
};

// game start messages for counter-strike...
enum GameMessage : int {
   GAME_MSG_NONE = 1,
   GAME_MSG_TEAM_SELECT = 2,
   GAME_MSG_CLASS_SELECT = 3,
   GAME_MSG_PURCHASE = 100,
   GAME_MSG_RADIO = 200,
   GAME_MSG_SAY_CMD = 10000,
   GAME_MSG_SAY_TEAM_MSG = 10001
};

// sensing states
enum SensingState : int {
   STATE_SEEING_ENEMY = cr::bit (0), // seeing an enemy
   STATE_HEARING_ENEMY = cr::bit (1), // hearing an enemy
   STATE_SUSPECT_ENEMY = cr::bit (2), // suspect enemy behind obstacle
   STATE_PICKUP_ITEM = cr::bit (3), // pickup item nearby
   STATE_THROW_HE = cr::bit (4), // could throw he grenade
   STATE_THROW_FB = cr::bit (5), // could throw flashbang
   STATE_THROW_SG = cr::bit (6) // could throw smokegrenade
};

// positions to aim at
enum AimPosition : int {
   AIM_NAVPOINT = cr::bit (0), // aim at nav point
   AIM_CAMP = cr::bit (1), // aim at camp vector
   AIM_PREDICT_PATH = cr::bit (2), // aim at predicted path
   AIM_LAST_ENEMY = cr::bit (3), // aim at last enemy
   AIM_ENTITY = cr::bit (4), // aim at entity like buttons, hostages
   AIM_ENEMY = cr::bit (5), // aim at enemy
   AIM_GRENADE = cr::bit (6), // aim for grenade throw
   AIM_OVERRIDE = cr::bit (7) // overrides all others (blinded)
};

// famas/glock burst mode status + m4a1/usp silencer
enum BurstMode : int {
   BURST_ON = cr::bit (0),
   BURST_OFF = cr::bit (1)
};

// visibility flags
enum Visibility : int {
   VISIBLE_HEAD = cr::bit (1),
   VISIBLE_BODY = cr::bit (2),
   VISIBLE_OTHER = cr::bit (3)
};

// defines for waypoint flags field (32 bits are available)
enum WaypointFlag : int32 {
   FLAG_LIFT = cr::bit (1), // wait for lift to be down before approaching this waypoint
   FLAG_CROUCH = cr::bit (2), // must crouch to reach this waypoint
   FLAG_CROSSING = cr::bit (3), // a target waypoint
   FLAG_GOAL = cr::bit (4), // mission goal point (bomb, hostage etc.)
   FLAG_LADDER = cr::bit (5), // waypoint is on ladder
   FLAG_RESCUE = cr::bit (6), // waypoint is a hostage rescue point
   FLAG_CAMP = cr::bit (7), // waypoint is a camping point
   FLAG_NOHOSTAGE = cr::bit (8), // only use this waypoint if no hostage
   FLAG_DOUBLEJUMP = cr::bit (9), // bot help's another bot (requster) to get somewhere (using djump)
   FLAG_SNIPER = cr::bit (28), // it's a specific sniper point
   FLAG_TF_ONLY = cr::bit (29), // it's a specific terrorist point
   FLAG_CF_ONLY = cr::bit (30),  // it's a specific ct point
};

// defines for waypoint connection flags field (16 bits are available)
enum PathFlag : int {
   PATHFLAG_JUMP = cr::bit (0) // must jump for this connection
};

// enum pathfind search type
enum SearchPathType : int {
   SEARCH_PATH_FASTEST = 0,
   SEARCH_PATH_SAFEST_FASTER,
   SEARCH_PATH_SAFEST
};

// defines waypoint connection types
enum PathConnection : int {
   CONNECTION_OUTGOING = 0,
   CONNECTION_INCOMING,
   CONNECTION_BOTHWAYS
};

// a* route state
enum RouteState : int {
   ROUTE_OPEN = 0,
   ROUTE_CLOSED,
   ROUTE_NEW
};

// waypoint edit states
enum WaypointEditState : int {
   WS_EDIT_ENABLED = cr::bit (1),
   WS_EDIT_NOCLIP = cr::bit (2),
   WS_EDIT_AUTO = cr::bit (3)
};

// bot known file headers
constexpr char FH_WAYPOINT[8] = "PODWAY!";
constexpr char FH_EXPERIENCE[8] = "SKYEXP!";
constexpr char FH_VISTABLE[8] = "SKYVIS!";
constexpr char FH_MATRIX[8] = "SKYPMX!";

constexpr int FV_WAYPOINT = 7;
constexpr int FV_EXPERIENCE = 5;
constexpr int FV_VISTABLE = 4;
constexpr int FV_MATRIX = 4;

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
constexpr int MAX_WAYPOINTS = 2048;
constexpr int MAX_ROUTE_LENGTH = MAX_WAYPOINTS / 2;
constexpr int MAX_WEAPONS = 32;
constexpr int NUM_WEAPONS = 26;
constexpr int MAX_COLLIDE_MOVES = 3;
constexpr int MAX_ENGINE_PLAYERS = 32;
constexpr int MAX_PRINT_BUFFER = 1024;
constexpr int MAX_TEAM_COUNT = 2;
constexpr int INVALID_WAYPOINT_INDEX = -1;

constexpr int MAX_WAYPOINT_BUCKET_SIZE = static_cast <int> (MAX_WAYPOINTS * 0.65);
constexpr int MAX_WAYPOINT_BUCKET_MAX = MAX_WAYPOINTS * 8 / MAX_WAYPOINT_BUCKET_SIZE + 1;
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
struct Keywords {
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
};

// array of clients struct
struct Client {
   edict_t *ent; // pointer to actual edict
   Vector origin; // position in the world
   Vector soundPos; // position sound was played
   int team; // bot team
   int team2; // real bot team in free for all mode (csdm)
   int flags; // client flags
   int radio; // radio orders
   int menu; // identifier to openen menu
   float hearingDistance; // distance this sound is heared
   float timeSoundLasting; // time sound is played/heared
   int iconFlags[MAX_ENGINE_PLAYERS]; // flag holding chatter icons
   float iconTimestamp[MAX_ENGINE_PLAYERS]; // timers for chatter icons
};

// experience data hold in memory while playing
struct Experience {
   int damage[MAX_TEAM_COUNT];
   int index[MAX_TEAM_COUNT];
   int value[MAX_TEAM_COUNT];
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
struct WaypointHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   char mapName[32];
   char author[32];
};

// general experience & vistable header information structure
struct ExtHeader {
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   int32 compressed;
   int32 uncompressed;
};

// floyd-warshall matrices
struct FloydMatrix {
   int dist;
   int index;
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
   int m_rechoiceGoalCount; // multiple failed goals?

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
   float m_chatterTimes[CHATTER_MAX]; // voice command timers
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
   void checkDarkness (void);
   void checkParachute (void);
   bool doPlayerAvoidance (const Vector &normal);

   void getCampDir (Vector *dest);
   void collectGoalExperience (int damage);
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

   void runMovement (void);
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
   Vector getBodyOffsetError (float distance);
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
   float m_slowFrameTimestamp; // time to per-second think
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
   int m_buyState; // current count in buying
   float m_nextBuyTime; // next buy time
   float m_checkDarkTime; // check for darkness time

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
   float m_turnAwayFromFlashbang; // bot turned away from flashbang

   float m_flashLevel; // flashlight level
   float m_blindTime; // time when bot is blinded
   float m_blindMoveSpeed; // mad speeds when bot is blind
   float m_blindSidemoveSpeed; // mad side move speeds when bot is blind
   int m_blindButton; // buttons bot press, when blind

   edict_t *m_doubleJumpEntity; // pointer to entity that request double jump
   edict_t *m_radioEntity; // pointer to entity issuing a radio command
   int m_radioOrder; // actual command

   float m_fallDownTime; // time bot started to fall 
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
   void slowFrame (void);

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
class BotManager : public Singleton <BotManager> {
private:
   Array <CreateQueue> m_creationTab; // bot creation tab

   Bot *m_bots[MAX_ENGINE_PLAYERS]; // all available bots

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
   float m_lastRadioTime[MAX_TEAM_COUNT]; // global radio time

   int m_lastWinner; // the team who won previous round
   int m_lastDifficulty; // last bots difficulty
   int m_bombSayStatus; // some bot is issued whine about bomb

   int m_radioSelect[MAX_ENGINE_PLAYERS];
   int m_lastRadio[MAX_TEAM_COUNT];

   bool m_leaderChoosen[MAX_TEAM_COUNT]; // is team leader choose theese round
   bool m_economicsGood[MAX_TEAM_COUNT]; // is team able to buy anything
   bool m_deathMsgSent; // for fake ping

   bool m_bombPlanted;
   bool m_botsCanPause;
   bool m_roundEnded;

   Array <edict_t *> m_activeGrenades; // holds currently active grenades on the map
   Array <edict_t *> m_intrestingEntities;  // holds currently intresting entities on the map

   edict_t *m_killerEntity; // killer entity for bots
   Array <Task> m_filters;

protected:
   BotCreationResult create (const String &name, int difficulty, int personality, int team, int member);

public:
   BotManager (void);
   ~BotManager (void);

public:
   bool isTeamStacked (int team);
   void setBombPlanted (bool isPlanted);

   int index (edict_t *ent);
   Bot *getBot (int index);
   Bot *getBot (edict_t *ent);
   Bot *getAliveBot (void);
   Bot *getHighfragBot (int team);

   int getHumansCount (bool ignoreSpectators = false);
   int getAliveHumansCount (void);
   int getBotCount (void);

   void countTeamPlayers (int &ts, int &cts);
   void slowFrame (void);
   void frame (void);
   void createKillerEntity (void);
   void destroyKillerEntity (void);
   void touchKillerEntity (Bot *bot);
   void destroy (void);
   void destroy (int index);
   void addbot (const String &name, int difficulty, int personality, int team, int member, bool manual);
   void addbot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual);
   void serverFill (int selection, int personality = PERSONALITY_NORMAL, int difficulty = -1, int numToAdd = -1);
   void kickEveryone (bool instant = false,  bool zeroQuota = true);
   bool kickRandom (bool decQuota = true, Team fromTeam = TEAM_UNASSIGNED);
   void kickBot (int index);
   void kickFromTeam (Team team, bool removeAll = false);
   void killAllBots (int team = -1);
   void maintainQuota (void);
   void initQuota (void);
   void initRound (void);
   void decrementQuota (int by = 1);
   void selectLeaders (int team, bool reset);
   void listBots (void);
   void setWeaponMode (int selection);
   void updateTeamEconomics (int team, bool setTrue = false);
   void updateBotDifficulties (void);
   void reset (void);
   void initFilters (void);
   void resetFilters (void);
   void updateActiveGrenade (void);
   void updateIntrestingEntities (void);
   void calculatePingOffsets (void);
   void sendPingOffsets (edict_t *to);
   void sendDeathMsgFix (void);
   void captureChatRadio (const char *cmd, const char *arg, edict_t *ent);

   static void execGameEntity (entvars_t *vars);

public:
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

   inline bool checkTeamEco (int team) {
      return m_economicsGood[team];
   }

   inline int getLastWinner (void) const {
      return m_lastWinner;
   }

   inline void setLastWinner (int winner) {
      m_lastWinner = winner;
   }

   // get the list of filters
   inline Array <Task> &getFilters (void) {
      return m_filters;
   }

   void createRandom (bool manual = false) {
      addbot ("", -1, -1, -1, -1, manual);
   }

   inline void updateDeathMsgState (bool sent) {
      m_deathMsgSent = sent;
   }

   inline bool isBombPlanted (void) const {
      return m_bombPlanted;
   }

   inline float getTimeBombPlanted (void) const {
      return m_timeBombPlanted;
   }

   inline float getRoundStartTime (void) const {
      return m_timeRoundStart;
   }

   inline float getRoundMidTime (void) const {
      return m_timeRoundMid;
   }

   inline float getRoundEndTime (void) const {
      return m_timeRoundEnd;
   }

   inline bool isRoundOver (void) const {
      return m_roundEnded;
   }

   inline void setRoundOver (const bool over) {
      m_roundEnded = over;
   }

   inline bool canPause (void) const {
      return m_botsCanPause;
   }

   inline void setCanPause (const bool pause) {
      m_botsCanPause = pause;
   }

   inline bool hasBombSay (int type) {
      return (m_bombSayStatus & type) == type;
   }

   inline void clearBombSay (int type) {
      m_bombSayStatus &= ~type;
   }

   inline void setPlantedBombSearchTimestamp (const float timestamp) {
      m_plantSearchUpdateTime = timestamp;
   }

   inline float getPlantedBombSearchTimestamp (void) const {
      return m_plantSearchUpdateTime;
   }

   inline void setLastRadioTimestamp (const int team, const float timestamp) {
      m_lastRadioTime[team] = timestamp;
   }

   inline float getLastRadioTimestamp (const int team) const {
      return m_lastRadioTime[team];
   }

   inline void setLastRadio (const int team, const int radio) {
      m_lastRadio[team] = radio;
   }

   inline int getLastRadio (const int team) const {
      return m_lastRadio[team];
   }

   inline void setLastChatTimestamp (const float timestamp) {
      m_lastChatTime = timestamp;
   }

   inline float getLastChatTimestamp (void) const {
      return m_lastChatTime;
   }
};

// waypoint operation class
class Waypoint : public Singleton <Waypoint> {
public:
   friend class Bot;

private:
   struct Bucket {
      int x, y, z;
   };

   Path *m_paths[MAX_WAYPOINTS];
   Experience *m_experience;

   bool m_waypointPaths;
   bool m_isOnLadder;
   bool m_endJumpPoint;
   bool m_learnJumpWaypoint;
   bool m_waypointsChanged;
   float m_timeJumpStarted;

   Vector m_learnVelocity;
   Vector m_learnPosition;
   Vector m_bombPos;

   int m_editFlags;
   int m_numWaypoints;
   int m_loadTries;
   int m_cacheWaypointIndex;
   int m_lastJumpWaypoint;
   int m_visibilityIndex;
   int m_killHistory;
   int m_highestDamage[MAX_TEAM_COUNT];

   Vector m_lastWaypoint;
   uint8 m_visLUT[MAX_WAYPOINTS][MAX_WAYPOINTS / 4];

   float m_autoPathDistance;
   float m_pathDisplayTime;
   float m_arrowDisplayTime;
   float m_waypointDisplayTime[MAX_WAYPOINTS];
   float m_waypointLightLevel[MAX_WAYPOINTS];

   int m_findWPIndex;
   int m_facingAtIndex;
   char m_infoBuffer[MAX_PRINT_BUFFER];

   IntArray m_terrorPoints;
   IntArray m_ctPoints;
   IntArray m_goalPoints;
   IntArray m_campPoints;
   IntArray m_sniperPoints;
   IntArray m_rescuePoints;
   IntArray m_visitedGoals;
   IntArray m_buckets[MAX_WAYPOINT_BUCKET_MAX][MAX_WAYPOINT_BUCKET_MAX][MAX_WAYPOINT_BUCKET_MAX];

   FloydMatrix *m_matrix;

public:
   bool m_redoneVisibility;

   Waypoint (void);
   ~Waypoint (void);

public:
   void init (void);
   void loadExperience (void);
   void loadVisibility (void);
   void initTypes (void);
   void initLightLevels (void);

   void addPath (int addIndex, int pathIndex, float distance);

   int getFacingIndex (void);
   int getFarest (const Vector &origin, float maxDistance = 32.0);
   int getNearest (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   int getNearestNoBuckets (const Vector &origin, float minDistance = 9999.0f, int flags = -1);
   int getEditorNeareset (void);
   IntArray searchRadius (float radius, const Vector &origin, int maxCount = -1);

   void push (int flags, const Vector &waypointOrigin = Vector::null ());
   void erase (int target);
   void toggleFlags (int toggleFlag);
   void setRadius (int index, float radius);
   bool isConnected (int pointA, int pointB);
   bool isConnected (int num);
   void rebuildVisibility (void);
   void pathCreate (char dir);
   void erasePath (void);
   void cachePoint (int index);

   float calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin);
   bool isVisible (int srcIndex, int destIndex);
   bool isStandVisible (int srcIndex, int destIndex);
   bool isDuckVisible (int srcIndex, int destIndex);
   void calculatePathRadius (int index);

   bool load (void);
   void save (void);
   void cleanupPathMemory (void);
   int removeUselessConnections (int index, bool outputToConsole = true);

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

   bool saveExtFile (const char *ext, const char *type, const char *magic, const int version, uint8 *data, const int32 size);
   bool loadExtFile (const char *ext, const char *type, const char *magic, const int version, uint8 *data);

   int getPathDist (int srcIndex, int destIndex);
   const char *getInformation (int id);

   void setSearchIndex (int index);
   void startLearnJump (void);

   bool isVisited (int index);
   void setVisited (int index);
   void clearVisited (void);

   const char *getDataDirectory (bool isMemoryFile = false);
   const char *getWaypointFilename (bool isMemoryFile = false);
   void setBombPos (bool reset = false, const Vector &pos = Vector::null ());

   void closeSocket (int sock);
   WaypointDownloadError downloadWaypoint (void);

   // initalize waypoint buckets
   void initBuckets (void);

   void addToBucket (const Vector &pos, int index);
   void eraseFromBucket (const Vector &pos, int index);

   Bucket locateBucket (const Vector &pos);
   IntArray &getWaypointsInBucket (const Vector &pos);

   void updateGlobalExperience (void);

   int getDangerIndex (int team, int start, int goal);
   int getDangerValue (int team, int start, int goal);
   int getDangerDamage (int team, int start, int goal);

public:
   inline Experience *getRawExperience (void) {
      return m_experience;
   }

   inline int getHighestDamageForTeam (int team) const {
      return m_highestDamage[team];
   }

   inline void setHighestDamageForTeam (int team, int value) {
      m_highestDamage[team] = value;
   }

   inline const char *getAuthor (void) const {
      return m_infoBuffer;
   }

   inline bool hasChanged (void) const {
      return m_waypointsChanged;
   }

   inline bool hasEditFlag (int flag) const {
      return !!(m_editFlags & flag);
   }

   inline void setEditFlag (int flag) {
      m_editFlags |= flag;
   }

   inline void clearEditFlag (int flag) {
      m_editFlags &= ~flag;
   }

   inline void setAutoPathDistance (const float distance) {
      m_autoPathDistance = distance;
   }

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

   // get the light level of waypoint
   inline float getLightLevel (int id) const {
      return m_waypointLightLevel[id];
   }
};

// mostly config stuff, and some stuff dealing with menus
class Config final : public Singleton <Config> {
private:
   Array <StringArray> m_chat;
   Array <Array <ChatterItem>> m_chatter;
   Array <BotName> m_botNames;
   Array <Keywords> m_replies;
   Array <WeaponInfo> m_weapons;

   // weapon info gathered through engine messages
   WeaponProp m_weaponProps[MAX_WEAPONS + 1];

   // default tables for personality weapon preferences, overridden by general.cfg
   int m_normalWeaponPrefs[NUM_WEAPONS] =       { 0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 18, 21, 17, 19, 15, 17, 9, 14, 16 };
   int m_rusherWeaponPrefs[NUM_WEAPONS] =       { 0, 2, 1, 4, 5, 6, 3, 24, 19, 22, 23, 20, 21, 10, 12, 13, 7, 8, 11, 9, 18, 17, 19, 25, 15, 16 };
   int m_carefulWeaponPrefs[NUM_WEAPONS] =      { 0, 2, 1, 4, 25, 6, 3, 7, 8, 12, 10, 13, 11, 9, 24, 18, 14, 17, 16, 15, 19, 20, 21, 22, 23, 5 };
   int m_grenadeBuyPrecent[NUM_WEAPONS - 23] =  { 95, 85, 60 };
   int m_botBuyEconomyTable[NUM_WEAPONS - 15] = { 1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000 };
   int *m_weaponPrefs[3] =                      { m_normalWeaponPrefs, m_rusherWeaponPrefs, m_carefulWeaponPrefs };

public:
   Config (void) = default;
   ~Config (void) = default;

public:

   // load the configuration files
   void load (bool onlyMain);

   // picks random bot name
   BotName *pickBotName (void);

   // remove bot name from used list
   void clearUsedName (Bot *bot);

   // initialize weapon info
   void initWeapons (void);

   // fix weapon prices (ie for elite)
   void adjustWeaponPrices (void);

   WeaponInfo &findWeaponById (const int id);

public:

   // get the chat array
   inline Array <StringArray> &getChat (void) {
      return m_chat;
   }

   // get's the chatter array
   inline Array <Array <ChatterItem>> &getChatter (void) {
      return m_chatter;
   }

   // get's the replies array
   inline Array <Keywords> &getReplies (void) {
      return m_replies;
   }

   // get's the weapon info data
   inline Array <WeaponInfo> &getWeapons (void) {
      return m_weapons;
   }

   // get's raw weapon info
   inline WeaponInfo *getRawWeapons (void) {
      return m_weapons.begin ();
   }

   // set's the weapon properties
   inline void setWeaponProp (const WeaponProp &prop) {
      m_weaponProps[prop.id] = prop;
   }

   // get's the weapons prop
   inline const WeaponProp &getWeaponProp (const int id) const {
      return m_weaponProps[id];
   }

   // get's weapon preferences for personality
   inline int *getWeaponPrefs (const int personality) const {
      return m_weaponPrefs[personality];
   }

   // get economics value
   inline int *getEconLimit (void) {
      return m_botBuyEconomyTable;
   }

   // get's grenade buy percents
   inline bool chanceToBuyGrenade (const int grenadeType) const {
      return RandomSequence::ref ().chance (m_grenadeBuyPrecent[grenadeType]);
   }
};

class BotUtils final : public Singleton <BotUtils> {
private:
   bool m_needToSendWelcome;
   float m_welcomeReceiveTime;
   StringArray m_sentences;
   Array <Client> m_clients;

public:
   BotUtils (void);
   ~BotUtils (void) = default;

public:
   // need to send welcome message ?
   void checkWelcome (void);

   // gets the weapon alias as hlsting, maybe move to config...
   int getWeaponAlias (bool isString, const char *weaponAlias, int weaponIndex = -1);

   // gets the build number of bot
   int buildNumber (void);

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

   // simple loggerfor  bots
   void logEntry (bool outputToConsole, int logLevel, const char *format, ...);

   // tracing decals for bots spraying logos
   void traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex);

   // attaches sound to client struct
   void attachSoundsToClients (edict_t *ent, const char *sample, float volume);

   // simulate sound for players
   void simulateSoundUpdates (int playerIndex);

   // simple format utility
   const char *format (const char *format, ...);

   // update stats on clients
   void updateClients (void);

public:

   // re-show welcome after changelevel ?
   inline void setNeedForWelcome (bool need) {
      m_needToSendWelcome = need;
   }

   // get array of clients
   inline Array <Client> &getClients (void) {
      return m_clients;
   }

   // get clients as const-reference
   inline const Array <Client> &getClients (void) const {
      return m_clients;
   }

   // get sinle client as ref
   inline Client &getClient (const int index) {
      return m_clients[index];
   }

   // checks if string is not empty
   inline bool isEmptyStr (const char *input) {
      if (input == nullptr) {
         return true;
      }
      return *input == '\0';
   }
};

// command handler status
enum BotCommandStatus : int {
   CMD_STATUS_HANDLED = 0, // command successfully handled 
   CMD_STATUS_LISTENSERV, // command is only avaialble on listen server
   CMD_STATUS_DENIED, // access to this command is denied
   CMD_STATUS_BADFORMAT // wrong params
};

// bot command manager
class BotControl final : public Singleton <BotControl> {
public:
   using Handler = int (BotControl::*) (void);
   using MenuHandler = int (BotControl::*) (int);

public:
   // generic bot command
   struct BotCmd {
      String name, format, help;
      Handler handler;
   };

   // single bot menu
   struct Menu {
      int ident, slots;
      String text;
      MenuHandler handler;
   };

private:
   StringArray m_args;
   Array <BotCmd> m_cmds;
   Array <Menu> m_menus;

   edict_t *m_ent;

   bool m_isFromConsole;
   bool m_isMenuFillCommand;
   int m_menuServerFillTeam;
   int m_interMenuData[4] = { 0, };

public:
   BotControl (void);
   ~BotControl (void) = default;

private:
   int cmdAddBot (void);
   int cmdKickBot (void);
   int cmdKickBots (void);
   int cmdKillBots (void);
   int cmdFill (void);
   int cmdVote (void);
   int cmdWeaponMode (void);
   int cmdVersion (void);
   int cmdWaypointMenu (void);
   int cmdMenu (void);
   int cmdList (void);
   int cmdWaypoint (void);
   int cmdWaypointOn (void);
   int cmdWaypointOff (void);
   int cmdWaypointAdd (void);
   int cmdWaypointAddBasic (void);
   int cmdWaypointSave (void);
   int cmdWaypointLoad (void);
   int cmdWaypointErase (void);
   int cmdWaypointDelete (void);
   int cmdWaypointCheck (void);
   int cmdWaypointCache (void);
   int cmdWaypointClean (void);
   int cmdWaypointSetRadius (void);
   int cmdWaypointSetFlags (void);
   int cmdWaypointTeleport (void);
   int cmdWaypointPathCreate (void);
   int cmdWaypointPathDelete (void);
   int cmdWaypointPathSetAutoDistance (void);

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
   int menuWaypointPage1 (int item);
   int menuWaypointPage2 (int item);
   int menuWaypointRadius (int item);
   int menuWaypointType (int item);
   int menuWaypointFlag (int item);
   int menuWaypointPath (int item);
   int menuAutoPathDistance (int item);
   int menuKickPage1 (int item);
   int menuKickPage2 (int item);
   int menuKickPage3 (int item);
   int menuKickPage4 (int item);

private:
   void enableDrawModels (const bool enable);
   void createMenus (void);

public:
   bool executeCommands (void);
   bool executeMenus (void);
   void showMenu (int id);
   void kickBotByMenu (int page);
   void msg (const char *fmt, ...);
   void assignAdminRights (edict_t *ent, char *infobuffer);
   void maintainAdminRights (void);

public:
   inline void setFromConsole (const bool console) {
      m_isFromConsole = console;
   }

   inline void setArgs (StringArray args) {
      m_args.assign (args);
   }

   inline void setIssuer (edict_t *ent) {
      m_ent = ent;
   }

   inline void fixMissingArgs (size_t num) {
      if (num < m_args.length ()) {
         return;
      }

      do {
         m_args.push ("");
      } while (num--);
   }

   inline int getInt (size_t arg) const {
      if (!hasArg (arg)) {
         return false;
      }
      return m_args[arg].toInt32 ();
   }

   inline const String &getStr (size_t arg) {
      static String empty ("empty");

      if (!hasArg (arg) || m_args[arg].empty ()) {
         return empty;
      }
      return m_args[arg];
   }

   inline bool hasArg (size_t arg) const {
      return arg < m_args.length ();
   }

   inline StringArray collectArgs (void) {
      StringArray args;

      for (int i = 0; i < engfuncs.pfnCmd_Argc (); i++) {
         args.push (engfuncs.pfnCmd_Argv (i));
      }
      return args;
   }

public:

   // for the server commands
   static void handleEngineCommands (void);

   // for the client commands
   bool handleClientCommands (edict_t *ent);

   // for the client menu commands
   bool handleMenuCommands (edict_t *ent);
};

#include <engine.h>

// expose bot super-globals
static auto &waypoints = Waypoint::ref ();
static auto &bots = BotManager::ref ();
static auto &game = Game::ref ();
static auto &rng = RandomSequence::ref ();
static auto &illum = LightMeasure::ref ();
static auto &conf = Config::ref ();
static auto &util = BotUtils::ref ();
static auto &ctrl = BotControl::ref ();

// very global convars
extern ConVar yb_jasonmode;
extern ConVar yb_communication_type;
extern ConVar yb_ignore_enemies;

inline int Bot::index (void) {
   return game.indexOfEntity (ent ());
}