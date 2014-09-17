//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_listenserver_welcome ("yb_listenserver_welcome", "1");

ConVar mp_roundtime ("mp_roundtime", NULL, VT_NOREGISTER);
ConVar mp_freezetime ("mp_freezetime", NULL, VT_NOREGISTER);

void TraceLine (const Vector &start, const Vector &end, bool ignoreMonsters, bool ignoreGlass, edict_t *ignoreEntity, TraceResult *ptr)
{
   // this function traces a line dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or false), and stops
   // at the first obstacle encountered, returning the results of the trace in the TraceResult structure
   // ptr. Such results are (amongst others) the distance traced, the hit surface, the hit plane
   // vector normal, etc. See the TraceResult structure for details. This function allows to specify
   // whether the trace starts "inside" an entity's polygonal model, and if so, to specify that entity
   // in ignoreEntity in order to ignore it as a possible obstacle.
   // this is an overloaded prototype to add IGNORE_GLASS in the same way as IGNORE_MONSTERS work.

   (*g_engfuncs.pfnTraceLine) (start, end, (ignoreMonsters ? TRUE : FALSE) | (ignoreGlass ? 0x100 : 0), ignoreEntity, ptr);
}

void TraceLine (const Vector &start, const Vector &end, bool ignoreMonsters, edict_t *ignoreEntity, TraceResult *ptr)
{
   // this function traces a line dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or false), and stops
   // at the first obstacle encountered, returning the results of the trace in the TraceResult structure
   // ptr. Such results are (amongst others) the distance traced, the hit surface, the hit plane
   // vector normal, etc. See the TraceResult structure for details. This function allows to specify
   // whether the trace starts "inside" an entity's polygonal model, and if so, to specify that entity
   // in ignoreEntity in order to ignore it as a possible obstacle.

   (*g_engfuncs.pfnTraceLine) (start, end, ignoreMonsters ? TRUE : FALSE, ignoreEntity, ptr);
}

void TraceHull (const Vector &start, const Vector &end, bool ignoreMonsters, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr)
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

   (*g_engfuncs.pfnTraceHull) (start, end, ignoreMonsters ? TRUE : FALSE, hullNumber, ignoreEntity, ptr);
}

uint16 FixedUnsigned16 (float value, float scale)
{
   int output = (static_cast <int> (value * scale));

   if (output < 0)
      output = 0;

   if (output > 0xffff)
      output = 0xffff;

   return static_cast <uint16> (output);
}

short FixedSigned16 (float value, float scale)
{
   int output = (static_cast <int> (value * scale));

   if (output > 32767)
      output = 32767;

   if (output < -32768)
      output = -32768;

   return static_cast <short> (output);
}

char *FormatBuffer (char *format, ...)
{
   va_list ap;
   static char staticBuffer[3072];

   va_start (ap, format);
   vsprintf (staticBuffer, format, ap);
   va_end (ap);

   return &staticBuffer[0];
}

bool IsAlive (edict_t *ent)
{
   if (FNullEnt (ent))
      return false; // reliability check

   return (ent->v.deadflag == DEAD_NO) && (ent->v.health > 0) && (ent->v.movetype != MOVETYPE_NOCLIP);
}

float GetShootingConeDeviation (edict_t *ent, Vector *position)
{
   const Vector &dir = (*position - (ent->v.origin + ent->v.view_ofs)).Normalize ();
   MakeVectors (ent->v.v_angle);

   // he's facing it, he meant it
   return g_pGlobals->v_forward | dir;
}

bool IsInViewCone (Vector origin, edict_t *ent)
{
   MakeVectors (ent->v.v_angle);

   if (((origin - (ent->v.origin + ent->v.view_ofs)).Normalize () | g_pGlobals->v_forward) >= cosf (((ent->v.fov > 0 ? ent->v.fov : 90.0) / 2) * Math::MATH_PI / 180.0))
      return true;

   return false;
}

bool IsVisible (const Vector &origin, edict_t *ent)
{
   if (FNullEnt (ent))
      return false;

   TraceResult tr;
   TraceLine (ent->v.origin + ent->v.view_ofs, origin, true, true, ent, &tr);

   if (tr.flFraction != 1.0)
      return false; // line of sight is not established

   return true; // line of sight is valid.
}

Vector GetEntityOrigin (edict_t *ent)
{
   // this expanded function returns the vector origin of a bounded entity, assuming that any
   // entity that has a bounding box has its center at the center of the bounding box itself.

   if (FNullEnt (ent))
      return nullvec;

   if (ent->v.origin == nullvec)
      return ent->v.absmin + (ent->v.size * 0.5);

   return ent->v.origin;
}

void DisplayMenuToClient (edict_t *ent, MenuText *menu)
{
   if (!IsValidPlayer (ent))
      return;

   int clientIndex = IndexOfEntity (ent) - 1;

   if (menu != NULL)
   {
      String tempText = String (menu->menuText);
      tempText.Replace ("\v", "\n");

      char *text = g_localizer->TranslateInput (tempText.ToString ());
      tempText = String (text);

      // make menu looks best
      for (int i = 0; i <= 9; i++)
         tempText.Replace (FormatBuffer ("%d.", i), FormatBuffer ("\\r%d.\\w", i));

      text = (char *) tempText.ToString ();

      while (strlen (text) >= 64)
      {
         MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, g_netMsg->GetId (NETMSG_SHOWMENU), NULL, ent);
            WRITE_SHORT (menu->validSlots);
            WRITE_CHAR (-1);
            WRITE_BYTE (1);

         for (int i = 0; i <= 63; i++)
            WRITE_CHAR (text[i]);

         MESSAGE_END ();

         text += 64;
      }

      MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, g_netMsg->GetId (NETMSG_SHOWMENU), NULL, ent);
         WRITE_SHORT (menu->validSlots);
         WRITE_CHAR (-1);
         WRITE_BYTE (0);
         WRITE_STRING (text);
      MESSAGE_END();

      g_clients[clientIndex].menu = menu;
   }
   else
   {
      MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, g_netMsg->GetId (NETMSG_SHOWMENU), NULL, ent);
         WRITE_SHORT (0);
         WRITE_CHAR (0);
         WRITE_BYTE (0);
         WRITE_STRING ("");
      MESSAGE_END();

     g_clients[clientIndex].menu = NULL;
   }
   CLIENT_COMMAND (ent, "speak \"player/geiger1\"\n"); // Stops others from hearing menu sounds..
}

void DecalTrace (entvars_t *pev, TraceResult *trace, int logotypeIndex)
{
   // this function draw spraypaint depending on the tracing results.

   static Array <String> logotypes;

   if (logotypes.IsEmpty ())
      logotypes = String ("{biohaz;{graf004;{graf005;{lambda06;{target;{hand1").Split (";");

   int entityIndex = -1, message = TE_DECAL;
   int decalIndex = (*g_engfuncs.pfnDecalIndex) (logotypes[logotypeIndex].ToString ());

   if (decalIndex < 0)
      decalIndex = (*g_engfuncs.pfnDecalIndex) ("{lambda06");

   if (trace->flFraction == 1.0)
      return;

   if (!FNullEnt (trace->pHit))
   {
      if (trace->pHit->v.solid == SOLID_BSP || trace->pHit->v.movetype == MOVETYPE_PUSHSTEP)
         entityIndex = IndexOfEntity (trace->pHit);
      else
         return;
   }
   else
      entityIndex = 0;

   if (entityIndex != 0)
   {
      if (decalIndex > 255)
      {
         message = TE_DECALHIGH;
         decalIndex -= 256;
      }
   }
   else
   {
      message = TE_WORLDDECAL;

      if (decalIndex > 255)
      {
         message = TE_WORLDDECALHIGH;
         decalIndex -= 256;
      }
   }

   if (logotypes[logotypeIndex].Contains ("{"))
   {
      MESSAGE_BEGIN (MSG_BROADCAST, SVC_TEMPENTITY);
         WRITE_BYTE (TE_PLAYERDECAL);
         WRITE_BYTE (IndexOfEntity (ENT (pev)));
         WRITE_COORD (trace->vecEndPos.x);
         WRITE_COORD (trace->vecEndPos.y);
         WRITE_COORD (trace->vecEndPos.z);
         WRITE_SHORT (static_cast <short> (IndexOfEntity (trace->pHit)));
         WRITE_BYTE (decalIndex);
      MESSAGE_END ();
   }
   else
   {
      MESSAGE_BEGIN (MSG_BROADCAST, SVC_TEMPENTITY);
         WRITE_BYTE (message);
         WRITE_COORD (trace->vecEndPos.x);
         WRITE_COORD (trace->vecEndPos.y);
         WRITE_COORD (trace->vecEndPos.z);
         WRITE_BYTE (decalIndex);

      if (entityIndex)
         WRITE_SHORT (entityIndex);

      MESSAGE_END();
   }
}

void FreeLibraryMemory (void)
{
   // this function free's all allocated memory
   g_waypoint->Init (); // frees waypoint data

   if (g_experienceData != NULL)
      delete [] g_experienceData;

   g_experienceData = NULL;
}

void FakeClientCommand (edict_t *fakeClient, const char *format, ...)
{
   // the purpose of this function is to provide fakeclients (bots) with the same client
   // command-scripting advantages (putting multiple commands in one line between semicolons)
   // as real players. It is an improved version of botman's FakeClientCommand, in which you
   // supply directly the whole string as if you were typing it in the bot's "console". It
   // is supposed to work exactly like the pfnClientCommand (server-sided client command).

   va_list ap;
   static char command[256];
   int start, stop, i, index, stringIndex = 0;

   if (FNullEnt (fakeClient))
      return; // reliability check

   // concatenate all the arguments in one string
   va_start (ap, format);
   _vsnprintf (command, sizeof (command), format, ap);
   va_end (ap);

   if (IsNullString (command))
      return; // if nothing in the command buffer, return

   g_isFakeCommand = true; // set the "fakeclient command" flag
   int length = strlen (command); // get the total length of the command string

   // process all individual commands (separated by a semicolon) one each a time
   while (stringIndex < length)
   {
      start = stringIndex; // save field start position (first character)

      while (stringIndex < length && command[stringIndex] != ';')
         stringIndex++; // reach end of field

      if (command[stringIndex - 1] == '\n')
         stop = stringIndex - 2; // discard any trailing '\n' if needed
      else
         stop = stringIndex - 1; // save field stop position (last character before semicolon or end)

      for (i = start; i <= stop; i++)
         g_fakeArgv[i - start] = command[i]; // store the field value in the g_fakeArgv global string

      g_fakeArgv[i - start] = 0; // terminate the string
      stringIndex++; // move the overall string index one step further to bypass the semicolon

      index = 0;
      g_fakeArgc = 0; // let's now parse that command and count the different arguments

      // count the number of arguments
      while (index < i - start)
      {
         while (index < i - start && g_fakeArgv[index] == ' ')
            index++; // ignore spaces

         // is this field a group of words between quotes or a single word ?
         if (g_fakeArgv[index] == '"')
         {
            index++; // move one step further to bypass the quote

            while (index < i - start && g_fakeArgv[index] != '"')
               index++; // reach end of field

            index++; // move one step further to bypass the quote
         }
         else
            while (index < i - start && g_fakeArgv[index] != ' ')
               index++; // this is a single word, so reach the end of field

         g_fakeArgc++; // we have processed one argument more
      }

      // tell now the MOD DLL to execute this ClientCommand...
      MDLL_ClientCommand (fakeClient);
   }

   g_fakeArgv[0] = 0; // when it's done, reset the g_fakeArgv field
   g_isFakeCommand = false; // reset the "fakeclient command" flag
   g_fakeArgc = 0; // and the argument count
}

const char *GetField (const char *string, int fieldId, bool endLine)
{
   // This function gets and returns a particuliar field in a string where several szFields are
   // concatenated. Fields can be words, or groups of words between quotes ; separators may be
   // white space or tabs. A purpose of this function is to provide bots with the same Cmd_Argv
   // convenience the engine provides to real clients. This way the handling of real client
   // commands and bot client commands is exactly the same, just have a look in engine.cpp
   // for the hooking of pfnCmd_Argc, pfnCmd_Args and pfnCmd_Argv, which redirects the call
   // either to the actual engine functions (when the caller is a real client), either on
   // our function here, which does the same thing, when the caller is a bot.

   static char field[256];

   // reset the string
   memset (field, 0, sizeof (field));

   int length, i, index = 0, fieldCount = 0, start, stop;

   field[0] = 0; // reset field
   length = strlen (string); // get length of string

   // while we have not reached end of line
   while (index < length && fieldCount <= fieldId)
   {
      while (index < length && (string[index] == ' ' || string[index] == '\t'))
         index++; // ignore spaces or tabs

      // is this field multi-word between quotes or single word ?
      if (string[index] == '"')
      {
         index++; // move one step further to bypass the quote
         start = index; // save field start position

         while ((index < length) && (string[index] != '"'))
            index++; // reach end of field

         stop = index - 1; // save field stop position
         index++; // move one step further to bypass the quote
      }
      else
      {
         start = index; // save field start position

         while (index < length && (string[index] != ' ' && string[index] != '\t'))
            index++; // reach end of field

         stop = index - 1; // save field stop position
      }

      // is this field we just processed the wanted one ?
      if (fieldCount == fieldId)
      {
         for (i = start; i <= stop; i++)
            field[i - start] = string[i]; // store the field value in a string

         field[i - start] = 0; // terminate the string
         break; // and stop parsing
      }
      fieldCount++; // we have parsed one field more
   }

   if (endLine)
      field[strlen (field) - 1] = 0;

   strtrim (field);

   return (&field[0]); // returns the wanted field
}

void strtrim (char *string)
{
   char *ptr = string;

   int length = 0, toggleFlag = 0, increment = 0;
   int i = 0;

   while (*ptr++)
      length++;

   for (i = length - 1; i >= 0; i--)
   {
#if defined (PLATFORM_WIN32)
      if (!iswspace (string[i]))
#else
      if (!isspace (string[i]))
#endif
         break;
      else
      {
         string[i] = 0;
         length--;
      }
   }

   for (i = 0; i < length; i++)
   {
#if defined (PLATFORM_WIN32)
      if (iswspace (string[i]) && !toggleFlag) // win32 crash fx
#else
      if (isspace (string[i]) && !toggleFlag)
#endif
      {
         increment++;

         if (increment + i < length)
            string[i] = string[increment + i];
      }
      else
      {
         if (!toggleFlag)
            toggleFlag = 1;

         if (increment)
            string[i] = string[increment + i];
      }
   }
   string[length] = 0;
}

const char *GetModName (void)
{
   static char modName[256];

   GET_GAME_DIR (modName); // ask the engine for the MOD directory path
   int length = strlen (modName); // get the length of the returned string

   // format the returned string to get the last directory name
   int stop = length - 1;
   while ((modName[stop] == '\\' || modName[stop] == '/') && stop > 0)
      stop--; // shift back any trailing separator

   int start = stop;
   while (modName[start] != '\\' && modName[start] != '/' && start > 0)
      start--; // shift back to the start of the last subdirectory name

   if (modName[start] == '\\' || modName[start] == '/')
      start++; // if we reached a separator, step over it

   // now copy the formatted string back onto itself character per character
   for (length = start; length <= stop; length++)
      modName[length - start] = modName[length];

   modName[length - start] = 0; // terminate the string

   return &modName[0];
}

// Create a directory tree
void CreatePath (char *path)
{
   for (char *ofs = path + 1 ; *ofs ; ofs++)
   {
      if (*ofs == '/')
      {
         // create the directory
         *ofs = 0;
#ifdef PLATFORM_WIN32
         mkdir (path);
#else
         mkdir (path, 0777);
#endif
         *ofs = '/';
      }
   }
#ifdef PLATFORM_WIN32
   mkdir (path);
#else
   mkdir (path, 0777);
#endif
}

void DrawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life)
{
   // this function draws a line visible from the client side of the player whose player entity
   // is pointed to by ent, from the vector location start to the vector location end,
   // which is supposed to last life tenths seconds, and having the color defined by RGB.

   if (!IsValidPlayer (ent))
      return; // reliability check

   MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, ent);
      WRITE_BYTE (TE_BEAMPOINTS);
      WRITE_COORD (start.x);
      WRITE_COORD (start.y);
      WRITE_COORD (start.z);
      WRITE_COORD (end.x);
      WRITE_COORD (end.y);
      WRITE_COORD (end.z);
      WRITE_SHORT (g_modelIndexLaser);
      WRITE_BYTE (0); // framestart
      WRITE_BYTE (10); // framerate
      WRITE_BYTE (life); // life in 0.1's
      WRITE_BYTE (width); // width
      WRITE_BYTE (noise);  // noise

      WRITE_BYTE (red);   // r, g, b
      WRITE_BYTE (green);   // r, g, b
      WRITE_BYTE (blue);   // r, g, b

      WRITE_BYTE (brightness);   // brightness
      WRITE_BYTE (speed);    // speed
   MESSAGE_END ();
}

void DrawArrow (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life)
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
      WRITE_SHORT (g_modelIndexArrow);
      WRITE_BYTE (0); // framestart
      WRITE_BYTE (10); // framerate
      WRITE_BYTE (life); // life in 0.1's
      WRITE_BYTE (width); // width
      WRITE_BYTE (noise);  // noise

      WRITE_BYTE (red);   // r, g, b
      WRITE_BYTE (green);   // r, g, b
      WRITE_BYTE (blue);   // r, g, b

      WRITE_BYTE (brightness);   // brightness
      WRITE_BYTE (speed);    // speed
   MESSAGE_END ();
}

void UpdateGlobalExperienceData (void)
{
   // this function called after each end of the round to update knowledge about most dangerous waypoints for each team.

   // no waypoints, no experience used or waypoints edited or being edited?
   if (g_numWaypoints < 1 || g_waypointsChanged)
      return; // no action

   unsigned short maxDamage; // maximum damage
   unsigned short actDamage; // actual damage

   int bestIndex; // best index to store
   bool recalcKills = false;

   // get the most dangerous waypoint for this position for terrorist team
   for (int i = 0; i < g_numWaypoints; i++)
   {
      maxDamage = 0;
      bestIndex = -1;

      for (int j = 0; j < g_numWaypoints; j++)
      {
         if (i == j)
            continue;

         actDamage = (g_experienceData + (i * g_numWaypoints) + j)->team0Damage;

         if (actDamage > maxDamage)
         {
            maxDamage = actDamage;
            bestIndex = j;
         }
      }

      if (maxDamage > MAX_DAMAGE_VALUE)
         recalcKills = true;

      (g_experienceData + (i * g_numWaypoints) + i)->team0DangerIndex = static_cast <short> (bestIndex);
   }

   // get the most dangerous waypoint for this position for counter-terrorist team
   for (int i = 0; i < g_numWaypoints; i++)
   {
      maxDamage = 0;
      bestIndex = -1;

      for (int j = 0; j < g_numWaypoints; j++)
      {
         if (i == j)
            continue;

         actDamage = (g_experienceData + (i * g_numWaypoints) + j)->team1Damage;

         if (actDamage > maxDamage)
         {
            maxDamage = actDamage;
            bestIndex = j;
         }
      }

      if (maxDamage > MAX_DAMAGE_VALUE)
         recalcKills = true;

     (g_experienceData + (i * g_numWaypoints) + i)->team1DangerIndex = static_cast <short> (bestIndex);
   }

   // adjust values if overflow is about to happen
   if (recalcKills)
   {
      for (int i = 0; i < g_numWaypoints; i++)
      {
         for (int j = 0; j < g_numWaypoints; j++)
         {
            if (i == j)
               continue;

            int clip = (g_experienceData + (i * g_numWaypoints) + j)->team0Damage;
            clip -= static_cast <int> (MAX_DAMAGE_VALUE * 0.5);

            if (clip < 0)
               clip = 0;

            (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = static_cast <unsigned short> (clip);

            clip = (g_experienceData + (i * g_numWaypoints) + j)->team1Damage;
            clip -= static_cast <int> (MAX_DAMAGE_VALUE * 0.5);

            if (clip < 0)
               clip = 0;

            (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = static_cast <unsigned short> (clip);
         }
      }
   }
   g_highestKills++;

   int clip = g_highestDamageT - static_cast <int> (MAX_DAMAGE_VALUE * 0.5);

   if (clip < 1)
      clip = 1;

   g_highestDamageT = clip;

   clip = (int) g_highestDamageCT - static_cast <int> (MAX_DAMAGE_VALUE * 0.5);

   if (clip < 1)
      clip = 1;

   g_highestDamageCT = clip;

   if (g_highestKills == MAX_KILL_HISTORY)
   {
      for (int i = 0; i < g_numWaypoints; i++)
      {
         (g_experienceData + (i * g_numWaypoints) + i)->team0Damage /= static_cast <unsigned short> (GetMaxClients () * 0.5);
         (g_experienceData + (i * g_numWaypoints) + i)->team1Damage /= static_cast <unsigned short> (GetMaxClients () * 0.5);
      }
      g_highestKills = 1;
   }
}

void RoundInit (void)
{
   // this is called at the start of each round

   g_roundEnded = false;

   // check team economics
   g_botManager->CheckTeamEconomics (TEAM_TF);
   g_botManager->CheckTeamEconomics (TEAM_CF);

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (g_botManager->GetBot (i))
         g_botManager->GetBot (i)->NewRound ();

      g_radioSelect[i] = 0;
   }
   g_waypoint->SetBombPosition (true);
   g_waypoint->ClearGoalScore ();

   g_bombSayString = false;
   g_timeBombPlanted = 0.0;
   g_timeNextBombUpdate = 0.0;

   g_leaderChoosen[TEAM_CF] = false;
   g_leaderChoosen[TEAM_TF] =  false;

   g_lastRadioTime[0] = 0.0;
   g_lastRadioTime[1] = 0.0;
   g_botsCanPause = false;

   UpdateGlobalExperienceData (); // update experience data on round start

   // calculate the round mid/end in world time
   g_timeRoundStart = GetWorldTime () + mp_freezetime.GetFloat ();
   g_timeRoundMid = g_timeRoundStart + mp_roundtime.GetFloat () * 60 / 2;
   g_timeRoundEnd = g_timeRoundStart + mp_roundtime.GetFloat () * 60;
}

bool IsWeaponShootingThroughWall (int id)
{
   // returns if weapon can pierce through a wall

   int i = 0;

   while (g_weaponSelect[i].id)
   {
      if (g_weaponSelect[i].id == id)
      {
         if (g_weaponSelect[i].shootsThru)
            return true;

         return false;
      }
      i++;
   }
   return false;
}

int GetTeam (edict_t *ent)
{
   return g_clients[IndexOfEntity (ent) - 1].team;
}

bool IsValidPlayer (edict_t *ent)
{
   if (FNullEnt (ent))
      return false;

   if (ent->v.flags & FL_PROXY)
      return false;

   if ((ent->v.flags & (FL_CLIENT | FL_FAKECLIENT)) || g_botManager->GetBot (ent) != NULL)
      return !IsNullString (STRING (ent->v.netname));

   return false;
}

bool IsValidBot (edict_t *ent)
{
   if (g_botManager->GetBot (ent) != NULL || (!FNullEnt (ent) && (ent->v.flags & FL_FAKECLIENT)))
      return true;

   return false;
}

bool IsDedicatedServer (void)
{
   // return true if server is dedicated server, false otherwise

   return (IS_DEDICATED_SERVER () > 0); // ask engine for this
}

bool TryFileOpen (char *fileName)
{
   // this function tests if a file exists by attempting to open it

   File fp;

   // check if got valid handle
   if (fp.Open (fileName, "rb"))
   {
      fp.Close ();
      return true;
   }
   return false;
}

void HudMessage (edict_t *ent, bool toCenter, Vector rgb, char *format, ...)
{
   if (!IsValidPlayer (ent) || IsValidBot (ent))
      return;

   va_list ap;
   char buffer[1024];

   va_start (ap, format);
   vsprintf (buffer, format, ap);
   va_end (ap);

   MESSAGE_BEGIN (MSG_ONE, SVC_TEMPENTITY, NULL, ent);
      WRITE_BYTE (TE_TEXTMESSAGE);
      WRITE_BYTE (1);
      WRITE_SHORT (FixedSigned16 (-1, 1 << 13));
      WRITE_SHORT (FixedSigned16 (toCenter ? -1 : 0, 1 << 13));
      WRITE_BYTE (2);
      WRITE_BYTE (static_cast <int> (rgb.x));
      WRITE_BYTE (static_cast <int> (rgb.y));
      WRITE_BYTE (static_cast <int> (rgb.z));
      WRITE_BYTE (0);
      WRITE_BYTE (g_randGen.Long (230, 255));
      WRITE_BYTE (g_randGen.Long (230, 255));
      WRITE_BYTE (g_randGen.Long (230, 255));
      WRITE_BYTE (200);
      WRITE_SHORT (FixedUnsigned16 (0.0078125, 1 << 8));
      WRITE_SHORT (FixedUnsigned16 (2, 1 << 8));
      WRITE_SHORT (FixedUnsigned16 (6, 1 << 8));
      WRITE_SHORT (FixedUnsigned16 (0.1, 1 << 8));
      WRITE_STRING (const_cast <const char *> (&buffer[0]));
   MESSAGE_END ();
}

void ServerPrint (const char *format, ...)
{
   va_list ap;
   char string[3072];

   va_start (ap, format);
   vsprintf (string, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   SERVER_PRINT (FormatBuffer ("[%s] %s\n", PRODUCT_LOGTAG, string));
}

void ServerPrintNoTag (const char *format, ...)
{
   va_list ap;
   char string[3072];

   va_start (ap, format);
   vsprintf (string, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   SERVER_PRINT (FormatBuffer ("%s\n", string));
}

void CenterPrint (const char *format, ...)
{
   va_list ap;
   char string[2048];

   va_start (ap, format);
   vsprintf (string, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      ServerPrint (string);
      return;
   }

   MESSAGE_BEGIN (MSG_BROADCAST, g_netMsg->GetId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTCENTER);
      WRITE_STRING (FormatBuffer ("%s\n", string));
   MESSAGE_END ();
}

void ChartPrint (const char *format, ...)
{
   va_list ap;
   char string[2048];

   va_start (ap, format);
   vsprintf (string, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   if (IsDedicatedServer ())
   {
      ServerPrint (string);
      return;
   }
   strcat (string, "\n");

   MESSAGE_BEGIN (MSG_BROADCAST, g_netMsg->GetId (NETMSG_TEXTMSG));
      WRITE_BYTE (HUD_PRINTTALK);
      WRITE_STRING (string);
   MESSAGE_END ();

}

void ClientPrint (edict_t *ent, int dest, const char *format, ...)
{
   va_list ap;
   char string[2048];

   va_start (ap, format);
   vsprintf (string, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   if (FNullEnt (ent) || ent == g_hostEntity)
   {
      if (dest & 0x3ff)
         ServerPrint (string);
      else
         ServerPrintNoTag (string);

      return;
   }
   strcat (string, "\n");

   if (dest & 0x3ff)
      (*g_engfuncs.pfnClientPrintf) (ent, static_cast <PRINT_TYPE> (dest &= ~0x3ff), FormatBuffer ("[YAPB] %s", string));
   else
      (*g_engfuncs.pfnClientPrintf) (ent, static_cast <PRINT_TYPE> (dest), string);

}

void ServerCommand (const char *format, ...)
{
   // this function asks the engine to execute a server command

   va_list ap;
   static char string[1024];

   // concatenate all the arguments in one string
   va_start (ap, format);
   vsprintf (string, format, ap);
   va_end (ap);

   SERVER_COMMAND (FormatBuffer ("%s\n", string)); // execute command
}

const char *GetMapName (void)
{
   // this function gets the map name and store it in the map_name global string variable.

   static char mapName[256];
   strcpy (mapName, const_cast <const char *> (g_pGlobals->pStringBase + static_cast <int> (g_pGlobals->mapname)));

   return &mapName[0]; // and return a pointer to it
}

bool OpenConfig (const char *fileName, char *errorIfNotExists, File *outFile, bool languageDependant)
{
   if (outFile->IsValid ())
      outFile->Close ();

   if (languageDependant)
   {
      extern ConVar yb_language;

      if (strcmp (fileName, "lang.cfg") == 0 && strcmp (yb_language.GetString (), "en") == 0)
         return false;

      char *languageDependantConfigFile = FormatBuffer ("%s/addons/yapb/config/language/%s_%s", GetModName (), yb_language.GetString (), fileName);

      // check is file is exists for this language
      if (TryFileOpen (languageDependantConfigFile))
         outFile->Open (languageDependantConfigFile, "rt");
      else
         outFile->Open (FormatBuffer ("%s/addons/yapb/config/language/en_%s", GetModName (), fileName), "rt");
   }
   else
      outFile->Open (FormatBuffer ("%s/addons/yapb/config/%s", GetModName (), fileName), "rt");

   if (!outFile->IsValid ())
   {
      AddLogEntry (true, LL_ERROR, errorIfNotExists);
      return false;
   }
   return true;
}

const char *GetWaypointDir (void)
{
   return FormatBuffer ("%s/addons/yapb/wptdefault/", GetModName ());
}

void RegisterCommand (char *command, void funcPtr (void))
{
   // this function tells the engine that a new server command is being declared, in addition
   // to the standard ones, whose name is command_name. The engine is thus supposed to be aware
   // that for every "command_name" server command it receives, it should call the function
   // pointed to by "function" in order to handle it.

   if (IsNullString (command) || funcPtr == NULL)
      return; // reliability check

   REG_SVR_COMMAND (command, funcPtr); // ask the engine to register this new command
}

void CheckWelcomeMessage (void)
{
   // the purpose of this function, is  to send quick welcome message, to the listenserver entity.

   static bool isReceived = false;
   static float receiveTime = 0.0;

   if (isReceived || !yb_listenserver_welcome.GetBool ())
      return;

   Array <String> sentences;

   // add default messages
   sentences.Push ("hello user,communication is acquired");
   sentences.Push ("your presence is acknowledged");
   sentences.Push ("high man, your in command now");
   sentences.Push ("blast your hostile for good");
   sentences.Push ("high man, kill some idiot here");
   sentences.Push ("is there a doctor in the area");
   sentences.Push ("warning, experimental materials detected");
   sentences.Push ("high amigo, shoot some but");
   sentences.Push ("attention, hours of work software, detected");
   sentences.Push ("time for some bad ass explosion");
   sentences.Push ("bad ass son of a breach device activated");
   sentences.Push ("high, do not question this great service");
   sentences.Push ("engine is operative, hello and goodbye");
   sentences.Push ("high amigo, your administration has been great last day");
   sentences.Push ("attention, expect experimental armed hostile presence");
   sentences.Push ("warning, medical attention required");

   if (IsAlive (g_hostEntity) && !isReceived && receiveTime < 1.0 && (g_numWaypoints > 0 ? g_isCommencing : true))
      receiveTime = GetWorldTime () + 4.0; // receive welcome message in four seconds after game has commencing

   if (receiveTime > 0.0 && receiveTime < GetWorldTime () && !isReceived && (g_numWaypoints > 0 ? g_isCommencing : true))
   {
      ServerCommand ("speak \"%s\"", const_cast <char *> (sentences.GetRandomElement ().ToString ()));

      ChartPrint ("----- YaPB v%s (Build: %u), {%s}, (c) 2014, by %s -----", PRODUCT_VERSION, GenerateBuildNumber (), PRODUCT_DATE, PRODUCT_AUTHOR);
      HudMessage (g_hostEntity, true, Vector (g_randGen.Long (33, 255), g_randGen.Long (33, 255), g_randGen.Long (33, 255)), "\nServer is running YaPB v%s (Build: %u)\nDeveloped by %s\n\n%s", PRODUCT_VERSION, GenerateBuildNumber (), PRODUCT_AUTHOR, g_waypoint->GetInfo ());

      receiveTime = 0.0;
      isReceived = true;
   }
}

void DetectCSVersion (void)
{
   byte *detection = NULL;
   const char *const infoBuffer = "Game Registered: CS %s (0x%d)";

   // switch version returned by dll loader
   switch (g_gameVersion)
   {
   // counter-strike 1.x, WON ofcourse
   case CSV_OLD:
      ServerPrint (infoBuffer, "1.x (WON)", sizeof (Bot));
      break;

   // counter-strike 1.6 or higher (plus detects for non-steam versions of 1.5)
   case CSV_STEAM:
      detection = (*g_engfuncs.pfnLoadFileForMe) ("events/galil.sc", NULL);

      if (detection != NULL)
      {
         ServerPrint (infoBuffer, "1.6 (Steam)", sizeof (Bot));
         g_gameVersion = CSV_STEAM; // just to be sure
      }
      else if (detection == NULL)
      {
         ServerPrint (infoBuffer, " <= 1.5 (WON)", sizeof (Bot));
         g_gameVersion = CSV_OLD; // reset it to WON
      }

      // if we have loaded the file free it
      if (detection != NULL)
         (*g_engfuncs.pfnFreeFile) (detection);
      break;

   // counter-strike cz
   case CSV_CZERO:
      ServerPrint (infoBuffer, "CZ (Steam)", sizeof (Bot));
      break;
   }
}

void PlaySound (edict_t *ent, const char *name)
{
   // TODO: make this obsolete
   EMIT_SOUND_DYN2 (ent, CHAN_WEAPON, name, 1.0, ATTN_NORM, 0, 100);

   return;
}

float GetWaveLength (const char *fileName)
{
   WavHeader waveHdr;
   memset (&waveHdr, 0, sizeof (waveHdr));

   extern ConVar yb_chatter_path;

   File fp (FormatBuffer ("%s/%s/%s.wav", GetModName (), yb_chatter_path.GetString (), fileName), "rb");

   // we're got valid handle?
   if (!fp.IsValid ())
      return 0;

   if (fp.Read (&waveHdr, sizeof (WavHeader)) == 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has wrong or unsupported format", fileName);
      return 0;
   }

   if (strncmp (waveHdr.chunkID, "WAVE", 4) != 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has wrong wave chunk id", fileName);
      return 0;
   }
   fp.Close ();

   if (waveHdr.dataChunkLength == 0)
   {
      AddLogEntry (true, LL_ERROR, "Wave File %s - has zero length!", fileName);
      return 0;
   }

   char ch[32];
   sprintf (ch, "0.%u", static_cast <unsigned int> (waveHdr.dataChunkLength));

   float secondLength = static_cast <float> (waveHdr.dataChunkLength / waveHdr.bytesPerSecond);
   float milliSecondLength = atof (ch);

   return (secondLength == 0.0 ? milliSecondLength : secondLength);
}

void AddLogEntry (bool outputToConsole, int logLevel, const char *format, ...)
{
   // this function logs a message to the message log file root directory.

   va_list ap;
   char buffer[512] = {0, }, levelString[32] = {0, }, logLine[1024] = {0, };

   va_start (ap, format);
   vsprintf (buffer, g_localizer->TranslateInput (format), ap);
   va_end (ap);

   switch (logLevel)
   {
   case LL_DEFAULT:
      strcpy (levelString, "Log: ");
      break;

   case LL_WARNING:
      strcpy (levelString, "Warning: ");
      break;

   case LL_ERROR:
      strcpy (levelString, "Error: ");
      break;

   case LL_FATAL:
      strcpy (levelString, "Critical: ");
      break;
   }

   if (outputToConsole)
      ServerPrintNoTag ("%s%s", levelString, buffer);

   // now check if logging disabled
   if (!(logLevel & LL_IGNORE))
   {
      extern ConVar yb_debug;

      if (logLevel == LL_DEFAULT && yb_debug.GetInt () < 3)
         return; // no log, default logging is disabled

      if (logLevel == LL_WARNING && yb_debug.GetInt () < 2)
         return; // no log, warning logging is disabled

      if (logLevel == LL_ERROR && yb_debug.GetInt () < 1)
         return; // no log, error logging is disabled
   }

   // open file in a standard stream
   File fp ("yapb.txt", "at");

   // check if we got a valid handle
   if (!fp.IsValid ())
      return;

   time_t tickTime = time (&tickTime);
   tm *time = localtime (&tickTime);

   sprintf (logLine, "[%02d:%02d:%02d] %s%s", time->tm_hour, time->tm_min, time->tm_sec, levelString, buffer);

   fp.Printf ("%s\n", logLine);
   fp.Close ();

   if (logLevel == LL_FATAL)
   {
#if defined (PLATFORM_WIN32)
      MessageBoxA (GetActiveWindow (), buffer, "YaPB Error", MB_ICONSTOP);
#else
      printf (buffer);
#endif
      FreeLibraryMemory ();

#if defined (PLATFORM_WIN32)
      _exit (1);
#else
      exit (1);
#endif
   }
}

char *Localizer::TranslateInput (const char *input)
{
   if (IsDedicatedServer ())
      return const_cast <char *> (&input[0]);

   static char string[1024];
   const char *ptr = input + strlen (input) - 1;

   while (ptr > input && *ptr == '\n')
      ptr--;

   if (ptr != input)
      ptr++;

   strncpy (string, input, 1024);
   strtrim (string);

   IterateArray (m_langTab, i)
   {
      if (strcmp (string, m_langTab[i].original) == 0)
      {
         strncpy (string, m_langTab[i].translated, 1024);

         if (ptr != input)
            strncat (string, ptr, 1024 - 1 - strlen (string));

         return &string[0];
      }
   }
   return const_cast <char *> (&input[0]); // nothing found
}

uint32 RandGen::GetRandomBits (void) 
{
   // generate next number
   uint32 x = _lrotl (m_historyBuffer[m_bufferIndex1][0], R1) + m_historyBuffer[m_bufferIndex2][0];
   uint32 y = _lrotl (m_historyBuffer[m_bufferIndex1][1], R2) + m_historyBuffer[m_bufferIndex2][1];

   m_historyBuffer[m_bufferIndex1][0] = y; 
   m_historyBuffer[m_bufferIndex1][1] = x;

   // rotate list pointers
   if (m_bufferIndex1-- < 0) 
      m_bufferIndex1 = KK - 1;

   if (m_bufferIndex2-- < 0) 
      m_bufferIndex2 = KK - 1;

   // perform self-test
   if (m_historyBuffer[m_bufferIndex1][0] == m_selfTestBuffer[0][0] && memcmp (m_historyBuffer, m_selfTestBuffer[KK - m_bufferIndex1], 2 * KK * sizeof (uint32)) == 0) 
   {
      // self-test failed
      if ((m_bufferIndex2 + KK - m_bufferIndex1) % KK != JJ)
         AddLogEntry (false, LL_FATAL, "Random number generator could not be initialized.");
      else
         AddLogEntry (false, LL_FATAL, "Random number generator returned to initial state.");
   }
   m_randomBits[0] = y;
   m_randomBits[1] = x;

   return y;
}

long double RandGen::Random (void) 
{
   return static_cast <double> (GetRandomBits () * (1.0 / (static_cast <double> (static_cast <uint32> (-1L)) + 1.0)));
}

int RandGen::Long (int low, int high)
{
   // this function returns a random integer number between (and including) the starting and
   // ending values passed by parameters from and to.

   int interval = high - low + 1;

   if (interval <= 0)
      return low;

   int truncate = static_cast <int> (interval * Random ());

   if (truncate >= interval)
      truncate = interval - 1.0;

   return low + truncate;
}

float RandGen::Float (float low, float high)
{
   // this function returns a random floating-point number between (and including) the starting
   // and ending values passed by parameters from and to.

   float interval = high - low;

   if (interval <= 0.0)
      return low;

   float truncate = static_cast <float> (interval * Random ());

   if (truncate >= interval)
      truncate = interval;

   return low + truncate;
}

void RandGen::Initialize (uint32 seed)
{
   // this function initializes the random number generator.

   // make random numbers and put them into the buffer
   for (int i = 0; i < KK; i++) 
   {
      for (int j = 0; j < 2; j++) 
      {
         seed = seed * 2891336453UL + 1;
         m_historyBuffer[i][j] = seed;
      }
   }

   // set exponent of randp
   m_randomBits[2] = 0; 
   m_randomPtr = 1.0;

   // initialize pointers to circular buffer
   m_bufferIndex1 = 0;  
   m_bufferIndex2 = JJ;

   // store state for self-test
   memcpy (m_selfTestBuffer, m_historyBuffer, 2 * KK * sizeof (uint32));
   memcpy (m_selfTestBuffer[KK], m_historyBuffer, 2 * KK * sizeof (uint32));

   // randomize some more
   for (int i = 0; i < 128; i++) 
      GetRandomBits ();
}

bool FindNearestPlayer (void **pvHolder, edict_t *to, float searchDistance, bool sameTeam, bool needBot, bool isAlive, bool needDrawn)
{
   // this function finds nearest to to, player with set of parameters, like his
   // team, live status, search distance etc. if needBot is true, then pvHolder, will
   // be filled with bot pointer, else with edict pointer(!).

   edict_t *ent = NULL, *survive = NULL; // pointer to temporaly & survive entity
   float nearestPlayer = 4096.0; // nearest player

   while (!FNullEnt (ent = FIND_ENTITY_IN_SPHERE (ent, to->v.origin, searchDistance)))
   {
      if (FNullEnt (ent) || !IsValidPlayer (ent) || to == ent)
         continue; // skip invalid players

      if ((sameTeam && GetTeam (ent) != GetTeam (to)) || (isAlive && !IsAlive (ent)) || (needBot && !IsValidBot (ent)) || (needDrawn && (ent->v.effects & EF_NODRAW)))
         continue; // filter players with parameters

      float distance = (ent->v.origin - to->v.origin).GetLength ();

      if (distance < nearestPlayer)
      {
         nearestPlayer = distance;
         survive = ent;
      }
   }

   if (FNullEnt (survive))
      return false; // nothing found

   // fill the holder
   if (needBot)
      *pvHolder = reinterpret_cast <void *> (g_botManager->GetBot (survive));
   else
      *pvHolder = reinterpret_cast <void *> (survive);

   return true;
}

void SoundAttachToThreat (edict_t *ent, const char *sample, float volume)
{
   // this function called by the sound hooking code (in emit_sound) enters the played sound into
   // the array associated with the entity

   if (FNullEnt (ent) || IsNullString (sample))
      return; // reliability check

   Vector origin = GetEntityOrigin (ent);
   int index = IndexOfEntity (ent) - 1;

   if (index < 0 || index >= GetMaxClients ())
   {
      float nearestDistance = FLT_MAX;

      // loop through all players
      for (int i = 0; i < GetMaxClients (); i++)
      {
         if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE))
            continue;

         float distance = (g_clients[i].ent->v.origin - origin).GetLengthSquared ();

         // now find nearest player
         if (distance < nearestDistance)
         {
            index = i;
            nearestDistance = distance;
         }
      }
   }

   if (strncmp ("player/bhit_flesh", sample, 17) == 0 || strncmp ("player/headshot", sample, 15) == 0)
   {
      // hit/fall sound?
      g_clients[index].hearingDistance = 768.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 0.5;
      g_clients[index].maxTimeSoundLasting = 0.5;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("items/gunpickup", sample, 15) == 0)
   {
      // weapon pickup?
      g_clients[index].hearingDistance = 768.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 0.5;
      g_clients[index].maxTimeSoundLasting = 0.5;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("weapons/zoom", sample, 12) == 0)
   {
      // sniper zooming?
      g_clients[index].hearingDistance = 512.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 0.1;
      g_clients[index].maxTimeSoundLasting = 0.1;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("items/9mmclip", sample, 13) == 0)
   {
      // ammo pickup?
      g_clients[index].hearingDistance = 512.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 0.1;
      g_clients[index].maxTimeSoundLasting = 0.1;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("hostage/hos", sample, 11) == 0)
   {
      // CT used hostage?
      g_clients[index].hearingDistance = 1024.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 5.0;
      g_clients[index].maxTimeSoundLasting = 0.5;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("debris/bustmetal", sample, 16) == 0 || strncmp ("debris/bustglass", sample, 16) == 0)
   {
      // broke something?
      g_clients[index].hearingDistance = 1024.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 2.0;
      g_clients[index].maxTimeSoundLasting = 2.0;
      g_clients[index].soundPosition = origin;
   }
   else if (strncmp ("doors/doormove", sample, 14) == 0)
   {
      // someone opened a door
      g_clients[index].hearingDistance = 1024.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 3.0;
      g_clients[index].maxTimeSoundLasting = 3.0;
      g_clients[index].soundPosition = origin;
   }
#if 0
   else if (strncmp ("weapons/reload",  sample, 14) == 0)
   {
      // reloading ?
      g_clients[index].hearingDistance = 512.0 * volume;
      g_clients[index].timeSoundLasting = GetWorldTime () + 0.5;
      g_clients[index].maxTimeSoundLasting = 0.5;
      g_clients[index].soundPosition = origin;
   }
#endif
}

void SoundSimulateUpdate (int playerIndex)
{
   // this function tries to simulate playing of sounds to let the bots hear sounds which aren't
   // captured through server sound hooking

   InternalAssert (playerIndex >= 0);
   InternalAssert (playerIndex < GetMaxClients ());

   if (playerIndex < 0 || playerIndex >= GetMaxClients ())
      return; // reliability check

   edict_t *player = g_clients[playerIndex].ent;

   float velocity = player->v.velocity.GetLength2D ();
   float hearDistance = 0.0;
   float timeSound = 0.0;
   float timeMaxSound = 0.5;

   if (player->v.oldbuttons & IN_ATTACK) // pressed attack button?
   {
      hearDistance = 3072.0;
      timeSound = GetWorldTime () + 0.3;
      timeMaxSound = 0.3;
   }
   else if (player->v.oldbuttons & IN_USE) // pressed used button?
   {
      hearDistance = 512.0;
      timeSound = GetWorldTime () + 0.5;
      timeMaxSound = 0.5;
   }
   else if (player->v.oldbuttons & IN_RELOAD) // pressed reload button?
   {
      hearDistance = 512.0;
      timeSound = GetWorldTime () + 0.5;
      timeMaxSound = 0.5;
   }
   else if (player->v.movetype == MOVETYPE_FLY) // uses ladder?
   {
      if (fabs (player->v.velocity.z) > 50.0)
      {
         hearDistance = 1024.0;
         timeSound = GetWorldTime () + 0.3;
         timeMaxSound = 0.3;
      }
   }
   else
   {
      extern ConVar mp_footsteps;

      if (mp_footsteps.GetBool ())
      {
         // moves fast enough?
         hearDistance = 1280.0 * (velocity / 240);
         timeSound = GetWorldTime () + 0.3;
         timeMaxSound = 0.3;
      }
   }

   if (hearDistance <= 0.0)
      return; // didn't issue sound?

   // some sound already associated
   if (g_clients[playerIndex].timeSoundLasting > GetWorldTime ())
   {
      // new sound louder (bigger range) than old one ?
      if (g_clients[playerIndex].maxTimeSoundLasting <= 0.0)
         g_clients[playerIndex].maxTimeSoundLasting = 0.5;

      if (g_clients[playerIndex].hearingDistance * (g_clients[playerIndex].timeSoundLasting - GetWorldTime ()) / g_clients[playerIndex].maxTimeSoundLasting <= hearDistance)
      {
         // override it with new
         g_clients[playerIndex].hearingDistance = hearDistance;
         g_clients[playerIndex].timeSoundLasting = timeSound;
         g_clients[playerIndex].maxTimeSoundLasting = timeMaxSound;
         g_clients[playerIndex].soundPosition = player->v.origin;
      }
   }
   else
   {
      // just remember it
      g_clients[playerIndex].hearingDistance = hearDistance;
      g_clients[playerIndex].timeSoundLasting = timeSound;
      g_clients[playerIndex].maxTimeSoundLasting = timeMaxSound;
      g_clients[playerIndex].soundPosition = player->v.origin;
   }
}

uint16 GenerateBuildNumber (void)
{
   // this function generates build number from the compiler date macros

   // get compiling date using compiler macros
   const char *date = __DATE__;

   // array of the month names
   const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

   // array of the month days
   byte monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

   int day = 0; // day of the year
   int year = 0; // year
   int i = 0;

   // go through all monthes, and calculate, days since year start
   for (i = 0; i < 11; i++)
   {
      if (strncmp (&date[0], months[i], 3) == 0)
         break; // found current month break

      day += monthDays[i]; // add month days
   }
   day += atoi (&date[4]) - 1; // finally calculate day
   year = atoi (&date[7]) - 2000; // get years since year 2000

   uint16 buildNumber = day + static_cast <int> ((year - 1) * 365.25);

   // if the year is a leap year?
   if ((year % 4) == 0 && i > 1)
      buildNumber += 1; // add one year more

   return buildNumber - 1114;
}

int GetWeaponReturn (bool needString, const char *weaponAlias, int weaponIndex)
{
   // this function returning weapon id from the weapon alias and vice versa.

   // structure definition for weapon tab
   struct WeaponTab_t
   {
      Weapon weaponIndex; // weapon id
      const char *alias; // weapon alias
   };

   // weapon enumeration
   WeaponTab_t weaponTab[] =
   {
      {WEAPON_USP, "usp"}, // HK USP .45 Tactical
      {WEAPON_GLOCK, "glock"}, // Glock18 Select Fire
      {WEAPON_DEAGLE, "deagle"}, // Desert Eagle .50AE
      {WEAPON_P228, "p228"}, // SIG P228
      {WEAPON_ELITE, "elite"}, // Dual Beretta 96G Elite
      {WEAPON_FIVESEVEN, "fn57"}, // FN Five-Seven
      {WEAPON_M3, "m3"}, // Benelli M3 Super90
      {WEAPON_XM1014, "xm1014"}, // Benelli XM1014
      {WEAPON_MP5, "mp5"}, // HK MP5-Navy
      {WEAPON_TMP, "tmp"}, // Steyr Tactical Machine Pistol
      {WEAPON_P90, "p90"}, // FN P90
      {WEAPON_MAC10, "mac10"}, // Ingram MAC-10
      {WEAPON_UMP45, "ump45"}, // HK UMP45
      {WEAPON_AK47, "ak47"}, // Automat Kalashnikov AK-47
      {WEAPON_GALIL, "galil"}, // IMI Galil
      {WEAPON_FAMAS, "famas"}, // GIAT FAMAS
      {WEAPON_SG552, "sg552"}, // Sig SG-552 Commando
      {WEAPON_M4A1, "m4a1"}, // Colt M4A1 Carbine
      {WEAPON_AUG, "aug"}, // Steyr Aug
      {WEAPON_SCOUT, "scout"}, // Steyr Scout
      {WEAPON_AWP, "awp"}, // AI Arctic Warfare/Magnum
      {WEAPON_G3SG1, "g3sg1"}, // HK G3/SG-1 Sniper Rifle
      {WEAPON_SG550, "sg550"}, // Sig SG-550 Sniper
      {WEAPON_M249, "m249"}, // FN M249 Para
      {WEAPON_FLASHBANG, "flash"}, // Concussion Grenade
      {WEAPON_EXPLOSIVE, "hegren"}, // High-Explosive Grenade
      {WEAPON_SMOKE, "sgren"}, // Smoke Grenade
      {WEAPON_ARMOR, "vest"}, // Kevlar Vest
      {WEAPON_ARMORHELM, "vesthelm"}, // Kevlar Vest and Helmet
      {WEAPON_DEFUSER, "defuser"}, // Defuser Kit
      {WEAPON_SHIELD, "shield"}, // Tactical Shield
   };

   // if we need to return the string, find by weapon id
   if (needString && weaponIndex != -1)
   {
      for (int i = 0; i < ARRAYSIZE_HLSDK (weaponTab); i++)
      {
         if (weaponTab[i].weaponIndex == weaponIndex) // is weapon id found?
            return MAKE_STRING (weaponTab[i].alias);
      }
      return MAKE_STRING ("(none)"); // return none
   }

   // else search weapon by name and return weapon id
   for (int i = 0; i < ARRAYSIZE_HLSDK (weaponTab); i++)
   {
      if (strncmp (weaponTab[i].alias, weaponAlias, strlen (weaponTab[i].alias)) == 0)
         return weaponTab[i].weaponIndex;
   }
   return -1; // no weapon was found return -1
}