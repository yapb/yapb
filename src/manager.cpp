//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_autovacate ("yb_autovacate", "1", "Kick bots to automatically make room for human players.");
ConVar cv_autovacate_keep_slots ("yb_autovacate_keep_slots", "1", "How many slots autovacate feature should keep for human players", true, 1.0f, 8.0f);
ConVar cv_kick_after_player_connect ("yb_kick_after_player_connect", "1", "Kick the bot immediately when a human player joins the server (yb_autovacate must be enabled).");

ConVar cv_quota ("yb_quota", "9", "Specifies the number bots to be added to the game.", true, 0.0f, static_cast <float> (kGameMaxPlayers));
ConVar cv_quota_mode ("yb_quota_mode", "normal", "Specifies the type of quota.\nAllowed values: 'normal', 'fill', and 'match'.\nIf 'fill', the server will adjust bots to keep N players in the game, where N is yb_quota.\nIf 'match', the server will maintain a 1:N ratio of humans to bots, where N is yb_quota_match.", false);
ConVar cv_quota_match ("yb_quota_match", "0", "Number of players to match if yb_quota_mode set to 'match'", true, 0.0f, static_cast <float> (kGameMaxPlayers));
ConVar cv_think_fps ("yb_think_fps", "26.0", "Specifies how many times per second bot code will run.", true, 24.0f, 90.0f);
ConVar cv_autokill_delay ("yb_autokill_delay", "0.0", "Specifies amount of time in seconds when bots will be killed if no humans left alive.", true, 0.0f, 90.0f);

ConVar cv_join_after_player ("yb_join_after_player", "0", "Specifies whether bots should join server, only when at least one human player in game.");
ConVar cv_join_team ("yb_join_team", "any", "Forces all bots to join team specified here.", false);
ConVar cv_join_delay ("yb_join_delay", "5.0", "Specifies after how many seconds bots should start to join the game after the changelevel.", true, 0.0f, 30.0f);
ConVar cv_name_prefix ("yb_name_prefix", "", "All the bot names will be prefixed with string specified with this cvar.", false);

ConVar cv_difficulty ("yb_difficulty", "3", "All bots difficulty level. Changing at runtime will affect already created bots.", true, 0.0f, 4.0f);

ConVar cv_difficulty_min ("yb_difficulty_min", "-1", "Lower bound of random difficulty on bot creation. Only affects newly created bots. -1 means yb_difficulty only used.", true, -1.0f, 4.0f);
ConVar cv_difficulty_max ("yb_difficulty_max", "-1", "Upper bound of random difficulty on bot creation. Only affects newly created bots. -1 means yb_difficulty only used.", true, -1.0f, 4.0f);
ConVar cv_difficulty_auto ("yb_difficulty_auto", "0", "Allows each bot to balance their own difficulty based kd-ratio of team.", true, 0.0f, 1.0f);
ConVar cv_difficulty_auto_balance_interval ("yb_difficulty_auto_balance_interval", "30", "Interval in which bots will balance their difficulty.", true, 30.0f, 240.0f);

ConVar cv_show_avatars ("yb_show_avatars", "1", "Enables or disables displaying bot avatars in front of their names in scoreboard. Note, that is currently you can see only avatars of your steam friends.");
ConVar cv_show_latency ("yb_show_latency", "2", "Enables latency display in scoreboard.\nAllowed values: '0', '1', '2'.\nIf '0', there is nothing displayed.\nIf '1', there is a 'BOT' is displayed.\nIf '2' fake ping is displayed.", true, 0.0f, 2.0f);
ConVar cv_save_bots_names ("yb_save_bots_names", "1", "Allows to save bot names upon changelevel, so bot names will be the same after a map change.", true, 0.0f, 1.0f);

ConVar cv_botskin_t ("yb_botskin_t", "0", "Specifies the bots wanted skin for Terrorist team.", true, 0.0f, 5.0f);
ConVar cv_botskin_ct ("yb_botskin_ct", "0", "Specifies the bots wanted skin for CT team.", true, 0.0f, 5.0f);

ConVar cv_ping_base_min ("yb_ping_base_min", "7", "Lower bound for base bot ping shown in scoreboard upon creation.", true, 0.0f, 100.0f);
ConVar cv_ping_base_max ("yb_ping_base_max", "34", "Upper bound for base bot ping shown in scoreboard upon creation.", true, 0.0f, 100.0f);

ConVar cv_quota_adding_interval ("yb_quota_adding_interval", "0.10", "Interval in which bots are added to the game.", true, 0.10f, 1.0f);
ConVar cv_quota_maintain_interval ("yb_quota_maintain_interval", "0.40", "Interval on which overall bot quota are checked.", true, 0.40f, 2.0f);

ConVar cv_language ("yb_language", "en", "Specifies the language for bot messages and menus.", false);

ConVar cv_rotate_bots ("yb_rotate_bots", "0", "Randomly disconnect and connect bots, simulating players join/quit.");
ConVar cv_rotate_stay_min ("yb_rotate_stay_min", "360.0", "Specifies minimum amount of seconds bot keep connected, if rotation active.", true, 120.0f, 7200.0f);
ConVar cv_rotate_stay_max ("yb_rotate_stay_max", "3600.0", "Specifies maximum amount of seconds bot keep connected, if rotation active.", true, 1800.0f, 14400.0f);

ConVar mp_limitteams ("mp_limitteams", nullptr, Var::GameRef);
ConVar mp_autoteambalance ("mp_autoteambalance", nullptr, Var::GameRef);
ConVar mp_roundtime ("mp_roundtime", nullptr, Var::GameRef);
ConVar mp_timelimit ("mp_timelimit", nullptr, Var::GameRef);
ConVar mp_freezetime ("mp_freezetime", nullptr, Var::GameRef, true, "0");

BotManager::BotManager () {
   // this is a bot manager class constructor

   m_lastDifficulty = 0;
   m_lastWinner = -1;

   m_timeRoundStart = 0.0f;
   m_timeRoundMid = 0.0f;
   m_timeRoundEnd = 0.0f;

   m_autoKillCheckTime = 0.0f;
   m_maintainTime = 0.0f;
   m_quotaMaintainTime = 0.0f;
   m_difficultyBalanceTime = 0.0f;

   m_bombPlanted = false;
   m_botsCanPause = false;
   m_roundOver = false;

   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;

   for (int i = 0; i < kGameTeamNum; ++i) {
      m_leaderChoosen[i] = false;
      m_economicsGood[i] = true;

      m_lastRadioTime[i] = 0.0f;
      m_lastRadio[i] = -1;
   }
   reset ();

   m_addRequests.clear ();
   m_killerEntity = nullptr;

   initFilters ();
}

void BotManager::createKillerEntity () {
   // this function creates single trigger_hurt for using in Bot::kill, to reduce lags, when killing all the bots

   m_killerEntity = engfuncs.pfnCreateNamedEntity ("trigger_hurt");

   m_killerEntity->v.dmg = kInfiniteDistance;
   m_killerEntity->v.dmg_take = 1.0f;
   m_killerEntity->v.dmgtime = 2.0f;
   m_killerEntity->v.effects |= EF_NODRAW;

   engfuncs.pfnSetOrigin (m_killerEntity, Vector (-kInfiniteDistance, -kInfiniteDistance, -kInfiniteDistance));
   MDLL_Spawn (m_killerEntity);
}

void BotManager::destroyKillerEntity () {
   if (!game.isNullEntity (m_killerEntity)) {
      engfuncs.pfnRemoveEntity (m_killerEntity);
      m_killerEntity = nullptr;
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

   m_killerEntity->v.classname = prop.classname.chars ();
   m_killerEntity->v.dmg_inflictor = bot->ent ();
   m_killerEntity->v.dmg = (bot->pev->health + bot->pev->armorvalue) * 4.0f;

   KeyValueData kv {};
   kv.szClassName = prop.classname.chars ();
   kv.szKeyName = "damagetype";
   kv.szValue = strings.format ("%d", cr::bit (4));
   kv.fHandled = HLFalse;

   MDLL_KeyValue (m_killerEntity, &kv);
   MDLL_Touch (m_killerEntity, bot->ent ());
}

void BotManager::execGameEntity (edict_t *ent) {
   // this function calls gamedll player() function, in case to create player entity in game

   if (game.is (GameFlags::Metamod)) {
      MUTIL_CallGameEntity (PLID, "player", &ent->v);
      return;
   }
   ents.callPlayerFunction (ent);
}

void BotManager::forEach (ForEachBot handler) {
   for (const auto &bot : m_bots) {
      if (handler (bot.get ())) {
         return;
      }
   }
}

BotCreateResult BotManager::create (StringRef name, int difficulty, int personality, int team, int skin) {
   // this function completely prepares bot entity (edict) for creation, creates team, difficulty, sets named etc, and
   // then sends result to bot constructor

   edict_t *bot = nullptr;
   String resultName;

   // do not allow create bots when there is no graph
   if (!graph.length ()) {
      ctrl.msg ("There is no graph found. Cannot create bot.");
      return BotCreateResult::GraphError;
   }

   // don't allow creating bots with changed graph (distance tables are messed up)
   else if (graph.hasChanged ()) {
      ctrl.msg ("Graph has been changed. Load graph again...");
      return BotCreateResult::GraphError;
   }
   else if (team != -1 && isTeamStacked (team - 1)) {
      ctrl.msg ("Desired team is stacked. Unable to proceed with bot creation.");
      return BotCreateResult::TeamStacked;
   }
   if (difficulty < 0 || difficulty > 4) {
      difficulty = cv_difficulty.int_ ();

      if (difficulty < 0 || difficulty > 4) {
         difficulty = rg.get (3, 4);
         cv_difficulty.set (difficulty);
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
         resultName.assignf ("%s_%d.%d", product.nameLower, rg.get (100, 10000), rg.get (100, 10000)); // just pick ugly random name
      }
   }
   else {
      resultName = name;
   }
   const bool hasNamePrefix = !strings.isEmpty (cv_name_prefix.str ());

   // disable save bots names if prefix is enabled
   if (hasNamePrefix && cv_save_bots_names.bool_ ()) {
      cv_save_bots_names.set (0);
   }

   if (hasNamePrefix) {
      String prefixed; // temp buffer for storing modified name
      prefixed.assignf ("%s %s", cv_name_prefix.str (), resultName);

      // buffer has been modified, copy to real name
      resultName = cr::move (prefixed);
   }
   bot = engfuncs.pfnCreateFakeClient (resultName.chars ());

   if (game.isNullEntity (bot)) {
      ctrl.msg ("Maximum players reached (%d/%d). Unable to create Bot.", game.maxClients (), game.maxClients ());
      return BotCreateResult::MaxPlayersReached;
   }
   auto object = cr::makeUnique <Bot> (bot, difficulty, personality, team, skin);
   auto index = object->index ();

   // assign owner of bot name
   if (botName != nullptr) {
      botName->usedBy = index; // save by who name is used
   }
   else {
      conf.setBotNameUsed (index, resultName);
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

void BotManager::frame () {
   // this function calls showframe function for all available at call moment bots

   for (const auto &bot : m_bots) {
      bot->frame ();
   }
}

void BotManager::addbot (StringRef name, int difficulty, int personality, int team, int skin, bool manual) {
   // this function putting bot creation process to queue to prevent engine crashes

   BotRequest request {};

   // fill the holder
   request.name = name;
   request.difficulty = difficulty;
   request.personality = personality;
   request.team = team;
   request.skin = skin;
   request.manual = manual;

   // restore the bot name
   if (cv_save_bots_names.bool_ () && name.empty () && !m_saveBotNames.empty ()) {
      request.name = m_saveBotNames.popFront ();
   }

   // put to queue
   m_addRequests.emplaceLast (cr::move (request));
}

void BotManager::addbot (StringRef name, StringRef difficulty, StringRef personality, StringRef team, StringRef skin, bool manual) {
   // this function is same as the function above, but accept as parameters string instead of integers

   BotRequest request {};
   static StringRef any = "*";

   request.name = (name.empty () || name == any) ? StringRef ("\0") : name;
   request.difficulty = (difficulty.empty () || difficulty == any) ? -1 : difficulty.int_ ();
   request.team = (team.empty () || team == any) ? -1 : team.int_ ();
   request.skin = (skin.empty () || skin == any) ? -1 : skin.int_ ();
   request.personality = (personality.empty () || personality == any) ? -1 : personality.int_ ();
   request.manual = manual;

   addbot (request.name, request.difficulty, request.personality, request.team, request.skin, request.manual);
}

void BotManager::maintainQuota () {
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (graph.length () < 1 || graph.hasChanged ()) {
      if (cv_quota.int_ () > 0) {
         ctrl.msg ("There is no graph found. Cannot create bot.");
      }
      cv_quota.set (0);
      return;
   }

   // bot's creation update
   if (!m_addRequests.empty () && m_maintainTime < game.time ()) {
      const BotRequest &request = m_addRequests.popFront ();
      const BotCreateResult createResult = create (request.name, request.difficulty, request.personality, request.team, request.skin);

      if (request.manual) {
         cv_quota.set (cv_quota.int_ () + 1);
      }

      // check the result of creation
      if (createResult == BotCreateResult::GraphError) {
         m_addRequests.clear (); // something wrong with graph, reset tab of creation
         cv_quota.set (0); // reset quota
      }
      else if (createResult == BotCreateResult::MaxPlayersReached) {
         m_addRequests.clear (); // maximum players reached, so set quota to maximum players
         cv_quota.set (getBotCount ());
      }
      else if (createResult == BotCreateResult::TeamStacked) {
         ctrl.msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round)");

         m_addRequests.clear ();
         cv_quota.set (getBotCount ());
      }
      m_maintainTime = game.time () + cv_quota_adding_interval.float_ ();
   }

   // now keep bot number up to date
   if (m_quotaMaintainTime > game.time ()) {
      return;
   }
   cv_quota.set (cr::clamp <int> (cv_quota.int_ (), 0, game.maxClients ()));

   int totalHumansInGame = getHumansCount ();
   int humanPlayersInGame = getHumansCount (true);

   if (!game.isDedicated () && !totalHumansInGame) {
      return;
   }

   int desiredBotCount = cv_quota.int_ ();
   int botsInGame = getBotCount ();

   if (strings.matches (cv_quota_mode.str (), "fill")) {
      botsInGame += humanPlayersInGame;
   }
   else if (strings.matches (cv_quota_mode.str (), "match")) {
      int detectQuotaMatch = cv_quota_match.int_ () == 0 ? cv_quota.int_ () : cv_quota_match.int_ ();

      desiredBotCount = cr::max <int> (0, detectQuotaMatch * humanPlayersInGame);
   }

   if (cv_join_after_player.bool_ () && humanPlayersInGame == 0) {
      desiredBotCount = 0;
   }
   int maxClients = game.maxClients ();

   if (cv_autovacate.bool_ ()) {
      if (cv_kick_after_player_connect.bool_ ()) {
         desiredBotCount = cr::min <int> (desiredBotCount, maxClients - (totalHumansInGame + cv_autovacate_keep_slots.int_ ()));
      }
      else {
         desiredBotCount = cr::min <int> (desiredBotCount, maxClients - (humanPlayersInGame + cv_autovacate_keep_slots.int_ ()));
      }
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
      const auto &tp = countTeamPlayers ();
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
   else {
      // clear the saved names when quota balancing ended
      if (cv_save_bots_names.bool_ () && !m_saveBotNames.empty ()) {
         m_saveBotNames.clear ();
      }
   }
   m_quotaMaintainTime = game.time () + cv_quota_maintain_interval.float_ ();
}

void BotManager::maintainLeaders () {

   // select leader each team somewhere in round start
   if (m_timeRoundStart + 5.0f > game.time () && m_timeRoundStart + 10.0f < game.time ()) {
      for (int team = 0; team < kGameTeamNum; ++team) {
         selectLeaders (team, false);
      }
   }
}

void BotManager::maintainAutoKill () {
   const float killDelay = cv_autokill_delay.float_ ();

   if (killDelay < 1.0f || m_roundOver) {
      return;
   }

   // check if we're reached the delay, so kill out bots
   if (!cr::fzero (m_autoKillCheckTime) && m_autoKillCheckTime < game.time ()) {
      killAllBots ();
      m_autoKillCheckTime = 0.0f;

      return;
   }

   int aliveBots = 0;

   // do not interrupt bomb-defuse scenario
   if (game.mapIs (MapFlags::Demolition) && isBombPlanted ()) {
      return;
   }
   int totalHumans = getHumansCount (true); // we're ignore spectators intentionally 

   // if we're have no humans in teams do not bother to proceed
   if (!totalHumans) {
      return;
   }

   for (const auto &bot : m_bots) {
      if (bot->m_notKilled) {
         ++aliveBots;

         // do not interrupt assassination scenario, if vip is a bot
         if (game.is (MapFlags::Assassination) && util.isPlayerVIP (bot->ent ())) {
            return;
         }
      }
   }
   int aliveHumans = getAliveHumansCount ();

   // check if we're have no alive players and some alive bots, and start autokill timer
   if (!aliveHumans && aliveBots > 0 && cr::fzero (m_autoKillCheckTime)) {
      m_autoKillCheckTime = game.time () + killDelay;
   }
}

void BotManager::reset () {
   m_grenadeUpdateTime = 0.0f;
   m_entityUpdateTime = 0.0f;
   m_plantSearchUpdateTime = 0.0f;
   m_lastChatTime = 0.0f;
   m_timeBombPlanted = 0.0f;
   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;

   m_interestingEntities.clear ();
   m_activeGrenades.clear ();
}

void BotManager::initFilters () {
   // table with all available actions for the bots (filtered in & out in bot::setconditions) some of them have subactions included

   m_filters.emplace (&Bot::normal_, Task::Normal, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (&Bot::pause_, Task::Pause, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::moveToPos_, Task::MoveToPosition, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (&Bot::followUser_, Task::FollowUser, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (&Bot::pickupItem_, Task::PickupItem, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (&Bot::camp_, Task::Camp, 0.0f, kInvalidNodeIndex, 0.0f, true);
   m_filters.emplace (&Bot::plantBomb_, Task::PlantBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::defuseBomb_, Task::DefuseBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::attackEnemy_, Task::Attack, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::huntEnemy_, Task::Hunt, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::seekCover_, Task::SeekCover, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::throwExplosive_, Task::ThrowExplosive, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::throwFlashbang_, Task::ThrowFlashbang, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::throwSmoke_, Task::ThrowSmoke, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::doublejump_, Task::DoubleJump, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::escapeFromBomb_, Task::EscapeFromBomb, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::shootBreakable_, Task::ShootBreakable, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::hide_, Task::Hide, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::blind_, Task::Blind, 0.0f, kInvalidNodeIndex, 0.0f, false);
   m_filters.emplace (&Bot::spraypaint_, Task::Spraypaint, 0.0f, kInvalidNodeIndex, 0.0f, false);
}

void BotManager::resetFilters () {
   for (auto &task : m_filters) {
      task.time = 0.0f;
   }
}

void BotManager::decrementQuota (int by) {
   if (by != 0) {
      cv_quota.set (cr::clamp <int> (cv_quota.int_ () - by, 0, cv_quota.int_ ()));
      return;
   }
   cv_quota.set (0);
}

void BotManager::initQuota () {
   m_maintainTime = game.time () + cv_join_delay.float_ ();
   m_quotaMaintainTime = game.time () + cv_join_delay.float_ ();

   m_addRequests.clear ();
}

void BotManager::serverFill (int selection, int personality, int difficulty, int numToAdd) {
   // this function fill server with bots, with specified team & personality

   // always keep one slot
   int maxClients = cv_autovacate.bool_ () ? game.maxClients () - cv_autovacate_keep_slots.int_ () - (game.isDedicated () ? 0 : getHumansCount ()) : game.maxClients ();

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
   char teams[6][12] = { "", {"Terrorists"}, {"CTs"}, "", "", {"Random"}, };
   auto toAdd = numToAdd == -1 ? maxClients - (getHumansCount () + getBotCount ()) : numToAdd;

   for (int i = 0; i <= toAdd; ++i) {
      addbot ("", difficulty, personality, selection, -1, true);
   }
   ctrl.msg ("Fill server with %s bots...", &teams[selection][0]);
}

void BotManager::kickEveryone (bool instant, bool zeroQuota) {
   // this function drops all bot clients from server (this function removes only yapb's)

   if (cv_quota.bool_ () && hasBotsOnline ()) {
      ctrl.msg ("Bots are removed from server.");
   }

   if (zeroQuota) {
      decrementQuota (0);
   }

   // if everyone is kicked, clear the saved bot names
   if (cv_save_bots_names.bool_ () && !m_saveBotNames.empty ()) {
      m_saveBotNames.clear ();
   }

   if (instant) {
      for (const auto &bot : m_bots) {
         bot->kick ();
      }
   }
   m_addRequests.clear ();
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
   ctrl.msg ("All bots died...");
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
   float score = kInfiniteDistance;

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

void BotManager::setLastWinner (int winner) {
   m_lastWinner = winner;
   m_roundOver = true;

   if (cv_radio_mode.int_ () != 2) {
      return;
   }
   auto notify = findAliveBot ();

   if (notify) {
      if (notify->m_team == winner) {
         if (getRoundMidTime () > game.time ()) {
            notify->pushChatterMessage (Chatter::QuickWonRound);
         }
         else {
            notify->pushChatterMessage (Chatter::WonTheRound);
         }
      }
   }
}

void BotManager::checkBotModel (edict_t *ent, char *infobuffer) {
   for (const auto &bot : bots) {
      if (bot->ent () == ent) {
         bot->refreshModelName (infobuffer);
         break;
      }
   }
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
   constexpr char modes[7][12] = { {"Knife"}, {"Pistol"}, {"Shotgun"}, {"Machine Gun"}, {"Rifle"}, {"Sniper"}, {"Standard"} };

   // get the raw weapons array
   auto tab = conf.getRawWeapons ();

   // set the correct weapon mode
   for (int i = 0; i < kNumWeapons; ++i) {
      tab[i].teamStandard = std[selection][i];
      tab[i].teamAS = as[selection][i];
   }
   cv_jasonmode.set (selection == 0 ? 1 : 0);

   ctrl.msg ("%s weapon mode selected.", &modes[selection][0]);
}

void BotManager::listBots () {
   // this function list's bots currently playing on the server

   ctrl.msg ("%-3.5s\t%-19.16s\t%-10.12s\t%-3.4s\t%-3.4s\t%-3.4s\t%-3.5s\t%-3.8s", "index", "name", "personality", "team", "difficulty", "frags", "alive", "timeleft");

   auto botTeam = [] (int32_t team) -> String {
      switch (team) {
      case Team::CT:
         return "CT";

      case Team::Terrorist:
         return "TE";

      case Team::Unassigned:
      default:
         return "UN";

      case Team::Spectator:
         return "SP";
      }
   };

   for (const auto &bot : bots) {
      ctrl.msg ("[%-3.1d]\t%-19.16s\t%-10.12s\t%-3.4s\t%-3.1d\t%-3.1d\t%-3.4s\t%-3.0f secs", bot->index (), bot->pev->netname.chars (), bot->m_personality == Personality::Rusher ? "rusher" : bot->m_personality == Personality::Normal ? "normal" : "careful", botTeam (bot->m_team), bot->m_difficulty, static_cast <int> (bot->pev->frags), bot->m_notKilled ? "yes" : "no", cv_rotate_bots.bool_ () ? bot->m_stayTime - game.time () : 0.0f);
   }
   ctrl.msg ("%d bots", m_bots.length ());
}

float BotManager::getConnectTime (StringRef name, float original) {
   // this function get's fake bot player time.

   for (const auto &bot : m_bots) {
      if (name.startsWith (bot->pev->netname.chars ())) {
         return bot->getConnectionTime ();
      }
   }
   return original;
}

float BotManager::getAverageTeamKPD (bool calcForBots) {
   Twin <float, int32_t> calc {};

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used)) {
         continue;
      }
      auto bot = bots[client.ent];

      if (calcForBots && bot) {
         calc.first += client.ent->v.frags;
         calc.second++;
      }
      else if (!calcForBots && !bot) {
         calc.first += client.ent->v.frags;
         calc.second++;
      }
   }

   if (calc.second > 0) {
      return calc.first / static_cast <float> (cr::max (1, calc.second));
   }
   return 0.0f;
}

Twin <int, int> BotManager::countTeamPlayers () {
   int ts = 0, cts = 0;

   for (const auto &client : util.getClients ()) {
      if (client.flags & ClientFlags::Used) {
         if (client.team2 == Team::Terrorist) {
            ++ts;
         }
         else if (client.team2 == Team::CT) {
            ++cts;
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

   if (setTrue || !cv_economics_rounds.bool_ ()) {
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
            ++numPoorPlayers;
         }
         ++numTeamPlayers; // update count of team
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
   // if min/max difficulty is specified  this should not have effect
   if (cv_difficulty_min.int_ () != Difficulty::Invalid || cv_difficulty_max.int_ () != Difficulty::Invalid || cv_difficulty_auto.bool_ ()) {
      return;
   }
   auto difficulty = cv_difficulty.int_ ();

   if (difficulty != m_lastDifficulty) {

      // sets new difficulty for all bots
      for (const auto &bot : m_bots) {
         bot->m_difficulty = difficulty;
      }
      m_lastDifficulty = difficulty;
   }
}

void BotManager::balanceBotDifficulties () {
   // difficulty chaning once per round (time)
   auto updateDifficulty = [] (Bot *bot, int32_t offset) {
      bot->m_difficulty = cr::clamp (static_cast <Difficulty> (bot->m_difficulty + offset), Difficulty::Noob, Difficulty::Expert);
   };

   if (cv_difficulty_auto.bool_ () && m_difficultyBalanceTime < game.time ()) {
      auto ratioPlayer = getAverageTeamKPD (false);
      auto ratioBots = getAverageTeamKPD (true);

      // calculate for each the bot
      for (auto &bot : m_bots) {
         float score = bot->m_kpdRatio;

         // if kd ratio is going to go to low, we need to try to set higher difficulty
         if (score < 0.8f || (score <= 1.2f && ratioBots < ratioPlayer)) {
            updateDifficulty (bot.get (), +1);
         }
         else if (score > 4.0f || (score >= 2.5f && ratioBots > ratioPlayer)) {
            updateDifficulty (bot.get (), -1);
         }
      }
      m_difficultyBalanceTime = game.time () + cv_difficulty_auto_balance_interval.float_ ();
   }
}

void BotManager::destroy () {
   // this function free all bots slots (used on server shutdown)

   for (auto &bot : m_bots) {
      bot->markStale ();
   }
   m_bots.clear ();
}

Bot::Bot (edict_t *bot, int difficulty, int personality, int team, int skin) {
   // this function does core operation of creating bot, it's called by addbot (),
   // when bot setup completed, (this is a bot class constructor)

   int clientIndex = game.indexOfEntity (bot);
   pev = &bot->v;

   if (bot->pvPrivateData != nullptr) {
      engfuncs.pfnFreeEntPrivateData (bot);
   }

   bot->pvPrivateData = nullptr;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   bots.execGameEntity (bot);

   // set all info buffer keys for this bot
   auto buffer = engfuncs.pfnGetInfoKeyBuffer (bot);
   engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "_vgui_menus", "0");

   if (!game.is (GameFlags::Legacy)) {
      if (cv_show_latency.int_ () == 1) {
         engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*bot", "1");
      }
      auto avatar = conf.getRandomAvatar ();

      if (cv_show_avatars.bool_ () && !avatar.empty ()) {
         engfuncs.pfnSetClientKeyValue (clientIndex, buffer, "*sid", avatar.chars ());
      }
   }

   char reject[256] = { 0, };
   MDLL_ClientConnect (bot, bot->v.netname.chars (), strings.format ("127.0.0.%d", clientIndex + 100), reject);

   if (!strings.isEmpty (reject)) {
      logger.error ("Server refused '%s' connection (%s)", bot->v.netname.chars (), reject);
      game.serverCommand ("kick \"%s\"", bot->v.netname.chars ()); // kick the bot player if the server refused it

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

   if (cv_rotate_bots.bool_ ()) {
      m_stayTime = game.time () + rg.get (cv_rotate_stay_min.float_ (), cv_rotate_stay_max.float_ ());
   }
   else {
      m_stayTime = game.time () + kInfiniteDistance;
   }

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = rg.get (3.8f, 10.0f);
   m_sayTextBuffer.chatProbability = rg.get (10, 100);

   m_notKilled = false;
   m_weaponBurstMode = BurstMode::Off;
   m_difficulty = cr::clamp (static_cast <Difficulty> (difficulty), Difficulty::Noob, Difficulty::Expert);

   auto minDifficulty = cv_difficulty_min.int_ ();
   auto maxDifficulty = cv_difficulty_max.int_ ();

   // if we're have min/max difficulty specified, choose value from they
   if (minDifficulty != Difficulty::Invalid && maxDifficulty != Difficulty::Invalid) {
      if (maxDifficulty > minDifficulty) {
         cr::swap (maxDifficulty, minDifficulty);
      }
      m_difficulty = rg.get (minDifficulty, maxDifficulty);
   }
   m_basePing = rg.get (cv_ping_base_min.int_ (), cv_ping_base_max.int_ ());

   m_lastCommandTime = game.time () - 0.1f;
   m_frameInterval = game.time ();
   m_heavyTimestamp = game.time ();
   m_slowFrameTimestamp = 0.0f;
   m_kpdRatio = 0.0f;

   // stuff from jk_botti
   m_playServerTime = 60.0f * rg.get (30.0f, 240.0f);
   m_joinServerTime = plat.seconds () - m_playServerTime * rg.get (0.2f, 0.8f);

   switch (personality) {
   case 1:
      m_personality = Personality::Rusher;
      m_baseAgressionLevel = rg.get (0.7f, 1.0f);
      m_baseFearLevel = rg.get (0.0f, 0.4f);
      break;

   case 2:
      m_personality = Personality::Careful;
      m_baseAgressionLevel = rg.get (0.2f, 0.5f);
      m_baseFearLevel = rg.get (0.7f, 1.0f);
      break;

   default:
      m_personality = Personality::Normal;
      m_baseAgressionLevel = rg.get (0.4f, 0.7f);
      m_baseFearLevel = rg.get (0.4f, 0.7f);
      break;
   }

   plat.bzero (&m_ammoInClip, sizeof (m_ammoInClip));
   plat.bzero (&m_ammo, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_weaponType = WeaponType::None; // current weapon type is not assigned at start

   m_voicePitch = rg.get (80, 115); // assign voice pitch

   // copy them over to the temp level variables
   m_agressionLevel = m_baseAgressionLevel;
   m_fearLevel = m_baseFearLevel;
   m_nextEmotionUpdate = game.time () + 0.5f;
   m_healthValue = bot->v.health;

   // just to be sure
   m_msgQueue.clear ();

   // init path walker
   m_pathWalk.init (graph.getMaxRouteLength ());

   // init async planner
   m_planner = cr::makeUnique <AStarAlgo> (graph.length ());

   // bot is not kicked by rotation
   m_kickedByRotation = false;

   // assign team and class
   m_wantedTeam = team;
   m_wantedSkin = skin;

   m_tasks.reserve (Task::Max);

   newRound ();
}

float Bot::getConnectionTime () {
   auto current = plat.seconds ();

   if (current - m_joinServerTime > m_playServerTime || current - m_joinServerTime <= 0.0f) {
      m_playServerTime = 60.0f * rg.get (30.0f, 240.0f);
      m_joinServerTime = current - m_playServerTime * rg.get (0.2f, 0.8f);
   }
   return current - m_joinServerTime;
}

int BotManager::getHumansCount (bool ignoreSpectators) {
   // this function returns number of humans playing on the server

   int count = 0;

   for (const auto &client : util.getClients ()) {
      if ((client.flags & ClientFlags::Used) && !bots[client.ent] && !(client.ent->v.flags & FL_FAKECLIENT)) {
         if (ignoreSpectators && client.team2 != Team::Terrorist && client.team2 != Team::CT) {
            continue;
         }
         ++count;
      }
   }
   return count;
}

int BotManager::getAliveHumansCount () {
   // this function returns number of humans playing on the server

   int count = 0;

   for (const auto &client : util.getClients ()) {
      if ((client.flags & ClientFlags::Alive) && bots[client.ent] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT)) {
         ++count;
      }
   }
   return count;
}

int BotManager::getPlayerPriority (edict_t *ent) {
   constexpr auto highPrio = 512;

   // always check for only our own bots
   auto bot = bots[ent];

   // if player just return high prio
   if (!bot) {
      return game.indexOfEntity (ent) + highPrio;
   }

   // give bots some priority
   if (bot->m_hasC4 || bot->m_isVIP || bot->m_hasHostage || bot->m_healthValue < ent->v.health) {
      return bot->entindex () + highPrio;
   }
   auto task = bot->getCurrentTaskId ();

   // higher priority if camping or hiding
   if (task == Task::Camp || task == Task::Hide) {
      return bot->entindex () + highPrio;
   }
   return bot->entindex ();
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
         ++teamCount[client.team2];
      }
   }
   return teamCount[team] + 1 > teamCount[team == Team::CT ? Team::Terrorist : Team::CT] + limitTeams;
}

void BotManager::erase (Bot *bot) {
   for (auto &e : m_bots) {
      if (e.get () != bot) {
         continue;
      }

      if (!bot->m_kickedByRotation && cv_save_bots_names.bool_ ()) {
         m_saveBotNames.emplaceLast (bot->pev->netname.str ());
      }
      bot->markStale ();

      auto index = m_bots.index (e);
      e.reset ();

      m_bots.erase (index, 1); // remove from bots array
      break;
   }
}

void BotManager::handleDeath (edict_t *killer, edict_t *victim) {
   auto killerTeam = game.getTeam (killer);
   auto victimTeam = game.getTeam (victim);

   if (cv_radio_mode.int_ () == 2) {
      // need to send congrats on well placed shot
      for (const auto &notify : bots) {
         if (notify->m_notKilled && killerTeam == notify->m_team && killerTeam != victimTeam && killer != notify->ent () && notify->seesEntity (victim->v.origin)) {
            if (!(killer->v.flags & FL_FAKECLIENT)) {
               notify->pushChatterMessage (Chatter::NiceShotCommander);
            }
            else {
               notify->pushChatterMessage (Chatter::NiceShotPall);
            }
            break;
         }
      }
   }
   Bot *killerBot = nullptr;
   Bot *victimBot = nullptr;

   // notice nearby to victim teammates, that attacker is near
   for (const auto &notify : bots) {
      if (notify->m_seeEnemyTime + 2.0f < game.time () && notify->m_notKilled && notify->m_team == victimTeam && game.isNullEntity (notify->m_enemy) && killerTeam != victimTeam && util.isVisible (killer->v.origin, notify->ent ())) {
         notify->m_actualReactionTime = 0.0f;
         notify->m_seeEnemyTime = game.time ();
         notify->m_enemy = killer;
         notify->m_lastEnemy = killer;
         notify->m_lastEnemyOrigin = killer->v.origin;
      }

      if (notify->ent () == killer) {
         killerBot = notify.get ();
      }
      else if (notify->ent () == victim) {
         victimBot = notify.get ();
      }
   }

   // is this message about a bot who killed somebody?
   if (killerBot != nullptr) {
      killerBot->m_lastVictim = victim;
   }

   // did a human kill a bot on his team?
   else {
      if (victimBot != nullptr) {
         if (killerTeam == victimBot->m_team) {
            victimBot->m_voteKickIndex = game.indexOfEntity (killer);
            for (const auto &notify : bots) {
               if (notify->seesEntity (victim->v.origin)) {
                  notify->pushChatterMessage (Chatter::TeamKill);
               }
            }
         }
         victimBot->m_notKilled = false;
      }
   }
}


void Bot::newRound () {
   // this function initializes a bot after creation & at the start of each round

   // delete all allocated path nodes
   clearSearchNodes ();

   m_pathOrigin = nullptr;
   m_destOrigin = nullptr;

   m_path = nullptr;
   m_currentTravelFlags = 0;
   m_desiredVelocity = nullptr;
   m_currentNodeIndex = kInvalidNodeIndex;
   m_prevGoalIndex = kInvalidNodeIndex;
   m_chosenGoalIndex = kInvalidNodeIndex;
   m_loosedBombNodeIndex = kInvalidNodeIndex;
   m_plantedBombNodeIndex = kInvalidNodeIndex;

   m_grenadeRequested = false;
   m_moveToC4 = false;
   m_defuseNotified = false;
   m_duckDefuse = false;
   m_duckDefuseCheckTime = 0.0f;

   m_numFriendsLeft = 0;
   m_numEnemiesLeft = 0;
   m_oldButtons = pev->button;

   for (auto &node : m_previousNodes) {
      node = kInvalidNodeIndex;
   }
   m_navTimeset = game.time ();
   m_team = game.getTeam (ent ());

   resetPathSearchType ();

   // clear all states & tasks
   m_states = 0;
   clearTasks ();

   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;
   m_preventFlashing = 0.0f;

   m_timeTeamOrder = 0.0f;
   m_askCheckTime = rg.get (30.0f, 90.0f);
   m_minSpeed = 260.0f;
   m_prevSpeed = 0.0f;
   m_prevVelocity = 0.0f;
   m_prevOrigin = Vector (kInfiniteDistance, kInfiniteDistance, kInfiniteDistance);
   m_prevTime = game.time ();
   m_lookUpdateTime = game.time ();
   m_changeViewTime = game.time () + (rg.chance (25) ? mp_freezetime.float_ () : 0.0f);
   m_aimErrorTime = game.time ();

   m_viewDistance = 4096.0f;
   m_maxViewDistance = 4096.0f;

   m_liftEntity = nullptr;
   m_pickupItem = nullptr;
   m_itemIgnore = nullptr;
   m_itemCheckTime = 0.0f;

   m_breakableEntity = nullptr;
   m_breakableOrigin = nullptr;
   m_lastBreakable = nullptr;

   m_timeDoorOpen = 0.0f;

   resetCollision ();
   resetDoubleJump ();

   m_enemy = nullptr;
   m_lastVictim = nullptr;
   m_lastEnemy = nullptr;
   m_lastEnemyOrigin = nullptr;
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
   m_breakableTime = 0.0f;

   m_avoidGrenade = nullptr;
   m_needAvoidGrenade = 0;

   m_lastDamageType = -1;
   m_voteKickIndex = 0;
   m_lastVoteKick = 0;
   m_voteMap = 0;
   m_tryOpenDoor = 0;
   m_aimFlags = 0;
   m_liftState = 0;

   m_aimLastError = nullptr;
   m_position = nullptr;
   m_liftTravelPos = nullptr;

   setIdealReactionTimers (true);

   m_targetEntity = nullptr;
   m_followWaitTime = 0.0f;

   m_hostages.clear ();

   for (auto &timer : m_chatterTimes) {
      timer = kMaxChatterRepeatInterval;
   }
   refreshModelName (nullptr);

   m_isReloading = false;
   m_reloadState = Reload::None;

   m_reloadCheckTime = 0.0f;
   m_shootTime = game.time ();
   m_playerTargetTime = game.time ();
   m_firePause = 0.0f;
   m_timeLastFired = 0.0f;

   m_sniperStopTime = 0.0f;
   m_grenadeCheckTime = 0.0f;
   m_isUsingGrenade = false;
   m_bombSearchOverridden = false;

   m_blindButton = 0;
   m_blindTime = 0.0f;
   m_jumpTime = 0.0f;
   m_jumpFinished = false;
   m_isStuck = false;

   m_sayTextBuffer.timeNextChat = game.time ();
   m_sayTextBuffer.entityIndex = -1;
   m_sayTextBuffer.sayText.clear ();

   m_buyState = BuyState::PrimaryWeapon;
   m_lastEquipTime = 0.0f;

   // if bot died, clear all weapon stuff and force buying again
   if (!m_notKilled) {
      plat.bzero (&m_ammoInClip, sizeof (m_ammoInClip));
      plat.bzero (&m_ammo, sizeof (m_ammo));

      m_currentWeapon = 0;
      m_weaponType = 0;
   }
   m_flashLevel = 100;
   m_checkDarkTime = game.time ();

   m_knifeAttackTime = game.time () + rg.get (1.3f, 2.6f);
   m_nextBuyTime = game.time () + rg.get (0.6f, 2.0f);

   m_buyPending = false;
   m_inBombZone = false;
   m_ignoreBuyDelay = false;
   m_hasC4 = false;
   m_hasHostage = false;

   m_fallDownTime = 0.0f;
   m_shieldCheckTime = 0.0f;
   m_zoomCheckTime = 0.0f;
   m_strafeSetTime = 0.0f;
   m_combatStrafeDir = Dodge::None;
   m_fightStyle = Fight::None;
   m_lastFightStyleCheck = 0.0f;
   m_stuckTimestamp = 0.0f;

   m_checkWeaponSwitch = true;
   m_checkKnifeSwitch = true;
   m_buyingFinished = false;

   m_radioEntity = nullptr;
   m_radioOrder = 0;
   m_defendedBomb = false;
   m_defendHostage = false;
   m_headedTime = 0.0f;

   m_timeLogoSpray = game.time () + rg.get (5.0f, 30.0f);
   m_spawnTime = game.time ();
   m_lastChatTime = game.time ();

   m_timeCamping = 0.0f;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = game.time ();

   m_msgQueue.clear ();
   m_ignoredBreakable.clear ();

   // and put buying into its message queue
   pushMsgQueue (BotMsg::Buy);
   startTask (Task::Normal, TaskPri::Normal, kInvalidNodeIndex, 0.0f, true);

   // restore fake client bit, just in case
   pev->flags |= FL_FAKECLIENT;

   if (rg.chance (50)) {
      pushChatterMessage (Chatter::NewRound);
   }
   auto interval = cr::clamp (cv_think_fps.float_ (), 24.0f, 90.0f);

   if (game.is (GameFlags::Xash3D) && interval < 50.0f) {
      interval = 50.0f; // xash works acceptable at 50fps
   }
   m_updateInterval = game.is (GameFlags::Legacy) ? 0.0f : 1.0f / interval;
}

void Bot::resetPathSearchType () {
   const auto morale = m_fearLevel > m_agressionLevel ? rg.chance (30) : rg.chance (70);

   switch (m_personality) {
   default:
   case Personality::Normal:
      m_pathType = morale ? FindPath::Optimal : FindPath::Fast;
      break;

   case Personality::Rusher:
      m_pathType = morale ? FindPath::Fast : FindPath::Optimal;
      break;

   case Personality::Careful:
      m_pathType = morale ? FindPath::Optimal : FindPath::Safe;
      break;
   }
}

void Bot::kill () {
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   bots.touchKillerEntity (this);
}

void Bot::kick () {
   // this function kick off one bot from the server.
   auto username = pev->netname.chars ();

   if (!(pev->flags & FL_CLIENT) || strings.isEmpty (username)) {
      return;
   }
   markStale ();

   game.serverCommand ("kick \"%s\"", username);
   ctrl.msg ("Bot '%s' kicked.", username);
}

void Bot::markStale () {
   // switch chatter icon off
   showChatterIcon (false, true);

   // mark bot as leaving
   m_isStale = true;

   // clear the bot name
   conf.clearUsedName (this);

   // clear fakeclient bit
   pev->flags &= ~FL_FAKECLIENT;

   // make as not receiveing any messages
   pev->flags |= FL_DORMANT;
}

void Bot::updateTeamJoin () {
   // this function handles the selection of teams & class

   if (!m_notStarted) {
      return;
   }

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
   if (m_startAction == BotMsg::None) {
      if (++m_retryJoin > 3 && bots.isTeamStacked (m_wantedTeam - 1)) {
         m_retryJoin = 0;

         ctrl.msg ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).");
         kick ();

         return;
      }
   }

   // handle counter-strike stuff here...
   if (m_startAction == BotMsg::TeamSelect) {
      m_startAction = BotMsg::None; // switch back to idle

      if (m_wantedTeam == -1) {
         char teamJoin = cv_join_team.str ()[0];

         if (teamJoin == 'C' || teamJoin == 'c') {
            m_wantedTeam = 2;
         }
         else if (teamJoin == 'T' || teamJoin == 't') {
            m_wantedTeam = 1;
         }
      }

      if (m_wantedTeam != 1 && m_wantedTeam != 2) {
         const auto &players = bots.countTeamPlayers ();

         // balance the team upon creation, we can't use game auto select (5) from now, as we use enforced skins belows
         // due to we don't know the team bot selected, and TeamInfo messages still shows us we're spectators..

         if (players.first > players.second) {
            m_wantedTeam = 2;
         }
         else if (players.first < players.second) {
            m_wantedTeam = 1;
         }
         else {
            m_wantedTeam = rg.get (1, 2);
         }
      }

      // select the team the bot wishes to join...
      issueCommand ("menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == BotMsg::ClassSelect) {
      m_startAction = BotMsg::None; // switch back to idle

      // czero has additional models
      auto maxChoice = game.is (GameFlags::ConditionZero) ? 5 : 4;
      auto enforcedSkin = 0;

      // setup enforced skin based on selected team
      if (m_wantedTeam == 1 || m_team == Team::Terrorist) {
         enforcedSkin = cv_botskin_t.int_ ();
      }
      else if (m_wantedTeam == 2 || m_team == Team::CT) {
         enforcedSkin = cv_botskin_ct.int_ ();
      }
      enforcedSkin = cr::clamp (enforcedSkin, 0, maxChoice);

      // try to choice manually
      if (m_wantedSkin < 1 || m_wantedSkin > maxChoice) {
         m_wantedSkin = rg.get (1, maxChoice); // use random if invalid
      }

      // and set enforced if any
      if (enforcedSkin > 0) {
         m_wantedSkin = enforcedSkin;
      }

      // select the class the bot wishes to use...
      issueCommand ("menuselect %d", m_wantedSkin);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (rg.chance (20)) {
         m_needToSendWelcomeChat = true;
      }
   }
}

void BotManager::captureChatRadio (const char *cmd, const char *arg, edict_t *ent) {
   if (game.isBotCmd ()) {
      return;
   }

   if (strings.matches (cmd, "say") || strings.matches (cmd, "say_team")) {
      bool alive = util.isAlive (ent);
      int team = -1;

      if (cr::strcmp (cmd, "say_team") == 0) {
         team = game.getTeam (ent);
      }

      for (const auto &client : util.getClients ()) {
         if (!(client.flags & ClientFlags::Used) || (team != -1 && team != client.team2) || alive != util.isAlive (client.ent)) {
            continue;
         }
         auto target = bots[client.ent];

         if (target != nullptr) {
            target->m_sayTextBuffer.entityIndex = game.indexOfPlayer (ent);

            if (strings.isEmpty (engfuncs.pfnCmd_Args ())) {
               continue;
            }
            target->m_sayTextBuffer.sayText = engfuncs.pfnCmd_Args ();
            target->m_sayTextBuffer.timeNextChat = game.time () + target->m_sayTextBuffer.chatDelay;
         }
      }
   }
   auto &target = util.getClient (game.indexOfPlayer (ent));

   // check if this player alive, and issue something
   if ((target.flags & ClientFlags::Alive) && target.radio != 0 && cr::strncmp (cmd, "menuselect", 10) == 0) {
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
         bots.setLastRadioTimestamp (target.team, game.time ());
      }
      target.radio = 0;
   }
   else if (cr::strncmp (cmd, "radio", 5) == 0) {
      target.radio = atoi (&cmd[5]);
   }
}

void BotManager::notifyBombDefuse () {
   // notify all terrorists that CT is starting bomb defusing

   const auto &bombPos = graph.getBombOrigin ();

   for (const auto &bot : bots) {
      auto task = bot->getCurrentTaskId ();

      if (!bot->m_defuseNotified && bot->m_notKilled && task != Task::MoveToPosition && task != Task::DefuseBomb && task != Task::EscapeFromBomb) {
         if (bot->m_team == Team::Terrorist && bot->pev->origin.distanceSq (bombPos) < cr::sqrf (384.0f)) {
            bot->clearSearchNodes ();

            bot->m_pathType = FindPath::Fast;
            bot->m_position = bombPos;
            bot->m_defuseNotified = true;

            bot->startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);
         }
      }
   }
}

void BotManager::updateActiveGrenade () {
   if (m_grenadeUpdateTime > game.time ()) {
      return;
   }
   m_activeGrenades.clear (); // clear previously stored grenades

   // need to ignore bomb model in active grenades...
   auto bombModel = conf.getBombModelName ();

   // search the map for any type of grenade
   game.searchEntities ("classname", "grenade", [&] (edict_t *e) {
      // do not count c4 as a grenade
      if (!util.isModel (e, bombModel)) {
         m_activeGrenades.push (e);
      }
      return EntitySearchResult::Continue; // continue iteration
   });
   m_grenadeUpdateTime = game.time () + 0.25f;
}

void BotManager::updateInterestingEntities () {
   if (m_entityUpdateTime > game.time ()) {
      return;
   }

   // clear previously stored entities
   m_interestingEntities.clear ();

   // search the map for any type of grenade
   game.searchEntities (nullptr, kInfiniteDistance, [&] (edict_t *e) {
      auto classname = e->v.classname.chars ();

      // search for grenades, weaponboxes, weapons, items and armoury entities
      if (cr::strncmp ("weaponbox", classname, 9) == 0 || cr::strncmp ("grenade", classname, 7) == 0 || util.isItem (e) || cr::strncmp ("armoury", classname, 7) == 0) {
         m_interestingEntities.push (e);
      }

      // pickup some csdm stuff if we're running csdm
      if (game.mapIs (MapFlags::HostageRescue) && cr::strncmp ("hostage", classname, 7) == 0) {
         m_interestingEntities.push (e);
      }

      // add buttons
      if (game.mapIs (MapFlags::HasButtons) && cr::strncmp ("func_button", classname, 11) == 0) {
         m_interestingEntities.push (e);
      }

      // pickup some csdm stuff if we're running csdm
      if (game.is (GameFlags::CSDM) && cr::strncmp ("csdm", classname, 4) == 0) {
         m_interestingEntities.push (e);
      }

      if (cv_attack_monsters.bool_ () && util.isMonster (e)) {
         m_interestingEntities.push (e);
      }

      // continue iteration
      return EntitySearchResult::Continue;
   });
   m_entityUpdateTime = game.time () + 0.5f;
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
                  if (cv_radio_mode.int_ () == 2) {
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
   else if (game.mapIs (MapFlags::Escape | MapFlags::KnifeArena | MapFlags::FightYard)) {
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

   m_roundOver = false;

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

   graph.setBombOrigin (true);
   graph.clearVisited ();

   m_bombSayStatus = BombPlantedSay::ChatSay | BombPlantedSay::Chatter;
   m_timeBombPlanted = 0.0f;
   m_plantSearchUpdateTime = 0.0f;
   m_autoKillCheckTime = 0.0f;
   m_botsCanPause = false;

   resetFilters ();
   practice.update (); // update practice data on round start

   // calculate the round mid/end in world time
   m_timeRoundStart = game.time () + mp_freezetime.float_ ();
   m_timeRoundMid = m_timeRoundStart + mp_roundtime.float_ () * 60.0f * 0.5f;
   m_timeRoundEnd = m_timeRoundStart + mp_roundtime.float_ () * 60.0f;
}

void BotManager::setBombPlanted (bool isPlanted) {
   if (cv_ignore_objectives.bool_ ()) {
      m_bombPlanted = false;
      return;
   }

   if (isPlanted) {
      m_timeBombPlanted = game.time ();
   }
   m_bombPlanted = isPlanted;
}

void BotThreadWorker::shutdown () {
   game.print ("Shutting down bot thread worker.");
   m_botWorker.shutdown ();
}

void BotThreadWorker::startup (int workers) {
   size_t count = m_botWorker.threadCount ();

   if (count > 0) {
      logger.error ("Tried to start thread pool with existing %d threads in pool.", count);
      return;
   }

   int requestedThreadCount = workers;
   int hardwareConcurrency = plat.hardwareConcurrency ();

   if (requestedThreadCount == -1) {
      requestedThreadCount = hardwareConcurrency / 4;

      if (requestedThreadCount == 0) {
         requestedThreadCount = 1;
      }
   }
   requestedThreadCount = cr::clamp (requestedThreadCount, 1, hardwareConcurrency - 1);

   // notify user
   game.print ("Starting up bot thread worker with %d threads.", requestedThreadCount);

   // start up the worker
   m_botWorker.startup (static_cast <size_t> (requestedThreadCount));
}
