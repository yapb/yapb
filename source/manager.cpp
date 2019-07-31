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

ConVar yb_quota ("yb_quota", "0", Var::Normal);
ConVar yb_quota_mode ("yb_quota_mode", "normal");
ConVar yb_quota_match ("yb_quota_match", "0");
ConVar yb_think_fps ("yb_think_fps", "30.0");

ConVar yb_join_after_player ("yb_join_after_player", "0");
ConVar yb_join_team ("yb_join_team", "any");
ConVar yb_join_delay ("yb_join_delay", "5.0");

ConVar yb_name_prefix ("yb_name_prefix", "");
ConVar yb_difficulty ("yb_difficulty", "4");

ConVar yb_show_avatars ("yb_show_avatars", "1");
ConVar yb_show_latency ("yb_show_latency", "2");
ConVar yb_language ("yb_language", "en");
ConVar yb_ignore_cvars_on_changelevel ("yb_ignore_cvars_on_changelevel", "yb_quota,yb_autovacate");

ConVar mp_limitteams ("mp_limitteams", nullptr, Var::NoRegister);
ConVar mp_autoteambalance ("mp_autoteambalance", nullptr, Var::NoRegister);
ConVar mp_roundtime ("mp_roundtime", nullptr, Var::NoRegister);
ConVar mp_timelimit ("mp_timelimit", nullptr, Var::NoRegister);
ConVar mp_freezetime ("mp_freezetime", nullptr, Var::NoRegister, true, "0");

BotManager::BotManager () {
   // this is a bot manager class constructor

   m_lastDifficulty = 0;
   m_lastWinner = -1;

   m_timeRoundStart = 0.0f;
   m_timeRoundMid = 0.0f;
   m_timeRoundEnd = 0.0f;

   m_bombPlanted = false;
   m_botsCanPause = false;

   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;

   for (int i = 0; i < kGameTeamNum; ++i) {
      m_leaderChoosen[i] = false;
      m_economicsGood[i] = true;
   }
   reset ();

   m_creationTab.clear ();
   m_killerEntity = nullptr;

   initFilters ();
}

void BotManager::createKillerEntity () {
   // this function creates single trigger_hurt for using in Bot::Kill, to reduce lags, when killing all the bots

   m_killerEntity = engfuncs.pfnCreateNamedEntity (MAKE_STRING ("trigger_hurt"));

   m_killerEntity->v.dmg = 9999.0f;
   m_killerEntity->v.dmg_take = 1.0f;
   m_killerEntity->v.dmgtime = 2.0f;
   m_killerEntity->v.effects |= EF_NODRAW;

   engfuncs.pfnSetOrigin (m_killerEntity, Vector (-99999.0f, -99999.0f, -99999.0f));
   MDLL_Spawn (m_killerEntity);
}

void BotManager::destroyKillerEntity () {
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
   kv.szValue = const_cast <char *> (strings.format ("%d", cr::bit (4)));
   kv.fHandled = FALSE;

   MDLL_KeyValue (m_killerEntity, &kv);
   MDLL_Touch (m_killerEntity, bot->ent ());
}

void BotManager::execGameEntity (entvars_t *vars) {
   // this function calls gamedll player() function, in case to create player entity in game

   if (game.is (GameFlags::Metamod)) {
      CALL_GAME_ENTITY (PLID, "player", vars);
      return;
   }
   ents.getPlayerFunction () (vars);
}

void BotManager::forEach (ForEachBot handler) {
   for (const auto &bot : m_bots) {
      if (handler (bot.get ())) {
         return;
      }
   }
}

BotCreateResult BotManager::create (const String &name, int difficulty, int personality, int team, int member) {
   // this function completely prepares bot entity (edict) for creation, creates team, difficulty, sets named etc, and
   // then sends result to bot constructor

   edict_t *bot = nullptr;
   String resultName;

   // do not allow create bots when there is no graph
   if (!graph.length ()) {
      ctrl.msg ("There is not graph found. Cannot create bot.");
      return BotCreateResult::GraphError;
   }

   // don't allow creating bots with changed graph (distance tables are messed up)
   else if (graph.hasChanged ())  {
      ctrl.msg ("Graph has been changed. Load graph again...");
      return BotCreateResult::GraphError;
   }
   else if (team != -1 && isTeamStacked (team - 1)) {
      ctrl.msg ("Desired team is stacked. Unable to proceed with bot creation.");
      return BotCreateResult::TeamStacked;
   }
   if (difficulty < 0 || difficulty > 4) {
      difficulty = yb_difficulty.int_ ();

      if (difficulty < 0 || difficulty > 4) {
         difficulty = rg.int_ (3, 4);
         yb_difficulty.set (difficulty);
      }
   }

   if (personality < Personality::Normal || personality > Personality::Careful) {
      if (rg.chance (50)) {
         personality = Personality::Normal;
      }
      else {
         if (rg.chance (50)) {
            personality = Personality::Rusher;
         }
         else {
            personality = Personality::Careful;
         }
      }
   }

   BotName *botName = nullptr;

   // setup name
   if (name.empty ()) {
      botName = conf.pickBotName ();

      if (botName) {
         resultName = botName->name;
      }
      else {
         resultName.assignf ("yapb_%d.%d", rg.int_ (100, 10000), rg.int_ (100, 10000)); // just pick ugly random name
      }
   }
   else {
      resultName = name;
   }

   if (!util.isEmptyStr (yb_name_prefix.str ())) {
      String prefixed; // temp buffer for storing modified name
      prefixed.assignf ("%s %s", yb_name_prefix.str (), resultName.chars ());

      // buffer has been modified, copy to real name
      resultName = cr::move (prefixed);
   }
   bot = engfuncs.pfnCreateFakeClient (resultName.chars ());

   if (game.isNullEntity (bot)) {
      ctrl.msg ("Maximum players reached (%d/%d). Unable to create Bot.", game.maxClients (), game.maxClients ());
      return BotCreateResult::MaxPlayersReached;
   }
   auto object = cr::createUnique <Bot> (bot, difficulty, personality, team, member);
   auto index = object->index ();

   // assign owner of bot name
   if (botName != nullptr) {
      botName->usedBy = index; // save by who name is used
   }
   m_bots.push (cr::move (object));

   ctrl.msg ("Connecting Bot...");

   return BotCreateResult::Success;
}

Bot *BotManager::findBotByIndex (int index) {
   // this function finds a bot specified by index, and then returns pointer to it (using own bot array)

   if (index < 0 || index >= kGameMaxPlayers) {
      return nullptr;
   }
   for (const auto &bot : m_bots) {
      if (bot->m_index == index) {
         return bot.get ();
      }
   }
   return nullptr; // no bot
}

Bot *BotManager::findBotByEntity (edict_t *ent) {
   // same as above, but using bot entity

   return findBotByIndex (game.indexOfPlayer (ent));
}

Bot *BotManager::findAliveBot () {
   // this function finds one bot, alive bot :)

   for (const auto &bot : m_bots) {
      if (bot->m_notKilled) {
         return bot.get ();
      }
   }
   return nullptr;
}

void BotManager::slowFrame () {
   // this function calls showframe function for all available at call moment bots

   for (const auto &bot : m_bots) {
      bot->slowFrame ();
   }
}

void BotManager::frame () {
   // this function calls periodic frame function for all available at call moment bots

   for (const auto &bot : m_bots) {
      bot->frame ();
   }

   // select leader each team somewhere in round start
   if (m_timeRoundStart + 5.0f > game.timebase () && m_timeRoundStart + 10.0f < game.timebase ()) {
      for (int team = 0; team < kGameTeamNum; ++team) {
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
   create.difficulty = (difficulty.empty () || difficulty == any) ? -1 : difficulty.int_ ();
   create.team = (team.empty () || team == any) ? -1 : team.int_ ();
   create.member = (member.empty () || member == any) ? -1 : member.int_ ();
   create.personality = (personality.empty () || personality == any) ? -1 : personality.int_ ();
   create.manual = manual;

   m_creationTab.push (cr::move (create));
}

void BotManager::maintainQuota () {
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (graph.length () < 1 || graph.hasChanged ()) {
      if (yb_quota.int_ () > 0) {
         ctrl.msg ("There is no graph found. Cannot create bot.");
      }
      yb_quota.set (0);
      return;
   }

   // bot's creation update
   if (!m_creationTab.empty () && m_maintainTime < game.timebase ()) {
      const CreateQueue &last = m_creationTab.pop ();
      const BotCreateResult callResult = create (last.name, last.difficulty, last.personality, last.team, last.member);

      if (last.manual) {
         yb_quota.set (yb_quota.int_ () + 1);
      }

      // check the result of creation
      if (callResult == BotCreateResult::GraphError) {
         m_creationTab.clear (); // something wrong with graph, reset tab of creation
         yb_quota.set (0); // reset quota
      }
      else if (callResult == BotCreateResult::MaxPlayersReached) {
         m_creationTab.clear (); // maximum players reached, so set quota to maximum players
         yb_quota.set (getBotCount ());
      }
      else if (callResult == BotCreateResult::TeamStacked) {
         ctrl.msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round)");

         m_creationTab.clear ();
         yb_quota.set (getBotCount ());
      }
      m_maintainTime = game.timebase () + 0.10f;
   }

   // now keep bot number up to date
   if (m_quotaMaintainTime > game.timebase ()) {
      return;
   }
   yb_quota.set (cr::clamp <int> (yb_quota.int_ (), 0, game.maxClients ()));

   int totalHumansInGame = getHumansCount ();
   int humanPlayersInGame = getHumansCount (true);

   if (!game.isDedicated () && !totalHumansInGame) {
      return;
   }

   int desiredBotCount = yb_quota.int_ ();
   int botsInGame = getBotCount ();

   if (plat.caseStrMatch (yb_quota_mode.str (), "fill")) {
      botsInGame += humanPlayersInGame;
   }
   else if (plat.caseStrMatch (yb_quota_mode.str (), "match")) {
      int detectQuotaMatch = yb_quota_match.int_ () == 0 ? yb_quota.int_ () : yb_quota_match.int_ ();

      desiredBotCount = cr::max <int> (0, detectQuotaMatch * humanPlayersInGame);
   }

   if (yb_join_after_player.bool_ () && humanPlayersInGame == 0) {
      desiredBotCount = 0;
   }
   int maxClients = game.maxClients ();

   if (yb_autovacate.bool_ ()) {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - (humanPlayersInGame + 1));
   }
   else {
      desiredBotCount = cr::min <int> (desiredBotCount, maxClients - humanPlayersInGame);
   }
   int maxSpawnCount = game.getSpawnCount (Team::Terrorist) + game.getSpawnCount (Team::CT) - humanPlayersInGame;

   // sent message only to console from here
   ctrl.setFromConsole (true);

   // add bots if necessary
   if (desiredBotCount > botsInGame && botsInGame < maxSpawnCount) {
      createRandom ();
   }
   else if (desiredBotCount < botsInGame) {
      auto tp = countTeamPlayers ();

      bool isKicked = false;

      if (tp.first > tp.second) {
         isKicked = kickRandom (false, Team::Terrorist);
      }
      else if (tp.first < tp.second) {
         isKicked = kickRandom (false, Team::CT);
      }
      else {
         isKicked = kickRandom (false, Team::Unassigned);
      }

      // if we can't kick player from correct team, just kick any random to keep quota control work
      if (!isKicked) {
         kickRandom (false, Team::Unassigned);
      }
   }
   m_quotaMaintainTime = game.timebase () + 0.40f;
}

void BotManager::reset () {
   m_maintainTime = 0.0f;
   m_quotaMaintainTime = 0.0f;
   m_grenadeUpdateTime = 0.0f;
   m_entityUpdateTime = 0.0f;
   m_plantSearchUpdateTime = 0.0f;
   m_lastChatTime = 0.0f;
   m_timeBombPlanted = 0.0f;
   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;

   m_intrestingEntities.clear ();
   m_activeGrenades.clear ();
}

void BotManager::initFilters () {
   // table with all available actions for the bots (filtered in & out in bot::setconditions) some of them have subactions included

   m_filters.emplace (Task::Normal, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (Task::Pause, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::MoveToPosition, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (Task::FollowUser, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (Task::PickupItem, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (Task::Camp, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (Task::PlantBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::DefuseBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::Attack, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::Hunt, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::SeekCover, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::ThrowExplosive, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::ThrowFlashbang, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::ThrowSmoke, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::DoubleJump, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::EscapeFromBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::ShootBreakable, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::Hide, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::Blind, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (Task::Spraypaint, 0.0f, kInvalidNodeIndex, 0.0f, false);
}

void BotManager::resetFilters () {
   for (auto &task : m_filters) {
      task.time = 0.0f;
   }
}

void BotManager::decrementQuota (int by) {
   if (by != 0) {
      yb_quota.set (cr::clamp <int> (yb_quota.int_ () - by, 0, yb_quota.int_ ()));
      return;
   }
   yb_quota.set (0);
}

void BotManager::initQuota () {
   m_maintainTime = game.timebase () + yb_join_delay.float_ ();
   m_quotaMaintainTime = game.timebase () + yb_join_delay.float_ ();

   m_creationTab.clear ();
}

void BotManager::serverFill (int selection, int personality, int difficulty, int numToAdd) {
   // this function fill server with bots, with specified team & personality

   // always keep one slot
   int maxClients = yb_autovacate.bool_ () ? game.maxClients () - 1 - (game.isDedicated () ? 0 : getHumansCount ()) : game.maxClients ();

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

   for (int i = 0; i <= toAdd; ++i) {
      addbot ("", difficulty, personality, selection, -1, true);
   }
   ctrl.msg ("Fill Server with %s bots...", &teams[selection][0]);
}

void BotManager::kickEveryone (bool instant, bool zeroQuota) {
   // this function drops all bot clients from server (this function removes only yapb's)`q

   if (!hasBotsOnline () || !yb_quota.bool_ ()) {
      return;
   }

   ctrl.msg ("Bots are removed from server.");

   if (zeroQuota) {
      decrementQuota (0);
   }

   if (instant) {
      for (const auto &bot : m_bots) {
         bot->kick ();
      }
   }
   m_creationTab.clear ();
}

void BotManager::kickFromTeam (Team team, bool removeAll) {
   // this function remove random bot from specified team (if removeAll value = 1 then removes all players from team)

   for (const auto &bot : m_bots) {
      if (team == bot->m_team) {
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

   for (const auto &bot : m_bots) {
      if (team != -1 && team != bot->m_team) {
         continue;
      }
      bot->kill ();
   }
   ctrl.msg ("All Bots died !");
}

void BotManager::kickBot (int index) {
   auto bot = findBotByIndex (index);

   if (bot) {
      decrementQuota ();
      bot->kick ();
   }
}

bool BotManager::kickRandom (bool decQuota, Team fromTeam) {
   // this function removes random bot from server (only yapb's)

   // if forTeam is unassigned, that means random team
   bool deadBotFound = false;

   auto updateQuota = [&] () {
      if (decQuota) {
         decrementQuota ();
      }
   };

   auto belongsTeam = [&] (Bot *bot) {
      if (fromTeam == Team::Unassigned) {
         return true;
      }
      return bot->m_team == fromTeam;
   };

   // first try to kick the bot that is currently dead
   for (const auto &bot : m_bots) {
      if (!bot->m_notKilled && belongsTeam (bot.get ())) // is this slot used?
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
   Bot *selected = nullptr;
   float score = 9999.0f;

   // search bots in this team
   for (const auto &bot : m_bots) {
      if (bot->pev->frags < score && belongsTeam (bot.get ())) {
         selected = bot.get ();
         score = bot->pev->frags;
      }
   }

   // if found some bots
   if (selected != nullptr) {
      updateQuota ();
      selected->kick ();

      return true;
   }

   // worst case, just kick some random bot
   for (const auto &bot : m_bots) {
      if (belongsTeam (bot.get ())) // is this slot used?
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

   constexpr int std[7][kNumWeapons] = {
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Knife only
      {-1, -1, -1, 2, 2, 0, 1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Pistols only
      {-1, -1, -1, -1, -1, -1, -1, 2, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1}, // Shotgun only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 1, 2, 0, 2, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1}, // Machine Guns only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 1, 0, 1, 1, -1, -1, -1, -1, -1, -1}, // Rifles only
      {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, 2, 0, 1, -1, -1}, // Snipers only
      {-1, -1, -1, 2, 2, 0, 1, 2, 2, 2, 1, 2, 0, 2, 0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 2, 1} // Standard
   };

   constexpr int as[7][kNumWeapons] = {
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
   for (int i = 0; i < kNumWeapons; ++i) {
      tab[i].teamStandard = std[selection][i];
      tab[i].teamAS = as[selection][i];
   }
   yb_jasonmode.set (selection == 0 ? 1 : 0);

   ctrl.msg ("%s weapon mode selected", &modes[selection][0]);
}

void BotManager::listBots () {
   // this function list's bots currently playing on the server

   ctrl.msg ("%-3.5s\t%-19.16s\t%-10.12s\t%-3.4s\t%-3.4s\t%-3.4s", "index", "name", "personality", "team", "difficulty", "frags");

   for (const auto &bot : bots) {;
      ctrl.msg ("[%-3.1d]\t%-19.16s\t%-10.12s\t%-3.4s\t%-3.1d\t%-3.1d", bot->index (), STRING (bot->pev->netname), bot->m_personality == Personality::Rusher ? "rusher" : bot->m_personality == Personality::Normal ? "normal" : "careful", bot->m_team == Team::CT ? "CT" : "T", bot->m_difficulty, static_cast <int> (bot->pev->frags));
   }
   ctrl.msg ("%d bots", m_bots.length ());
}

int BotManager::getBotCount () {
   // this function returns number of yapb's playing on the server

   return m_bots.length ();
}

float BotManager::getConnectionTime (int botId) {
   // this function get's fake bot player time.

   for (const auto &bot : m_bots) {
      if (bot->index () == botId) {
         auto current = plat.seconds ();

         if (current - bot->m_joinServerTime > bot->m_playServerTime || current - bot->m_joinServerTime <= 0) {
            bot->m_playServerTime = 60.0f * rg.float_ (30.0f, 240.0f);
            bot->m_joinServerTime = current - bot->m_playServerTime *  rg.float_ (0.2f, 0.8f);
         }
         return current - bot->m_joinServerTime;
      }
   }
   return 0.0f;
}

Twin <int, int> BotManager::countTeamPlayers () {
   int ts = 0, cts = 0;

   for (const auto &client : util.getClients ()) {
      if (client.flags & ClientFlags::Used) {
         if (client.team2 == Team::Terrorist) {
            ts++;
         }
         else if (client.team2 == Team::CT) {
            cts++;
         }
      }
   }
   return { ts, cts };
}

Bot *BotManager::findHighestFragBot (int team) {
   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (const auto &bot : bots) {
      if (bot->m_notKilled && bot->m_team == team) {
         if (bot->pev->frags > bestScore) {
            bestIndex = bot->index ();
            bestScore = bot->pev->frags;
         }
      }
   }
   return findBotByIndex (bestIndex);
}

void BotManager::updateTeamEconomics (int team, bool setTrue) {
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar yb_economics_rounds;

   if (setTrue || !yb_economics_rounds.bool_ ()) {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }
   const int *econLimit = conf.getEconLimit ();

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (const auto &bot : m_bots) {
      if (bot->m_team == team) {
         if (bot->m_moneyAmount <= econLimit[EcoLimit::PrimaryGreater]) {
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

void BotManager::updateBotDifficulties () {
   int difficulty = yb_difficulty.int_ ();

   if (difficulty != m_lastDifficulty) {

      // sets new difficulty for all bots
      for (const auto &bot : m_bots) {
         bot->m_difficulty = difficulty;
      }
      m_lastDifficulty = difficulty;
   }
}

void BotManager::destroy () {
   // this function free all bots slots (used on server shutdown)

   m_bots.clear ();
}

Bot::Bot (edict_t *bot, int difficulty, int personality, int team, int member) {
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
   bots.execGameEntity (&bot->v);

   // set all info buffer keys for this bot
   auto buffer = engfuncs.pfnGetInfoKeyBuffer (bot);
   engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "_vgui_menus", "0");

   if (!game.is (GameFlags::Legacy)) {

      if (yb_show_latency.int_ () == 1) {
         engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*bot", "1");
      }
      auto avatar = conf.getRandomAvatar ();

      if (yb_show_avatars.bool_ () && !avatar.empty ()) {
         engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*sid", avatar.chars ());
      }
   }

   char reject[256] = {0, };
   MDLL_ClientConnect (bot, STRING (bot->v.netname), strings.format ("127.0.0.%d", clientIndex + 100), reject);

   if (!util.isEmptyStr (reject)) {
      logger.error ("Server refused '%s' connection (%s)", STRING (bot->v.netname), reject);
      game.serverCommand ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
      return;
   }

   MDLL_ClientPutInServer (bot);
   bot->v.flags |= FL_FAKECLIENT; // set this player as fakeclient

   // initialize all the variables for this bot...
   m_notStarted = true; // hasn't joined game yet
   m_forceRadio = false;

   m_index = clientIndex - 1;
   m_startAction = BotMsg::None;
   m_retryJoin = 0;
   m_moneyAmount = 0;
   m_logotypeIndex = conf.getRandomLogoIndex ();

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = rg.float_ (3.8f, 10.0f);
   m_sayTextBuffer.chatProbability = rg.int_ (10, 100);

   m_notKilled = false;
   m_weaponBurstMode = BurstMode::Off;
   m_difficulty = cr::clamp (difficulty, 0, 4);
   m_basePing = rg.int_ (7, 14);

   m_lastCommandTime = game.timebase () - 0.1f;
   m_frameInterval = game.timebase ();
   m_slowFrameTimestamp = 0.0f;

   // stuff from jk_botti
   m_playServerTime = 60.0f * rg.float_ (30.0f, 240.0f);
   m_joinServerTime = plat.seconds () - m_playServerTime * rg.float_ (0.2f, 0.8f);

   switch (personality) {
   case 1:
      m_personality = Personality::Rusher;
      m_baseAgressionLevel = rg.float_ (0.7f, 1.0f);
      m_baseFearLevel = rg.float_ (0.0f, 0.4f);
      break;

   case 2:
      m_personality = Personality::Careful;
      m_baseAgressionLevel = rg.float_ (0.2f, 0.5f);
      m_baseFearLevel = rg.float_ (0.7f, 1.0f);
      break;

   default:
      m_personality = Personality::Normal;
      m_baseAgressionLevel = rg.float_ (0.4f, 0.7f);
      m_baseFearLevel = rg.float_ (0.4f, 0.7f);
      break;
   }

   memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
   memset (&m_ammo, 0, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_voicePitch = rg.int_ (80, 115); // assign voice pitch

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

float Bot::getFrameInterval () {
   return cr::fzero (m_thinkInterval) ? m_frameInterval : m_thinkInterval;
}

int BotManager::getHumansCount (bool ignoreSpectators) {
   // this function returns number of humans playing on the server

   int count = 0;

   for (const auto &client : util.getClients ()) {
      if ((client.flags & ClientFlags::Used) && !bots[client.ent] && !(client.ent->v.flags & FL_FAKECLIENT)) {
         if (ignoreSpectators && client.team2 != Team::Terrorist && client.team2 != Team::CT) {
            continue;
         }
         count++;
      }
   }
   return count;
}

int BotManager::getAliveHumansCount () {
   // this function returns number of humans playing on the server

   int count = 0;

   for (const auto &client : util.getClients ()) {
      if ((client.flags & (ClientFlags::Used | ClientFlags::Alive)) && bots[client.ent] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
         count++;
      }
   }
   return count;
}

bool BotManager::isTeamStacked (int team) {
   if (team != Team::CT && team != Team::Terrorist) {
      return false;
   }
   int limitTeams = mp_limitteams.int_ ();

   if (!limitTeams) {
      return false;
   }
   int teamCount[kGameTeamNum] = { 0, };

   for (const auto &client : util.getClients ()) {
      if ((client.flags & ClientFlags::Used) && client.team2 != Team::Unassigned && client.team2 != Team::Spectator) {
         teamCount[client.team2]++;
      }
   }
   return teamCount[team] + 1 > teamCount[team == Team::CT ? Team::Terrorist : Team::CT] + limitTeams;
}

void BotManager::erase (Bot *bot) {
   for (const auto &e : m_bots) {
      if (e.get () == bot) {
         m_bots.remove (e); // remove from bots array
         break;
      }
   }
}

void Bot::newRound () {
   // this function initializes a bot after creation & at the start of each round

   int i = 0;

   // delete all allocated path nodes
   clearSearchNodes ();
   clearRoute ();

   m_pathOrigin= nullvec;
   m_destOrigin= nullvec;
   m_path = nullptr;
   m_currentTravelFlags = 0;
   m_desiredVelocity= nullvec;
   m_currentNodeIndex = kInvalidNodeIndex;
   m_prevGoalIndex = kInvalidNodeIndex;
   m_chosenGoalIndex = kInvalidNodeIndex;
   m_loosedBombNodeIndex = kInvalidNodeIndex;
   m_plantedBombNodeIndex = kInvalidNodeIndex;

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

   for (i = 0; i < 5; ++i) {
      m_previousNodes[i] = kInvalidNodeIndex;
   }
   m_navTimeset = game.timebase ();
   m_team = game.getTeam (ent ());
   m_isVIP = false;

   switch (m_personality) {
   default:
   case Personality::Normal:
      m_pathType = rg.chance (50) ? FindPath::Optimal : FindPath::Safe;
      break;

   case Personality::Rusher:
      m_pathType = FindPath::Fast;
      break;

   case Personality::Careful:
      m_pathType = FindPath::Safe;
      break;
   }

   // clear all states & tasks
   m_states = 0;
   clearTasks ();

   m_isVIP = false;
   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;
   m_preventFlashing = 0.0f;

   m_timeTeamOrder = 0.0f;
   m_askCheckTime = rg.float_ (30.0f, 90.0f);
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
   m_breakableOrigin= nullvec;
   m_timeDoorOpen = 0.0f;

   resetCollision ();
   resetDoubleJump ();

   m_enemy = nullptr;
   m_lastVictim = nullptr;
   m_lastEnemy = nullptr;
   m_lastEnemyOrigin= nullvec;
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

   m_aimLastError= nullvec;
   m_position= nullvec;
   m_liftTravelPos= nullvec;

   setIdealReactionTimers (true);

   m_targetEntity = nullptr;
   m_followWaitTime = 0.0f;

   m_hostages.clear ();

   for (i = 0; i < Chatter::Count; ++i) {
      m_chatterTimes[i] = kMaxChatterRepeatInteval;
   }
   m_isReloading = false;
   m_reloadState = Reload::None;

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

   m_buyState = BuyState::PrimaryWeapon;
   m_lastEquipTime = 0.0f;

   // if bot died, clear all weapon stuff and force buying again
   if (!m_notKilled) {
      memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
      memset (&m_ammo, 0, sizeof (m_ammo));

      m_currentWeapon = 0;
   }
   m_flashLevel = 100.0f;
   m_checkDarkTime = game.timebase ();

   m_knifeAttackTime = game.timebase () + rg.float_ (1.3f, 2.6f);
   m_nextBuyTime = game.timebase () + rg.float_ (0.6f, 2.0f);

   m_buyPending = false;
   m_inBombZone = false;
   m_ignoreBuyDelay = false;
   m_hasC4 = false;

   m_fallDownTime = 0.0f;
   m_shieldCheckTime = 0.0f;
   m_zoomCheckTime = 0.0f;
   m_strafeSetTime = 0.0f;
   m_combatStrafeDir = Dodge::None;
   m_fightStyle = Fight::None;
   m_lastFightStyleCheck = 0.0f;

   m_checkWeaponSwitch = true;
   m_checkKnifeSwitch = true;
   m_buyingFinished = false;

   m_radioEntity = nullptr;
   m_radioOrder = 0;
   m_defendedBomb = false;
   m_defendHostage = false;
   m_headedTime = 0.0f;

   m_timeLogoSpray = game.timebase () + rg.float_ (5.0f, 30.0f);
   m_spawnTime = game.timebase ();
   m_lastChatTime = game.timebase ();

   m_timeCamping = 0.0f;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = game.timebase ();

   // clear its message queue
   for (i = 0; i < 32; ++i) {
      m_messageQueue[i] = BotMsg::None;
   }
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   pushMsgQueue (BotMsg::Buy);
   startTask (Task::Normal, TaskPri::Normal, kInvalidNodeIndex, 0.0f, true);

   if (rg.chance (50)) {
      pushChatterMessage (Chatter::NewRound);
   }
   m_thinkInterval = game.is (GameFlags::Legacy | GameFlags::Xash3D) ? 0.0f : (1.0f / cr::clamp (yb_think_fps.float_ (), 30.0f, 90.0f)) * rg.float_ (0.95f, 1.05f);
}

void Bot::kill () {
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   bots.touchKillerEntity (this);
}

void Bot::kick () {
   // this function kick off one bot from the server.
   auto username = STRING (pev->netname);

   if (!(pev->flags & FL_FAKECLIENT) || util.isEmptyStr (username)) {
      return;
   }
   // clear fakeclient bit
   pev->flags &= ~FL_FAKECLIENT;

   game.serverCommand ("kick \"%s\"", username);
   ctrl.msg ("Bot '%s' kicked", username);
}

void Bot::updateTeamJoin () {
   // this function handles the selection of teams & class

   // cs prior beta 7.0 uses hud-based motd, so press fire once
   if (game.is (GameFlags::Legacy)) {
      pev->button |= IN_ATTACK;
   }

   // check if something has assigned team to us
   else if (m_team == Team::Terrorist || m_team == Team::CT) {
      m_notStarted = false;
   }
   else if (m_team == Team::Unassigned && m_retryJoin > 2) {
      m_startAction = BotMsg::TeamSelect;
   }

   // if bot was unable to join team, and no menus popups, check for stacked team
   if (m_startAction == BotMsg::None && ++m_retryJoin > 3) {
      if (bots.isTeamStacked (m_wantedTeam - 1)) {
         m_retryJoin = 0;

         ctrl.msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).");
         kick ();

         return;
      }
   }

   // handle counter-strike stuff here...
   if (m_startAction == BotMsg::TeamSelect) {
      m_startAction = BotMsg::None; // switch back to idle

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
      game.botCommand (ent (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == BotMsg::ClassSelect) {
      m_startAction = BotMsg::None; // switch back to idle

      // czero has additional models
      int maxChoice = game.is (GameFlags::ConditionZero) ? 5 : 4;

      if (m_wantedClass < 1 || m_wantedClass > maxChoice) {
         m_wantedClass = rg.int_ (1, maxChoice); // use random if invalid
      }

      // select the class the bot wishes to use...
      game.botCommand (ent (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (rg.chance (20)) {
         pushChatMessage (Chat::Hello);
      }
   }
}

void BotManager::captureChatRadio (const char *cmd, const char *arg, edict_t *ent) {
   if (game.isBotCmd ()) {
      return;
   }

   if (plat.caseStrMatch (cmd, "say") || plat.caseStrMatch (cmd, "say_team")) {
      if (strcmp (arg, "dropme") == 0 || strcmp (arg, "dropc4") == 0) {
         Bot *bot = nullptr;

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
         if (!(client.flags & ClientFlags::Used) || (team != -1 && team != client.team2) || alive != util.isAlive (client.ent)) {
            continue;
         }
         auto target = bots[client.ent];
         
         if (target != nullptr) {
            target->m_sayTextBuffer.entityIndex = game.indexOfPlayer (ent);
   
            if (util.isEmptyStr (engfuncs.pfnCmd_Args ())) {
               continue;
            }
            target->m_sayTextBuffer.sayText = engfuncs.pfnCmd_Args ();
            target->m_sayTextBuffer.timeNextChat = game.timebase () + target->m_sayTextBuffer.chatDelay;
         }
      }
   }
   Client &target = util.getClient (game.indexOfPlayer (ent));

   // check if this player alive, and issue something
   if ((target.flags & ClientFlags::Alive) && target.radio != 0 && strncmp (cmd, "menuselect", 10) == 0) {
      int radioCommand = atoi (arg);

      if (radioCommand != 0) {
         radioCommand += 10 * (target.radio - 1);

         if (radioCommand != Radio::RogerThat && radioCommand != Radio::Negative && radioCommand != Radio::ReportingIn) {
            for (const auto &bot : bots) {

               // validate bot
               if (bot->m_team == target.team && ent != bot->ent () && bot->m_radioOrder == 0) {
                  bot->m_radioOrder = radioCommand;
                  bot->m_radioEntity = ent;
               }
            }
         }
         bots.setLastRadioTimestamp (target.team, game.timebase ());
      }
      target.radio = 0;
   }
   else if (strncmp (cmd, "radio", 5) == 0) {
      target.radio = atoi (&cmd[5]);
   }
}

void BotManager::notifyBombDefuse () {
   // notify all terrorists that CT is starting bomb defusing

   for (const auto &bot : bots) {
      if (bot->m_team == Team::Terrorist && bot->m_notKilled && bot->getCurrentTaskId () != Task::MoveToPosition) {
         bot->clearSearchNodes ();

         bot->m_position = graph.getBombPos ();
         bot->startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);
      }
   }
}

void BotManager::updateActiveGrenade () {
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

void BotManager::updateIntrestingEntities () {
   if (m_entityUpdateTime > game.timebase ()) {
      return;
   }

   // clear previously stored entities
   m_intrestingEntities.clear ();

   // search the map for entities
   for (int i = kGameMaxPlayers - 1; i < globals->maxEntities; ++i) {
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
      if (game.mapIs (MapFlags::HostageRescue) && strncmp ("hostage", classname, 7) == 0) {
         m_intrestingEntities.push (ent);
      }
      
      // pickup some csdm stuff if we're running csdm
      if (game.is (GameFlags::CSDM) && strncmp ("csdm", classname, 4) == 0) {
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

   if (game.mapIs (MapFlags::Assassination)) {
      if (team == Team::CT && !m_leaderChoosen[Team::CT]) {
         for (const auto &bot : m_bots) {
            if (bot->m_isVIP) {
               // vip bot is the leader
               bot->m_isLeader = true;

               if (rg.chance (50)) {
                  bot->pushRadioMessage (Radio::FollowMe);
                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[Team::CT] = true;
      }
      else if (team == Team::Terrorist && !m_leaderChoosen[Team::Terrorist]) {
         auto bot = bots.findHighestFragBot (team);

         if (bot != nullptr && bot->m_notKilled) {
            bot->m_isLeader = true;

            if (rg.chance (45)) {
               bot->pushRadioMessage (Radio::FollowMe);
            }
         }
         m_leaderChoosen[Team::Terrorist] = true;
      }
   }
   else if (game.mapIs (MapFlags::Demolition)) {
      if (team == Team::Terrorist && !m_leaderChoosen[Team::Terrorist]) {
         for (const auto &bot : m_bots) {
            if (bot->m_hasC4) {
               // bot carrying the bomb is the leader
               bot->m_isLeader = true;

               // terrorist carrying a bomb needs to have some company
               if (rg.chance (75)) {
                  if (yb_radio_mode.int_ () == 2) {
                     bot->pushChatterMessage (Chatter::GoingToPlantBomb);
                  }
                  else {
                     bot->pushChatterMessage (Radio::FollowMe);
                  }
                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[Team::Terrorist] = true;
      }
      else if (!m_leaderChoosen[Team::CT]) {
         if (auto bot = bots.findHighestFragBot (team)) {
            bot->m_isLeader = true;

            if (rg.chance (30)) {
               bot->pushRadioMessage (Radio::FollowMe);
            }
         }
         m_leaderChoosen[Team::CT] = true;
      }
   }
   else if (game.mapIs (MapFlags::Escape | MapFlags::KnifeArena | MapFlags::Fun)) {
      auto bot = bots.findHighestFragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rg.chance (30)) {
            bot->pushRadioMessage (Radio::FollowMe);
         }
         m_leaderChoosen[team] = true;
      }
   }
   else {
      auto bot = bots.findHighestFragBot (team);

      if (!m_leaderChoosen[team] && bot) {
         bot->m_isLeader = true;

         if (rg.chance (team == Team::Terrorist ? 30 : 40)) {
            bot->pushRadioMessage (Radio::FollowMe);
         }
         m_leaderChoosen[team] = true;
      }
   }
}

void BotManager::initRound () {
   // this is called at the start of each round

   m_roundEnded = false;

   // check team economics
   for (int team = 0; team < kGameTeamNum; ++team) {
      updateTeamEconomics (team);
      selectLeaders (team, true);

      m_lastRadioTime[team] = 0.0f;
   }
   reset ();

   // notify all bots about new round arrived
   for (const auto &bot : bots) {
      bot->newRound ();
   }

   // reset current radio message for all client
   for (auto &client : util.getClients ()) {
      client.radio = 0;
   }

   graph.setBombPos (true);
   graph.clearVisited ();

   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;
   m_timeBombPlanted = 0.0f;
   m_plantSearchUpdateTime = 0.0f;
   m_botsCanPause = false;

   resetFilters ();
   graph.updateGlobalPractice (); // update experience data on round start

   // calculate the round mid/end in world time
   m_timeRoundStart = game.timebase () + mp_freezetime.float_ ();
   m_timeRoundMid = m_timeRoundStart + mp_roundtime.float_ () * 60.0f * 0.5f;
   m_timeRoundEnd = m_timeRoundStart + mp_roundtime.float_ () * 60.0f;
}

void BotManager::setBombPlanted (bool isPlanted) {
   if (isPlanted) {
      m_timeBombPlanted = game.timebase ();
   }
   m_bombPlanted = isPlanted;
}

BotConfig::BotConfig () {
   m_chat.ensure (Chat::Count);
   m_chatter.ensure (Chatter::Count);
   m_weaponProps.ensure (kMaxWeapons);
}

void BotConfig::loadConfigs () {
   setupMemoryFiles ();

   loadNamesConfig ();
   loadChatConfig ();
   loadChatterConfig ();
   loadWeaponsConfig ();
   loadLanguageConfig ();
   loadLogosConfig ();
   loadAvatarsConfig ();
}

void BotConfig::loadMainConfig () {
   if (game.is (GameFlags::Legacy) && !game.is (GameFlags::Xash3D)) {
      util.setNeedForWelcome (true);
   }
   setupMemoryFiles ();

   static bool firstLoad = true;

   auto needsToIgnoreVar = [] (StringArray &list, const char *needle) {
      for (const auto &var : list) {
         if (var == needle) {
            return true;
         }
      }
      return false;
   };
   String line;
   MemFile file;

   // this is does the same as exec of engine, but not overwriting values of cvars spcified in yb_ignore_cvars_on_changelevel
   if (util.openConfig ("yapb.cfg", "YaPB main config file is not found.", &file, false)) {
      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }

         if (firstLoad) {
            game.serverCommand (line.chars ());
            continue;
         }
         auto keyval = line.split (" ");

         if (keyval.length () > 1) {
            auto ignore = String (yb_ignore_cvars_on_changelevel.str ()).split (",");

            auto key = keyval[0].trim ().chars ();
            auto cvar = engfuncs.pfnCVarGetPointer (key);

            if (cvar != nullptr) {
               auto value = const_cast <char *> (keyval[1].trim ().trim ("\"").trim ().chars ());

               if (needsToIgnoreVar (ignore, key) && !plat.caseStrMatch (value, cvar->string)) {
                  game.print ("Bot CVAR '%s' differs from the stored in the config (%s/%s). Ignoring.", cvar->name, cvar->string, value);

                  // ensure cvar will have old value
                  engfuncs.pfnCvar_DirectSet (cvar, cvar->string);
               }
               else {
                  engfuncs.pfnCvar_DirectSet (cvar, value);
               }
            }
            else {
               game.serverCommand (line.chars ());
            }
         }
      }
      file.close ();
   }
   firstLoad = false;

   // android is abit hard to play, lower the difficulty by default
   if (plat.isAndroid && yb_difficulty.int_ () > 2) {
      yb_difficulty.set (2);
   }
   return;
}

void BotConfig::loadNamesConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // naming initialization
   if (util.openConfig ("names.cfg", "Name configuration file not found.", &file, true)) {
      m_botNames.clear ();

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         // max botname is 32 characters
         if (line.length () > 32) {
            line[32] = '\0';
         }
         m_botNames.emplace (line, -1);
      }
      file.close ();
   }
}

void BotConfig::loadWeaponsConfig () {
   setupMemoryFiles ();

   auto addWeaponEntries = [] (SmallArray <WeaponInfo> &weapons, bool as, const String &name, const StringArray &data) {

      // we're have null terminator element in weapons array...
      if (data.length () + 1 != weapons.length ()) {
         logger.error ("%s entry in weapons config is not valid or malformed (%d/%d).", name.chars (), data.length (), weapons.length ());

         return;
      }

      for (size_t i = 0; i < data.length (); ++i) {
         if (as) {
            weapons[i].teamAS = data[i].int_ ();
         }
         else {
            weapons[i].teamStandard = data[i].int_ ();
         }
      }
   };

   auto addIntEntries = [] (SmallArray <int32> &to, const String &name, const StringArray &data) {
      if (data.length () != to.length ()) {
         logger.error ("%s entry in weapons config is not valid or malformed (%d/%d).", name.chars (), data.length (), to.length ());
         return;
      }

      for (size_t i = 0; i < to.length (); ++i) {
         to[i] = data[i].int_ ();
      }
   };
   String line;
   MemFile file;

   // weapon data initialization
   if (util.openConfig ("general.cfg", "General configuration file not found. Loading defaults", &file)) {
      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         auto pair = line.split ("=");

         if (pair.length () != 2) {
            continue;
         }

         for (auto &trim : pair) {
            trim.trim ();
         }
         auto splitted = pair[1].split (",");

         if (pair[0].startsWith ("MapStandard")) {
            addWeaponEntries (m_weapons, false, pair[0], splitted);
         }
         else if (pair[0].startsWith ("MapAS")) {
            addWeaponEntries (m_weapons, true, pair[0], splitted);
         }

         else if (pair[0].startsWith ("GrenadePercent")) {
            addIntEntries (m_grenadeBuyPrecent, pair[0], splitted);
         }
         else if (pair[0].startsWith ("Economics")) {
            addIntEntries (m_botBuyEconomyTable, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityNormal")) {
            addIntEntries (m_normalWeaponPrefs, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityRusher")) {
            addIntEntries (m_rusherWeaponPrefs, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityCareful")) {
            addIntEntries (m_carefulWeaponPrefs, pair[0], splitted);
         }
      }
      file.close ();
   }
}

void BotConfig::loadChatterConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // chatter initialization
   if (game.is (GameFlags::HasBotVoice) && yb_radio_mode.int_ () == 2 && util.openConfig ("chatter.cfg", "Couldn't open chatter system configuration", &file)) {
      struct EventMap {
         String str;
         int code;
         float repeat;
      } chatterEventMap[] = {
         { "Radio_CoverMe", Radio::CoverMe, kMaxChatterRepeatInteval },
         { "Radio_YouTakePoint", Radio::YouTakeThePoint, kMaxChatterRepeatInteval },
         { "Radio_HoldPosition", Radio::HoldThisPosition, 10.0f },
         { "Radio_RegroupTeam", Radio::RegroupTeam, 10.0f },
         { "Radio_FollowMe", Radio::FollowMe, 15.0f },
         { "Radio_TakingFire", Radio::TakingFireNeedAssistance, 5.0f },
         { "Radio_GoGoGo", Radio::GoGoGo, kMaxChatterRepeatInteval },
         { "Radio_Fallback", Radio::TeamFallback, kMaxChatterRepeatInteval },
         { "Radio_StickTogether", Radio::StickTogetherTeam, kMaxChatterRepeatInteval },
         { "Radio_GetInPosition", Radio::GetInPositionAndWaitForGo, kMaxChatterRepeatInteval },
         { "Radio_StormTheFront", Radio::StormTheFront, kMaxChatterRepeatInteval },
         { "Radio_ReportTeam", Radio::ReportInTeam, kMaxChatterRepeatInteval },
         { "Radio_Affirmative", Radio::RogerThat, kMaxChatterRepeatInteval },
         { "Radio_EnemySpotted", Radio::EnemySpotted, 4.0f },
         { "Radio_NeedBackup", Radio::NeedBackup, kMaxChatterRepeatInteval },
         { "Radio_SectorClear", Radio::SectorClear, 10.0f },
         { "Radio_InPosition", Radio::ImInPosition, 10.0f },
         { "Radio_ReportingIn", Radio::ReportingIn, kMaxChatterRepeatInteval },
         { "Radio_ShesGonnaBlow", Radio::ShesGonnaBlow, kMaxChatterRepeatInteval },
         { "Radio_Negative", Radio::Negative, kMaxChatterRepeatInteval },
         { "Radio_EnemyDown", Radio::EnemyDown, 10.0f },
         { "Chatter_DiePain", Chatter::DiePain, kMaxChatterRepeatInteval },
         { "Chatter_GoingToPlantBomb", Chatter::GoingToPlantBomb, 5.0f },
         { "Chatter_GoingToGuardVIPSafety", Chatter::GoingToGuardVIPSafety, kMaxChatterRepeatInteval },
         { "Chatter_RescuingHostages", Chatter::RescuingHostages, kMaxChatterRepeatInteval },
         { "Chatter_TeamKill", Chatter::FriendlyFire, kMaxChatterRepeatInteval },
         { "Chatter_GuardingVipSafety", Chatter::GuardingVIPSafety, kMaxChatterRepeatInteval },
         { "Chatter_PlantingC4", Chatter::PlantingBomb, 10.0f },
         { "Chatter_InCombat", Chatter::InCombat,  kMaxChatterRepeatInteval },
         { "Chatter_SeeksEnemy", Chatter::SeekingEnemies, kMaxChatterRepeatInteval },
         { "Chatter_Nothing", Chatter::Nothing,  kMaxChatterRepeatInteval },
         { "Chatter_EnemyDown", Chatter::EnemyDown, 10.0f },
         { "Chatter_UseHostage", Chatter::UsingHostages, kMaxChatterRepeatInteval },
         { "Chatter_WonTheRound", Chatter::WonTheRound, kMaxChatterRepeatInteval },
         { "Chatter_QuicklyWonTheRound", Chatter::QuickWonRound, kMaxChatterRepeatInteval },
         { "Chatter_NoEnemiesLeft", Chatter::NoEnemiesLeft, kMaxChatterRepeatInteval },
         { "Chatter_FoundBombPlace", Chatter::FoundC4Plant, 15.0f },
         { "Chatter_WhereIsTheBomb", Chatter::WhereIsTheC4, kMaxChatterRepeatInteval },
         { "Chatter_DefendingBombSite", Chatter::DefendingBombsite, kMaxChatterRepeatInteval },
         { "Chatter_BarelyDefused", Chatter::BarelyDefused, kMaxChatterRepeatInteval },
         { "Chatter_NiceshotCommander", Chatter::NiceShotCommander, kMaxChatterRepeatInteval },
         { "Chatter_ReportingIn", Chatter::ReportingIn, 10.0f },
         { "Chatter_SpotTheBomber", Chatter::SpotTheBomber, 4.3f },
         { "Chatter_VIPSpotted", Chatter::VIPSpotted, 5.3f },
         { "Chatter_FriendlyFire", Chatter::FriendlyFire, 2.1f },
         { "Chatter_GotBlinded", Chatter::Blind, 12.0f },
         { "Chatter_GuardDroppedC4", Chatter::GuardingDroppedC4, 3.0f },
         { "Chatter_DefusingC4", Chatter::DefusingBomb, 3.0f },
         { "Chatter_FoundC4", Chatter::FoundC4, 5.5f },
         { "Chatter_ScaredEmotion", Chatter::ScaredEmotion, 6.1f },
         { "Chatter_HeardEnemy", Chatter::ScaredEmotion, 12.8f },
         { "Chatter_SniperWarning", Chatter::SniperWarning, 14.3f },
         { "Chatter_SniperKilled", Chatter::SniperKilled, 12.1f },
         { "Chatter_OneEnemyLeft", Chatter::OneEnemyLeft, 12.5f },
         { "Chatter_TwoEnemiesLeft", Chatter::TwoEnemiesLeft, 12.5f },
         { "Chatter_ThreeEnemiesLeft", Chatter::ThreeEnemiesLeft, 12.5f },
         { "Chatter_NiceshotPall", Chatter::NiceShotPall, 2.0f },
         { "Chatter_GoingToGuardHostages", Chatter::GoingToGuardHostages, 3.0f },
         { "Chatter_GoingToGuardDoppedBomb", Chatter::GoingToGuardDroppedC4, 6.0f },
         { "Chatter_OnMyWay", Chatter::OnMyWay, 1.5f },
         { "Chatter_LeadOnSir", Chatter::LeadOnSir, 5.0f },
         { "Chatter_Pinned_Down", Chatter::PinnedDown, 5.0f },
         { "Chatter_GottaFindTheBomb", Chatter::GottaFindC4, 3.0f },
         { "Chatter_You_Heard_The_Man", Chatter::YouHeardTheMan, 3.0f },
         { "Chatter_Lost_The_Commander", Chatter::LostCommander, 4.5f },
         { "Chatter_NewRound", Chatter::NewRound, 3.5f },
         { "Chatter_CoverMe", Chatter::CoverMe, 3.5f },
         { "Chatter_BehindSmoke", Chatter::BehindSmoke, 3.5f },
         { "Chatter_BombSiteSecured", Chatter::BombsiteSecured, 3.5f },
         { "Chatter_GoingToCamp", Chatter::GoingToCamp, 30.0f },
         { "Chatter_Camp", Chatter::Camping, 10.0f },
      };

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         extern ConVar yb_chatter_path;

         if (line.startsWith ("RewritePath")) {
            yb_chatter_path.set (line.substr (11).trim ().chars ());
         }
         else if (line.startsWith ("Event")) {
            auto items = line.substr (5).split ("=");

            if (items.length () != 2) {
               logger.error ("Error in chatter config file syntax... Please correct all errors.");
               continue;
            }

            for (auto &item : items) {
               item.trim ();
            }
            items[1].trim ("(;)");

            for (const auto &event : chatterEventMap) {
               if (event.str == items[0]) {
                  // this does common work of parsing comma-separated chatter line
                  auto sounds = items[1].split (",");

                  for (auto &sound : sounds) {
                     sound.trim ().trim ("\"");
                     float duration = game.getWaveLen (sound.chars ());

                     if (duration > 0.0f) {
                        m_chatter[event.code].emplace (cr::move (sound), event.repeat, duration);
                     }
                  }
                  sounds.clear ();
               }
            }
         }
      }
      file.close ();
   }
   else {
      yb_radio_mode.set (1);
      logger.message ("Bots chatter communication disabled.");
   }
}

void BotConfig::loadChatConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // chat config initialization
   if (util.openConfig ("chat.cfg", "Chat file not found.", &file, true)) {
      StringArray *chat = nullptr;

      StringArray keywords {};
      StringArray replies {};

      // clear all the stuff before loading new one
      for (auto &item : m_chat) {
         item.clear ();
      }
      m_replies.clear ();

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }

         if (line.startsWith ("[KILLED]")) {
            chat = &m_chat[Chat::Kill];
            continue;
         }
         else if (line.startsWith ("[BOMBPLANT]")) {
            chat = &m_chat[Chat::Kill];
            continue;
         }
         else if (line.startsWith ("[DEADCHAT]")) {
            chat = &m_chat[Chat::Dead];
            continue;
         }
         else if (line.startsWith ("[REPLIES]")) {
            chat = nullptr;
            continue;
         }
         else if (line.startsWith ("[UNKNOWN]")) {
            chat = &m_chat[Chat::NoKeyword];
            continue;
         }
         else if (line.startsWith ("[TEAMATTACK]")) {
            chat = &m_chat[Chat::TeamAttack];
            continue;
         }
         else if (line.startsWith ("[WELCOME]")) {
            chat = &m_chat[Chat::Hello];
            continue;
         }
         else if (line.startsWith ("[TEAMKILL]")) {
            chat = &m_chat[Chat::TeamKill];
            continue;
         }

         if (chat != nullptr) {
            chat->push (line);
         }
         else {
            if (line.startsWith ("@KEY")) {
               if (!keywords.empty () && !replies.empty ()) {
                  m_replies.emplace (keywords, replies);

                  keywords.clear ();
                  replies.clear ();
               }

               keywords.clear ();
               keywords = cr::move (line.substr (4).split (","));

               for (auto &keyword : keywords) {
                  keyword.trim ().trim ("\"");
               }
            }
            else if (!keywords.empty () && !line.empty ()) {
               replies.push (line);
            }
         }
      }

      // shuffle chat a bit
      for (auto &item : m_chat) {
         item.shuffle ();
         item.shuffle ();
      }
      file.close ();
   }
   else {
      yb_chat.set (0);
   }
}

void BotConfig::loadLanguageConfig () {
   setupMemoryFiles ();

   if (game.isDedicated () || game.is (GameFlags::Legacy)) {

      if (game.is (GameFlags::Legacy)) {
         logger.message ("Bots multilingual system disabled, due to your Counter-Strike Version!");
      }
      return; // dedicated server will use only english translation
   }
   String line;
   MemFile file;

   // localizer inititalization
   if (util.openConfig ("lang.cfg", "Specified language not found.", &file, true)) {
      String temp;
      Twin <String, String> lang;

      // clear all the translations before new load
      game.clearTranslation ();

      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }

         if (line.startsWith ("[ORIGINAL]")) {
            if (!temp.empty ()) {
               lang.second = cr::move (temp);
            }

            if (!lang.second.empty () && !lang.first.empty ()) {
               game.addTranslation (lang.first.trim (), lang.second.trim ());
            }
         }
         else if (line.startsWith ("[TRANSLATED]") && !temp.empty ()) {
            lang.first = cr::move (temp);
         }
         else {
            temp += line;
         }
      }
      file.close ();
   }
   else if (strcmp (yb_language.str (), "en") != 0) {
      logger.error ("Couldn't load language configuration");
   }
}

void BotConfig::loadAvatarsConfig () {
   setupMemoryFiles ();

   if (game.is (GameFlags::Legacy)) {
      return;
   }

   String line;
   MemFile file;

   // avatars inititalization
   if (util.openConfig ("avatars.cfg", "Avatars config file not found. Avatars will not display.", &file)) {
      m_avatars.clear ();
      
      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }
         m_avatars.push (cr::move (line.trim ()));
      }
   }
}

void BotConfig::loadLogosConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // logos inititalization
   if (util.openConfig ("logos.cfg", "Logos config file not found. Loading defaults.", &file)) {
      m_logos.clear ();

      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }
         m_logos.push (cr::move (line.trim ()));
      }
   }
   else {
      m_logos = cr::move (String ("{biohaz;{graf003;{graf004;{graf005;{lambda06;{target;{hand1;{spit2;{bloodhand6;{foot_l;{foot_r").split (";"));
   }
}

void BotConfig::setupMemoryFiles () {
   static bool setMemoryPointers = true;

   auto wrapLoadFile= [] (const char *filename, int *length) {
      return engfuncs.pfnLoadFileForMe (filename, length);
   };

   auto wrapFreeFile = [] (void *buffer) {
      return engfuncs.pfnFreeFile (buffer);
   };

   if (setMemoryPointers) {
      MemFileStorage::get ().initizalize (wrapLoadFile, wrapFreeFile);
      setMemoryPointers = true;
   }
}

BotName *BotConfig::pickBotName () {
   if (m_botNames.empty ()) {
      return nullptr;
   }

   for (size_t i = 0; i < m_botNames.length () * 2; ++i) {
      auto botName = &m_botNames.random ();

      if (botName->name.length () < 3 || botName->usedBy != -1) {
         continue;
      }
      return botName;
   }
   return nullptr;
}

void BotConfig::clearUsedName (Bot *bot) {
   for (auto &name : m_botNames) {
      if (name.usedBy == bot->index ()) {
         name.usedBy = -1;
         break;
      }
   }
}

void BotConfig::initWeapons () {
   // fill array with available weapons
   m_weapons.emplace (Weapon::Knife,      "weapon_knife",     "knife.mdl",     0,    0, -1, -1,  0,  0,  0,  0,  0,  0,   true  );
   m_weapons.emplace (Weapon::USP,        "weapon_usp",       "usp.mdl",       500,  1, -1, -1,  1,  1,  2,  2,  0,  12,  false );
   m_weapons.emplace (Weapon::Glock18,    "weapon_glock18",   "glock18.mdl",   400,  1, -1, -1,  1,  2,  1,  1,  0,  20,  false );
   m_weapons.emplace (Weapon::Deagle,     "weapon_deagle",    "deagle.mdl",    650,  1,  2,  2,  1,  3,  4,  4,  2,  7,   false );
   m_weapons.emplace (Weapon::P228,       "weapon_p228",      "p228.mdl",      600,  1,  2,  2,  1,  4,  3,  3,  0,  13,  false );
   m_weapons.emplace (Weapon::Elite,      "weapon_elite",     "elite.mdl",     800,  1,  0,  0,  1,  5,  5,  5,  0,  30,  false );
   m_weapons.emplace (Weapon::FiveSeven,  "weapon_fiveseven", "fiveseven.mdl", 750,  1,  1,  1,  1,  6,  5,  5,  0,  20,  false );
   m_weapons.emplace (Weapon::M3,         "weapon_m3",        "m3.mdl",        1700, 1,  2, -1,  2,  1,  1,  1,  0,  8,   false );
   m_weapons.emplace (Weapon::XM1014,     "weapon_xm1014",    "xm1014.mdl",    3000, 1,  2, -1,  2,  2,  2,  2,  0,  7,   false );
   m_weapons.emplace (Weapon::MP5,        "weapon_mp5navy",   "mp5.mdl",       1500, 1,  2,  1,  3,  1,  2,  2,  0,  30,  true  );
   m_weapons.emplace (Weapon::TMP,        "weapon_tmp",       "tmp.mdl",       1250, 1,  1,  1,  3,  2,  1,  1,  0,  30,  true  );
   m_weapons.emplace (Weapon::P90,        "weapon_p90",       "p90.mdl",       2350, 1,  2,  1,  3,  3,  4,  4,  0,  50,  true  );
   m_weapons.emplace (Weapon::MAC10,      "weapon_mac10",     "mac10.mdl",     1400, 1,  0,  0,  3,  4,  1,  1,  0,  30,  true  );
   m_weapons.emplace (Weapon::UMP45,      "weapon_ump45",     "ump45.mdl",     1700, 1,  2,  2,  3,  5,  3,  3,  0,  25,  true  );
   m_weapons.emplace (Weapon::AK47,       "weapon_ak47",      "ak47.mdl",      2500, 1,  0,  0,  4,  1,  2,  2,  2,  30,  true  );
   m_weapons.emplace (Weapon::SG552,      "weapon_sg552",     "sg552.mdl",     3500, 1,  0, -1,  4,  2,  4,  4,  2,  30,  true  );
   m_weapons.emplace (Weapon::M4A1,       "weapon_m4a1",      "m4a1.mdl",      3100, 1,  1,  1,  4,  3,  3,  3,  2,  30,  true  );
   m_weapons.emplace (Weapon::Galil,      "weapon_galil",     "galil.mdl",     2000, 1,  0,  0,  4,  -1, 1,  1,  2,  35,  true  );
   m_weapons.emplace (Weapon::Famas,      "weapon_famas",     "famas.mdl",     2250, 1,  1,  1,  4,  -1, 1,  1,  2,  25,  true  );
   m_weapons.emplace (Weapon::AUG,        "weapon_aug",       "aug.mdl",       3500, 1,  1,  1,  4,  4,  4,  4,  2,  30,  true  );
   m_weapons.emplace (Weapon::Scout,      "weapon_scout",     "scout.mdl",     2750, 1,  2,  0,  4,  5,  3,  2,  3,  10,  false );
   m_weapons.emplace (Weapon::AWP,        "weapon_awp",       "awp.mdl",       4750, 1,  2,  0,  4,  6,  5,  6,  3,  10,  false );
   m_weapons.emplace (Weapon::G3SG1,      "weapon_g3sg1",     "g3sg1.mdl",     5000, 1,  0,  2,  4,  7,  6,  6,  3,  20,  false );
   m_weapons.emplace (Weapon::SG550,      "weapon_sg550",     "sg550.mdl",     4200, 1,  1,  1,  4,  8,  5,  5,  3,  30,  false );
   m_weapons.emplace (Weapon::M249,       "weapon_m249",      "m249.mdl",      5750, 1,  2,  1,  5,  1,  1,  1,  2,  100, true  );
   m_weapons.emplace (Weapon::Shield,     "weapon_shield",    "shield.mdl",    2200, 0,  1,  1,  8,  -1, 8,  8,  0,  0,   false );

   // not needed actually, but cause too much refactoring for now. todo
   m_weapons.emplace (0,                 "",                 "",              0,    0,  0,  0,  0,   0, 0,  0,  0,  0,   false );
}

void BotConfig::adjustWeaponPrices () {
   // elite price is 1000$ on older versions of cs...
   if (!(game.is (GameFlags::Legacy))) {
      return;
   }

   for (auto &weapon : m_weapons) {
      if (weapon.id == Weapon::Elite) {
         weapon.price = 1000;
         break;
      }
   }
}

WeaponInfo &BotConfig::findWeaponById (const int id) {
   for (auto &weapon : m_weapons) {
      if (weapon.id == id) {
         return weapon;
      }
   }
   return m_weapons.at (0);
}
