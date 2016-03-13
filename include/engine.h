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

// netmessage functions
enum NetMsgId
{
   NETMSG_UNDEFINED = -1,
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
   NETMSG_NUM = 21
};

// variable reg pair
struct VarPair
{
   VarType type;
   cvar_t reg;
   bool regMissing;
   class ConVar *self;
};

// translation pair
struct TranslatorPair
{
   const char *original;
   const char *translated;
};

// network message block
struct MessageBlock
{
   int bot;
   int state;
   int msg;
   int regMsgs[NETMSG_NUM];
};

// provides utility functions to not call original engine (less call-cost)
class Engine : public Singleton <Engine>
{
private:
   short m_drawModels[DRAW_NUM];

   // bot client command
   bool m_isBotCommand;
   char m_arguments[256];
   int m_argumentCount;

   edict_t *m_startEntity;
   edict_t *m_localEntity;

   Array <VarPair> m_cvars;
   Array <TranslatorPair> m_language;

   MessageBlock m_msgBlock;

public:
   Engine (void);

   ~Engine (void);

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

   // adds cvar to registration stack
   void PushVariableToStack (const char *variable, const char *value, VarType varType, bool regMissing, ConVar *self);

   // sends local registration stack for engine registration
   void PushRegisteredConVarsToEngine (bool gameVars = false);

   // translates bot message into needed language
   char *TraslateMessage (const char *input);

   // cleanup translator resources
   void TerminateTranslator (void);

   // do actual network message processing
   void ProcessMessageCapture (void *ptr);

   // public inlines
public:

   // get the current time on server
   FORCEINLINE float Time (void)
   {
      return g_pGlobals->time;
   }

   // get "maxplayers" limit on server
   FORCEINLINE int MaxClients (void)
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

   // gets edict pointer out of entity index
   FORCEINLINE edict_t *EntityOfIndex (const int index)
   {
      return static_cast <edict_t *> (m_startEntity + index);
   };

   // gets edict index out of it's pointer
   FORCEINLINE int IndexOfEntity (const edict_t *ent)
   {
      return static_cast <int> (ent - m_startEntity);
   };

   // verify entity isn't null
   FORCEINLINE bool IsNullEntity (const edict_t *ent)
   {
      return !ent || !IndexOfEntity (ent);
   }

   // gets the player team
   FORCEINLINE int GetTeam (edict_t *ent)
   {
      extern Client g_clients[MAX_ENGINE_PLAYERS];

#ifndef XASH_CSDM
      return g_clients[IndexOfEntity (ent) - 1].team;
#else
      return g_clients[IndexOfEntity (ent) - 1].team = ent->v.team == 1 ? TERRORIST : CT;
#endif
   }

   // adds translation pair from config
   inline void PushTranslationPair (const TranslatorPair &lang)
   {
      m_language.Push (lang);
   }

   // resets the message capture mechanism
   inline void ResetMessageCapture (void)
   {
      m_msgBlock.msg = NETMSG_UNDEFINED;
      m_msgBlock.state = 0;
      m_msgBlock.bot = 0;
   };

   // sets the currently executed message
   inline void SetOngoingMessageId (int message)
   {
      m_msgBlock.msg = message;
   }

   // set the bot entity that receive this message
   inline void SetOngoingMessageReceiver (int id)
   {
      m_msgBlock.bot = id;
   }

   // find registered message id
   FORCEINLINE int FindMessageId (int type)
   {
      return m_msgBlock.regMsgs[type];
   }

   // assigns message id for message type
   inline void AssignMessageId (int type, int id)
   {
      m_msgBlock.regMsgs[type] = id;
   }

   // tries to set needed message id
   FORCEINLINE void TryCaptureMessage (int type, int msgId)
   {
      if (type == m_msgBlock.regMsgs[msgId])
         SetOngoingMessageId (msgId);
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
   ConVar (const char *name, const char *initval, VarType type = VT_NOSERVER, bool regMissing = false);

   FORCEINLINE bool GetBool (void) { return m_eptr->value > 0.0f; }
   FORCEINLINE int GetInt (void) { return static_cast <int> (m_eptr->value); }
   FORCEINLINE float GetFloat (void) { return m_eptr->value; }
   FORCEINLINE const char *GetString (void) { return m_eptr->string; }
   FORCEINLINE void SetFloat (float val) { m_eptr->value = val; }
   FORCEINLINE void SetInt (int val) { SetFloat (static_cast <float> (val)); }
   FORCEINLINE void SetString (const char *val) { g_engfuncs.pfnCvar_DirectSet (m_eptr, const_cast <char *> (val)); }
};
