//
// Copyright (c) 2014, by YaPB Development Team. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation
// the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the
// Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// Version: $Id:$
//

#include <core.h>

ConVar yb_autovacate ("yb_autovacate", "-1");

ConVar yb_quota ("yb_quota", "0");
ConVar yb_quota_match ("yb_quota_match", "0");
ConVar yb_quota_match_max ("yb_quota_match_max", "0");
ConVar yb_join_after_player ("yb_join_after_player", "0");

ConVar yb_join_team ("yb_join_team", "any");
ConVar yb_name_prefix ("yb_name_prefix", "");

ConVar yb_minskill ("yb_minskill", "60");
ConVar yb_maxskill ("yb_maxskill", "100");

ConVar yb_skill_tags ("yb_skill_tags", "0");
ConVar yb_latency_display ("yb_latency_display", "2");

BotManager::BotManager (void)
{
   // this is a bot manager class constructor

   m_lastWinner = -1;

   m_economicsGood[TEAM_TF] = true;
   m_economicsGood[TEAM_CF] = true;

   memset (m_bots, 0, sizeof (m_bots));

   if (g_pGlobals != NULL)
      InitQuota ();
}

BotManager::~BotManager (void)
{
   // this is a bot manager class destructor, do not use GetMaxClients () here !!

   Free ();
}

void BotManager::CallGameEntity (entvars_t *vars)
{
   // this function calls gamedll player() function, in case to create player entity in game

   if (g_isMetamod)
   {
      CALL_GAME_ENTITY (PLID, "player", vars);
      return;
   }

   static EntityPtr_t playerFunction = NULL;

   if (playerFunction == NULL)
      playerFunction = (EntityPtr_t) g_gameLib->GetFunctionAddr ("player");

   if (playerFunction != NULL)
      (*playerFunction) (vars);
}

int BotManager::CreateBot (String name, int skill, int personality, int team, int member)
{
   // this function completely prepares bot entity (edict) for creation, creates team, skill, sets name etc, and
   // then sends result to bot constructor

   
   edict_t *bot = NULL;
   char outputName[33];

   if (g_numWaypoints < 1) // don't allow creating bots with no waypoints loaded
   {
      CenterPrint ("Map is not waypointed. Cannot create bot");
      return 0;
   }
   else if (g_waypointsChanged) // don't allow creating bots with changed waypoints (distance tables are messed up)
   {
      CenterPrint ("Waypoints have been changed. Load waypoints again...");
      return 0;
   }

   if (skill < 0 || skill > 100)
      skill = g_randGen.Long (yb_minskill.GetInt (), yb_maxskill.GetInt ());

   if (skill > 100 || skill < 0)
      skill = g_randGen.Long (0, 100);

   if (personality < 0 || personality > 2)
   {
      if (g_randGen.Long (0, 100) < 50)
         personality = PERSONALITY_NORMAL;
      else
      {
         if (g_randGen.Long (0, 100) > 50)
            personality = PERSONALITY_RUSHER;
         else
            personality = PERSONALITY_CAREFUL;
      }
   }

   // setup name
   if (name.IsEmpty ())
   {
      if (!g_botNames.IsEmpty ())
      {
         bool nameFound = false;

         for (int i = 0; i < g_botNames.GetSize (); i++)
         {
            if (nameFound)
               break;

            BotName *name = &g_botNames.GetRandomElement ();

            if (name == NULL || (name != NULL && name->used))
               continue;

            name->used = nameFound = true;
            strcpy (outputName, name->name);
         }
      }
      else
         sprintf (outputName, "bot%i", g_randGen.Long (0, 100)); // just pick ugly random name
   }
   else
      strncpy (outputName, name, 21);

   if (!IsNullString (yb_name_prefix.GetString ()) || yb_skill_tags.GetBool ())
   {
      char prefixedName[33]; // temp buffer for storing modified name

      if (!IsNullString (yb_name_prefix.GetString ()))
         sprintf (prefixedName, "%s %s", yb_name_prefix.GetString (), outputName);

      else if (yb_skill_tags.GetBool ())
         sprintf  (prefixedName, "%s (%d)", outputName, skill);

      else if (!IsNullString (yb_name_prefix.GetString ()) && yb_skill_tags.GetBool ())
         sprintf (prefixedName, "%s %s (%d)", yb_name_prefix.GetString (), outputName, skill);

      // buffer has been modified, copy to real name
      if (!IsNullString (prefixedName))
         strcpy (outputName, prefixedName);
   }

   if (FNullEnt ((bot = (*g_engfuncs.pfnCreateFakeClient) (outputName))))
   {
      CenterPrint ("Maximum players reached (%d/%d). Unable to create Bot.", GetMaxClients (), GetMaxClients ());
      return 2;
   }

   int index = ENTINDEX (bot) - 1;

   InternalAssert (index >= 0 && index <= 32); // check index
   InternalAssert (m_bots[index] == NULL); // check bot slot

   m_bots[index] = new Bot (bot, skill, personality, team, member);

   if (m_bots == NULL)
      TerminateOnMalloc ();

   ServerPrint ("Connecting Bot... (Skill %d)", skill);

   return 1;
}

int BotManager::GetIndex (edict_t *ent)
{
   // this function returns index of bot (using own bot array)
   if (FNullEnt (ent))
      return -1;

   int index = ENTINDEX (ent) - 1;

   if (index < 0 || index >= 32)
      return -1;

   if (m_bots[index] != NULL)
      return index;

   return -1; // if no edict, return -1;
}

Bot *BotManager::GetBot (int index)
{
   // this function finds a bot specified by index, and then returns pointer to it (using own bot array)

   if (index < 0 || index >= 32)
      return NULL;

   if (m_bots[index] != NULL)
      return m_bots[index];

   return NULL; // no bot
}

Bot *BotManager::GetBot (edict_t *ent)
{
   // same as above, but using bot entity

   return GetBot (GetIndex (ent));
}

Bot *BotManager::FindOneValidAliveBot (void)
{
   // this function finds one bot, alive bot :)

   Array <int> foundBots;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL && IsAlive (m_bots[i]->GetEntity ()))
         foundBots.Push (i);
   }

   if (!foundBots.IsEmpty ())
      return m_bots[foundBots.GetRandomElement ()];

   return NULL;
}

void BotManager::Think (void)
{
   // this function calls think () function for all available at call moment bots, and
   // try to catch internal error if such shit occurs

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL)
      {
         // use these try-catch blocks to prevent server crashes when error occurs
#if defined (NDEBUG) && !defined (PLATFORM_LINUX) && !defined (PLATFORM_OSX)
         try
         {
            m_bots[i]->Think ();
         }
         catch (...)
         {
            // error occurred. kick off all bots and then print a warning message
            RemoveAll ();

            ServerPrintNoTag ("**** INTERNAL BOT ERROR! PLEASE SHUTDOWN AND RESTART YOUR SERVER! ****");
         }
#else
         m_bots[i]->Think ();

#endif
         
      }
   }
}

void BotManager::AddBot (const String &name, int skill, int personality, int team, int member)
{
   // this function putting bot creation process to queue to prevent engine crashes

   CreateQueue bot;

   // fill the holder
   bot.name = name;
   bot.skill = skill;
   bot.personality = personality;
   bot.team = team;
   bot.member = member;

   // put to queue
   m_creationTab.Push (bot);

   // keep quota number up to date
   if (GetBotsNum () + 1 > yb_quota.GetInt ())
      yb_quota.SetInt (GetBotsNum () + 1);
}

void BotManager::AddBot (String name, String skill, String personality, String team, String member)
{
   // this function is same as the function above, but accept as parameters string instead of integers

   CreateQueue bot;
   const String &any = "*";

   bot.name = (name.IsEmpty () || (name == any)) ? String ("\0") : name;
   bot.skill = (skill.IsEmpty () || (skill == any)) ? -1 : skill.ToInt ();
   bot.team = (team.IsEmpty () || (team == any)) ? -1 : team.ToInt ();
   bot.member = (member.IsEmpty () || (member == any)) ? -1 : member.ToInt ();
   bot.personality = (personality.IsEmpty () || (personality == any)) ? -1 : personality.ToInt ();

   m_creationTab.Push (bot);

   // keep quota number up to date
   if (GetBotsNum () + 1 > yb_quota.GetInt ())
      yb_quota.SetInt (GetBotsNum () + 1);
}

void BotManager::CheckAutoVacate (edict_t *ent)
{
   // this function sets timer to kick one bot off.

   if (yb_autovacate.GetBool ())
      RemoveRandom ();
}

void BotManager::MaintainBotQuota (void)
{
   // this function keeps number of bots up to date, and don't allow to maintain bot creation
   // while creation process in process.

   if (yb_join_after_player.GetInt () > 0 && GetHumansJoinedTeam () == 0)
   {
      RemoveAll (false);
      return;
   }

   if (!m_creationTab.IsEmpty () && m_maintainTime < GetWorldTime ())
   {
      CreateQueue last = m_creationTab.Pop ();
      int resultOfCall = CreateBot (last.name, last.skill, last.personality, last.team, last.member);

      // check the result of creation
      if (resultOfCall == 0)
      {
         m_creationTab.RemoveAll (); // something worng with waypoints, reset tab of creation
         yb_quota.SetInt (0); // reset quota
      }
      else if (resultOfCall == 2)
      {
         m_creationTab.RemoveAll (); // maximum players reached, so set quota to maximum players
         yb_quota.SetInt (GetBotsNum ());
      }
      m_maintainTime = GetWorldTime () + 0.15f;
   }

   // now keep bot number up to date
   if (m_maintainTime < GetWorldTime ())
   {
      int botNumber = GetBotsNum ();
      int humanNumber = GetHumansNum ();

      if (botNumber > yb_quota.GetInt ())
         RemoveRandom ();

      if (yb_quota_match.GetInt () > 0)
      {
         int num = yb_quota_match.GetInt () * humanNumber;

         if (num >= GetMaxClients () - humanNumber)
            num = GetMaxClients () - humanNumber;

         if (yb_quota_match_max.GetInt () > 0 && num > yb_quota_match_max.GetInt ())
            num = yb_quota_match_max.GetInt ();

         yb_quota.SetInt (num);
         yb_autovacate.SetInt (0);
      }

      if (yb_autovacate.GetBool ())
      {
         if (botNumber < yb_quota.GetInt () && botNumber < GetMaxClients () - 1)
            AddRandom ();

         if (humanNumber >= GetMaxClients ())
            RemoveRandom ();
      }
      else
      {
         if (botNumber < yb_quota.GetInt () && botNumber < GetMaxClients ())
            AddRandom ();
      }

      int botQuota = yb_autovacate.GetBool () ? (GetMaxClients () - 1 - (humanNumber + 1)) : GetMaxClients ();

      // check valid range of quota
      if (yb_quota.GetInt () > botQuota)
         yb_quota.SetInt (botQuota);

      else if (yb_quota.GetInt () < 0)
         yb_quota.SetInt (0);

      m_maintainTime = GetWorldTime () + 0.15f;
   }
}

void BotManager::InitQuota (void)
{
   m_maintainTime = GetWorldTime () + 1.5f;
   m_creationTab.RemoveAll ();
}

void BotManager::FillServer (int selection, int personality, int skill, int numToAdd)
{
   // this function fill server with bots, with specified team & personality

   if (GetBotsNum () >= GetMaxClients () - GetHumansNum ())
      return;

   if (selection == 1 || selection == 2)
   {
      CVAR_SET_STRING ("mp_limitteams", "0");
      CVAR_SET_STRING ("mp_autoteambalance", "0");
   }
   else
      selection = 5;

   char teamDesc[6][12] =
   {
      "",
      {"Terrorists"},
      {"CTs"},
      "",
      "",
      {"Random"},
   };

   int toAdd = numToAdd == -1 ? GetMaxClients () - (GetHumansNum () + GetBotsNum ()) : numToAdd;

   for (int i = 0; i <= toAdd; i++)
   {
      // since we got constant skill from menu (since creation process call automatic), we need to manually
      // randomize skill here, on given skill there.
      int randomizedSkill = 0;

      if (skill >= 0 && skill <= 20)
         randomizedSkill = g_randGen.Long (0, 20);
      else if (skill > 20 && skill <= 40)
         randomizedSkill = g_randGen.Long (20, 40);
      else if (skill > 40 && skill <= 60)
         randomizedSkill = g_randGen.Long (40, 60);
      else if (skill > 60 && skill <= 80)
         randomizedSkill = g_randGen.Long (60, 80);
      else if (skill > 80 && skill <= 99)
         randomizedSkill = g_randGen.Long (80, 99);
      else if (skill == 100)
         randomizedSkill = skill;

      AddBot ("", randomizedSkill, personality, selection, -1);
   }

   yb_quota.SetInt (toAdd);
   yb_quota_match.SetInt (0);

   CenterPrint ("Fill Server with %s bots...", &teamDesc[selection][0]);
}

void BotManager::RemoveAll (bool zeroQuota)
{
   // this function drops all bot clients from server (this function removes only yapb's)`q

   if (zeroQuota)
      CenterPrint ("Bots are removed from server.");

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL)  // is this slot used?
         m_bots[i]->Kick ();
   }
   m_creationTab.RemoveAll ();

   // reset cvars
   if (zeroQuota)
   {
      yb_quota.SetInt (0);
      yb_autovacate.SetInt (0);
   }
}

void BotManager::RemoveFromTeam (Team team, bool removeAll)
{
   // this function remove random bot from specified team (if removeAll value = 1 then removes all players from team)

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL && team == GetTeam (m_bots[i]->GetEntity ()))
      {
         m_bots[i]->Kick ();

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

   static char tempBuffer[1024];
   char buffer[1024];

   memset (tempBuffer, 0, sizeof (tempBuffer));
   memset (buffer, 0, sizeof (buffer));

   int validSlots = (selection == 4) ? (1 << 9) : ((1 << 8) | (1 << 9));

   for (int i = ((selection - 1) * 8); i < selection * 8; i++)
   {
      if ((m_bots[i] != NULL) && !FNullEnt (m_bots[i]->GetEntity ()))
      {
         validSlots |= 1 << (i - ((selection - 1) * 8));
         sprintf (buffer, "%s %1.1d. %s%s\n", buffer, i - ((selection - 1) * 8) + 1, STRING (m_bots[i]->pev->netname), GetTeam (m_bots[i]->GetEntity ()) == TEAM_CF ? " \\y(CT)\\w" : " \\r(T)\\w");
      }
      else
         sprintf (buffer, "%s\\d %1.1d. Not a YaPB\\w\n", buffer, i - ((selection - 1) * 8) + 1);
   }

   sprintf (tempBuffer, "\\yYaPB Remove Menu (%d/4):\\w\n\n%s\n%s 0. Back", selection, buffer, (selection == 4) ? "" : " 9. More...\n");

   switch (selection)
   {
   case 1:
      g_menus[14].validSlots = validSlots & static_cast <unsigned int> (-1);
      g_menus[14].menuText = tempBuffer;

      DisplayMenuToClient (ent, &g_menus[14]);
      break;

   case 2:
      g_menus[15].validSlots = validSlots & static_cast <unsigned int> (-1);
      g_menus[15].menuText = tempBuffer;

      DisplayMenuToClient (ent, &g_menus[15]);
      break;

    case 3:
      g_menus[16].validSlots = validSlots & static_cast <unsigned int> (-1);
      g_menus[16].menuText = tempBuffer;

      DisplayMenuToClient (ent, &g_menus[16]);
      break;

   case 4:
      g_menus[17].validSlots = validSlots & static_cast <unsigned int> (-1);
      g_menus[17].menuText = tempBuffer;

      DisplayMenuToClient (ent, &g_menus[17]);
      break;
   }
}

void BotManager::KillAll (int team)
{
   // this function kills all bots on server (only this dll controlled bots)

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL)
      {
         if (team != -1 && team != GetTeam (m_bots[i]->GetEntity ()))
            continue;

         m_bots[i]->Kill ();
      }
   }
   CenterPrint ("All Bots died !");
}

void BotManager::RemoveRandom (void)
{
   // this function removes random bot from server (only yapb's)

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL)  // is this slot used?
      {
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

   CenterPrint ("%s weapon mode selected", &modeName[selection][0]);
}

void BotManager::ListBots (void)
{
   // this function list's bots currently playing on the server

   ServerPrintNoTag ("%-3.5s %-9.13s %-17.18s %-3.4s %-3.4s %-3.4s", "index", "name", "personality", "team", "skill", "frags");

   for (int i = 0; i < GetMaxClients (); i++)
   {
      edict_t *player = INDEXENT (i);

      // is this player slot valid
      if (IsValidBot (player))
      {
         Bot *bot = GetBot (player);

         if (bot != NULL)
            ServerPrintNoTag ("[%-3.1d] %-9.13s %-17.18s %-3.4s %-3.1d %-3.1d", i, STRING (player->v.netname), bot->m_personality == PERSONALITY_RUSHER ? "rusher" : bot->m_personality == PERSONALITY_NORMAL ? "normal" : "careful", GetTeam (player) != 0 ? "CT" : "T", bot->m_skill, static_cast <int> (player->v.frags));
      }
   }
}

int BotManager::GetBotsNum (void)
{
   // this function returns number of yapb's playing on the server

   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL)
         count++;
   }
   return count;
}

Bot *BotManager::GetHighestFragsBot (int team)
{
   Bot *highFragBot = NULL;

   int bestIndex = 0;
   float bestScore = -1;

   // search bots in this team
   for (int i = 0; i < GetMaxClients (); i++)
   {
      highFragBot = g_botManager->GetBot (i);

      if (highFragBot != NULL && IsAlive (highFragBot->GetEntity ()) && GetTeam (highFragBot->GetEntity ()) == team)
      {
         if (highFragBot->pev->frags > bestScore)
         {
            bestIndex = i;
            bestScore = highFragBot->pev->frags;
         }
      }
   }
   return GetBot (bestIndex);
}

void BotManager::CheckTeamEconomics (int team)
{
   // this function decides is players on specified team is able to buy primary weapons by calculating players
   // that have not enough money to buy primary (with economics), and if this result higher 80%, player is can't
   // buy primary weapons.

   extern ConVar yb_economics_rounds;

   if (!yb_economics_rounds.GetBool ())
   {
      m_economicsGood[team] = true;
      return; // don't check economics while economics disable
   }

   int numPoorPlayers = 0;
   int numTeamPlayers = 0;

   // start calculating
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (m_bots[i] != NULL && GetTeam (m_bots[i]->GetEntity ()) == team)
      {
         if (m_bots[i]->m_moneyAmount <= g_botBuyEconomyTable[0])
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

   for (int i = 0; i < 32; i++)
   {
      if (m_bots[i] != NULL)
         Free (i);
   }
}

void BotManager::Free (int index)
{
   // this function frees one bot selected by index (used on bot disconnect)

   m_bots[index]->SwitchChatterIcon (false);
   m_bots[index]->ReleaseUsedName ();
 
   delete m_bots[index];
   m_bots[index] = NULL;
}

Bot::Bot (edict_t *bot, int skill, int personality, int team, int member)
{
   // this function does core operation of creating bot, it's called by CreateBot (),
   // when bot setup completed, (this is a bot class constructor)

   char rejectReason[128];
   int clientIndex = ENTINDEX (bot);

   memset (this, 0, sizeof (Bot));

   pev = VARS (bot);

   if (bot->pvPrivateData != NULL)
      FREE_PRIVATE (bot);

   bot->pvPrivateData = NULL;
   bot->v.frags = 0;

   // create the player entity by calling MOD's player function
   BotManager::CallGameEntity (&bot->v);

   // set all info buffer keys for this bot
   char *buffer = GET_INFOKEYBUFFER (bot);

   SET_CLIENT_KEYVALUE (clientIndex, buffer, "model", "");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "rate", "3500.000000");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_updaterate", "20");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_lw", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_lc", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "tracker", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "cl_dlmax", "128");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "friends", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "dm", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "_ah", "0");
   SET_CLIENT_KEYVALUE (clientIndex, buffer, "_vgui_menus", "0");

   if (g_gameVersion != CSV_OLD && yb_latency_display.GetInt () == 1)
      SET_CLIENT_KEYVALUE (clientIndex, buffer, "*bot", "1");

   rejectReason[0] = 0; // reset the reject reason template string
   MDLL_ClientConnect (bot, "BOT", FormatBuffer ("127.0.0.%d", ENTINDEX (bot) + 100), rejectReason);

   if (!IsNullString (rejectReason))
   {
      AddLogEntry (true, LL_WARNING, "Server refused '%s' connection (%s)", STRING (bot->v.netname), rejectReason);
      ServerCommand ("kick \"%s\"", STRING (bot->v.netname)); // kick the bot player if the server refused it

      bot->v.flags |= FL_KILLME;
   }

   MDLL_ClientPutInServer (bot);
   bot->v.flags |= FL_FAKECLIENT; // set this player as fakeclient

   // initialize all the variables for this bot...
   m_notStarted = true;  // hasn't joined game yet

   m_startAction = GSM_IDLE;
   m_moneyAmount = 0;

   m_logotypeIndex = g_randGen.Long (0, 5);
   m_msecVal = static_cast <byte> (g_pGlobals->frametime * 1000.0);

   // assign how talkative this bot will be
   m_sayTextBuffer.chatDelay = g_randGen.Float (3.8, 10.0);
   m_sayTextBuffer.chatProbability = g_randGen.Long (1, 100);

   m_notKilled = false;
   m_skill = skill;
   m_weaponBurstMode = BM_OFF;

   m_lastThinkTime = GetWorldTime () - 0.1f;
   m_frameInterval = GetWorldTime ();

   bot->v.idealpitch = bot->v.v_angle.x;
   bot->v.ideal_yaw = bot->v.v_angle.y;

   bot->v.yaw_speed = g_randGen.Float (g_skillTab[m_skill / 20].minTurnSpeed, g_skillTab[m_skill / 20].maxTurnSpeed);
   bot->v.pitch_speed = g_randGen.Float (g_skillTab[m_skill / 20].minTurnSpeed, g_skillTab[m_skill / 20].maxTurnSpeed);

   switch (personality)
   {
   case 1:
      m_personality = PERSONALITY_RUSHER;
      m_baseAgressionLevel = g_randGen.Float (0.7, 1.0);
      m_baseFearLevel = g_randGen.Float (0.0, 0.4);
      break;

   case 2:
      m_personality = PERSONALITY_CAREFUL;
      m_baseAgressionLevel = g_randGen.Float (0.2, 0.5);
      m_baseFearLevel = g_randGen.Float (0.7, 1.0);
      break;

   default:
      m_personality = PERSONALITY_NORMAL;
      m_baseAgressionLevel = g_randGen.Float (0.4, 0.7);
      m_baseFearLevel = g_randGen.Float (0.4, 0.7);
      break;
   }

   memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
   memset (&m_ammo, 0, sizeof (m_ammo));

   m_currentWeapon = 0; // current weapon is not assigned at start
   m_voicePitch = g_randGen.Long (166, 250) / 2; // assign voice pitch

   // copy them over to the temp level variables
   m_agressionLevel = m_baseAgressionLevel;
   m_fearLevel = m_baseFearLevel;
   m_nextEmotionUpdate = GetWorldTime () + 0.5;

   // just to be sure
   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // assign team and class
   m_wantedTeam = team;
   m_wantedClass = member;

   NewRound ();
}

void Bot::ReleaseUsedName (void)
{
   IterateArray (g_botNames, j)
   {
      BotName &name = g_botNames[j];

      if (strcmp (name.name, STRING (pev->netname)) == 0)
      {
         name.used = false;
         break;
      }
   }
}

Bot::~Bot (void)
{
   // this is bot destructor

   DeleteSearchNodes ();
   ResetTasks ();
}

int BotManager::GetHumansNum (void)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Client *cl = &g_clients[i];

      if ((cl->flags & CF_USED) && m_bots[i] == NULL && !(cl->ent->v.flags & FL_FAKECLIENT))
         count++;
   }
   return count;
}

int BotManager::GetHumansAliveNum (void)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Client *cl = &g_clients[i];

      if ((cl->flags & (CF_USED | CF_ALIVE)) && m_bots[i] == NULL && !(cl->ent->v.flags & FL_FAKECLIENT))
         count++;
   }
   return count;
}

int BotManager::GetHumansJoinedTeam (void)
{
   // this function returns number of humans playing on the server

   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Client *cl = &g_clients[i];

      if ((cl->flags & (CF_USED | CF_ALIVE)) && m_bots[i] == NULL && cl->team != TEAM_SPEC && !(cl->ent->v.flags & FL_FAKECLIENT) && cl->ent->v.movetype != MOVETYPE_FLY)
         count++;
   }
   return count;
}

void Bot::NewRound (void)
{     
   // this function initializes a bot after creation & at the start of each round
   
   g_canSayBombPlanted = true;
   int i = 0;
   

   // delete all allocated path nodes
   DeleteSearchNodes ();

   m_waypointOrigin = nullvec;
   m_destOrigin = nullvec;
   m_currentWaypointIndex = -1;
   m_currentPath = NULL;
   m_currentTravelFlags = 0;
   m_desiredVelocity = nullvec;
   m_prevGoalIndex = -1;
   m_chosenGoalIndex = -1;
   m_loosedBombWptIndex = -1;

   m_moveToC4 = false;
   m_duckDefuse = false;
   m_duckDefuseCheckTime = 0.0;

   m_prevWptIndex[0] = -1;
   m_prevWptIndex[1] = -1;
   m_prevWptIndex[2] = -1;
   m_prevWptIndex[3] = -1;
   m_prevWptIndex[4] = -1;

   m_navTimeset = GetWorldTime ();
   m_team = GetTeam (GetEntity ());

   switch (m_personality)
   {
   case PERSONALITY_NORMAL:
      m_pathType = g_randGen.Long (0, 100) > 50 ? 1 : 2;
      break;

   case PERSONALITY_RUSHER:
      m_pathType = 0;
      break;

   case PERSONALITY_CAREFUL:
      m_pathType = 2;
      break;
   }

   // clear all states & tasks
   m_states = 0;
   ResetTasks ();

   m_isVIP = false;
   m_isLeader = false;
   m_hasProgressBar = false;
   m_canChooseAimDirection = true;

   m_timeTeamOrder = 0.0;
   m_askCheckTime = 0.0;
   m_minSpeed = 260.0;
   m_prevSpeed = 0.0;
   m_prevOrigin = Vector (9999.0, 9999.0, 9999.0);
   m_prevTime = GetWorldTime ();
   m_blindRecognizeTime = GetWorldTime ();
   
   m_viewDistance = 4096.0;
   m_maxViewDistance = 4096.0;

   m_liftEntity = NULL;
   m_pickupItem = NULL;
   m_itemIgnore = NULL;
   m_itemCheckTime = 0.0;

   m_breakableEntity = NULL;
   m_breakable = nullvec;
   m_timeDoorOpen = 0.0;

   ResetCollideState ();
   ResetDoubleJumpState ();

   m_enemy = NULL;
   m_lastVictim = NULL;
   m_lastEnemy = NULL;
   m_lastEnemyOrigin = nullvec;
   m_trackingEdict = NULL;
   m_timeNextTracking = 0.0;

   m_buttonPushTime = 0.0;
   m_enemyUpdateTime = 0.0;
   m_seeEnemyTime = 0.0;
   m_shootAtDeadTime = 0.0;
   m_oldCombatDesire = 0.0;
   m_liftUsageTime = 0.0;

   m_avoidGrenade = NULL;
   m_needAvoidGrenade = 0;

   m_lastDamageType = -1;
   m_voteKickIndex = 0;
   m_lastVoteKick = 0;
   m_voteMap = 0;
   m_doorOpenAttempt = 0;
   m_aimFlags = 0;
   m_liftState = 0;
   m_burstShotsFired = 0;

   m_position = nullvec;
   m_liftTravelPos = nullvec;

   m_idealReactionTime = g_skillTab[m_skill / 20].minSurpriseTime;
   m_actualReactionTime = g_skillTab[m_skill / 20].minSurpriseTime;

   m_targetEntity = NULL;
   m_tasks = NULL;
   m_followWaitTime = 0.0;

   for (i = 0; i < MAX_HOSTAGES; i++)
      m_hostages[i] = NULL;

   for (i = 0; i < Chatter_Total; i++)
      m_voiceTimers[i] = -1.0;

   m_isReloading = false;
   m_reloadState = RELOAD_NONE;

   m_reloadCheckTime = 0.0;
   m_shootTime = GetWorldTime ();
   m_playerTargetTime = GetWorldTime ();
   m_firePause = 0.0;
   m_timeLastFired = 0.0;

   m_grenadeCheckTime = 0.0;
   m_isUsingGrenade = false;

   m_skillOffset = static_cast <float> ((100 - m_skill) / 100.0);
   m_blindButton = 0;
   m_blindTime = 0.0;
   m_jumpTime = 0.0;
   m_jumpFinished = false;
   m_isStuck = false;

   m_sayTextBuffer.timeNextChat = GetWorldTime ();
   m_sayTextBuffer.entityIndex = -1;
   m_sayTextBuffer.sayText[0] = 0x0;

   m_buyState = 0;

   if (!m_notKilled) // if bot died, clear all weapon stuff and force buying again
   {
      memset (&m_ammoInClip, 0, sizeof (m_ammoInClip));
      memset (&m_ammo, 0, sizeof (m_ammo));

      m_currentWeapon = 0;
   }

   m_knifeAttackTime = GetWorldTime () + g_randGen.Float (1.3, 2.6);
   m_nextBuyTime = GetWorldTime () + g_randGen.Float (0.6, 1.2);

   m_buyPending = false;
   m_inBombZone = false;

   m_shieldCheckTime = 0.0;
   m_zoomCheckTime = 0.0;
   m_strafeSetTime = 0.0;
   m_combatStrafeDir = 0;
   m_fightStyle = 0;
   m_lastFightStyleCheck = 0.0;

   m_checkWeaponSwitch = true;
   m_checkKnifeSwitch = true;
   m_buyingFinished = false;

   m_radioEntity = NULL;
   m_radioOrder = 0;
   m_defendedBomb = false;

   m_timeLogoSpray = GetWorldTime () + g_randGen.Float (0.5, 2.0);
   m_spawnTime = GetWorldTime ();
   m_lastChatTime = GetWorldTime ();
   pev->v_angle.y = pev->ideal_yaw;

   m_timeCamping = 0;
   m_campDirection = 0;
   m_nextCampDirTime = 0;
   m_campButtons = 0;

   m_soundUpdateTime = 0.0;
   m_heardSoundTime = GetWorldTime ();

   // clear its message queue
   for (i = 0; i < 32; i++)
      m_messageQueue[i] = GSM_IDLE;

   m_actMessageIndex = 0;
   m_pushMessageIndex = 0;

   // and put buying into its message queue
   PushMessageQueue (GSM_BUY_STUFF);
   StartTask (TASK_NORMAL, TASKPRI_NORMAL, -1, 0.0, true);

   if (g_randGen.Long (0, 100) < 50)
      ChatterMessage (Chatter_NewRound);
}

void Bot::Kill (void)
{
   // this function kills a bot (not just using ClientKill, but like the CSBot does)
   // base code courtesy of Lazy (from bots-united forums!)

   edict_t *hurtEntity = (*g_engfuncs.pfnCreateNamedEntity) (MAKE_STRING ("trigger_hurt"));

   if (FNullEnt (hurtEntity))
      return;

   hurtEntity->v.classname = MAKE_STRING (g_weaponDefs[m_currentWeapon].className);
   hurtEntity->v.dmg_inflictor = GetEntity ();
   hurtEntity->v.dmg = 9999.0;
   hurtEntity->v.dmg_take = 1.0;
   hurtEntity->v.dmgtime = 2.0;
   hurtEntity->v.effects |= EF_NODRAW;

   (*g_engfuncs.pfnSetOrigin) (hurtEntity, Vector (-4000, -4000, -4000));

   KeyValueData kv;
   kv.szClassName = const_cast <char *> (g_weaponDefs[m_currentWeapon].className);
   kv.szKeyName = "damagetype";
   kv.szValue = FormatBuffer ("%d", (1 << 4));
   kv.fHandled = FALSE;

   MDLL_KeyValue (hurtEntity, &kv);

   MDLL_Spawn (hurtEntity);
   MDLL_Touch (hurtEntity, GetEntity ());

   (*g_engfuncs.pfnRemoveEntity) (hurtEntity);
}

void Bot::Kick (void)
{
   // this function kick off one bot from the server.

   ServerCommand ("kick #%d", GETPLAYERUSERID (GetEntity ()));
   CenterPrint ("Bot '%s' kicked", STRING (pev->netname));

   // balances quota
   if (g_botManager->GetBotsNum () - 1 < yb_quota.GetInt ())
      yb_quota.SetInt (g_botManager->GetBotsNum () - 1);
}

void Bot::StartGame (void)
{
   // this function handles the selection of teams & class

   // handle counter-strike stuff here...
   if (m_startAction == GSM_TEAM_SELECT)
   {
      m_startAction = GSM_IDLE;  // switch back to idle

      if (yb_join_team.GetString ()[0] == 'C' || yb_join_team.GetString ()[0] == 'c')
         m_wantedTeam = 2;
      else if (yb_join_team.GetString ()[0] == 'T' || yb_join_team.GetString ()[0] == 't')
         m_wantedTeam = 1;

      if (m_wantedTeam != 1 && m_wantedTeam != 2)
         m_wantedTeam = 5;

      // select the team the bot wishes to join...
      FakeClientCommand (GetEntity (), "menuselect %d", m_wantedTeam);
   }
   else if (m_startAction == GSM_CLASS_SELECT)
   {
      m_startAction = GSM_IDLE;  // switch back to idle

      if (g_gameVersion == CSV_CZERO) // czero has spetsnaz and militia skins
      {
         if (m_wantedClass < 1 || m_wantedClass > 5)
            m_wantedClass = g_randGen.Long (1, 5); //  use random if invalid
      }
      else
      {
         if (m_wantedClass < 1 || m_wantedClass > 4)
            m_wantedClass = g_randGen.Long (1, 4); // use random if invalid
      }

      // select the class the bot wishes to use...
      FakeClientCommand (GetEntity (), "menuselect %d", m_wantedClass);

      // bot has now joined the game (doesn't need to be started)
      m_notStarted = false;

      // check for greeting other players, since we connected
      if (g_randGen.Long (0, 100) < 20)
         ChatMessage (CHAT_WELCOME);
   }
}

void BotManager::CalculatePingOffsets (void)
{
   if (yb_latency_display.GetInt () != 2)
      return;

   int averagePing = 0;
   int numHumans = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      edict_t *ent = INDEXENT (i + 1);

      if (!IsValidPlayer (ent))
         continue;

      numHumans++;

      int ping, loss;
      PLAYER_CNX_STATS (ent, &ping, &loss);

      if (ping < 0 || ping > 100)
         ping = g_randGen.Long (3, 15);

      averagePing += ping;
   }

   if (numHumans > 0)
      averagePing /= numHumans;
   else
      averagePing = g_randGen.Long (30, 40);

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Bot *bot = GetBot (i);

      if (bot == NULL)
         continue;

      int botPing = g_randGen.Long (averagePing - averagePing * 0.2f, averagePing + averagePing * 0.2f) + g_randGen.Long (bot->m_skill / 100 * 0.5, bot->m_skill / 100);

      if (botPing <= 5)
         botPing = g_randGen.Long (10, 23);
      else if (botPing > 100)
         botPing = g_randGen.Long (30, 40);

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
   if (yb_latency_display.GetInt () != 2)
      return;

   if (!IsValidPlayer (to) || IsValidBot (to))
      return;

   if (!((to->v.button & IN_SCORE) || (to->v.oldbuttons & IN_SCORE)))
      return;

   static int sending;
   sending = 0;

   // missing from sdk
   static const int SVC_PINGS = 17;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Bot *bot = GetBot (i);

      if (bot == NULL)
         continue;

      switch (sending)
      {
      case 0:
         {
            // start a new message
            MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_PINGS, NULL, to);
            WRITE_BYTE ((m_bots[i]->m_pingOffset[sending] * 64) + (1 + 2 * i));
            WRITE_SHORT (m_bots[i]->m_ping[sending]);

            sending++;
         }
      case 1:
         {
            // append additional data
            WRITE_BYTE ((m_bots[i]->m_pingOffset[sending] * 128) + (2 + 4 * i));
            WRITE_SHORT (m_bots[i]->m_ping[sending]);

            sending++;
         }
      case 2:
         {
            // append additional data and end message
            WRITE_BYTE (4 + 8 * i);
            WRITE_SHORT (m_bots[i]->m_ping[sending]);
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

      for (int i = 0; i < GetMaxClients (); i++)
         SendPingDataOffsets (g_clients[i].ent);
   }
}