//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

void Engine::Precache (edict_t *startEntity)
{
   // this function precaches needed models and initialize class variables

   m_drawModels[DRAW_SIMPLE] = PRECACHE_MODEL (ENGINE_STR ("sprites/laserbeam.spr"));
   m_drawModels[DRAW_ARROW] = PRECACHE_MODEL (ENGINE_STR ("sprites/arrow1.spr"));

   m_isBotCommand = false;
   m_argumentCount = 0;
   m_arguments[0] = { 0, };

   m_localEntity = NULL;
   m_startEntity = startEntity;
}

void Engine::Printf (const char *fmt, ...)
{
   // this function outputs string into server console

   va_list ap;
   static char string[1024];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), locale.TranslateInput (fmt), ap);
   va_end (ap);

   g_engfuncs.pfnServerPrint (string);
   g_engfuncs.pfnServerPrint ("\n");
}

void Engine::ChatPrintf (const char *fmt, ...)
{
   va_list ap;
   static char string[1024];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), locale.TranslateInput (fmt), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      engine.Printf (string);
      return;
   }
   strcat (string, "\n");

   MESSAGE_BEGIN (MSG_BROADCAST, netmsg.GetId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTTALK);
      WRITE_STRING (string);
   MESSAGE_END ();
}

void Engine::CenterPrintf (const char *fmt, ...)
{
   va_list ap;
   static char string[1024];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), locale.TranslateInput (fmt), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      engine.Printf (string);
      return;
   }
   strcat (string, "\n");

   MESSAGE_BEGIN (MSG_BROADCAST, netmsg.GetId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTCENTER);
      WRITE_STRING (string);
   MESSAGE_END ();
}

void Engine::ClientPrintf (edict_t *ent, const char *fmt, ...)
{
   va_list ap;
   static char string[1024];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), locale.TranslateInput (fmt), ap);
   va_end (ap);

   if (engine.IsNullEntity (ent) || ent == g_hostEntity)
   {
      engine.Printf (string);
      return;
   }
   strcat (string, "\n");
   g_engfuncs.pfnClientPrintf (ent, print_console, string);
}

void Engine::DrawLine (edict_t * ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life, DrawLineType type)
{
   // this function draws a arrow visible from the client side of the player whose player entity
   // is pointed to by ent, from the vector location start to the vector location end,
   // which is supposed to last life tenths seconds, and having the color defined by RGB.

   if (!IsValidPlayer (ent))
      return; // reliability check

   MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, ent);
      WRITE_BYTE (TE_BEAMPOINTS);
      WRITE_COORD (end.x);
      WRITE_COORD (end.y);
      WRITE_COORD (end.z);
      WRITE_COORD (start.x);
      WRITE_COORD (start.y);
      WRITE_COORD (start.z);
      WRITE_SHORT (m_drawModels[type]);
      WRITE_BYTE (0); // framestart
      WRITE_BYTE (10); // framerate
      WRITE_BYTE (life); // life in 0.1's
      WRITE_BYTE (width); // width
      WRITE_BYTE (noise); // noise

      WRITE_BYTE (red); // r, g, b
      WRITE_BYTE (green); // r, g, b
      WRITE_BYTE (blue); // r, g, b

      WRITE_BYTE (brightness); // brightness
      WRITE_BYTE (speed); // speed
   MESSAGE_END ();
}

void Engine::TestLine (const Vector &start, const Vector &end, int ignoreFlags, edict_t *ignoreEntity, TraceResult *ptr)
{
   // this function traces a line dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or false), and stops
   // at the first obstacle encountered, returning the results of the trace in the TraceResult structure
   // ptr. Such results are (amongst others) the distance traced, the hit surface, the hit plane
   // vector normal, etc. See the TraceResult structure for details. This function allows to specify
   // whether the trace starts "inside" an entity's polygonal model, and if so, to specify that entity
   // in ignoreEntity in order to ignore it as a possible obstacle.

   int engineFlags = 0;

   if (ignoreFlags & TRACE_IGNORE_MONSTERS)
      engineFlags = 1;

   if (ignoreFlags & TRACE_IGNORE_GLASS)
      engineFlags |= 0x100;

   g_engfuncs.pfnTraceLine (start, end, engineFlags, ignoreEntity, ptr);
}

void Engine::TestHull (const Vector &start, const Vector &end, int ignoreFlags, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr)
{
   // this function traces a hull dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or
   // false), and stops at the first obstacle encountered, returning the results
   // of the trace in the TraceResult structure ptr, just like TraceLine. Hulls that can be traced
   // (by parameter hull_type) are point_hull (a line), head_hull (size of a crouching player),
   // human_hull (a normal body size) and large_hull (for monsters?). Not all the hulls in the
   // game can be traced here, this function is just useful to give a relative idea of spatial
   // reachability (i.e. can a hostage pass through that tiny hole ?) Also like TraceLine, this
   // function allows to specify whether the trace starts "inside" an entity's polygonal model,
   // and if so, to specify that entity in ignoreEntity in order to ignore it as an obstacle.

   (*g_engfuncs.pfnTraceHull) (start, end, (ignoreFlags & TRACE_IGNORE_MONSTERS), hullNumber, ignoreEntity, ptr);
}

float Engine::GetWaveLength (const char *fileName)
{
   extern ConVar yb_chatter_path;
   const char *filePath = FormatBuffer ("%s/%s/%s.wav", GetModName (), yb_chatter_path.GetString (), fileName);

   File fp (filePath, "rb");

   // we're got valid handle?
   if (!fp.IsValid ())
      return 0.0f;

   // check if we have engine function for this
   if (g_engfuncs.pfnGetApproxWavePlayLen != NULL)
   {
      fp.Close ();
      return g_engfuncs.pfnGetApproxWavePlayLen (filePath) / 1000.0f;
   }

   // else fuck with manual search
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
   } waveHdr;

   memset (&waveHdr, 0, sizeof (waveHdr));

   if (fp.Read (&waveHdr, sizeof (WavHeader)) == 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has wrong or unsupported format", filePath);
      return 0.0f;
   }

   if (strncmp (waveHdr.chunkID, "WAVE", 4) != 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has wrong wave chunk id", filePath);
      return 0.0f;
   }
   fp.Close ();

   if (waveHdr.dataChunkLength == 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has zero length!", filePath);
      return 0.0f;
   }
   return static_cast <float> (waveHdr.dataChunkLength) / static_cast <float> (waveHdr.bytesPerSecond);
}

bool Engine::IsDedicatedServer (void)
{
   // return true if server is dedicated server, false otherwise
   return g_engfuncs.pfnIsDedicatedServer () > 0;
}

const char *Engine::GetModName (void)
{
   // this function returns mod name without path

   static char engineModName[256];
   g_engfuncs.pfnGetGameDir (engineModName); // ask the engine for the MOD directory path

   String mod (engineModName);
   int pos = mod.ReverseFind ('\\');

   if (pos == -1)
      pos = mod.ReverseFind ('/');

   if (pos == -1)
      return &engineModName[0];

   return mod.Mid (pos, -1).GetBuffer ();
}

const char *Engine::GetMapName (void)
{
   // this function gets the map name and store it in the map_name global string variable.

   static char engineMap[256];
   strncpy (engineMap, const_cast <const char *> (g_pGlobals->pStringBase + static_cast <int> (g_pGlobals->mapname)), SIZEOF_CHAR (engineMap));

   return &engineMap[0];
}

Vector Engine::GetAbsOrigin (edict_t *ent)
{
   // this expanded function returns the vector origin of a bounded entity, assuming that any
   // entity that has a bounding box has its center at the center of the bounding box itself.

   if (engine.IsNullEntity (ent))
      return Vector::GetZero ();

   if (ent->v.origin.IsZero ())
      return ent->v.absmin + ent->v.size * 0.5f;

   return ent->v.origin;
}

void Engine::RegisterCmd (const char * command, void func (void))
{
   // this function tells the engine that a new server command is being declared, in addition
   // to the standard ones, whose name is command_name. The engine is thus supposed to be aware
   // that for every "command_name" server command it receives, it should call the function
   // pointed to by "function" in order to handle it.

   g_engfuncs.pfnAddServerCommand (const_cast <char *> (command), func);
}

void Engine::EmitSound (edict_t *ent, const char *sound)
{
   g_engfuncs.pfnEmitSound (ent, CHAN_WEAPON, sound, 1.0f, ATTN_NORM, 0, 100.0f);
}

void Engine::IssueBotCommand (edict_t *ent, const char *fmt, ...)
{
   // the purpose of this function is to provide fakeclients (bots) with the same client
   // command-scripting advantages (putting multiple commands in one line between semicolons)
   // as real players. It is an improved version of botman's FakeClientCommand, in which you
   // supply directly the whole string as if you were typing it in the bot's "console". It
   // is supposed to work exactly like the pfnClientCommand (server-sided client command).

   if (engine.IsNullEntity (ent))
      return; 

   va_list ap;
   static char string[256];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), fmt, ap);
   va_end (ap);

   if (IsNullString (string))
      return;

   m_arguments[0] = { 0, };
   m_argumentCount = 0;

   m_isBotCommand = true;

   int i, pos = 0;
   int length = strlen (string);

   while (pos < length)
   {
      int start = pos;
      int stop = pos;

      while (pos < length && string[pos] != ';')
         pos++;

      if (string[pos - 1] == '\n')
         stop = pos - 2;
      else
         stop = pos - 1; 

      for (i = start; i <= stop; i++)
         m_arguments[i - start] = string[i];

      m_arguments[i - start] = 0;
      pos++;

      int index = 0;
      m_argumentCount = 0;

      while (index < i - start)
      {
         while (index < i - start && m_arguments[index] == ' ')
            index++;

         if (m_arguments[index] == '"')
         {
            index++;

            while (index < i - start && m_arguments[index] != '"')
               index++;
            index++;
         }
         else
            while (index < i - start && m_arguments[index] != ' ')
               index++;

         m_argumentCount++;
      }
      MDLL_ClientCommand (ent);
   }
   m_isBotCommand = false;

   m_arguments[0] = { 0, };
   m_argumentCount = 0;
}

const char *Engine::ExtractSingleField (const char *string, int id, bool terminate)
{
   // this function gets and returns a particuliar field in a string where several strings are concatenated

   static char field[256];
   field[0] = { 0, };

   int pos = 0, count = 0, start = 0, stop = 0;
   int length = strlen (string);

   while (pos < length && count <= id)
   {
      while (pos < length && (string[pos] == ' ' || string[pos] == '\t'))
         pos++;

      if (string[pos] == '"')
      {
         pos++;
         start = pos;

         while (pos < length && string[pos] != '"')
            pos++;

         stop = pos - 1;
         pos++;
      }
      else
      {
         start = pos;

         while (pos < length && string[pos] != ' ' && string[pos] != '\t')
            pos++;

         stop = pos - 1;
      }

      if (count == id)
      {
         int i = start;

         for (; i <= stop; i++)
            field[i - start] = string[i];

         field[i - start] = 0;
         break;
      }
      count++; // we have parsed one field more
   }

   if (terminate)
      field[strlen (field) - 1] = 0;

   String::TrimExternalBuffer (field);
   return field;
}

void Engine::IssueCmd (const char *fmt, ...)
{
   // this function asks the engine to execute a server command

   va_list ap;
   static char string[1024];

   // concatenate all the arguments in one string
   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), fmt, ap);
   va_end (ap);

   strcat (string, "\n");
   g_engfuncs.pfnServerCommand (string);
}

void ConVarWrapper::RegisterVariable (const char *variable, const char *value, VarType varType, ConVar *self)
{
   // this function adds globally defined variable to registration stack

   VarPair pair;
   memset (&pair, 0, sizeof (VarPair));

   pair.reg.name = const_cast <char *> (variable);
   pair.reg.string = const_cast <char *> (value);

   int engineFlags = FCVAR_EXTDLL;

   if (varType == VT_NORMAL)
      engineFlags |= FCVAR_SERVER;
   else if (varType == VT_READONLY)
      engineFlags |= FCVAR_SERVER | FCVAR_SPONLY | FCVAR_PRINTABLEONLY;
   else if (varType == VT_PASSWORD)
      engineFlags |= FCVAR_PROTECTED;

   pair.reg.flags = engineFlags;
   pair.self = self;
   pair.type = varType;

   m_regs.Push (pair);
}

void ConVarWrapper::PushRegisteredConVarsToEngine (bool gameVars)
{
   // this function pushes all added global variables to engine registration

   FOR_EACH_AE (m_regs, i)
   {
      VarPair *ptr = &m_regs[i];

      if (ptr->type != VT_NOREGISTER)
      {
         ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);

         if (ptr->self->m_eptr == NULL)
         {
            g_engfuncs.pfnCVarRegister (&ptr->reg);
            ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);
         }
      }
      else if (gameVars && ptr->type == VT_NOREGISTER)
      {
         ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);

         // ensure game cvar exists
         InternalAssert (ptr->self->m_eptr != NULL);
      }
   }
}

ConVar::ConVar (const char *name, const char *initval, VarType type) : m_eptr (NULL)
{
   ConVarWrapper::GetReference ().RegisterVariable (name, initval, type, this);
}
