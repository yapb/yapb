//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#pragma once

#include <extdll.h>
#include <stdio.h>
#include <memory.h>

#include <dllapi.h>
#include <meta_api.h>

using namespace Math;

// detects the build platform
#if defined (__linux__) || defined (__debian__) || defined (__linux)
   #define PLATFORM_LINUX 1
#elif defined (__APPLE__)
   #define PLATFORM_OSX 1
#elif defined (_WIN32)
   #define PLATFORM_WIN32 1
#endif

// detects the compiler
#if defined (_MSC_VER)
   #define COMPILER_VISUALC _MSC_VER
#elif defined (__MINGW32__)
   #define COMPILER_MINGW32 __MINGW32__
#endif

// configure export macros
#if defined (COMPILER_VISUALC) || defined (COMPILER_MINGW32)
   #define export extern "C" __declspec (dllexport)
#elif defined (PLATFORM_LINUX) || defined (PLATFORM_OSX) 
   #define export extern "C" __attribute__((visibility("default")))
#else
   #error "Can't configure export macros. Compiler unrecognized."
#endif

// operating system specific macros, functions and typedefs
#ifdef PLATFORM_WIN32

   #include <direct.h>

   #define DLL_ENTRYPOINT int STDCALL DllMain (HINSTANCE hModule, DWORD dwReason, LPVOID)
   #define DLL_DETACHING (dwReason == DLL_PROCESS_DETACH)
   #define DLL_RETENTRY return TRUE

   #if defined (COMPILER_VISUALC)
      #define DLL_GIVEFNPTRSTODLL extern "C" void STDCALL
   #elif defined (COMPILER_MINGW32)
      #define DLL_GIVEFNPTRSTODLL export void STDCALL
   #endif

   // specify export parameter
   #if defined (COMPILER_VISUALC) && (COMPILER_VISUALC > 1000)
      #pragma comment (linker, "/EXPORT:GiveFnptrsToDll=_GiveFnptrsToDll@8,@1")
      #pragma comment (linker, "/SECTION:.data,RW")
      #pragma warning (disable : 4288)
   #endif

   typedef int (FAR *EntityAPI_t) (gamefuncs_t *, int);
   typedef int (FAR *NewEntityAPI_t) (newgamefuncs_t *, int *);
   typedef int (FAR *BlendAPI_t) (int, void **, void *, float (*)[3][4], float (*)[128][3][4]);
   typedef void (STDCALL *FuncPointers_t) (enginefuncs_t *, globalvars_t *);
   typedef void (FAR *EntityPtr_t) (entvars_t *);

#elif defined (PLATFORM_LINUX) || defined (PLATFORM_OSX)

   #include <unistd.h>
   #include <dlfcn.h>
   #include <errno.h>
   #include <fcntl.h>
   #include <sys/stat.h>

   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <netdb.h>
   #include <arpa/inet.h>

   #define DLL_ENTRYPOINT void _fini (void)
   #define DLL_DETACHING TRUE
   #define DLL_RETENTRY return
   #define DLL_GIVEFNPTRSTODLL extern "C" void __attribute__((visibility("default")))

   static inline uint32 _lrotl (uint32 x, int r) { return (x << r) | (x >> (sizeof (x) * 8 - r));}

   typedef int (*EntityAPI_t) (gamefuncs_t *, int);
   typedef int (*NewEntityAPI_t) (newgamefuncs_t *, int *);
   typedef int (*BlendAPI_t) (int, void **, void *, float (*)[3][4], float (*)[128][3][4]);
   typedef void (*FuncPointers_t) (enginefuncs_t *, globalvars_t *);
   typedef void (*EntityPtr_t) (entvars_t *);

#else
   #error "Platform unrecognized."
#endif

// library wrapper
class Library
{
private:
   void *m_ptr;
   
public:

   Library (const char *fileName)
   {
      if (fileName == NULL)
         return;

#ifdef PLATFORM_WIN32
      m_ptr = LoadLibrary (fileName);
#else
      m_ptr = dlopen (fileName, RTLD_NOW);
#endif
   }

 ~Library (void)
  {
      if (!IsLoaded ())
         return;

#ifdef PLATFORM_WIN32
      FreeLibrary ((HMODULE) m_ptr);
#else
      dlclose (m_ptr);
#endif
  }

public:
   void *GetFunctionAddr (const char *functionName)
   {
      if (!IsLoaded ())
         return NULL;

#ifdef PLATFORM_WIN32
      return GetProcAddress ((HMODULE) m_ptr, functionName);
#else
      return dlsym (m_ptr, functionName);
#endif
   }

   template <typename R> R GetHandle (void)
   {
      return (R) m_ptr;
   }

   inline bool IsLoaded (void) const
   {
      return m_ptr != NULL;
   }
};

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <float.h>
#include <time.h>

#include <corelib.h>

// defines bots tasks
enum TaskId_t
{
   TASK_NORMAL,
   TASK_PAUSE,
   TASK_MOVETOPOSITION,
   TASK_FOLLOWUSER,
   TASK_WAITFORGO,
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
enum CSVersion
{
   CSV_STEAM = 1, // Counter-Strike 1.6 and Above
   CSV_CZERO = 2, // Counter-Strike: Condition Zero
   CSV_OLD = 3 // Counter-Strike 1.3-1.5 with/without Steam
};

// log levels
enum LogLevel
{
   LL_DEFAULT = 1, // default log message
   LL_WARNING = 2, // warning log message
   LL_ERROR = 3, // error log message
   LL_IGNORE = 4, // additional flag
   LL_FATAL = 5  // fatal error log message (terminate the game!)

};

// chat types id's
enum ChatType
{
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
enum Personality
{
   PERSONALITY_NORMAL = 0,
   PERSONALITY_RUSHER,
   PERSONALITY_CAREFUL
};

// collision states
enum CollisionState
{
   COLLISION_NOTDECICED,
   COLLISION_PROBING,
   COLLISION_NOMOVE,
   COLLISION_JUMP,
   COLLISION_DUCK,
   COLLISION_STRAFELEFT,
   COLLISION_STRAFERIGHT
};

// counter-strike team id's
enum Team
{
   TEAM_TF = 0,
   TEAM_CF,
   TEAM_SPEC
};

// client flags
enum ClientFlags
{
   CF_USED  = (1 << 0),
   CF_ALIVE = (1 << 1),
   CF_ADMIN = (1 << 2)
};

// radio messages
enum RadioMessage_t
{
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
enum ChatterMessage
{  
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
enum Weapon
{
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

// defines for pickup items
enum PickupType
{
   PICKUP_NONE,
   PICKUP_WEAPON,
   PICKUP_DROPPED_C4,
   PICKUP_PLANTED_C4,
   PICKUP_HOSTAGE,
   PICKUP_BUTTON,
   PICKUP_SHIELD,
   PICKUP_DEFUSEKIT
};

// reload state
enum ReloadState
{
   RELOAD_NONE = 0, // no reload state currrently
   RELOAD_PRIMARY = 1, // primary weapon reload state
   RELOAD_SECONDARY = 2  // secondary weapon reload state
};

// collision probes
enum CollisionProbe
{
   PROBE_JUMP = (1 << 0), // probe jump when colliding
   PROBE_DUCK = (1 << 1), // probe duck when colliding
   PROBE_STRAFE = (1 << 2) // probe strafing when colliding
};

// vgui menus (since latest steam updates is obsolete, but left for old cs)
enum VGuiMenu
{
   VMS_TEAM = 2, // menu select team
   VMS_TF = 26, // terrorist select menu
   VMS_CT = 27  // ct select menu
};

// lift usage states
enum LiftState
{
   LIFT_NO_NEARBY = 0,
   LIFT_LOOKING_BUTTON_OUTSIDE,
   LIFT_WAITING_FOR,
   LIFT_ENTERING_IN,
   LIFT_WAIT_FOR_TEAMMATES,
   LIFT_LOOKING_BUTTON_INSIDE,
   LIFT_TRAVELING_BY,
   LIFT_LEAVING
};

// game start messages for counter-strike...
enum GameStartMessage
{
   GSM_IDLE = 1,
   GSM_TEAM_SELECT = 2,
   GSM_CLASS_SELECT = 3,
   GSM_BUY_STUFF = 100,
   GSM_RADIO = 200,
   GSM_SAY = 10000,
   GSM_SAY_TEAM = 10001
};

// netmessage functions
enum NetworkMessage
{
   NETMSG_VGUI = 1,
   NETMSG_SHOWMENU = 2,
   NETMSG_WEAPONLIST = 3,
   NETMSG_CURWEAPON = 4,
   NETMSG_AMMOX = 5,
   NETMSG_AMMOPICKUP = 6,
   NETMSG_DAMAGE = 7,
   NETMSG_MONEY = 8,
   NETMSG_STATUSICON = 9,
   NETMSG_DEATH = 10,
   NETMSG_SCREENFADE = 11,
   NETMSG_HLTV = 12,
   NETMSG_TEXTMSG = 13,
   NETMSG_SCOREINFO = 14,
   NETMSG_BARTIME = 15,
   NETMSG_SENDAUDIO = 17,
   NETMSG_SAYTEXT = 18,
   NETMSG_BOTVOICE = 19,
   NETMSG_RESETHUD = 20,
   NETMSG_UNDEFINED = 0
};

// sensing states
enum SensingState
{
   STATE_SEEING_ENEMY = (1 << 0), // seeing an enemy
   STATE_HEARING_ENEMY = (1 << 1), // hearing an enemy
   STATE_SUSPECT_ENEMY = (1 << 2), // suspect enemy behind obstacle
   STATE_PICKUP_ITEM = (1 << 3), // pickup item nearby
   STATE_THROW_HE = (1 << 4), // could throw he grenade
   STATE_THROW_FB = (1 << 5), // could throw flashbang
   STATE_THROW_SG = (1 << 6) // could throw smokegrenade
};

// positions to aim at
enum AimPosition
{
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
enum BurstMode
{
   BM_ON  = 1,
   BM_OFF = 2
};

// visibility flags
enum Visibility
{
   VISIBLE_HEAD = (1 << 1),
   VISIBLE_BODY = (1 << 2),
   VISIBLE_OTHER = (1 << 3)
};

// defines map type
enum MapType
{
   MAP_AS = (1 << 0),
   MAP_CS = (1 << 1),
   MAP_DE = (1 << 2),
   MAP_ES = (1 << 3),
   MAP_KA = (1 << 4),
   MAP_FY = (1 << 5)
};

// defines for waypoint flags field (32 bits are available)
enum WaypointFlag
{
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
enum PathFlag
{
   PATHFLAG_JUMP = (1 << 0), // must jump for this connection
};

// defines waypoint connection types
enum PathConnection
{
   CONNECTION_OUTGOING = 0,
   CONNECTION_INCOMING,
   CONNECTION_BOTHWAYS
};

// bot known file headers
const char FH_WAYPOINT[] = "PODWAY!";
const char FH_EXPERIENCE[] = "PODEXP!";
const char FH_VISTABLE[] = "PODVIS!";

const int FV_WAYPOINT = 7;
const int FV_EXPERIENCE = 3;
const int FV_VISTABLE = 1;

// some hardcoded desire defines used to override calculated ones
const float TASKPRI_NORMAL = 35.0;
const float TASKPRI_PAUSE = 36.0;
const float TASKPRI_CAMP  = 37.0;
const float TASKPRI_SPRAYLOGO = 38.0;
const float TASKPRI_FOLLOWUSER = 39.0;
const float TASKPRI_MOVETOPOSITION = 50.0;
const float TASKPRI_DEFUSEBOMB = 89.0;
const float TASKPRI_PLANTBOMB = 89.0;
const float TASKPRI_ATTACK = 90.0;
const float TASKPRI_SEEKCOVER = 91.0;
const float TASKPRI_HIDE = 92.0;
const float TASKPRI_THROWGRENADE = 99.0;
const float TASKPRI_DOUBLEJUMP = 99.0;
const float TASKPRI_BLINDED = 100.0;
const float TASKPRI_SHOOTBREAKABLE = 100.0;
const float TASKPRI_ESCAPEFROMBOMB = 100.0;

const float MAX_GRENADE_TIMER = 2.34f;

const int MAX_HOSTAGES = 8;
const int MAX_PATH_INDEX = 8;
const int MAX_DAMAGE_VALUE = 2040;
const int MAX_GOAL_VALUE = 2040;
const int MAX_KILL_HISTORY = 16;
const int MAX_REG_MESSAGES = 256;
const int MAX_WAYPOINTS = 1024;
const int MAX_WEAPONS = 32;
const int NUM_WEAPONS = 26;

// weapon masks
const int WEAPON_PRIMARY = ((1 << WEAPON_XM1014) | (1 <<WEAPON_M3) | (1 << WEAPON_MAC10) | (1 << WEAPON_UMP45) | (1 << WEAPON_MP5) | (1 << WEAPON_TMP) | (1 << WEAPON_P90) | (1 << WEAPON_AUG) | (1 << WEAPON_M4A1) | (1 << WEAPON_SG552) | (1 << WEAPON_AK47) | (1 << WEAPON_SCOUT) | (1 << WEAPON_SG550) | (1 << WEAPON_AWP) | (1 << WEAPON_G3SG1) | (1 << WEAPON_M249) | (1 << WEAPON_FAMAS) | (1 << WEAPON_GALIL));
const int WEAPON_SECONDARY = ((1 << WEAPON_P228) | (1 << WEAPON_ELITE) | (1 << WEAPON_USP) | (1 << WEAPON_GLOCK) | (1 << WEAPON_DEAGLE) | (1 << WEAPON_FIVESEVEN));

// this structure links waypoints returned from pathfinder
struct PathNode
{
   int index;
   PathNode *next;
};

// links keywords and replies together
struct KeywordFactory
{
   Array <String> keywords;
   Array <String> replies;
   Array <String> usedReplies;
};

// tasks definition
struct TaskItem
{
   TaskId_t id; // major task/action carried out
   float desire; // desire (filled in) for this task
   int data; // additional data (waypoint index)
   float time; // time task expires
   bool resume; // if task can be continued if interrupted
};

// wave structure
struct WavHeader
{
   char riffChunkId[4];
   unsigned long packageSize;
   char chunkID[4];
   char formatChunkId[4];
   unsigned long formatChunkLength;
   unsigned short dummy;
   unsigned short channels;
   unsigned long sampleRate;
   unsigned long bytesPerSecond;
   unsigned short bytesPerSample;
   unsigned short bitsPerSample;
   char dataChunkId[4];
   unsigned long dataChunkLength;
};

// botname structure definition
struct BotName
{
   String name;
   bool used;
};

// voice config structure definition
struct ChatterItem
{
   String name;
   float repeatTime;
};

// language config structure definition
struct LanguageItem
{
   char *original; // original string
   char *translated; // string to replace for
};

struct WeaponSelect
{
   int id; // the weapon id value
   char *weaponName; // name of the weapon when selecting it
   char *modelName; // model name to separate cs weapons
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

// fire delay definiton
struct FireDelay
{
   int weaponIndex;
   int maxFireBullets;
   float minBurstPauseFactor;
   float primaryBaseDelay;
   float primaryMinDelay[6];
   float primaryMaxDelay[6];
   float secondaryBaseDelay;
   float secondaryMinDelay[5];
   float secondaryMaxDelay[5];
};

// struct for menus
struct MenuText
{
   int validSlots; // ored together bits for valid keys
   char *menuText; // ptr to actual string
};

// array of clients struct
struct Client
{
   MenuText *menu; // pointer to opened bot menu
   edict_t *ent; // pointer to actual edict
   Vector origin; // position in the world
   Vector soundPosition; // position sound was played
   int team; // bot team
   int realTeam; // real bot team in free for all mode (csdm)
   int flags; // client flags
   float hearingDistance; // distance this sound is heared
   float timeSoundLasting; // time sound is played/heared
   float maxTimeSoundLasting; // max time sound is played/heared (to divide the difference between that above one and the current one)
};

// experience data hold in memory while playing
struct Experience
{
   unsigned short team0Damage;
   unsigned short team1Damage;
   signed short team0DangerIndex;
   signed short team1DangerIndex;
   signed short team0Value;
   signed short team1Value;
};

// experience data when saving/loading
struct ExperienceSave
{
   unsigned char team0Damage;
   unsigned char team1Damage;
   signed char team0Value;
   signed char team1Value;
};

// bot creation tab
struct CreateQueue
{
   int difficulty;
   int team;
   int member;
   int personality;
   String name;
};

// weapon properties structure
struct WeaponProperty
{
   char className[64];
   int ammo1; // ammo index for primary ammo
   int ammo1Max; // max primary ammo
   int slotID; // HUD slot (0 based)
   int position; // slot position
   int id; // weapon ID
   int flags; // flags???
};

// define chatting collection structure
struct ChatCollection
{
   char chatProbability;
   float chatDelay;
   float timeNextChat;
   int entityIndex;
   char sayText[512];
   Array <String> lastUsedSentences;
};

// general waypoint header information structure
struct WaypointHeader
{
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
   char mapName[32];
   char author[32];
};

// general experience & vistable header information structure
struct ExtensionHeader
{
   char header[8];
   int32 fileVersion;
   int32 pointNumber;
};

// define general waypoint structure
struct Path
{
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

   struct Vis { uint16 stand, crouch; } vis;
};

// main bot class
class Bot
{
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

   float m_prevTime; // time previously checked movement speed
   float m_prevSpeed; // speed some frames before
   Vector m_prevOrigin; // origin some frames before

   int m_messageQueue[32]; // stack for messages
   char m_tempStrings[160]; // space for strings (say text...)
   int m_radioSelect; // radio entry
   float m_headedTime; 

   float m_blindRecognizeTime; // time to recognize enemy
   float m_itemCheckTime; // time next search for items needs to be done
   PickupType m_pickupType; // type of entity which needs to be used/picked up
   Vector m_breakable; // origin of breakable

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

   unsigned int m_collisionProbeBits; // bits of possible collision moves
   unsigned int m_collideMoves[4]; // sorted array of movements
   unsigned int m_collStateIndex; // index into collide moves
   CollisionState m_collisionState; // collision State

   PathNode *m_navNode; // pointer to current node from path
   PathNode *m_navNodeStart; // pointer to start of path finding nodes
   Path *m_currentPath; // pointer to the current path waypoint

   unsigned char m_pathType; // which pathfinder to use
   unsigned char m_visibility; // visibility flags

   int m_currentWaypointIndex; // current waypoint index
   int m_travelStartIndex; // travel start index to double jump action
   int m_prevWptIndex[5]; // previous waypoint indices from waypoint find
   int m_waypointFlags; // current waypoint flags
   int m_loosedBombWptIndex; // nearest to loosed bomb waypoint
   int m_plantedBombWptIndex;// nearest to planted bomb waypoint

   unsigned short m_currentTravelFlags; // connection flags like jumping
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
   char m_needAvoidGrenade; // which direction to strafe away

   float m_followWaitTime; // wait to follow time
   edict_t *m_targetEntity; // the entity that the bot is trying to reach
   edict_t *m_LeaderEntity; // the entity that the bot is picking as a leader
   edict_t *m_hostages[MAX_HOSTAGES]; // pointer to used hostage entities

   bool m_isStuck; // bot is stuck
   bool m_isReloading; // bot is reloading a gun
   int m_reloadState; // current reload state
   int m_voicePitch; // bot voice pitch

   bool m_duckDefuse; // should or not bot duck to defuse bomb
   float m_duckDefuseCheckTime; // time to check for ducking for defuse

   float m_msecVal; // calculated msec value
   float m_msecDel;

   float m_frameInterval; // bot's frame interval
   float m_lastThinkTime; // time bot last thinked

   float m_reloadCheckTime; // time to check reloading
   float m_zoomCheckTime; // time to check zoom again
   float m_shieldCheckTime; // time to check shiled drawing again
   float m_grenadeCheckTime; // time to check grenade usage
   float m_lastEquipTime; // last time we equipped in buyzone

   bool m_checkKnifeSwitch; // is time to check switch to knife action
   bool m_checkWeaponSwitch; // is time to check weapon switch
   bool m_isUsingGrenade; // bot currently using grenade??

   unsigned char m_combatStrafeDir; // direction to strafe
   unsigned char m_fightStyle; // combat style to use
   float m_lastFightStyleCheck; // time checked style
   float m_strafeSetTime; // time strafe direction was set

   float m_timeCamping; // time to camp
   int m_campDirection; // camp Facing direction
   float m_nextCampDirTime; // time next camp direction change
   int m_campButtons; // buttons to press while camping
   int m_doorOpenAttempt; // attempt's to open the door
   int m_liftState; // state of lift handling

   float m_duckTime; // time to duck
   float m_jumpTime; // time last jump happened
   float m_voiceTimers[Chatter_Total]; // voice command timers
   float m_soundUpdateTime; // time to update the sound
   float m_heardSoundTime; // last time noise is heard
   float m_buttonPushTime; // time to push the button
   float m_liftUsageTime; // time to use lift

   int m_pingOffset[2]; 
   int m_ping[3]; // bots pings in scoreboard

   Vector m_liftTravelPos; // lift travel position
   Vector m_moveAngles; // bot move angles
   bool m_moveToGoal; // bot currently moving to goal??

   Vector m_idealAngles; // angle wanted
   Vector m_randomizedIdealAngles; // angle wanted with noise
   Vector m_angularDeviation; // angular deviation from current to ideal angles
   Vector m_aimSpeed; // aim speed calculated
   Vector m_targetOriginAngularSpeed; // target/enemy angular speed

   float m_randomizeAnglesTime; // time last randomized location
   float m_playerTargetTime; // time last targeting

   void InstantChatterMessage (int type);
   void BotAI (void);
   void CheckSpawnTimeConditions (void);
   bool IsMorePowerfulWeaponCanBeBought (void);
   void PerformWeaponPurchase (void);

   bool CanDuckUnder (const Vector &normal);
   bool CanJumpUp (const Vector &normal);
   bool CanStrafeLeft (TraceResult *tr);
   bool CanStrafeRight (TraceResult *tr);
   bool CantMoveForward (const Vector &normal, TraceResult *tr);

   void ChangePitch (float speed);
   void ChangeYaw (float speed);
   void CheckMessageQueue (void);
   void CheckRadioCommands (void);
   void CheckReload (void);
   void AvoidGrenades (void);
   void CheckBurstMode (float distance);

   void CheckSilencer (void);
   bool CheckWallOnLeft (void);
   bool CheckWallOnRight (void);
   void ChooseAimDirection (void);
   int ChooseBombWaypoint (void);

   bool DoWaypointNav (void);
   bool EnemyIsThreat (void);
   void FacePosition (void);
   void SetIdealReactionTimes (bool actual = false);
   bool IsRestricted (int weaponIndex);
   bool IsRestrictedAMX (int weaponIndex);

   bool IsInViewCone (const Vector &origin);
   void ReactOnSound (void);
   bool CheckVisibility (entvars_t *targetOrigin, Vector *origin, byte *bodyPart);
   bool IsEnemyViewable (edict_t *player);

   edict_t *FindNearestButton (const char *className);
   edict_t *FindBreakable (void);
   int FindCoverWaypoint (float maxDistance);
   int FindDefendWaypoint (Vector origin);
   int FindGoal (void);
   void FindItem (void);
   void CheckTerrain (float movedDistance, const Vector &dir, const Vector &dirNormal);

   void GetCampDirection (Vector *dest);
   void CollectGoalExperience (int damage, int team);
   void CollectExperienceData (edict_t *attacker, int damage);
   int GetMessageQueue (void);
   bool GoalIsValid (void);
   bool HeadTowardWaypoint (void);
   int InFieldOfView (const Vector &dest);

   bool IsBombDefusing (Vector bombOrigin);
   bool IsBlockedLeft (void);
   bool IsBlockedRight (void);
   bool IsPointOccupied (int index);

   inline bool IsOnLadder (void) { return pev->movetype == MOVETYPE_FLY; }
   inline bool IsOnFloor (void) { return (pev->flags & (FL_ONGROUND | FL_PARTIALGROUND)) != 0; }
   inline bool IsInWater (void) { return pev->waterlevel >= 2; }

   inline float GetWalkSpeed (void) { return static_cast <float> ((static_cast <int> (pev->maxspeed) / 2) + (static_cast <int> (pev->maxspeed) / 50)) - 18; }
   
   bool ItemIsVisible (const Vector &dest, char *itemName);
   bool LastEnemyShootable (void);
   bool IsLastEnemyViewable (void);
   bool IsBehindSmokeClouds (edict_t *ent);
   void RunTask (void);

   bool IsShootableBreakable (edict_t *ent);
   bool RateGroundWeapon (edict_t *ent);
   bool ReactOnEnemy (void);
   void ResetCollideState (void);
   void SetConditions (void);
   void SetStrafeSpeed (Vector moveDir, float strafeSpeed);
   void StartGame (void);
   void TaskComplete (void);
   bool GetBestNextWaypoint (void);
   int GetBestWeaponCarried (void);
   int GetBestSecondaryWeaponCarried (void);

   void RunPlayerMovement (void);
   void ThrottleMsec(void);
   void GetValidWaypoint (void);
   void ChangeWptIndex (int waypointIndex);
   bool IsDeadlyDrop (Vector targetOriginPos);
   bool OutOfBombTimer (void);
   void SelectLeaderEachTeam (int team);

   Vector CheckToss (const Vector &start, Vector end);
   Vector CheckThrow (const Vector &start, Vector end);
   Vector GetAimPosition (void);
   Vector CheckBombAudible (void);

   float GetZOffset (float distance);

   int CheckGrenades (void);
   void CommandTeam (void);
   void AttachToUser (void);
   void CombatFight (void);
   bool IsWeaponBadInDistance (int weaponIndex, float distance);
   bool DoFirePause (float distance, FireDelay *fireDelay);
   bool LookupEnemy (void);
   void FireWeapon (void);
   void FocusEnemy (void);

   void SelectBestWeapon (void);
   void SelectPistol (void);
   bool IsFriendInLineOfFire (float distance);
   bool IsGroupOfEnemies (Vector location, int numEnemies = 1, int radius = 256);

   bool IsShootableThruObstacle (const Vector &dest);
   bool IsShootableThruObstacleEx (const Vector &dest);

   int GetNearbyEnemiesNearPosition (Vector origin, int radius);
   int GetNearbyFriendsNearPosition (Vector origin, int radius);
   void SelectWeaponByName (const char *name);
   void SelectWeaponbyNumber (int num);
   int GetHighestWeapon (void);

   bool IsEnemyProtectedByShield (edict_t *enemy);
   bool ParseChat (char *reply);
   bool RepliesToPlayer (void);
   float GetBombTimeleft (void);
   float GetEstimatedReachTime (void);

   int GetAimingWaypoint (void);
   int GetAimingWaypoint (Vector targetOriginPos);
   void FindShortestPath (int srcIndex, int destIndex);
   void FindPath (int srcIndex, int destIndex, unsigned char pathType = 0);
   void DebugMsg (const char *format, ...);
   void SecondThink (void);

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

   bool m_isVIP; // bot is vip?
   bool m_bIsDefendingTeam; // bot in defending team on this map

   int m_startAction; // team/class selection state
   bool m_notKilled; // has the player been killed or has he just respawned
   bool m_notStarted; // team/class not chosen yet

   int m_voteKickIndex; // index of player to vote against
   int m_lastVoteKick; // last index
   int m_voteMap; // number of map to vote for
   int m_logotypeIndex; // index for logotype
   int m_burstShotsFired; // number of bullets fired

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
   
   float m_breakableCheckTime;
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

   int m_actMessageIndex; // current processed message
   int m_pushMessageIndex; // offset for next pushed message

   int m_prevGoalIndex; // holds destination goal waypoint
   int m_chosenGoalIndex; // used for experience, same as above
   float m_goalValue; // ranking value for this waypoint

   Vector m_waypointOrigin; // origin of waypoint
   Vector m_destOrigin; // origin of move destination
   Vector m_position; // position to move to in move to position task
   Vector m_doubleJumpOrigin; // origin of double jump
   Vector m_lastBombPosition; // origin of last remembered bomb position

   float m_viewDistance; // current view distance
   float m_maxViewDistance; // maximum view distance
   Vector m_lastEnemyOrigin; // vector to last enemy origin
   ChatCollection m_sayTextBuffer; // holds the index & the actual message of the last unprocessed text message of a player
   BurstMode m_weaponBurstMode; // bot using burst mode? (famas/glock18, but also silencer mode)

   edict_t *m_enemy; // pointer to enemy entity
   float m_enemyUpdateTime; // time to check for new enemies
   float m_enemyReachableTimer; // time to recheck if Enemy reachable
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

   // a little optimization
   int m_team;

   Array <TaskItem> m_tasks;

   Bot (edict_t *bot, int difficulty, int personality, int team, int member);
  ~Bot (void);

   int GetAmmo (void);
   inline int GetAmmoInClip (void) { return m_ammoInClip[m_currentWeapon]; }

   inline edict_t *GetEntity (void) { return pev->pContainingEntity; };
   int GetIndex (void);

   inline Vector Center (void) { return (pev->absmax + pev->absmin) * 0.5; };
   inline Vector EyePosition (void) { return pev->origin + pev->view_ofs; };

   void Think (void);
   void NewRound (void);
   void EquipInBuyzone (int buyCount);
   void PushMessageQueue (int message);
   void PrepareChatMessage (char *text);
   bool FindWaypoint (void);
   bool EntityIsVisible (const Vector &dest, bool fromBody = false);

   void SwitchChatterIcon (bool show);
   void DeleteSearchNodes (void);

   void RemoveCertainTask (TaskId_t id);
   void StartTask (TaskId_t id, float desire, int data, float time, bool canContinue);

   void ResetTasks (void);
   TaskItem *GetTask (void);
   inline TaskId_t GetTaskId (void) { return GetTask ()->id; };

   void TakeDamage (edict_t *inflictor, int damage, int armor, int bits);
   void TakeBlinded (const Vector &fade, int alpha);

   void DiscardWeaponForUser (edict_t *user, bool discardC4);

   void SayText (const char *text);
   void TeamSayText (const char *text);

   void ChatMessage (int type, bool isTeamSay = false);
   void RadioMessage (int message);
   void ChatterMessage (int message);
   void HandleChatterMessage (const char *sz);
   void TryHeadTowardRadioEntity (void);

   void Kill (void);
   void Kick (void);
   void ResetDoubleJumpState (void);
   void MoveToVector (Vector to);
   int FindPlantedBomb(void);

   bool HasHostage (void);
   bool UsesRifle (void);
   bool UsesPistol (void);
   bool UsesSniper (void);
   bool UsesSubmachineGun (void);
   bool UsesZoomableRifle (void);
   bool UsesBadPrimary (void);
   bool UsesCampGun (void);
   bool HasPrimaryWeapon (void);
   bool HasSecondaryWeapon(void);
   bool HasShield (void);
   bool IsShieldDrawn (void);

   void ReleaseUsedName (void);
};

// manager class
class BotManager : public Singleton <BotManager>
{
private:
   Array <CreateQueue> m_creationTab; // bot creation tab

   Bot *m_bots[32]; // all available bots
   float m_maintainTime; // time to maintain bot creation quota

   int m_lastWinner; // the team who won previous round
   int m_roundCount; // rounds passed

   bool m_economicsGood[2]; // is team able to buy anything
   bool m_deathMsgSent; // for fakeping

protected:
   int CreateBot (String name, int difficulty, int personality, int team, int member);

public:
   BotManager (void);
  ~BotManager (void);

   bool EconomicsValid (int team) { return m_economicsGood[team]; }

   int GetLastWinner (void) const { return m_lastWinner; }
   void SetLastWinner (int winner) { m_lastWinner = winner; }

   int GetIndex (edict_t *ent);
   Bot *GetBot (int index);
   Bot *GetBot (edict_t *ent);
   Bot *FindOneValidAliveBot (void);
   Bot *GetHighestFragsBot (int team);

   int GetHumansNum (void);
   int GetHumansAliveNum(void);
   int GetHumansJoinedTeam (void);
   int GetBotsNum (void);

   void Think (void);
   void Free (void);
   void Free (int index);
   void CheckAutoVacate (edict_t *ent);

   void AddRandom (void) { AddBot ("", -1, -1, -1, -1); }
   void AddBot (const String &name, int difficulty, int personality, int team, int member);
   void AddBot (String name, String difficulty, String personality, String team, String member);
   void FillServer (int selection, int personality = PERSONALITY_NORMAL, int difficulty = -1, int numToAdd = -1);

   void RemoveAll (bool zeroQuota = true);
   void RemoveRandom (void);
   void RemoveFromTeam (Team team, bool removeAll = false);
   void RemoveMenu (edict_t *ent, int selection);
   void KillAll (int team = -1);
   void MaintainBotQuota (void);
   void InitQuota (void);

   void ListBots (void);
   void SetWeaponMode (int selection);
   void CheckTeamEconomics (int team);

   static void CallGameEntity (entvars_t *vars);

   inline void SetDeathMsgState (bool sent) { m_deathMsgSent = sent; }
   inline bool GetDeathMsgState (void) { return m_deathMsgSent; }

public:
   void CalculatePingOffsets (void);
   void SendPingDataOffsets (edict_t *to);
   void SendDeathMsgFix (void);
};

// texts localizer
class Localizer : public Singleton <Localizer>
{
public:
   Array <LanguageItem> m_langTab;

public:
   Localizer (void) { m_langTab.RemoveAll (); }
  ~Localizer (void) { m_langTab.RemoveAll (); }

   char *TranslateInput (const char *input);
};

// netmessage handler class
class NetworkMsg : public Singleton <NetworkMsg>
{
private:
   Bot *m_bot;
   int m_state;
   int m_message;
   int m_registerdMessages[NETMSG_BOTVOICE + 1];

public:
   NetworkMsg (void);
  ~NetworkMsg   (void) { };

   void Execute (void *p);
   inline void Reset (void) { m_message = NETMSG_UNDEFINED; m_state = 0; m_bot = NULL; };
   void HandleMessageIfRequired (int messageType, int requiredType);

   inline void SetMessage (int message) { m_message = message; }
   inline void SetBot (Bot *bot) { m_bot = bot; }

   inline int GetId (int messageType) { return m_registerdMessages[messageType]; }
   inline void SetId (int messageType, int messsageIdentifier) { m_registerdMessages[messageType] = messsageIdentifier; }
};

// waypoint operation class
class Waypoint : public Singleton <Waypoint>
{
   friend class Bot;

private:
   Path *m_paths[MAX_WAYPOINTS];

   bool m_waypointPaths;
   bool m_isOnLadder;
   bool m_endJumpPoint;
   bool m_learnJumpWaypoint;
   float m_timeJumpStarted;

   Vector m_learnVelocity;
   Vector m_learnPosition;
   Vector m_foundBombOrigin;

   int m_cacheWaypointIndex;
   int m_lastJumpWaypoint;
   int m_visibilityIndex;
   Vector m_lastWaypoint;
   byte m_visLUT[MAX_WAYPOINTS][MAX_WAYPOINTS / 4];

   float m_pathDisplayTime;
   float m_arrowDisplayTime;
   float m_waypointDisplayTime[MAX_WAYPOINTS];
   float m_goalsScore[MAX_WAYPOINTS];
   int m_findWPIndex;
   int m_facingAtIndex;
   char m_infoBuffer[256];

   int *m_distMatrix;
   int *m_pathMatrix;

   Array <int> m_terrorPoints;
   Array <int> m_ctPoints;
   Array <int> m_goalPoints;
   Array <int> m_campPoints;
   Array <int> m_sniperPoints;
   Array <int> m_rescuePoints;
   Array <int> m_visitedGoals;

public:
   bool m_redoneVisibility;

   Waypoint (void);
  ~Waypoint (void);

   void Init (void);
   void InitExperienceTab (void);
   void InitVisibilityTab (void);

   void InitTypes (void);
   void AddPath (short int addIndex,  short int pathIndex, float distance);

   int GetFacingIndex (void);
   int FindFarest (Vector origin, float maxDistance = 32.0);
   int FindNearest (Vector origin, float minDistance = 9999.0, int flags = -1);
   void FindInRadius (Vector origin, float radius, int *holdTab, int *count);
   void FindInRadius (Array <int> &queueID, float radius, Vector origin);

   void Add (int flags, const Vector &waypointOrigin = nullvec);
   void Delete (void);
   void ToggleFlags (int toggleFlag);
   void SetRadius (int radius);
   bool IsConnected (int pointA, int pointB);
   bool IsConnected (int num);
   void InitializeVisibility (void);
   void CreatePath (char dir);
   void DeletePath (void);
   void CacheWaypoint (void);

   float GetTravelTime (float maxSpeed, Vector src, Vector origin);
   bool IsVisible (int srcIndex, int destIndex);
   bool IsStandVisible (int srcIndex, int destIndex);
   bool IsDuckVisible (int srcIndex, int destIndex);
   void CalculateWayzone (int index);

   bool Load (void);
   void Save (void);

   bool Reachable (Bot *bot, int index);
   bool IsNodeReachable (Vector src, Vector destination);
   void Think (void);
   bool NodesValid (void);
   void SaveExperienceTab (void);
   void SaveVisibilityTab (void);
   void CreateBasic (void);
   void EraseFromHardDisk (void);

   void InitPathMatrix (void);
   void SavePathMatrix (void);
   bool LoadPathMatrix (void);

   int GetPathDistance (int srcIndex, int destIndex);
   Path *GetPath (int id);
   char *GetWaypointInfo (int id);
   char *GetInfo (void) { return m_infoBuffer; }

   int AddGoalScore (int index, int other[4]);
   void SetFindIndex (int index);
   void SetLearnJumpWaypoint (void);
   void ClearGoalScore (void);

   bool IsGoalVisited (int index);
   void SetGoalVisited (int index);

   inline Vector GetBombPosition (void) { return m_foundBombOrigin; }
   void SetBombPosition (bool shouldReset = false);
   String CheckSubfolderFile (void);
};

// wayponit auto-downloader
enum WaypointDownloadError
{
   WDE_SOCKET_ERROR,
   WDE_CONNECT_ERROR,
   WDE_NOTFOUND_ERROR,
   WDE_NOERROR
};

class WaypointDownloader
{
public:

   // free's socket handle
   void FreeSocket (int sock);

   // do actually downloading of waypoint file
   WaypointDownloadError DoDownload (void);
};

enum VarType
{
   VT_NORMAL = 0,
   VT_READONLY,
   VT_PASSWORD,
   VT_NOSERVER,
   VT_NOREGISTER
};

class ConVarWrapper : public Singleton <ConVarWrapper>
{
private:
   struct VarPair
   {
      VarType type;
      cvar_t reg;
      class ConVar *self;
   } m_regVars[101];
   
   int m_regCount;

public:
   void RegisterVariable (const char *variable, const char *value, VarType varType, ConVar *self);
   void PushRegisteredConVarsToEngine (bool gameVars = false);
};

#define g_netMsg NetworkMsg::GetObject ()
#define g_botManager BotManager::GetObject ()
#define g_localizer Localizer::GetObject ()
#define g_convarWrapper ConVarWrapper::GetObject ()
#define g_waypoint Waypoint::GetObject ()

// simplify access for console variables
class ConVar
{
public:
   cvar_t *m_eptr;

public:
   ConVar (const char *name, const char *initval, VarType type = VT_NORMAL)
   {
      g_convarWrapper->RegisterVariable (name, initval, type, this);
   }

   inline bool GetBool(void)
   {
      return m_eptr->value > 0.0f;
   }

   inline int GetInt (void)
   {
      return static_cast <int> (m_eptr->value);
   }

   inline int GetFlags (void)
   {
      return m_eptr->flags;
   }

   inline float GetFloat (void)
   {
      return m_eptr->value;
   }

   inline const char *GetString (void)
   {
      return m_eptr->string;
   }

   inline const char *GetName (void)
   {
      return m_eptr->name;
   }

   inline void SetFloat (float val)
   {
      g_engfuncs.pfnCVarSetFloat (m_eptr->name, val);
   }

   inline void SetInt (int val)
   {
      SetFloat (static_cast <float> (val));
   }

   inline void SetString (const char *val)
   {
      g_engfuncs.pfnCVarSetString (m_eptr->name, val);
   }
};

// prototypes of bot functions...
extern int GetWeaponReturn (bool isString, const char *weaponAlias, int weaponIndex = -1);
extern int GetTeam (edict_t *ent);

extern float GetShootingConeDeviation (edict_t *ent, Vector *position);
extern float GetWaveLength (const char *fileName);

extern bool TryFileOpen (const char *fileName);
extern bool IsDedicatedServer (void);
extern bool IsVisible (const Vector &origin, edict_t *ent);
extern bool IsAlive (edict_t *ent);
extern bool IsInViewCone (Vector origin, edict_t *ent);
extern int GetWeaponPenetrationPower (int id);
extern bool IsValidBot (edict_t *ent);
extern bool IsValidPlayer (edict_t *ent);
extern bool OpenConfig (const char *fileName, char *errorIfNotExists, File *outFile, bool languageDependant = false);
extern bool FindNearestPlayer (void **holder, edict_t *to, float searchDistance = 4096.0, bool sameTeam = false, bool needBot = false, bool needAlive = false, bool needDrawn = false);

extern const char *GetMapName (void);
extern const char *GetWaypointDir (void);
extern const char *GetModName (void);
extern const char *GetField (const char *string, int fieldId, bool endLine = false);
extern const char *FormatBuffer (const char *format, ...);

extern uint16 GenerateBuildNumber (void);
extern Vector GetEntityOrigin (edict_t *ent);

extern void FreeLibraryMemory (void);
extern void RoundInit (void);
extern void FakeClientCommand (edict_t *fakeClient, const char *format, ...);
extern void strtrim (char *string);
extern void CreatePath (char *path);
extern void ServerCommand (const char *format, ...);
extern void RegisterCommand (char *command, void funcPtr (void));
extern void CheckWelcomeMessage (void);
extern void DetectCSVersion (void);
extern void PlaySound (edict_t *ent, const char *soundName);
extern void ServerPrint (const char *format, ...);
extern void ChartPrint (const char *format, ...);
extern void ServerPrintNoTag (const char *format, ...);
extern void CenterPrint (const char *format, ...);
extern void ClientPrint (edict_t *ent, int dest, const char *format, ...);
extern void HudMessage (edict_t *ent, bool toCenter, Vector rgb, char *format, ...);

extern void AddLogEntry (bool outputToConsole, int logLevel, const char *format, ...);
extern void TraceLine (const Vector &start, const Vector &end, bool ignoreMonsters, bool ignoreGlass, edict_t *ignoreEntity, TraceResult *ptr);
extern void TraceLine (const Vector &start, const Vector &end, bool ignoreMonsters, edict_t *ignoreEntity, TraceResult *ptr);
extern void TraceHull (const Vector &start, const Vector &end, bool ignoreMonsters, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr);
extern void DrawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life);
extern void DrawArrow (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life);
extern void DisplayMenuToClient (edict_t *ent, MenuText *menu);
extern void DecalTrace (entvars_t *pev, TraceResult *trace, int logotypeIndex);
extern void SoundAttachToThreat (edict_t *ent, const char *sample, float volume);
extern void SoundSimulateUpdate (int playerIndex);

// very global convars
extern ConVar yb_jasonmode;
extern ConVar yb_communication_type;
extern ConVar yb_csdm_mode;
extern ConVar yb_ignore_enemies;

#include <globals.h>
#include <compress.h>
#include <resource.h>

inline int Bot::GetIndex (void)
{
   return IndexOfEntity (GetEntity ());
}