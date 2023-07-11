//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

class BotSupport final : public Singleton <BotSupport> {
private:
   mutable Mutex m_cs {};

private:
   bool m_needToSendWelcome {};
   float m_welcomeReceiveTime {};

   StringArray m_sentences {};
   SmallArray <Client> m_clients {};
   SmallArray <Twin <String, String>> m_tags {};

   HashMap <int32_t, String> m_weaponAlias {};
   Detour <decltype (sendto)> m_sendToDetour { };

public:
   BotSupport ();
   ~BotSupport () = default;

public:
   // need to send welcome message ?
   void checkWelcome ();

   // converts weapon id to alias name
   StringRef weaponIdToAlias (int32_t id);

   // check if origin is visible from the entity side
   bool isVisible (const Vector &origin, edict_t *ent);

   // check if entity is alive
   bool isAlive (edict_t *ent);

   // checks if entity is fakeclient
   bool isFakeClient (edict_t *ent);

   // check if entity is a player
   bool isPlayer (edict_t *ent);

   // check if entity is a monster
   bool isMonster (edict_t *ent);

   // check if entity is a item
   bool isItem (edict_t *ent);

   // check if entity is a vip
   bool isPlayerVIP (edict_t *ent);

   // check if entity is a hostage entity
   bool isHostageEntity (edict_t *ent);

   // check if entity is a door enitty
   bool isDoorEntity (edict_t *ent);

   // this function is checking that pointed by ent pointer obstacle, can be destroyed
   bool isShootableBreakable (edict_t *ent);

   // nearest player search helper
   bool findNearestPlayer (void **holder, edict_t *to, float searchDistance = 4096.0, bool sameTeam = false, bool needBot = false, bool needAlive = false, bool needDrawn = false, bool needBotWithC4 = false);

   // tracing decals for bots spraying logos
   void traceDecals (entvars_t *pev, TraceResult *trace, int logotypeIndex);

   // update stats on clients
   void updateClients ();

   // chat helper to strip the clantags out of the string
   void stripTags (String &line);

   // chat helper to make player name more human-like
   void humanizePlayerName (String &playerName);

   // chat helper to add errors to the bot chat string
   void addChatErrors (String &line);

   // chat helper to find keywords for given string
   bool checkKeywords (StringRef line, String &reply);

   // generates ping bitmask for SVC_PINGS message
   int getPingBitmask (edict_t *ent, int loss, int ping);

   // calculate our own pings for all the players
   void syncCalculatePings ();

   // calculate our own pings for all the players
   void calculatePings ();

   // send modified pings to all the clients
   void emitPings (edict_t *to);

   // installs the sendto function interception
   void installSendTo ();

   // checks if same model omitting the models directory
   bool isModel (const edict_t *ent, StringRef model);

   // get the current date and time as string
   String getCurrentDateTime ();

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
      return m_sendToDetour.restore ();
   }

   // gets the shooting cone deviation
   float getShootingCone (edict_t *ent, const Vector &position) {
      return ent->v.v_angle.forward () | (position - (ent->v.origin + ent->v.view_ofs)).normalize (); // he's facing it, he meant it
   }

   // check if origin is inside view cone of entity
   bool isInViewCone (const Vector &origin, edict_t *ent) {
      return getShootingCone (ent, origin) >= cr::cosf (cr::deg2rad ((ent->v.fov > 0 ? ent->v.fov : 90.0f) * 0.5f));
   }

public:
   static int32_t CR_STDCALL sendTo (int socket, const void *message, size_t length, int flags, const struct sockaddr *dest, int destLength);
};

// expose global
CR_EXPOSE_GLOBAL_SINGLETON (BotSupport, util);
