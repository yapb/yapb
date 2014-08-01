//
// Copyright (c) 2014, by YaPB Development Team. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Version: $Id:$
//

#include <core.h>

bool g_canSayBombPlanted = true;
bool g_isMetamod = false;
bool g_radioInsteadVoice = false;
bool g_roundEnded = true;
bool g_botsCanPause = false;
bool g_sendAudioFinished = true;
bool g_bombPlanted = false;
bool g_bombSayString = false;
bool g_isCommencing = false;
bool g_editNoclip = false;
bool g_isFakeCommand = false;
bool g_waypointOn = false;
bool g_waypointsChanged = true;
bool g_autoWaypoint = false;
bool g_bLearnJumpWaypoint = false;
bool g_leaderChoosen[2] = {false, false};

float g_lastChatTime = 0.0;
float g_timeRoundStart = 0.0;
float g_timeRoundEnd = 0.0;
float g_timeRoundMid = 0.0;
float g_timeNextBombUpdate = 0.0;
float g_timeBombPlanted = 0.0;
float g_lastRadioTime[2] = {0.0, 0.0};
float g_autoPathDistance = 250.0;

int g_lastRadio[2];
int g_storeAddbotVars[4];
int g_radioSelect[32];
int g_fakeArgc = 0;
int g_gameVersion = CSV_STEAM;
int g_numWaypoints = 0;
int g_mapType = 0;
int g_killHistory = 0;

short g_modelIndexLaser = 0;
short g_modelIndexArrow = 0;
char g_fakeArgv[256];

Array <Array <String> > g_chatFactory;
Array <Array <ChatterItem> > g_chatterFactory;
Array <BotName> g_botNames;
Array <KeywordFactory> g_replyFactory;
RandGen g_randGen;
Library *g_gameLib = NULL;

meta_globals_t *gpMetaGlobals = NULL;
gamedll_funcs_t *gpGamedllFuncs = NULL;
mutil_funcs_t *gpMetaUtilFuncs = NULL;

gamefuncs_t g_functionTable;
EntityAPI_t g_entityAPI = NULL;
NewEntityAPI_t g_getNewEntityAPI = NULL;
BlendAPI_t g_serverBlendingAPI = NULL;
FuncPointers_t g_funcPointers = NULL;

enginefuncs_t g_engfuncs;
Client g_clients[32];
WeaponProperty g_weaponDefs[MAX_WEAPONS + 1];

edict_t *g_worldEdict = NULL;
edict_t *g_hostEntity = NULL;
globalvars_t *g_pGlobals = NULL;
Experience *g_experienceData = NULL;



// default tables for personality weapon preferences, overridden by weapons.cfg
int g_normalWeaponPrefs[NUM_WEAPONS] =
   {0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 18, 21, 17, 19, 15, 17, 9, 14, 16};

int g_rusherWeaponPrefs[NUM_WEAPONS] =
   {0, 2, 1, 4, 5, 6, 3, 24, 19, 22, 23, 20, 21, 10, 12, 13, 7, 8, 11, 9, 18, 17, 19, 25, 15, 16};

int g_carefulWeaponPrefs[NUM_WEAPONS] =
   {0, 2, 1, 4, 25, 6, 3, 7, 8, 12, 10, 13, 11, 9, 24, 18, 14, 17, 16, 15, 19, 20, 21, 22, 23, 5};

int g_grenadeBuyPrecent[NUM_WEAPONS - 23] =
   {95, 85, 60};

int g_botBuyEconomyTable[NUM_WEAPONS - 15] =
   {1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000};

SkillDefinition g_skillTab[6] =
{
   {0.8, 1.0, 45.0, 65.0, 2.0, 3.0, 40.0, 40.0, 50.0,   0,   0,   0, 50},
   {0.6, 0.8, 40.0, 60.0, 3.0, 4.0, 30.0, 30.0, 42.0,  10,   0,   0, 40},
   {0.4, 0.6, 35.0, 55.0, 4.0, 6.0, 20.0, 20.0, 32.0,  30,   0,  50, 35},
   {0.2, 0.3, 30.0, 50.0, 6.0, 8.0, 10.0, 10.0, 18.0,   0,  30,  80, 30},
   {0.1, 0.2, 25.0, 40.0, 8.0, 10.0, 5.0,  5.0, 10.0,  80,  50, 100, 23},
   {0.0, 0.1, 20.0, 30.0, 9.0, 12.0, 0.0,  5.0,  0.0, 100, 100, 100, 20}
};

int *g_weaponPrefs[] =
{
   g_normalWeaponPrefs,
   g_rusherWeaponPrefs,
   g_carefulWeaponPrefs
};

// metamod engine & dllapi function tables
metamod_funcs_t gMetaFunctionTable =
{
   NULL, // pfnEntityAPI_t ()
   NULL, // pfnEntityAPI_t_Post ()
   GetEntityAPI2, // pfnEntityAPI_t2 ()
   GetEntityAPI2_Post, // pfnEntityAPI_t2_Post ()
   NULL, // pfnGetNewDLLFunctions ()
   NULL, // pfnGetNewDLLFunctions_Post ()
   GetEngineFunctions, // pfnGetEngineFunctions ()
   GetEngineFunctions_Post, // pfnGetEngineFunctions_Post ()
};

// metamod plugin information
plugin_info_t Plugin_info =
{
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
TaskItem g_taskFilters[] =
{
   {TASK_NORMAL, 0, -1, 0.0, true},
   {TASK_PAUSE, 0, -1, 0.0, false},
   {TASK_MOVETOPOSITION, 0, -1, 0.0, true},
   {TASK_FOLLOWUSER, 0, -1,0.0, true},
   {TASK_WAITFORGO, 0, -1, 0.0, true},
   {TASK_PICKUPITEM, 0, -1, 0.0, true},
   {TASK_CAMP, 0, -1, 0.0, true},
   {TASK_PLANTBOMB, 0, -1, 0.0, false},
   {TASK_DEFUSEBOMB, 0, -1, 0.0, false},
   {TASK_ATTACK, 0, -1, 0.0, false},
   {TASK_HUNTENEMY, 0, -1, 0.0, false},
   {TASK_SEEKCOVER, 0, -1, 0.0, false},
   {TASK_THROWHEGRENADE, 0, -1, 0.0, false},
   {TASK_THROWFLASHBANG, 0, -1, 0.0, false},
   {TASK_THROWSMOKE, 0, -1, 0.0, false},
   {TASK_DOUBLEJUMP, 0, -1, 0.0, false},
   {TASK_ESCAPEFROMBOMB, 0, -1, 0.0, false},
   {TASK_SHOOTBREAKABLE, 0, -1, 0.0, false},
   {TASK_HIDE, 0, -1, 0.0, false},
   {TASK_BLINDED, 0, -1, 0.0, false},
   {TASK_SPRAY, 0, -1, 0.0, false}
};

// weapons and their specifications
WeaponSelect g_weaponSelect[NUM_WEAPONS + 1] =
{
   {WEAPON_KNIFE,      "weapon_knife",     "knife.mdl",     0,    0, -1, -1,  0,  0,  0,  0,  false, true },
   {WEAPON_USP,        "weapon_usp",       "usp.mdl",       500,  1, -1, -1,  1,  1,  2,  2,  false, false},
   {WEAPON_GLOCK,      "weapon_glock18",   "glock18.mdl",   400,  1, -1, -1,  1,  2,  1,  1,  false, false},
   {WEAPON_DEAGLE,     "weapon_deagle",    "deagle.mdl",    650,  1,  2,  2,  1,  3,  4,  4,  true,  false},
   {WEAPON_P228,       "weapon_p228",      "p228.mdl",      600,  1,  2,  2,  1,  4,  3,  3,  false, false},
   {WEAPON_ELITE,      "weapon_elite",     "elite.mdl",     1000, 1,  0,  0,  1,  5,  5,  5,  false, false},
   {WEAPON_FIVESEVEN,  "weapon_fiveseven", "fiveseven.mdl", 750,  1,  1,  1,  1,  6,  5,  5,  false, false},
   {WEAPON_M3,         "weapon_m3",        "m3.mdl",        1700, 1,  2, -1,  2,  1,  1,  1,  false, false},
   {WEAPON_XM1014,     "weapon_xm1014",    "xm1014.mdl",    3000, 1,  2, -1,  2,  2,  2,  2,  false, false},
   {WEAPON_MP5,        "weapon_mp5navy",   "mp5.mdl",       1500, 1,  2,  1,  3,  1,  2,  2,  false, true },
   {WEAPON_TMP,        "weapon_tmp",       "tmp.mdl",       1250, 1,  1,  1,  3,  2,  1,  1,  false, true },
   {WEAPON_P90,        "weapon_p90",       "p90.mdl",       2350, 1,  2,  1,  3,  3,  4,  4,  false, true },
   {WEAPON_MAC10,      "weapon_mac10",     "mac10.mdl",     1400, 1,  0,  0,  3,  4,  1,  1,  false, true },
   {WEAPON_UMP45,      "weapon_ump45",     "ump45.mdl",     1700, 1,  2,  2,  3,  5,  3,  3,  false, true },
   {WEAPON_AK47,       "weapon_ak47",      "ak47.mdl",      2500, 1,  0,  0,  4,  1,  2,  2,  true,  true },
   {WEAPON_SG552,      "weapon_sg552",     "sg552.mdl",     3500, 1,  0, -1,  4,  2,  4,  4,  true,  true },
   {WEAPON_M4A1,       "weapon_m4a1",      "m4a1.mdl",      3100, 1,  1,  1,  4,  3,  3,  3,  true,  true },
   {WEAPON_GALIL,      "weapon_galil",     "galil.mdl",     2000, 1,  0,  0,  4,  -1, 1,  1,  true,  true },
   {WEAPON_FAMAS,      "weapon_famas",     "famas.mdl",     2250, 1,  1,  1,  4,  -1, 1,  1,  true,  true },
   {WEAPON_AUG,        "weapon_aug",       "aug.mdl",       3500, 1,  1,  1,  4,  4,  4,  4,  true,  true },
   {WEAPON_SCOUT,      "weapon_scout",     "scout.mdl",     2750, 1,  2,  0,  4,  5,  3,  2,  true,  false},
   {WEAPON_AWP,        "weapon_awp",       "awp.mdl",       4750, 1,  2,  0,  4,  6,  5,  6,  true,  false},
   {WEAPON_G3SG1,      "weapon_g3sg1",     "g3sg1.mdl",     5000, 1,  0,  2,  4,  7,  6,  6,  true,  false},
   {WEAPON_SG550,      "weapon_sg550",     "sg550.mdl",     4200, 1,  1,  1,  4,  8,  5,  5,  true,  false},
   {WEAPON_M249,       "weapon_m249",      "m249.mdl",      5750, 1,  2,  1,  5,  1,  1,  1,  true,  true },
   {WEAPON_SHIELD,     "weapon_shield",    "shield.mdl",    2200, 0,  1,  1,  8,  -1, 8,  8,  false, false},
   {0,                 "",                 "",              0,    0,  0,  0,  0,   0, 0,  0,  false, false}
};

// weapon firing delay based on skill (min and max delay for each weapon)
FireDelay g_fireDelay[NUM_WEAPONS + 1] =
{
   {WEAPON_KNIFE,     255, 256, 0.10, {0.0, 0.2, 0.3, 0.4, 0.6, 0.8}, {0.1, 0.3, 0.5, 0.7, 1.0, 1.2}, 0.0, {0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}},
   {WEAPON_USP,       3,   853, 0.15, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_GLOCK,     5,   853, 0.15, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_DEAGLE,    2,   640, 0.20, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_P228,      4,   853, 0.14, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_ELITE,     3,   640, 0.20, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_FIVESEVEN, 4,   731, 0.14, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_M3,        8,   365, 0.86, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_XM1014,    7,   512, 0.15, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_MP5,       4,   731, 0.10, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_TMP,       3,   731, 0.05, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_P90,       4,   731, 0.10, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_MAC10,     3,   731, 0.06, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_UMP45,     4,   731, 0.15, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_AK47,      2,   512, 0.09, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_SG552,     3,   512, 0.11, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_M4A1,      3,   512, 0.08, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_GALIL,     4,   512, 0.09, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_FAMAS,     4,   512, 0.10, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_AUG,       3,   512, 0.11, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_SCOUT,     10,  256, 0.18, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_AWP,       10,  170, 0.22, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_G3SG1,     4,   256, 0.25, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_SG550,     4,   256, 0.25, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_M249,      3,   640, 0.10, {0.0, 0.1, 0.2, 0.3, 0.4, 0.6}, {0.1, 0.2, 0.3, 0.4, 0.5, 0.7}, 0.2, {0.0, 0.0, 0.1, 0.1, 0.2}, {0.1, 0.1, 0.2, 0.2, 0.4}},
   {WEAPON_SHIELD,    0,   256, 0.00, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0.0, {0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}},
   {0,                0,   256, 0.00, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}, 0.0, {0.0, 0.0, 0.0, 0.0, 0.0}, {0.0, 0.0, 0.0, 0.0, 0.0}}
};


// bot menus
MenuText g_menus[21] =
{
   // main menu
   {
      0x2ff,
      "\\yYaPB Main Menu\\w\v\v"
      "1. YaPB Control\v"
      "2. Features\v\v"
      "3. Fill Server\v"
      "4. End Round\v\v"
      "0. Exit"
   },

   // bot features menu
   {
      0x25f,
      "\\yYaPB Features\\w\v\v"
      "1. Weapon Mode Menu\v"
      "2. Waypoint Menu\v"
      "3. Select Personality\v\v"
      "4. Toggle Debug Mode\v"
      "5. Command Menu\v\v"
      "0. Exit"
   },

   // bot control menu
   {
      0x2ff,
      "\\yYaPB Control Menu\\w\v\v"
      "1. Add a Bot, Quick\v"
      "2. Add a Bot, Specified\v\v"
      "3. Remove Random Bot\v"
      "4. Remove All Bots\v\v"
      "5. Remove Bot Menu\v\v"
      "0. Exit"
   },

   // weapon mode select menu
   {
      0x27f,
      "\\yYaPB Weapon Mode\\w\v\v"
      "1. Knives only\v"
      "2. Pistols only\v"
      "3. Shotguns only\v"
      "4. Machine Guns only\v"
      "5. Rifles only\v"
      "6. Sniper Weapons only\v"
      "7. All Weapons\v\v"
      "0. Exit"
   },

   // personality select menu
   {
      0x20f,
      "\\yYaPB Personality\\w\v\v"
      "1. Random\v"
      "2. Normal\v"
      "3. Aggressive\v"
      "4. Careful\v\v"
      "0. Exit"
   },

   // skill select menu
   {
      0x23f,
      "\\yYaPB Skill Level\\w\v\v"
      "1. Stupid (0-20)\v"
      "2. Newbie (20-40)\v"
      "3. Average (40-60)\v"
      "4. Advanced (60-80)\v"
      "5. Professional (80-99)\v"
      "6. Godlike (100)\v\v"
      "0. Exit"
   },

   // team select menu
   {
      0x213,
      "\\ySelect a team\\w\v\v"
      "1. Terrorist Force\v"
      "2. Counter-Terrorist Force\v\v"
      "5. Auto-select\v\v"
      "0. Exit"
   },

   // terrorist model select menu
   {
      0x21f,
      "\\ySelect an appearance\\w\v\v"
      "1. Phoenix Connexion\v"
      "2. L337 Krew\v"
      "3. Arctic Avengers\v"
      "4. Guerilla Warfare\v\v"
      "5. Auto-select\v\v"
      "0. Exit"
   },

   // counter-terrirust model select menu
   {
      0x21f,
      "\\ySelect an appearance\\w\v\v"
      "1. Seal Team 6 (DEVGRU)\v"
      "2. German GSG-9\v"
      "3. UK SAS\v"
      "4. French GIGN\v\v"
      "5. Auto-select\v\v"
      "0. Exit"
   },

   // main waypoint menu
   {
      0x3ff,
      "\\yWaypoint Operations (Page 1)\\w\v\v"
      "1. Show/Hide waypoints\v"
      "2. Cache waypoint\v"
      "3. Create path\v"
      "4. Delete path\v"
      "5. Add waypoint\v"
      "6. Delete waypoint\v"
      "7. Set Autopath Distance\v"
      "8. Set Radius\v\v"
      "9. Next...\v\v"
      "0. Exit"
   },

   // main waypoint menu (page 2)
   {
      0x3ff,
      "\\yWaypoint Operations (Page 2)\\w\v\v"
      "1. Waypoint stats\v"
      "2. Autowaypoint on/off\v"
      "3. Set flags\v"
      "4. Save waypoints\v"
      "5. Save without checking\v"
      "6. Load waypoints\v"
      "7. Check waypoints\v"
      "8. Noclip cheat on/off\v\v"
      "9. Previous...\v\v"
      "0. Exit"
   },

   // select waypoint radius menu
   {
      0x3ff,
      "\\yWaypoint Radius\\w\v\v"
      "1. SetRadius 0\v"
      "2. SetRadius 8\v"
      "3. SetRadius 16\v"
      "4. SetRadius 32\v"
      "5. SetRadius 48\v"
      "6. SetRadius 64\v"
      "7. SetRadius 80\v"
      "8. SetRadius 96\v"
      "9. SetRadius 128\v\v"
      "0. Exit"
   },

   // waypoint add menu
   {
      0x3ff,
      "\\yWaypoint Type\\w\v\v"
      "1. Normal\v"
      "\\r2. Terrorist Important\v"
      "3. Counter-Terrorist Important\v"
      "\\w4. Block with hostage / Ladder\v"
      "\\y5. Rescue Zone\v"
      "\\w6. Camping\v"
      "7. Camp End\v"
      "\\r8. Map Goal\v"
      "\\w9. Jump\v\v"
      "0. Exit"
   },

   // set waypoint flag menu
   {
      0x2ff,
      "\\yToggle Waypoint Flags\\w\v\v"
      "1. Block with Hostage\v"
      "2. Terrorists Specific\v"
      "3. CTs Specific\v"
      "4. Use Elevator\v"
      "5. Sniper Point (\\yFor Camp Points Only!\\w)\v\v"
      "0. Exit"
   },

   // kickmenu #1
   {
      0x0,
      NULL,
   },

   // kickmenu #2
   {
      0x0,
      NULL,
   },

   // kickmenu #3
   {
      0x0,
      NULL,
   },

   // kickmenu #4
   {
      0x0,
      NULL,
   },

   // command menu
   {
      0x23f,
      "\\yBot Command Menu\\w\v\v"
      "1. Make Double Jump\v"
      "2. Finish Double Jump\v\v"
      "3. Drop the C4 Bomb\v"
      "4. Drop the Weapon\v\v"
      "0. Exit"
   },

   // auto-path max distance
   {
      0x27f,
      "\\yAutoPath Distance\\w\v\v"
      "1. Distance 0\v"
      "2. Distance 100\v"
      "3. Distance 130\v"
      "4. Distance 160\v"
      "5. Distance 190\v"
      "6. Distance 220\v"
      "7. Distance 250 (Default)\v\v"
      "0. Exit"
   },

   // path connections
   {
      0x207,
      "\\yCreate Path (Choose Direction)\\w\v\v"
      "1. Outgoing Path\v"
      "2. Incoming Path\v"
      "3. Bidirectional (Both Ways)\v\v"
      "0. Exit"
   }
};