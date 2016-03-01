//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//
// Purpose: Engine & Game interfaces.
//

#pragma once

// line draw
enum DrawLineType
{
   DRAW_SIMPLE,
   DRAW_ARROW,
   DRAW_NUM
};

// trace ignore
enum TraceIgnore
{
   TRACE_IGNORE_NONE = 0,
   TRACE_IGNORE_GLASS = (1 << 0),
   TRACE_IGNORE_MONSTERS = (1 << 1),
   TRACE_IGNORE_EVERYTHING = TRACE_IGNORE_GLASS | TRACE_IGNORE_MONSTERS
};

// provides utility functions to not call original engine (less call-cost)
class Engine
{
private:
   short m_drawModels[DRAW_NUM];

   // public functions
public:

   // precaches internal stuff
   void Precache (void);

   // prints data to servers console
   void Printf (const char *fmt, ...);

   // prints chat message to all players
   void ChatPrintf (const char *fmt, ...);

   // prints center message to all players
   void CenterPrintf (const char *fmt, ...);

   // prints message to client console
   void ClientPrintf (edict_t *ent, const char *fmt, ...);

   // display world line
   void DrawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life, DrawLineType type = DRAW_SIMPLE);

   // test line
   void TestLine (const Vector &start, const Vector &end, int ignoreFlags, edict_t *ignoreEntity, TraceResult *ptr);

   // test line
   void TestHull (const Vector &start, const Vector &end, int ignoreFlags, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr);

   // get's the wave length
   float GetWaveLength (const char *fileName);

   // we are on dedicated server ?
   bool IsDedicatedServer (void);

   // get stripped down mod name
   const char *GetModName (void);

   // get the valid mapname
   const char *GetMapName (void);

   // get the "any" entity origin
   Vector GetAbsOrigin (edict_t *ent);

   // send server command
   void IssueCmd (const char *fmt, ...);

   // registers a server command
   void RegisterCmd (const char *command, void func (void));

   // play's sound to client
   void EmitSound (edict_t *ent, const char *sound);

   // public inlines
public:

   // get the current time on server
   static inline float Time (void)
   {
      return g_pGlobals->time;
   }

   // get "maxplayers" limit on server
   static inline int MaxClients (void)
   {
      return g_pGlobals->maxClients;
   }
};

// provides quick access to engine instance
extern Engine engine;