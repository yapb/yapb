//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#include <core.h>

Engine::Engine (void)
{
   m_startEntity = nullptr;
   m_localEntity = nullptr;

   m_language.RemoveAll ();
   ResetMessageCapture ();

   for (int i = 0; i < NETMSG_NUM; i++)
      m_msgBlock.regMsgs[i] = NETMSG_UNDEFINED;

   m_isBotCommand = false;
   m_argumentCount = 0;
   memset (m_arguments, 0, sizeof (m_arguments));

   m_cvars.RemoveAll ();
   m_language.RemoveAll ();
}

Engine::~Engine (void)
{
   TerminateTranslator ();
   ResetMessageCapture ();

   for (int i = 0; i < NETMSG_NUM; i++)
      m_msgBlock.regMsgs[i] = NETMSG_UNDEFINED;
}

void Engine::Precache (edict_t *startEntity)
{
   // this function precaches needed models and initialize class variables

   m_drawModels[DRAW_SIMPLE] = PRECACHE_MODEL (ENGINE_STR ("sprites/laserbeam.spr"));
   m_drawModels[DRAW_ARROW] = PRECACHE_MODEL (ENGINE_STR ("sprites/arrow1.spr"));

   m_localEntity = nullptr;
   m_startEntity = startEntity;
}

void Engine::Printf (const char *fmt, ...)
{
   // this function outputs string into server console

   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), TraslateMessage (fmt), ap);
   va_end (ap);

   strcat (string, "\n");
   g_engfuncs.pfnServerPrint (string);
}

void Engine::ChatPrintf (const char *fmt, ...)
{
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), TraslateMessage (fmt), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      Printf (string);
      return;
   }
   strcat (string, "\n");

   MESSAGE_BEGIN (MSG_BROADCAST, FindMessageId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTTALK);
      WRITE_STRING (string);
   MESSAGE_END ();
}

void Engine::CenterPrintf (const char *fmt, ...)
{
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), TraslateMessage (fmt), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      Printf (string);
      return;
   }
   strcat (string, "\n");

   MESSAGE_BEGIN (MSG_BROADCAST, FindMessageId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTCENTER);
      WRITE_STRING (string);
   MESSAGE_END ();
}

void Engine::ClientPrintf (edict_t *ent, const char *fmt, ...)
{
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), TraslateMessage (fmt), ap);
   va_end (ap);

   if (IsDedicatedServer () || IsNullEntity (ent) || ent == g_hostEntity)
   {
      Printf (string);
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

   MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, ent);
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
   if (g_engfuncs.pfnGetApproxWavePlayLen != nullptr)
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
      uint16 dummy;
      uint16 channels;
      unsigned long sampleRate;
      unsigned long bytesPerSecond;
      uint16 bytesPerSample;
      uint16 bitsPerSample;
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
   static bool dedicated = g_engfuncs.pfnIsDedicatedServer () > 0;

   return dedicated;
}

const char *Engine::GetModName (void)
{
   // this function returns mod name without path

   static char engineModName[256];

   g_engfuncs.pfnGetGameDir (engineModName);
   int length = strlen (engineModName);

   int stop = length - 1;
   while ((engineModName[stop] == '\\' || engineModName[stop] == '/') && stop > 0)
      stop--;

   int start = stop;
   while (engineModName[start] != '\\' && engineModName[start] != '/' && start > 0)
      start--;

   if (engineModName[start] == '\\' || engineModName[start] == '/')
      start++;

   for (length = start; length <= stop; length++)
      engineModName[length - start] = engineModName[length];

   engineModName[length - start] = 0; // terminate the string

   return &engineModName[0];
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

   if (IsNullEntity (ent))
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
   g_engfuncs.pfnEmitSound (ent, CHAN_WEAPON, sound, 1.0f, ATTN_NORM, 0, 100);
}

void Engine::IssueBotCommand (edict_t *ent, const char *fmt, ...)
{
   // the purpose of this function is to provide fakeclients (bots) with the same client
   // command-scripting advantages (putting multiple commands in one line between semicolons)
   // as real players. It is an improved version of botman's FakeClientCommand, in which you
   // supply directly the whole string as if you were typing it in the bot's "console". It
   // is supposed to work exactly like the pfnClientCommand (server-sided client command).

   if (IsNullEntity (ent))
      return; 

   va_list ap;
   static char string[256];

   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), fmt, ap);
   va_end (ap);

   if (IsNullString (string))
      return;

   m_arguments[0] = 0x0;
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

   m_arguments[0] = 0x0;
   m_argumentCount = 0;
}

const char *Engine::ExtractSingleField (const char *string, int id, bool terminate)
{
   // this function gets and returns a particular field in a string where several strings are concatenated

   static char field[256];
   field[0] = 0x0;

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
   char string[MAX_PRINT_BUFFER];

   // concatenate all the arguments in one string
   va_start (ap, fmt);
   vsnprintf (string, SIZEOF_CHAR (string), fmt, ap);
   va_end (ap);

   strcat (string, "\n");
   g_engfuncs.pfnServerCommand (string);
}

void Engine::PushVariableToStack (const char *variable, const char *value, VarType varType, bool regMissing, ConVar *self)
{
   // this function adds globally defined variable to registration stack

   VarPair pair;
   memset (&pair, 0, sizeof (VarPair));

   pair.reg.name = const_cast <char *> (variable);
   pair.reg.string = const_cast <char *> (value);
   pair.regMissing = regMissing;

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

   m_cvars.Push (pair);
}

void Engine::PushRegisteredConVarsToEngine (bool gameVars)
{
   // this function pushes all added global variables to engine registration

   FOR_EACH_AE (m_cvars, i)
   {
      VarPair *ptr = &m_cvars[i];

      if (ptr->type != VT_NOREGISTER)
      {
         ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);

         if (ptr->self->m_eptr == nullptr)
         {
            g_engfuncs.pfnCVarRegister (&ptr->reg);
            ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);
         }
      }
      else if (gameVars && ptr->type == VT_NOREGISTER)
      {
         ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);

         if (ptr->regMissing && ptr->self->m_eptr == nullptr)
         {
            g_engfuncs.pfnCVarRegister (&ptr->reg);
            ptr->self->m_eptr = g_engfuncs.pfnCVarGetPointer (ptr->reg.name);
         }
         InternalAssert (ptr->self->m_eptr != nullptr); // ensure game var exists
      }
   }
}

char *Engine::TraslateMessage (const char *input)
{
   // this function translate input string into needed language

   if (IsDedicatedServer ())
      return const_cast <char *> (&input[0]);

   static char string[MAX_PRINT_BUFFER];
   const char *ptr = input + strlen (input) - 1;

   while (ptr > input && *ptr == '\n')
      ptr--;

   if (ptr != input)
      ptr++;

   strncpy (string, input, SIZEOF_CHAR (string));
   String::TrimExternalBuffer (string);

   FOR_EACH_AE (m_language, i)
   {
      if (strcmp (string, m_language[i].original) == 0)
      {
         strncpy (string, m_language[i].translated, SIZEOF_CHAR (string));

         if (ptr != input)
            strncat (string, ptr, MAX_PRINT_BUFFER - 1 - strlen (string));

         return &string[0];
      }
   }
   return const_cast <char *> (&input[0]); // nothing found
}

void Engine::TerminateTranslator (void)
{
   // this function terminates language translator and frees all memory

   FOR_EACH_AE (m_language, it)
   {
      delete[] m_language[it].original;
      delete[] m_language[it].translated;
   }
   m_language.RemoveAll ();
}

void Engine::ProcessMessageCapture (void *ptr)
{
   if (m_msgBlock.msg == NETMSG_UNDEFINED)
      return;

   // some needed variables
   static uint8 r, g, b;
   static uint8 enabled;

   static int damageArmor, damageTaken, damageBits;
   static int killerIndex, victimIndex, playerIndex;
   static int index, numPlayers;
   static int state, id, clip;

   static Vector damageOrigin;
   static WeaponProperty weaponProp;

   // some widely used stuff
   Bot *bot = bots.GetBot (m_msgBlock.bot);

   char *strVal = reinterpret_cast <char *> (ptr);
   int intVal = *reinterpret_cast <int *> (ptr);
   uint8 byteVal = *reinterpret_cast <uint8 *> (ptr);

   // now starts of network message execution
   switch (m_msgBlock.msg)
   {
   case NETMSG_VGUI:
      // this message is sent when a VGUI menu is displayed.

      if (m_msgBlock.state == 0)
      {
         switch (intVal)
         {
         case VMS_TEAM:
            bot->m_startAction = GSM_TEAM_SELECT;
            break;

         case VMS_TF:
         case VMS_CT:
            bot->m_startAction = GSM_CLASS_SELECT;
            break;
         }
      }
      break;

   case NETMSG_SHOWMENU:
      // this message is sent when a text menu is displayed.

      if (m_msgBlock.state < 3) // ignore first 3 fields of message
         break;

      if (strcmp (strVal, "#Team_Select") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#Team_Select_Spect") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#IG_Team_Select_Spect") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#IG_Team_Select") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#IG_VIP_Team_Select") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#IG_VIP_Team_Select_Spect") == 0) // team select menu?
         bot->m_startAction = GSM_TEAM_SELECT;
      else if (strcmp (strVal, "#Terrorist_Select") == 0) // T model select?
         bot->m_startAction = GSM_CLASS_SELECT;
      else if (strcmp (strVal, "#CT_Select") == 0) // CT model select menu?
         bot->m_startAction = GSM_CLASS_SELECT;

      break;

   case NETMSG_WEAPONLIST:
      // this message is sent when a client joins the game. All of the weapons are sent with the weapon ID and information about what ammo is used.

      switch (m_msgBlock.state)
      {
      case 0:
         strncpy (weaponProp.className, strVal, SIZEOF_CHAR (weaponProp.className));
         break;

      case 1:
         weaponProp.ammo1 = intVal; // ammo index 1
         break;

      case 2:
         weaponProp.ammo1Max = intVal; // max ammo 1
         break;

      case 5:
         weaponProp.slotID = intVal; // slot for this weapon
         break;

      case 6:
         weaponProp.position = intVal; // position in slot
         break;

      case 7:
         weaponProp.id = intVal; // weapon ID
         break;

      case 8:
         weaponProp.flags = intVal; // flags for weapon (WTF???)
         g_weaponDefs[weaponProp.id] = weaponProp; // store away this weapon with it's ammo information...
         break;
      }
      break;

   case NETMSG_CURWEAPON:
      // this message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon). In CS it's also called when Ammo is increased/decreased

      switch (m_msgBlock.state)
      {
      case 0:
         state = intVal; // state of the current weapon (WTF???)
         break;

      case 1:
         id = intVal; // weapon ID of current weapon
         break;

      case 2:
         clip = intVal; // ammo currently in the clip for this weapon

         if (id <= 31)
         {
            if (state != 0)
               bot->m_currentWeapon = id;

            // ammo amount decreased ? must have fired a bullet...
            if (id == bot->m_currentWeapon && bot->m_ammoInClip[id] > clip)
               bot->m_timeLastFired = Time (); // remember the last bullet time

            bot->m_ammoInClip[id] = clip;
         }
         break;
      }
      break;

   case NETMSG_AMMOX:
      // this message is sent whenever ammo amounts are adjusted (up or down). NOTE: Logging reveals that CS uses it very unreliable!

      switch (m_msgBlock.state)
      {
      case 0:
         index = intVal; // ammo index (for type of ammo)
         break;

      case 1:
         bot->m_ammo[index] = intVal; // store it away
         break;
      }
      break;

   case NETMSG_AMMOPICKUP:
      // this message is sent when the bot picks up some ammo (AmmoX messages are also sent so this message is probably
      // not really necessary except it allows the HUD to draw pictures of ammo that have been picked up.  The bots
      // don't really need pictures since they don't have any eyes anyway.

      switch (m_msgBlock.state)
      {
      case 0:
         index = intVal;
         break;

      case 1:
         bot->m_ammo[index] = intVal;
         break;
      }
      break;

   case NETMSG_DAMAGE:
      // this message gets sent when the bots are getting damaged.

      switch (m_msgBlock.state)
      {
      case 0:
         damageArmor = intVal;
         break;

      case 1:
         damageTaken = intVal;
         break;

      case 2:
         damageBits = intVal;

         if (bot != nullptr && (damageArmor > 0 || damageTaken > 0))
            bot->TakeDamage (bot->pev->dmg_inflictor, damageTaken, damageArmor, damageBits);
         break;
      }
      break;

   case NETMSG_MONEY:
      // this message gets sent when the bots money amount changes

      if (m_msgBlock.state == 0)
         bot->m_moneyAmount = intVal; // amount of money
      break;

   case NETMSG_STATUSICON:
      switch (m_msgBlock.state)
      {
      case 0:
         enabled = byteVal;
         break;

      case 1:
         if (strcmp (strVal, "defuser") == 0)
            bot->m_hasDefuser = (enabled != 0);
         else if (strcmp (strVal, "buyzone") == 0)
         {
            bot->m_inBuyZone = (enabled != 0);

            // try to equip in buyzone
            bot->EquipInBuyzone (BUYSTATE_PRIMARY_WEAPON);
         }
         else if (strcmp (strVal, "vipsafety") == 0)
            bot->m_inVIPZone = (enabled != 0);
         else if (strcmp (strVal, "c4") == 0)
            bot->m_inBombZone = (enabled == 2);

         break;
      }
      break;

   case NETMSG_DEATH: // this message sends on death
      switch (m_msgBlock.state)
      {
      case 0:
         killerIndex = intVal;
         break;

      case 1:
         victimIndex = intVal;
         break;

      case 2:
         bots.SetDeathMsgState (true);

         if (killerIndex != 0 && killerIndex != victimIndex)
         {
            edict_t *killer = EntityOfIndex (killerIndex);
            edict_t *victim = EntityOfIndex (victimIndex);

            if (IsNullEntity (killer) || IsNullEntity (victim))
               break;

            if (yb_communication_type.GetInt () == 2)
            {
               // need to send congrats on well placed shot
               for (int i = 0; i < MaxClients (); i++)
               {
                  Bot *notify = bots.GetBot (i);

                  if (notify != nullptr && notify->m_notKilled && killer != notify->GetEntity () && notify->EntityIsVisible (victim->v.origin) && GetTeam (killer) == notify->m_team && GetTeam (killer) != GetTeam (victim))
                  {
                     if (killer == g_hostEntity)
                        notify->HandleChatterMessage ("#Bot_NiceShotCommander");
                     else
                        notify->HandleChatterMessage ("#Bot_NiceShotPall");

                     break;
                  }
               }
            }

            // notice nearby to victim teammates, that attacker is near
            for (int i = 0; i < MaxClients (); i++)
            {
               Bot *notify = bots.GetBot (i);

               if (notify != nullptr && notify->m_seeEnemyTime + 2.0f < Time () && notify->m_notKilled && notify->m_team == GetTeam (victim) && IsVisible (killer->v.origin, notify->GetEntity ()) && IsNullEntity (notify->m_enemy) && GetTeam (killer) != GetTeam (victim))
               {
                  notify->m_actualReactionTime = 0.0f;
                  notify->m_seeEnemyTime = Time ();
                  notify->m_enemy = killer;
                  notify->m_lastEnemy = killer;
                  notify->m_lastEnemyOrigin = killer->v.origin;
               }
            }

            Bot *notify = bots.GetBot (killer);

            // is this message about a bot who killed somebody?
            if (notify != nullptr)
               notify->m_lastVictim = victim;

            else // did a human kill a bot on his team?
            {
               Bot *target = bots.GetBot (victim);

               if (target != nullptr)
               {
                  if (GetTeam (killer) == GetTeam (victim))
                     target->m_voteKickIndex = killerIndex;

                  target->m_notKilled = false;
               }
            }
         }
         break;
      }
      break;

   case NETMSG_SCREENFADE: // this message gets sent when the screen fades (flashbang)
      switch (m_msgBlock.state)
      {
      case 3:
         r = byteVal;
         break;

      case 4:
         g = byteVal;
         break;

      case 5:
         b = byteVal;
         break;

      case 6:
         bot->TakeBlinded (r, g, b, byteVal);
         break;
      }
      break;

   case NETMSG_HLTV: // round restart in steam cs
      switch (m_msgBlock.state)
      {
      case 0:
         numPlayers = intVal;
         break;

      case 1:
         if (numPlayers == 0 && intVal == 0)
            RoundInit ();
         break;
      }
      break;


   case NETMSG_TEXTMSG:
      if (m_msgBlock.state == 1)
      {
         if (FStrEq (strVal, "#CTs_Win") ||
            FStrEq (strVal, "#Bomb_Defused") ||
            FStrEq (strVal, "#Terrorists_Win") ||
            FStrEq (strVal, "#Round_Draw") ||
            FStrEq (strVal, "#All_Hostages_Rescued") ||
            FStrEq (strVal, "#Target_Saved") ||
            FStrEq (strVal, "#Hostages_Not_Rescued") ||
            FStrEq (strVal, "#Terrorists_Not_Escaped") ||
            FStrEq (strVal, "#VIP_Not_Escaped") ||
            FStrEq (strVal, "#Escaping_Terrorists_Neutralized") ||
            FStrEq (strVal, "#VIP_Assassinated") ||
            FStrEq (strVal, "#VIP_Escaped") ||
            FStrEq (strVal, "#Terrorists_Escaped") ||
            FStrEq (strVal, "#CTs_PreventEscape") ||
            FStrEq (strVal, "#Target_Bombed") ||
            FStrEq (strVal, "#Game_Commencing") ||
            FStrEq (strVal, "#Game_will_restart_in"))
         {
            g_roundEnded = true;

            if (FStrEq (strVal, "#Game_Commencing"))
               g_gameWelcomeSent = true;

            if (FStrEq (strVal, "#CTs_Win"))
            {
               bots.SetLastWinner (CT); // update last winner for economics

               if (yb_communication_type.GetInt () == 2)
               {
                  Bot *notify = bots.FindOneValidAliveBot ();

                  if (notify != nullptr && notify->m_notKilled)
                     notify->HandleChatterMessage (strVal);
               }
            }

            if (FStrEq (strVal, "#Game_will_restart_in"))
            {
               bots.CheckTeamEconomics (CT, true);
               bots.CheckTeamEconomics (TERRORIST, true);
            }

            if (FStrEq (strVal, "#Terrorists_Win"))
            {
               bots.SetLastWinner (TERRORIST); // update last winner for economics

               if (yb_communication_type.GetInt () == 2)
               {
                  Bot *notify = bots.FindOneValidAliveBot ();

                  if (notify != nullptr && notify->m_notKilled)
                     notify->HandleChatterMessage (strVal);
               }
            }
            waypoints.SetBombPosition (true);
         }
         else if (!g_bombPlanted && FStrEq (strVal, "#Bomb_Planted"))
         {
            g_bombPlanted = g_bombSayString = true;
            g_timeBombPlanted = Time ();

            for (int i = 0; i < MaxClients (); i++)
            {
               Bot *notify = bots.GetBot (i);

               if (notify != nullptr && notify->m_notKilled)
               {
                  notify->DeleteSearchNodes ();
                  notify->ResetTasks ();

                  if (yb_communication_type.GetInt () == 2 && Random.Int (0, 100) < 75 && notify->m_team == CT)
                     notify->ChatterMessage (Chatter_WhereIsTheBomb);
               }
            }
            waypoints.SetBombPosition ();
         }
         else if (bot != nullptr && FStrEq (strVal, "#Switch_To_BurstFire"))
            bot->m_weaponBurstMode = BM_ON;
         else if (bot != nullptr && FStrEq (strVal, "#Switch_To_SemiAuto"))
            bot->m_weaponBurstMode = BM_OFF;
      }
      break;

   case NETMSG_SCOREINFO:
      switch (m_msgBlock.state)
      {
      case 0:
         playerIndex = intVal;
         break;

      case 4:
         if (playerIndex >= 0 && playerIndex <= MaxClients ())
         {
#ifndef XASH_CSDM
            Client &client = g_clients[playerIndex - 1];

            if (intVal == 1)
               client.team2 = TERRORIST;
            else if (intVal == 2)
               client.team2 = CT;
            else
               client.team2 = SPECTATOR;

            if (yb_csdm_mode.GetInt () == 2)
               client.team = playerIndex;
            else
               client.team = client.team2;
#endif
         }
         break;
      }
      break;

   case NETMSG_BARTIME:
      if (m_msgBlock.state == 0)
      {
         if (intVal > 0)
            bot->m_hasProgressBar = true; // the progress bar on a hud
         else if (intVal == 0)
            bot->m_hasProgressBar = false; // no progress bar or disappeared
      }
      break;

   default:
      AddLogEntry (true, LL_FATAL, "Network message handler error. Call to unrecognized message id (%d).\n", m_msgBlock.msg);
   }
   m_msgBlock.state++; // and finally update network message state
}

ConVar::ConVar (const char *name, const char *initval, VarType type, bool regMissing) : m_eptr (nullptr)
{
   engine.PushVariableToStack (name, initval, type, regMissing, this);
}
