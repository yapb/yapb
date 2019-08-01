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

   resetMessages ();

   for (auto &msg : m_msgBlock.regMsgs) {
      msg = NetMsg::None;
   }
   m_precached = false;
   m_isBotCommand = false;

   memset (m_drawModels, 0, sizeof (m_drawModels));
   memset (m_spawnCount, 0, sizeof (m_spawnCount));

   m_gameFlags = 0;
   m_mapFlags = 0;
   m_slowFrame = 0.0;

   m_cvars.clear ();
}

Game::~Game () {
   resetMessages ();
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

         m_spawnCount[Team::CT]++;
      }
      else if (strcmp (classname, "info_player_deathmatch") == 0) {
         engfuncs.pfnSetModel (ent, ENGINE_STR ("models/player/terror/terror.mdl"));

         ent->v.rendermode = kRenderTransAlpha; // set its render mode to transparency
         ent->v.renderamt = 127; // set its transparency amount
         ent->v.effects |= EF_NODRAW;

         m_spawnCount[Team::Terrorist]++;
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

   MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullvec, ent)
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

   if (slash != String::kInvalidIndex) {
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
      return nullvec;
   }

   if (ent->v.origin.empty ()) {
      return ent->v.absmin + (ent->v.size * 0.5);
   }
   return ent->v.origin;
}

void Game::registerCmd (const char *command, void func ()) {
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

void Game::sendClientMessage (bool console, edict_t *ent, const char *message) {
   // helper to sending the client message

   MessageWriter (MSG_ONE, getMessageId (NetMsg::TextMsg), nullvec, ent)
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
      if (space != String::kInvalidIndex) {
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

   if (str.find (';', 0) != String::kInvalidIndex) {
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

const char *Game::translate (const char *input) {
   // this function translate input string into needed language

   if (isDedicated ()) {
      return input;
   }
   static String result;

   if (m_language.find (input, result)) {
      return result.chars ();
   }
   return input; // nothing found
}

void Game::processMessages (void *ptr) {
   if (m_msgBlock.msg == NetMsg::None) {
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
   auto bot = bots[m_msgBlock.bot];

   auto strVal = reinterpret_cast <char *> (ptr);
   auto intVal = *reinterpret_cast <int *> (ptr);
   auto byteVal = *reinterpret_cast <uint8 *> (ptr);

   // now starts of network message execution
   switch (m_msgBlock.msg) {
   case NetMsg::VGUI:
      // this message is sent when a VGUI menu is displayed.

      if (bot != nullptr && m_msgBlock.state == 0) {
         switch (intVal) {
         case GuiMenu::TeamSelect:
            bot->m_startAction = BotMsg::TeamSelect;
            break;

         case GuiMenu::TerroristSelect:
         case GuiMenu::CTSelect:
            bot->m_startAction = BotMsg::ClassSelect;
            break;
         }
      }
      break;

   case NetMsg::ShowMenu:
      // this message is sent when a text menu is displayed.

      // ignore first 3 fields of message
      if (m_msgBlock.state < 3 || bot == nullptr) {
         break;
      }

      if (strcmp (strVal, "#Team_Select") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#Team_Select_Spect") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#IG_Team_Select_Spect") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#IG_Team_Select") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#IG_VIP_Team_Select") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#IG_VIP_Team_Select_Spect") == 0) {
         bot->m_startAction = BotMsg::TeamSelect;
      }
      else if (strcmp (strVal, "#Terrorist_Select") == 0) {
         bot->m_startAction = BotMsg::ClassSelect;
      }
      else if (strcmp (strVal, "#CT_Select") == 0) {
         bot->m_startAction = BotMsg::ClassSelect;
      }
      break;

   case NetMsg::WeaponList:
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

   case NetMsg::CurWeapon:
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

   case NetMsg::AmmoX:
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

   case NetMsg::AmmoPickup:
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

   case NetMsg::Damage:
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

   case NetMsg::Money:
      // this message gets sent when the bots money amount changes

      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_moneyAmount = intVal; // amount of money
      }
      break;

   case NetMsg::StatusIcon:
      switch (m_msgBlock.state) {
      case 0:
         enabled = byteVal;
         break;

      case 1:
         if (bot != nullptr) {
            if (strcmp (strVal, "buyzone") == 0) {
               bot->m_inBuyZone = (enabled != 0);

               // try to equip in buyzone
               bot->processBuyzoneEntering (BuyState::PrimaryWeapon);
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

   case NetMsg::DeathMsg: // this message sends on death
      switch (m_msgBlock.state) {
      case 0:
         killerIndex = intVal;
         break;

      case 1:
         victimIndex = intVal;
         break;

      case 2:
         if (killerIndex != 0 && killerIndex != victimIndex) {
            edict_t *killer = entityOfIndex (killerIndex);
            edict_t *victim = entityOfIndex (victimIndex);

            if (isNullEntity (killer) || isNullEntity (victim)) {
               break;
            }

            if (yb_radio_mode.int_ () == 2) {
               // need to send congrats on well placed shot
               for (const auto &notify : bots) {
                  if (notify->m_notKilled && killer != notify->ent () && notify->seesEntity (victim->v.origin) && getTeam (killer) == notify->m_team && getTeam (killer) != getTeam (victim)) {
                     if (!bots[killer]) {
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
            for (const auto &notify : bots) {
               if (notify->m_seeEnemyTime + 2.0f < timebase () && notify->m_notKilled && notify->m_team == getTeam (victim) && util.isVisible (killer->v.origin, notify->ent ()) && isNullEntity (notify->m_enemy) && getTeam (killer) != getTeam (victim)) {
                  notify->m_actualReactionTime = 0.0f;
                  notify->m_seeEnemyTime = timebase ();
                  notify->m_enemy = killer;
                  notify->m_lastEnemy = killer;
                  notify->m_lastEnemyOrigin = killer->v.origin;
               }
            }

            auto notify = bots[killer];

            // is this message about a bot who killed somebody?
            if (notify != nullptr) {
               notify->m_lastVictim = victim;
            }
            else // did a human kill a bot on his team?
            {
               auto target = bots[victim];

               if (target != nullptr) {
                  if (getTeam (killer) == target->m_team) {
                     target->m_voteKickIndex = killerIndex;
                  }
                  target->m_notKilled = false;
               }
            }
         }
         break;
      }
      break;

   case NetMsg::ScreenFade: // this message gets sent when the screen fades (flashbang)
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

   case NetMsg::HLTV: // round restart in steam cs
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

   case NetMsg::TextMsg:
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
               bots.setLastWinner (Team::CT); // update last winner for economics

               if (yb_radio_mode.int_ () == 2) {
                  Bot *notify = bots.findAliveBot ();

                  if (notify != nullptr && notify->m_notKilled) {
                     notify->processChatterMessage (strVal);
                  }
               }
            }

            if (strcmp (strVal, "#Game_will_restart_in") == 0) {
               bots.updateTeamEconomics (Team::CT, true);
               bots.updateTeamEconomics (Team::Terrorist, true);
            }

            if (strcmp (strVal, "#Terrorists_Win") == 0) {
               bots.setLastWinner (Team::Terrorist); // update last winner for economics

               if (yb_radio_mode.int_ () == 2) {
                  Bot *notify = bots.findAliveBot ();

                  if (notify != nullptr && notify->m_notKilled) {
                     notify->processChatterMessage (strVal);
                  }
               }
            }
            graph.setBombPos (true);
         }
         else if (!bots.isBombPlanted () && strcmp (strVal, "#Bomb_Planted") == 0) {
            bots.setBombPlanted (true);

            for (const auto &notify : bots) {
               if (notify->m_notKilled) {
                  notify->clearSearchNodes ();
                  notify->clearTasks ();

                  if (yb_radio_mode.int_ () == 2 && rg.chance (55) && notify->m_team == Team::CT) {
                     notify->pushChatterMessage (Chatter::WhereIsTheC4);
                  }
               }
            }
            graph.setBombPos ();
         }
         else if (bot != nullptr && strcmp (strVal, "#Switch_To_BurstFire") == 0) {
            bot->m_weaponBurstMode = BurstMode::On;
         }
         else if (bot != nullptr && strcmp (strVal, "#Switch_To_SemiAuto") == 0) {
            bot->m_weaponBurstMode = BurstMode::Off;
         }
      }
      break;

   case NetMsg::TeamInfo:
      switch (m_msgBlock.state) {
      case 0:
         playerIndex = intVal;
         break;

      case 1:
         if (playerIndex > 0 && playerIndex <= maxClients ()) {
            int team = Team::Unassigned;

            if (strVal[0] == 'U' && strVal[1] == 'N') {
               team = Team::Unassigned;
            }
            else if (strVal[0] == 'T' && strVal[1] == 'E') {
               team = Team::Terrorist;
            }
            else if (strVal[0] == 'C' && strVal[1] == 'T') {
               team = Team::CT;
            }
            else if (strVal[0] == 'S' && strVal[1] == 'P') {
               team = Team::Spectator;
            }
            auto &client = util.getClient (playerIndex - 1);

            client.team2 = team;
            client.team = is (GameFlags::FreeForAll) ? playerIndex : team;
         }
         break;
      }
      break;

   case NetMsg::BarTime:
      if (bot != nullptr && m_msgBlock.state == 0) {
         if (intVal > 0) {
            bot->m_hasProgressBar = true; // the progress bar on a hud

            if (mapIs (MapFlags::Demolition) && bots.isBombPlanted () && bot->m_team == Team::CT) {
               bots.notifyBombDefuse ();
            }
         }
         else if (intVal == 0) {
            bot->m_hasProgressBar = false; // no progress bar or disappeared
         }
      }
      break;

   case NetMsg::ItemStatus:
      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_hasNVG = (intVal & ItemStatus::Nightvision) ? true : false;
         bot->m_hasDefuser = (intVal & ItemStatus::DefusalKit) ? true : false;
      }
      break;

   case NetMsg::FlashBat:
      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_flashLevel = static_cast <float> (intVal);
      }
      break;

   case NetMsg::NVGToggle:
      if (bot != nullptr && m_msgBlock.state == 0) {
         bot->m_usesNVG = intVal > 0;
      }
      break;

   default:
      logger.error ("Network message handler error. Call to unrecognized message id (%d).\n", m_msgBlock.msg);
   }
   m_msgBlock.state++; // and finally update network message state
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
      auto *path = strings.format ("%s/dlls/%s", modname, lib.chars ());

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
   const char *mod = getModName ();

   // create the needed paths
   File::createPath (strings.format ("%s/addons/yapb/conf/lang", mod));
   File::createPath (strings.format ("%s/addons/yapb/data/learned", mod));
   File::createPath (strings.format ("%s/addons/yapb/data/graph", mod));
   File::createPath (strings.format ("%s/addons/yapb/data/logs", mod));

   // set out user agent for http stuff
   http.setUserAgent (strings.format ("%s/%s", PRODUCT_SHORT_NAME, PRODUCT_VERSION));

   // print game detection info
   auto printGame = [&] () {
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
      printGame ();

   }
   else {
      bool binaryLoaded = loadCSBinary ();

      if (!binaryLoaded && !is (GameFlags::Metamod)) {
         logger.fatal ("Mod that you has started, not supported by this bot (gamedir: %s)", getModName ());
      }
      printGame ();

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
   if (m_slowFrame > timebase ()) {
      return;
   }
   ctrl.maintainAdminRights ();

   // calculate light levels for all waypoints if needed
   graph.initLightLevels ();

   // update bot difficulties to newly selected from cvar
   bots.updateBotDifficulties ();

   // update client pings
   util.calculatePings ();

   // detect csdm
   detectDeathmatch ();

   // display welcome message
   util.checkWelcome ();
   m_slowFrame = timebase () + 1.0f;
}

void Game::beginMessage (edict_t *ent, int dest, int type) {
   // store the message type in our own variables, since the GET_USER_MSG_ID () will just do a lot of strcmp()'s...
   if (is (GameFlags::Metamod) && getMessageId (NetMsg::Money) == -1) {

      auto setMsgId = [&] (const char *name, NetMsg id) {
         setMessageId (id, GET_USER_MSG_ID (PLID, name, nullptr));
      };
      setMsgId ("VGUIMenu", NetMsg::VGUI);
      setMsgId ("ShowMenu", NetMsg::ShowMenu);
      setMsgId ("WeaponList", NetMsg::WeaponList);
      setMsgId ("CurWeapon", NetMsg::CurWeapon);
      setMsgId ("AmmoX", NetMsg::AmmoX);
      setMsgId ("AmmoPickup", NetMsg::AmmoPickup);
      setMsgId ("Damage", NetMsg::Damage);
      setMsgId ("Money", NetMsg::Money);
      setMsgId ("StatusIcon", NetMsg::StatusIcon);
      setMsgId ("DeathMsg", NetMsg::DeathMsg);
      setMsgId ("ScreenFade", NetMsg::ScreenFade);
      setMsgId ("HLTV", NetMsg::HLTV);
      setMsgId ("TextMsg", NetMsg::TextMsg);
      setMsgId ("TeamInfo", NetMsg::TeamInfo);
      setMsgId ("BarTime", NetMsg::BarTime);
      setMsgId ("SendAudio", NetMsg::SendAudio);
      setMsgId ("SayText", NetMsg::SayText);
      setMsgId ("FlashBat", NetMsg::FlashBat);
      setMsgId ("Flashlight", NetMsg::Fashlight);
      setMsgId ("NVGToggle", NetMsg::NVGToggle);
      setMsgId ("ItemStatus", NetMsg::ItemStatus);

      if (is (GameFlags::HasBotVoice)) {
         setMessageId (NetMsg::BotVoice, GET_USER_MSG_ID (PLID, "BotVoice", nullptr));
      }
   }

   if ((!is (GameFlags::Legacy) || is (GameFlags::Xash3D)) && dest == MSG_SPEC && type == getMessageId (NetMsg::HLTV)) {
      setCurrentMessageId (NetMsg::HLTV);
   }
   captureMessage (type, NetMsg::WeaponList);

   if (!isNullEntity (ent) && !(ent->v.flags & FL_DORMANT)) {
      auto bot = bots[ent];

      // is this message for a bot?
      if (bot != nullptr) {
         setCurrentMessageOwner (bot->index ());

         // message handling is done in usermsg.cpp
         captureMessage (type, NetMsg::VGUI);
         captureMessage (type, NetMsg::CurWeapon);
         captureMessage (type, NetMsg::AmmoX);
         captureMessage (type, NetMsg::AmmoPickup);
         captureMessage (type, NetMsg::Damage);
         captureMessage (type, NetMsg::Money);
         captureMessage (type, NetMsg::StatusIcon);
         captureMessage (type, NetMsg::ScreenFade);
         captureMessage (type, NetMsg::BarTime);
         captureMessage (type, NetMsg::TextMsg);
         captureMessage (type, NetMsg::ShowMenu);
         captureMessage (type, NetMsg::FlashBat);
         captureMessage (type, NetMsg::NVGToggle);
         captureMessage (type, NetMsg::ItemStatus);
      }
   }
   else if (dest == MSG_ALL) {
      captureMessage (type, NetMsg::TeamInfo);
      captureMessage (type, NetMsg::DeathMsg);
      captureMessage (type, NetMsg::TextMsg);

      if (type == SVC_INTERMISSION) {
         for (const auto &bot : bots) {
            bot->m_notKilled = false;
         }
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
   const int index = static_cast <int> (game.timebase () * 10.0f);

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

   if (util.isEmptyStr (value)){
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
