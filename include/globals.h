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

#ifndef GLOBALS_INCLUDED
#define GLOBALS_INCLUDED

extern bool g_canSayBombPlanted;
extern bool g_bombPlanted;
extern bool g_bombSayString; 
extern bool g_roundEnded;
extern bool g_radioInsteadVoice;
extern bool g_waypointOn;
extern bool g_waypointsChanged;
extern bool g_autoWaypoint;
extern bool g_botsCanPause; 
extern bool g_editNoclip;
extern bool g_isMetamod;
extern bool g_isFakeCommand;
extern bool g_sendAudioFinished;
extern bool g_isCommencing;
extern bool g_leaderChoosen[2];

extern float g_autoPathDistance;
extern float g_timeBombPlanted;
extern float g_timeNextBombUpdate;
extern float g_lastChatTime;
extern float g_timeRoundEnd;
extern float g_timeRoundMid;
extern float g_timeNextBombUpdate;
extern float g_timeRoundStart;
extern float g_lastRadioTime[2];

extern int g_mapType;
extern int g_numWaypoints;
extern int g_gameVersion;
extern int g_fakeArgc;

extern int g_highestDamageCT;
extern int g_highestDamageT;
extern int g_highestKills;

extern int g_normalWeaponPrefs[NUM_WEAPONS];
extern int g_rusherWeaponPrefs[NUM_WEAPONS];
extern int g_carefulWeaponPrefs[NUM_WEAPONS];
extern int g_grenadeBuyPrecent[NUM_WEAPONS - 23];
extern int g_botBuyEconomyTable[NUM_WEAPONS - 15];
extern int g_radioSelect[32];
extern int g_lastRadio[2];
extern int g_storeAddbotVars[4];
extern int *g_weaponPrefs[];

extern short g_modelIndexLaser;
extern short g_modelIndexArrow;
extern char g_fakeArgv[256];

extern Array <Array <String> > g_chatFactory;
extern Array <Array <ChatterItem> > g_chatterFactory;
extern Array <BotName> g_botNames;
extern Array <KeywordFactory> g_replyFactory;
extern RandGen g_randGen;

extern FireDelay g_fireDelay[NUM_WEAPONS + 1];
extern WeaponSelect g_weaponSelect[NUM_WEAPONS + 1];
extern WeaponProperty g_weaponDefs[MAX_WEAPONS + 1];

extern Client g_clients[32];
extern MenuText g_menus[21];
extern SkillDefinition g_skillTab[6];
extern TaskItem g_taskFilters[];

extern Experience *g_experienceData;

extern edict_t *g_hostEntity; 
extern edict_t *g_worldEdict;
extern Library *g_gameLib;

extern gamefuncs_t g_functionTable;
extern EntityAPI_t g_entityAPI;
extern FuncPointers_t g_funcPointers;
extern NewEntityAPI_t g_getNewEntityAPI;
extern BlendAPI_t g_serverBlendingAPI;

#endif // GLOBALS_INCLUDED
