//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// noise types
CR_DECLARE_SCOPED_ENUM (Noise,
   NeedHandle = cr::bit (0),
   HitFall = cr::bit (1),
   Pickup = cr::bit (2),
   Zoom = cr::bit (3),
   Ammo = cr::bit (4),
   Hostage = cr::bit (5),
   Broke = cr::bit (6),
   Door = cr::bit (7)
)

class BotUtils final : public Singleton <BotUtils> {
private:
   bool m_needToSendWelcome;
   float m_welcomeReceiveTime;

   StringArray m_sentences;
   SmallArray <Client> m_clients;
   SmallArray <Twin <String, String>> m_tags;

   Dictionary <String, int32> m_noiseCache;
   SimpleHook m_sendToHook;

public:
   BotUtils ();
   ~BotUtils () = default;

public:
   // need to send welcome message ?
   void checkWelcome ();

   // gets the weapon alias as hlsting, maybe move to config...
   int getWeaponAlias (bool needString, const char *weaponAlias, int weaponIndex = -1);

   // gets the build number of bot
   int buildNumber ();

   // check if origin is visible from the entity side
   bool isVisible (const Vector &origin, edict_t *ent);

   // check if entity is alive
   bool isAlive (edict_t *ent);

   // checks if entitiy is fakeclient
   bool isFakeClient (edict_t *ent);

   // check if entitiy is a player
   bool isPlayer (edict_t *ent);

   // check if entity is a vip
   bool isPlayerVIP (edict_t *ent);

   // opens config helper
   bool openConfig (const char *fileName, const char *errorIfNotExists, MemFile *outFile, bool languageDependant = false);

   // nearest player search helper
   bool findNearestPlayer (void **holder, edict_t *to, float searchDistance = 4096.0, bool sameTeam = false, bool needBot = false, bool needAlive = false, bool needDrawn = false, bool needBotWithC4 = false);

   // tracing decals for bots spraying logos
   void traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex);

   // attaches sound to client struct
   void listenNoise (edict_t *ent, const String &sample, float volume);

   // simulate sound for players
   void simulateNoise (int playerIndex);

   // update stats on clients
   void updateClients ();

   // chat helper to strip the clantags out of the string
   void stripTags (String &line);

   // chat helper to make player name more human-like
   void humanizePlayerName (String &playerName);

   // chat helper to add errors to the bot chat string
   void addChatErrors (String &line);

   // chat helper to find keywords for given string
   bool checkKeywords (const String &line, String &reply);

   // generates ping bitmask for SVC_PINGS message
   int getPingBitmask (edict_t *ent, int loss, int ping);

   // calculate our own pings for all the players
   void calculatePings ();

   // send modified pings to all the clients
   void sendPings (edict_t *to);

   // installs the sendto function intreception
   void installSendTo ();

public:

   // re-show welcome after changelevel ?
   void setNeedForWelcome (bool need) {
      m_needToSendWelcome = need;
   }

   // get array of clients
   SmallArray <Client> &getClients () {
      return m_clients;
   }

   // get clients as const-reference
   const SmallArray <Client> &getClients () const {
      return m_clients;
   }

   // get single client as ref
   Client &getClient (const int index) {
      return m_clients[index];
   }

   // disables send hook
   bool disableSendTo () {
      return m_sendToHook.disable ();
   }

   // enables send hook
   bool enableSendTo () {
      return m_sendToHook.enable ();
   }

   // gets the shooting cone deviation
   float getShootingCone (edict_t *ent, const Vector &position) {
      return ent->v.v_angle.forward () | (position - (ent->v.origin + ent->v.view_ofs)).normalize (); // he's facing it, he meant it
   }

   // check if origin is inside view cone of entity
   bool isInViewCone (const Vector &origin, edict_t *ent) {
      return getShootingCone (ent, origin) >= cr::cosf (cr::degreesToRadians ((ent->v.fov > 0 ? ent->v.fov : 90.0f) * 0.5f));
   }

public:
   static int32 CR_STDCALL sendTo (int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int destLength);
};

// explose global
static auto &util = BotUtils::get ();