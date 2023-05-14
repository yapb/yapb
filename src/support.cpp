//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_display_welcome_text ("yb_display_welcome_text", "1", "Enables or disables showing welcome message to host entity on game start.");
ConVar cv_enable_query_hook ("yb_enable_query_hook", "0", "Enables or disables fake server queries response, that shows bots as real players in server browser.");

BotSupport::BotSupport () {
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

   // register weapon aliases
   m_weaponAlias[Weapon::USP] = "usp"; // HK USP .45 Tactical
   m_weaponAlias[Weapon::Glock18] = "glock"; // Glock18 Select Fire
   m_weaponAlias[Weapon::Deagle] = "deagle"; // Desert Eagle .50AE
   m_weaponAlias[Weapon::P228] = "p228"; // SIG P228
   m_weaponAlias[Weapon::Elite] = "elite"; // Dual Beretta 96G Elite
   m_weaponAlias[Weapon::FiveSeven] = "fn57"; // FN Five-Seven
   m_weaponAlias[Weapon::M3] = "m3"; // Benelli M3 Super90
   m_weaponAlias[Weapon::XM1014] = "xm1014"; // Benelli XM1014
   m_weaponAlias[Weapon::MP5] = "mp5"; // HK MP5-Navy
   m_weaponAlias[Weapon::TMP] = "tmp"; // Steyr Tactical Machine Pistol
   m_weaponAlias[Weapon::P90] = "p90"; // FN P90
   m_weaponAlias[Weapon::MAC10] = "mac10"; // Ingram MAC-10
   m_weaponAlias[Weapon::UMP45] = "ump45"; // HK UMP45
   m_weaponAlias[Weapon::AK47] = "ak47"; // Automat Kalashnikov AK-47
   m_weaponAlias[Weapon::Galil] = "galil"; // IMI Galil
   m_weaponAlias[Weapon::Famas] = "famas"; // GIAT FAMAS
   m_weaponAlias[Weapon::SG552] = "sg552"; // Sig SG-552 Commando
   m_weaponAlias[Weapon::M4A1] = "m4a1"; // Colt M4A1 Carbine
   m_weaponAlias[Weapon::AUG] = "aug"; // Steyr Aug
   m_weaponAlias[Weapon::Scout] = "scout"; // Steyr Scout
   m_weaponAlias[Weapon::AWP] = "awp"; // AI Arctic Warfare/Magnum
   m_weaponAlias[Weapon::G3SG1] = "g3sg1"; // HK G3/SG-1 Sniper Rifle
   m_weaponAlias[Weapon::SG550] = "sg550"; // Sig SG-550 Sniper
   m_weaponAlias[Weapon::M249] = "m249"; // FN M249 Para
   m_weaponAlias[Weapon::Flashbang] = "flash"; // Concussion Grenade
   m_weaponAlias[Weapon::Explosive] = "hegren"; // High-Explosive Grenade
   m_weaponAlias[Weapon::Smoke] = "sgren"; // Smoke Grenade
   m_weaponAlias[Weapon::Armor] = "vest"; // Kevlar Vest
   m_weaponAlias[Weapon::ArmorHelm] = "vesthelm"; // Kevlar Vest and Helmet
   m_weaponAlias[Weapon::Defuser] = "defuser"; // Defuser Kit
   m_weaponAlias[Weapon::Shield] = "shield"; // Tactical Shield
   m_weaponAlias[Weapon::Knife] = "knife"; // Knife

   m_clients.resize (kGameMaxPlayers + 1);
}

bool BotSupport::isAlive (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   return ent->v.deadflag == DEAD_NO && ent->v.health > 0 && ent->v.movetype != MOVETYPE_NOCLIP;
}

bool BotSupport::isVisible (const Vector &origin, edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   TraceResult tr {};
   game.testLine (ent->v.origin + ent->v.view_ofs, origin, TraceIgnore::Everything, ent, &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      return false;
   }
   return true;
}

void BotSupport::traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex) {
   // this function draw spraypaint depending on the tracing results.

   auto logo = conf.getRandomLogoName (logotypeIndex);

   int entityIndex = -1, message = TE_DECAL;
   int decalIndex = engfuncs.pfnDecalIndex (logo.chars ());

   if (decalIndex < 0) {
      decalIndex = engfuncs.pfnDecalIndex ("{lambda06");
   }

   if (cr::fequal (trace->flFraction, 1.0f)) {
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

bool BotSupport::isPlayer (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }

   if (ent->v.flags & FL_PROXY) {
      return false;
   }

   if ((ent->v.flags & (FL_CLIENT | FL_FAKECLIENT)) || bots[ent] != nullptr) {
      return !strings.isEmpty (ent->v.netname.chars ());
   }
   return false;
}

bool BotSupport::isMonster (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }

   if (~ent->v.flags & FL_MONSTER) {
      return false;
   }

   if (strncmp ("hostage", ent->v.classname.chars (), 7) == 0) {
      return false;
   }

   return true;
}

bool BotSupport::isItem (edict_t *ent) {
   return !!(strstr (ent->v.classname.chars (), "item_"));
}

bool BotSupport::isPlayerVIP (edict_t *ent) {
   if (!game.mapIs (MapFlags::Assassination)) {
      return false;
   }

   if (!isPlayer (ent)) {
      return false;
   }
   return *(engfuncs.pfnInfoKeyValue (engfuncs.pfnGetInfoKeyBuffer (ent), "model")) == 'v';
}

bool BotSupport::isFakeClient (edict_t *ent) {
   if (bots[ent] != nullptr || (!game.isNullEntity (ent) && (ent->v.flags & FL_FAKECLIENT))) {
      return true;
   }
   return false;
}

void BotSupport::checkWelcome () {
   // the purpose of this function, is  to send quick welcome message, to the listenserver entity.

   if (game.isDedicated () || !cv_display_welcome_text.bool_ () || !m_needToSendWelcome) {
      return;
   }
   m_welcomeReceiveTime = 0.0f;


   bool needToSendMsg = (graph.length () > 0 ? m_needToSendWelcome : true);
   auto receiveEntity = game.getLocalEntity ();

   if (isAlive (receiveEntity) && m_welcomeReceiveTime < 1.0f && needToSendMsg) {
      m_welcomeReceiveTime = game.time () + 4.0f; // receive welcome message in four seconds after game has commencing
   }

   if (m_welcomeReceiveTime > 0.0f && needToSendMsg) {
      if (!game.is (GameFlags::Mobility | GameFlags::Xash3D)) {
         game.serverCommand ("speak \"%s\"", m_sentences.random ());
      }
      String authorStr = "Official Navigation Graph";

      StringRef graphAuthor = graph.getAuthor ();
      StringRef graphModified = graph.getModifiedBy ();

      if (!graphAuthor.startsWith (product.name)) {
         authorStr.assignf ("Navigation Graph by: %s", graphAuthor);

         if (!graphModified.empty ()) {
            authorStr.appendf (" (Modified by: %s)", graphModified);
         }
      }

      MessageWriter (MSG_ONE, msgs.id (NetMsg::TextMsg), nullptr, receiveEntity)
         .writeByte (HUD_PRINTTALK)
         .writeString (strings.format ("----- %s v%s {%s}, (c) %s, by %s (%s)-----", product.name, product.version, product.date, product.year, product.author, product.url));

      MessageWriter (MSG_ONE, SVC_TEMPENTITY, nullptr, receiveEntity)
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (1)
         .writeShort (MessageWriter::fs16 (-1.0f, 13.0f))
         .writeShort (MessageWriter::fs16 (-1.0f, 13.0f))
         .writeByte (2)
         .writeByte (rg.get (33, 255))
         .writeByte (rg.get (33, 255))
         .writeByte (rg.get (33, 255))
         .writeByte (0)
         .writeByte (rg.get (230, 255))
         .writeByte (rg.get (230, 255))
         .writeByte (rg.get (230, 255))
         .writeByte (200)
         .writeShort (MessageWriter::fu16 (0.0078125f, 8.0f))
         .writeShort (MessageWriter::fu16 (2.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (6.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (0.1f, 8.0f))
         .writeString (strings.format ("\nHello! You are playing with %s v%s\nDevised by %s\n\n%s", product.name, product.version, product.author, authorStr));

      m_welcomeReceiveTime = 0.0f;
      m_needToSendWelcome = false;
   }
}

bool BotSupport::findNearestPlayer (void **pvHolder, edict_t *to, float searchDistance, bool sameTeam, bool needBot, bool needAlive, bool needDrawn, bool needBotWithC4) {
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

      if ((sameTeam && client.team != toTeam) || (needAlive && !(client.flags & ClientFlags::Alive)) || (needBot && !bots[client.ent]) || (needDrawn && (client.ent->v.effects & EF_NODRAW)) || (needBotWithC4 && (client.ent->v.weapons & Weapon::C4))) {
         continue; // filter players with parameters
      }
      float distance = client.ent->v.origin.distance (to->v.origin);

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

void BotSupport::updateClients () {

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
            sounds.simulateNoise (i);
         }
      }
      else {
         client.flags &= ~(ClientFlags::Used | ClientFlags::Alive);
         client.ent = nullptr;
      }
   }
}

int BotSupport::getPingBitmask (edict_t *ent, int loss, int ping) {
   // this function generats bitmask for SVC_PINGS engine message. See SV_EmitPings from engine for details

   const auto emit = [] (int s0, int s1, int s2) {
      return (s0 & (cr::bit (s1) - 1)) << s2;
   };
   return emit (loss, 7, 18) | emit (ping, 12, 6) | emit (game.indexOfPlayer (ent), 5, 1) | 1;
}

void BotSupport::calculatePings () {
   worker.enqueue ([this] () {
      syncCalculatePings ();
   });
}

void BotSupport::syncCalculatePings () {
   if (!game.is (GameFlags::HasFakePings) || cv_show_latency.int_ () != 2) {
      return;
   }
   MutexScopedLock lock (m_cs);

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
      client.ping = getPingBitmask (client.ent, loss, ping > 0 ? ping : rg.get (8, 16)); // getting player ping sometimes fails
      ++numHumans;

      average.first += ping;
      average.second += loss;
   }

   if (numHumans > 0) {
      average.first /= numHumans;
      average.second /= numHumans;
   }
   else {
      average.first = rg.get (30, 40);
      average.second = rg.get (5, 10);
   }

   // now calculate bot ping based on average from players
   for (auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used)) {
         continue;
      }
      auto bot = bots[client.ent];

      // we're only interested in bots here
      if (!bot) {
         continue;
      }
      int part = static_cast <int> (static_cast <float> (average.first) * 0.2f);

      int botPing = bot->m_basePing + rg.get (average.first - part, average.first + part) + rg.get (bot->m_difficulty / 2, bot->m_difficulty);
      int botLoss = rg.get (average.second / 2, average.second);

      if (botPing <= 5) {
         botPing = rg.get (10, 23);
      }
      else if (botPing > 70) {
         botPing = rg.get (30, 40);
      }
      client.ping = getPingBitmask (client.ent, botLoss, botPing);
   }
}

void BotSupport::emitPings (edict_t *to) {
   MessageWriter msg;

   auto isThirdpartyBot = [] (edict_t *ent) {
      return !bots[ent] && (ent->v.flags & FL_FAKECLIENT);
   };

   for (auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used) || client.ent == game.getLocalEntity () || isThirdpartyBot (client.ent)) {
         continue;
      }

      // no ping, no fun
      if (!client.ping) {
         client.ping = getPingBitmask (client.ent, rg.get (5, 10), rg.get (15, 40));
      }

      msg.start (MSG_ONE_UNRELIABLE, SVC_PINGS, nullptr, to)
         .writeLong (client.ping)
         .end ();
   }
   return;
}

void BotSupport::installSendTo () {
   // if previously requested to disable?
   if (!cv_enable_query_hook.bool_ ()) {
      if (m_sendToDetour.detoured ()) {
         disableSendTo ();
      }
      return;
   }

   // do not enable on not dedicated server
   if (!game.isDedicated ()) {
      return;
   }

   // enable only on modern games
   if (game.is (GameFlags::Modern) && (plat.nix || plat.win) && !plat.arm && !m_sendToDetour.detoured ()) {
      m_sendToDetour.install (reinterpret_cast <void *> (BotSupport::sendTo), true);
   }
}

bool BotSupport::isObjectInsidePlane (FrustumPlane &plane, const Vector &center, float height, float radius) {
   auto isPointInsidePlane = [&] (const Vector &point) -> bool {
      return plane.result + (plane.normal | point) >= 0.0f;
   };

   const Vector &test = plane.normal.get2d ();
   const Vector &top = center + Vector (0.0f, 0.0f, height * 0.5f) + test * radius;
   const Vector &bottom = center - Vector (0.0f, 0.0f, height * 0.5f) + test * radius;

   return isPointInsidePlane (top) || isPointInsidePlane (bottom);
}

bool BotSupport::isModel (const edict_t *ent, StringRef model) {
   return model.startsWith (ent->v.model.chars (9));
}

String BotSupport::getCurrentDateTime () {
   time_t ticks = time (&ticks);
   tm timeinfo {};

   plat.loctime (&timeinfo, &ticks);

   auto timebuf = strings.chars ();
   strftime (timebuf, StringBuffer::StaticBufferSize, "%d-%m-%Y %H:%M:%S", &timeinfo);

   return String (timebuf);
}

int32_t BotSupport::sendTo (int socket, const void *message, size_t length, int flags, const sockaddr *dest, int destLength) {
   const auto send = [&] (const Twin <const uint8_t *, size_t> &msg) -> int32_t {
      return Socket::sendto (socket, msg.first, msg.second, flags, dest, destLength);
   };

   auto packet = reinterpret_cast <const uint8_t *> (message);

   // player replies response
   if (length > 5 && packet[0] == 0xff && packet[1] == 0xff && packet[2] == 0xff && packet[3] == 0xff) {

      if (packet[4] == 'D') {
         QueryBuffer buffer (packet, length, 5);
         auto count = buffer.read <uint8_t> ();

         for (uint8_t i = 0; i < count; ++i) {
            buffer.skip <uint8_t> (); // number
            auto name = buffer.readString (); // name
            buffer.skip <int32_t> (); // score

            auto ctime = buffer.read <float> (); // override connection time
            buffer.write <float> (bots.getConnectTime (name, ctime));
         }
         return send (buffer.data ());
      }
      else if (packet[4] == 'I') {
         QueryBuffer buffer (packet, length, 5);
         buffer.skip <uint8_t> (); // protocol

         // skip server name, folder, map game
         for (size_t i = 0; i < 4; ++i) {
            buffer.skipString ();
         }
         buffer.skip <short> (); // steam app id
         buffer.skip <uint8_t> (); // players
         buffer.skip <uint8_t> (); // maxplayers
         buffer.skip <uint8_t> (); // bots
         buffer.write <uint8_t> (0); // zero out bot count

         return send (buffer.data ());
      }
      else if (packet[4] == 'm') {
         QueryBuffer buffer (packet, length, 5);

         buffer.shiftToEnd (); // shift to the end of buffer
         buffer.write <uint8_t> (0); // zero out bot count

         return send (buffer.data ());
      }
   }
   return send ({ packet, length });
}

StringRef BotSupport::weaponIdToAlias (int32_t id) {
   StringRef none = "none";

   if (m_weaponAlias.exists (id)) {
      return m_weaponAlias[id];
   }
   return none;
}
