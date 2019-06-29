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
ConVar yb_join_delay ("yb_join_delay", "5.0");

ConVar yb_name_prefix ("yb_name_prefix", "");
ConVar yb_difficulty ("yb_difficulty", "4");

ConVar yb_latency_display ("yb_latency_display", "2");
ConVar yb_avatar_display ("yb_avatar_display", "1");

ConVar yb_language ("yb_language", "en");
ConVar yb_ignore_cvars_on_changelevel ("yb_ignore_cvars_on_changelevel", "yb_quota,yb_autovacate");

ConVar mp_limitteams ("mp_limitteams", nullptr, VT_NOREGISTER);
ConVar mp_autoteambalance ("mp_autoteambalance", nullptr, VT_NOREGISTER);
ConVar mp_roundtime ("mp_roundtime", nullptr, VT_NOREGISTER);
ConVar mp_timelimit ("mp_timelimit", nullptr, VT_NOREGISTER);
ConVar mp_freezetime ("mp_freezetime", nullptr, VT_NOREGISTER, true, "0");

BotManager::BotManager (void) {
   // this is a bot manager class constructor

   m_lastDifficulty = 0;
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
   m_filters.reserve (TASK_MAX);

   initFilters ();
}

BotManager::~BotManager (void) {
   // this is a bot manager class destructor, do not use game.MaxClients () here !!
   destroy ();
}

void BotManager::createKillerEntity (void) {
   // this function creates single trigger_hurt for using in Bot::Kill, to reduce lags, when killing all the bots

   m_killerEntity = engfuncs.pfnCreateNamedEntity (MAKE_STRING ("trigger_hurt"));

   m_killerEntity->v.dmg = 9999.0f;
   m_killerEntity->v.dmg_take = 1.0f;
   m_killerEntity->v.dmgtime = 2.0f;
   m_killerEntity->v.effects |= EF_NODRAW;

   engfuncs.pfnSetOrigin (m_killerEntity, Vector (-99999.0f, -99999.0f, -99999.0f));
   MDLL_Spawn (m_killerEntity);
}

void BotManager::destroyKillerEntity (void) {
   if (!game.isNullEntity (m_killerEntity)) {
      engfuncs.pfnRemoveEntity (m_killerEntity);
   }
}

void BotManager::touchKillerEntity (Bot *bot) {

   // bot is already dead.
   if (!bot->m_notKilled) {
      return;
   }

   if (game.isNullEntity (m_killerEntity)) {
      createKillerEntity ();

      if (game.isNullEntity (m_killerEntity)) {
         MDLL_ClientKill (bot->ent ());
         return;
      }
   }
   const auto &prop = conf.getWeaponProp (bot->m_currentWeapon);

   m_killerEntity->v.classname = MAKE_STRING (prop.classname);
   m_killerEntity->v.dmg_inflictor = bot->ent ();

   KeyValueData kv;
   kv.szClassName = const_cast <char *> (prop.classname);
   kv.szKeyName = "damagetype";
   kv.szValue = const_cast <char *> (util.format ("%d", cr::bit (4)));
   kv.fHandled = FALSE;

   MDLL_KeyValue (m_killerEntity, &kv);
   MDLL_Touch (m_killerEntity, bot->ent ());
}

// it's already defined in interface.cpp
extern "C" void player (entvars_t *pev);

void BotManager::execGameEntity (entvars_t *vars) {
   // this function calls gamedll player() function, in case to create player entity in game

   if (game.is (GAME_METAMOD)) {
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
      game.centerPrint ("Map is not waypointed. Cannot create bot");
      return BOT_RESULT_NAV_ERROR;
   }

   // don't allow creating bots with changed waypoints (distance tables are messed up)
   else if (waypoints.hasChanged ())  {
      game.centerPrint ("Waypoints have been changed. Load waypoints again...");
      return BOT_RESULT_NAV_ERROR;
   }
   else if (team != -1 && isTeamStacked (team - 1)) {
      game.centerPrint ("Desired team is stacked. Unable to proceed with bot creation");
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
      if (rng.chance (5)) {
         personality = PERSONALITY_NORMAL;
      }
      else {
         if (rng.chance (50)) {
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
      botName = conf.pickBotName ();

      if (botName) {
         resultName = botName->name;
         steamId = botName->steamId;
      }
      else {
         resultName.format ("yapb_%d.%d", rng.getInt (0, 10), rng.getInt (0, 10)); // just pick ugly random name
      }
   }
   else {
      resultName = name;
   }

   if (!util.isEmptyStr (yb_name_prefix.str ())) {
      String prefixed; // temp buffer for storing modified name
      prefixed.format ("%s %s", yb_name_prefix.str (), resultName.chars ());

      // buffer has been modified, copy to real name
      resultName = cr::move (prefixed);
   }
   bot = engfuncs.pfnCreateFakeClient (resultName.chars ());

   if (game.isNullEntity (bot)) {
      game.centerPrint ("Maximum players reached (%d/%d). Unable to create Bot.", game.maxClients (), game.maxClients ());
      return BOT_RESULT_MAX_PLAYERS_REACHED;
   }
   int index = game.indexOfEntity (bot) - 1;

   // ensure it free
   destroy (index);

   assert (index >= 0 && index <= MAX_ENGINE_PLAYERS); // check index
   assert (m_bots[index] == nullptr); // check bot slot

   m_bots[index] = new Bot (bot, difficulty, personality, team, member, steamId);

   // assign owner of bot name
   if (botName != nullptr) {
      botName->usedBy = m_bots[index]->index ();
   }
   game.print ("Connecting Bot...");

   return BOT_RESULT_CREATED;
}

int BotManager::index (edict_t *ent) {
   // this function returns index of bot (using own bot array)
   if (game.isNullEntity (ent)) {
      return -1;
   }
   int index = game.indexOfEntity (ent) - 1;

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

   for (int i = 0; i < game.maxClients (); i++) {
      if (result.length () > 4) {
         break;
      }
      if (m_bots[i] != nullptr && util.isAlive (m_bots[i]->ent ())) {
         result.push (i);
      }
   }

   if (!result.empty ()) {
      return m_bots[result.random ()];
   }
   return nullptr;
}

void BotManager::slowFrame (void) {
   // this function calls think () function for all available at call moment bots

   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr) {
         bot->slowFrame ();
      }
   }
}

void BotManager::frame (void) {
   // this function calls periodic SecondThink () function for all available at call moment bots

   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr) {
         bot->frame ();
      }
   }

   // select leader each team somewhere in round start
   if (m_timeRoundStart + 5.0f > game.timebase () && m_timeRoundStart + 10.0f < game.timebase ()) {
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
         game.centerPrint ("Map is not waypointed. Cannot create bot");
      }
      yb_quota.set (0);
      return;
   }

   // bot's creation update
   if (!m_creationTab.empty () && m_maintainTime < game.timebase ()) {
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
         game.print ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round)");

         m_creationTab.clear ();
         yb_quota.set (getBotCount ());
      }
      m_maintainTime = game.timebase () + 0.10f;
   }

   // now keep bot number up to date
   if (m_quotaMaintainTime > game.timebase ()) {
      return;
   }
   
   // not a best place for this, but whatever
   updateBotDifficulties ();

   yb_quota.set (cr::clamp <int> (yb_quota.integer (), 0, game.maxClients ()));

   int totalHumansInGame = getHumansCount ();
   int humanPlayersInGame = getHumansCount (true);

   if (!game.isDedicated () && !totalHumansInGame) {
      return;
   }

   int desiredBotCount = yb_quota.integer ();
   int botsInGame = getBotCount ();

   if (stricmp (yb_quota_mode.str (), "fill") == 0) {
      botsInGame += humanPlayersInGame;
   }
   else if (stricmp (yb_quota_mode.str (), "match") == 0) {
      int detectQuotaMatch = yb_quota_match.integer () == 0 ? yb_quota.integer () : yb_quota_match.integer ();

      desiredBotCount = cr::max <int> (0, detectQuotaMatch * humanPlayersInGame);
   }

   if (yb_join_after_player.boolean () && humanPlayersInGame == 0) {
      desiredBotCount = 0;
   }
   int maxClients = game.maxClients ();

   if (yb_autovacate.boolean ()) {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - (humanPlayersInGame + 1));
   }
   else {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - humanPlayersInGame);
   }
   int maxSpawnCount = game.getSpawnCount (TEAM_TERRORIST) + game.getSpawnCount (TEAM_COUNTER);

   // add bots if necessary
   if (desiredBotCount > botsInGame && botsInGame < maxSpawnCount) {
      createRandom ();
   }
   else if (desiredBotCount < botsInGame) {
      int ts = 0, cts = 0;
      countTeamPlayers (ts, cts);

      bool isKicked = false;

      if (ts > cts) {
         isKicked = kickRandom (false, TEAM_TERRORIST);
      }
      else if (ts < cts) {
         isKicked = kickRandom (false, TEAM_COUNTER);
      }
      else {
         isKicked = kickRandom (false, TEAM_UNASSIGNED);
      }

      // if we can't kick player from correct team, just kick any random to keep quota control work
      if (!isKicked) {
         kickRandom (false, TEAM_UNASSIGNED);
      }
   }
   m_quotaMaintainTime = game.timebase () + 0.40f;
}

void BotManager::reset (void) {
   m_maintainTime = 0.0f;
   m_quotaMaintainTime = 0.0f;
   m_grenadeUpdateTime = 0.0f;
   m_entityUpdateTime = 0.0f;

   m_intrestingEntities.clear ();
   m_activeGrenades.clear ();
}

void BotManager::initFilters (void) {
   // table with all available actions for the bots (filtered in & out in bot::setconditions) some of them have subactions included

   m_filters.push ({ TASK_NORMAL, 0, INVALID_WAYPOINT_INDEX, 0.0f, true });
   m_filters.push ({ TASK_PAUSE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_MOVETOPOSITION, 0, INVALID_WAYPOINT_INDEX, 0.0f, true });
   m_filters.push ({ TASK_FOLLOWUSER, 0, INVALID_WAYPOINT_INDEX, 0.0f, true });
   m_filters.push ({ TASK_PICKUPITEM, 0, INVALID_WAYPOINT_INDEX, 0.0f, true });
   m_filters.push ({ TASK_CAMP, 0, INVALID_WAYPOINT_INDEX, 0.0f, true });
   m_filters.push ({ TASK_PLANTBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_DEFUSEBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_ATTACK, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_HUNTENEMY, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_SEEKCOVER, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_THROWHEGRENADE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_THROWFLASHBANG, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_THROWSMOKE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_DOUBLEJUMP, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_ESCAPEFROMBOMB, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_SHOOTBREAKABLE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_HIDE, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_BLINDED, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
   m_filters.push ({ TASK_SPRAY, 0, INVALID_WAYPOINT_INDEX, 0.0f, false });
}

void BotManager::resetFilters (void) {
   for (auto &task : m_filters) {
      task.time = 0.0f;
   }
}

void BotManager::decrementQuota (int by) {
   if (by != 0) {
      yb_quota.set (cr::clamp <int> (yb_quota.integer () - by, 0, yb_quota.integer ()));
      return;
   }
   yb_quota.set (0);
}

void BotManager::initQuota (void) {
   m_maintainTime = game.timebase () + yb_join_delay.flt ();
   m_quotaMaintainTime = game.timebase () + yb_join_delay.flt ();

   m_creationTab.clear ();
}

void BotManager::serverFill (int selection, int personality, int difficulty, int numToAdd) {
   // this function fill server with bots, with specified team & personality

   // always keep one slot
   int maxClients = yb_autovacate.boolean () ? game.maxClients () - 1 - (game.isDedicated () ? 0 : getHumansCount ()) : game.maxClients ();

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
   game.centerPrint ("Fill Server with %s bots...", &teams[selection][0]);
}

void BotManager::kickEveryone (bool instant, bool zeroQuota) {
   // this function drops all bot clients from server (this function removes only yapb's)`q

   game.centerPrint ("Bots are removed from server.");

   if (zeroQuota) {
      decrementQuota (0);
   }

   if (instant) {
      for (int i = 0; i < game.maxClients (); i++) {
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

   for (int i = 0; i < game.maxClients (); i++) {
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

void BotManager::killAllBots (int team) {
   // this function kills all bots on server (only this dll controlled bots)

   for (int i = 0; i < game.maxClients (); i++) {
      if (m_bots[i] != nullptr) {
         if (team != -1 && team != m_bots[i]->m_team) {
            continue;
         }
         m_bots[i]->kill ();
      }
   }
   game.centerPrint ("All Bots died !");
}

void BotManager::kickBot (int index) {
   auto bot = getBot (index);

   if (bot) {
      decrementQuota ();
      bot->kick ();
   }
}

bool BotManager::kickRandom (bool decQuota, Team fromTeam) {
   // this function removes random bot from server (only yapb's)

   // if forTeam is unassigned, that means random team
   bool deadBotFound = false;

   auto updateQuota = [&] (void) {
      if (decQuota) {
         decrementQuota ();
      }
   };

   auto belongsTeam = [&] (Bot *bot) {
      if (fromTeam == TEAM_UNASSIGNED) {
         return true;
      }
      return bot->m_team == fromTeam;
   };

   // first try to kick the bot that is currently dead
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot && !bot->m_notKilled && belongsTeam (bot)) // is this slot used?
      {
         updateQuota ();
         bot->kick ();

         deadBotFound = true;
         break;
      }
   }

   if (deadBotFound) {
      return true;
   }

   // if no dead bots found try to find one with lowest amount of frags
   int index = 0;
   float score = 9999.0f;

   // search bots in this team
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots [i];

      if (bot && bot->pev->frags < score && belongsTeam (bot)) {
         index = i;
         score = bot->pev->frags;
      }
   }

   // if found some bots
   if (index != 0) {
      updateQuota ();
      m_bots[index]->kick ();

      return true;
   }

   // worst case, just kick some random bot
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot && belongsTeam (bot)) // is this slot used?
      {
         updateQuota ();
         bot->kick ();

         return true;
      }
   }
   return false;
}

void BotManager::setWeaponMode (int selection) {
   // this function sets bots weapon mode

   selection--;

   constexpr int std[7][NUM_WEAPONS] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Knife only
      {-1, -1, -1, 2, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Pistols only
      {-1, -1, -1, -1, -1, -1, -1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Shotgun only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 1, 2, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1}, // Machine Guns only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 0, 1, 1, -1, -1, -1, -1, -1, -1}, // Rifles only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 2, 0, 1, -1, -1}, // Snipers only
      {-1, -1, -1, 2, 2, 0, 1, 2, 2, 2, 1, 2, 0, 2, 0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 2, 1} // Standard
   };

   constexpr int as[7][NUM_WEAPONS] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Knife only
      {-1, -1, -1, 2, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Pistols only
      {-1, -1, -1, -1, -1, -1, -1, 1, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Shotgun only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, 1, 1, 1, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 1, -1}, // Machine Guns only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, -1, 1, 0, 1, 1, -1, -1, -1, -1, -1, -1}, // Rifles only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, -1, 1, -1, -1}, // Snipers only
      {-1, -1, -1, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0, -1, 1, 0, 1, 1, 0, 0, -1, 1, 1, 1} // Standard
   };
   constexpr char modes[7][12] = {{"Knife"}, {"Pistol"}, {"Shotgun"}, {"Machine Gun"}, {"Rifle"}, {"Sniper"}, {"Standard"}};
   
   // get the raw weapons array
   auto tab = conf.getRawWeapons ();

   // set the correct weapon mode
   for (int i = 0; i < NUM_WEAPONS; i++) {
      tab[i].teamStandard = std[selection][i];
      tab[i].teamAS = as[selection][i];
   }
   yb_jasonmode.set (selection == 0 ? 1 : 0);

   game.centerPrint ("%s weapon mode selected", &modes[selection][0]);
}

void BotManager::listBots (void) {
   // this function list's bots currently playing on the server

   game.print ("%-3.5s %-9.13s %-17.18s %-3.4s %-3.4s %-3.4s", "index", "name", "personality", "team", "difficulty", "frags");

   for (int i = 0; i < game.maxClients (); i++) {
      Bot *bot = getBot (i);

      // is this player slot valid
      if (bot != nullptr) {
         game.print ("[%-3.1d] %-9.13s %-17.18s %-3.4s %-3.1d %-3.1d", i, STRING (bot->pev->netname), bot->m_personality == PERSONALITY_RUSHER ? "rusher" : bot->m_personality == PERSONALITY_NORMAL ? "normal" : "careful", bot->m_team == TEAM_COUNTER ? "CT" : "T", bot->m_difficulty, static_cast <int> (bot->pev->frags));
      }
   }
}

int BotManager::getBotCount (void) {
   // this function returns number of yapb's playing on the server

   int count = 0;

   for (int i = 0; i < game.maxClients (); i++) {
      if (m_bots[i] != nullptr) {
         count++;
      }
   }
   return count;
}

void BotManager::countTeamPlayers (int &ts, int &cts) {
   for (const auto &client : util.getClients ()) {
      if (client.flags & CF_USED) {
         if (client.team2 == TEAM_TERRORIST) {
            ts++;
         }
         else if (client.team2 == TEAM_COUNTER) {
            cts++;
         }
      }
   }
}

Bot *BotManager::getHighfragBot (int team) {
   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (int i = 0; i < game.maxClients (); i++) {
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

void BotManager::updateTeamEconomics (int team, bool setTrue) {
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar yb_economics_rounds;

   if (setTrue || !yb_economics_rounds.boolean ()) {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }
   const int *econLimit = conf.getEconLimit ();

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

      if (bot != nullptr && bot->m_team == team) {
         if (bot->m_moneyAmount <= econLimit[ECO_PRIMARY_GT]) {
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

void BotManager::updateBotDifficulties (void) {
   int difficulty = yb_difficulty.integer ();

   if (difficulty != m_lastDifficulty) {

      // sets new difficulty for all bots
      for (int i = 0; i < game.maxClients (); i++) {
         auto bot = m_bots[i];

         if (bot != nullptr) {
            bot->m_difficulty = difficulty;
         }
      }
      m_lastDifficulty = difficulty;
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

   int clientIndex = game.indexOfEntity (bot);

   memset (reinterpret_cast <void *> (this), 0, sizeof (*this));
   pev = &bot->v;

   if (bot->pvPrivateData != nullptr) {
      engfuncs.pfnFreeEntPrivateData (bot);
   }

   bot->pvPrivateData = nullptr;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   BotManager::execGameEntity (&bot->v);

   // set all info buffer keys for this bot
   char *buffer = engfuncs.pfnGetInfoKeyBuffer (bot);
   engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "_vgui_menus", "0");

   if (!(game.is (GAME_LEGACY)) && yb_latency_display.integer () == 1) {
      engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*bot", "1");
   }

   char reject[256] = {0, };
   MDLL_ClientConnect (bot, STRING (bot->v.netname), util.format ("127.0.0.%d", game.indexOfEntity (bot) + 100), reject);

   if (!util.isEmptyStr (reject)) {
      util.logEntry (true, LL_WARNING, "Server refused '%s' connection (%s)", STRING (bot->v.netname), reject);
      game.execCmd ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
      return;
   }

   // should be set after client connect
   if (yb_avatar_display.boolean () && !steamId.empty ()) {
      engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*sid", steamId.chars ());
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
   m_weaponBurstMode = BURST_OFF;
   m_difficulty = difficulty;

   if (difficulty < 0 || difficulty > 4) {
      difficulty = rng.getInt (3, 4);
      yb_difficulty.set (difficulty);
   }

   m_lastCommandTime = game.timebase () - 0.1f;
   m_frameInterval = game.timebase ();
   m_slowFrameTimestamp = 0.0f;

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
   m_nextEmotionUpdate = game.timebase () + 0.5f;

   // just to be sure
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // assign team and class
   m_wantedTeam = team;
   m_wantedClass = member;

   newRound ();
}

float Bot::calcThinkInterval (void) {
   return cr::fzero (m_thinkInterval) ? m_frameInterval : m_thinkInterval;
}

Bot::~Bot (void) {
   // this is bot destructor

   clearSearchNodes ();
   clearRoute ();
   clearTasks ();

   conf.clearUsedName (this);
}

int BotManager::getHumansCount (bool ignoreSpectators) {
   // this function returns number of humans playing on the server

   int count = 0;

   for (const auto &client : util.getClients ()) {
      if ((client.flags & CF_USED) && getBot (client.ent) == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
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

   for (const auto &client : util.getClients ()) {
      if ((client.flags & (CF_USED | CF_ALIVE)) && getBot (client.ent) == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
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

   for (const auto &client : util.getClients ()) {
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
   m_navTimeset = game.timebase ();
   m_team = game.getTeam (ent ());
   m_isVIP = false;

   switch (m_personality) {
   case PERSONALITY_NORMAL:
      m_pathType = rng.chance (50) ? SEARCH_PATH_SAFEST_FASTER : SEARCH_PATH_SAFEST;
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
   m_prevTime = game.timebase ();
   m_lookUpdateTime = game.timebase ();
   m_aimErrorTime = game.timebase ();

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
   m_shootTime = game.timebase ();
   m_playerTargetTime = game.timebase ();
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

   m_sayTextBuffer.timeNextChat = game.timebase ();
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
   m_flashLevel = 100.0f;
   m_checkDarkTime = game.timebase ();

   m_knifeAttackTime = game.timebase () + rng.getFloat (1.3f, 2.6f);
   m_nextBuyTime = game.timebase () + rng.getFloat (0.6f, 2.0f);

   m_buyPending = false;
   m_inBombZone = false;
   m_ignoreBuyDelay = false;
   m_hasC4 = false;

   m_fallDownTime = 0.0f;
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

   m_timeLogoSpray = game.timebase () + rng.getFloat (5.0f, 30.0f);
   m_spawnTime = game.timebase ();
   m_lastChatTime = game.timebase ();

   m_timeCamping = 0.0f;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = game.timebase ();

   // clear its message queue
   for (i = 0; i < 32; i++) {
      m_messageQueue[i] = GAME_MSG_NONE;
   }
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   pushMsgQueue (GAME_MSG_PURCHASE);
   startTask (TASK_NORMAL, TASKPRI_NORMAL, INVALID_WAYPOINT_INDEX, 0.0f, true);

   if (rng.chance (50)) {
      pushChatterMessage (CHATTER_NEW_ROUND);
   }
   m_thinkInterval = game.is (GAME_LEGACY | GAME_XASH_ENGINE) ? 0.0f : (1.0f / cr::clamp (yb_think_fps.flt (), 30.0f, 90.0f)) * rng.getFloat (0.95f, 1.05f);
}

void Bot::kill (void) {
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   bots.touchKillerEntity (this);
}

void Bot::kick (void) {
   // this function kick off one bot from the server.
   auto username = STRING (pev->netname);

   if (!(pev->flags & FL_FAKECLIENT) || util.isEmptyStr (username)) {
      return;
   }
   // clear fakeclient bit
   pev->flags &= ~FL_FAKECLIENT;

   game.execCmd ("kick \"%s\"", username);
   game.centerPrint ("Bot '%s' kicked", username);
}

void Bot::processTeamJoin (void) {
   // this function handles the selection of teams & class

   // cs prior beta 7.0 uses hud-based motd, so press fire once
   if (game.is (GAME_LEGACY)) {
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

         game.print ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).");
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
      game.execBotCmd (ent (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == GAME_MSG_CLASS_SELECT) {
      m_startAction = GAME_MSG_NONE; // switch back to idle

      // czero has additional models
      int maxChoice = game.is (GAME_CZERO) ? 5 : 4;

      if (m_wantedClass < 1 || m_wantedClass > maxChoice) {
         m_wantedClass = rng.getInt (1, maxChoice); // use random if invalid
      }

      // select the class the bot wishes to use...
      game.execBotCmd (ent (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (rng.chance (20)) {
         pushChatMessage (CHAT_WELCOME);
      }
   }
}

void BotManager::calculatePingOffsets (void) {
   if (!game.is (GAME_SUPPORT_SVC_PINGS) || yb_latency_display.integer () != 2) {
      return;
   }
   int averagePing = 0;
   int numHumans = 0;

   for (int i = 0; i < game.maxClients (); i++) {
      edict_t *ent = game.entityOfIndex (i + 1);

      if (!util.isPlayer (ent)) {
         continue;
      }
      numHumans++;

      int ping, loss;
      engfuncs.pfnGetPlayerStats (ent, &ping, &loss);

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
   
   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = getBot (i);

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

      for (bot->m_pingOffset[0] = 0; bot->m_pingOffset[0] < 4; bot->m_pingOffset[0]++) {
         if ((botPing - bot->m_pingOffset[0]) % 4 == 0) {
            bot->m_ping[0] = (botPing - bot->m_pingOffset[0]) / 4;
            break;
         }
      }

      for (bot->m_pingOffset[1] = 0; bot->m_pingOffset[1] < 2; bot->m_pingOffset[1]++) {
         if ((botPing - bot->m_pingOffset[1]) % 2 == 0) {
            bot->m_ping[1] = (botPing - bot->m_pingOffset[1]) / 2;
            break;
         }
      }
      bot->m_ping[2] = botPing;
   }
}

void BotManager::sendPingOffsets (edict_t *to) {
   if (!game.is (GAME_SUPPORT_SVC_PINGS) || yb_latency_display.integer () != 2 || game.isNullEntity (to) || (to->v.flags & FL_FAKECLIENT)) {
      return;
   }
   if (!(to->v.flags & FL_CLIENT) || !(((to->v.button & IN_SCORE) || (to->v.oldbuttons & IN_SCORE)))) {
      return;
   }
   MessageWriter msg;

   // missing from sdk
   constexpr int SVC_PINGS = 17;

   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = m_bots[i];

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

      for (const auto &client : util.getClients ()) {
         sendPingOffsets (client.ent);
      }
   }
}

void BotManager::captureChatRadio (const char *cmd, const char *arg, edict_t *ent) {
   if (game.isBotCmd ()) {
      return;
   }

   if (stricmp (cmd, "say") == 0 || stricmp (cmd, "say_team") == 0) {
      Bot *bot = nullptr;

      if (strcmp (arg, "dropme") == 0 || strcmp (arg, "dropc4") == 0) {
         if (util.findNearestPlayer (reinterpret_cast <void **> (&bot), ent, 300.0f, true, true, true)) {
            bot->dropWeaponForUser (ent, util.isEmptyStr (strstr (arg, "c4")) ? false : true);
         }
         return;
      }

      bool alive = util.isAlive (ent);
      int team = -1;

      if (strcmp (cmd, "say_team") == 0) {
         team = game.getTeam (ent);
      }

      for (const auto &client : util.getClients ()) {
         if (!(client.flags & CF_USED) || team != client.team || alive != util.isAlive (client.ent)) {
            continue;
         }
         auto target = bots.getBot (client.ent);

         if (target != nullptr) {
            target->m_sayTextBuffer.entityIndex = game.indexOfEntity (ent);

            if (util.isEmptyStr (engfuncs.pfnCmd_Args ())) {
               continue;
            }
            target->m_sayTextBuffer.sayText = engfuncs.pfnCmd_Args ();
            target->m_sayTextBuffer.timeNextChat = game.timebase () + target->m_sayTextBuffer.chatDelay;
         }
      }
   }
   Client &radioTarget = util.getClient (game.indexOfEntity (ent) - 1);

   // check if this player alive, and issue something
   if ((radioTarget.flags & CF_ALIVE) && radioTarget.radio != 0 && strncmp (cmd, "menuselect", 10) == 0) {
      int radioCommand = atoi (arg);

      if (radioCommand != 0) {
         radioCommand += 10 * (radioTarget.radio - 1);

         if (radioCommand != RADIO_AFFIRMATIVE && radioCommand != RADIO_NEGATIVE && radioCommand != RADIO_REPORTING_IN) {
            for (int i = 0; i < game.maxClients (); i++) {
               auto bot = bots.getBot (i);

               // validate bot
               if (bot != nullptr && bot->m_team == radioTarget.team && ent != bot->ent () && bot->m_radioOrder == 0) {
                  bot->m_radioOrder = radioCommand;
                  bot->m_radioEntity = ent;
               }
            }
         }
         bots.setLastRadioTimestamp (radioTarget.team, game.timebase ());
      }
      radioTarget.radio = 0;
   }
   else if (strncmp (cmd, "radio", 5) == 0) {
      radioTarget.radio = atoi (&cmd[5]);
   }
}

void BotManager::notifyBombDefuse (void) {
   // notify all terrorists that CT is starting bomb defusing

   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = bots.getBot (i);

      if (bot && bot->m_team == TEAM_TERRORIST && bot->m_notKilled && bot->taskId () != TASK_MOVETOPOSITION) {
         bot->clearSearchNodes ();

         bot->m_position = waypoints.getBombPos ();
         bot->startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);
      }
   }
}

void BotManager::updateActiveGrenade (void) {
   if (m_grenadeUpdateTime > game.timebase ()) {
      return;
   }
   edict_t *grenade = nullptr;

   // clear previously stored grenades
   m_activeGrenades.clear ();

   // search the map for any type of grenade
   while (!game.isNullEntity (grenade = engfuncs.pfnFindEntityByString (grenade, "classname", "grenade"))) {
      // do not count c4 as a grenade
      if (strcmp (STRING (grenade->v.model) + 9, "c4.mdl") == 0) {
         continue;
      }
      m_activeGrenades.push (grenade);
   }
   m_grenadeUpdateTime = game.timebase () + 0.213f;
}

void BotManager::updateIntrestingEntities (void) {
   if (m_entityUpdateTime > game.timebase ()) {
      return;
   }

   // clear previously stored entities
   m_intrestingEntities.clear ();

   // search the map for entities
   for (int i = MAX_ENGINE_PLAYERS - 1; i < globals->maxEntities; i++) {
      auto ent = game.entityOfIndex (i);

      // only valid drawn entities
      if (game.isNullEntity (ent) || ent->free || ent->v.classname == 0 || (ent->v.effects & EF_NODRAW)) {
         continue;
      }
      auto classname = STRING (ent->v.classname);

      // search for grenades, weaponboxes, weapons, items and armoury entities
      if (strncmp ("weapon", classname, 6) == 0 || strncmp ("grenade", classname, 7) == 0 || strncmp ("item", classname, 4) == 0 || strncmp ("armoury", classname, 7) == 0) {
         m_intrestingEntities.push (ent);
      }

      // pickup some csdm stuff if we're running csdm
      if (game.mapIs (MAP_CS) && strncmp ("hostage", classname, 7) == 0) {
         m_intrestingEntities.push (ent);
      }
      
      // pickup some csdm stuff if we're running csdm
      if (game.is (GAME_CSDM) && strncmp ("csdm", classname, 4) == 0) {
         m_intrestingEntities.push (ent);
      }
   }
   m_entityUpdateTime = game.timebase () + 0.5f;
}

void BotManager::selectLeaders (int team, bool reset) {
   if (reset) {
      m_leaderChoosen[team] = false;
      return;
   }

   if (m_leaderChoosen[team]) {
      return;
   }

   if (game.mapIs (MAP_AS)) {
      if (team == TEAM_COUNTER && !m_leaderChoosen[TEAM_COUNTER]) {
         for (int i = 0; i < game.maxClients (); i++) {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_isVIP) {
               // vip bot is the leader
               bot->m_isLeader = true;

               if (rng.chance (50)) {
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

            if (rng.chance (45)) {
               bot->pushRadioMessage (RADIO_FOLLOW_ME);
            }
         }
         m_leaderChoosen[TEAM_TERRORIST] = true;
      }
   }
   else if (game.mapIs (MAP_DE)) {
      if (team == TEAM_TERRORIST && !m_leaderChoosen[TEAM_TERRORIST]) {
         for (int i = 0; i < game.maxClients (); i++) {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_hasC4) {
               // bot carrying the bomb is the leader
               bot->m_isLeader = true;

               // terrorist carrying a bomb needs to have some company
               if (rng.chance (75)) {
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

            if (rng.chance (30)) {
               bot->pushRadioMessage (RADIO_FOLLOW_ME);
            }
         }
         m_leaderChoosen[TEAM_COUNTER] = true;
      }
   }
   else if (game.mapIs (MAP_ES | MAP_KA | MAP_FY)) {
      auto bot = bots.getHighfragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rng.chance (30)) {
            bot->pushRadioMessage (RADIO_FOLLOW_ME);
         }
         m_leaderChoosen[team] = true;
      }
   }
   else {
      auto bot = bots.getHighfragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rng.chance (team == TEAM_TERRORIST ? 30 : 40)) {
            bot->pushRadioMessage (RADIO_FOLLOW_ME);
         }
         m_leaderChoosen[team] = true;
      }
   }
}

void BotManager::initRound (void) {
   // this is called at the start of each round

   m_roundEnded = false;

   // check team economics
   for (int team = 0; team < MAX_TEAM_COUNT; team++) {
      updateTeamEconomics (team);
      selectLeaders (team, true);

      m_lastRadioTime[team] = 0.0f;
   }
   reset ();

   for (int i = 0; i < game.maxClients (); i++) {
      auto bot = getBot (i);

      if (bot != nullptr) {
         bot->newRound ();
      }
      util.getClient (i).radio = 0;
   }
   waypoints.setBombPos (true);
   waypoints.clearVisited ();

   m_bombSayStatus = 0;
   m_timeBombPlanted = 0.0f;
   m_plantSearchUpdateTime = 0.0f;
   m_botsCanPause = false;

   resetFilters ();
   waypoints.updateGlobalExperience (); // update experience data on round start

   // calculate the round mid/end in world time
   m_timeRoundStart = game.timebase () + mp_freezetime.flt ();
   m_timeRoundMid = m_timeRoundStart + mp_roundtime.flt () * 60.0f * 0.5f;
   m_timeRoundEnd = m_timeRoundStart + mp_roundtime.flt () * 60.0f;
}

void BotManager::setBombPlanted (bool isPlanted) {
   if (isPlanted) {
      m_timeBombPlanted = game.timebase ();
   }
   m_bombPlanted = isPlanted;
}

void Config::load (bool onlyMain) {
   static bool setMemoryPointers = true;

   if (setMemoryPointers) {
      MemoryLoader::ref ().setup (engfuncs.pfnLoadFileForMe, engfuncs.pfnFreeFile);
      setMemoryPointers = true;
   }

   auto isCommentLine = [] (const char *line) {
      char ch = *line;
      return ch == '#' || ch == '/' || ch == '\r' || ch == ';' || ch == 0 || ch == ' ';
   };

   MemFile fp;
   char lineBuffer[512];

   // this is does the same as exec of engine, but not overwriting values of cvars spcified in yb_ignore_cvars_on_changelevel
   if (onlyMain) {
      static bool firstLoad = true;

      auto needsToIgnoreVar = [] (StringArray & list, const char *needle) {
         for (auto &var : list) {
            if (var == needle) {
               return true;
            }
         }
         return false;
      };

      if (util.openConfig ("yapb.cfg", "YaPB main config file is not found.", &fp, false)) {
         while (fp.gets (lineBuffer, 255)) {
            if (isCommentLine (lineBuffer)) {
               continue;
            }
            if (firstLoad) {
               game.execCmd (lineBuffer);
               continue;
            }
            auto keyval = String (lineBuffer).split (" ");

            if (keyval.length () > 1) {
               auto ignore = String (yb_ignore_cvars_on_changelevel.str ()).split (",");

               auto key = keyval[0].trim ().chars ();
               auto cvar = engfuncs.pfnCVarGetPointer (key);

               if (cvar != nullptr) {
                  auto value = const_cast <char *> (keyval[1].trim ().trim ("\"").trim ().chars ());

                  if (needsToIgnoreVar (ignore, key) && !!stricmp (value, cvar->string)) {
                     game.print ("Bot CVAR '%s' differs from the stored in the config (%s/%s). Ignoring.", cvar->name, cvar->string, value);

                     // ensure cvar will have old value
                     engfuncs.pfnCvar_DirectSet (cvar, cvar->string);
                  }
                  else {
                     engfuncs.pfnCvar_DirectSet (cvar, value);
                  }
               }
               else {
                  game.execCmd (lineBuffer);
               }
            }
         }
         fp.close ();
      }
      firstLoad = false;
      return;
   }
   Keywords replies;

   // reserve some space for chat
   m_chat.reserve (CHAT_TOTAL);
   m_chatter.reserve (CHATTER_MAX);
   m_botNames.reserve (CHATTER_MAX);

   // NAMING SYSTEM INITIALIZATION
   if (util.openConfig ("names.cfg", "Name configuration file not found.", &fp, true)) {
      m_botNames.clear ();

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         StringArray pair = String (lineBuffer).split ("\t\t");

         if (pair.length () > 1) {
            strncpy (lineBuffer, pair[0].trim ().chars (), cr::bufsize (lineBuffer));
         }

         String::trimChars (lineBuffer);
         lineBuffer[32] = 0;

         BotName item;
         item.name = lineBuffer;
         item.usedBy = 0;

         if (pair.length () > 1) {
            item.steamId = pair[1].trim ();
         }
         m_botNames.push (cr::move (item));
      }
      fp.close ();
   }

   // CHAT SYSTEM CONFIG INITIALIZATION
   if (util.openConfig ("chat.cfg", "Chat file not found.", &fp, true)) {

      int chatType = -1;

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         String section (lineBuffer, strlen (lineBuffer) - 1);
         section.trim ();

         if (section == "[KILLED]") {
            chatType = 0;
            continue;
         }
         else if (section == "[BOMBPLANT]") {
            chatType = 1;
            continue;
         }
         else if (section == "[DEADCHAT]") {
            chatType = 2;
            continue;
         }
         else if (section == "[REPLIES]") {
            chatType = 3;
            continue;
         }
         else if (section == "[UNKNOWN]") {
            chatType = 4;
            continue;
         }
         else if (section == "[TEAMATTACK]") {
            chatType = 5;
            continue;
         }
         else if (section == "[WELCOME]") {
            chatType = 6;
            continue;
         }
         else if (section == "[TEAMKILL]") {
            chatType = 7;
            continue;
         }

         if (chatType != 3) {
            lineBuffer[79] = 0;
         }

         switch (chatType) {
         case 0:
            m_chat[CHAT_KILLING].push (lineBuffer);
            break;

         case 1:
            m_chat[CHAT_BOMBPLANT].push (lineBuffer);
            break;

         case 2:
            m_chat[CHAT_DEAD].push (lineBuffer);
            break;

         case 3:
            if (strstr (lineBuffer, "@KEY") != nullptr) {
               if (!replies.keywords.empty () && !replies.replies.empty ()) {
                  m_replies.push (cr::forward <Keywords> (replies));
                  replies.replies.clear ();
               }

               replies.keywords.clear ();
               replies.keywords = String (&lineBuffer[4]).split (",");

               for (auto &keywords : replies.keywords) {
                  keywords.trim ().trim ("\"");
               }
            }
            else if (!replies.keywords.empty ()) {
               replies.replies.push (lineBuffer);
            }
            break;

         case 4:
            m_chat[CHAT_NOKW].push (lineBuffer);
            break;

         case 5:
            m_chat[CHAT_TEAMATTACK].push (lineBuffer);
            break;

         case 6:
            m_chat[CHAT_WELCOME].push (lineBuffer);
            break;

         case 7:
            m_chat[CHAT_TEAMKILL].push (lineBuffer);
            break;
         }
      }

      for (auto &item : m_chat) {
         item.shuffle ();
      }
      fp.close ();
   }
   else {
      yb_chat.set (0);
   }

   // GENERAL DATA INITIALIZATION
   if (util.openConfig ("general.cfg", "General configuration file not found. Loading defaults", &fp)) {
      
      auto badSyntax = [] (const String &name) {
         util.logEntry (true, LL_ERROR, "%s entry in general config is not valid or malformed.", name.chars ());
      };

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }
         auto pair = String (lineBuffer).split ("=");

         if (pair.length () != 2) {
            continue;
         }

         for (auto &trim : pair) {
            trim.trim ();
         }
         auto splitted = pair[1].split (",");

         if (pair[0] == "MapStandard") {
            if (splitted.length () != NUM_WEAPONS) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < NUM_WEAPONS; i++) {
                  m_weapons[i].teamStandard = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "MapAS") {
            if (splitted.length () != NUM_WEAPONS) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < NUM_WEAPONS; i++) {
                  m_weapons[i].teamAS = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "GrenadePercent") {
            if (splitted.length () != 3) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < 3; i++) {
                  m_grenadeBuyPrecent[i] = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "Economics") {
            if (splitted.length () != 11) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < 11; i++) {
                  m_botBuyEconomyTable[i] = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "PersonalityNormal") {
            if (splitted.length () != NUM_WEAPONS) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < NUM_WEAPONS; i++) {
                  m_normalWeaponPrefs[i] = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "PersonalityRusher") {
            if (splitted.length () != NUM_WEAPONS) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < NUM_WEAPONS; i++) {
                  m_rusherWeaponPrefs[i] = splitted[i].toInt32 ();
               }
            }
         }
         else if (pair[0] == "PersonalityCareful") {
            if (splitted.length () != NUM_WEAPONS) {
               badSyntax (pair[0]);
            }
            else {
               for (int i = 0; i < NUM_WEAPONS; i++) {
                  m_carefulWeaponPrefs[i] = splitted[i].toInt32 ();
               }
            }
         }
      }
      fp.close ();
   }

   // CHATTER SYSTEM INITIALIZATION
   if (game.is (GAME_SUPPORT_BOT_VOICE) && yb_communication_type.integer () == 2 && util.openConfig ("chatter.cfg", "Couldn't open chatter system configuration", &fp)) {
      struct EventMap {
         const char *str;
         int code;
         float repeat;
      } chatterEventMap[] = {
         { "Radio_CoverMe", RADIO_COVER_ME, MAX_CHATTER_REPEAT },
         { "Radio_YouTakePoint", RADIO_YOU_TAKE_THE_POINT, MAX_CHATTER_REPEAT },
         { "Radio_HoldPosition", RADIO_HOLD_THIS_POSITION, 10.0f },
         { "Radio_RegroupTeam", RADIO_REGROUP_TEAM, 10.0f },
         { "Radio_FollowMe", RADIO_FOLLOW_ME, 15.0f },
         { "Radio_TakingFire", RADIO_TAKING_FIRE, 5.0f },
         { "Radio_GoGoGo", RADIO_GO_GO_GO, MAX_CHATTER_REPEAT },
         { "Radio_Fallback", RADIO_TEAM_FALLBACK, MAX_CHATTER_REPEAT },
         { "Radio_StickTogether", RADIO_STICK_TOGETHER_TEAM, MAX_CHATTER_REPEAT },
         { "Radio_GetInPosition", RADIO_GET_IN_POSITION, MAX_CHATTER_REPEAT },
         { "Radio_StormTheFront", RADIO_STORM_THE_FRONT, MAX_CHATTER_REPEAT },
         { "Radio_ReportTeam", RADIO_REPORT_TEAM, MAX_CHATTER_REPEAT },
         { "Radio_Affirmative", RADIO_AFFIRMATIVE, MAX_CHATTER_REPEAT },
         { "Radio_EnemySpotted", RADIO_ENEMY_SPOTTED, 4.0f },
         { "Radio_NeedBackup", RADIO_NEED_BACKUP, MAX_CHATTER_REPEAT },
         { "Radio_SectorClear", RADIO_SECTOR_CLEAR, 10.0f },
         { "Radio_InPosition", RADIO_IN_POSITION, 10.0f },
         { "Radio_ReportingIn", RADIO_REPORTING_IN, MAX_CHATTER_REPEAT },
         { "Radio_ShesGonnaBlow", RADIO_SHES_GONNA_BLOW, MAX_CHATTER_REPEAT },
         { "Radio_Negative", RADIO_NEGATIVE, MAX_CHATTER_REPEAT },
         { "Radio_EnemyDown", RADIO_ENEMY_DOWN, 10.0f },
         { "Chatter_DiePain", CHATTER_PAIN_DIED, MAX_CHATTER_REPEAT },
         { "Chatter_GoingToPlantBomb", CHATTER_GOING_TO_PLANT_BOMB, 5.0f },
         { "Chatter_GoingToGuardVIPSafety", CHATTER_GOING_TO_GUARD_VIP_SAFETY, MAX_CHATTER_REPEAT },
         { "Chatter_RescuingHostages", CHATTER_RESCUING_HOSTAGES, MAX_CHATTER_REPEAT },
         { "Chatter_TeamKill", CHATTER_TEAM_ATTACK, MAX_CHATTER_REPEAT },
         { "Chatter_GuardingVipSafety", CHATTER_GUARDING_VIP_SAFETY, MAX_CHATTER_REPEAT },
         { "Chatter_PlantingC4", CHATTER_PLANTING_BOMB, 10.0f },
         { "Chatter_InCombat", CHATTER_IN_COMBAT,  MAX_CHATTER_REPEAT },
         { "Chatter_SeeksEnemy", CHATTER_SEEK_ENEMY, MAX_CHATTER_REPEAT },
         { "Chatter_Nothing", CHATTER_NOTHING,  MAX_CHATTER_REPEAT },
         { "Chatter_EnemyDown", CHATTER_ENEMY_DOWN, 10.0f },
         { "Chatter_UseHostage", CHATTER_USING_HOSTAGES, MAX_CHATTER_REPEAT },
         { "Chatter_WonTheRound", CHATTER_WON_THE_ROUND, MAX_CHATTER_REPEAT },
         { "Chatter_QuicklyWonTheRound", CHATTER_QUICK_WON_ROUND, MAX_CHATTER_REPEAT },
         { "Chatter_NoEnemiesLeft", CHATTER_NO_ENEMIES_LEFT, MAX_CHATTER_REPEAT },
         { "Chatter_FoundBombPlace", CHATTER_FOUND_BOMB_PLACE, 15.0f },
         { "Chatter_WhereIsTheBomb", CHATTER_WHERE_IS_THE_BOMB, MAX_CHATTER_REPEAT },
         { "Chatter_DefendingBombSite", CHATTER_DEFENDING_BOMBSITE, MAX_CHATTER_REPEAT },
         { "Chatter_BarelyDefused", CHATTER_BARELY_DEFUSED, MAX_CHATTER_REPEAT },
         { "Chatter_NiceshotCommander", CHATTER_NICESHOT_COMMANDER, MAX_CHATTER_REPEAT },
         { "Chatter_ReportingIn", CHATTER_REPORTING_IN, 10.0f },
         { "Chatter_SpotTheBomber", CHATTER_SPOT_THE_BOMBER, 4.3f },
         { "Chatter_VIPSpotted", CHATTER_VIP_SPOTTED, 5.3f },
         { "Chatter_FriendlyFire", CHATTER_FRIENDLY_FIRE, 2.1f },
         { "Chatter_GotBlinded", CHATTER_BLINDED, 5.0f },
         { "Chatter_GuardDroppedC4", CHATTER_GUARDING_DROPPED_BOMB, 3.0f },
         { "Chatter_DefusingC4", CHATTER_DEFUSING_BOMB, 3.0f },
         { "Chatter_FoundC4", CHATTER_FOUND_BOMB, 5.5f },
         { "Chatter_ScaredEmotion", CHATTER_SCARED_EMOTE, 6.1f },
         { "Chatter_HeardEnemy", CHATTER_SCARED_EMOTE, 12.8f },
         { "Chatter_SniperWarning", CHATTER_SNIPER_WARNING, 14.3f },
         { "Chatter_SniperKilled", CHATTER_SNIPER_KILLED, 12.1f },
         { "Chatter_OneEnemyLeft", CHATTER_ONE_ENEMY_LEFT, 12.5f },
         { "Chatter_TwoEnemiesLeft", CHATTER_TWO_ENEMIES_LEFT, 12.5f },
         { "Chatter_ThreeEnemiesLeft", CHATTER_THREE_ENEMIES_LEFT, 12.5f },
         { "Chatter_NiceshotPall", CHATTER_NICESHOT_PALL, 2.0f },
         { "Chatter_GoingToGuardHostages", CHATTER_GOING_TO_GUARD_HOSTAGES, 3.0f },
         { "Chatter_GoingToGuardDoppedBomb", CHATTER_GOING_TO_GUARD_DROPPED_BOMB, 6.0f },
         { "Chatter_OnMyWay", CHATTER_ON_MY_WAY, 1.5f },
         { "Chatter_LeadOnSir", CHATTER_LEAD_ON_SIR, 5.0f },
         { "Chatter_Pinned_Down", CHATTER_PINNED_DOWN, 5.0f },
         { "Chatter_GottaFindTheBomb", CHATTER_GOTTA_FIND_BOMB, 3.0f },
         { "Chatter_You_Heard_The_Man", CHATTER_YOU_HEARD_THE_MAN, 3.0f },
         { "Chatter_Lost_The_Commander", CHATTER_LOST_COMMANDER, 4.5f },
         { "Chatter_NewRound", CHATTER_NEW_ROUND, 3.5f },
         { "Chatter_CoverMe", CHATTER_COVER_ME, 3.5f },
         { "Chatter_BehindSmoke", CHATTER_BEHIND_SMOKE, 3.5f },
         { "Chatter_BombSiteSecured", CHATTER_BOMB_SITE_SECURED, 3.5f },
         { "Chatter_GoingToCamp", CHATTER_GOING_TO_CAMP, 25.0f },
         { "Chatter_Camp", CHATTER_CAMP, 25.0f },
      };

      while (fp.gets (lineBuffer, 511)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }

         if (strncmp (lineBuffer, "RewritePath", 11) == 0) {
            extern ConVar yb_chatter_path;
            yb_chatter_path.set (String (&lineBuffer[12]).trim ().chars ());
         }
         else if (strncmp (lineBuffer, "Event", 5) == 0) {
            auto items = String (&lineBuffer[6]).split ("=");

            if (items.length () != 2) {
               util.logEntry (true, LL_ERROR, "Error in chatter config file syntax... Please correct all errors.");
               continue;
            }

            for (auto &item : items) {
               item.trim ();
            }
            items[1].trim ("(;)");

            for (const auto &event : chatterEventMap) {
               if (stricmp (event.str, items[0].chars ()) == 0) {
                  // this does common work of parsing comma-separated chatter line
                  auto sounds = items[1].split (",");

                  for (auto &sound : sounds) {
                     sound.trim ().trim ("\"");
                     float duration = game.getWaveLen (sound.chars ());

                     if (duration > 0.0f) {
                        m_chatter[event.code].push ({ sound, event.repeat, duration });
                     }
                  }
                  sounds.clear ();
               }
            }
         }
      }
      fp.close ();
   }
   else {
      yb_communication_type.set (1);
      util.logEntry (true, LL_DEFAULT, "Chatter Communication disabled.");
   }

   // LOCALIZER INITITALIZATION
   if (!(game.is (GAME_LEGACY)) && util.openConfig ("lang.cfg", "Specified language not found", &fp, true)) {
      if (game.isDedicated ()) {
         return; // dedicated server will use only english translation
      }
      enum Lang { LANG_ORIGINAL, LANG_TRANSLATED, LANG_UNDEFINED } langState = static_cast <Lang> (LANG_UNDEFINED);

      char buffer[MAX_PRINT_BUFFER] = { 0, };
      Pair <String, String> lang;

      while (fp.gets (lineBuffer, 255)) {
         if (isCommentLine (lineBuffer)) {
            continue;
         }

         if (strncmp (lineBuffer, "[ORIGINAL]", 10) == 0) {
            langState = LANG_ORIGINAL;

            if (buffer[0] != 0) {
               lang.second = buffer;
               lang.second.trim ();

               buffer[0] = 0;
            }

            if (!lang.second.empty () && !lang.first.empty ()) {
               game.addTranslation (lang.first, lang.second);
            }
         }
         else if (strncmp (lineBuffer, "[TRANSLATED]", 12) == 0) {

            lang.first = buffer;
            lang.first.trim ();

            langState = LANG_TRANSLATED;
            buffer[0] = 0;
         }
         else {
            switch (langState) {
            case LANG_ORIGINAL:
               strncat (buffer, lineBuffer, MAX_PRINT_BUFFER - 1 - strlen (buffer));
               break;

            case LANG_TRANSLATED:
               strncat (buffer, lineBuffer, MAX_PRINT_BUFFER - 1 - strlen (buffer));
               break;

            case LANG_UNDEFINED:
               break;
            }
         }
      }
      fp.close ();
   }
   else if (game.is (GAME_LEGACY)) {
      util.logEntry (true, LL_DEFAULT, "Multilingual system disabled, due to your Counter-Strike Version!");
   }
   else if (strcmp (yb_language.str (), "en") != 0) {
      util.logEntry (true, LL_ERROR, "Couldn't load language configuration");
   }

   // set personality weapon pointers here
   m_weaponPrefs[PERSONALITY_NORMAL] = reinterpret_cast <int *> (&m_normalWeaponPrefs);
   m_weaponPrefs[PERSONALITY_RUSHER] = reinterpret_cast <int *> (&m_rusherWeaponPrefs);
   m_weaponPrefs[PERSONALITY_CAREFUL] = reinterpret_cast <int *> (&m_carefulWeaponPrefs);
}

BotName *Config::pickBotName (void) {
   if (m_botNames.empty ()) {
      return nullptr;
   }

   for (int i = 0; i < MAX_ENGINE_PLAYERS * 4; i++) {
      auto botName = &m_botNames.random ();

      if (botName->name.length () < 3 || botName->usedBy != 0) {
         continue;
      }
      return botName;
   }
   return nullptr;
}

void Config::clearUsedName (Bot *bot) {
   for (auto &name : m_botNames) {
      if (name.usedBy == bot->index ()) {
         name.usedBy = 0;
         break;
      }
   }
}

void Config::initWeapons (void) {
   m_weapons.reserve (NUM_WEAPONS + 1);

   // fill array with available weapons
   m_weapons.push ({ WEAPON_KNIFE,      "weapon_knife",     "knife.mdl",     0,    0, -1, -1,  0,  0,  0,  0,  0,  0,   true  });
   m_weapons.push ({ WEAPON_USP,        "weapon_usp",       "usp.mdl",       500,  1, -1, -1,  1,  1,  2,  2,  0,  12,  false });
   m_weapons.push ({ WEAPON_GLOCK,      "weapon_glock18",   "glock18.mdl",   400,  1, -1, -1,  1,  2,  1,  1,  0,  20,  false });
   m_weapons.push ({ WEAPON_DEAGLE,     "weapon_deagle",    "deagle.mdl",    650,  1,  2,  2,  1,  3,  4,  4,  2,  7,   false });
   m_weapons.push ({ WEAPON_P228,       "weapon_p228",      "p228.mdl",      600,  1,  2,  2,  1,  4,  3,  3,  0,  13,  false });
   m_weapons.push ({ WEAPON_ELITE,      "weapon_elite",     "elite.mdl",     800,  1,  0,  0,  1,  5,  5,  5,  0,  30,  false });
   m_weapons.push ({ WEAPON_FIVESEVEN,  "weapon_fiveseven", "fiveseven.mdl", 750,  1,  1,  1,  1,  6,  5,  5,  0,  20,  false });
   m_weapons.push ({ WEAPON_M3,         "weapon_m3",        "m3.mdl",        1700, 1,  2, -1,  2,  1,  1,  1,  0,  8,   false });
   m_weapons.push ({ WEAPON_XM1014,     "weapon_xm1014",    "xm1014.mdl",    3000, 1,  2, -1,  2,  2,  2,  2,  0,  7,   false });
   m_weapons.push ({ WEAPON_MP5,        "weapon_mp5navy",   "mp5.mdl",       1500, 1,  2,  1,  3,  1,  2,  2,  0,  30,  true  });
   m_weapons.push ({ WEAPON_TMP,        "weapon_tmp",       "tmp.mdl",       1250, 1,  1,  1,  3,  2,  1,  1,  0,  30,  true  });
   m_weapons.push ({ WEAPON_P90,        "weapon_p90",       "p90.mdl",       2350, 1,  2,  1,  3,  3,  4,  4,  0,  50,  true  });
   m_weapons.push ({ WEAPON_MAC10,      "weapon_mac10",     "mac10.mdl",     1400, 1,  0,  0,  3,  4,  1,  1,  0,  30,  true  });
   m_weapons.push ({ WEAPON_UMP45,      "weapon_ump45",     "ump45.mdl",     1700, 1,  2,  2,  3,  5,  3,  3,  0,  25,  true  });
   m_weapons.push ({ WEAPON_AK47,       "weapon_ak47",      "ak47.mdl",      2500, 1,  0,  0,  4,  1,  2,  2,  2,  30,  true  });
   m_weapons.push ({ WEAPON_SG552,      "weapon_sg552",     "sg552.mdl",     3500, 1,  0, -1,  4,  2,  4,  4,  2,  30,  true  });
   m_weapons.push ({ WEAPON_M4A1,       "weapon_m4a1",      "m4a1.mdl",      3100, 1,  1,  1,  4,  3,  3,  3,  2,  30,  true  });
   m_weapons.push ({ WEAPON_GALIL,      "weapon_galil",     "galil.mdl",     2000, 1,  0,  0,  4,  -1, 1,  1,  2,  35,  true  });
   m_weapons.push ({ WEAPON_FAMAS,      "weapon_famas",     "famas.mdl",     2250, 1,  1,  1,  4,  -1, 1,  1,  2,  25,  true  });
   m_weapons.push ({ WEAPON_AUG,        "weapon_aug",       "aug.mdl",       3500, 1,  1,  1,  4,  4,  4,  4,  2,  30,  true  });
   m_weapons.push ({ WEAPON_SCOUT,      "weapon_scout",     "scout.mdl",     2750, 1,  2,  0,  4,  5,  3,  2,  3,  10,  false });
   m_weapons.push ({ WEAPON_AWP,        "weapon_awp",       "awp.mdl",       4750, 1,  2,  0,  4,  6,  5,  6,  3,  10,  false });
   m_weapons.push ({ WEAPON_G3SG1,      "weapon_g3sg1",     "g3sg1.mdl",     5000, 1,  0,  2,  4,  7,  6,  6,  3,  20,  false });
   m_weapons.push ({ WEAPON_SG550,      "weapon_sg550",     "sg550.mdl",     4200, 1,  1,  1,  4,  8,  5,  5,  3,  30,  false });
   m_weapons.push ({ WEAPON_M249,       "weapon_m249",      "m249.mdl",      5750, 1,  2,  1,  5,  1,  1,  1,  2,  100, true  });
   m_weapons.push ({ WEAPON_SHIELD,     "weapon_shield",    "shield.mdl",    2200, 0,  1,  1,  8,  -1, 8,  8,  0,  0,   false });

   // not needed actually, but cause too much refactoring for now. todo
   m_weapons.push ({ 0,                 "",                 "",              0,    0,  0,  0,  0,   0, 0,  0,  0,  0,   false });
}

void Config::adjustWeaponPrices (void) {
   // elite price is 1000$ on older versions of cs...
   if (!(game.is (GAME_LEGACY))) {
      return;
   }

   for (auto &weapon : m_weapons) {
      if (weapon.id == WEAPON_ELITE) {
         weapon.price = 1000;
         break;
      }
   }
}

WeaponInfo &Config::findWeaponById (const int id) {
   for (auto &weapon : m_weapons) {
      if (weapon.id == id) {
         return weapon;
      }
   }
   return m_weapons.at (0);
}
