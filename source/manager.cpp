//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_autovacate ("yb_autovacate", "1");

ConVar yb_quota ("yb_quota", "0", VT_NORMAL);
ConVar yb_quota_mode ("yb_quota_mode", "normal");
ConVar yb_quota_match ("yb_quota_match", "0");
ConVar yb_think_fps ("yb_think_fps", "30.0");

ConVar yb_join_after_player ("yb_join_after_player", "0");
ConVar yb_join_team ("yb_join_team", "any");

ConVar yb_name_prefix ("yb_name_prefix", "");
ConVar yb_difficulty ("yb_difficulty", "4");

ConVar yb_latency_display ("yb_latency_display", "2");
ConVar yb_avatar_display ("yb_avatar_display", "1");

ConVar mp_limitteams ("mp_limitteams", nullptr, VT_NOREGISTER);
ConVar mp_autoteambalance ("mp_autoteambalance", nullptr, VT_NOREGISTER);

BotManager::BotManager (void) {
   // this is a bot manager class constructor

   m_lastWinner = -1;
   m_deathMsgSent = false;

   for (int i = 0; i < MAX_TEAM_COUNT; i++) {
      m_leaderChoosen[i] = false;
      m_economicsGood[i] = true;
   }
   memset (m_bots, 0, sizeof (m_bots));
   reset ();

   m_creationTab.clear ();
   m_killerEntity = nullptr;

   m_activeGrenades.reserve (16);
   m_intrestingEntities.reserve (128);
}

BotManager::~BotManager (void) {
   // this is a bot manager class destructor, do not use engine.MaxClients () here !!
   destroy ();
}

void BotManager::createKillerEntity (void) {
   // this function creates single trigger_hurt for using in Bot::Kill, to reduce lags, when killing all the bots

   m_killerEntity = g_engfuncs.pfnCreateNamedEntity (MAKE_STRING ("trigger_hurt"));

   m_killerEntity->v.dmg = 9999.0f;
   m_killerEntity->v.dmg_take = 1.0f;
   m_killerEntity->v.dmgtime = 2.0f;
   m_killerEntity->v.effects |= EF_NODRAW;

   g_engfuncs.pfnSetOrigin (m_killerEntity, Vector (-99999.0f, -99999.0f, -99999.0f));
   MDLL_Spawn (m_killerEntity);
}

void BotManager::destroyKillerEntity (void) {
   if (!engine.isNullEntity (m_killerEntity)) {
      g_engfuncs.pfnRemoveEntity (m_killerEntity);
   }
}

void BotManager::touchKillerEntity (Bot *bot) {

   // bot is already dead.
   if (!bot->m_notKilled) {
      return;
   }

   if (engine.isNullEntity (m_killerEntity)) {
      createKillerEntity ();

      if (engine.isNullEntity (m_killerEntity)) {
         MDLL_ClientKill (bot->ent ());
         return;
      }
   }

   m_killerEntity->v.classname = MAKE_STRING (g_weaponDefs[bot->m_currentWeapon].className);
   m_killerEntity->v.dmg_inflictor = bot->ent ();

   KeyValueData kv;
   kv.szClassName = const_cast <char *> (g_weaponDefs[bot->m_currentWeapon].className);
   kv.szKeyName = "damagetype";
   kv.szValue = const_cast <char *> (format ("%d", (1 << 4)));
   kv.fHandled = FALSE;

   MDLL_KeyValue (m_killerEntity, &kv);
   MDLL_Touch (m_killerEntity, bot->ent ());
}

// it's already defined in interface.cpp
extern "C" void player (entvars_t *pev);

void BotManager::execGameEntity (entvars_t *vars) {
   // this function calls gamedll player() function, in case to create player entity in game

   if (g_gameFlags & GAME_METAMOD) {
      CALL_GAME_ENTITY (PLID, "player", vars);
      return;
   }
   player (vars);
}

BotCreationResult BotManager::create (const String &name, int difficulty, int personality, int team, int member) {
   // this function completely prepares bot entity (edict) for creation, creates team, difficulty, sets named etc, and
   // then sends result to bot constructor

   edict_t *bot = nullptr;
   String resultName;

   // do not allow create bots when there is no waypoints
   if (!waypoints.length ()) {
      engine.centerPrint ("Map is not waypointed. Cannot create bot");
      return BOT_RESULT_NAV_ERROR;
   }

   // don't allow creating bots with changed waypoints (distance tables are messed up)
   else if (waypoints.hasChanged ())  {
      engine.centerPrint ("Waypoints have been changed. Load waypoints again...");
      return BOT_RESULT_NAV_ERROR;
   }
   else if (team != -1 && isTeamStacked (team - 1)) {
      engine.centerPrint ("Desired team is stacked. Unable to proceed with bot creation");
      return BOT_RESULT_TEAM_STACKED;
   }
   if (difficulty < 0 || difficulty > 4) {
      difficulty = yb_difficulty.integer ();

      if (difficulty < 0 || difficulty > 4) {
         difficulty = rng.getInt (3, 4);
         yb_difficulty.set (difficulty);
      }
   }

   if (personality < PERSONALITY_NORMAL || personality > PERSONALITY_CAREFUL) {
      if (rng.getInt (0, 100) < 50) {
         personality = PERSONALITY_NORMAL;
      }
      else {
         if (rng.getInt (0, 100) < 50) {
            personality = PERSONALITY_RUSHER;
         }
         else {
            personality = PERSONALITY_CAREFUL;
         }
      }
   }

   String steamId = "";
   BotName *botName = nullptr;

   // setup name
   if (name.empty ()) {
      if (!g_botNames.empty ()) {
         bool nameFound = false;

         for (int i = 0; i < MAX_ENGINE_PLAYERS * 4; i++) {
            if (nameFound) {
               break;
            }
            botName = &g_botNames.random ();

            if (botName->name.length () < 3 || botName->usedBy != 0) {
               continue;
            }
            nameFound = true;

            resultName = botName->name;
            steamId = botName->steamId;
         }
      }
      else
         resultName.format ("yapb_%d.%d", rng.getInt (0, 10), rng.getInt (0, 10)); // just pick ugly random name
   }
   else {
      resultName = name;
   }

   if (!isEmptyStr (yb_name_prefix.str ())) {
      String prefixed; // temp buffer for storing modified name
      prefixed.format ("%s %s", yb_name_prefix.str (), resultName.chars ());

      // buffer has been modified, copy to real name
      resultName = cr::move (prefixed);
   }
   bot = g_engfuncs.pfnCreateFakeClient (resultName.chars ());

   if (engine.isNullEntity (bot)) {
      engine.centerPrint ("Maximum players reached (%d/%d). Unable to create Bot.", engine.maxClients (), engine.maxClients ());
      return BOT_RESULT_MAX_PLAYERS_REACHED;
   }
   int index = engine.indexOfEntity (bot) - 1;

   // ensure it free
   destroy (index);

   assert (index >= 0 && index <= MAX_ENGINE_PLAYERS); // check index
   assert (m_bots[index] == nullptr); // check bot slot

   m_bots[index] = new Bot (bot, difficulty, personality, team, member, steamId);

   // assign owner of bot name
   if (botName != nullptr) {
      botName->usedBy = m_bots[index]->index ();
   }
   engine.print ("Connecting Bot...");

   return BOT_RESULT_CREATED;
}

int BotManager::index (edict_t *ent) {
   // this function returns index of bot (using own bot array)
   if (engine.isNullEntity (ent)) {
      return -1;
   }
   int index = engine.indexOfEntity (ent) - 1;

   if (index < 0 || index >= MAX_ENGINE_PLAYERS) {
      return -1;
   }

   if (m_bots[index] != nullptr) {
      return index;
   }
   return -1; // if no edict, return -1;
}

Bot *BotManager::getBot (int index) {
   // this function finds a bot specified by index, and then returns pointer to it (using own bot array)

   if (index < 0 || index >= MAX_ENGINE_PLAYERS) {
      return nullptr;
   }

   if (m_bots[index] != nullptr) {
      return m_bots[index];
   }
   return nullptr; // no bot
}

Bot *BotManager::getBot (edict_t *ent) {
   // same as above, but using bot entity

   return getBot (index (ent));
}

Bot *BotManager::getAliveBot (void) {
   // this function finds one bot, alive bot :)

   IntArray result;

   for (int i = 0; i < engine.maxClients (); i++) {
      if (result.length () > 4) {
         break;
      }
      if (m_bots[i] != nullptr && isAlive (m_bots[i]->ent ())) {
         result.push (i);
      }
   }

   if (!result.empty ()) {
      return m_bots[result.random ()];
   }
   return nullptr;
}

void BotManager::framePeriodic (void) {
   // this function calls think () function for all available at call moment bots

   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr) {
         bot->framePeriodic ();
      }
   }
}

void BotManager::frame (void) {
   // this function calls periodic SecondThink () function for all available at call moment bots

   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr) {
         bot->frame ();
      }
   }

   // select leader each team somewhere in round start
   if (g_timeRoundStart + 5.0f > engine.timebase () && g_timeRoundStart + 10.0f < engine.timebase ()) {
      for (int team = 0; team < MAX_TEAM_COUNT; team++) {
         selectLeaders (team, false);
      }
   }
}

void BotManager::addbot (const String &name, int difficulty, int personality, int team, int member, bool manual) {
   // this function putting bot creation process to queue to prevent engine crashes

   CreateQueue create;

   // fill the holder
   create.name = name;
   create.difficulty = difficulty;
   create.personality = personality;
   create.team = team;
   create.member = member;
   create.manual = manual;

   // put to queue
   m_creationTab.push (cr::move (create));
}

void BotManager::addbot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual) {
   // this function is same as the function above, but accept as parameters string instead of integers

   CreateQueue create;
   const String &any = "*";

   create.name = (name.empty () || name == any) ? String ("\0") : name;
   create.difficulty = (difficulty.empty () || difficulty == any) ? -1 : difficulty.toInt32 ();
   create.team = (team.empty () || team == any) ? -1 : team.toInt32 ();
   create.member = (member.empty () || member == any) ? -1 : member.toInt32 ();
   create.personality = (personality.empty () || personality == any) ? -1 : personality.toInt32 ();
   create.manual = manual;

   m_creationTab.push (cr::move (create));
}

void BotManager::maintainQuota (void) {
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (waypoints.length () < 1 || waypoints.hasChanged ()) {
      if (yb_quota.integer () > 0) {
         engine.centerPrint ("Map is not waypointed. Cannot create bot");
      }
      yb_quota.set (0);
      return;
   }

   // bot's creation update
   if (!m_creationTab.empty () && m_maintainTime < engine.timebase ()) {
      const CreateQueue &last = m_creationTab.pop ();
      const BotCreationResult callResult = create (last.name, last.difficulty, last.personality, last.team, last.member);

      if (last.manual) {
         yb_quota.set (yb_quota.integer () + 1);
      }

      // check the result of creation
      if (callResult == BOT_RESULT_NAV_ERROR) {
         m_creationTab.clear (); // something wrong with waypoints, reset tab of creation
         yb_quota.set (0); // reset quota
      }
      else if (callResult == BOT_RESULT_MAX_PLAYERS_REACHED) {
         m_creationTab.clear (); // maximum players reached, so set quota to maximum players
         yb_quota.set (getBotCount ());
      }
      else if (callResult == BOT_RESULT_TEAM_STACKED) {
         engine.print ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round)");

         m_creationTab.clear ();
         yb_quota.set (getBotCount ());
      }
      m_maintainTime = engine.timebase () + 0.10f;
   }

   // now keep bot number up to date
   if (m_quotaMaintainTime > engine.timebase ()) {
      return;
   }
   yb_quota.set (cr::clamp <int> (yb_quota.integer (), 0, engine.maxClients ()));

   int totalHumansInGame = getHumansCount ();
   int humanPlayersInGame = getHumansCount (true);

   if (!engine.isDedicated () && !totalHumansInGame) {
      return;
   }
   int desiredBotCount = yb_quota.integer ();
   int botsInGame = getBotCount ();

   bool halfRoundPassed = g_gameWelcomeSent && g_timeRoundMid > engine.timebase ();

   if (stricmp (yb_quota_mode.str (), "fill") == 0) {
      if (halfRoundPassed) {
         desiredBotCount = botsInGame;
      }
      else {
         desiredBotCount = cr::max <int> (0, desiredBotCount - humanPlayersInGame);
      }
   }
   else if (stricmp (yb_quota_mode.str (), "match") == 0) {
      if (halfRoundPassed) {
         desiredBotCount = botsInGame;
      }
      else {
         int quotaMatch = yb_quota_match.integer () == 0 ? yb_quota.integer () : yb_quota_match.integer ();
         desiredBotCount = cr::max <int> (0, quotaMatch * humanPlayersInGame);
      }
   }

   if (yb_join_after_player.boolean () && humanPlayersInGame == 0) {
      desiredBotCount = 0;
   }
   int maxClients = engine.maxClients ();

   if (yb_autovacate.boolean ()) {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - (humanPlayersInGame + 1));
   }
   else {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - humanPlayersInGame);
   }

   // add bots if necessary
   if (desiredBotCount > botsInGame) {
      createRandom ();
   }
   else if (desiredBotCount < botsInGame) {
      kickRandom (false);
   }
   m_quotaMaintainTime = engine.timebase () + 0.40f;
}

void BotManager::reset (void) {
   m_maintainTime = 0.0f;
   m_quotaMaintainTime = 0.0f;
   m_grenadeUpdateTime = 0.0f;
   m_entityUpdateTime = 0.0f;

   m_intrestingEntities.clear ();
   m_activeGrenades.clear ();
}

void BotManager::decrementQuota (int by) {
   if (by != 0) {
      yb_quota.set (cr::clamp <int> (yb_quota.integer () - by, 0, yb_quota.integer ()));
      return;
   }
   yb_quota.set (0);
}

void BotManager::initQuota (void) {
   m_maintainTime = engine.timebase () + 3.0f;
   m_quotaMaintainTime = engine.timebase () + 3.0f;

   m_creationTab.clear ();
}

void BotManager::serverFill (int selection, int personality, int difficulty, int numToAdd) {
   // this function fill server with bots, with specified team & personality

   // always keep one slot
   int maxClients = yb_autovacate.boolean () ? engine.maxClients () - 1 - (engine.isDedicated () ? 0 : getHumansCount ()) : engine.maxClients ();

   if (getBotCount () >= maxClients - getHumansCount ()) {
      return;
   }
   if (selection == 1 || selection == 2) {
      mp_limitteams.set (0);
      mp_autoteambalance.set (0);
   }
   else {
      selection = 5;
   }
   char teams[6][12] = {"", {"Terrorists"}, {"CTs"}, "", "", {"Random"}, };

   int toAdd = numToAdd == -1 ? maxClients - (getHumansCount () + getBotCount ()) : numToAdd;

   for (int i = 0; i <= toAdd; i++) {
      addbot ("", difficulty, personality, selection, -1, true);
   }
   engine.centerPrint ("Fill Server with %s bots...", &teams[selection][0]);
}

void BotManager::kickEveryone (bool instant, bool zeroQuota) {
   // this function drops all bot clients from server (this function removes only yapb's)`q

   engine.centerPrint ("Bots are removed from server.");

   if (zeroQuota) {
      decrementQuota (0);
   }

   if (instant) {
      for (int i = 0; i < engine.maxClients (); i++) {
         auto bot = m_bots[i];

         if (bot != nullptr) {
            bot->kick ();
         }
      }
   }
   m_creationTab.clear ();
}

void BotManager::kickFromTeam (Team team, bool removeAll) {
   // this function remove random bot from specified team (if removeAll value = 1 then removes all players from team)

   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr && team == bot->m_team) {
         decrementQuota ();
         bot->kick ();

         if (!removeAll) {
            break;
         }
      }
   }
}

void BotManager::kickBotByMenu (edict_t *ent, int selection) {
   // this function displays remove bot menu to specified entity (this function show's only yapb's).

   if (selection > 4 || selection < 1)
      return;

   String menus;
   menus.format ("\\yBots Remove Menu (%d/4):\\w\n\n", selection);

   int menuKeys = (selection == 4) ? (1 << 9) : ((1 << 8) | (1 << 9));
   int menuKey = (selection - 1) * 8;

   for (int i = menuKey; i < selection * 8; i++) {
      auto bot = getBot (i);

      if (bot != nullptr && (bot->pev->flags & FL_FAKECLIENT)) {
         menuKeys |= 1 << (cr::abs (i - menuKey));
         menus.formatAppend ("%1.1d. %s%s\n", i - menuKey + 1, STRING (bot->pev->netname), bot->m_team == TEAM_COUNTER ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else {
         menus.formatAppend ("\\d %1.1d. Not a Bot\\w\n", i - menuKey + 1);
      }
   }
   menus.formatAppend ("\n%s 0. Back", (selection == 4) ? "" : " 9. More...\n");

   // force to clear current menu
   showMenu (ent, BOT_MENU_INVALID);

   auto searchMenu = [](MenuId id) {
      int menuIndex = 0;

      for (; menuIndex < BOT_MENU_TOTAL_MENUS; menuIndex++) {
         if (g_menus[menuIndex].id == id) {
            break;
         }
      }
      return &g_menus[menuIndex];
   };

   const unsigned int slots = menuKeys & static_cast <unsigned int> (-1);
   MenuId id = static_cast <MenuId> (BOT_MENU_KICK_PAGE_1 - 1 + selection);

   auto menu = searchMenu (id);

   menu->slots = slots;
   menu->text = menus;

   showMenu (ent, id);
}

void BotManager::killAllBots (int team) {
   // this function kills all bots on server (only this dll controlled bots)

   for (int i = 0; i < engine.maxClients (); i++) {
      if (m_bots[i] != nullptr) {
         if (team != -1 && team != m_bots[i]->m_team) {
            continue;
         }
         m_bots[i]->kill ();
      }
   }
   engine.centerPrint ("All Bots died !");
}

void BotManager::kickBot (int index) {
   auto bot = getBot (index);

   if (bot) {
      decrementQuota ();
      bot->kick ();
   }
}

void BotManager::kickRandom (bool decQuota) {
   // this function removes random bot from server (only yapb's)

   bool deadBotFound = false;

   auto updateQuota = [&](void) {
      if (decQuota) {
         decrementQuota ();
      }
   };

   // first try to kick the bot that is currently dead
   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = bots.getBot (i);

      if (bot != nullptr && !bot->m_notKilled) // is this slot used?
      {
         updateQuota ();
         bot->kick ();

         deadBotFound = true;
         break;
      }
   }

   if (deadBotFound) {
      return;
   }

   // if no dead bots found try to find one with lowest amount of frags
   int index = 0;
   float score = 9999.0f;

   // search bots in this team
   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = bots.getBot (i);

      if (bot != nullptr && bot->pev->frags < score) {
         index = i;
         score = bot->pev->frags;
      }
   }

   // if found some bots
   if (index != 0) {
      updateQuota ();
      m_bots[index]->kick ();

      return;
   }

   // worst case, just kick some random bot
   for (int i = 0; i < engine.maxClients (); i++) {
      if (m_bots[i] != nullptr) // is this slot used?
      {
         updateQuota ();
         m_bots[i]->kick ();

         break;
      }
   }
}

void BotManager::setWeaponMode (int selection) {
   // this function sets bots weapon mode

   int tabMapStandart[7][NUM_WEAPONS] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Knife only
      {-1, -1, -1, 2, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Pistols only
      {-1, -1, -1, -1, -1, -1, -1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Shotgun only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 1, 2, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1}, // Machine Guns only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 0, 1, 1, -1, -1, -1, -1, -1, -1}, // Rifles only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 2, 0, 1, -1, -1}, // Snipers only
      {-1, -1, -1, 2, 2, 0, 1, 2, 2, 2, 1, 2, 0, 2, 0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 2, 1} // Standard
   };

   int tabMapAS[7][NUM_WEAPONS] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Knife only
      {-1, -1, -1, 2, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Pistols only
      {-1, -1, -1, -1, -1, -1, -1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Shotgun only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, -1}, // Machine Guns only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, 1, 0, 1, 1, -1, -1, -1, -1, -1, -1}, // Rifles only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, -1, 1, -1, -1}, // Snipers only
      {-1, -1, -1, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0, -1, 1, 0, 1, 1, 0, 0, -1, 1, 1, 1} // Standard
   };

   char modeName[7][12] = {{"Knife"}, {"Pistol"}, {"Shotgun"}, {"Machine Gun"}, {"Rifle"}, {"Sniper"}, {"Standard"}};
   selection--;

   for (int i = 0; i < NUM_WEAPONS; i++) {
      g_weaponSelect[i].teamStandard = tabMapStandart[selection][i];
      g_weaponSelect[i].teamAS = tabMapAS[selection][i];
   }

   if (selection == 0) {
      yb_jasonmode.set (1);
   }
   else {
      yb_jasonmode.set (0);
   }
   engine.centerPrint ("%s weapon mode selected", &modeName[selection][0]);
}

void BotManager::listBots (void) {
   // this function list's bots currently playing on the server

   engine.print ("%-3.5s %-9.13s %-17.18s %-3.4s %-3.4s %-3.4s", "index", "name", "personality", "team", "difficulty", "frags");

   for (int i = 0; i < engine.maxClients (); i++) {
      Bot *bot = getBot (i);

      // is this player slot valid
      if (bot != nullptr) {
         engine.print ("[%-3.1d] %-9.13s %-17.18s %-3.4s %-3.1d %-3.1d", i, STRING (bot->pev->netname), bot->m_personality == PERSONALITY_RUSHER ? "rusher" : bot->m_personality == PERSONALITY_NORMAL ? "normal" : "careful", bot->m_team == TEAM_COUNTER ? "CT" : "T", bot->m_difficulty, static_cast <int> (bot->pev->frags));
      }
   }
}

int BotManager::getBotCount (void) {
   // this function returns number of yapb's playing on the server

   int count = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      if (m_bots[i] != nullptr) {
         count++;
      }
   }
   return count;
}

Bot *BotManager::getHighfragBot (int team) {
   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = bots.getBot (i);

      if (bot != nullptr && bot->m_notKilled && bot->m_team == team) {
         if (bot->pev->frags > bestScore) {
            bestIndex = i;
            bestScore = bot->pev->frags;
         }
      }
   }
   return getBot (bestIndex);
}

void BotManager::updateTeamEconomics (int team, bool forceGoodEconomics) {
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar yb_economics_rounds;

   if (forceGoodEconomics || !yb_economics_rounds.boolean ()) {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (int i = 0; i < engine.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr && bot->m_team == team) {
         if (bot->m_moneyAmount <= g_botBuyEconomyTable[0]) {
            numPoorPlayers++;
         }
         numTeamPlayers++; // update count of team
      }
   }
   m_economicsGood[team] = true;

   if (numTeamPlayers <= 1) {
      return;
   }
   // if 80 percent of team have no enough money to purchase primary weapon
   if ((numTeamPlayers * 80) / 100 <= numPoorPlayers) {
      m_economicsGood[team] = false;
   }

   // winner must buy something!
   if (m_lastWinner == team) {
      m_economicsGood[team] = true;
   }
}

void BotManager::destroy (void) {
   // this function free all bots slots (used on server shutdown)

   for (int i = 0; i < MAX_ENGINE_PLAYERS; i++) {
      destroy (i);
   }
}

void BotManager::destroy (int index) {
   // this function frees one bot selected by index (used on bot disconnect)

   delete m_bots[index];
   m_bots[index] = nullptr;
}

Bot::Bot (edict_t *bot, int difficulty, int personality, int team, int member, const String &steamId) {
   // this function does core operation of creating bot, it's called by CreateBot (),
   // when bot setup completed, (this is a bot class constructor)

   int clientIndex = engine.indexOfEntity (bot);

   memset (reinterpret_cast <void *> (this), 0, sizeof (*this));
   pev = &bot->v;

   if (bot->pvPrivateData != nullptr) {
      g_engfuncs.pfnFreeEntPrivateData (bot);
   }

   bot->pvPrivateData = nullptr;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   BotManager::execGameEntity (&bot->v);

   // set all info buffer keys for this bot
   char *buffer = g_engfuncs.pfnGetInfoKeyBuffer (bot);
   g_engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "_vgui_menus", "0");

   if (!(g_gameFlags & GAME_LEGACY) && yb_latency_display.integer () == 1) {
      g_engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*bot", "1");
   }

   char reject[256] = {0, };
   MDLL_ClientConnect (bot, STRING (bot->v.netname), format ("127.0.0.%d", engine.indexOfEntity (bot) + 100), reject);

   if (!isEmptyStr (reject)) {
      logEntry (true, LL_WARNING, "Server refused '%s' connection (%s)", STRING (bot->v.netname), reject);
      engine.execCmd ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
      return;
   }

   // should be set after client connect
   if (yb_avatar_display.boolean () && !steamId.empty ()) {
      g_engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*sid", steamId.chars ());
   }
   memset (&m_pingOffset, 0, sizeof (m_pingOffset));
   memset (&m_ping, 0, sizeof (m_ping));

   MDLL_ClientPutInServer (bot);
   bot->v.flags |= FL_FAKECLIENT; // set this player as fakeclient

   // initialize all the variables for this bot...
   m_notStarted = true; // hasn't joined game yet
   m_forceRadio = false;

   m_startAction = GAME_MSG_NONE;
   m_retryJoin = 0;
   m_moneyAmount = 0;
   m_logotypeIndex = rng.getInt (0, 9);
   m_tasks.reserve (TASK_MAX);

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = rng.getFloat (3.8f, 10.0f);
   m_sayTextBuffer.chatProbability = rng.getInt (1, 100);

   m_notKilled = false;
   m_weaponBurstMode = BM_OFF;
   m_difficulty = difficulty;

   if (difficulty < 0 || difficulty > 4) {
      difficulty = rng.getInt (3, 4);
      yb_difficulty.set (difficulty);
   }

   m_lastCommandTime = engine.timebase () - 0.1f;
   m_frameInterval = engine.timebase ();
   m_timePeriodicUpdate = 0.0f;

   switch (personality) {
   case 1:
      m_personality = PERSONALITY_RUSHER;
      m_baseAgressionLevel = rng.getFloat (0.7f, 1.0f);
      m_baseFearLevel = rng.getFloat (0.0f, 0.4f);
      break;

   case 2:
      m_personality = PERSONALITY_CAREFUL;
      m_baseAgressionLevel = rng.getFloat (0.2f, 0.5f);
      m_baseFearLevel = rng.getFloat (0.7f, 1.0f);
      break;

   default:
      m_personality = PERSONALITY_NORMAL;
      m_baseAgressionLevel = rng.getFloat (0.4f, 0.7f);
      m_baseFearLevel = rng.getFloat (0.4f, 0.7f);
      break;
   }

   memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
   memset (&m_ammo, 0, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_voicePitch = rng.getInt (80, 115); // assign voice pitch

   // copy them over to the temp level variables
   m_agressionLevel = m_baseAgressionLevel;
   m_fearLevel = m_baseFearLevel;
   m_nextEmotionUpdate = engine.timebase () + 0.5f;

   // just to be sure
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // assign team and class
   m_wantedTeam = team;
   m_wantedClass = member;

   newRound ();
}

void Bot::clearUsedName (void) {
   for (auto &name : g_botNames) {
      if (name.usedBy == index ()) {
         name.usedBy = 0;
         break;
      }
   }
}

float Bot::calcThinkInterval (void) {
   return cr::fzero (m_thinkInterval) ? m_frameInterval : m_thinkInterval;
}

Bot::~Bot (void) {
   // this is bot destructor

   clearUsedName ();
   clearSearchNodes ();
   clearRoute ();
   clearTasks ();
}

int BotManager::getHumansCount (bool ignoreSpectators) {
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if ((client.flags & CF_USED) && m_bots[i] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
         if (ignoreSpectators && client.team2 != TEAM_TERRORIST && client.team2 != TEAM_COUNTER) {
            continue;
         }
         count++;
      }
   }
   return count;
}

int BotManager::getAliveHumansCount (void) {
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if ((client.flags & (CF_USED | CF_ALIVE)) && m_bots[i] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
         count++;
      }
   }
   return count;
}

bool BotManager::isTeamStacked (int team) {
   if (team != TEAM_COUNTER && team != TEAM_TERRORIST) {
      return false;
   }
   int limitTeams = mp_limitteams.integer ();

   if (!limitTeams) {
      return false;
   }
   int teamCount[MAX_TEAM_COUNT] = { 0, };

   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if ((client.flags & CF_USED) && client.team2 != TEAM_UNASSIGNED && client.team2 != TEAM_SPECTATOR) {
         teamCount[client.team2]++;
      }
   }
   return teamCount[team] + 1 > teamCount[team == TEAM_COUNTER ? TEAM_TERRORIST : TEAM_COUNTER] + limitTeams;
}

void Bot::newRound (void) {
   // this function initializes a bot after creation & at the start of each round

   int i = 0;

   // delete all allocated path nodes
   clearSearchNodes ();
   clearRoute ();

   m_waypointOrigin.nullify ();
   m_destOrigin.nullify ();
   m_currentPath = nullptr;
   m_currentTravelFlags = 0;
   m_desiredVelocity.nullify ();
   m_currentWaypointIndex = INVALID_WAYPOINT_INDEX;
   m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
   m_chosenGoalIndex = INVALID_WAYPOINT_INDEX;
   m_loosedBombWptIndex = INVALID_WAYPOINT_INDEX;
   m_plantedBombWptIndex = INVALID_WAYPOINT_INDEX;

   m_grenadeRequested = false;
   m_moveToC4 = false;
   m_duckDefuse = false;
   m_duckDefuseCheckTime = 0.0f;

   m_numFriendsLeft = 0;
   m_numEnemiesLeft = 0;
   m_oldButtons = pev->button;
   m_rechoiceGoalCount = 0;

   m_avoid = nullptr;
   m_avoidTime = 0.0f;

   for (i = 0; i < 5; i++) {
      m_prevWptIndex[i] = INVALID_WAYPOINT_INDEX;
   }
   m_navTimeset = engine.timebase ();
   m_team = engine.getTeam (ent ());
   m_isVIP = false;

   switch (m_personality) {
   case PERSONALITY_NORMAL:
      m_pathType = rng.getInt (0, 100) > 50 ? SEARCH_PATH_SAFEST_FASTER : SEARCH_PATH_SAFEST;
      break;

   case PERSONALITY_RUSHER:
      m_pathType = SEARCH_PATH_FASTEST;
      break;

   case PERSONALITY_CAREFUL:
      m_pathType = SEARCH_PATH_SAFEST;
      break;
   }

   // clear all states & tasks
   m_states = 0;
   clearTasks ();

   m_isVIP = false;
   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;
   m_turnAwayFromFlashbang = 0.0f;

   m_timeTeamOrder = 0.0f;
   m_timeRepotingInDelay = rng.getFloat (40.0f, 240.0f);
   m_askCheckTime = 0.0f;
   m_minSpeed = 260.0f;
   m_prevSpeed = 0.0f;
   m_prevOrigin = Vector (9999.0f, 9999.0f, 9999.0f);
   m_prevTime = engine.timebase ();
   m_lookUpdateTime = engine.timebase ();
   m_aimErrorTime = engine.timebase ();

   m_viewDistance = 4096.0f;
   m_maxViewDistance = 4096.0f;

   m_liftEntity = nullptr;
   m_pickupItem = nullptr;
   m_itemIgnore = nullptr;
   m_itemCheckTime = 0.0f;

   m_breakableEntity = nullptr;
   m_breakableOrigin.nullify ();
   m_timeDoorOpen = 0.0f;

   resetCollision ();
   resetDoubleJump ();

   m_enemy = nullptr;
   m_lastVictim = nullptr;
   m_lastEnemy = nullptr;
   m_lastEnemyOrigin.nullify ();
   m_trackingEdict = nullptr;
   m_timeNextTracking = 0.0f;

   m_buttonPushTime = 0.0f;
   m_enemyUpdateTime = 0.0f;
   m_enemyIgnoreTimer = 0.0f;
   m_retreatTime = 0.0f;
   m_seeEnemyTime = 0.0f;
   m_shootAtDeadTime = 0.0f;
   m_oldCombatDesire = 0.0f;
   m_liftUsageTime = 0.0f;

   m_avoidGrenade = nullptr;
   m_needAvoidGrenade = 0;

   m_lastDamageType = -1;
   m_voteKickIndex = 0;
   m_lastVoteKick = 0;
   m_voteMap = 0;
   m_tryOpenDoor = 0;
   m_aimFlags = 0;
   m_liftState = 0;

   m_aimLastError.nullify ();
   m_position.nullify ();
   m_liftTravelPos.nullify ();

   setIdealReactionTimers (true);

   m_targetEntity = nullptr;
   m_tasks.reserve (TASK_MAX);
   m_followWaitTime = 0.0f;

   m_hostages.clear ();

   for (i = 0; i < CHATTER_MAX; i++) {
      m_chatterTimes[i] = -1.0f;
   }
   m_isReloading = false;
   m_reloadState = RELOAD_NONE;

   m_reloadCheckTime = 0.0f;
   m_shootTime = engine.timebase ();
   m_playerTargetTime = engine.timebase ();
   m_firePause = 0.0f;
   m_timeLastFired = 0.0f;

   m_sniperStopTime = 0.0f;
   m_grenadeCheckTime = 0.0f;
   m_isUsingGrenade = false;
   m_bombSearchOverridden = false;

   m_blindButton = 0;
   m_blindTime = 0.0f;
   m_jumpTime = 0.0f;
   m_jumpStateTimer = 0.0f;
   m_jumpFinished = false;
   m_isStuck = false;

   m_sayTextBuffer.timeNextChat = engine.timebase ();
   m_sayTextBuffer.entityIndex = -1;
   m_sayTextBuffer.sayText.clear ();

   m_buyState = BUYSTATE_PRIMARY_WEAPON;
   m_lastEquipTime = 0.0f;

   // if bot died, clear all weapon stuff and force buying again
   if (!m_notKilled) {
      memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
      memset (&m_ammo, 0, sizeof (m_ammo));

      m_currentWeapon = 0;
   }

   m_knifeAttackTime = engine.timebase () + rng.getFloat (1.3f, 2.6f);
   m_nextBuyTime = engine.timebase () + rng.getFloat (0.6f, 2.0f);

   m_buyPending = false;
   m_inBombZone = false;
   m_ignoreBuyDelay = false;
   m_hasC4 = false;

   m_shieldCheckTime = 0.0f;
   m_zoomCheckTime = 0.0f;
   m_strafeSetTime = 0.0f;
   m_combatStrafeDir = STRAFE_DIR_NONE;
   m_fightStyle = FIGHT_NONE;
   m_lastFightStyleCheck = 0.0f;

   m_checkWeaponSwitch = true;
   m_checkKnifeSwitch = true;
   m_buyingFinished = false;

   m_radioEntity = nullptr;
   m_radioOrder = 0;
   m_defendedBomb = false;
   m_defendHostage = false;
   m_headedTime = 0.0f;

   m_timeLogoSpray = engine.timebase () + rng.getFloat (5.0f, 30.0f);
   m_spawnTime = engine.timebase ();
   m_lastChatTime = engine.timebase ();

   m_timeCamping = 0.0f;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = engine.timebase ();

   // clear its message queue
   for (i = 0; i < 32; i++) {
      m_messageQueue[i] = GAME_MSG_NONE;
   }
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   pushMsgQueue (GAME_MSG_PURCHASE);
   startTask (TASK_NORMAL, TASKPRI_NORMAL, INVALID_WAYPOINT_INDEX, 0.0f, true);

   if (rng.getInt (0, 100) < 50) {
      pushChatterMessage (CHATTER_NEW_ROUND);
   }
   m_thinkInterval = (g_gameFlags & (GAME_LEGACY | GAME_XASH_ENGINE)) ? 0.0f : (1.0f / cr::clamp (yb_think_fps.flt (), 30.0f, 90.0f)) * rng.getFloat (0.95f, 1.05f);
}

void Bot::kill (void) {
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   bots.touchKillerEntity (this);
}

void Bot::kick (void) {
   // this function kick off one bot from the server.
   auto username = STRING (pev->netname);

   if (!(pev->flags & FL_FAKECLIENT) || isEmptyStr (username)) {
      return;
   }
   // clear fakeclient bit
   pev->flags &= ~FL_FAKECLIENT;

   engine.execCmd ("kick \"%s\"", username);
   engine.centerPrint ("Bot '%s' kicked", username);
}

void Bot::processTeamJoin (void) {
   // this function handles the selection of teams & class

   // cs prior beta 7.0 uses hud-based motd, so press fire once
   if (g_gameFlags & GAME_LEGACY) {
      pev->button |= IN_ATTACK;
   }

   // check if something has assigned team to us
   else if (m_team == TEAM_TERRORIST || m_team == TEAM_COUNTER) {
      m_notStarted = false;
   }
   else if (m_team == TEAM_UNASSIGNED && m_retryJoin > 2) {
      m_startAction = GAME_MSG_TEAM_SELECT;
   }

   // if bot was unable to join team, and no menus popups, check for stacked team
   if (m_startAction == GAME_MSG_NONE && ++m_retryJoin > 3) {
      if (bots.isTeamStacked (m_wantedTeam - 1)) {
         m_retryJoin = 0;

         engine.print ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).");
         kick ();

         return;
      }
   }

   // handle counter-strike stuff here...
   if (m_startAction == GAME_MSG_TEAM_SELECT) {
      m_startAction = GAME_MSG_NONE; // switch back to idle

      char teamJoin = yb_join_team.str ()[0];

      if (teamJoin == 'C' || teamJoin == 'c') {
         m_wantedTeam = 2;
      }
      else if (teamJoin == 'T' || teamJoin == 't') {
         m_wantedTeam = 1;
      }

      if (m_wantedTeam != 1 && m_wantedTeam != 2) {
         m_wantedTeam = 5;
      }

      // select the team the bot wishes to join...
      engine.execBotCmd (ent (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == GAME_MSG_CLASS_SELECT) {
      m_startAction = GAME_MSG_NONE; // switch back to idle

      // czero has additional models
      int maxChoice = (g_gameFlags & GAME_CZERO) ? 5 : 4;

      if (m_wantedClass < 1 || m_wantedClass > maxChoice) {
         m_wantedClass = rng.getInt (1, maxChoice); // use random if invalid
      }

      // select the class the bot wishes to use...
      engine.execBotCmd (ent (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (rng.getInt (0, 100) < 20) {
         pushChatMessage (CHAT_WELCOME);
      }
   }
}

void BotManager::calculatePingOffsets (void) {
   if (!(g_gameFlags & GAME_SUPPORT_SVC_PINGS) || yb_latency_display.integer () != 2) {
      return;
   }
   int averagePing = 0;
   int numHumans = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      edict_t *ent = engine.entityOfIndex (i + 1);

      if (!isPlayer (ent)) {
         continue;
      }
      numHumans++;

      int ping, loss;
      g_engfuncs.pfnGetPlayerStats (ent, &ping, &loss);

      if (ping < 0 || ping > 100) {
         ping = rng.getInt (3, 15);
      }
      averagePing += ping;
   }

   if (numHumans > 0) {
      averagePing /= numHumans;
   }
   else {
      averagePing = rng.getInt (30, 40);
   }

   for (int i = 0; i < engine.maxClients (); i++) {
      Bot *bot = getBot (i);

      if (bot == nullptr) {
         continue;
      }

      int part = static_cast <int> (averagePing * 0.2f);
      int botPing = rng.getInt (averagePing - part, averagePing + part) + rng.getInt (bot->m_difficulty + 3, bot->m_difficulty + 6) + 10;

      if (botPing <= 5) {
         botPing = rng.getInt (10, 23);
      }
      else if (botPing > 100) {
         botPing = rng.getInt (30, 40);
      }

      for (int j = 0; j < 2; j++) {
         for (bot->m_pingOffset[j] = 0; bot->m_pingOffset[j] < 4; bot->m_pingOffset[j]++) {
            if ((botPing - bot->m_pingOffset[j]) % 4 == 0) {
               bot->m_ping[j] = (botPing - bot->m_pingOffset[j]) / 4;
               break;
            }
         }
      }
      bot->m_ping[2] = botPing;
   }
}

void BotManager::sendPingOffsets (edict_t *to) {
   if (!(g_gameFlags & GAME_SUPPORT_SVC_PINGS) || yb_latency_display.integer () != 2 || engine.isNullEntity (to) || (to->v.flags & FL_FAKECLIENT)) {
      return;
   }

   if (!(to->v.flags & FL_CLIENT) && !(((to->v.button & IN_SCORE) || !(to->v.oldbuttons & IN_SCORE)))) {
      return;
   }
   MessageWriter msg;

   // missing from sdk
   constexpr int SVC_PINGS = 17;

   for (int i = 0; i < engine.maxClients (); i++) {
      Bot *bot = m_bots[i];

      if (bot == nullptr) {
         continue;
      }
      msg.start (MSG_ONE_UNRELIABLE, SVC_PINGS, Vector::null (), to)
         .writeByte (bot->m_pingOffset[0] * 64 + (1 + 2 * i))
         .writeShort (bot->m_ping[0])
         .writeByte (bot->m_pingOffset[1] * 128 + (2 + 4 * i))
         .writeShort (bot->m_ping[1])
         .writeByte (4 + 8 * i)
         .writeShort (bot->m_ping[2])
         .writeByte (0)
         .end ();
   }
}

void BotManager::sendDeathMsgFix (void) {
   if (yb_latency_display.integer () == 2 && m_deathMsgSent) {
      m_deathMsgSent = false;

      for (int i = 0; i < engine.maxClients (); i++) {
         sendPingOffsets (g_clients[i].ent);
      }
   }
}

void BotManager::updateActiveGrenade (void) {
   if (m_grenadeUpdateTime > engine.timebase ()) {
      return;
   }
   edict_t *grenade = nullptr;

   // clear previously stored grenades
   m_activeGrenades.clear ();

   // search the map for any type of grenade
   while (!engine.isNullEntity (grenade = g_engfuncs.pfnFindEntityByString (grenade, "classname", "grenade"))) {
      // do not count c4 as a grenade
      if (strcmp (STRING (grenade->v.model) + 9, "c4.mdl") == 0) {
         continue;
      }
      m_activeGrenades.push (grenade);
   }
   m_grenadeUpdateTime = engine.timebase () + 0.213f;
}

void BotManager::updateIntrestingEntities (void) {
   if (m_entityUpdateTime > engine.timebase ()) {
      return;
   }

   // clear previously stored entities
   m_intrestingEntities.clear ();

   // search the map for entities
   for (int i = MAX_ENGINE_PLAYERS - 1; i < g_pGlobals->maxEntities; i++) {
      auto ent = engine.entityOfIndex (i);

      // only valid drawn entities
      if (engine.isNullEntity (ent) || ent->free || ent->v.classname == 0 || (ent->v.effects & EF_NODRAW)) {
         continue;
      }
      auto classname = STRING (ent->v.classname);

      // search for grenades, weaponboxes, weapons, items and armoury entities
      if (strncmp ("weapon", classname, 6) == 0 || strncmp ("grenade", classname, 7) == 0 || strncmp ("item", classname, 4) == 0 || strncmp ("armoury", classname, 7) == 0) {
         m_intrestingEntities.push (ent);
      }

      // pickup some csdm stuff if we're running csdm
      if ((g_mapFlags & MAP_CS) && strncmp ("hostage", classname, 7) == 0) {
         m_intrestingEntities.push (ent);
      }
      
      // pickup some csdm stuff if we're running csdm
      if ((g_gameFlags & GAME_CSDM) && strncmp ("csdm", classname, 4) == 0) {
         m_intrestingEntities.push (ent);
      }
   }
   m_entityUpdateTime = engine.timebase () + 0.5f;
}

void BotManager::selectLeaders (int team, bool reset) {
   if (reset) {
      m_leaderChoosen[team] = false;
      return;
   }

   if (m_leaderChoosen[team]) {
      return;
   }

   if (g_mapFlags & MAP_AS) {
      if (team == TEAM_COUNTER && !m_leaderChoosen[TEAM_COUNTER]) {
         for (int i = 0; i < engine.maxClients (); i++) {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_isVIP) {
               // vip bot is the leader
               bot->m_isLeader = true;

               if (rng.getInt (1, 100) < 50) {
                  bot->pushRadioMessage (RADIO_FOLLOW_ME);
                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[TEAM_COUNTER] = true;
      }
      else if (team == TEAM_TERRORIST && !m_leaderChoosen[TEAM_TERRORIST]) {
         auto bot = bots.getHighfragBot (team);

         if (bot != nullptr && bot->m_notKilled) {
            bot->m_isLeader = true;

            if (rng.getInt (1, 100) < 45) {
               bot->pushRadioMessage (RADIO_FOLLOW_ME);
            }
         }
         m_leaderChoosen[TEAM_TERRORIST] = true;
      }
   }
   else if (g_mapFlags & MAP_DE) {
      if (team == TEAM_TERRORIST && !m_leaderChoosen[TEAM_TERRORIST]) {
         for (int i = 0; i < engine.maxClients (); i++) {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_hasC4) {
               // bot carrying the bomb is the leader
               bot->m_isLeader = true;

               // terrorist carrying a bomb needs to have some company
               if (rng.getInt (1, 100) < 80) {
                  if (yb_communication_type.integer () == 2) {
                     bot->pushChatterMessage (CHATTER_GOING_TO_PLANT_BOMB);
                  }
                  else {
                     bot->pushChatterMessage (RADIO_FOLLOW_ME);
                  }
                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[TEAM_TERRORIST] = true;
      }
      else if (!m_leaderChoosen[TEAM_COUNTER]) {
         if (auto bot = bots.getHighfragBot (team)) {
            bot->m_isLeader = true;

            if (rng.getInt (1, 100) < 30) {
               bot->pushRadioMessage (RADIO_FOLLOW_ME);
            }
         }
         m_leaderChoosen[TEAM_COUNTER] = true;
      }
   }
   else if (g_mapFlags & (MAP_ES | MAP_KA | MAP_FY)) {
      auto bot = bots.getHighfragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rng.getInt (1, 100) < 30) {
            bot->pushRadioMessage (RADIO_FOLLOW_ME);
         }
         m_leaderChoosen[team] = true;
      }
   }
   else {
      auto bot = bots.getHighfragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rng.getInt (1, 100) < (team == TEAM_TERRORIST ? 30 : 40)) {
            bot->pushRadioMessage (RADIO_FOLLOW_ME);
         }
         m_leaderChoosen[team] = true;
      }
   }
}
