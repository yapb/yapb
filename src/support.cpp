//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_display_welcome_text ("display_welcome_text", "1", "Enables or disables showing welcome message to host entity on game start.");
ConVar cv_enable_query_hook ("enable_query_hook", "0", "Enables or disables fake server queries response, that shows bots as real players in server browser.");
ConVar cv_breakable_health_limit ("breakable_health_limit", "500.0", "Specifies the maximum health of breakable object, that bot will consider to destroy.", true, 1.0f, 3000.0);
ConVar cv_enable_fake_steamids ("enable_fake_steamids", "0", "Allows or disallows bots to return fake steam id.");

BotSupport::BotSupport () {
   m_needToSendWelcome = false;
   m_welcomeReceiveTime = 0.0f;

   // add default messages
   m_sentences = {
      "hello user,communication is acquired",
      "your presence is acknowledged",
      "high man, your in command now",
      "blast your hostile for good",
      "high man, kill some idiot here",
      "is there a doctor in the area",
      "warning, experimental materials detected",
      "high amigo, shoot some but",
      "time for some bad ass explosion",
      "bad ass son of a breach device activated",
      "high, do not question this great service",
      "engine is operative, hello and goodbye",
      "high amigo, your administration has been great last day",
      "attention, expect experimental armed hostile presence",
      "warning, medical attention required",
      "high man, at your command",
      "check check, test, mike check, talk device is activated",
      "hello pal, at your service",
      "good, day mister, your administration is now acknowledged",
      "attention, anomalous agent activity, detected",
      "mister, you are going down",
      "all command access granted, over and out",
      "buzwarn hostile presence detected nearest to your sector. over and out. doop"
      "hostile resistance detected"
   };

   // register weapon aliases
   m_weaponAliases = {
      { Weapon::USP, "usp" }, // HK USP .45 Tactical
      { Weapon::Glock18, "glock" }, // Glock18 Select Fire
      { Weapon::Deagle, "deagle" }, // Desert Eagle .50AE
      { Weapon::P228, "p228" }, // SIG P228
      { Weapon::Elite, "elite" }, // Dual Beretta 96G Elite
      { Weapon::FiveSeven, "fn57" }, // FN Five-Seven
      { Weapon::M3, "m3" }, // Benelli M3 Super90
      { Weapon::XM1014, "xm1014" }, // Benelli XM1014
      { Weapon::MP5, "mp5" }, // HK MP5-Navy
      { Weapon::TMP, "tmp" }, // Steyr Tactical Machine Pistol
      { Weapon::P90, "p90" }, // FN P90
      { Weapon::MAC10, "mac10" }, // Ingram MAC-10
      { Weapon::UMP45, "ump45" }, // HK UMP45
      { Weapon::AK47, "ak47" }, // Automat Kalashnikov AK-47
      { Weapon::Galil, "galil" }, // IMI Galil
      { Weapon::Famas, "famas" }, // GIAT FAMAS
      { Weapon::SG552, "sg552" }, // Sig SG-552 Commando
      { Weapon::M4A1, "m4a1" }, // Colt M4A1 Carbine
      { Weapon::AUG, "aug" }, // Steyr Aug
      { Weapon::Scout, "scout" }, // Steyr Scout
      { Weapon::AWP, "awp" }, // AI Arctic Warfare/Magnum
      { Weapon::G3SG1, "g3sg1" }, // HK G3/SG-1 Sniper Rifle
      { Weapon::SG550, "sg550" }, // Sig SG-550 Sniper
      { Weapon::M249, "m249" }, // FN M249 Para
      { Weapon::Flashbang, "flash" }, // Concussion Grenade
      { Weapon::Explosive, "hegren" }, // High-Explosive Grenade
      { Weapon::Smoke, "sgren" }, // Smoke Grenade
      { Weapon::Armor, "vest" }, // Kevlar Vest
      { Weapon::ArmorHelm, "vesthelm" }, // Kevlar Vest and Helmet
      { Weapon::Defuser, "defuser" }, // Defuser Kit
      { Weapon::Shield, "shield" }, // Tactical Shield
      { Weapon::Knife, "knife" } // Knife
   };

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

void BotSupport::decalTrace (entvars_t *pev, TraceResult *trace, int logotypeIndex) {
   // this function draw spraypaint depending on the tracing results.

   auto logo = conf.getLogoName (logotypeIndex);

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
      MessageWriter msg {};

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

   if (isHostageEntity (ent)) {
      return false;
   }
   return true;
}

bool BotSupport::isItem (edict_t *ent) {
   return ent && ent->v.classname.str ().contains ("item_");
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

bool BotSupport::isDoorEntity (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   return ent->v.classname.str ().startsWith ("func_door");
}

bool BotSupport::isHostageEntity (edict_t *ent) {
   if (game.isNullEntity (ent)) {
      return false;
   }
   const auto classHash = ent->v.classname.str ().hash ();

   constexpr auto kHostageEntity = StringRef::fnv1a32 ("hostage_entity");
   constexpr auto kMonsterScientist = StringRef::fnv1a32 ("monster_scientist");

   return classHash == kHostageEntity || classHash == kMonsterScientist;
}

bool BotSupport::isShootableBreakable (edict_t *ent) {
   if (game.isNullEntity (ent) || ent == game.getStartEntity ()) {
      return false;
   }
   const auto limit = cv_breakable_health_limit.as <float> ();

   // not shootable
   if (ent->v.health >= limit) {
      return false;
   }
   constexpr auto kFuncBreakable = StringRef::fnv1a32 ("func_breakable");
   constexpr auto kFuncPushable = StringRef::fnv1a32 ("func_pushable");
   constexpr auto kFuncWall = StringRef::fnv1a32 ("func_wall");

   if (ent->v.takedamage > 0.0f && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY)) {
      auto classHash = ent->v.classname.str ().hash ();

      if (classHash == kFuncBreakable || (classHash == kFuncPushable && (ent->v.spawnflags & SF_PUSH_BREAKABLE)) || classHash == kFuncWall) {
         return ent->v.movetype == MOVETYPE_PUSH || ent->v.movetype == MOVETYPE_PUSHSTEP;
      }
   }
   return false;
}

bool BotSupport::isFakeClient (edict_t *ent) {
   if (bots[ent] != nullptr || (!game.isNullEntity (ent) && (ent->v.flags & FL_FAKECLIENT))) {
      return true;
   }
   return false;
}

void BotSupport::checkWelcome () {
   // the purpose of this function, is  to send quick welcome message, to the listenserver entity.

   if (game.isDedicated () || !cv_display_welcome_text || !m_needToSendWelcome) {
      return;
   }

   const bool needToSendMsg = (graph.length () > 0 ? m_needToSendWelcome : true);
   auto receiveEnt = game.getLocalEntity ();

   if (isAlive (receiveEnt) && m_welcomeReceiveTime < 1.0f && needToSendMsg) {
      m_welcomeReceiveTime = game.time () + 2.0f + mp_freezetime.as <float> (); // receive welcome message in four seconds after game has commencing
   }

   if (m_welcomeReceiveTime > 0.0f && m_welcomeReceiveTime < game.time () && needToSendMsg) {
      game.serverCommand ("speak \"%s\"", m_sentences.random ());
      String authorStr = "Official Navigation Graph";

      auto graphAuthor = graph.getAuthor ();
      auto graphModified = graph.getModifiedBy ();

      // legacy welcome message, to respect the original code
      constexpr StringRef kLegacyWelcomeMessage = "Welcome to POD-Bot V2.5 by Count Floyd\n"
         "Visit http://www.nuclearbox.com/podbot/ or\n"
         "      http://www.botepidemic.com/podbot for Updates\n";

      // it's should be send in very rare cases
      const bool sendLegacyWelcome = rg.chance (game.is (GameFlags::Legacy) ? 25 : 2);

      if (!graphAuthor.startsWith (product.name)) {
         authorStr.assignf ("Navigation Graph by: %s", graphAuthor);

         if (!graphModified.empty ()) {
            authorStr.appendf (" (Modified by: %s)", graphModified);
         }
      }
      StringRef modernWelcomeMessage = strings.format ("\nHello! You are playing with %s v%s\nDevised by %s\n\n%s", product.name, product.version, product.author, authorStr);
      StringRef modernChatWelcomeMessage = strings.format ("----- %s v%s {%s}, (c) %s, by %s (%s)-----", product.name, product.version, product.date, product.year, product.author, product.url);

      // send a chat-position message
      MessageWriter (MSG_ONE, msgs.id (NetMsg::TextMsg), nullptr, receiveEnt)
         .writeByte (HUD_PRINTTALK)
         .writeString (modernChatWelcomeMessage.chars ());

      static hudtextparms_t textParams {};

      textParams.channel = 1;
      textParams.x = -1.0f;
      textParams.y = sendLegacyWelcome ? 0.0f : -1.0f;
      textParams.effect = rg (1, 2);

      textParams.r1 = static_cast <uint8_t> (sendLegacyWelcome ? 255 : rg (33, 255));
      textParams.g1 = static_cast <uint8_t> (sendLegacyWelcome ? 0 : rg (33, 255));
      textParams.b1 = static_cast <uint8_t> (sendLegacyWelcome ? 0 : rg (33, 255));
      textParams.a1 = static_cast <uint8_t> (0);

      textParams.r2 = static_cast <uint8_t> (sendLegacyWelcome ? 255 : rg (230, 255));
      textParams.g2 = static_cast <uint8_t> (sendLegacyWelcome ? 255 : rg (230, 255));
      textParams.b2 = static_cast <uint8_t> (sendLegacyWelcome ? 255 : rg (230, 255));
      textParams.a2 = static_cast <uint8_t> (200);

      textParams.fadeinTime = 0.0078125f;
      textParams.fadeoutTime = 2.0f;
      textParams.holdTime = 6.0f;
      textParams.fxTime = 0.25f;

      // send the hud message
      game.sendHudMessage (receiveEnt, textParams,
         sendLegacyWelcome ? kLegacyWelcomeMessage.chars () : modernWelcomeMessage.chars ());

      m_welcomeReceiveTime = 0.0f;
      m_needToSendWelcome = false;
   }
}

bool BotSupport::findNearestPlayer (void **pvHolder, edict_t *to, float searchDistance, bool sameTeam, bool needBot, bool needAlive, bool needDrawn, bool needBotWithC4) {
   // this function finds nearest to to, player with set of parameters, like his
   // team, live status, search distance etc. if needBot is true, then pvHolder, will
   // be filled with bot pointer, else with edict pointer(!).

   searchDistance = cr::sqrf (searchDistance);

   edict_t *survive = nullptr; // pointer to temporally & survive entity
   float nearestPlayer = 4096.0f; // nearest player

   const int toTeam = game.getTeam (to);

   for (const auto &client : m_clients) {
      if (!(client.flags & ClientFlags::Used) || client.ent == to) {
         continue;
      }

      if ((sameTeam && client.team != toTeam) || (needAlive && !(client.flags & ClientFlags::Alive)) || (needBot && !bots[client.ent]) || (needDrawn && (client.ent->v.effects & EF_NODRAW)) || (needBotWithC4 && (client.ent->v.weapons & Weapon::C4))) {
         continue; // filter players with parameters
      }
      const float distanceSq = client.ent->v.origin.distanceSq (to->v.origin);

      if (distanceSq < nearestPlayer && distanceSq < searchDistance) {
         nearestPlayer = distanceSq;
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

      if (!game.isNullEntity (player) && (player->v.flags & FL_CLIENT) && !(player->v.flags & FL_DORMANT)) {
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

StringRef BotSupport::getFakeSteamId (edict_t *ent) {
   if (!cv_enable_fake_steamids || !isPlayer (ent)) {
      return "BOT";
   }
   auto botNameHash = StringRef::fnv1a32 (ent->v.netname.chars ());

   // just fake steam id a d return it with get player authid function
   return strings.format ("STEAM_0:1:%d", cr::abs (static_cast <int32_t> (botNameHash) & 0xffff00));
}

StringRef BotSupport::weaponIdToAlias (int32_t id) {
   StringRef none = "none";

   if (m_weaponAliases.exists (id)) {
      return m_weaponAliases[id];
   }
   return none;
}

// helper class for reading wave header
class WaveEndianessHelper final : public NonCopyable {
private:
#if defined (CR_ARCH_CPU_BIG_ENDIAN)
   bool little { false };
#else
   bool little { true };
#endif

public:
   uint16_t read16 (uint16_t value) {
      return little ? value : static_cast <uint16_t> ((value >> 8) | (value << 8));
   }

   uint32_t read32 (uint32_t value) {
      return little ? value : (((value & 0x000000ff) << 24) | ((value & 0x0000ff00) << 8) | ((value & 0x00ff0000) >> 8) | ((value & 0xff000000) >> 24));
   }

   bool isWave (char *format) const {
      if (little && memcmp (format, "WAVE", 4) == 0) {
         return true;
      }
      return *reinterpret_cast <uint32_t *> (format) == 0x57415645;
   }
};

float BotSupport::getWaveLength (StringRef filename) {
   auto filePath = strings.joinPath (cv_chatter_path.as <StringRef> (), strings.format ("%s.wav", filename));

   MemFile fp (filePath);

   // we're got valid handle?
   if (!fp) {
      return 0.0f;
   }

   // else fuck with manual search
   struct WavHeader {
      char riff[4];
      uint32_t chunkSize;
      char wave[4];
      char fmt[4];
      uint32_t subchunk1Size;
      uint16_t audioFormat;
      uint16_t numChannels;
      uint32_t sampleRate;
      uint32_t byteRate;
      uint16_t blockAlign;
      uint16_t bitsPerSample;
      char dataChunkId[4];
      uint32_t dataChunkLength;
   } header {};

   static WaveEndianessHelper weh {};

   if (fp.read (&header, sizeof (WavHeader)) == 0) {
      logger.error ("Wave File %s - has wrong or unsupported format", filePath);
      return 0.0f;
   }
   fp.close ();

   if (!weh.isWave (header.wave)) {
      logger.error ("Wave File %s - has wrong wave chunk id", filePath);
      return 0.0f;
   }

   if (weh.read32 (header.dataChunkLength) == 0) {
      logger.error ("Wave File %s - has zero length!", filePath);
      return 0.0f;
   }

   const auto length = static_cast <float> (weh.read32 (header.dataChunkLength));
   const auto bps = static_cast <float> (weh.read16 (header.bitsPerSample)) / 8;
   const auto channels = static_cast <float> (weh.read16 (header.numChannels));
   const auto rate = static_cast <float> (weh.read32 (header.sampleRate));

   return length / bps / channels / rate;
}
