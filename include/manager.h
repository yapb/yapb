//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// bot creation tab
struct CreateQueue {
   bool manual;
   int difficulty;
   int team;
   int member;
   int personality;
   String name;
};

// manager class
class BotManager final : public Singleton <BotManager> {
public:
   using ForEachBot = Lambda <bool (Bot *)>;
   using UniqueBot = UniquePtr <Bot>;

private:
   float m_timeRoundStart;
   float m_timeRoundEnd;
   float m_timeRoundMid;

   float m_maintainTime; // time to maintain bot creation
   float m_quotaMaintainTime; // time to maintain bot quota
   float m_grenadeUpdateTime; // time to update active grenades
   float m_entityUpdateTime; // time to update intresting entities
   float m_plantSearchUpdateTime; // time to update for searching planted bomb
   float m_lastChatTime; // global chat time timestamp
   float m_timeBombPlanted; // time the bomb were planted
   float m_lastRadioTime[kGameTeamNum]; // global radio time

   int m_lastWinner; // the team who won previous round
   int m_lastDifficulty; // last bots difficulty
   int m_bombSayStatus; // some bot is issued whine about bomb
   int m_lastRadio[kGameTeamNum]; // last radio message for team

   bool m_leaderChoosen[kGameTeamNum]; // is team leader choose theese round
   bool m_economicsGood[kGameTeamNum]; // is team able to buy anything
   bool m_bombPlanted;
   bool m_botsCanPause;
   bool m_roundEnded;

   Array <edict_t *> m_activeGrenades; // holds currently active grenades on the map
   Array <edict_t *> m_intrestingEntities;  // holds currently intresting entities on the map

   SmallArray <CreateQueue> m_creationTab; // bot creation tab
   SmallArray <BotTask> m_filters; // task filters
   SmallArray <UniqueBot> m_bots; // all available bots

   edict_t *m_killerEntity; // killer entity for bots

protected:
   BotCreateResult create (const String &name, int difficulty, int personality, int team, int member);

public:
   BotManager ();
   ~BotManager () = default;

public:
   Twin <int, int> countTeamPlayers ();

   Bot *findBotByIndex (int index);
   Bot *findBotByEntity (edict_t *ent);

   Bot *findAliveBot ();
   Bot *findHighestFragBot (int team);

   int getHumansCount (bool ignoreSpectators = false);
   int getAliveHumansCount ();
   int getBotCount ();
   float getConnectionTime (int botId);

   void setBombPlanted (bool isPlanted);
   void slowFrame ();
   void frame ();
   void createKillerEntity ();
   void destroyKillerEntity ();
   void touchKillerEntity (Bot *bot);
   void destroy ();
   void addbot (const String &name, int difficulty, int personality, int team, int member, bool manual);
   void addbot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual);
   void serverFill (int selection, int personality = Personality::Normal, int difficulty = -1, int numToAdd = -1);
   void kickEveryone (bool instant = false, bool zeroQuota = true);
   bool kickRandom (bool decQuota = true, Team fromTeam = Team::Unassigned);
   void kickBot (int index);
   void kickFromTeam (Team team, bool removeAll = false);
   void killAllBots (int team = -1);
   void maintainQuota ();
   void initQuota ();
   void initRound ();
   void decrementQuota (int by = 1);
   void selectLeaders (int team, bool reset);
   void listBots ();
   void setWeaponMode (int selection);
   void updateTeamEconomics (int team, bool setTrue = false);
   void updateBotDifficulties ();
   void reset ();
   void initFilters ();
   void resetFilters ();
   void updateActiveGrenade ();
   void updateIntrestingEntities ();
   void captureChatRadio (const char *cmd, const char *arg, edict_t *ent);
   void notifyBombDefuse ();
   void execGameEntity (entvars_t *vars);
   void forEach (ForEachBot handler);
   void erase (Bot *bot);
   void handleDeath (edict_t *killer, edict_t *victim);

   bool isTeamStacked (int team);

public:
   Array <edict_t *> &searchActiveGrenades () {
      return m_activeGrenades;
   }

   Array <edict_t *> &searchIntrestingEntities () {
      return m_intrestingEntities;
   }

   bool hasActiveGrenades () const {
      return !m_activeGrenades.empty ();
   }

   bool hasIntrestingEntities () const {
      return !m_intrestingEntities.empty ();
   }

   bool checkTeamEco (int team) const {
      return m_economicsGood[team];
   }

   int getLastWinner () const {
      return m_lastWinner;
   }

   void setLastWinner (int winner) {
      m_lastWinner = winner;
   }

   // get the list of filters
   SmallArray <BotTask> &getFilters () {
      return m_filters;
   }

   void createRandom (bool manual = false) {
      addbot ("", -1, -1, -1, -1, manual);
   }

   bool isBombPlanted () const {
      return m_bombPlanted;
   }

   float getTimeBombPlanted () const {
      return m_timeBombPlanted;
   }

   float getRoundStartTime () const {
      return m_timeRoundStart;
   }

   float getRoundMidTime () const {
      return m_timeRoundMid;
   }

   float getRoundEndTime () const {
      return m_timeRoundEnd;
   }

   bool isRoundOver () const {
      return m_roundEnded;
   }

   void setRoundOver (const bool over) {
      m_roundEnded = over;
   }

   bool canPause () const {
      return m_botsCanPause;
   }

   void setCanPause (const bool pause) {
      m_botsCanPause = pause;
   }

   bool hasBombSay (int type) {
      return (m_bombSayStatus & type) == type;
   }

   void clearBombSay (int type) {
      m_bombSayStatus &= ~type;
   }

   void setPlantedBombSearchTimestamp (const float timestamp) {
      m_plantSearchUpdateTime = timestamp;
   }

   float getPlantedBombSearchTimestamp () const {
      return m_plantSearchUpdateTime;
   }

   void setLastRadioTimestamp (const int team, const float timestamp) {
      if (team == Team::CT || team == Team::Terrorist) {
         m_lastRadioTime[team] = timestamp;
      }
   }

   float getLastRadioTimestamp (const int team) const {
      if (team == Team::CT || team == Team::Terrorist) {
         return m_lastRadioTime[team];
      }
      return 0.0f;
   }

   void setLastRadio (const int team, const int radio) {
      m_lastRadio[team] = radio;
   }

   int getLastRadio (const int team) const {
      return m_lastRadio[team];
   }

   void setLastChatTimestamp (const float timestamp) {
      m_lastChatTime = timestamp;
   }

   float getLastChatTimestamp () const {
      return m_lastChatTime;
   }

   // some bots are online ?
   bool hasBotsOnline () {
      return getBotCount () > 0;
   }

public:
   Bot *operator [] (int index) {
      return findBotByIndex (index);
   }

   Bot *operator [] (edict_t *ent) {
      return findBotByEntity (ent);
   }

public:
   UniqueBot *begin () {
      return m_bots.begin ();
   }

   UniqueBot *begin () const {
      return m_bots.begin ();
   }

   UniqueBot *end () {
      return m_bots.end ();
   }

   UniqueBot *end () const {
      return m_bots.end ();
   }
};

// explose global
static auto &bots = BotManager::get ();