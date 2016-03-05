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

// variable type
enum VarType
{
   VT_NORMAL = 0,
   VT_READONLY,
   VT_PASSWORD,
   VT_NOSERVER,
   VT_NOREGISTER
};

// need since we don't want to use Singleton on engine class
class ConVarWrapper : public Singleton <ConVarWrapper>
{
private:
   struct VarPair
   {
      VarType type;
      cvar_t reg;
      class ConVar *self;
   };
   Array <VarPair> m_regs;

public:
   void RegisterVariable (const char *variable, const char *value, VarType varType, ConVar *self);
   void PushRegisteredConVarsToEngine (bool gameVars = false);
};

// provides utility functions to not call original engine (less call-cost)
class Engine
{
private:
   short m_drawModels[DRAW_NUM];

   // bot client command
   bool m_isBotCommand;
   char m_arguments[256];
   int m_argumentCount;

   edict_t *m_startEntity;
   edict_t *m_localEntity;

   // public functions
public:

   // precaches internal stuff
   void Precache (edict_t *startEntity);

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

   // sends bot command
   void IssueBotCommand (edict_t *ent, const char *fmt, ...);

   // public inlines
public:

   // get the current time on server
   inline float Time (void)
   {
      return g_pGlobals->time;
   }

   // get "maxplayers" limit on server
   inline int MaxClients (void)
   {
      return g_pGlobals->maxClients;
   }

   // get the fakeclient command interface
   inline bool IsBotCommand (void)
   {
      return m_isBotCommand;
   }

   // gets custom engine args for client command
   inline const char *GetOverrideArgs (void)
   {
      if (strncmp ("say ", m_arguments, 4) == 0)
         return &m_arguments[4];
      else if (strncmp ("say_team ", m_arguments, 9) == 0)
         return &m_arguments[9];

      return m_arguments;
   }

   // gets custom engine argv for client command
   inline const char *GetOverrideArgv (int num)
   {
      return ExtractSingleField (m_arguments, num, false);
   }

   // gets custom engine argc for client command
   inline int GetOverrideArgc (void)
   {
      return m_argumentCount;
   }

   inline edict_t *EntityOfIndex (const int index)
   {
      return static_cast <edict_t *> (m_startEntity + index);
   };

   inline int IndexOfEntity (const edict_t *ent)
   {
      return static_cast <int> (ent - m_startEntity);
   };

   inline bool IsNullEntity (const edict_t *ent)
   {
      return !ent || !IndexOfEntity (ent);
   }

   inline int GetTeam (edict_t *ent)
   {
      extern Client g_clients[32];

#ifndef XASH_CSDM
      return g_clients[IndexOfEntity (ent) - 1].team;
#else
      return g_clients[IndexOfEntity (ent) - 1].team = ent->v.team == 1 ? TERRORIST : CT;
#endif
   }

   // static utility functions
public:
   static const char *ExtractSingleField (const char *string, int id, bool terminate);
};

// simplify access for console variables
class ConVar
{
public:
   cvar_t *m_eptr;

public:
   ConVar (const char *name, const char *initval, VarType type = VT_NOSERVER);

   inline bool GetBool (void) { return m_eptr->value > 0.0f; }
   inline int GetInt (void) { return static_cast <int> (m_eptr->value); }
   inline float GetFloat (void) { return m_eptr->value; }
   inline const char *GetString (void) { return m_eptr->string; }
   inline void SetFloat (float val) { m_eptr->value = val; }
   inline void SetInt (int val) { SetFloat (static_cast <float> (val)); }
   inline void SetString (const char *val) { g_engfuncs.pfnCVarSetString (m_eptr->name, val); }
};
