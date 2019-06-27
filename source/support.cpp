//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_display_welcome_text ("yb_display_welcome_text", "1");

BotUtils::BotUtils (void) {
   m_needToSendWelcome = false;
   m_welcomeReceiveTime = 0.0f;

   // add default messages
   m_sentences.push ("hello user,communication is acquired");
   m_sentences.push ("your presence is acknowledged");
   m_sentences.push ("high man, your in command now");
   m_sentences.push ("blast your hostile for good");
   m_sentences.push ("high man, kill some idiot here");
   m_sentences.push ("is there a doctor in the area");
   m_sentences.push ("warning, experimental materials detected");
   m_sentences.push ("high amigo, shoot some but");
   m_sentences.push ("attention, hours of work software, detected");
   m_sentences.push ("time for some bad ass explosion");
   m_sentences.push ("bad ass son of a breach device activated");
   m_sentences.push ("high, do not question this great service");
   m_sentences.push ("engine is operative, hello and goodbye");
   m_sentences.push ("high amigo, your administration has been great last day");
   m_sentences.push ("attention, expect experimental armed hostile presence");
   m_sentences.push ("warning, medical attention required");

   m_clients.resize (MAX_ENGINE_PLAYERS + 1);
}

const char *BotUtils::format (const char *format, ...) {
   static char strBuffer[2][MAX_PRINT_BUFFER];
   static int rotator = 0;

   if (format == nullptr) {
      return strBuffer[rotator];
   }
   static char *ptr = strBuffer[rotator ^= 1];

   va_list ap;
   va_start (ap, format);
   vsnprintf (ptr, MAX_PRINT_BUFFER - 1, format, ap);
   va_end (ap);

   return ptr;
}

bool BotUtils::isAlive (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   return ent->v.deadflag == DEAD_NO && ent->v.health > 0 && ent->v.movetype != MOVETYPE_NOCLIP;
}

float BotUtils::getShootingCone (edict_t *ent, const Vector &position) {
   game.makeVectors (ent->v.v_angle);

   // he's facing it, he meant it
   return game.vec.forward | (position - (ent->v.origin + ent->v.view_ofs)).normalize ();
}

bool BotUtils::isInViewCone (const Vector &origin, edict_t *ent) {
   return getShootingCone (ent, origin) >= cr::cosf (cr::deg2rad ((ent->v.fov > 0 ? ent->v.fov : 90.0f) * 0.5f));
}

bool BotUtils::isVisible (const Vector &origin, edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   TraceResult tr;
   game.testLine (ent->v.origin + ent->v.view_ofs, origin, TRACE_IGNORE_EVERYTHING, ent, &tr);

   if (tr.flFraction != 1.0f) {
      return false;
   }
   return true;
}

void BotUtils::traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex) {
   // this function draw spraypaint depending on the tracing results.

   static StringArray logotypes;

   if (logotypes.empty ()) {
      logotypes = String ("{biohaz;{graf003;{graf004;{graf005;{lambda06;{target;{hand1;{spit2;{bloodhand6;{foot_l;{foot_r").split (";");
   }
   int entityIndex = -1, message = TE_DECAL;
   int decalIndex = engfuncs.pfnDecalIndex (logotypes[logotypeIndex].chars ());

   if (decalIndex < 0) {
      decalIndex = engfuncs.pfnDecalIndex ("{lambda06");
   }

   if (trace->flFraction == 1.0f) {
      return;
   }
   if (!game.isNullEntity (trace->pHit)) {
      if (trace->pHit->v.solid == SOLID_BSP || trace->pHit->v.movetype == MOVETYPE_PUSHSTEP) {
         entityIndex = game.indexOfEntity (trace->pHit);
      }
      else {
         return;
      }
   }
   else {
      entityIndex = 0;
   }

   if (entityIndex != 0) {
      if (decalIndex > 255) {
         message = TE_DECALHIGH;
         decalIndex -= 256;
      }
   }
   else {
      message = TE_WORLDDECAL;

      if (decalIndex > 255) {
         message = TE_WORLDDECALHIGH;
         decalIndex -= 256;
      }
   }

   if (logotypes[logotypeIndex].contains ("{")) {
      MessageWriter (MSG_BROADCAST, SVC_TEMPENTITY)
         .writeByte (TE_PLAYERDECAL)
         .writeByte (game.indexOfEntity (pev->pContainingEntity))
         .writeCoord (trace->vecEndPos.x)
         .writeCoord (trace->vecEndPos.y)
         .writeCoord (trace->vecEndPos.z)
         .writeShort (static_cast <short> (game.indexOfEntity (trace->pHit)))
         .writeByte (decalIndex);
   }
   else {
      MessageWriter msg;

      msg.start (MSG_BROADCAST, SVC_TEMPENTITY)
      .writeByte (message)
      .writeCoord (trace->vecEndPos.x)
      .writeCoord (trace->vecEndPos.y)
      .writeCoord (trace->vecEndPos.z)
      .writeByte (decalIndex);

      if (entityIndex) {
         msg.writeShort (entityIndex);
      }
      msg.end ();
   }
}

bool BotUtils::isPlayer (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }

   if (ent->v.flags & FL_PROXY) {
      return false;
   }

   if ((ent->v.flags & (FL_CLIENT | FL_FAKECLIENT)) || bots.getBot (ent) != nullptr) {
      return !isEmptyStr (STRING (ent->v.netname));
   }
   return false;
}

bool BotUtils::isPlayerVIP (edict_t *ent) {
   if (!game.mapIs (MAP_AS)) {
      return false;
   }

   if (!isPlayer (ent)) {
      return false;
   }
   return *(engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), "model")) == 'v';
}

bool BotUtils::isFakeClient (edict_t *ent) {
   if (bots.getBot (ent) != nullptr || (!game.isNullEntity (ent) && (ent->v.flags & FL_FAKECLIENT))) {
      return true;
   }
   return false;
}

bool BotUtils::openConfig (const char *fileName, const char *errorIfNotExists, MemFile *outFile, bool languageDependant /*= false*/) {
   if (outFile->isValid ()) {
      outFile->close ();
   }

   // save config dir
   const char *configDir = "addons/yapb/conf";

   if (languageDependant) {
      extern ConVar yb_language;

      if (strcmp (fileName, "lang.cfg") == 0 && strcmp (yb_language.str (), "en") == 0) {
         return false;
      }
      const char *langConfig = format ("%s/lang/%s_%s", configDir, yb_language.str (), fileName);

      // check file existence
      int size = 0;
      uint8 *buffer = nullptr;

      // check is file is exists for this language
      if ((buffer = MemoryLoader::ref ().load (langConfig, &size)) != nullptr) {
         MemoryLoader::ref ().unload (buffer);

         // unload and reopen file using MemoryFile
         outFile->open (langConfig);
      }
      else
         outFile->open (format ("%s/lang/en_%s", configDir, fileName));
   }
   else
      outFile->open (format ("%s/%s", configDir, fileName));

   if (!outFile->isValid ()) {
      logEntry (true, LL_ERROR, errorIfNotExists);
      return false;
   }
   return true;
}

void BotUtils::checkWelcome (void) {
   // the purpose of this function, is  to send quick welcome message, to the listenserver entity.

   if (game.isDedicated () || !yb_display_welcome_text.boolean () || !m_needToSendWelcome) {
      return;
   }
   m_welcomeReceiveTime = 0.0f;

   if (game.is (GAME_LEGACY)) {
      m_needToSendWelcome = true;
      return;
   }
   bool needToSendMsg = (waypoints.length () > 0 ? m_needToSendWelcome : true);

   if (isAlive (game.getLocalEntity ()) && m_welcomeReceiveTime < 1.0 && needToSendMsg) {
      m_welcomeReceiveTime = game.timebase () + 4.0f; // receive welcome message in four seconds after game has commencing
   }

   if (m_welcomeReceiveTime > 0.0f && needToSendMsg) {
      if (!game.is (GAME_MOBILITY | GAME_XASH_ENGINE)) {
         game.execCmd ("speak \"%s\"", m_sentences.random ().chars ());
      }
      game.chatPrint ("----- %s v%s (Build: %u), {%s}, (c) %s, by %s (%s)-----", PRODUCT_SHORT_NAME, PRODUCT_VERSION, buildNumber (), PRODUCT_DATE, PRODUCT_END_YEAR, PRODUCT_AUTHOR, PRODUCT_URL);

      MessageWriter (MSG_ONE, SVC_TEMPENTITY, Vector::null (), game.getLocalEntity ())
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (1)
         .writeShort (MessageWriter::fs16 (-1, 1 << 13))
         .writeShort (MessageWriter::fs16 (-1, 1 << 13))
         .writeByte (2)
         .writeByte (rng.getInt (33, 255))
         .writeByte (rng.getInt (33, 255))
         .writeByte (rng.getInt (33, 255))
         .writeByte (0)
         .writeByte (rng.getInt (230, 255))
         .writeByte (rng.getInt (230, 255))
         .writeByte (rng.getInt (230, 255))
         .writeByte (200)
         .writeShort (MessageWriter::fu16 (0.0078125f, 1 << 8))
         .writeShort (MessageWriter::fu16 (2.0f, 1 << 8))
         .writeShort (MessageWriter::fu16 (6.0f, 1 << 8))
         .writeShort (MessageWriter::fu16 (0.1f, 1 << 8))
         .writeString (format ("\nServer is running %s v%s (Build: %u)\nDeveloped by %s\n\n%s", PRODUCT_SHORT_NAME, PRODUCT_VERSION, buildNumber (), PRODUCT_AUTHOR, waypoints.getAuthor ()));

      m_welcomeReceiveTime = 0.0f;
      m_needToSendWelcome = false;
   }
}

void BotUtils::logEntry (bool outputToConsole, int logLevel, const char *format, ...) {
   // this function logs a message to the message log file root directory.

   va_list ap;
   char buffer[MAX_PRINT_BUFFER] = { 0, }, levelString[32] = { 0, };

   va_start (ap, format);
   vsnprintf (buffer, cr::bufsize (buffer), format, ap);
   va_end (ap);

   switch (logLevel) {
   case LL_DEFAULT:
      strcpy (levelString, "LOG: ");
      break;

   case LL_WARNING:
      strcpy (levelString, "WARN: ");
      break;

   case LL_ERROR:
      strcpy (levelString, "ERROR: ");
      break;

   case LL_FATAL:
      strcpy (levelString, "FATAL: ");
      break;
   }

   if (outputToConsole) {
      game.print ("%s%s", levelString, buffer);
   }

   // now check if logging disabled
   if (!(logLevel & LL_IGNORE)) {
      extern ConVar yb_debug;

      if (logLevel == LL_DEFAULT && yb_debug.integer () < 3) {
         return; // no log, default logging is disabled
      }

      if (logLevel == LL_WARNING && yb_debug.integer () < 2) {
         return; // no log, warning logging is disabled
      }

      if (logLevel == LL_ERROR && yb_debug.integer () < 1) {
         return; // no log, error logging is disabled
      }
   }

   // open file in a standard stream
   File fp ("yapb.txt", "at");

   // check if we got a valid handle
   if (!fp.isValid ()) {
      return;
   }

   time_t tickTime = time (&tickTime);
   tm *time = localtime (&tickTime);

   fp.writeFormat ("%02d:%02d:%02d --> %s%s\n", time->tm_hour, time->tm_min, time->tm_sec, levelString, buffer);
   fp.close ();

   if (logLevel == LL_FATAL) {
      bots.kickEveryone (true);
      waypoints.init ();

#if defined(PLATFORM_WIN32)
      DestroyWindow (GetForegroundWindow ());
      MessageBoxA (GetActiveWindow (), buffer, "YaPB Error", MB_ICONSTOP);
#else
      printf ("%s\n", buffer);
#endif

#if defined(PLATFORM_WIN32)
      _exit (1);
#else
      exit (1);
#endif
   }
}

bool BotUtils::findNearestPlayer (void **pvHolder, edict_t *to, float searchDistance, bool sameTeam, bool needBot, bool isAlive, bool needDrawn, bool needBotWithC4) {
   // this function finds nearest to to, player with set of parameters, like his
   // team, live status, search distance etc. if needBot is true, then pvHolder, will
   // be filled with bot pointer, else with edict pointer(!).

   edict_t *survive = nullptr; // pointer to temporally & survive entity
   float nearestPlayer = 4096.0f; // nearest player

   int toTeam = game.getTeam (to);

   for (const auto &client : m_clients) {
      if (!(client.flags & CF_USED) || client.ent == to) {
         continue;
      }

      if ((sameTeam && client.team != toTeam) || (isAlive && !(client.flags & CF_ALIVE)) || (needBot && !isFakeClient (client.ent)) || (needDrawn && (client.ent->v.effects & EF_NODRAW)) || (needBotWithC4 && (client.ent->v.weapons & WEAPON_C4))) {
         continue; // filter players with parameters
      }
      float distance = (client.ent->v.origin - to->v.origin).length ();

      if (distance < nearestPlayer && distance < searchDistance) {
         nearestPlayer = distance;
         survive = client.ent;
      }
   }

   if (game.isNullEntity (survive))
      return false; // nothing found

   // fill the holder
   if (needBot) {
      *pvHolder = reinterpret_cast <void *> (bots.getBot (survive));
   }
   else {
      *pvHolder = reinterpret_cast <void *> (survive);
   }
   return true;
}

void BotUtils::attachSoundsToClients (edict_t *ent, const char *sample, float volume) {
   // this function called by the sound hooking code (in emit_sound) enters the played sound into
   // the array associated with the entity

   if (game.isNullEntity (ent) || isEmptyStr (sample)) {
      return;
   }
   const Vector &origin = game.getAbsPos (ent);

   if (origin.empty ()) {
      return;
   }
   int index = game.indexOfEntity (ent) - 1;

   if (index < 0 || index >= game.maxClients ()) {
      float nearestDistance = 99999.0f;

      // loop through all players
      for (int i = 0; i < game.maxClients (); i++) {
         const Client &client = m_clients[i];

         if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE)) {
            continue;
         }
         float distance = (client.origin - origin).length ();

         // now find nearest player
         if (distance < nearestDistance) {
            index = i;
            nearestDistance = distance;
         }
      }
   }

   // in case of worst case
   if (index < 0 || index >= game.maxClients ()) {
      return;
   }
   Client &client = m_clients[index];

   if (strncmp ("player/bhit_flesh", sample, 17) == 0 || strncmp ("player/headshot", sample, 15) == 0) {
      // hit/fall sound?
      client.hearingDistance = 768.0f * volume;
      client.timeSoundLasting = game.timebase () + 0.5f;
      client.sound = origin;
   }
   else if (strncmp ("items/gunpickup", sample, 15) == 0) {
      // weapon pickup?
      client.hearingDistance = 768.0f * volume;
      client.timeSoundLasting = game.timebase () + 0.5f;
      client.sound = origin;
   }
   else if (strncmp ("weapons/zoom", sample, 12) == 0) {
      // sniper zooming?
      client.hearingDistance = 512.0f * volume;
      client.timeSoundLasting = game.timebase () + 0.1f;
      client.sound = origin;
   }
   else if (strncmp ("items/9mmclip", sample, 13) == 0) {
      // ammo pickup?
      client.hearingDistance = 512.0f * volume;
      client.timeSoundLasting = game.timebase () + 0.1f;
      client.sound = origin;
   }
   else if (strncmp ("hostage/hos", sample, 11) == 0) {
      // CT used hostage?
      client.hearingDistance = 1024.0f * volume;
      client.timeSoundLasting = game.timebase () + 5.0f;
      client.sound = origin;
   }
   else if (strncmp ("debris/bustmetal", sample, 16) == 0 || strncmp ("debris/bustglass", sample, 16) == 0) {
      // broke something?
      client.hearingDistance = 1024.0f * volume;
      client.timeSoundLasting = game.timebase () + 2.0f;
      client.sound = origin;
   }
   else if (strncmp ("doors/doormove", sample, 14) == 0) {
      // someone opened a door
      client.hearingDistance = 1024.0f * volume;
      client.timeSoundLasting = game.timebase () + 3.0f;
      client.sound = origin;
   }
}

void BotUtils::simulateSoundUpdates (int playerIndex) {
   // this function tries to simulate playing of sounds to let the bots hear sounds which aren't
   // captured through server sound hooking

   if (playerIndex < 0 || playerIndex >= game.maxClients ()) {
      return; // reliability check
   }
   Client &client = m_clients[playerIndex];

   float hearDistance = 0.0f;
   float timeSound = 0.0f;

   if (client.ent->v.oldbuttons & IN_ATTACK) // pressed attack button?
   {
      hearDistance = 2048.0f;
      timeSound = game.timebase () + 0.3f;
   }
   else if (client.ent->v.oldbuttons & IN_USE) // pressed used button?
   {
      hearDistance = 512.0f;
      timeSound = game.timebase () + 0.5f;
   }
   else if (client.ent->v.oldbuttons & IN_RELOAD) // pressed reload button?
   {
      hearDistance = 512.0f;
      timeSound = game.timebase () + 0.5f;
   }
   else if (client.ent->v.movetype == MOVETYPE_FLY) // uses ladder?
   {
      if (cr::abs (client.ent->v.velocity.z) > 50.0f) {
         hearDistance = 1024.0f;
         timeSound = game.timebase () + 0.3f;
      }
   }
   else {
      extern ConVar mp_footsteps;

      if (mp_footsteps.boolean ()) {
         // moves fast enough?
         hearDistance = 1280.0f * (client.ent->v.velocity.length2D () / 260.0f);
         timeSound = game.timebase () + 0.3f;
      }
   }

   if (hearDistance <= 0.0) {
      return; // didn't issue sound?
   }

   // some sound already associated
   if (client.timeSoundLasting > game.timebase ()) {
      if (client.hearingDistance <= hearDistance) {
         // override it with new
         client.hearingDistance = hearDistance;
         client.timeSoundLasting = timeSound;
         client.sound = client.ent->v.origin;
      }
   }
   else {
      // just remember it
      client.hearingDistance = hearDistance;
      client.timeSoundLasting = timeSound;
      client.sound = client.ent->v.origin;
   }
}

void BotUtils::updateClients (void) {
   // record some stats of all players on the server
   for (int i = 0; i < game.maxClients (); i++) {
      edict_t *player = game.entityOfIndex (i + 1);
      Client &client = m_clients[i];

      if (!game.isNullEntity (player) && (player->v.flags & FL_CLIENT)) {
         client.ent = player;
         client.flags |= CF_USED;

         if (util.isAlive (player)) {
            client.flags |= CF_ALIVE;
         }
         else {
            client.flags &= ~CF_ALIVE;
         }

         if (client.flags & CF_ALIVE) {
            // keep the clipping mode enabled, or it can be turned off after new round has started
            if (game.getLocalEntity () == player && waypoints.hasEditFlag (WS_EDIT_NOCLIP)) {
               game.getLocalEntity ()->v.movetype = MOVETYPE_NOCLIP;
            }
            client.origin = player->v.origin;
            simulateSoundUpdates (i);
         }
      }
      else {
         client.flags &= ~(CF_USED | CF_ALIVE);
         client.ent = nullptr;
      }
   }
}

int BotUtils::buildNumber (void) {
   // this function generates build number from the compiler date macros

   static int buildNumber = 0;

   if (buildNumber != 0) {
      return buildNumber;
   }
   // get compiling date using compiler macros
   const char *date = __DATE__;

   // array of the month names
   const char *months[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

   // array of the month days
   uint8 monthDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

   int day = 0; // day of the year
   int year = 0; // year
   int i = 0;

   // go through all months, and calculate, days since year start
   for (i = 0; i < 11; i++) {
      if (strncmp (&date[0], months[i], 3) == 0) {
         break; // found current month break
      }
      day += monthDays[i]; // add month days
   }
   day += atoi (&date[4]) - 1; // finally calculate day
   year = atoi (&date[7]) - 2000; // get years since year 2000

   buildNumber = day + static_cast <int> ((year - 1) * 365.25);

   // if the year is a leap year?
   if ((year % 4) == 0 && i > 1) {
      buildNumber += 1; // add one year more
   }
   buildNumber -= 1114;

   return buildNumber;
}

int BotUtils::getWeaponAlias (bool needString, const char *weaponAlias, int weaponIndex) {
   // this function returning weapon id from the weapon alias and vice versa.

   // structure definition for weapon tab
   struct WeaponTab_t {
      Weapon weaponIndex; // weapon id
      const char *alias; // weapon alias
   };

   // weapon enumeration
   WeaponTab_t weaponTab[] = {
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
      {WEAPON_KNIFE, "knife"} // Knife
   };

   // if we need to return the string, find by weapon id
   if (needString && weaponIndex != -1) {
      for (size_t i = 0; i < cr::arrsize (weaponTab); i++) {
         if (weaponTab[i].weaponIndex == weaponIndex) { // is weapon id found?
            return MAKE_STRING (weaponTab[i].alias);
         }
      }
      return MAKE_STRING ("(none)"); // return none
   }

   // else search weapon by name and return weapon id
   for (size_t i = 0; i < cr::arrsize (weaponTab); i++) {
      if (strncmp (weaponTab[i].alias, weaponAlias, strlen (weaponTab[i].alias)) == 0) {
         return weaponTab[i].weaponIndex;
      }
   }
   return -1; // no weapon was found return -1
}