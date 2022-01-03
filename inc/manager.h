//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2022 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// bot creation tab
struct BotRequest {
   bool manual;
   int difficulty;
   int team;
   int skin;
   int personality;
   String name;
};

// initial frustum data
struct FrustumData : public Singleton <FrustumData> {
private:
   float Fov = 75.0f;
   float AspectRatio = 1.33333f;

public:
   float MaxView = 4096.0f;
   float MinView = 5.0f;

public:
   float farHeight; // height of the far frustum
   float farWidth; // width of the far frustum

   float nearHeight; // height of the near frustum
   float nearWidth; // width of the near frustum

public:
   FrustumData () {
      nearHeight = 2.0f * cr::tanf (Fov * 0.0174532925f * 0.5f) * MinView;
      nearWidth = nearHeight * AspectRatio;

      farHeight = 2.0f * cr::tanf (Fov * 0.0174532925f * 0.5f) * MaxView;
      farWidth = farHeight * AspectRatio;
   }
};

// declare global frustum data
CR_EXPOSE_GLOBAL_SINGLETON (FrustumData, frustum);

// manager class
class BotManager final : public Singleton <BotManager> {
public:
   using ForEachBot = Lambda <bool (Bot *)>;
   using UniqueBot = UniquePtr <Bot>;

private:
   float m_timeRoundStart;
   float m_timeRoundEnd;
   float m_timeRoundMid;

   float m_autoKillCheckTime; // time to kill all the bots ?
   float m_maintainTime; // time to maintain bot creation
   float m_quotaMaintainTime; // time to maintain bot quota
   float m_grenadeUpdateTime {}; // time to update active grenades
   float m_entityUpdateTime {}; // time to update intresting entities
   float m_plantSearchUpdateTime {}; // time to update for searching planted bomb
   float m_lastChatTime {}; // global chat time timestamp
   float m_timeBombPlanted {}; // time the bomb were planted
   float m_lastRadioTime[kGameTeamNum] {}; // global radio time

   int m_lastWinner; // the team who won previous round
   int m_lastDifficulty; // last bots difficulty
   int m_bombSayStatus; // some bot is issued whine about bomb
   int m_lastRadio[kGameTeamNum] {}; // last radio message for team

   bool m_leaderChoosen[kGameTeamNum] {}; // is team leader choose theese round
   bool m_economicsGood[kGameTeamNum] {}; // is team able to buy anything
   bool m_bombPlanted;
   bool m_botsCanPause;
   bool m_roundOver;

   Array <edict_t *> m_activeGrenades; // holds currently active grenades on the map
   Array <edict_t *> m_intrestingEntities;  // holds currently intresting entities on the map

   Deque <String> m_saveBotNames; // bots names that persist upon changelevel
   Deque <BotRequest> m_addRequests; // bot creation tab
   SmallArray <BotTask> m_filters; // task filters
   SmallArray <UniqueBot> m_bots; // all available bots

   edict_t *m_killerEntity; // killer entity for bots
   FrustumData m_frustumData {};

protected:
   BotCreateResult create (StringRef name, int difficulty, int personality, int team, int skin);

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

   float getConnectTime (StringRef name, float original);
   float getAverageTeamKPD (bool calcForBots);

   void setBombPlanted (bool isPlanted);
   void frame ();
   void createKillerEntity ();
   void destroyKillerEntity ();
   void touchKillerEntity (Bot *bot);
   void destroy ();
   void addbot (StringRef name, int difficulty, int personality, int team, int skin, bool manual);
   void addbot (StringRef name, StringRef difficulty, StringRef personality, StringRef team, StringRef skin, bool manual);
   void serverFill (int selection, int personality = Personality::Normal, int difficulty = -1, int numToAdd = -1);
   void kickEveryone (bool instant = false, bool zeroQuota = true);
   void kickBot (int index);
   void kickFromTeam (Team team, bool removeAll = false);
   void killAllBots (int team = -1);
   void maintainQuota ();
   void maintainAutoKill ();
   void maintainLeaders ();
   void initQuota ();
   void initRound ();
   void decrementQuota (int by = 1);
   void selectLeaders (int team, bool reset);
   void listBots ();
   void setWeaponMode (int selection);
   void updateTeamEconomics (int team, bool setTrue = false);
   void updateBotDifficulties ();
   void balanceBotDifficulties ();
   void reset ();
   void initFilters ();
   void resetFilters ();
   void updateActiveGrenade ();
   void updateIntrestingEntities ();
   void captureChatRadio (const char *cmd, const char *arg, edict_t *ent);
   void notifyBombDefuse ();
   void execGameEntity (edict_t *ent);
   void forEach (ForEachBot handler);
   void erase (Bot *bot);
   void handleDeath (edict_t *killer, edict_t *victim);
   void setLastWinner (int winner);

   bool isTeamStacked (int team);
   bool kickRandom (bool decQuota = true, Team fromTeam = Team::Unassigned);

public:
   const Array <edict_t *> &getActiveGrenades () {
      return m_activeGrenades;
   }

   const Array <edict_t *> &getIntrestingEntities () {
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

   int32 getLastWinner () const {
      return m_lastWinner;
   }

   int32 getBotCount () {
      return m_bots.length <int32> ();
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
      return m_roundOver;
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
CR_EXPOSE_GLOBAL_SINGLETON (BotManager, bots);
