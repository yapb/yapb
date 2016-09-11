//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#pragma once

extern bool g_canSayBombPlanted;
extern bool g_bombPlanted;
extern bool g_bombSayString; 
extern bool g_roundEnded;
extern bool g_waypointOn;
extern bool g_autoWaypoint;
extern bool g_botsCanPause; 
extern bool g_editNoclip;
extern bool g_isCommencing;

extern float g_autoPathDistance;
extern float g_timeBombPlanted;
extern float g_timeNextBombUpdate;
extern float g_lastChatTime;
extern float g_timeRoundEnd;
extern float g_timeRoundMid;
extern float g_timeRoundStart;
extern float g_timePerSecondUpdate;
extern float g_lastRadioTime[2];

extern int g_mapType;
extern int g_numWaypoints;
extern int g_gameFlags;

extern int g_highestDamageCT;
extern int g_highestDamageT;
extern int g_highestKills;

extern int g_normalWeaponPrefs[NUM_WEAPONS];
extern int g_rusherWeaponPrefs[NUM_WEAPONS];
extern int g_carefulWeaponPrefs[NUM_WEAPONS];
extern int g_grenadeBuyPrecent[NUM_WEAPONS - 23];
extern int g_botBuyEconomyTable[NUM_WEAPONS - 15];
extern int g_radioSelect[MAX_ENGINE_PLAYERS];
extern int g_lastRadio[2];
extern int g_storeAddbotVars[4];
extern int *g_weaponPrefs[];

extern Array <Array <String> > g_chatFactory;
extern Array <Array <ChatterItem> > g_chatterFactory;
extern Array <BotName> g_botNames;
extern Array <KeywordFactory> g_replyFactory;
extern RandomSequenceOfUnique Random;

extern WeaponSelect g_weaponSelect[NUM_WEAPONS + 1];
extern WeaponProperty g_weaponDefs[MAX_WEAPONS + 1];

extern Client g_clients[MAX_ENGINE_PLAYERS];
extern MenuText g_menus[21];
extern TaskItem g_taskFilters[];

extern Experience *g_experienceData;

extern edict_t *g_hostEntity; 
extern Library *g_gameLib;

extern gamefuncs_t g_functionTable;

static inline bool IsNullString (const char *input)
{
   if (input == nullptr)
      return true;

   return *input == '\0';
}
