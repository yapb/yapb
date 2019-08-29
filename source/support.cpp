// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_display_welcome_text ("yb_display_welcome_text", "1", "Enables or disables showing welcome message to host entity on game start.");
ConVar yb_enable_query_hook ("yb_enable_query_hook", "1", "Enables or disabled fake server queries response, that shows bots as real players in server browser.");

BotUtils::BotUtils () {
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

   m_tags.emplace ("[[", "]]");
   m_tags.emplace ("-=", "=-");
   m_tags.emplace ("-[", "]-");
   m_tags.emplace ("-]", "[-");
   m_tags.emplace ("-}", "{-");
   m_tags.emplace ("-{", "}-");
   m_tags.emplace ("<[", "]>");
   m_tags.emplace ("<]", "[>");
   m_tags.emplace ("[-", "-]");
   m_tags.emplace ("]-", "-[");
   m_tags.emplace ("{-", "-}");
   m_tags.emplace ("}-", "-{");
   m_tags.emplace ("[", "]");
   m_tags.emplace ("{", "}");
   m_tags.emplace ("<", "[");
   m_tags.emplace (">", "<");
   m_tags.emplace ("-", "-");
   m_tags.emplace ("|", "|");
   m_tags.emplace ("=", "=");
   m_tags.emplace ("+", "+");
   m_tags.emplace ("(", ")");
   m_tags.emplace (")", "(");

   // register noise cache
   m_noiseCache["player/bhit"] = Noise::NeedHandle | Noise::HitFall;
   m_noiseCache["player/head"] = Noise::NeedHandle | Noise::HitFall;
   m_noiseCache["items/gunpi"] = Noise::NeedHandle | Noise::Pickup;
   m_noiseCache["items/9mmcl"] = Noise::NeedHandle | Noise::Ammo;
   m_noiseCache["weapons/zoo"] = Noise::NeedHandle | Noise::Zoom;
   m_noiseCache["hostage/hos"] = Noise::NeedHandle | Noise::Hostage;
   m_noiseCache["debris/bust"] = Noise::NeedHandle | Noise::Broke;
   m_noiseCache["debris/bust"] = Noise::NeedHandle | Noise::Broke;
   m_noiseCache["doors/doorm"] = Noise::NeedHandle | Noise::Door;

   m_clients.resize (kGameMaxPlayers + 1);
}

bool BotUtils::isAlive (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   return ent->v.deadflag == DEAD_NO && ent->v.health > 0 && ent->v.movetype != MOVETYPE_NOCLIP;
}

bool BotUtils::isVisible (const Vector &origin, edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   TraceResult tr;
   game.testLine (ent->v.origin + ent->v.view_ofs, origin, TraceIgnore::Everything, ent, &tr);

   if (tr.flFraction != 1.0f) {
      return false;
   }
   return true;
}

void BotUtils::traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex) {
   // this function draw spraypaint depending on the tracing results.

   auto logo = conf.getRandomLogoName (logotypeIndex);

   int entityIndex = -1, message = TE_DECAL;
   int decalIndex = engfuncs.pfnDecalIndex (logo.chars ());

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

   if (logo.startsWith ("{")) {
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

   if ((ent->v.flags & (FL_CLIENT | FL_FAKECLIENT)) || bots[ent] != nullptr) {
      return !strings.isEmpty (STRING (ent->v.netname));
   }
   return false;
}

bool BotUtils::isPlayerVIP (edict_t *ent) {
   if (!game.mapIs (MapFlags::Assassination)) {
      return false;
   }

   if (!isPlayer (ent)) {
      return false;
   }
   return *(engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), "model")) == 'v';
}

bool BotUtils::isFakeClient (edict_t *ent) {
   if (bots[ent] != nullptr || (!game.isNullEntity (ent) && (ent->v.flags & FL_FAKECLIENT))) {
      return true;
   }
   return false;
}

bool BotUtils::openConfig (const char *fileName, const char *errorIfNotExists, MemFile *outFile, bool languageDependant /*= false*/) {
   if (*outFile) {
      outFile->close ();
   }

   // save config dir
   const char *configDir = "addons/yapb/conf";

   if (languageDependant) {
      extern ConVar yb_language;

      if (strcmp (fileName, "lang.cfg") == 0 && strcmp (yb_language.str (), "en") == 0) {
         return false;
      }
      auto langConfig = strings.format ("%s/lang/%s_%s", configDir, yb_language.str (), fileName);

      // check is file is exists for this language
      if (!outFile->open (langConfig)) {
         outFile->open (strings.format ("%s/lang/en_%s", configDir, fileName));
      }
   }
   else {
      outFile->open (strings.format ("%s/%s", configDir, fileName));
   }

   if (!*outFile) {
      logger.error (errorIfNotExists);
      return false;
   }
   return true;
}

void BotUtils::checkWelcome () {
   // the purpose of this function, is  to send quick welcome message, to the listenserver entity.

   if (game.isDedicated () || !yb_display_welcome_text.bool_ () || !m_needToSendWelcome) {
      return;
   }
   m_welcomeReceiveTime = 0.0f;


   bool needToSendMsg = (graph.length () > 0 ? m_needToSendWelcome : true);
   auto receiveEntity = game.getLocalEntity ();

   if (isAlive (receiveEntity) && m_welcomeReceiveTime < 1.0 && needToSendMsg) {
      m_welcomeReceiveTime = game.time () + 4.0f; // receive welcome message in four seconds after game has commencing
   }


   if (m_welcomeReceiveTime > 0.0f && needToSendMsg) {
      if (!game.is (GameFlags::Mobility | GameFlags::Xash3D)) {
         game.serverCommand ("speak \"%s\"", m_sentences.random ().chars ());
      }

      MessageWriter (MSG_ONE, msgs.id (NetMsg::TextMsg), nullptr, receiveEntity)
         .writeByte (HUD_PRINTTALK)
         .writeString (strings.format ("----- %s v%s (Build: %u), {%s}, (c) %s, by %s (%s)-----", PRODUCT_SHORT_NAME, PRODUCT_VERSION, buildNumber (), PRODUCT_DATE, PRODUCT_END_YEAR, PRODUCT_AUTHOR, PRODUCT_URL));

      MessageWriter (MSG_ONE, SVC_TEMPENTITY, nullptr, receiveEntity)
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (1)
         .writeShort (MessageWriter::fs16 (-1.0f, 13.0f))
         .writeShort (MessageWriter::fs16 (-1.0f, 13.0f))
         .writeByte (2)
         .writeByte (rg.int_ (33, 255))
         .writeByte (rg.int_ (33, 255))
         .writeByte (rg.int_ (33, 255))
         .writeByte (0)
         .writeByte (rg.int_ (230, 255))
         .writeByte (rg.int_ (230, 255))
         .writeByte (rg.int_ (230, 255))
         .writeByte (200)
         .writeShort (MessageWriter::fu16 (0.0078125f, 8.0f))
         .writeShort (MessageWriter::fu16 (2.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (6.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (0.1f, 8.0f))
         .writeString (strings.format ("\nServer is running %s v%s (Build: %u)\nDeveloped by %s\n\n%s", PRODUCT_SHORT_NAME, PRODUCT_VERSION, buildNumber (), PRODUCT_AUTHOR, graph.getAuthor ()));

      m_welcomeReceiveTime = 0.0f;
      m_needToSendWelcome = false;
   }
}

bool BotUtils::findNearestPlayer (void **pvHolder, edict_t *to, float searchDistance, bool sameTeam, bool needBot, bool needAlive, bool needDrawn, bool needBotWithC4) {
   // this function finds nearest to to, player with set of parameters, like his
   // team, live status, search distance etc. if needBot is true, then pvHolder, will
   // be filled with bot pointer, else with edict pointer(!).

   edict_t *survive = nullptr; // pointer to temporally & survive entity
   float nearestPlayer = 4096.0f; // nearest player

   int toTeam = game.getTeam (to);

   for (const auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used) || client.ent == to) {
         continue;
      }

      if ((sameTeam && client.team != toTeam) || (needAlive && !(client.flags & ClientFlags::Alive)) || (needBot && !isFakeClient (client.ent)) || (needDrawn && (client.ent->v.effects & EF_NODRAW)) || (needBotWithC4 && (client.ent->v.weapons & Weapon::C4))) {
         continue; // filter players with parameters
      }
      float distance = (client.ent->v.origin - to->v.origin).length ();

      if (distance < nearestPlayer && distance < searchDistance) {
         nearestPlayer = distance;
         survive = client.ent;
      }
   }

   if (game.isNullEntity (survive)) {
      return false; // nothing found
   }

   // fill the holder
   if (needBot) {
      *pvHolder = reinterpret_cast <void *> (bots[survive]);
   }
   else {
      *pvHolder = reinterpret_cast <void *> (survive);
   }
   return true;
}

void BotUtils::listenNoise (edict_t *ent, const String &sample, float volume) {
   // this function called by the sound hooking code (in emit_sound) enters the played sound into  the array associated with the entity

   if (game.isNullEntity (ent) || sample.empty ()) {
      return;
   }
   const Vector &origin = game.getEntityWorldOrigin (ent);

   // something wrong with sound...
   if (origin.empty ()) {
      return;
   }
   auto noise = m_noiseCache[sample.substr (0, 11)];

   // we're not handling theese
   if (!(noise & Noise::NeedHandle)) {
      return;
   }

   // find nearest player to sound origin
   auto findNearbyClient = [&origin] () {
      float nearest = kInfiniteDistance;
      Client *result = nullptr;

      // loop through all players
      for (auto &client : util.getClients ()) {
         if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive)) {
            continue;
         }
         auto distance = (client.origin - origin).lengthSq ();

         // now find nearest player
         if (distance < nearest) {
            result = &client;
            nearest = distance;
         }
      }
      return result;
   };
   auto client = findNearbyClient ();

   // update noise stats
   auto registerNoise = [&origin, &client, &volume] (float distance, float lasting) {
      client->noise.dist = distance * volume;
      client->noise.last = game.time () + lasting;
      client->noise.pos = origin;
   };

   // client wasn't found
   if (!client) {
      return;
   }

   // hit/fall sound?
   if (noise & Noise::HitFall) {
      registerNoise (768.0f, 0.52f);
   }

   // weapon pickup?
   else if (noise & Noise::Pickup) {
      registerNoise (768.0f, 0.45f);
   }

   // sniper zooming?
   else if (noise & Noise::Zoom) {
      registerNoise (512.0f, 0.10f);
   }

   // ammo pickup?
   else if (noise & Noise::Ammo) {
      registerNoise (512.0f, 0.25f);
   }

   // ct used hostage?
   else if (noise & Noise::Hostage) {
      registerNoise (1024.0f, 5.00);
   }

   // broke something?
   else if (noise & Noise::Broke) {
      registerNoise (1024.0f, 2.00f);
   }

   // someone opened a door
   else if (noise & Noise::Door) {
      registerNoise (1024.0f, 3.00f);
   }
}

void BotUtils::simulateNoise (int playerIndex) {
   // this function tries to simulate playing of sounds to let the bots hear sounds which aren't
   // captured through server sound hooking

   if (playerIndex < 0 || playerIndex >= game.maxClients ()) {
      return; // reliability check
   }
   Client &client = m_clients[playerIndex];
   ClientNoise noise {};

   auto buttons = client.ent->v.button | client.ent->v.oldbuttons;

   // pressed attack button?
   if (buttons & IN_ATTACK) {
      noise.dist = 2048.0f;
      noise.last = game.time () + 0.3f;
   }

   // pressed used button?
   else if (buttons & IN_USE) {
      noise.dist = 512.0f;
      noise.last = game.time () + 0.5f;
   }

   // pressed reload button?
   else if (buttons & IN_RELOAD)  {
      noise.dist = 512.0f;
      noise.last = game.time () + 0.5f;
   }

   // uses ladder?
   else if (client.ent->v.movetype == MOVETYPE_FLY) {
      if (cr::abs (client.ent->v.velocity.z) > 50.0f) {
         noise.dist = 1024.0f;
         noise.last = game.time () + 0.3f;
      }
   }
   else {
      extern ConVar mp_footsteps;

      if (mp_footsteps.bool_ ()) {
         // moves fast enough?
         noise.dist = 1280.0f * (client.ent->v.velocity.length2d () / 260.0f);
         noise.last = game.time () + 0.3f;
      }
   }

   if (noise.dist <= 0.0) {
      return; // didn't issue sound?
   }

   // some sound already associated
   if (client.noise.last > game.time ()) {
      if (client.noise.dist <= noise.dist) {
         // override it with new
         client.noise.dist = noise.dist;
         client.noise.last = noise.last;
         client.noise.pos = client.ent->v.origin;
      }
   }
   else if (!cr::fzero (noise.last)) {
      // just remember it
      client.noise.dist = noise.dist;
      client.noise.last = noise.last;
      client.noise.pos = client.ent->v.origin;
   }
}

void BotUtils::updateClients () {

   // record some stats of all players on the server
   for (int i = 0; i < game.maxClients (); ++i) {
      edict_t *player = game.playerOfIndex (i);
      Client &client = m_clients[i];

      if (!game.isNullEntity (player) && (player->v.flags & FL_CLIENT)) {
         client.ent = player;
         client.flags |= ClientFlags::Used;

         if (util.isAlive (player)) {
            client.flags |= ClientFlags::Alive;
         }
         else {
            client.flags &= ~ClientFlags::Alive;
         }

         if (client.flags & ClientFlags::Alive) {
            client.origin = player->v.origin;
            simulateNoise (i);
         }
      }
      else {
         client.flags &= ~(ClientFlags::Used | ClientFlags::Alive);
         client.ent = nullptr;
      }
   }
}

int BotUtils::getPingBitmask (edict_t *ent, int loss, int ping) {
   // this function generats bitmask for SVC_PINGS engine message. See SV_EmitPings from engine for details

   const auto emit = [] (int s0, int s1, int s2) {
      return (s0 & (cr::bit (s1) - 1)) << s2;
   };
   return emit (loss, 7, 18) | emit (ping, 12, 6) | emit (game.indexOfPlayer (ent), 5, 1) | 1;
}

void BotUtils::calculatePings () {
   if (!game.is (GameFlags::HasFakePings) || yb_show_latency.int_ () != 2) {
      return;
   }

   Twin <int, int> average { 0, 0 };
   int numHumans = 0;

   // first get average ping on server, and store real client pings
   for (auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used) || isFakeClient (client.ent)) {
         continue;
      }
      int ping, loss;
      engfuncs.pfnGetPlayerStats (client.ent, &ping, &loss);

      // store normal client ping
      client.ping = getPingBitmask (client.ent, loss, ping > 0 ? ping / 2 : rg.int_ (8, 16)); // getting player ping sometimes fails
      client.pingUpdate = true; // force resend ping

      ++numHumans;

      average.first += ping;
      average.second += loss;
   }

   if (numHumans > 0) {
      average.first /= numHumans;
      average.second /= numHumans;
   }
   else {
      average.first = rg.int_ (30, 40);
      average.second = rg.int_ (5, 10);
   }

   // now calculate bot ping based on average from players
   for (auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used)) {
         continue;
      }
      auto bot = bots[client.ent];

      // we're only intrested in bots here
      if (!bot) {
         continue;
      }
      int part = static_cast <int> (average.first * 0.2f);

      int botPing = bot->m_basePing + rg.int_ (average.first - part, average.first + part) + rg.int_ (bot->m_difficulty / 2, bot->m_difficulty);
      int botLoss = rg.int_ (average.second / 2, average.second);

      client.ping = getPingBitmask (client.ent, botLoss, botPing);
      client.pingUpdate = true; // force resend ping
   }
}

void BotUtils::sendPings (edict_t *to) {
   MessageWriter msg;

   // missing from sdk
   constexpr int kGamePingSVC = 17;
   
   for (auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used) || client.ent == game.getLocalEntity ()) {
         continue;
      }
      if (!client.pingUpdate) {
         continue;
      }
      client.pingUpdate = false;

      // no ping, no fun
      if (!client.ping) {
         client.ping = getPingBitmask (client.ent, rg.int_ (5, 10), rg.int_ (15, 40));
      }
      
      msg.start (MSG_ONE_UNRELIABLE, kGamePingSVC, nullptr, to)
         .writeLong (client.ping)
         .end ();
   }
   return;
}

void BotUtils::installSendTo () {
   // if previously requested to disable?
   if (!yb_enable_query_hook.bool_ ()) {
      if (m_sendToHook.enabled ()) {
         disableSendTo ();
      }
      return;
   }

   // do not enable on not dedicated server
   if (!game.isDedicated ()) {
      return;
   }

   // enable only on modern games
   if (game.is (GameFlags::Modern) && (plat.isLinux || plat.isWindows) && !plat.isArm && !m_sendToHook.enabled ()) {
      m_sendToHook.patch (reinterpret_cast <void *> (&sendto), reinterpret_cast <void *> (&BotUtils::sendTo));
   }
}

int32 BotUtils::sendTo (int socket, const void *message, size_t length, int flags, const sockaddr *dest, int destLength) {
   const auto send = [&] (const Twin <const uint8 *, size_t> &msg) -> int32 {
      return Socket::sendto (socket, msg.first, msg.second, flags, dest, destLength);
   };
   auto packet = reinterpret_cast <const uint8 *> (message);

   // player replies response
   if (length > 5 && packet[0] == 0xff && packet[1] == 0xff && packet[2] == 0xff && packet[3] == 0xff) {
      if (packet[4] == 'D') {
         QueryBuffer buffer (packet, length, 5);
         auto count = buffer.read <uint8> ();

         for (uint8 i = 0; i < count; ++i) {
            buffer.read <uint8> (); // number
            buffer.write <uint8> (i); // override number

            buffer.skipString (); // name
            buffer.skip <int32> (); // score

            buffer.read <float> (); // override connection time
            buffer.write <float> (bots.getConnectionTime (i));
         }
         return send (buffer.data ());
      }
      else if (packet[4] == 'I') {
         QueryBuffer buffer (packet, length, 5);
         buffer.skip <uint8> (); // protocol

         // skip server name, folder, map game
         for (size_t i = 0; i < 4; ++i) {
            buffer.skipString ();
         }
         buffer.skip <short> (); // steam app id

         buffer.skip <uint8> (); // players
         buffer.skip <uint8> (); // maxplayers
         buffer.skip <uint8> (); // bots
         buffer.write <uint8> (0); // zero out bot count

         return send (buffer.data ());
      }
      else if (packet[4] == 'm') {
         QueryBuffer buffer (packet, length, 5);

         buffer.shiftToEnd (); // shift to the end of buffer
         buffer.write <uint8> (0); // zero out bot count

         return send (buffer.data ());
      }
   }
   return send ({ packet, length });
}

int BotUtils::buildNumber () {
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
   for (i = 0; i < 11; ++i) {
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
      {Weapon::USP, "usp"}, // HK USP .45 Tactical
      {Weapon::Glock18, "glock"}, // Glock18 Select Fire
      {Weapon::Deagle, "deagle"}, // Desert Eagle .50AE
      {Weapon::P228, "p228"}, // SIG P228
      {Weapon::Elite, "elite"}, // Dual Beretta 96G Elite
      {Weapon::FiveSeven, "fn57"}, // FN Five-Seven
      {Weapon::M3, "m3"}, // Benelli M3 Super90
      {Weapon::XM1014, "xm1014"}, // Benelli XM1014
      {Weapon::MP5, "mp5"}, // HK MP5-Navy
      {Weapon::TMP, "tmp"}, // Steyr Tactical Machine Pistol
      {Weapon::P90, "p90"}, // FN P90
      {Weapon::MAC10, "mac10"}, // Ingram MAC-10
      {Weapon::UMP45, "ump45"}, // HK UMP45
      {Weapon::AK47, "ak47"}, // Automat Kalashnikov AK-47
      {Weapon::Galil, "galil"}, // IMI Galil
      {Weapon::Famas, "famas"}, // GIAT FAMAS
      {Weapon::SG552, "sg552"}, // Sig SG-552 Commando
      {Weapon::M4A1, "m4a1"}, // Colt M4A1 Carbine
      {Weapon::AUG, "aug"}, // Steyr Aug
      {Weapon::Scout, "scout"}, // Steyr Scout
      {Weapon::AWP, "awp"}, // AI Arctic Warfare/Magnum
      {Weapon::G3SG1, "g3sg1"}, // HK G3/SG-1 Sniper Rifle
      {Weapon::SG550, "sg550"}, // Sig SG-550 Sniper
      {Weapon::M249, "m249"}, // FN M249 Para
      {Weapon::Flashbang, "flash"}, // Concussion Grenade
      {Weapon::Explosive, "hegren"}, // High-Explosive Grenade
      {Weapon::Smoke, "sgren"}, // Smoke Grenade
      {Weapon::Armor, "vest"}, // Kevlar Vest
      {Weapon::ArmorHelm, "vesthelm"}, // Kevlar Vest and Helmet
      {Weapon::Defuser, "defuser"}, // Defuser Kit
      {Weapon::Shield, "shield"}, // Tactical Shield
      {Weapon::Knife, "knife"} // Knife
   };

   // if we need to return the string, find by weapon id
   if (needString && weaponIndex != -1) {
      for (auto &tab : weaponTab) {
         if (tab.weaponIndex == weaponIndex) { // is weapon id found?
            return MAKE_STRING (tab.alias);
         }
      }
      return MAKE_STRING ("(none)"); // return none
   }

   // else search weapon by name and return weapon id
   for (auto &tab : weaponTab) {
      if (strncmp (tab.alias, weaponAlias, strlen (tab.alias)) == 0) {
         return tab.weaponIndex;
      }
   }
   return -1; // no weapon was found return -1
}