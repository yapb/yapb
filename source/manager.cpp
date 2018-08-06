//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_autovacate ("yb_autovacate", "1");

ConVar yb_quota ("yb_quota", "0", VT_NORMAL);
ConVar yb_quota_mode ("yb_quota_mode", "normal");
ConVar yb_quota_match ("yb_quota_match", "0");

ConVar yb_join_after_player ("yb_join_after_player", "0");
ConVar yb_join_team ("yb_join_team", "any");

ConVar yb_name_prefix ("yb_name_prefix", "");
ConVar yb_difficulty ("yb_difficulty", "4");

ConVar yb_latency_display ("yb_latency_display", "2");
ConVar yb_avatar_display ("yb_avatar_display", "1");

ConVar mp_limitteams ("mp_limitteams", nullptr, VT_NOREGISTER);

BotManager::BotManager (void)
{
   // this is a bot manager class constructor

   m_lastWinner = -1;
   m_deathMsgSent = false;

   for (int i = 0; i < MAX_TEAM_COUNT; i++)
   {
      m_leaderChoosen[i] = false;
      m_economicsGood[i] = true;
   }
   memset (m_bots, 0, sizeof (m_bots));

   m_maintainTime = 0.0f;
   m_quotaMaintainTime = 0.0f;
   m_grenadeUpdateTime = 0.0f;

   m_creationTab.RemoveAll ();
   m_killerEntity = nullptr;
}

BotManager::~BotManager (void)
{
   // this is a bot manager class destructor, do not use engine.MaxClients () here !!
   Free ();
}

void BotManager::CreateKillerEntity (void)
{
   // this function creates single trigger_hurt for using in Bot::Kill, to reduce lags, when killing all the bots

   m_killerEntity = g_engfuncs.pfnCreateNamedEntity (MAKE_STRING ("trigger_hurt"));

   m_killerEntity->v.dmg = 9999.0f;
   m_killerEntity->v.dmg_take = 1.0f;
   m_killerEntity->v.dmgtime = 2.0f;
   m_killerEntity->v.effects |= EF_NODRAW;

   g_engfuncs.pfnSetOrigin (m_killerEntity, Vector (-99999.0f, -99999.0f, -99999.0f));
   MDLL_Spawn (m_killerEntity);
}

void BotManager::DestroyKillerEntity (void)
{
   if (!engine.IsNullEntity (m_killerEntity))
      g_engfuncs.pfnRemoveEntity (m_killerEntity);
}

void BotManager::TouchWithKillerEntity (Bot *bot)
{
   if (engine.IsNullEntity (m_killerEntity))
   {
      CreateKillerEntity ();

      if (engine.IsNullEntity (m_killerEntity))
         MDLL_ClientKill (bot->GetEntity ());

      return;
   }

   m_killerEntity->v.classname = MAKE_STRING (g_weaponDefs[bot->m_currentWeapon].className);
   m_killerEntity->v.dmg_inflictor = bot->GetEntity ();

   KeyValueData kv;
   kv.szClassName = const_cast <char *> (g_weaponDefs[bot->m_currentWeapon].className);
   kv.szKeyName = "damagetype";
   kv.szValue = const_cast <char *> (FormatBuffer ("%d", (1 << 4)));
   kv.fHandled = FALSE;

   MDLL_KeyValue (m_killerEntity, &kv);
   MDLL_Touch (m_killerEntity, bot->GetEntity ());
}

// it's already defined in interface.cpp
extern "C" void player (entvars_t *pev);

void BotManager::CallGameEntity (entvars_t *vars)
{
   // this function calls gamedll player() function, in case to create player entity in game

   if (g_gameFlags & GAME_METAMOD)
   {
      CALL_GAME_ENTITY (PLID, "player", vars);
      return;
   }
   player (vars);
}

BotCreationResult BotManager::CreateBot (const String &name, int difficulty, int personality, int team, int member)
{
   // this function completely prepares bot entity (edict) for creation, creates team, difficulty, sets name etc, and
   // then sends result to bot constructor
   
   edict_t *bot = nullptr;
   char outputName[33];

   if (g_numWaypoints < 1) // don't allow creating bots with no waypoints loaded
   {
      engine.CenterPrintf ("Map is not waypointed. Cannot create bot");
      return BOT_RESULT_NAV_ERROR;
   }
   else if (waypoints.HasChanged ()) // don't allow creating bots with changed waypoints (distance tables are messed up)
   {
      engine.CenterPrintf ("Waypoints have been changed. Load waypoints again...");
      return BOT_RESULT_NAV_ERROR;
   }
   else if (team != -1 && IsTeamStacked (team - 1))
   {
      engine.CenterPrintf ("Desired team is stacked. Unable to proceed with bot creation");
      return BOT_RESULT_TEAM_STACKED;
   }

   if (difficulty < 0 || difficulty > 4)
   {
      difficulty = yb_difficulty.GetInt ();

      if (difficulty < 0 || difficulty > 4)
      {
         difficulty = Random.Int (3, 4);
         yb_difficulty.SetInt (difficulty);
      }
   }

   if (personality < PERSONALITY_NORMAL || personality > PERSONALITY_CAREFUL)
   {
      if (Random.Int (0, 100) < 50)
         personality = PERSONALITY_NORMAL;
      else
      {
         if (Random.Int (0, 100) < 50)
            personality = PERSONALITY_RUSHER;
         else
            personality = PERSONALITY_CAREFUL;
      }
   }

   String steamId = "";
   BotName *botName = nullptr;

   // setup name
   if (name.IsEmpty ())
   {
      if (!g_botNames.IsEmpty ())
      {
         bool nameFound = false;

         FOR_EACH_AE (g_botNames, i)
         {
            if (nameFound)
               break;

            botName = &g_botNames.GetRandomElement ();

            if (botName->name.GetLength () < 3 || botName->usedBy != 0)
               continue;

            nameFound = true;

            strncpy (outputName, botName->name.GetBuffer (), SIZEOF_CHAR (outputName));
            steamId = botName->steamId;
         }
      }
      else
         sprintf (outputName, "bot%i", Random.Int (0, 100)); // just pick ugly random name
   }
   else
      strncpy (outputName, name, 21);

   if (!IsNullString (yb_name_prefix.GetString ()))
   {
      char prefixedName[33]; // temp buffer for storing modified name
      snprintf (prefixedName, SIZEOF_CHAR (prefixedName), "%s %s", yb_name_prefix.GetString (), outputName);

      // buffer has been modified, copy to real name
      if (!IsNullString (prefixedName))
         strncpy (outputName, prefixedName, SIZEOF_CHAR (outputName));
   }
   bot = g_engfuncs.pfnCreateFakeClient (outputName);

   if (engine.IsNullEntity (bot))
   {
      engine.CenterPrintf ("Maximum players reached (%d/%d). Unable to create Bot.", engine.MaxClients (), engine.MaxClients ());
      return BOT_RESULT_MAX_PLAYERS_REACHED;
   }
   int index = engine.IndexOfEntity (bot) - 1;

   // ensure it free
   Free (index);

   InternalAssert (index >= 0 && index <= MAX_ENGINE_PLAYERS); // check index
   InternalAssert (m_bots[index] == nullptr); // check bot slot

   m_bots[index] = new Bot (bot, difficulty, personality, team, member, steamId);

   // assign owner of bot name
   if (botName != nullptr)
      botName->usedBy = m_bots[index]->GetIndex ();

   engine.Printf ("Connecting Bot...");

   return BOT_RESULT_CREATED;
}

int BotManager::GetIndex (edict_t *ent)
{
   // this function returns index of bot (using own bot array)
   if (engine.IsNullEntity (ent))
      return -1;

   int index = engine.IndexOfEntity (ent) - 1;

   if (index < 0 || index >= MAX_ENGINE_PLAYERS)
      return -1;

   if (m_bots[index] != nullptr)
      return index;

   return -1; // if no edict, return -1;
}

Bot *BotManager::GetBot (int index)
{
   // this function finds a bot specified by index, and then returns pointer to it (using own bot array)

   if (index < 0 || index >= MAX_ENGINE_PLAYERS)
      return nullptr;

   if (m_bots[index] != nullptr)
      return m_bots[index];

   return nullptr; // no bot
}

Bot *BotManager::GetBot (edict_t *ent)
{
   // same as above, but using bot entity

   return GetBot (GetIndex (ent));
}

Bot *BotManager::GetAliveBot (void)
{
   // this function finds one bot, alive bot :)

   Array <int> result;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (result.GetSize () > 4)
         break;

      if (m_bots[i] != nullptr && IsAlive (m_bots[i]->GetEntity ()))
         result.Push (i);
   }

   if (!result.IsEmpty ())
      return m_bots[result.GetRandomElement ()];

   return nullptr;
}

void BotManager::Think (void)
{
   // this function calls think () function for all available at call moment bots

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = m_bots[i];

      if (bot != nullptr)
         bot->Think ();
   }
}

void BotManager::PeriodicThink (void)
{
   // this function calls periodic SecondThink () function for all available at call moment bots

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = m_bots[i];

      if (bot != nullptr)
         bot->PeriodicThink ();
   }

   // select leader each team somewhere in round start
   if (g_timeRoundStart + 5.0f > engine.Time () && g_timeRoundStart + 10.0f < engine.Time ())
   {
      for (int team = 0; team < MAX_TEAM_COUNT; team++)
         SelectLeaderEachTeam (team, false);
   }
}

void BotManager::AddBot (const String &name, int difficulty, int personality, int team, int member, bool manual)
{
   // this function putting bot creation process to queue to prevent engine crashes

   CreateQueue bot;

   // fill the holder
   bot.name = name;
   bot.difficulty = difficulty;
   bot.personality = personality;
   bot.team = team;
   bot.member = member;
   bot.manual = manual;

   // put to queue
   m_creationTab.Push (bot);
}

void BotManager::AddBot (const String &name, const String &difficulty, const String &personality, const String &team, const String &member, bool manual)
{
   // this function is same as the function above, but accept as parameters string instead of integers

   CreateQueue bot;
   const String &any = "*";

   bot.name = (name.IsEmpty () || name == any) ? String ("\0") : name;
   bot.difficulty = (difficulty.IsEmpty () || difficulty == any) ? -1 : difficulty.ToInt ();
   bot.team = (team.IsEmpty () || team == any) ? -1 : team.ToInt ();
   bot.member = (member.IsEmpty () || member == any) ? -1 : member.ToInt ();
   bot.personality = (personality.IsEmpty () || personality == any) ? -1 : personality.ToInt ();
   bot.manual = manual;

   m_creationTab.Push (bot);
}

void BotManager::MaintainBotQuota (void)
{
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (g_numWaypoints < 1 || waypoints.HasChanged ())
      return;

   // bot's creation update
   if (!m_creationTab.IsEmpty () && m_maintainTime < engine.Time ())
   {
      const CreateQueue &last = m_creationTab.Pop ();
      const BotCreationResult callResult = CreateBot (last.name, last.difficulty, last.personality, last.team, last.member);

      if (last.manual)
         yb_quota.SetInt (yb_quota.GetInt () + 1);

      // check the result of creation
      if (callResult == BOT_RESULT_NAV_ERROR)
      {
         m_creationTab.RemoveAll (); // something wrong with waypoints, reset tab of creation
         yb_quota.SetInt (0); // reset quota
      }
      else if (callResult == BOT_RESULT_MAX_PLAYERS_REACHED)
      {
         m_creationTab.RemoveAll (); // maximum players reached, so set quota to maximum players
         yb_quota.SetInt (GetBotsNum ());
      }
      else if (callResult == BOT_RESULT_TEAM_STACKED)
      {
         engine.Printf ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round)");

         m_creationTab.RemoveAll ();
         yb_quota.SetInt (GetBotsNum ());
      }
      m_maintainTime = engine.Time () + 0.10f;
   }

   // now keep bot number up to date
   if (m_quotaMaintainTime > engine.Time ())
      return;

   yb_quota.SetInt (A_clamp <int> (yb_quota.GetInt (), 0, engine.MaxClients ()));

   int totalHumansInGame = GetHumansNum ();
   int humanPlayersInGame = GetHumansNum (true);

   if (!engine.IsDedicatedServer () && !totalHumansInGame)
      return;

   int desiredBotCount = yb_quota.GetInt ();
   int botsInGame = GetBotsNum ();

   bool halfRoundPassed = g_gameWelcomeSent && g_timeRoundMid > engine.Time ();

   if (A_stricmp (yb_quota_mode.GetString (), "fill") == 0)
   {
      if (halfRoundPassed)
         desiredBotCount = botsInGame;
      else
         desiredBotCount = A_max <int> (0, desiredBotCount - humanPlayersInGame);
   }
   else if (A_stricmp (yb_quota_mode.GetString (), "match") == 0)
   {
      if (halfRoundPassed)
         desiredBotCount = botsInGame;
      else
      {
         int quotaMatch = yb_quota_match.GetInt () == 0 ? yb_quota.GetInt () : yb_quota_match.GetInt ();
         desiredBotCount = A_max <int> (0, quotaMatch * humanPlayersInGame);
      }
   }

   if (yb_join_after_player.GetBool () && humanPlayersInGame == 0)
       desiredBotCount = 0;

   int maxClients = engine.MaxClients ();

   if (yb_autovacate.GetBool ())
      desiredBotCount = A_min <int> (desiredBotCount, maxClients - (humanPlayersInGame + 1));
   else
      desiredBotCount = A_min <int> (desiredBotCount, maxClients - humanPlayersInGame);

   // add bots if necessary
   if (desiredBotCount > botsInGame)
      AddRandom ();
   else if (desiredBotCount < botsInGame)
      RemoveRandom (false);

   m_quotaMaintainTime = engine.Time () + 0.40f;
}

void BotManager::DecrementQuota (int by)
{
   if (by != 0)
   {
      yb_quota.SetInt (A_clamp <int> (yb_quota.GetInt () - by, 0, yb_quota.GetInt ()));
      return;
   }
   yb_quota.SetInt (0);
}

void BotManager::InitQuota (void)
{
   m_maintainTime = engine.Time () + 3.0f;
   m_quotaMaintainTime = engine.Time () + 3.0f;

   m_trackedPlayers.RemoveAll ();
   m_creationTab.RemoveAll ();
}

void BotManager::FillServer (int selection, int personality, int difficulty, int numToAdd)
{
   // this function fill server with bots, with specified team & personality

   // always keep one slot
   int maxClients = yb_autovacate.GetBool () ? engine.MaxClients () - 1 - (engine.IsDedicatedServer () ? 0 : GetHumansNum ()) : engine.MaxClients ();

   if (GetBotsNum () >= maxClients - GetHumansNum ())
      return;

   if (selection == 1 || selection == 2)
   {
      CVAR_SET_STRING ("mp_limitteams", "0");
      CVAR_SET_STRING ("mp_autoteambalance", "0");
   }
   else
      selection = 5;

   char teams[6][12] =
   {
      "",
      {"Terrorists"},
      {"CTs"},
      "",
      "",
      {"Random"},
   };

   int toAdd = numToAdd == -1 ? maxClients - (GetHumansNum () + GetBotsNum ()) : numToAdd;

   for (int i = 0; i <= toAdd; i++)
      AddBot ("", difficulty, personality, selection, -1, true);

   engine.CenterPrintf ("Fill Server with %s bots...", &teams[selection][0]);
}

void BotManager::RemoveAll (bool instant)
{
   // this function drops all bot clients from server (this function removes only yapb's)`q

   engine.CenterPrintf ("Bots are removed from server.");
   DecrementQuota (0);

   if (instant)
   {
      for (int i = 0; i < engine.MaxClients (); i++)
      {
         auto bot = m_bots[i];

         if (bot != nullptr)
            bot->Kick ();
      }
   }
   m_creationTab.RemoveAll ();
}

void BotManager::RemoveFromTeam (Team team, bool removeAll)
{
   // this function remove random bot from specified team (if removeAll value = 1 then removes all players from team)

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = m_bots[i];

      if (bot != nullptr && team == bot->m_team)
      {
         DecrementQuota ();
         bot->Kick ();

         if (!removeAll)
            break;
      }
   }
}

void BotManager::RemoveMenu (edict_t *ent, int selection)
{
   // this function displays remove bot menu to specified entity (this function show's only yapb's).

   if (selection > 4 || selection < 1)
      return;

   const int MaxMenuLength = 768;

   static char menuBuffer[MaxMenuLength];
   char menuHolder[MaxMenuLength];

   memset (menuBuffer, 0, sizeof (menuBuffer));
   memset (menuHolder, 0, sizeof (menuHolder));

   int menuKeys = (selection == 4) ? (1 << 9) : ((1 << 8) | (1 << 9));
   int menuKey = (selection - 1) * 8;

   for (int i = menuKey; i < selection * 8; i++)
   {
      char menuItem[32];
      auto bot = GetBot (i);

      if (bot != nullptr && (bot->pev->flags & FL_FAKECLIENT))
      {
         menuKeys |= 1 << (i - menuKey);
         snprintf (menuItem, SIZEOF_CHAR (menuItem), "%1.1d. %s%s\n",  i - menuKey + 1, STRING (bot->pev->netname), bot->m_team == TEAM_COUNTER ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else
         snprintf (menuItem, SIZEOF_CHAR (menuItem), "\\d %1.1d. Not a Bot\\w\n", i - menuKey + 1);

      strncat (menuHolder, menuItem, SIZEOF_CHAR (menuHolder));
   }
   snprintf (menuBuffer, SIZEOF_CHAR (menuBuffer), "\\yBots Remove Menu (%d/4):\\w\n\n%s\n%s 0. Back", selection, menuHolder, (selection == 4) ? "" : " 9. More...\n");

   // force to clear current menu
   DisplayMenuToClient (ent, BOT_MENU_INVALID);

   auto FindMenu = [] (MenuId id)
   {
      int menuIndex = 0;

      for (; menuIndex < ARRAYSIZE_HLSDK (g_menus); menuIndex++)
      {
         if (g_menus[menuIndex].id == id)
            break;
      }
      return &g_menus[menuIndex];
   };

   const unsigned int slots = menuKeys & static_cast <unsigned int> (-1);
   MenuId id = static_cast <MenuId> (BOT_MENU_KICK_PAGE_1 - 1 + selection);

   MenuText *menu = FindMenu (id);

   menu->slots = slots;
   menu->text = menuBuffer;

   DisplayMenuToClient (ent, id);
}

void BotManager::KillAll (int team)
{
   // this function kills all bots on server (only this dll controlled bots)

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (m_bots[i] != nullptr)
      {
         if (team != -1 && team != m_bots[i]->m_team)
            continue;

         m_bots[i]->Kill ();
      }
   }
   engine.CenterPrintf ("All Bots died !");
}

void BotManager::RemoveBot (int index)
{
   auto bot = GetBot (index);

   if (bot)
   {
      DecrementQuota ();
      bot->Kick ();
   }
}

void BotManager::RemoveRandom (bool decrementQuota)
{
   // this function removes random bot from server (only yapb's)

   bool deadBotFound = false;

   auto UpdateQuota = [&] (void)
   {
      if (decrementQuota)
         DecrementQuota ();
   };

   // first try to kick the bot that is currently dead
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = bots.GetBot (i);

      if (bot != nullptr && !bot->m_notKilled)  // is this slot used?
      {
         UpdateQuota ();
         bot->Kick ();
         
         deadBotFound = true;
         break;
      }
   }

   if (deadBotFound)
      return;

   // if no dead bots found try to find one with lowest amount of frags
   int index = 0;
   float score = 9999.0f;

   // search bots in this team
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = bots.GetBot (i);

      if (bot != nullptr && bot->pev->frags < score)
      {
         index = i;
         score = bot->pev->frags;
      }
   }

   // if found some bots
   if (index != 0)
   {
      UpdateQuota ();
      m_bots[index]->Kick ();

      return;
   }

   // worst case, just kick some random bot
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (m_bots[i] != nullptr)  // is this slot used?
      {
         UpdateQuota ();
         m_bots[i]->Kick ();
        
         break;
      }
   }
}

void BotManager::SetWeaponMode (int selection)
{
   // this function sets bots weapon mode

   int tabMapStandart[7][NUM_WEAPONS] =
   {
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Knife only
      {-1,-1,-1, 2, 2, 0, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Pistols only
      {-1,-1,-1,-1,-1,-1,-1, 2, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Shotgun only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 1, 2, 0, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2,-1}, // Machine Guns only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0, 1, 0, 1, 1,-1,-1,-1,-1,-1,-1}, // Rifles only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 2, 2, 0, 1,-1,-1}, // Snipers only
      {-1,-1,-1, 2, 2, 0, 1, 2, 2, 2, 1, 2, 0, 2, 0, 0, 1, 0, 1, 1, 2, 2, 0, 1, 2, 1}  // Standard
   };

   int tabMapAS[7][NUM_WEAPONS] =
   {
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Knife only
      {-1,-1,-1, 2, 2, 0, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Pistols only
      {-1,-1,-1,-1,-1,-1,-1, 1, 1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, // Shotgun only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1, 1, 1, 1, 0, 2,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 1,-1}, // Machine Guns only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0,-1, 1, 0, 1, 1,-1,-1,-1,-1,-1,-1}, // Rifles only
      {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1, 0, 0,-1, 1,-1,-1}, // Snipers only
      {-1,-1,-1, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0,-1, 1, 0, 1, 1, 0, 0,-1, 1, 1, 1}  // Standard
   };

   char modeName[7][12] =
   {
      {"Knife"},
      {"Pistol"},
      {"Shotgun"},
      {"Machine Gun"},
      {"Rifle"},
      {"Sniper"},
      {"Standard"}
   };
   selection--;

   for (int i = 0; i < NUM_WEAPONS; i++)
   {
      g_weaponSelect[i].teamStandard = tabMapStandart[selection][i];
      g_weaponSelect[i].teamAS = tabMapAS[selection][i];
   }

   if (selection == 0)
      yb_jasonmode.SetInt (1);
   else
      yb_jasonmode.SetInt (0);

   engine.CenterPrintf ("%s weapon mode selected", &modeName[selection][0]);
}

void BotManager::ListBots (void)
{
   // this function list's bots currently playing on the server

   engine.Printf ("%-3.5s %-9.13s %-17.18s %-3.4s %-3.4s %-3.4s", "index", "name", "personality", "team", "difficulty", "frags");

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = GetBot (i);

      // is this player slot valid
      if (bot != nullptr)
         engine.Printf ("[%-3.1d] %-9.13s %-17.18s %-3.4s %-3.1d %-3.1d", i, STRING (bot->pev->netname), bot->m_personality == PERSONALITY_RUSHER ? "rusher" : bot->m_personality == PERSONALITY_NORMAL ? "normal" : "careful", bot->m_team == TEAM_COUNTER ? "CT" : "T", bot->m_difficulty, static_cast <int> (bot->pev->frags));
   }
}

int BotManager::GetBotsNum (void)
{
   // this function returns number of yapb's playing on the server

   int count = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (m_bots[i] != nullptr)
         count++;
   }
   return count;
}

Bot *BotManager::GetHighestFragsBot (int team)
{
   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = bots.GetBot (i);

      if (bot != nullptr && bot->m_notKilled && bot->m_team == team)
      {
         if (bot->pev->frags > bestScore)
         {
            bestIndex = i;
            bestScore = bot->pev->frags;
         }
      }
   }
   return GetBot (bestIndex);
}

void BotManager::CheckTeamEconomics (int team, bool forceGoodEconomics)
{
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar yb_economics_rounds;

   if (forceGoodEconomics || !yb_economics_rounds.GetBool ())
   {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      auto bot = m_bots[i];

      if (bot != nullptr && bot->m_team == team)
      {
         if (bot->m_moneyAmount <= g_botBuyEconomyTable[0])
            numPoorPlayers++;

         numTeamPlayers++; // update count of team
      }
   }
   m_economicsGood[team] = true;

   if (numTeamPlayers <= 1)
      return;

   // if 80 percent of team have no enough money to purchase primary weapon
   if ((numTeamPlayers * 80) / 100 <= numPoorPlayers)
      m_economicsGood[team] = false;

   // winner must buy something!
   if (m_lastWinner == team)
      m_economicsGood[team] = true;
}

void BotManager::Free (void)
{
   // this function free all bots slots (used on server shutdown)

   for (int i = 0; i < MAX_ENGINE_PLAYERS; i++)
      Free (i);
}

void BotManager::Free (int index)
{
   // this function frees one bot selected by index (used on bot disconnect)

   delete m_bots[index];
   m_bots[index] = nullptr;
}

Bot::Bot (edict_t *bot, int difficulty, int personality, int team, int member, const String &steamId)
{
   // this function does core operation of creating bot, it's called by CreateBot (),
   // when bot setup completed, (this is a bot class constructor)

   int clientIndex = engine.IndexOfEntity (bot);

   memset (reinterpret_cast <void *> (this), 0, sizeof (*this));
   pev = &bot->v;

   if (bot->pvPrivateData != nullptr)
      FREE_PRIVATE (bot);

   bot->pvPrivateData = nullptr;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   BotManager::CallGameEntity (&bot->v);

   // set all info buffer keys for this bot
   char *buffer = GET_INFOKEYBUFFER (bot);
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "_vgui_menus", "0");

   if (!(g_gameFlags & GAME_LEGACY) && yb_latency_display.GetInt () == 1)
      SET_CLIENT_KEYVALUE (clientIndex, buffer, "*bot", "1");

   char reject[256] = { 0, };
   MDLL_ClientConnect (bot, STRING (bot->v.netname), FormatBuffer ("127.0.0.%d", engine.IndexOfEntity (bot) + 100), reject);

   if (!IsNullString (reject))
   {
      AddLogEntry (true, LL_WARNING, "Server refused '%s' connection (%s)", STRING (bot->v.netname), reject);
      engine.IssueCmd ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
      return;
   }

   // should be set after client connect
   if (yb_avatar_display.GetBool () && !steamId.IsEmpty ())
      SET_CLIENT_KEYVALUE (clientIndex, buffer, "*sid", const_cast <char *> (steamId.GetBuffer ()));

   memset (&m_pingOffset, 0, sizeof (m_pingOffset));
   memset (&m_ping, 0, sizeof (m_ping));

   MDLL_ClientPutInServer (bot);
   bot->v.flags |= FL_FAKECLIENT; // set this player as fakeclient

   // initialize all the variables for this bot...
   m_notStarted = true;  // hasn't joined game yet
   m_forceRadio = false;

   m_startAction = GAME_MSG_NONE;
   m_retryJoin = 0;
   m_moneyAmount = 0;
   m_logotypeIndex = Random.Int (0, 9);

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = Random.Float (3.8f, 10.0f);
   m_sayTextBuffer.chatProbability = Random.Int (1, 100);

   m_notKilled = false;
   m_weaponBurstMode = BM_OFF;
   m_difficulty = difficulty;

   if (difficulty < 0 || difficulty > 4)
   {
      difficulty = Random.Int (3, 4);
      yb_difficulty.SetInt (difficulty);
   }

   m_lastCommandTime = engine.Time () - 0.1f;
   m_frameInterval = engine.Time ();
   m_timePeriodicUpdate = 0.0f;

   switch (personality)
   {
   case 1:
      m_personality = PERSONALITY_RUSHER;
      m_baseAgressionLevel = Random.Float (0.7f, 1.0f);
      m_baseFearLevel = Random.Float (0.0f, 0.4f);
      break;

   case 2:
      m_personality = PERSONALITY_CAREFUL;
      m_baseAgressionLevel = Random.Float (0.2f, 0.5f);
      m_baseFearLevel = Random.Float (0.7f, 1.0f);
      break;

   default:
      m_personality = PERSONALITY_NORMAL;
      m_baseAgressionLevel = Random.Float (0.4f, 0.7f);
      m_baseFearLevel = Random.Float (0.4f, 0.7f);
      break;
   }

   memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
   memset (&m_ammo, 0, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_voicePitch = Random.Int (80, 115); // assign voice pitch

   // copy them over to the temp level variables
   m_agressionLevel = m_baseAgressionLevel;
   m_fearLevel = m_baseFearLevel;
   m_nextEmotionUpdate = engine.Time () + 0.5f;

   // just to be sure
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // assign team and class
   m_wantedTeam = team;
   m_wantedClass = member;

   // initialize a*
   m_routes = new Route[g_numWaypoints + 1];

   NewRound ();
}

void Bot::ReleaseUsedName (void)
{
   FOR_EACH_AE (g_botNames, j)
   {
      BotName &name = g_botNames[j];

      if (name.usedBy == GetIndex ())
      {
         name.usedBy = 0;
         break;
      }
   }
}

float Bot::GetThinkInterval (void)
{
   if (Math::FltZero (m_thinkInterval))
      return m_frameInterval;

   return m_thinkInterval;
}

Bot::~Bot (void)
{
   // this is bot destructor

   ReleaseUsedName ();
   DeleteSearchNodes ();
   ResetTasks ();

   // clear a* paths
   delete[] m_routes;
}

int BotManager::GetHumansNum (bool ignoreSpectators)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if ((client.flags & CF_USED) && m_bots[i] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT))
      {
         if (ignoreSpectators && client.team2 != TEAM_TERRORIST && client.team2 != TEAM_COUNTER)
            continue;

         count++;
      }
   }
   return count;
}

int BotManager::GetHumansAliveNum (void)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if ((client.flags & (CF_USED | CF_ALIVE)) && m_bots[i] == nullptr && !(client.ent->v.flags & FL_FAKECLIENT))
         count++;
   }
   return count;
}

bool BotManager::IsTeamStacked (int team)
{
   int limitTeams = mp_limitteams.GetInt ();

   if (!limitTeams)
      return false;

   int teamCount[MAX_TEAM_COUNT] = { 0, };

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if ((client.flags & CF_USED) && client.team2 != TEAM_UNASSIGNED && client.team2 != TEAM_SPECTATOR)
         teamCount[client.team2]++;
   }
   return teamCount[team] + 1 > teamCount[team == TEAM_COUNTER ? TEAM_TERRORIST : TEAM_COUNTER] + limitTeams;
}

void Bot::NewRound (void)
{     
   // this function initializes a bot after creation & at the start of each round
   
   int i = 0;

   // delete all allocated path nodes
   DeleteSearchNodes ();

   m_waypointOrigin.Zero ();
   m_destOrigin.Zero ();
   m_currentWaypointIndex = -1;
   m_currentPath = nullptr;
   m_currentTravelFlags = 0;
   m_goalFailed = 0;
   m_desiredVelocity.Zero ();
   m_prevGoalIndex = -1;
   m_chosenGoalIndex = -1;
   m_loosedBombWptIndex = -1;
   m_plantedBombWptIndex = -1;

   m_moveToC4 = false;
   m_duckDefuse = false;
   m_duckDefuseCheckTime = 0.0f;

   m_numFriendsLeft = 0;
   m_numEnemiesLeft = 0;
   m_oldButtons = pev->button;

   m_avoid = nullptr;
   m_avoidTime = 0.0f;

   for (i = 0; i < 5; i++)
      m_prevWptIndex[i] = -1;

   m_navTimeset = engine.Time ();
   m_team = engine.GetTeam (GetEntity ());

   switch (m_personality)
   {
   case PERSONALITY_NORMAL:
      m_pathType = Random.Int (0, 100) > 50 ? SEARCH_PATH_SAFEST_FASTER : SEARCH_PATH_SAFEST;
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
   ResetTasks ();

   m_isVIP = false;
   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;
   m_turnAwayFromFlashbang = 0.0f;

   m_timeTeamOrder = 0.0f;
   m_timeRepotingInDelay = Random.Float (40.0f, 240.0f);
   m_askCheckTime = 0.0f;
   m_minSpeed = 260.0f;
   m_prevSpeed = 0.0f;
   m_prevOrigin = Vector (9999.0f, 9999.0f, 9999.0f);
   m_prevTime = engine.Time ();
   m_lookUpdateTime = engine.Time ();
   
   m_viewDistance = 4096.0f;
   m_maxViewDistance = 4096.0f;

   m_liftEntity = nullptr;
   m_pickupItem = nullptr;
   m_itemIgnore = nullptr;
   m_itemCheckTime = 0.0f;

   m_breakableEntity = nullptr;
   m_breakableOrigin.Zero ();
   m_timeDoorOpen = 0.0f;

   ResetCollideState ();
   ResetDoubleJumpState ();

   m_enemy = nullptr;
   m_lastVictim = nullptr;
   m_lastEnemy = nullptr;
   m_lastEnemyOrigin.Zero ();
   m_trackingEdict = nullptr;
   m_timeNextTracking = 0.0f;

   m_buttonPushTime = 0.0f;
   m_enemyUpdateTime = 0.0f;
   m_enemyIgnoreTimer = 0.0f;
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

   m_position.Zero ();
   m_liftTravelPos.Zero ();

   SetIdealReactionTimes (true);

   m_targetEntity = nullptr;
   m_tasks.RemoveAll ();
   m_followWaitTime = 0.0f;

   for (i = 0; i < MAX_HOSTAGES; i++)
      m_hostages[i] = nullptr;

   for (i = 0; i < Chatter_Total; i++)
      m_chatterTimes[i] = -1.0f;

   m_isReloading = false;
   m_reloadState = RELOAD_NONE;

   m_reloadCheckTime = 0.0f;
   m_shootTime = engine.Time ();
   m_playerTargetTime = engine.Time ();
   m_firePause = 0.0f;
   m_timeLastFired = 0.0f;

   m_grenadeCheckTime = 0.0f;
   m_isUsingGrenade = false;
   m_bombSearchOverridden = false;

   m_blindButton = 0;
   m_blindTime = 0.0f;
   m_jumpTime = 0.0f;
   m_jumpStateTimer = 0.0f;
   m_jumpFinished = false;
   m_isStuck = false;

   m_sayTextBuffer.timeNextChat = engine.Time ();
   m_sayTextBuffer.entityIndex = -1;
   m_sayTextBuffer.sayText[0] = 0x0;

   m_buyState = BUYSTATE_PRIMARY_WEAPON;
   m_lastEquipTime = 0.0f;

   if (!m_notKilled) // if bot died, clear all weapon stuff and force buying again
   {
      memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
      memset (&m_ammo, 0, sizeof (m_ammo));

      m_currentWeapon = 0;
   }

   m_knifeAttackTime = engine.Time () + Random.Float (1.3f, 2.6f);
   m_nextBuyTime = engine.Time () + Random.Float (0.6f, 2.0f);

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

   m_timeLogoSpray = engine.Time () + Random.Float (5.0f, 30.0f);
   m_spawnTime = engine.Time ();
   m_lastChatTime = engine.Time ();

   m_timeCamping = 0.0f;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0f;
   m_heardSoundTime = engine.Time ();

   // clear its message queue
   for (i = 0; i < 32; i++)
      m_messageQueue[i] = GAME_MSG_NONE;

   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   PushMessageQueue (GAME_MSG_PURCHASE);
   PushTask (TASK_NORMAL, TASKPRI_NORMAL, -1, 0.0f, true);

   if (Random.Int (0, 100) < 50)
      ChatterMessage (Chatter_NewRound);

   m_thinkInterval = (g_gameFlags & GAME_LEGACY) ? 0.0f : (1.0f / 30.0f) * Random.Float (0.95f, 1.05f);
}

void Bot::Kill (void)
{
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   bots.TouchWithKillerEntity (this);
}

void Bot::Kick (void)
{
   // this function kick off one bot from the server.
   auto username = STRING (pev->netname);

   if (!(pev->flags & FL_FAKECLIENT) || IsNullString (username))
      return;

   engine.IssueCmd ("kick \"%s\"", username);
   engine.CenterPrintf ("Bot '%s' kicked", username);
}

void Bot::StartGame (void)
{
   // this function handles the selection of teams & class

   // cs prior beta 7.0 uses hud-based motd, so press fire once
   if (g_gameFlags & GAME_LEGACY)
      pev->button |= IN_ATTACK;

   // check if something has assigned team to us
   else if (m_team == TEAM_TERRORIST || m_team == TEAM_COUNTER)
      m_notStarted = false;

   // if bot was unable to join team, and no menus popups, check for stacked team
   if (m_startAction == GAME_MSG_NONE && ++m_retryJoin > 3)
   {
      if (bots.IsTeamStacked (m_wantedTeam - 1))
      {
         m_retryJoin = 0;

         engine.Printf ("Could not add bot to the game: Team is stacked (to disable this check, set mp_limitteams and mp_autoteambalance to zero and restart the round).");
         Kick ();

         return;
      }
   }

   // handle counter-strike stuff here...
   if (m_startAction == GAME_MSG_TEAM_SELECT)
   {
      m_startAction = GAME_MSG_NONE;  // switch back to idle
      
      char teamJoin = yb_join_team.GetString ()[0];

      if (teamJoin == 'C' || teamJoin == 'c')
         m_wantedTeam = 2;
      else if (teamJoin == 'T' || teamJoin == 't')
         m_wantedTeam = 1;

      if (m_wantedTeam != 1 && m_wantedTeam != 2)
         m_wantedTeam = 5;

      // select the team the bot wishes to join...
      engine.IssueBotCommand (GetEntity (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == GAME_MSG_CLASS_SELECT)
   {
      m_startAction = GAME_MSG_NONE;  // switch back to idle

      // czero has additional models
      int maxChoice = (g_gameFlags & GAME_CZERO) ? 5 : 4;

      if (m_wantedClass < 1 || m_wantedClass > maxChoice)
         m_wantedClass = Random.Int (1, maxChoice); // use random if invalid

      // select the class the bot wishes to use...
      engine.IssueBotCommand (GetEntity (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;
      
      // check for greeting other players, since we connected
      if (Random.Int (0, 100) < 20)
         ChatMessage (CHAT_WELCOME);
   }
}

void BotManager::CalculatePingOffsets (void)
{
   if (!(g_gameFlags & GAME_SUPPORT_SVC_PINGS) || yb_latency_display.GetInt () != 2)
      return;

   int averagePing = 0;
   int numHumans = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      edict_t *ent = engine.EntityOfIndex (i + 1);

      if (!IsValidPlayer (ent))
         continue;

      numHumans++;

      int ping, loss;
      PLAYER_CNX_STATS (ent, &ping, &loss);

      if (ping < 0 || ping > 100)
         ping = Random.Int (3, 15);

      averagePing += ping;
   }

   if (numHumans > 0)
      averagePing /= numHumans;
   else
      averagePing = Random.Int (30, 40);

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = GetBot (i);

      if (bot == nullptr)
         continue;

      int part = static_cast <int> (averagePing * 0.2f);
      int botPing = Random.Int (averagePing - part, averagePing + part) + Random.Int (bot->m_difficulty + 3, bot->m_difficulty + 6) + 10;

      if (botPing <= 5)
         botPing = Random.Int (10, 23);
      else if (botPing > 100)
         botPing = Random.Int (30, 40);

      for (int j = 0; j < 2; j++)
      {
         for (bot->m_pingOffset[j] = 0; bot->m_pingOffset[j] < 4; bot->m_pingOffset[j]++)
         {
            if ((botPing - bot->m_pingOffset[j]) % 4 == 0)
            {
               bot->m_ping[j] = (botPing - bot->m_pingOffset[j]) / 4;
               break;
            }
         }
      }
      bot->m_ping[2] = botPing;
   }
}

void BotManager::SendPingDataOffsets (edict_t *to)
{
   if (!(g_gameFlags & GAME_SUPPORT_SVC_PINGS) || yb_latency_display.GetInt () != 2 || engine.IsNullEntity (to) || (to->v.flags & FL_FAKECLIENT))
      return;

   if (!(to->v.flags & FL_CLIENT) && !(((to->v.button & IN_SCORE) || !(to->v.oldbuttons & IN_SCORE))))
      return;

   static int sending;
   sending = 0;

   // missing from sdk
   static const int SVC_PINGS = 17;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = m_bots[i];

      if (bot == nullptr)
         continue;

      switch (sending)
      {
      case 0:
         {
            // start a new message
            MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_PINGS, nullptr, to);
            WRITE_BYTE ((bot->m_pingOffset[sending] * 64) + (1 + 2 * i));
            WRITE_SHORT (bot->m_ping[sending]);

            sending++;
         }
      case 1:
         {
            // append additional data
            WRITE_BYTE ((bot->m_pingOffset[sending] * 128) + (2 + 4 * i));
            WRITE_SHORT (bot->m_ping[sending]);

            sending++;
         }
      case 2:
         {
            // append additional data and end message
            WRITE_BYTE (4 + 8 * i);
            WRITE_SHORT (bot->m_ping[sending]);
            WRITE_BYTE (0);
            MESSAGE_END ();

            sending = 0;
         }
      }
   }

   // end message if not yet sent
   if (sending)
   {
      WRITE_BYTE (0);
      MESSAGE_END ();
   }
}

void BotManager::SendDeathMsgFix (void)
{
   if (yb_latency_display.GetInt () == 2 && m_deathMsgSent)
   {
      m_deathMsgSent = false;

      for (int i = 0; i < engine.MaxClients (); i++)
         SendPingDataOffsets (g_clients[i].ent);
   }
}

void BotManager::UpdateActiveGrenades (void)
{
   if (m_grenadeUpdateTime > engine.Time ())
      return;

   edict_t *grenade = nullptr;

   // clear previously stored grenades
   m_activeGrenades.RemoveAll ();

   // search the map for any type of grenade
   while (!engine.IsNullEntity (grenade = FIND_ENTITY_BY_CLASSNAME (grenade, "grenade")))
   {
      // do not count c4 as a grenade
      if (strcmp (STRING (grenade->v.model) + 9, "c4.mdl") == 0)
         continue;

      m_activeGrenades.Push (grenade);
   }
   m_grenadeUpdateTime = engine.Time () + 0.213f;
}

const Array <edict_t *> &BotManager::GetActiveGrenades (void)
{
   return m_activeGrenades;
}

void BotManager::SelectLeaderEachTeam (int team, bool reset)
{
   if (reset)
   {
      m_leaderChoosen[team] = false;
      return;
   }

   if (m_leaderChoosen[team])
      return;

   if (g_mapType & MAP_AS)
   {
      if (team == TEAM_COUNTER && !m_leaderChoosen[TEAM_COUNTER])
      {
         for (int i = 0; i < engine.MaxClients (); i++)
         {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_isVIP)
            {
               // vip bot is the leader
               bot->m_isLeader = true;

               if (Random.Int (1, 100) < 50)
               {
                  bot->RadioMessage (Radio_FollowMe);
                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[TEAM_COUNTER] = true;
      }
      else if (team == TEAM_TERRORIST && !m_leaderChoosen[TEAM_TERRORIST])
      {
         auto bot = bots.GetHighestFragsBot (team);

         if (bot != nullptr && bot->m_notKilled)
         {
            bot->m_isLeader = true;

            if (Random.Int (1, 100) < 45)
               bot->RadioMessage (Radio_FollowMe);
         }
         m_leaderChoosen[TEAM_TERRORIST] = true;
      }
   }
   else if (g_mapType & MAP_DE)
   {
      if (team == TEAM_TERRORIST && !m_leaderChoosen[TEAM_TERRORIST])
      {
         for (int i = 0; i < engine.MaxClients (); i++)
         {
            auto bot = m_bots[i];

            if (bot != nullptr && bot->m_hasC4)
            {
               // bot carrying the bomb is the leader
               bot->m_isLeader = true;

               // terrorist carrying a bomb needs to have some company
               if (Random.Int (1, 100) < 80)
               {
                  if (yb_communication_type.GetInt () == 2)
                     bot->ChatterMessage (Chatter_GoingToPlantBomb);
                  else
                     bot->ChatterMessage (Radio_FollowMe);

                  bot->m_campButtons = 0;
               }
            }
         }
         m_leaderChoosen[TEAM_TERRORIST] = true;
      }
      else if (!m_leaderChoosen[TEAM_COUNTER])
      {
         if (auto bot = bots.GetHighestFragsBot (team))
         {
            bot->m_isLeader = true;

            if (Random.Int (1, 100) < 30)
               bot->RadioMessage (Radio_FollowMe);
         }
         m_leaderChoosen[TEAM_COUNTER] = true;
      }
   }
   else if (g_mapType & (MAP_ES | MAP_KA | MAP_FY))
   {
      auto bot = bots.GetHighestFragsBot (team);

      if (!m_leaderChoosen[team] && bot)
      {
         bot->m_isLeader = true;

         if (Random.Int (1, 100) < 30)
            bot->RadioMessage (Radio_FollowMe);

         m_leaderChoosen[team] = true;
      }
   }
   else
   {
      auto bot = bots.GetHighestFragsBot (team);

      if (!m_leaderChoosen[team] && bot)
      {
         bot->m_isLeader = true;

         if (Random.Int (1, 100) < (team == TEAM_TERRORIST ? 30 : 40))
            bot->RadioMessage (Radio_FollowMe);

         m_leaderChoosen[team] = true;
      }
   }
}
