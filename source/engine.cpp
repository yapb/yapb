//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar sv_skycolor_r ("sv_skycolor_r", nullptr, VT_NOREGISTER);
ConVar sv_skycolor_g ("sv_skycolor_g", nullptr, VT_NOREGISTER);
ConVar sv_skycolor_b ("sv_skycolor_b", nullptr, VT_NOREGISTER);

Engine::Engine (void) {
   m_startEntity = nullptr;
   m_localEntity = nullptr;

   resetMessages ();

   for (int i = 0; i < NETMSG_NUM; i++) {
      m_msgBlock.regMsgs[i] = NETMSG_UNDEFINED;
   }
   m_precached = false;
   m_isBotCommand = false;
   m_argumentCount = 0;

   memset (m_arguments, 0, sizeof (m_arguments));
   memset (m_drawModels, 0, sizeof (m_drawModels));
   memset (m_spawnCount, 0, sizeof (m_spawnCount));

   m_gameFlags = 0;
   m_mapFlags = 0;
   m_slowFrame = 0.0;

   m_cvars.clear ();
}

Engine::~Engine (void) {
   resetMessages ();

   for (int i = 0; i < NETMSG_NUM; i++) {
      m_msgBlock.regMsgs[i] = NETMSG_UNDEFINED;
   }
}

void Engine::precache (void) {
   if (m_precached) {
      return;
   }
   m_precached = true;

   m_drawModels[DRAW_SIMPLE] = engfuncs.pfnPrecacheModel (ENGINE_STR ("sprites/laserbeam.spr"));
   m_drawModels[DRAW_ARROW] = engfuncs.pfnPrecacheModel (ENGINE_STR ("sprites/arrow1.spr"));

   engfuncs.pfnPrecacheSound (ENGINE_STR ("weapons/xbow_hit1.wav")); // waypoint add
   engfuncs.pfnPrecacheSound (ENGINE_STR ("weapons/mine_activate.wav")); // waypoint delete
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_hudoff.wav")); // path add/delete start
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_hudon.wav")); // path add/delete done
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_moveselect.wav")); // path add/delete cancel
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_denyselect.wav")); // path add/delete error

   m_mapFlags = 0; // reset map type as worldspawn is the first entity spawned

   // detect official csbots here, as they causing crash in linkent code when active for some reason
   if (!(engine.is (GAME_LEGACY)) && engfuncs.pfnCVarGetPointer ("bot_stop") != nullptr) {
      m_gameFlags |= GAME_OFFICIAL_CSBOT;
   }
   pushRegStackToEngine (true);
}

void Engine::levelInitialize (void) {
   // this function precaches needed models and initialize class variables

   m_spawnCount[TEAM_COUNTER] = 0;
   m_spawnCount[TEAM_TERRORIST] = 0;
   
   // go thru the all entities on map, and do whatever we're want
   for (int i = 0; i < globals->maxEntities; i++) {

      auto ent = engfuncs.pfnPEntityOfEntIndex (i);

      // only valid entities
      if (engine.isNullEntity (ent) || ent->free || ent->v.classname == 0) {
         continue;
      }
      auto classname = STRING (ent->v.classname);

      if (strcmp (classname, "worldspawn") == 0) {
         m_startEntity = ent;

         // initialize some structures
         bots.initRound ();
      }
      else if (strcmp (classname, "player_weaponstrip") == 0) {
         if ((engine.is (GAME_LEGACY)) && (STRING (ent->v.target))[0] == '\0') {
            ent->v.target = ent->v.targetname = engfuncs.pfnAllocString ("fake");
         }
         else {
            engfuncs.pfnRemoveEntity (ent);
         }
      }
      else if (strcmp (classname, "info_player_start") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/urban/urban.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;

         m_spawnCount[TEAM_COUNTER]++;
      }
      else if (strcmp (classname, "info_player_deathmatch") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/terror/terror.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;

         m_spawnCount[TEAM_TERRORIST]++;
      }

      else if (strcmp (classname, "info_vip_start") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/vip/vip.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;
      }
      else if (strcmp (classname, "func_vip_safetyzone") == 0 || strcmp (classname, "info_vip_safetyzone") == 0) {
         m_mapFlags |= MAP_AS; // assassination map
      }
      else if (strcmp (classname, "hostage_entity") == 0) {
         m_mapFlags |= MAP_CS; // rescue map
      }
      else if (strcmp (classname, "func_bomb_target") == 0 || strcmp (classname, "info_bomb_target") == 0) {
         m_mapFlags |= MAP_DE; // defusion map
      }
      else if (strcmp (classname, "func_escapezone") == 0) {
         m_mapFlags |= MAP_ES;
      }
      else if (strncmp (classname, "func_door", 9) == 0) {
         m_mapFlags |= MAP_HAS_DOORS;
      }
   }

   // next maps doesn't have map-specific entities, so determine it by name
   if (strncmp (engine.getMapName (), "fy_", 3) == 0) {
      m_mapFlags |= MAP_FY;
   }
   else if (strncmp (engine.getMapName (), "ka_", 3) == 0) {
      m_mapFlags |= MAP_KA;
   }
}

void Engine::print (const char *fmt, ...) {
   // this function outputs string into server console

   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), translate (fmt), ap);
   va_end (ap);

   strcat (string, "\n");
   engfuncs.pfnServerPrint (string);
}

void Engine::chatPrint (const char *fmt, ...) {
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), translate (fmt), ap);
   va_end (ap);

   if (isDedicated ()) {
      print (string);
      return;
   }
   strcat (string, "\n");

   MessageWriter (MSG_BROADCAST, getMessageId (NETMSG_TEXTMSG))
      .writeByte (HUD_PRINTTALK)
      .writeString (string);
}

void Engine::centerPrint (const char *fmt, ...) {
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), translate (fmt), ap);
   va_end (ap);

   if (isDedicated ()) {
      print (string);
      return;
   }
   strcat (string, "\n");

   MessageWriter (MSG_BROADCAST, getMessageId (NETMSG_TEXTMSG))
      .writeByte (HUD_PRINTCENTER)
      .writeString (string);
}

void Engine::clientPrint (edict_t *ent, const char *fmt, ...) {
   va_list ap;
   char string[MAX_PRINT_BUFFER];

   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), translate (fmt), ap);
   va_end (ap);

   if (isNullEntity (ent)) {
      print (string);
      return;
   }
   strcat (string, "\n");
   engfuncs.pfnClientPrintf (ent, print_console, string);
}

void Engine::drawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, int red, int green, int blue, int brightness, int speed, int life, DrawLineType type) {
   // this function draws a arrow visible from the client side of the player whose player entity
   // is pointed to by ent, from the vector location start to the vector location end,
   // which is supposed to last life tenths seconds, and having the color defined by RGB.

   if (!util.isPlayer (ent)) {
      return; // reliability check
   }

   MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, Vector::null (), ent)
      .writeByte (TE_BEAMPOINTS)
      .writeCoord (end.x)
      .writeCoord (end.y)
      .writeCoord (end.z)
      .writeCoord (start.x)
      .writeCoord (start.y)
      .writeCoord (start.z)
      .writeShort (m_drawModels[type])
      .writeByte (0) // framestart
      .writeByte (10) // framerate
      .writeByte (life) // life in 0.1's
      .writeByte (width) // width
      .writeByte (noise) // noise
      .writeByte (red) // r, g, b
      .writeByte (green) // r, g, b
      .writeByte (blue) // r, g, b
      .writeByte (brightness) // brightness
      .writeByte (speed); // speed
}

void Engine::testLine (const Vector &start, const Vector &end, int ignoreFlags, edict_t *ignoreEntity, TraceResult *ptr) {
   // this function traces a line dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or false), and stops
   // at the first obstacle encountered, returning the results of the trace in the TraceResult structure
   // ptr. Such results are (amongst others) the distance traced, the hit surface, the hit plane
   // vector normal, etc. See the TraceResult structure for details. This function allows to specify
   // whether the trace starts "inside" an entity's polygonal model, and if so, to specify that entity
   // in ignoreEntity in order to ignore it as a possible obstacle.

   int engineFlags = 0;

   if (ignoreFlags & TRACE_IGNORE_MONSTERS) {
      engineFlags = 1;
   }

   if (ignoreFlags & TRACE_IGNORE_GLASS) {
      engineFlags |= 0x100;
   }
   engfuncs.pfnTraceLine (start, end, engineFlags, ignoreEntity, ptr);
}

void Engine::testHull (const Vector &start, const Vector &end, int ignoreFlags, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr) {
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

   engfuncs.pfnTraceHull (start, end, !!(ignoreFlags & TRACE_IGNORE_MONSTERS), hullNumber, ignoreEntity, ptr);
}

float Engine::getWaveLen (const char *fileName) {
   extern ConVar yb_chatter_path;
   const char *filePath = util.format ("%s/%s/%s.wav", getModName (), yb_chatter_path.str (), fileName);

   File fp (filePath, "rb");

   // we're got valid handle?
   if (!fp.isValid ()) {
      return 0.0f;
   }
   // check if we have engine function for this
   if (!engine.is (GAME_XASH_ENGINE) && engfuncs.pfnGetApproxWavePlayLen != nullptr) {
      fp.close ();
      return engfuncs.pfnGetApproxWavePlayLen (filePath) / 1000.0f;
   }

   // else fuck with manual search
   struct WavHeader {
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

   if (fp.read (&waveHdr, sizeof (WavHeader)) == 0) {
      util.logEntry (true, LL_ERROR, "Wave File %s - has wrong or unsupported format", filePath);
      return 0.0f;
   }

   if (strncmp (waveHdr.chunkID, "WAVE", 4) != 0) {
      util.logEntry (true, LL_ERROR, "Wave File %s - has wrong wave chunk id", filePath);
      return 0.0f;
   }
   fp.close ();

   if (waveHdr.dataChunkLength == 0) {
      util.logEntry (true, LL_ERROR, "Wave File %s - has zero length!", filePath);
      return 0.0f;
   }
   return static_cast <float> (waveHdr.dataChunkLength) / static_cast <float> (waveHdr.bytesPerSecond);
}

bool Engine::isDedicated (void) {
   // return true if server is dedicated server, false otherwise
   static bool dedicated = engfuncs.pfnIsDedicatedServer () > 0;

   return dedicated;
}

const char *Engine::getModName (void) {
   // this function returns mod name without path

   static char modname[256];

   engfuncs.pfnGetGameDir (modname);
   size_t length = strlen (modname);

   size_t stop = length - 1;
   while ((modname[stop] == '\\' || modname[stop] == '/') && stop > 0) {
      stop--;
   }

   size_t start = stop;
   while (modname[start] != '\\' && modname[start] != '/' && start > 0) {
      start--;
   }

   if (modname[start] == '\\' || modname[start] == '/') {
      start++;
   }

   for (length = start; length <= stop; length++) {
      modname[length - start] = modname[length];
   }
   modname[length - start] = 0; // terminate the string
   return &modname[0];
}

const char *Engine::getMapName (void) {
   // this function gets the map name and store it in the map_name global string variable.

   static char engineMap[256];
   strncpy (engineMap, STRING (globals->mapname), cr::bufsize (engineMap));

   return &engineMap[0];
}

Vector Engine::getAbsPos (edict_t *ent) {
   // this expanded function returns the vector origin of a bounded entity, assuming that any
   // entity that has a bounding box has its center at the center of the bounding box itself.

   if (isNullEntity (ent)) {
      return Vector::null ();
   }

   if (ent->v.origin.empty ()) {
      return ent->v.absmin + ent->v.size * 0.5f;
   }
   return ent->v.origin;
}

void Engine::registerCmd (const char *command, void func (void)) {
   // this function tells the engine that a new server command is being declared, in addition
   // to the standard ones, whose name is command_name. The engine is thus supposed to be aware
   // that for every "command_name" server command it receives, it should call the function
   // pointed to by "function" in order to handle it.

   // check for hl pre 1.1.0.4, as it's doesn't have pfnAddServerCommand
   if (!cr::checkptr (reinterpret_cast <const void *> (engfuncs.pfnAddServerCommand))) {
      util.logEntry (true, LL_FATAL, "YaPB's minimum HL engine version is 1.1.0.4 and minimum Counter-Strike is Beta 6.6. Please update your engine version.");
   }
   engfuncs.pfnAddServerCommand (const_cast <char *> (command), func);
}

void Engine::playSound (edict_t *ent, const char *sound) {
   engfuncs.pfnEmitSound (ent, CHAN_WEAPON, sound, 1.0f, ATTN_NORM, 0, 100);
}

void Engine::execBotCmd (edict_t *ent, const char *fmt, ...) {
   // the purpose of this function is to provide fakeclients (bots) with the same client
   // command-scripting advantages (putting multiple commands in one line between semicolons)
   // as real players. It is an improved version of botman's FakeClientCommand, in which you
   // supply directly the whole string as if you were typing it in the bot's "console". It
   // is supposed to work exactly like the pfnClientCommand (server-sided client command).

   if (!util.isFakeClient (ent)) {
      return;
   }
   va_list ap;
   char string[256];

   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), fmt, ap);
   va_end (ap);

   if (util.isEmptyStr (string)) {
      return;
   }
   m_arguments[0] = '\0';
   m_argumentCount = 0;

   m_isBotCommand = true;

   size_t i, pos = 0;
   size_t length = strlen (string);

   while (pos < length) {
      size_t start = pos;
      size_t stop = pos;

      while (pos < length && string[pos] != ';') {
         pos++;
      }

      if (pos > 1 && string[pos - 1] == '\n') {
         stop = pos - 2;
      }
      else {
         stop = pos - 1;
      }

      for (i = start; i <= stop; i++) {
         m_arguments[i - start] = string[i];
      }
      m_arguments[i - start] = 0;
      pos++;

      size_t index = 0;
      m_argumentCount = 0;

      while (index < i - start) {
         while (index < i - start && m_arguments[index] == ' ') {
            index++;
         }
         if (m_arguments[index] == '"') {
            index++;

            while (index < i - start && m_arguments[index] != '"') {
               index++;
            }
            index++;
         }
         else {
            while (index < i - start && m_arguments[index] != ' ') {
               index++;
            }
         }
         m_argumentCount++;
      }
      MDLL_ClientCommand (ent);
   }
   m_isBotCommand = false;

   m_arguments[0] = '\0';
   m_argumentCount = 0;
}

bool Engine::isSoftwareRenderer (void) {

   // xash always use "hw" structures
   if (is (GAME_XASH_ENGINE)) {
      return false;
   }

   // dedicated server (except xash) always use "sw" structures
   if (isDedicated ()) {
      return true;
   }

   // and on only windows version you can use software-render engine. Linux, OSX always defaults to OpenGL
#if defined (PLATFORM_WIN32)
   static bool isSoftware = GetModuleHandleA ("sw");
#else
   static bool isSoftware = false;
#endif
   return isSoftware;
}

const char *Engine::getField (const char *string, size_t id) {
   // this function gets and returns a particular field in a string where several strings are concatenated

   const int IterBufMax = 4;

   static char arg[IterBufMax][512];
   static int iter = -1;

   if (iter > IterBufMax - 1) {
      iter = 0;
   }

   char *ptr = arg[cr::clamp <int> (iter++, 0, IterBufMax - 1)];
   ptr[0] = 0;

   size_t pos = 0, count = 0, start = 0, stop = 0;
   size_t length = strlen (string);

   while (pos < length && count <= id) {
      while (pos < length && (string[pos] == ' ' || string[pos] == '\t')) {
         pos++;
      }
      if (string[pos] == '"') {
         pos++;
         start = pos;

         while (pos < length && string[pos] != '"') {
            pos++;
         }
         stop = pos - 1;
         pos++;
      }
      else {
         start = pos;

         while (pos < length && string[pos] != ' ' && string[pos] != '\t') {
            pos++;
         }
         stop = pos - 1;
      }

      if (count == id) {
         size_t i = start;

         for (; i <= stop; i++) {
            ptr[i - start] = string[i];
         }
         ptr[i - start] = 0;
         break;
      }
      count++; // we have parsed one field more
   }
   String::trimChars (ptr);

   return ptr;
}

void Engine::execCmd (const char *fmt, ...) {
   // this function asks the engine to execute a server command

   va_list ap;
   char string[MAX_PRINT_BUFFER];

   // concatenate all the arguments in one string
   va_start (ap, fmt);
   vsnprintf (string, cr::bufsize (string), fmt, ap);
   va_end (ap);

   strcat (string, "\n");
   engfuncs.pfnServerCommand (string);
}

void Engine::pushVarToRegStack (const char *variable, const char *value, VarType varType, bool regMissing, const char *regVal, ConVar *self) {
   // this function adds globally defined variable to registration stack

   VarPair pair;
   memset (&pair, 0, sizeof (VarPair));

   pair.reg.name = const_cast <char *> (variable);
   pair.reg.string = const_cast <char *> (value);
   pair.regMissing = regMissing;
   pair.regVal = regVal;

   int engineFlags = FCVAR_EXTDLL;

   if (varType == VT_NORMAL) {
      engineFlags |= FCVAR_SERVER;
   }
   else if (varType == VT_READONLY) {
      engineFlags |= FCVAR_SERVER | FCVAR_SPONLY | FCVAR_PRINTABLEONLY;
   }
   else if (varType == VT_PASSWORD) {
      engineFlags |= FCVAR_PROTECTED;
   }

   pair.reg.flags = engineFlags;
   pair.self = self;
   pair.type = varType;

   m_cvars.push (pair);
}

void Engine::pushRegStackToEngine (bool gameVars) {
   // this function pushes all added global variables to engine registration

   for (auto &var : m_cvars) {
      ConVar &self = *var.self;
      cvar_t &reg = var.reg;

      if (var.type != VT_NOREGISTER) {
         self.m_eptr = engfuncs.pfnCVarGetPointer (reg.name);

         if (self.m_eptr == nullptr) {
            engfuncs.pfnCVarRegister (&var.reg);
            self.m_eptr = engfuncs.pfnCVarGetPointer (reg.name);
         }
      }
      else if (gameVars) {
         self.m_eptr = engfuncs.pfnCVarGetPointer (reg.name);

         if (var.regMissing && self.m_eptr == nullptr) {
            if (reg.string == nullptr && var.regVal != nullptr) {
               reg.string = const_cast <char *> (var.regVal);
               reg.flags |= FCVAR_SERVER;
            }
            engfuncs.pfnCVarRegister (&var.reg);
            self.m_eptr = engfuncs.pfnCVarGetPointer (reg.name);
         }

         if (!self.m_eptr) {
            print ("Got nullptr on cvar %s!", reg.name);
         }
      }
   }
}

const char *Engine::translate (const char *input) {
   // this function translate input string into needed language

   if (isDedicated ()) {
      return input;
   }
   static String result;

   if (m_language.get (input, result)) {
      return result.chars ();
   }
   return input; // nothing found
}

void Engine::processMessages (void *ptr) {
   if (m_msgBlock.msg == NETMSG_UNDEFINED) {
      return;
   }

   // some needed variables
   static uint8 r, g, b;
   static uint8 enabled;

   static int damageArmor, damageTaken, damageBits;
   static int killerIndex, victimIndex, playerIndex;
   static int index, numPlayers;
   static int state, id, clip;

   static Vector damageOrigin;
   static WeaponProp weaponProp;

   // some widely used stuff
   Bot *bot = bots.getBot (m_msgBlock.bot);

   char *strVal = reinterpret_cast <char *> (ptr);
   int intVal = *reinterpret_cast <int *> (ptr);
   uint8 byteVal = *reinterpret_cast <uint8 *> (ptr);

   // now starts of network message execution
   switch (m_msgBlock.msg) {
   case NETMSG_VGUI:
      // this message is sent when a VGUI menu is displayed.

      if (bot != nullptr && m_msgBlock.state == 0) {
         switch (intVal) {
         case VMS_TEAM:
            bot->m_startAction = GAME_MSG_TEAM_SELECT;
            break;

         case VMS_TF:
         case VMS_CT:
            bot->m_startAction = GAME_MSG_CLASS_SELECT;
            break;
         }
      }
      break;

   case NETMSG_SHOWMENU:
      // this message is sent when a text menu is displayed.

      // ignore first 3 fields of message
      if (m_msgBlock.state < 3 || bot == nullptr) {
         break;
      }

      if (strcmp (strVal, "#Team_Select") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#Team_Select_Spect") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#IG_Team_Select_Spect") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#IG_Team_Select") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#IG_VIP_Team_Select") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#IG_VIP_Team_Select_Spect") == 0) {
         bot->m_startAction = GAME_MSG_TEAM_SELECT;
      }
      else if (strcmp (strVal, "#Terrorist_Select") == 0) {
         bot->m_startAction = GAME_MSG_CLASS_SELECT;
      }
      else if (strcmp (strVal, "#CT_Select") == 0) {
         bot->m_startAction = GAME_MSG_CLASS_SELECT;
      }
      break;

   case NETMSG_WEAPONLIST:
      // this message is sent when a client joins the game. All of the weapons are sent with the weapon ID and information about what ammo is used.

      switch (m_msgBlock.state) {
      case 0:
         strncpy (weaponProp.classname, strVal, cr::bufsize (weaponProp.classname));
         break;

      case 1:
         weaponProp.ammo1 = intVal; // ammo index 1
         break;

      case 2:
         weaponProp.ammo1Max = intVal; // max ammo 1
         break;

      case 5:
         weaponProp.slot = intVal; // slot for this weapon
         break;

      case 6:
         weaponProp.pos = intVal; // position in slot
         break;

      case 7:
         weaponProp.id = intVal; // weapon ID
         break;

      case 8:
         weaponProp.flags = intVal; // flags for weapon (WTF???)
         conf.setWeaponProp (weaponProp); // store away this weapon with it's ammo information...
         break;
      }
      break;

   case NETMSG_CURWEAPON:
      // this message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon). In CS it's also called when Ammo is increased/decreased

      switch (m_msgBlock.state) {
      case 0:
         state = intVal; // state of the current weapon (WTF???)
         break;

      case 1:
         id = intVal; // weapon ID of current weapon
         break;

      case 2:
         clip = intVal; // ammo currently in the clip for this weapon

         if (bot != nullptr && id <= 31) {
            if (state != 0) {
               bot->m_currentWeapon = id;
            }

            // ammo amount decreased ? must have fired a bullet...
            if (id == bot->m_currentWeapon && bot->m_ammoInClip[id] > clip) {
               bot->m_timeLastFired = timebase (); // remember the last bullet time
            }
            bot->m_ammoInClip[id] = clip;
         }
         break;
      }
      break;

   case NETMSG_AMMOX:
      // this message is sent whenever ammo amounts are adjusted (up or down). NOTE: Logging reveals that CS uses it very unreliable!

      switch (m_msgBlock.state) {
      case 0:
         index = intVal; // ammo index (for type of ammo)
         break;

      case 1:
         if (bot != nullptr) {
            bot->m_ammo[index] = intVal; // store it away
         }
         break;
      }
      break;

   case NETMSG_AMMOPICKUP:
      // this message is sent when the bot picks up some ammo (AmmoX messages are also sent so this message is probably
      // not really necessary except it allows the HUD to draw pictures of ammo that have been picked up.  The bots
      // don't really need pictures since they don't have any eyes anyway.

      switch (m_msgBlock.state) {
      case 0:
         index = intVal;
         break;

      case 1:
         if (bot != nullptr) {
            bot->m_ammo[index] = intVal;
         }
         break;
      }
      break;

   case NETMSG_DAMAGE:
      // this message gets sent when the bots are getting damaged.

      switch (m_msgBlock.state) {
      case 0:
         damageArmor = intVal;
         break;

      case 1:
         damageTaken = intVal;
         break;

      case 2:
         damageBits = intVal;

         if (bot != nullptr && (damageArmor > 0 || damageTaken > 0)) {
            bot->processDamage (bot->pev->dmg_inflictor, damageTaken, damageArmor, damageBits);
         }
         break;
      }
      break;

   case NETMSG_MONEY:
      // this message gets sent when the bots money amount changes

      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_moneyAmount = intVal; // amount of money
      }
      break;

   case NETMSG_STATUSICON:
      switch (m_msgBlock.state) {
      case 0:
         enabled = byteVal;
         break;

      case 1:
         if (bot != nullptr) {
            if (strcmp (strVal, "buyzone") == 0) {
               bot->m_inBuyZone = (enabled != 0);

               // try to equip in buyzone
               bot->processBuyzoneEntering (BUYSTATE_PRIMARY_WEAPON);
            }
            else if (strcmp (strVal, "vipsafety") == 0) {
               bot->m_inVIPZone = (enabled != 0);
            }
            else if (strcmp (strVal, "c4") == 0) {
               bot->m_inBombZone = (enabled == 2);
            }
         }
         break;
      }
      break;

   case NETMSG_DEATH: // this message sends on death
      switch (m_msgBlock.state) {
      case 0:
         killerIndex = intVal;
         break;

      case 1:
         victimIndex = intVal;
         break;

      case 2:
         bots.updateDeathMsgState (true);

         if (killerIndex != 0 && killerIndex != victimIndex) {
            edict_t *killer = entityOfIndex (killerIndex);
            edict_t *victim = entityOfIndex (victimIndex);

            if (isNullEntity (killer) || isNullEntity (victim)) {
               break;
            }

            if (yb_communication_type.integer () == 2) {
               // need to send congrats on well placed shot
               for (int i = 0; i < maxClients (); i++) {
                  Bot *notify = bots.getBot (i);

                  if (notify != nullptr && notify->m_notKilled && killer != notify->ent () && notify->seesEntity (victim->v.origin) && getTeam (killer) == notify->m_team && getTeam (killer) != getTeam (victim)) {
                     if (!bots.getBot (killer)) {
                        notify->processChatterMessage ("#Bot_NiceShotCommander");
                     }
                     else {
                        notify->processChatterMessage ("#Bot_NiceShotPall");
                     }
                     break;
                  }
               }
            }

            // notice nearby to victim teammates, that attacker is near
            for (int i = 0; i < maxClients (); i++) {
               Bot *notify = bots.getBot (i);

               if (notify != nullptr && notify->m_seeEnemyTime + 2.0f < timebase () && notify->m_notKilled && notify->m_team == getTeam (victim) && util.isVisible (killer->v.origin, notify->ent ()) && isNullEntity (notify->m_enemy) && getTeam (killer) != getTeam (victim)) {
                  notify->m_actualReactionTime = 0.0f;
                  notify->m_seeEnemyTime = timebase ();
                  notify->m_enemy = killer;
                  notify->m_lastEnemy = killer;
                  notify->m_lastEnemyOrigin = killer->v.origin;
               }
            }

            Bot *notify = bots.getBot (killer);

            // is this message about a bot who killed somebody?
            if (notify != nullptr) {
               notify->m_lastVictim = victim;
            }
            else // did a human kill a bot on his team?
            {
               Bot *target = bots.getBot (victim);

               if (target != nullptr) {
                  if (getTeam (killer) == getTeam (victim)) {
                     target->m_voteKickIndex = killerIndex;
                  }
                  target->m_notKilled = false;
               }
            }
         }
         break;
      }
      break;

   case NETMSG_SCREENFADE: // this message gets sent when the screen fades (flashbang)
      switch (m_msgBlock.state) {
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
         if (bot != nullptr && r >= 255 && g >= 255 && b >= 255 && byteVal > 170) {
            bot->processBlind (byteVal);
         }
         break;
      }
      break;

   case NETMSG_HLTV: // round restart in steam cs
      switch (m_msgBlock.state) {
      case 0:
         numPlayers = intVal;
         break;

      case 1:
         if (numPlayers == 0 && intVal == 0) {
            bots.initRound ();
         }
         break;
      }
      break;

   case NETMSG_TEXTMSG:
      if (m_msgBlock.state == 1) {
         if (strcmp (strVal, "#CTs_Win") == 0 ||
            strcmp (strVal, "#Bomb_Defused") == 0 ||
            strcmp (strVal, "#Terrorists_Win") == 0 ||
            strcmp (strVal, "#Round_Draw") == 0 ||
            strcmp (strVal, "#All_Hostages_Rescued") == 0 ||
            strcmp (strVal, "#Target_Saved") == 0 ||
            strcmp (strVal, "#Hostages_Not_Rescued") == 0 ||
            strcmp (strVal, "#Terrorists_Not_Escaped") == 0 ||
            strcmp (strVal, "#VIP_Not_Escaped") == 0 ||
            strcmp (strVal, "#Escaping_Terrorists_Neutralized") == 0 ||
            strcmp (strVal, "#VIP_Assassinated") == 0 ||
            strcmp (strVal, "#VIP_Escaped") == 0 ||
            strcmp (strVal, "#Terrorists_Escaped") == 0 ||
            strcmp (strVal, "#CTs_PreventEscape") == 0 ||
            strcmp (strVal, "#Target_Bombed") == 0 ||
            strcmp (strVal, "#Game_Commencing") == 0 ||
            strcmp (strVal, "#Game_will_restart_in") == 0) {
            bots.setRoundOver (true);

            if (strcmp (strVal, "#Game_Commencing") == 0) {
               util.setNeedForWelcome (true);
            }

            if (strcmp (strVal, "#CTs_Win") == 0) {
               bots.setLastWinner (TEAM_COUNTER); // update last winner for economics

               if (yb_communication_type.integer () == 2) {
                  Bot *notify = bots.getAliveBot ();

                  if (notify != nullptr && notify->m_notKilled) {
                     notify->processChatterMessage (strVal);
                  }
               }
            }

            if (strcmp (strVal, "#Game_will_restart_in") == 0) {
               bots.updateTeamEconomics (TEAM_COUNTER, true);
               bots.updateTeamEconomics (TEAM_TERRORIST, true);
            }

            if (strcmp (strVal, "#Terrorists_Win") == 0) {
               bots.setLastWinner (TEAM_TERRORIST); // update last winner for economics

               if (yb_communication_type.integer () == 2) {
                  Bot *notify = bots.getAliveBot ();

                  if (notify != nullptr && notify->m_notKilled) {
                     notify->processChatterMessage (strVal);
                  }
               }
            }
            waypoints.setBombPos (true);
         }
         else if (!bots.isBombPlanted () && strcmp (strVal, "#Bomb_Planted") == 0) {
            bots.setBombPlanted (true);

            for (int i = 0; i < maxClients (); i++) {
               Bot *notify = bots.getBot (i);

               if (notify != nullptr && notify->m_notKilled) {
                  notify->clearSearchNodes ();
                  notify->clearTasks ();

                  if (yb_communication_type.integer () == 2 && rng.chance (55) && notify->m_team == TEAM_COUNTER) {
                     notify->pushChatterMessage (CHATTER_WHERE_IS_THE_BOMB);
                  }
               }
            }
            waypoints.setBombPos ();
         }
         else if (bot != nullptr && strcmp (strVal, "#Switch_To_BurstFire") == 0) {
            bot->m_weaponBurstMode = BURST_ON;
         }
         else if (bot != nullptr && strcmp (strVal, "#Switch_To_SemiAuto") == 0) {
            bot->m_weaponBurstMode = BURST_OFF;
         }
      }
      break;

   case NETMSG_TEAMINFO:
      switch (m_msgBlock.state) {
      case 0:
         playerIndex = intVal;
         break;

      case 1:
         if (playerIndex > 0 && playerIndex <= maxClients ()) {
            int team = TEAM_UNASSIGNED;

            if (strVal[0] == 'U' && strVal[1] == 'N') {
               team = TEAM_UNASSIGNED;
            }
            else if (strVal[0] == 'T' && strVal[1] == 'E') {
               team = TEAM_TERRORIST;
            }
            else if (strVal[0] == 'C' && strVal[1] == 'T') {
               team = TEAM_COUNTER;
            }
            else if (strVal[0] == 'S' && strVal[1] == 'P') {
               team = TEAM_SPECTATOR;
            }
            auto &client = util.getClient (playerIndex - 1);

            client.team2 = team;
            client.team = engine.is (GAME_CSDM_FFA) ? playerIndex : team;
         }
         break;
      }
      break;

   case NETMSG_BARTIME:
      if (bot != nullptr && m_msgBlock.state == 0) {
         if (intVal > 0) {
            bot->m_hasProgressBar = true; // the progress bar on a hud
         }
         else if (intVal == 0) {
            bot->m_hasProgressBar = false; // no progress bar or disappeared
         }
      }
      break;

   case NETMSG_ITEMSTATUS:
      if (bot != nullptr && m_msgBlock.state == 0) {

         enum ItemStatus {
            IS_NIGHTVISION = (1 << 0),
            IS_DEFUSEKIT = (1 << 1)
         };

         bot->m_hasNVG = (intVal & IS_NIGHTVISION) ? true : false;
         bot->m_hasDefuser = (intVal & IS_DEFUSEKIT) ? true : false;
      }
      break;

   case NETMSG_FLASHBAT:
      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_flashLevel = static_cast <float> (intVal);
      }
      break;

   case NETMSG_NVGTOGGLE:
      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_usesNVG = intVal > 0;
      }
      break;

   default:
      util.logEntry (true, LL_FATAL, "Network message handler error. Call to unrecognized message id (%d).\n", m_msgBlock.msg);
   }
   m_msgBlock.state++; // and finally update network message state
}

bool Engine::loadCSBinary (void) {
   const char *modname = engine.getModName ();

   if (!modname) {
      return false;
   }

#if defined(PLATFORM_WIN32)
   const char *libs[] = { "mp.dll", "cs.dll" };
#elif defined(PLATFORM_LINUX)
   const char *libs[] = { "cs.so", "cs_i386.so" };
#elif defined(PLATFORM_OSX)
   const char *libs[] = { "cs.dylib" };
#endif

   auto libCheck = [&] (const char *modname, const char *dll) {
      // try to load gamedll
      if (!m_gameLib.isValid ()) {
         util.logEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", dll, modname);

         return false;
      }
      auto ent = m_gameLib.resolve <EntityFunction> ("trigger_random_unique");

      // detect regamedll by addon entity they provide
      if (ent != nullptr) {
         m_gameFlags |= GAME_REGAMEDLL;
      }
      return true;
   };

   // search the libraries inside game dlls directory
   for (size_t i = 0; i < cr::arrsize (libs); i++) {
      auto *path = util.format ("%s/dlls/%s", modname, libs[i]);

      // if we can't read file, skip it
      if (!File::exists (path)) {
         continue;
      }

      // special case, czero is always detected first, as it's has custom directory
      if (strcmp (modname, "czero") == 0) {
         m_gameFlags |= (GAME_CZERO | GAME_SUPPORT_BOT_VOICE | GAME_SUPPORT_SVC_PINGS);

         if (is (GAME_METAMOD)) {
            return false;
         }
         m_gameLib.load (path);

         // verify dll is OK 
         if (!libCheck (modname, libs[i])) {
            return false;
         }
         return true;
      }
      else {
         m_gameLib.load (path);

         // verify dll is OK 
         if (!libCheck (modname, libs[i])) {
            return false;
         }

         // detect if we're running modern game
         auto entity = m_gameLib.resolve <EntityFunction> ("weapon_famas");

         // detect xash engine
         if (engfuncs.pfnCVarGetPointer ("build") != nullptr) {
            m_gameFlags |= (GAME_LEGACY | GAME_XASH_ENGINE);

            if (entity != nullptr) {
               m_gameFlags |= GAME_SUPPORT_BOT_VOICE;
            }

            if (is (GAME_METAMOD)) {
               return false;
            }
            return true;
         }

         if (entity != nullptr) {
            m_gameFlags |= (GAME_CSTRIKE16 | GAME_SUPPORT_BOT_VOICE | GAME_SUPPORT_SVC_PINGS);
         }
         else {
            m_gameFlags |= GAME_LEGACY;
         }

         if (is (GAME_METAMOD)) {
            return false;
         }
         return true;
      }
   }
   return false;
}

bool Engine::postload (void) {
   // register our cvars
   engine.pushRegStackToEngine ();

   // ensure we're have all needed directories
   const char *mod = engine.getModName ();

   // create the needed paths
   File::pathCreate (const_cast <char *> (util.format ("%s/addons/yapb/conf/lang", mod)));
   File::pathCreate (const_cast <char *> (util.format ("%s/addons/yapb/data/learned", mod)));

   // print game detection info
   auto printGame = [&] (void) {
      String gameVersionStr;

      if (is (GAME_LEGACY)) {
         gameVersionStr.assign ("Legacy");
      }
      else if (is (GAME_CZERO)) {
         gameVersionStr.assign ("Condition Zero");
      }
      else if (is (GAME_CSTRIKE16)) {
         gameVersionStr.assign ("v1.6");
      }

      if (is (GAME_XASH_ENGINE)) {
         gameVersionStr.append (" @ Xash3D Engine");

         if (is (GAME_MOBILITY)) {
            gameVersionStr.append (" Mobile");
         }
         gameVersionStr.replace ("Legacy", "1.6 Limited");
      }

      if (is (GAME_SUPPORT_BOT_VOICE)) {
         gameVersionStr.append (" (BV)");
      }

      if (is (GAME_REGAMEDLL)) {
         gameVersionStr.append (" (RE)");
      }

      if (is (GAME_SUPPORT_SVC_PINGS)) {
         gameVersionStr.append (" (SVC)");
      }
      print ("[YAPB] Bot v%s.0.%d Loaded. Game detected as Counter-Strike: %s", PRODUCT_VERSION, util.buildNumber (), gameVersionStr.chars ());
   };

#ifdef PLATFORM_ANDROID
   m_gameFlags |= (GAME_XASH_ENGINE | GAME_MOBILITY | GAME_SUPPORT_BOT_VOICE | GAME_REGAMEDLL);

   if (is (GAME_METAMOD)) {
      return true; // we should stop the attempt for loading the real gamedll, since metamod handle this for us
   }

   extern ConVar yb_difficulty;
   yb_difficulty.set (2);

#ifdef LOAD_HARDFP
   const char *serverDLL = "libserver_hardfp.so";
#else
   const char *serverDLL = "libserver.so";
#endif

   char gameDLLName[256];
   snprintf (gameDLLName, cr::bufsize (gameDLLName), "%s/%s", getenv ("XASH3D_GAMELIBDIR"), serverDLL);

   m_gameLib.load (gameDLLName);

   if (!m_gameLib.isValid ()) {
      util.logEntry (true, LL_FATAL | LL_IGNORE, "Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", gameDLLName, engine.getModName ());
      return true;
   }
   printGame ();

#else
   bool binaryLoaded = loadCSBinary ();

   if (!binaryLoaded && !is (GAME_METAMOD)) {
      util.logEntry (true, LL_FATAL | LL_IGNORE, "Mod that you has started, not supported by this bot (gamedir: %s)", engine.getModName ());
      return true;
   }
   printGame ();

   if (is (GAME_METAMOD)) {
      return true;
   }
#endif
   return false;
}

void Engine::detectDeathmatch (void) {
   if (!engine.is (GAME_METAMOD | GAME_REGAMEDLL)) {
      return;
   }
   static auto dmActive = engfuncs.pfnCVarGetPointer ("csdm_active");
   static auto freeForAll = engfuncs.pfnCVarGetPointer ("mp_freeforall");

   // csdm is only with amxx and metamod
   if (dmActive) {
      if (dmActive->value > 0.0f) {
         m_gameFlags |= GAME_CSDM;
      }
      else if (is (GAME_CSDM)) {
         m_gameFlags &= ~GAME_CSDM;
      }
   }

   // but this can be provided by regamedll
   if (freeForAll) {
      if (freeForAll->value > 0.0f) {
         m_gameFlags |= GAME_CSDM_FFA;
      }
      else if (is (GAME_CSDM_FFA)) {
         m_gameFlags &= ~GAME_CSDM_FFA;
      }
   }
}

void Engine::slowFrame (void) {
   if (m_slowFrame > engine.timebase ()) {
      return;
   }
   extern ConVar yb_password_key, yb_password;

   for (int i = 0; i < maxClients (); i++) {
      edict_t *player = entityOfIndex (i + 1);

      // code below is executed only on dedicated server
      if (isDedicated () && !isNullEntity (player) && (player->v.flags & FL_CLIENT) && !(player->v.flags & FL_FAKECLIENT)) {
         Client &client = util.getClient (i);

         if (client.flags & CF_ADMIN) {
            if (util.isEmptyStr (yb_password_key.str ()) && util.isEmptyStr (yb_password.str ())) {
               client.flags &= ~CF_ADMIN;
            }
            else if (!!strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ())))) {
               client.flags &= ~CF_ADMIN;
               print ("Player %s had lost remote access to yapb.", STRING (player->v.netname));
            }
         }
         else if (!(client.flags & CF_ADMIN) && !util.isEmptyStr (yb_password_key.str ()) && !util.isEmptyStr (yb_password.str ())) {
            if (strcmp (yb_password.str (), engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (client.ent), const_cast <char *> (yb_password_key.str ()))) == 0) {
               client.flags |= CF_ADMIN;
               print ("Player %s had gained full remote access to yapb.", STRING (player->v.netname));
            }
         }
      }
   }
   bots.calculatePingOffsets ();

   // calculate light levels for all waypoints if needed
   waypoints.initLightLevels ();

   // detect csdm
   detectDeathmatch ();

   // display welcome message
   util.checkWelcome ();
   m_slowFrame = timebase () + 1.0f;
}

inline int Engine::getTeam (edict_t *ent) {
   return util.getClient (indexOfEntity (ent) - 1).team;
}

void LightMeasure::initializeLightstyles (void) {
   // this function initializes lighting information...

   // reset all light styles
   for (int i = 0; i < MAX_LIGHTSTYLES; i++) {
      m_lightstyle[i].length = 0;
      m_lightstyle[i].map[0] = 0x0;
   }

   for (int i = 0; i < MAX_LIGHTSTYLEVALUE; i++) {
      m_lightstyleValue[i] = 264;
   }
}

void LightMeasure::animateLight (void) {
   // this function performs light animations

   if (!m_doAnimation) {
      return;
   }

   // 'm' is normal light, 'a' is no light, 'z' is double bright
   const int index = static_cast <int> (engine.timebase () * 10.0f);

   for (int j = 0; j < MAX_LIGHTSTYLES; j++) {
      if (!m_lightstyle[j].length) {
         m_lightstyleValue[j] = 256;
         continue;
      }
      int value = m_lightstyle[j].map[index % m_lightstyle[j].length] - 'a';
      m_lightstyleValue[j] = value * 22;
   }
}

template <typename S, typename M> bool LightMeasure::recursiveLightPoint (const M *node, const Vector &start, const Vector &end) {
   if (node->contents < 0) {
      return false;
   }

   // determine which side of the node plane our points are on, fixme: optimize for axial
   auto plane = node->plane;

   float front = (start | plane->normal) - plane->dist;
   float back = (end | plane->normal) - plane->dist;

   int side = front < 0.0f;

   // if they're both on the same side of the plane, don't bother to split just check the appropriate child
   if ((back < 0.0f) == side) {
      return recursiveLightPoint <S, M> (reinterpret_cast <M *> (node->children[side]), start, end);
   }

   // calculate mid point
   float frac = front / (front - back);
   auto mid = start + (end - start) * frac;

   // go down front side
   if (recursiveLightPoint <S, M> (reinterpret_cast <M *> (node->children[side]), start, mid)) {
      return true; // hit something
   }

   // blow it off if it doesn't split the plane...
   if ((back < 0.0f) == !!side) {
      return false; // didn't hit anything
   }

   // check for impact on this node
   // lightspot = mid;
   // lightplane = plane;
   auto surf = reinterpret_cast <S *> (m_worldModel->surfaces) + node->firstsurface;

   for (int i = 0; i < node->numsurfaces; i++, surf++) {
      if (surf->flags & SURF_DRAWTILED) {
         continue; // no lightmaps
      }
      auto tex = surf->texinfo;

      // see where in lightmap space our intersection point is
      int s = static_cast <int> ((mid | Vector (tex->vecs[0])) + tex->vecs[0][3]);
      int t = static_cast <int> ((mid | Vector (tex->vecs[1])) + tex->vecs[1][3]);

      // not in the bounds of our lightmap? punt...
      if (s < surf->texturemins[0] || t < surf->texturemins[1]) {
         continue;
      }

      // assuming a square lightmap (fixme: which ain't always the case), lets see if it lies in that rectangle. if not, punt...
      int ds = s - surf->texturemins[0];
      int dt = t - surf->texturemins[1];

      if (ds > surf->extents[0] || dt > surf->extents[1]) {
         continue;
      }

      if (!surf->samples) {
         return true;
      }
      ds >>= 4;
      dt >>= 4;

      m_point.reset (); // reset point color.

      int smax = (surf->extents[0] >> 4) + 1;
      int tmax = (surf->extents[1] >> 4) + 1;
      int size = smax * tmax;

      auto lightmap = surf->samples + dt * smax + ds;

      // compute the lightmap color at a particular point
      for (int maps = 0u; maps < MAXLIGHTMAPS && surf->styles[maps] != 255u; ++maps) {
         uint32 scale = m_lightstyleValue[surf->styles[maps]];

         m_point.red += lightmap->r * scale;
         m_point.green += lightmap->g * scale;
         m_point.blue += lightmap->b * scale;

         lightmap += size; // skip to next lightmap
      }
      m_point.red >>= 8u;
      m_point.green >>= 8u;
      m_point.blue >>= 8u;

      return true;
   }
   return recursiveLightPoint <S, M> (reinterpret_cast <M *> (node->children[!side]), mid, end); // go down back side
}

float LightMeasure::getLightLevel (const Vector &point) {
   if (!m_worldModel) {
      return 0.0f;
   }

   if (!m_worldModel->lightdata) {
      return 255.0f;
   }

   Vector endPoint (point);
   endPoint.z -= 2048.0f;

   // it's depends if we're are on dedicated or on listenserver
   auto recursiveCheck = [&] (void) -> bool {
      if (!engine.isSoftwareRenderer ()) {
         return recursiveLightPoint <msurface_hw_t, mnode_hw_t> (reinterpret_cast <mnode_hw_t *> (m_worldModel->nodes), point, endPoint);
      }
      return recursiveLightPoint <msurface_t, mnode_t> (m_worldModel->nodes, point, endPoint);
   };
   return !recursiveCheck () ? 0.0f : 100 * cr::sqrtf (cr::min (75.0f, static_cast <float> (m_point.avg ())) / 75.0f);
}

float LightMeasure::getSkyColor (void) {
   return sv_skycolor_r.flt () + sv_skycolor_g.flt () + sv_skycolor_b.flt ();
}
