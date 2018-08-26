//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <core.h>

bool g_canSayBombPlanted = true;
bool g_roundEnded = true;
bool g_botsCanPause = false;
bool g_bombPlanted = false;
bool g_bombSayString = false;
bool g_gameWelcomeSent = false;

bool g_editNoclip = false;
bool g_waypointOn = false;
bool g_autoWaypoint = false;

float g_lastChatTime = 0.0f;
float g_timeRoundStart = 0.0f;
float g_timeRoundEnd = 0.0f;
float g_timeRoundMid = 0.0f;
float g_timeNextBombUpdate = 0.0f;
float g_timeBombPlanted = 0.0f;
float g_timePerSecondUpdate = 0.0f;
float g_lastRadioTime[MAX_TEAM_COUNT] = { 0.0f, };
float g_autoPathDistance = 250.0f;

int g_lastRadio[MAX_TEAM_COUNT];
int g_storeAddbotVars[4];
int g_radioSelect[MAX_ENGINE_PLAYERS];
int g_gameFlags = 0;
int g_numWaypoints = 0;
int g_mapType = 0;

int g_highestDamageCT = 1;
int g_highestDamageT = 1;
int g_highestKills = 1;

Array<StringArray> g_chatFactory;
Array<Array<ChatterItem>> g_chatterFactory;
Array<BotName> g_botNames;
Array<KeywordFactory> g_replyFactory;
RandomSequenceOfUnique rng;
Library *g_gameLib = nullptr;

meta_globals_t *gpMetaGlobals = nullptr;
gamedll_funcs_t *gpGamedllFuncs = nullptr;
mutil_funcs_t *gpMetaUtilFuncs = nullptr;

gamefuncs_t g_functionTable;

enginefuncs_t g_engfuncs;
Client g_clients[MAX_ENGINE_PLAYERS];
WeaponProperty g_weaponDefs[MAX_WEAPONS + 1];

edict_t *g_hostEntity = nullptr;
globalvars_t *g_pGlobals = nullptr;
Experience *g_experienceData = nullptr;

// default tables for personality weapon preferences, overridden by weapons.cfg
int g_normalWeaponPrefs[NUM_WEAPONS] = {0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 18, 21, 17, 19, 15, 17, 9, 14, 16};
int g_rusherWeaponPrefs[NUM_WEAPONS] = {0, 2, 1, 4, 5, 6, 3, 24, 19, 22, 23, 20, 21, 10, 12, 13, 7, 8, 11, 9, 18, 17, 19, 25, 15, 16};
int g_carefulWeaponPrefs[NUM_WEAPONS] = {0, 2, 1, 4, 25, 6, 3, 7, 8, 12, 10, 13, 11, 9, 24, 18, 14, 17, 16, 15, 19, 20, 21, 22, 23, 5};
int g_grenadeBuyPrecent[NUM_WEAPONS - 23] = {95, 85, 60};
int g_botBuyEconomyTable[NUM_WEAPONS - 15] = {1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000};
int *g_weaponPrefs[] = {g_normalWeaponPrefs, g_rusherWeaponPrefs, g_carefulWeaponPrefs};

// metamod plugin information
plugin_info_t Plugin_info = {
   META_INTERFACE_VERSION, // interface version
   PRODUCT_NAME, // plugin name
   PRODUCT_VERSION, // plugin version
   PRODUCT_DATE, // date of creation
   PRODUCT_AUTHOR, // plugin author
   PRODUCT_URL, // plugin URL
   PRODUCT_LOGTAG, // plugin logtag
   PT_CHANGELEVEL, // when loadable
   PT_ANYTIME, // when unloadable
};

// table with all available actions for the bots (filtered in & out in Bot::SetConditions) some of them have subactions included
Task g_taskFilters[] = {
   { TASK_NORMAL, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_PAUSE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_MOVETOPOSITION, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_FOLLOWUSER, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_WAITFORGO, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_PICKUPITEM, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_CAMP, 0, INVALID_WAYPOINT_INDEX, 0.0f, true },
   { TASK_PLANTBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_DEFUSEBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_ATTACK, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_HUNTENEMY, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_SEEKCOVER, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_THROWHEGRENADE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_THROWFLASHBANG, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_THROWSMOKE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_DOUBLEJUMP, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_ESCAPEFROMBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_SHOOTBREAKABLE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_HIDE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_BLINDED, 0, INVALID_WAYPOINT_INDEX, 0.0f, false },
   { TASK_SPRAY, 0, INVALID_WAYPOINT_INDEX, 0.0f, false }
};

// weapons and their specifications
WeaponSelect g_weaponSelect[NUM_WEAPONS + 1] = {
   { WEAPON_KNIFE,      "weapon_knife",     "knife.mdl",     0,    0, -1, -1,  0,  0,  0,  0,  0, true },
   { WEAPON_USP,        "weapon_usp",       "usp.mdl",       500,  1, -1, -1,  1,  1,  2,  2,  0, false },
   { WEAPON_GLOCK,      "weapon_glock18",   "glock18.mdl",   400,  1, -1, -1,  1,  2,  1,  1,  0, false },
   { WEAPON_DEAGLE,     "weapon_deagle",    "deagle.mdl",    650,  1,  2,  2,  1,  3,  4,  4,  2,  false },
   { WEAPON_P228,       "weapon_p228",      "p228.mdl",      600,  1,  2,  2,  1,  4,  3,  3,  0, false },
   { WEAPON_ELITE,      "weapon_elite",     "elite.mdl",     1000, 1,  0,  0,  1,  5,  5,  5,  0, false },
   { WEAPON_FIVESEVEN,  "weapon_fiveseven", "fiveseven.mdl", 750,  1,  1,  1,  1,  6,  5,  5,  0, false },
   { WEAPON_M3,         "weapon_m3",        "m3.mdl",        1700, 1,  2, -1,  2,  1,  1,  1,  0, false },
   { WEAPON_XM1014,     "weapon_xm1014",    "xm1014.mdl",    3000, 1,  2, -1,  2,  2,  2,  2,  0, false },
   { WEAPON_MP5,        "weapon_mp5navy",   "mp5.mdl",       1500, 1,  2,  1,  3,  1,  2,  2,  0, true },
   { WEAPON_TMP,        "weapon_tmp",       "tmp.mdl",       1250, 1,  1,  1,  3,  2,  1,  1,  0, true },
   { WEAPON_P90,        "weapon_p90",       "p90.mdl",       2350, 1,  2,  1,  3,  3,  4,  4,  0, true },
   { WEAPON_MAC10,      "weapon_mac10",     "mac10.mdl",     1400, 1,  0,  0,  3,  4,  1,  1,  0, true },
   { WEAPON_UMP45,      "weapon_ump45",     "ump45.mdl",     1700, 1,  2,  2,  3,  5,  3,  3,  0, true },
   { WEAPON_AK47,       "weapon_ak47",      "ak47.mdl",      2500, 1,  0,  0,  4,  1,  2,  2,  2,  true },
   { WEAPON_SG552,      "weapon_sg552",     "sg552.mdl",     3500, 1,  0, -1,  4,  2,  4,  4,  2,  true },
   { WEAPON_M4A1,       "weapon_m4a1",      "m4a1.mdl",      3100, 1,  1,  1,  4,  3,  3,  3,  2,  true },
   { WEAPON_GALIL,      "weapon_galil",     "galil.mdl",     2000, 1,  0,  0,  4,  -1, 1,  1,  2,  true },
   { WEAPON_FAMAS,      "weapon_famas",     "famas.mdl",     2250, 1,  1,  1,  4,  -1, 1,  1,  2,  true },
   { WEAPON_AUG,        "weapon_aug",       "aug.mdl",       3500, 1,  1,  1,  4,  4,  4,  4,  2,  true },
   { WEAPON_SCOUT,      "weapon_scout",     "scout.mdl",     2750, 1,  2,  0,  4,  5,  3,  2,  3,  false },
   { WEAPON_AWP,        "weapon_awp",       "awp.mdl",       4750, 1,  2,  0,  4,  6,  5,  6,  3,  false },
   { WEAPON_G3SG1,      "weapon_g3sg1",     "g3sg1.mdl",     5000, 1,  0,  2,  4,  7,  6,  6,  3,  false },
   { WEAPON_SG550,      "weapon_sg550",     "sg550.mdl",     4200, 1,  1,  1,  4,  8,  5,  5,  3,  false },
   { WEAPON_M249,       "weapon_m249",      "m249.mdl",      5750, 1,  2,  1,  5,  1,  1,  1,  2,  true },
   { WEAPON_SHIELD,     "weapon_shield",    "shield.mdl",    2200, 0,  1,  1,  8,  -1, 8,  8,  0, false },
   { 0,                 "",                 "",              0,    0,  0,  0,  0,   0, 0,  0,  0, false }
};

void setupBotMenus (void) {
   int counter = 0;

   // bots main menu
   g_menus[counter] = {
      BOT_MENU_MAIN, 0x2ff,
      "\\yMain Menu\\w\n\n"
      "1. Control Bots\n"
      "2. Features\n\n"
      "3. Fill Server\n"
      "4. End Round\n\n"
      "0. Exit"
   };

   // bots features menu
   g_menus[++counter] = {
      BOT_MENU_FEATURES, 0x25f,
      "\\yBots Features\\w\n\n"
      "1. Weapon Mode Menu\n"
      "2. Waypoint Menu\n"
      "3. Select Personality\n\n"
      "4. Toggle Debug Mode\n"
      "5. Command Menu\n\n"
      "0. Exit"
   };

   // bot control menu
   g_menus[++counter] = {
      BOT_MENU_CONTROL, 0x2ff,
      "\\yBots Control Menu\\w\n\n"
      "1. Add a Bot, Quick\n"
      "2. Add a Bot, Specified\n\n"
      "3. Remove Random Bot\n"
      "4. Remove All Bots\n\n"
      "5. Remove Bot Menu\n\n"
      "0. Exit"
   };

   // weapon mode select menu
   g_menus[++counter] = {
      BOT_MENU_WEAPON_MODE, 0x27f,
      "\\yBots Weapon Mode\\w\n\n"
      "1. Knives only\n"
      "2. Pistols only\n"
      "3. Shotguns only\n"
      "4. Machine Guns only\n"
      "5. Rifles only\n"
      "6. Sniper Weapons only\n"
      "7. All Weapons\n\n"
      "0. Exit"
   };

   // personality select menu
   g_menus[++counter] = {
      BOT_MENU_PERSONALITY, 0x20f,
      "\\yBots Personality\\w\n\n"
      "1. Random\n"
      "2. Normal\n"
      "3. Aggressive\n"
      "4. Careful\n\n"
      "0. Exit"
   };

   // difficulty select menu
   g_menus[++counter] = {
      BOT_MENU_DIFFICULTY, 0x23f,
      "\\yBots Difficulty Level\\w\n\n"
      "1. Newbie\n"
      "2. Average\n"
      "3. Normal\n"
      "4. Professional\n"
      "5. Godlike\n\n"
      "0. Exit"
   };

   // team select menu
   g_menus[++counter] = {
      BOT_MENU_TEAM_SELECT, 0x213,
      "\\ySelect a team\\w\n\n"
      "1. Terrorist Force\n"
      "2. Counter-Terrorist Force\n\n"
      "5. Auto-select\n\n"
      "0. Exit"
   };

   // terrorist model select menu
   g_menus[++counter] = {
      BOT_MENU_TERRORIST_SELECT, 0x21f,
      "\\ySelect an appearance\\w\n\n"
      "1. Phoenix Connexion\n"
      "2. L337 Krew\n"
      "3. Arctic Avengers\n"
      "4. Guerilla Warfare\n\n"
      "5. Auto-select\n\n"
      "0. Exit"
   };

   // counter-terrorist model select menu
   g_menus[++counter] = {
      BOT_MENU_CT_SELECT, 0x21f,
      "\\ySelect an appearance\\w\n\n"
      "1. Seal Team 6 (DEVGRU)\n"
      "2. German GSG-9\n"
      "3. UK SAS\n"
      "4. French GIGN\n\n"
      "5. Auto-select\n\n"
      "0. Exit"
   };

   // command menu
   g_menus[++counter] = {
      BOT_MENU_COMMANDS, 0x23fu,
      "\\yBot Command Menu\\w\n\n"
      "1. Make Double Jump\n"
      "2. Finish Double Jump\n\n"
      "3. Drop the C4 Bomb\n"
      "4. Drop the Weapon\n\n"
      "0. Exit"
   };

   // main waypoint menu
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_MAIN_PAGE1, 0x3ff,
      "\\yWaypoint Operations (Page 1)\\w\n\n"
      "1. Show/Hide waypoints\n"
      "2. Cache waypoint\n"
      "3. Create path\n"
      "4. Delete path\n"
      "5. Add waypoint\n"
      "6. Delete waypoint\n"
      "7. Set Autopath Distance\n"
      "8. Set Radius\n\n"
      "9. Next...\n\n"
      "0. Exit"
   };

   // main waypoint menu (page 2)
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_MAIN_PAGE2, 0x3ff,
      "\\yWaypoint Operations (Page 2)\\w\n\n"
      "1. Waypoint stats\n"
      "2. Autowaypoint on/off\n"
      "3. Set flags\n"
      "4. Save waypoints\n"
      "5. Save without checking\n"
      "6. Load waypoints\n"
      "7. Check waypoints\n"
      "8. Noclip cheat on/off\n\n"
      "9. Previous...\n\n"
      "0. Exit"
   };

   // select waypoint radius menu
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_RADIUS, 0x3ff,
      "\\yWaypoint Radius\\w\n\n"
      "1. SetRadius 0\n"
      "2. SetRadius 8\n"
      "3. SetRadius 16\n"
      "4. SetRadius 32\n"
      "5. SetRadius 48\n"
      "6. SetRadius 64\n"
      "7. SetRadius 80\n"
      "8. SetRadius 96\n"
      "9. SetRadius 128\n\n"
      "0. Exit"
   };

   // waypoint add menu
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_TYPE, 0x3ff,
      "\\yWaypoint Type\\w\n\n"
      "1. Normal\n"
      "\\r2. Terrorist Important\n"
      "3. Counter-Terrorist Important\n"
      "\\w4. Block with hostage / Ladder\n"
      "\\y5. Rescue Zone\n"
      "\\w6. Camping\n"
      "7. Camp End\n"
      "\\r8. Map Goal\n"
      "\\w9. Jump\n\n"
      "0. Exit"
   };

   // set waypoint flag menu
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_FLAG, 0x2ff,
      "\\yToggle Waypoint Flags\\w\n\n"
      "1. Block with Hostage\n"
      "2. Terrorists Specific\n"
      "3. CTs Specific\n"
      "4. Use Elevator\n"
      "5. Sniper Point (\\yFor Camp Points Only!\\w)\n\n"
      "0. Exit"
   };

   // auto-path max distance
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_AUTOPATH,
      0x27f,
      "\\yAutoPath Distance\\w\n\n"
      "1. Distance 0\n"
      "2. Distance 100\n"
      "3. Distance 130\n"
      "4. Distance 160\n"
      "5. Distance 190\n"
      "6. Distance 220\n"
      "7. Distance 250 (Default)\n\n"
      "0. Exit"
   };

   // path connections
   g_menus[++counter] = {
      BOT_MENU_WAYPOINT_PATH,
      0x207,
      "\\yCreate Path (Choose Direction)\\w\n\n"
      "1. Outgoing Path\n"
      "2. Incoming Path\n"
      "3. Bidirectional (Both Ways)\n\n"
      "0. Exit"
   };

   const String &empty = "";

   // kick menus
   g_menus[++counter] = { BOT_MENU_KICK_PAGE_1, 0x0, empty, };
   g_menus[++counter] = { BOT_MENU_KICK_PAGE_2, 0x0, empty, };
   g_menus[++counter] = { BOT_MENU_KICK_PAGE_3, 0x0, empty, };
   g_menus[++counter] = { BOT_MENU_KICK_PAGE_4, 0x0, empty, };
}

// bot menus
MenuText g_menus[BOT_MENU_TOTAL_MENUS];