//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar sv_skycolor_r ("sv_skycolor_r", nullptr, Var::NoRegister);
ConVar sv_skycolor_g ("sv_skycolor_g", nullptr, Var::NoRegister);
ConVar sv_skycolor_b ("sv_skycolor_b", nullptr, Var::NoRegister);

Game::Game () {
   m_startEntity = nullptr;
   m_localEntity = nullptr;

   m_precached = false;
   m_isBotCommand = false;

   memset (m_drawModels, 0, sizeof (m_drawModels));
   memset (m_spawnCount, 0, sizeof (m_spawnCount));

   m_gameFlags = 0;
   m_mapFlags = 0;
   m_slowFrame = 0.0;

   m_cvars.clear ();
}

void Game::precache () {
   if (m_precached) {
      return;
   }
   m_precached = true;

   m_drawModels[DrawLine::Simple] = engfuncs.pfnPrecacheModel (ENGINE_STR ("sprites/laserbeam.spr"));
   m_drawModels[DrawLine::Arrow] = engfuncs.pfnPrecacheModel (ENGINE_STR ("sprites/arrow1.spr"));

   engfuncs.pfnPrecacheSound (ENGINE_STR ("weapons/xbow_hit1.wav")); // waypoint add
   engfuncs.pfnPrecacheSound (ENGINE_STR ("weapons/mine_activate.wav")); // waypoint delete
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_hudoff.wav")); // path add/delete start
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_hudon.wav")); // path add/delete done
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_moveselect.wav")); // path add/delete cancel
   engfuncs.pfnPrecacheSound (ENGINE_STR ("common/wpn_denyselect.wav")); // path add/delete error

   m_mapFlags = 0; // reset map type as worldspawn is the first entity spawned

   // detect official csbots here
   if (!(is (GameFlags::Legacy)) && engfuncs.pfnCVarGetPointer ("bot_stop") != nullptr) {
      m_gameFlags |= GameFlags::CSBot;
   }
   registerCvars (true);
}

void Game::levelInitialize (edict_t *entities, int max) {
   // this function precaches needed models and initialize class variables

   m_spawnCount[Team::CT] = 0;
   m_spawnCount[Team::Terrorist] = 0;
   
   // go thru the all entities on map, and do whatever we're want
   for (int i = 0; i < max; ++i) {
      auto ent = entities + i;

      // only valid entities
      if (!ent || ent->free || ent->v.classname == 0) {
         continue;
      }
      auto classname = STRING (ent->v.classname);

      if (strcmp (classname, "worldspawn") == 0) {
         m_startEntity = ent;

         // initialize some structures
         bots.initRound ();

         // install the sendto hook to fake queries
         util.installSendTo ();
      }
      else if (strcmp (classname, "player_weaponstrip") == 0) {
         if ((is (GameFlags::Legacy)) && (STRING (ent->v.target))[0] == '\0') {
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

         ++m_spawnCount[Team::CT];
      }
      else if (strcmp (classname, "info_player_deathmatch") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/terror/terror.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;

         ++m_spawnCount[Team::Terrorist];
      }

      else if (strcmp (classname, "info_vip_start") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/vip/vip.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;
      }
      else if (strcmp (classname, "func_vip_safetyzone") == 0 || strcmp (classname, "info_vip_safetyzone") == 0) {
         m_mapFlags |= MapFlags::Assassination; // assassination map
      }
      else if (strcmp (classname, "hostage_entity") == 0) {
         m_mapFlags |= MapFlags::HostageRescue; // rescue map
      }
      else if (strcmp (classname, "func_bomb_target") == 0 || strcmp (classname, "info_bomb_target") == 0) {
         m_mapFlags |= MapFlags::Demolition; // defusion map
      }
      else if (strcmp (classname, "func_escapezone") == 0) {
         m_mapFlags |= MapFlags::Escape;
      }
      else if (strncmp (classname, "func_door", 9) == 0) {
         m_mapFlags |= MapFlags::HasDoors;
      }
      else if (strncmp (classname, "func_button", 11) == 0) {
         m_mapFlags |= MapFlags::HasButtons;
      }
   }

   // next maps doesn't have map-specific entities, so determine it by name
   if (strncmp (getMapName (), "fy_", 3) == 0) {
      m_mapFlags |= MapFlags::Fun;
   }
   else if (strncmp (getMapName (), "ka_", 3) == 0) {
      m_mapFlags |= MapFlags::KnifeArena;
   }

   // reset some timers
   m_slowFrame = 0.0f;
}

void Game::drawLine (edict_t *ent, const Vector &start, const Vector &end, int width, int noise, const Color &color, int brightness, int speed, int life, DrawLine type) {
   // this function draws a arrow visible from the client side of the player whose player entity
   // is pointed to by ent, from the vector location start to the vector location end,
   // which is supposed to last life tenths seconds, and having the color defined by RGB.

   if (!util.isPlayer (ent)) {
      return; // reliability check
   }

   MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, ent)
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
      .writeByte (color.red) // r, g, b
      .writeByte (color.green) // r, g, b
      .writeByte (color.blue) // r, g, b
      .writeByte (brightness) // brightness
      .writeByte (speed); // speed
}

void Game::testLine (const Vector &start, const Vector &end, int ignoreFlags, edict_t *ignoreEntity, TraceResult *ptr) {
   // this function traces a line dot by dot, starting from vecStart in the direction of vecEnd,
   // ignoring or not monsters (depending on the value of IGNORE_MONSTERS, true or false), and stops
   // at the first obstacle encountered, returning the results of the trace in the TraceResult structure
   // ptr. Such results are (amongst others) the distance traced, the hit surface, the hit plane
   // vector normal, etc. See the TraceResult structure for details. This function allows to specify
   // whether the trace starts "inside" an entity's polygonal model, and if so, to specify that entity
   // in ignoreEntity in order to ignore it as a possible obstacle.

   int engineFlags = 0;

   if (ignoreFlags & TraceIgnore::Monsters) {
      engineFlags = 1;
   }

   if (ignoreFlags & TraceIgnore::Glass) {
      engineFlags |= 0x100;
   }
   engfuncs.pfnTraceLine (start, end, engineFlags, ignoreEntity, ptr);
}

void Game::testHull (const Vector &start, const Vector &end, int ignoreFlags, int hullNumber, edict_t *ignoreEntity, TraceResult *ptr) {
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

   engfuncs.pfnTraceHull (start, end, !!(ignoreFlags & TraceIgnore::Monsters), hullNumber, ignoreEntity, ptr);
}

float Game::getWaveLen (const char *fileName) {
   extern ConVar yb_chatter_path;
   const char *filePath = strings.format ("%s/%s/%s.wav", getModName (), yb_chatter_path.str (), fileName);

   File fp (filePath, "rb");

   // we're got valid handle?
   if (!fp) {
      return 0.0f;
   }

   // check if we have engine function for this
   if (!is (GameFlags::Xash3D) && plat.checkPointer (engfuncs.pfnGetApproxWavePlayLen)) {
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
      logger.error ("Wave File %s - has wrong or unsupported format", filePath);
      return 0.0f;
   }

   if (strncmp (waveHdr.chunkID, "WAVE", 4) != 0) {
      logger.error ("Wave File %s - has wrong wave chunk id", filePath);
      return 0.0f;
   }
   fp.close ();

   if (waveHdr.dataChunkLength == 0) {
      logger.error ("Wave File %s - has zero length!", filePath);
      return 0.0f;
   }
   return static_cast <float> (waveHdr.dataChunkLength) / static_cast <float> (waveHdr.bytesPerSecond);
}

bool Game::isDedicated () {
   // return true if server is dedicated server, false otherwise
   static bool dedicated = engfuncs.pfnIsDedicatedServer () > 0;

   return dedicated;
}

const char *Game::getModName () {
   // this function returns mod name without path

   static String name;

   if (!name.empty ()) {
      return name.chars ();
   }

   char engineModName[256];
   engfuncs.pfnGetGameDir (engineModName);

   name = engineModName;
   size_t slash = name.findLastOf ("\\/");

   if (slash != String::InvalidIndex) {
      name = name.substr (slash + 1);
   }
   name = name.trim (" \\/");
   return name.chars ();
}

const char *Game::getMapName () {
   // this function gets the map name and store it in the map_name global string variable.

   return strings.format ("%s", STRING (globals->mapname));
}

Vector Game::getAbsPos (edict_t *ent) {
   // this expanded function returns the vector origin of a bounded entity, assuming that any
   // entity that has a bounding box has its center at the center of the bounding box itself.

   if (isNullEntity (ent)) {
      return nullptr;
   }

   if (ent->v.origin.empty ()) {
      return ent->v.absmin + (ent->v.size * 0.5);
   }
   return ent->v.origin;
}

void Game::registerEngineCommand (const char *command, void func ()) {
   // this function tells the engine that a new server command is being declared, in addition
   // to the standard ones, whose name is command_name. The engine is thus supposed to be aware
   // that for every "command_name" server command it receives, it should call the function
   // pointed to by "function" in order to handle it.

   // check for hl pre 1.1.0.4, as it's doesn't have pfnAddServerCommand
   if (!plat.checkPointer (engfuncs.pfnAddServerCommand)) {
      logger.fatal ("YaPB's minimum HL engine version is 1.1.0.4 and minimum Counter-Strike is Beta 6.6. Please update your engine version.");
   }
   engfuncs.pfnAddServerCommand (const_cast <char *> (command), func);
}

void Game::playSound (edict_t *ent, const char *sound) {
   if (isNullEntity (ent)) {
      return;
   }
   engfuncs.pfnEmitSound (ent, CHAN_WEAPON, sound, 1.0f, ATTN_NORM, 0, 100);
}

bool Game::checkVisibility (edict_t *ent, uint8 *set) {
   if (!set) {
      return true;
   }

   if (ent->headnode < 0) {
      for (int i = 0; i < ent->num_leafs; ++i) {
         auto leaf = ent->leafnums[i];

         if (set[leaf >> 3] & cr::bit (leaf & 7)) {
            return true;
         }
      }
      return false;
   }

   for (int i = 0; i < 48; ++i) {
      auto leaf = ent->leafnums[i];
      if (leaf == -1) {
         break;
      }

      if (set[leaf >> 3] & cr::bit (leaf & 7)) {
         return true;
      }
   }
   return engfuncs.pfnCheckVisibility (ent, set) > 0;
}

uint8 *Game::getVisibilitySet (Bot *bot, bool pvs) {
   if (is (GameFlags::Xash3D)) {
      return nullptr;
   }
   auto eyes = bot->getEyesPos ();

   if (bot->pev->flags & FL_DUCKING) {
      eyes += VEC_HULL_MIN - VEC_DUCK_HULL_MIN;
   }
   float org[3] { eyes.x, eyes.y, eyes.z };

   return pvs ? engfuncs.pfnSetFatPVS (org) : engfuncs.pfnSetFatPAS (org);
}

void Game::sendClientMessage (bool console, edict_t *ent, const char *message) {
   // helper to sending the client message

   MessageWriter (MSG_ONE, msgs.id (NetMsg::TextMsg), nullptr, ent)
      .writeByte (console ? HUD_PRINTCONSOLE : HUD_PRINTCENTER)
      .writeString (message);
}

void Game::prepareBotArgs (edict_t *ent, String str) {
   // the purpose of this function is to provide fakeclients (bots) with the same client
   // command-scripting advantages (putting multiple commands in one line between semicolons)
   // as real players. It is an improved version of botman's FakeClientCommand, in which you
   // supply directly the whole string as if you were typing it in the bot's "console". It
   // is supposed to work exactly like the pfnClientCommand (server-sided client command).

   if (str.empty ()) {
      return;
   }
   m_isBotCommand = true;
   m_botArgs.clear ();

   // helper to parse single (not multi) command
   auto parsePartArgs = [&] (String &args) {
      args.trim ("\r\n\t\" "); // trim new lines

      // we're have empty commands?
      if (args.empty ()) {
         return;
      }

      // find first space
      const size_t space = args.find (' ', 0);

      // if found space
      if (space != String::InvalidIndex) {
         const auto quote = space + 1; // check for quote next to space

         // check if we're got a quoted string
         if (quote < args.length () && args[quote] == '\"') {
            m_botArgs.push (cr::move (args.substr (0, space))); // add command
            m_botArgs.push (cr::move (args.substr (quote, args.length () - 1).trim ("\""))); // add string with trimmed quotes
         }
         else {
            for (auto &arg : args.split (" ")) {
               m_botArgs.push (cr::move (arg));
            }
         }
      }
      else {
         m_botArgs.push (cr::move (args)); // move all the part to args
      }
      MDLL_ClientCommand (ent);
      m_botArgs.clear (); // clear space for next cmd 
   };

   if (str.find (';', 0) != String::InvalidIndex) {
      for (auto &part : str.split (";")) {
         parsePartArgs (part);
      }
   }
   else {
      parsePartArgs (str);
   }
   m_isBotCommand = false;
}

bool Game::isSoftwareRenderer () {

   // xash always use "hw" structures
   if (is (GameFlags::Xash3D)) {
      return false;
   }

   // dedicated server (except xash) always use "sw" structures
   if (isDedicated ()) {
      return true;
   }

   // and on only windows version you can use software-render game. Linux, OSX always defaults to OpenGL
   if (plat.isWindows) {
      return plat.hasModule ("sw");
   }
   return false;
}

void Game::addNewCvar (const char *variable, const char *value, Var varType, bool regMissing, const char *regVal, ConVar *self) {
   // this function adds globally defined variable to registration stack

   VarPair pair {};

   pair.reg.name = const_cast <char *> (variable);
   pair.reg.string = const_cast <char *> (value);
   pair.missing = regMissing;
   pair.regval = regVal;

   int engineFlags = FCVAR_EXTDLL;

   if (varType == Var::Normal) {
      engineFlags |= FCVAR_SERVER;
   }
   else if (varType == Var::ReadOnly) {
      engineFlags |= FCVAR_SERVER | FCVAR_SPONLY | FCVAR_PRINTABLEONLY;
   }
   else if (varType == Var::Password) {
      engineFlags |= FCVAR_PROTECTED;
   }

   pair.reg.flags = engineFlags;
   pair.self = self;
   pair.type = varType;

   m_cvars.push (cr::move (pair));
}

void Game::registerCvars (bool gameVars) {
   // this function pushes all added global variables to engine registration

   for (auto &var : m_cvars) {
      ConVar &self = *var.self;
      cvar_t &reg = var.reg;

      if (var.type != Var::NoRegister) {
         self.eptr = engfuncs.pfnCVarGetPointer (reg.name);

         if (!self.eptr) {
            static cvar_t reg_;

            // fix metamod' memlocs not found
            if (is (GameFlags::Metamod)) {
               reg_ = var.reg;
               engfuncs.pfnCVarRegister (&reg_);
            }
            else {
               engfuncs.pfnCVarRegister (&var.reg);
            }
            self.eptr = engfuncs.pfnCVarGetPointer (reg.name);
         }
      }
      else if (gameVars) {
         self.eptr = engfuncs.pfnCVarGetPointer (reg.name);

         if (var.missing && !self.eptr) {
            if (reg.string == nullptr && var.regval != nullptr) {
               reg.string = const_cast <char *> (var.regval);
               reg.flags |= FCVAR_SERVER;
            }
            engfuncs.pfnCVarRegister (&var.reg);
            self.eptr = engfuncs.pfnCVarGetPointer (reg.name);
         }

         if (!self.eptr) {
            print ("Got nullptr on cvar %s!", reg.name);
         }
      }
   }
}

bool Game::loadCSBinary () {
   auto modname = getModName ();

   if (!modname) {
      return false;
   }
   StringArray libs;

   if (plat.isWindows) {
      libs.push ("mp.dll");
      libs.push ("cs.dll");
   }
   else if (plat.isLinux) {
      libs.push ("cs.so");
      libs.push ("cs_i386.so");
   }
   else if (plat.isOSX) {
      libs.push ("cs.dylib");
   }

   auto libCheck = [&] (const String &mod, const String &dll) {
      // try to load gamedll
      if (!m_gameLib) {
         logger.fatal ("Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", dll.chars (), mod.chars ());
      }
      auto ent = m_gameLib.resolve <EntityFunction> ("trigger_random_unique");

      // detect regamedll by addon entity they provide
      if (ent != nullptr) {
         m_gameFlags |= GameFlags::ReGameDLL;
      }
      return true;
   };
   
   // search the libraries inside game dlls directory
   for (const auto &lib : libs) {
      auto path = strings.format ("%s/dlls/%s", modname, lib.chars ());

      // if we can't read file, skip it
      if (!File::exists (path)) {
         continue;
      }
      
      // special case, czero is always detected first, as it's has custom directory
      if (strcmp (modname, "czero") == 0) {
         m_gameFlags |= (GameFlags::ConditionZero | GameFlags::HasBotVoice | GameFlags::HasFakePings);

         if (is (GameFlags::Metamod)) {
            return false;
         }
         m_gameLib.load (path);

         // verify dll is OK 
         return libCheck (modname, lib);
      }
      else {
         m_gameLib.load (path);

         // verify dll is OK 
         if (!libCheck (modname, lib)) {
            return false;
         }

         // detect if we're running modern game
         auto entity = m_gameLib.resolve <EntityFunction> ("weapon_famas");

         // detect xash engine
         if (engfuncs.pfnCVarGetPointer ("build") != nullptr) {
            m_gameFlags |= (GameFlags::Legacy | GameFlags::Xash3D);

            if (entity != nullptr) {
               m_gameFlags |= GameFlags::HasBotVoice;
            }

            if (is (GameFlags::Metamod)) {
               return false;
            }
            return true;
         }
         
         if (entity != nullptr) {
            m_gameFlags |= (GameFlags::Modern | GameFlags::HasBotVoice | GameFlags::HasFakePings);
         }
         else {
            m_gameFlags |= GameFlags::Legacy;
         }

         if (is (GameFlags::Metamod)) {
            return false;
         }
         return true;
      }
   }
   return false;
}

bool Game::postload () {

   // ensure we're have all needed directories
   for (const auto &dir : StringArray { "conf/lang", "data/learned", "data/graph", "data/logs" }) {
      File::createPath (strings.format ("%s/addons/yapb/%s", getModName (), dir.chars ()));
   }

   // set out user agent for http stuff
   http.setUserAgent (strings.format ("%s/%s", PRODUCT_SHORT_NAME, PRODUCT_VERSION));

   // register bot cvars
   game.registerCvars ();


   // register server command(s)
   registerEngineCommand ("yapb", [] () {
      ctrl.handleEngineCommands ();
      });

   registerEngineCommand ("yb", [] () {
      ctrl.handleEngineCommands ();
      });
   
   // register fake metamod command handler if we not! under mm
   if (!(game.is (GameFlags::Metamod))) {
      game.registerEngineCommand ("meta", [] () {
         game.print ("You're launched standalone version of %s. Metamod is not installed or not enabled!", PRODUCT_SHORT_NAME);
      });
   }

   // initialize weapons
   conf.initWeapons ();

   // print game detection info
   auto displayCSVersion = [&] () {
      String gameVersionStr;
      StringArray gameVersionFlags;

      if (is (GameFlags::Legacy)) {
         gameVersionStr.assign ("Legacy");
      }
      else if (is (GameFlags::ConditionZero)) {
         gameVersionStr.assign ("Condition Zero");
      }
      else if (is (GameFlags::Modern)) {
         gameVersionStr.assign ("v1.6");
      }

      if (is (GameFlags::Xash3D)) {
         gameVersionStr.append (" @ Xash3D Engine");

         if (is (GameFlags::Mobility)) {
            gameVersionStr.append (" Mobile");
         }
         gameVersionStr.replace ("Legacy", "1.6 Limited");
      }

      if (is (GameFlags::HasBotVoice)) {
         gameVersionFlags.push ("BotVoice");
      }

      if (is (GameFlags::ReGameDLL)) {
         gameVersionFlags.push ("ReGameDLL");
      }

      if (is (GameFlags::HasFakePings)) {
         gameVersionFlags.push ("FakePing");
      }

      if (is (GameFlags::Metamod)) {
         gameVersionFlags.push ("Metamod");
      }
      print ("%s v%s.0.%d successfully loaded for game: Counter-Strike %s (%s).\n", PRODUCT_SHORT_NAME, PRODUCT_VERSION, util.buildNumber (), gameVersionStr.chars (), String::join (gameVersionFlags, ", ").chars ());
   };

   if (plat.isAndroid) {
      m_gameFlags |= (GameFlags::Xash3D | GameFlags::Mobility | GameFlags::HasBotVoice | GameFlags::ReGameDLL);

      if (is (GameFlags::Metamod)) {
         return true; // we should stop the attempt for loading the real gamedll, since metamod handle this for us
      }
      auto gamedll = strings.format ("%s/%s", getenv ("XASH3D_GAMELIBDIR"), plat.isAndroidHardFP ? "libserver_hardfp.so" : "libserver.so");

      if (!m_gameLib.load (gamedll)) {
         logger.fatal ("Unable to load gamedll \"%s\". Exiting... (gamedir: %s)", gamedll, getModName ());
      }
      displayCSVersion ();

   }
   else {
      bool binaryLoaded = loadCSBinary ();

      if (!binaryLoaded && !is (GameFlags::Metamod)) {
         logger.fatal ("Mod that you has started, not supported by this bot (gamedir: %s)", getModName ());
      }
      displayCSVersion ();

      if (is (GameFlags::Metamod)) {
         m_gameLib.unload ();
         return true;
      }
   }
   return false;
}

void Game::detectDeathmatch () {
   if (!is (GameFlags::Metamod | GameFlags::ReGameDLL)) {
      return;
   }
   static auto dmActive = engfuncs.pfnCVarGetPointer ("csdm_active");
   static auto freeForAll = engfuncs.pfnCVarGetPointer ("mp_freeforall");

   // csdm is only with amxx and metamod
   if (dmActive) {
      if (dmActive->value > 0.0f) {
         m_gameFlags |= GameFlags::CSDM;
      }
      else if (is (GameFlags::CSDM)) {
         m_gameFlags &= ~GameFlags::CSDM;
      }
   }

   // but this can be provided by regamedll
   if (freeForAll) {
      if (freeForAll->value > 0.0f) {
         m_gameFlags |= GameFlags::FreeForAll;
      }
      else if (is (GameFlags::FreeForAll)) {
         m_gameFlags &= ~GameFlags::FreeForAll;
      }
   }
}

void Game::slowFrame () {
   if (m_slowFrame > time ()) {
      return;
   }
   ctrl.maintainAdminRights ();

   // update bot difficulties to newly selected from cvar
   bots.updateBotDifficulties ();

   // update client pings
   util.calculatePings ();

   // initialize light levels
   graph.initLightLevels ();

   // detect csdm
   detectDeathmatch ();

   // display welcome message
   util.checkWelcome ();
   m_slowFrame = time () + 1.0f;
}

void Game::searchEntities (const String &field, const String &value, EntitySearch functor) {
   edict_t *ent = nullptr;

   while (!game.isNullEntity (ent = engfuncs.pfnFindEntityByString (ent, field.chars (), value.chars ()))) {
      if ((ent->v.flags & EF_NODRAW) || (ent->v.flags & FL_CLIENT)) {
         continue;
      }

      if (functor (ent) == EntitySearchResult::Break) {
         break;
      }
   }
}

void Game::searchEntities (const Vector &position, const float radius, EntitySearch functor) {
   edict_t *ent = nullptr;
   const Vector &pos = position.empty () ? m_startEntity->v.origin : position;

   while (!game.isNullEntity (ent = engfuncs.pfnFindEntityInSphere (ent, pos, radius))) {
      if ((ent->v.flags & EF_NODRAW) || (ent->v.flags & FL_CLIENT)) {
         continue;
      }

      if (functor (ent) == EntitySearchResult::Break) {
         break;
      }
   }
}

void LightMeasure::initializeLightstyles () {
   // this function initializes lighting information...

   // reset all light styles
   for (auto &ls : m_lightstyle) {
      ls.length = 0;
      ls.map[0] = 0;
   }

   for (auto &lsv : m_lightstyleValue) {
      lsv = 264;
   }
}

void LightMeasure::animateLight () {
   // this function performs light animations

   if (!m_doAnimation) {
      return;
   }

   // 'm' is normal light, 'a' is no light, 'z' is double bright
   const int index = static_cast <int> (game.time () * 10.0f);

   for (int j = 0; j < MAX_LIGHTSTYLES; ++j) {
      if (!m_lightstyle[j].length) {
         m_lightstyleValue[j] = 256;
         continue;
      }
      int value = m_lightstyle[j].map[index % m_lightstyle[j].length] - 'a';
      m_lightstyleValue[j] = value * 22;
   }
}

void LightMeasure::updateLight (int style, char *value) {
   if (!m_doAnimation) {
      return;
   }

   if (style >= MAX_LIGHTSTYLES) {
      return;
   }

   if (strings.isEmpty (value)){
      m_lightstyle[style].length = 0u;
      m_lightstyle[style].map[0] = '\0';

      return;
   }
   const auto copyLimit = sizeof (m_lightstyle[style].map) - sizeof ('\0');
   strncpy (m_lightstyle[style].map, value, copyLimit);

   m_lightstyle[style].map[copyLimit] = '\0';
   m_lightstyle[style].length = strlen (m_lightstyle[style].map);
}

template <typename S, typename M> bool LightMeasure::recursiveLightPoint (const M *node, const Vector &start, const Vector &end) {
   if (!node || node->contents < 0) {
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

   for (int i = 0; i < node->numsurfaces; ++i, ++surf) {
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
   if (game.is (GameFlags::Legacy) && !game.is (GameFlags::Xash3D)) {
      return 0.0f;
   }

   if (!m_worldModel) {
      return 0.0f;
   }

   if (!m_worldModel->lightdata) {
      return 255.0f;
   }

   Vector endPoint (point);
   endPoint.z -= 2048.0f;

   // it's depends if we're are on dedicated or on listenserver
   auto recursiveCheck = [&] () -> bool {
      if (!game.isSoftwareRenderer ()) {
         return recursiveLightPoint <msurface_hw_t, mnode_hw_t> (reinterpret_cast <mnode_hw_t *> (m_worldModel->nodes), point, endPoint);
      }
      return recursiveLightPoint <msurface_t, mnode_t> (m_worldModel->nodes, point, endPoint);
   };
   return !recursiveCheck () ? 0.0f : 100 * cr::sqrtf (cr::min (75.0f, static_cast <float> (m_point.avg ())) / 75.0f);
}

float LightMeasure::getSkyColor () {
   return static_cast <float> (Color (sv_skycolor_r.int_ (), sv_skycolor_g.int_ (), sv_skycolor_b.int_ ()).avg ());
}

DynamicEntityLink::Handle DynamicEntityLink::search (Handle module, Name function) {
   const auto lookup = [&] (Handle handle) {
      Handle ret = nullptr;

      if (m_dlsym.disable ()) {
         ret = LookupSymbol (reinterpret_cast <CastType> (handle), function);
         m_dlsym.enable ();
      }
      return ret;
   };
   static const auto &gamedll = game.lib ();
   static const auto &yapb = m_self;

   // if requested module is yapb, put in cache the looked up symbol
   if (yapb.handle () == module) {
      if (m_exports.exists (function)) {
         return m_exports[function];
      }
      auto address = lookup (yapb.handle ());

      if (!address) {
         auto gameAddress = lookup (gamedll.handle ());

         if (gameAddress) {
            m_exports[function] = gameAddress;
         }
      }
      else {
         m_exports[function] = address;
      }

      if (m_exports.exists (function)) {
         return m_exports[function];
      }
   }
   return lookup (module);
}
