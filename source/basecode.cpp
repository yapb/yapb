//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_debug ("yb_debug", "0", VT_NOSERVER);
ConVar yb_debug_goal ("yb_debug_goal", "-1", VT_NOSERVER);
ConVar yb_user_follow_percent ("yb_user_follow_percent", "20", VT_NOSERVER);
ConVar yb_user_max_followers ("yb_user_max_followers", "1", VT_NOSERVER);

ConVar yb_jasonmode ("yb_jasonmode", "0");
ConVar yb_communication_type ("yb_communication_type", "2");
ConVar yb_economics_rounds ("yb_economics_rounds", "1");
ConVar yb_walking_allowed ("yb_walking_allowed", "1");

ConVar yb_tkpunish ("yb_tkpunish", "1");
ConVar yb_freeze_bots ("yb_freeze_bots", "0");
ConVar yb_spraypaints ("yb_spraypaints", "1");
ConVar yb_botbuy ("yb_botbuy", "1");

ConVar yb_timersound ("yb_timersound", "0.5", VT_NOSERVER);
ConVar yb_timerpickup ("yb_timerpickup", "0.5", VT_NOSERVER);
ConVar yb_timergrenade ("yb_timergrenade", "0.5", VT_NOSERVER);

ConVar yb_chatter_path ("yb_chatter_path", "sound/radio/bot", VT_NOSERVER);
ConVar yb_restricted_weapons ("yb_restricted_weapons", "ump45;p90;elite;tmp;mac10;m3;xm1014");

// game console variables
ConVar mp_c4timer ("mp_c4timer", NULL, VT_NOREGISTER);
ConVar mp_buytime ("mp_buytime", NULL, VT_NOREGISTER);
ConVar mp_footsteps ("mp_footsteps", NULL, VT_NOREGISTER);
ConVar sv_gravity ("sv_gravity", NULL, VT_NOREGISTER);

int Bot::GetMessageQueue (void)
{
   // this function get the current message from the bots message queue

   int message = m_messageQueue[m_actMessageIndex++];
   m_actMessageIndex &= 0x1f; // wraparound

   return message;
} 

void Bot::PushMessageQueue (int message)
{
   // this function put a message into the bot message queue

   if (message == GSM_SAY)
   {
      // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
      int entityIndex = GetIndex ();

      for (int i = 0; i < GetMaxClients (); i++)
      {
         Bot *otherBot = g_botManager->GetBot (i);

         if (otherBot != NULL && otherBot->pev != pev)
         {
            if (m_notKilled == IsAlive (otherBot->GetEntity ()))
            {
               otherBot->m_sayTextBuffer.entityIndex = entityIndex;
               strcpy (otherBot->m_sayTextBuffer.sayText, m_tempStrings);
            }
            otherBot->m_sayTextBuffer.timeNextChat = GetWorldTime () + otherBot->m_sayTextBuffer.chatDelay;
         }
      }
   }
   m_messageQueue[m_pushMessageIndex++] = message;
   m_pushMessageIndex &= 0x1f; // wraparound
}

int Bot::InFieldOfView (const Vector &destination)
{
   float entityAngle = AngleMod (destination.ToYaw ()); // find yaw angle from source to destination...
   float viewAngle = AngleMod (pev->v_angle.y); // get bot's current view angle...

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead, 45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle
   int absoluteAngle = abs (static_cast <int> (viewAngle) - static_cast <int> (entityAngle));

   if (absoluteAngle > 180)
      absoluteAngle = 360 - absoluteAngle;

   return absoluteAngle;
}

bool Bot::IsInViewCone (const Vector &origin)
{
   // this function returns true if the spatial vector location origin is located inside
   // the field of view cone of the bot entity, false otherwise. It is assumed that entities
   // have a human-like field of view, that is, about 90 degrees.

   return ::IsInViewCone (origin, GetEntity ());
}

bool Bot::CheckVisibility (entvars_t *targetEntity, Vector *origin, byte *bodyPart)
{
   // this function checks visibility of a bot target.

   Vector botHead = EyePosition ();
   TraceResult tr;

   *bodyPart = 0;

   // check for the body
   TraceLine (botHead, targetEntity->origin, true, true, GetEntity (), &tr);

   if (tr.flFraction >= 1.0)
   {
      *bodyPart |= VISIBLE_BODY;
      *origin = targetEntity->origin;
   }

   // check for the head
   TraceLine (botHead, targetEntity->origin + targetEntity->view_ofs, true, true, GetEntity (), &tr);

   if (tr.flFraction >= 1.0)
   {
      *bodyPart |= VISIBLE_HEAD;
      *origin = targetEntity->origin + targetEntity->view_ofs;
   }

   if (*bodyPart != 0)
      return true;

#if 0
   // dimension table
   const int8 dimensionTab[8][3] =
   {
      {1,  1,  1}, { 1,  1, -1},
      {1, -1,  1}, {-1,  1,  1},
      {1, -1, -1}, {-1, -1,  1},
      {-1, 1, -1}, {-1, -1, -1}
   };

   // check the box borders
   for (int i = 7; i >= 0; i--)
   {
      Vector targetOrigin = petargetOrigin->origin + Vector (dimensionTab[i][0] * 24.0, dimensionTab[i][1] * 24.0, dimensionTab[i][2] * 24.0);

      // check direct line to random part of the player body
      TraceLine (botHead, targetOrigin, true, true, GetEntity (), &tr);

      // check if we hit something
      if (tr.flFraction >= 1.0)
      {
         *origin = tr.vecEndPos;
         *bodyPart |= VISIBLE_OTHER;

         return true;
      }
   }
#else
   // worst case, choose random position in enemy body
   for (int i = 0; i < 5; i++)
   {
      Vector targetOrigin = targetEntity->origin; // get the player origin

      // find the vector beetwen mins and maxs of the player body
      targetOrigin.x += Random.Float (targetEntity->mins.x * 0.5, targetEntity->maxs.x * 0.5);
      targetOrigin.y += Random.Float (targetEntity->mins.y * 0.5, targetEntity->maxs.y * 0.5);
      targetOrigin.z += Random.Float (targetEntity->mins.z * 0.5, targetEntity->maxs.z * 0.5);

      // check direct line to random part of the player body
      TraceLine (botHead, targetOrigin, true, true, GetEntity (), &tr);

      // check if we hit something
      if (tr.flFraction >= 1.0)
      {
         *origin = tr.vecEndPos;
         *bodyPart |= VISIBLE_OTHER;

         return true;
      }
   }
#endif
   return false;
}

bool Bot::IsEnemyViewable (edict_t *player)
{
   if (IsEntityNull (player))
      return false;

   bool forceTrueIfVisible = false;

   if (IsValidPlayer (pev->dmg_inflictor) && GetTeam (pev->dmg_inflictor) != m_team && ::IsInViewCone (EyePosition (), pev->dmg_inflictor))
      forceTrueIfVisible = true;

   if (CheckVisibility (VARS (player), &m_enemyOrigin, &m_visibility) && (IsInViewCone (player->v.origin + Vector (0, 0, 14)) || forceTrueIfVisible))
   {
      m_seeEnemyTime = GetWorldTime ();
      m_lastEnemy = player;
      m_lastEnemyOrigin = player->v.origin;

      return true;
   }
   return false;
}

bool Bot::ItemIsVisible (const Vector &destination, char *itemName) 
{
   TraceResult tr;

   // trace a line from bot's eyes to destination..
   TraceLine (EyePosition (), destination, true, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction != 1.0)
   {
      // check for standard items
      if (strcmp (STRING (tr.pHit->v.classname), itemName) == 0)
         return true;

      if (tr.flFraction > 0.98 && strncmp (STRING (tr.pHit->v.classname), "csdmw_", 6) == 0)
         return true;

      return false;
   }
   return true;
}

bool Bot::EntityIsVisible (const Vector &dest, bool fromBody)
{
   TraceResult tr;

   // trace a line from bot's eyes to destination...
   TraceLine (fromBody ? pev->origin - Vector (0, 0, 1) : EyePosition (), dest, true, true, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0;
}


void Bot::AvoidGrenades (void)
{
   // checks if bot 'sees' a grenade, and avoid it

   edict_t *ent = m_avoidGrenade;

  // check if old pointers to grenade is invalid
   if (IsEntityNull (ent))
   {
      m_avoidGrenade = NULL;
      m_needAvoidGrenade = 0;
   }
   else if ((ent->v.flags & FL_ONGROUND) || (ent->v.effects & EF_NODRAW))
   {
      m_avoidGrenade = NULL;
      m_needAvoidGrenade = 0;
   }
   ent = NULL;

   // find all grenades on the map
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
   {
      // if grenade is invisible don't care for it
      if (ent->v.effects & EF_NODRAW)
         continue;

      // check if visible to the bot
      if (!EntityIsVisible (ent->v.origin) && InFieldOfView (ent->v.origin - EyePosition ()) > pev->fov / 2)
         continue;

      // TODO: should be done once for grenade, instead of checking several times
      if (m_difficulty == 4 && strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0)
      {
         Vector position = (GetEntityOrigin (ent) - EyePosition ()).ToAngles ();

         // don't look at flashbang
         if (!(m_states & STATE_SEEING_ENEMY))
         {
            pev->v_angle.y = AngleNormalize (position.y + 180.0);
            m_canChooseAimDirection = false;
         }
      }
      else if (strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0)
      {
         if (!IsEntityNull (m_avoidGrenade))
            return;

         if (GetTeam (ent->v.owner) == m_team && ent->v.owner != GetEntity ())
            return;

         if ((ent->v.flags & FL_ONGROUND) == 0)
         {
            float distance = (ent->v.origin - pev->origin).GetLength ();
            float distanceMoved = ((ent->v.origin + ent->v.velocity * m_frameInterval) - pev->origin).GetLength ();

            if (distanceMoved < distance && distance < 500)
            {
               MakeVectors (pev->v_angle);

               Vector dirToPoint = (pev->origin - ent->v.origin).Normalize2D ();
               Vector rightSide = g_pGlobals->v_right.Normalize2D ();

               if ((dirToPoint | rightSide) > 0)
                  m_needAvoidGrenade = -1;
               else
                  m_needAvoidGrenade = 1;

               m_avoidGrenade = ent;
            }
         }
      }
   }
}

bool Bot::IsBehindSmokeClouds (edict_t *ent)
{
   edict_t *pentGrenade = NULL;
   Vector betweenUs = (ent->v.origin - pev->origin).Normalize ();

   while (!IsEntityNull (pentGrenade = FIND_ENTITY_BY_CLASSNAME (pentGrenade, "grenade")))
   {
      // if grenade is invisible don't care for it
      if ((pentGrenade->v.effects & EF_NODRAW) || !(pentGrenade->v.flags & (FL_ONGROUND | FL_PARTIALGROUND)) || strcmp (STRING (pentGrenade->v.model) + 9, "smokegrenade.mdl"))
         continue;

      // check if visible to the bot
      if (!EntityIsVisible (ent->v.origin) && InFieldOfView (ent->v.origin - EyePosition ()) > pev->fov / 3)
         continue;

      Vector betweenNade = (GetEntityOrigin (pentGrenade) - pev->origin).Normalize ();
      Vector betweenResult = ((Vector (betweenNade.y, betweenNade.x, 0) * 150.0 + GetEntityOrigin (pentGrenade)) - pev->origin).Normalize ();

      if ((betweenNade | betweenUs) > (betweenNade | betweenResult))
         return true;
   }
   return false;
}

int Bot::GetBestWeaponCarried (void)
{
   // this function returns the best weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   // take the shield in account
   if (HasShield ())
      weapons |= (1 << WEAPON_SHIELD);

   for (int i = 0; i < NUM_WEAPONS; i++)
   {
      if (weapons & (1 << weaponTab[*ptr].id))
         weaponIndex = i;

      ptr++;
   }
   return weaponIndex;
}

int Bot::GetBestSecondaryWeaponCarried (void)
{
   // this function returns the best secondary weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   // take the shield in account
   if (HasShield ())
      weapons |= (1 << WEAPON_SHIELD);

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < NUM_WEAPONS; i++)
   {
      int id = weaponTab[*ptr].id;

      if ((weapons & (1 << weaponTab[*ptr].id)) && (id == WEAPON_USP || id == WEAPON_GLOCK || id == WEAPON_DEAGLE || id == WEAPON_P228 || id == WEAPON_ELITE || id == WEAPON_FIVESEVEN))
      {
         weaponIndex = i;
         break;
      }
      ptr++;
   }
   return weaponIndex;
}

bool Bot::RateGroundWeapon (edict_t *ent)
{
   // this function compares weapons on the ground to the one the bot is using

   int hasWeapon = 0;
   int groundIndex = 0;
   int *ptr = g_weaponPrefs[m_personality];

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < NUM_WEAPONS; i++)
   {
      if (strcmp (weaponTab[*ptr].modelName, STRING (ent->v.model) + 9) == 0)
      {
         groundIndex = i;
         break;
      }
      ptr++;   
   }

   if (groundIndex < 7)
      hasWeapon = GetBestSecondaryWeaponCarried ();
   else
      hasWeapon = GetBestWeaponCarried ();

   return groundIndex > hasWeapon;
}

void Bot::VerifyBreakable (edict_t *touch)
{
   if (m_breakableCheckTime >= GetWorldTime () || !IsShootableBreakable (touch))
      return;

   m_breakableEntity = FindBreakable ();

   if (IsEntityNull (m_breakableEntity))
      return;

   m_campButtons = pev->button & IN_DUCK;

   StartTask (TASK_SHOOTBREAKABLE, TASKPRI_SHOOTBREAKABLE, -1, 0.0, false);
   m_breakableCheckTime = GetWorldTime () + 1.0f;
}

edict_t *Bot::FindBreakable (void)
{
   // this function checks if bot is blocked by a shoot able breakable in his moving direction

   TraceResult tr;
   TraceLine (pev->origin, pev->origin + (m_destOrigin - pev->origin).Normalize () * 64, false, false, GetEntity (), &tr);

   if (tr.flFraction != 1.0)
   {
      edict_t *ent = tr.pHit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (IsShootableBreakable (ent))
      {
         m_breakable = tr.vecEndPos;
         return ent;
      }
   }
   TraceLine (EyePosition (), EyePosition () + (m_destOrigin - EyePosition ()).Normalize () * 64, false, false, GetEntity (), &tr);

   if (tr.flFraction != 1.0)
   {
      edict_t *ent = tr.pHit;

      if (IsShootableBreakable (ent))
      {
         m_breakable = tr.vecEndPos;
         return ent;
      }
   }
   m_breakableEntity = NULL;
   m_breakable = nullvec;

   return NULL;
}

void Bot::SetIdealReactionTimes (bool actual)
{
   float min = 0.0f;
   float max = 0.01f;

   switch (m_difficulty)
   {
   case 0:
      min = 0.8f; max = 1.0f;
      break;

   case 1:
      min = 0.4f; max = 0.6f;
      break;

   case 2:
      min = 0.2f; max = 0.4f;
      break;

   case 3:
      min = 0.0f; max = 0.1f;
      break;

   case 4:
   default:
      min = 0.0f;max = 0.01f;
      break;
   }

   if (actual)
   {
      m_idealReactionTime = min;
      m_actualReactionTime = min;

      return;
   }
   m_idealReactionTime = Random.Float (min, max);
}

void Bot::FindItem (void)
{
   // this function finds Items to collect or use in the near of a bot

   // don't try to pickup anything while on ladder or trying to escape from bomb...
   if (IsOnLadder () || GetTaskId () == TASK_ESCAPEFROMBOMB)
   {
      m_pickupItem = NULL;
      m_pickupType = PICKUP_NONE;

      return;
   }

   edict_t *ent = NULL, *pickupItem = NULL;
   Bot *bot = NULL;

   bool allowPickup = false;
   float distance, minDistance = 341.0;


   if (!IsEntityNull (m_pickupItem))
   {
      bool itemExists = false;
      pickupItem = m_pickupItem;

      while (!IsEntityNull (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, 340.0)))
      {
         if ((ent->v.effects & EF_NODRAW) || IsValidPlayer (ent->v.owner))
            continue; // someone owns this weapon or it hasn't respawned yet

         if (ent == pickupItem)
         {
            if (ItemIsVisible (GetEntityOrigin (ent), const_cast <char *> (STRING (ent->v.classname))))
               itemExists = true;
            break;
         }
      }
      if (itemExists)
         return;
      else
      {
         m_pickupItem = NULL;
         m_pickupType = PICKUP_NONE;
      }
   }

   ent = NULL;
   pickupItem = NULL;

   PickupType pickupType = PICKUP_NONE;

   Vector pickupOrigin = nullvec;
   Vector entityOrigin = nullvec;

   m_pickupItem = NULL;
   m_pickupType = PICKUP_NONE;

   while (!IsEntityNull (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, 340.0)))
   {
      allowPickup = false;  // assume can't use it until known otherwise

      if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore)
         continue; // someone owns this weapon or it hasn't respawned yet

      entityOrigin = GetEntityOrigin (ent);

      // check if line of sight to object is not blocked (i.e. visible)
      if (ItemIsVisible (entityOrigin, const_cast <char *> (STRING (ent->v.classname))))
      {
         if (strncmp ("hostage_entity", STRING (ent->v.classname), 14) == 0)
         {
            allowPickup = true;
            pickupType = PICKUP_HOSTAGE;
         }
         else if (strncmp ("weaponbox", STRING (ent->v.classname), 9) == 0 && strcmp (STRING (ent->v.model) + 9, "backpack.mdl") == 0)
         {
            allowPickup = true;
            pickupType = PICKUP_DROPPED_C4;
         }
         else if ((strncmp ("weaponbox", STRING (ent->v.classname), 9) == 0 || strncmp ("armoury_entity", STRING (ent->v.classname), 14) == 0 || strncmp ("csdm", STRING (ent->v.classname), 4) == 0) && !m_isUsingGrenade)
         {
            allowPickup = true;
            pickupType = PICKUP_WEAPON;
         }
         else if (strncmp ("weapon_shield", STRING (ent->v.classname), 13) == 0 && !m_isUsingGrenade)
         {
            allowPickup = true;
            pickupType = PICKUP_SHIELD;
         }
         else if (strncmp ("item_thighpack", STRING (ent->v.classname), 14) == 0 && m_team == TEAM_CF && !m_hasDefuser)
         {
            allowPickup = true;
            pickupType = PICKUP_DEFUSEKIT;
         }
         else if (strncmp ("grenade", STRING (ent->v.classname), 7) == 0 && strcmp (STRING (ent->v.model) + 9, "c4.mdl") == 0)
         {
            allowPickup = true;
            pickupType = PICKUP_PLANTED_C4;
         }
      }

      if (allowPickup) // if the bot found something it can pickup...
      {
         distance = (entityOrigin - pev->origin).GetLength ();

         // see if it's the closest item so far...
         if (distance < minDistance)
         {
            if (pickupType == PICKUP_WEAPON) // found weapon on ground?
            {
               int weaponCarried = GetBestWeaponCarried ();
               int secondaryWeaponCarried = GetBestSecondaryWeaponCarried ();

               if (secondaryWeaponCarried < 7 && (m_ammo[g_weaponSelect[secondaryWeaponCarried].id] > 0.3 * g_weaponDefs[g_weaponSelect[secondaryWeaponCarried].id].ammo1Max) && strcmp (STRING (ent->v.model) + 9, "w_357ammobox.mdl") == 0)
                  allowPickup = false;
               else if (!m_isVIP && weaponCarried >= 7 && (m_ammo[g_weaponSelect[weaponCarried].id] > 0.3 * g_weaponDefs[g_weaponSelect[weaponCarried].id].ammo1Max) && strncmp (STRING (ent->v.model) + 9, "w_", 2) == 0)
               {
                  if (strcmp (STRING (ent->v.model) + 9, "w_9mmarclip.mdl") == 0 && !(weaponCarried == WEAPON_FAMAS || weaponCarried == WEAPON_AK47 || weaponCarried == WEAPON_M4A1 || weaponCarried == WEAPON_GALIL || weaponCarried == WEAPON_AUG || weaponCarried == WEAPON_SG552))
                     allowPickup = false;
                  else if (strcmp (STRING (ent->v.model) + 9, "w_shotbox.mdl") == 0 && !(weaponCarried == WEAPON_M3 || weaponCarried == WEAPON_XM1014))
                     allowPickup = false;
                  else if (strcmp (STRING (ent->v.model) + 9, "w_9mmclip.mdl") == 0 && !(weaponCarried == WEAPON_MP5 || weaponCarried == WEAPON_TMP || weaponCarried == WEAPON_P90 || weaponCarried == WEAPON_MAC10 || weaponCarried == WEAPON_UMP45))
                     allowPickup = false;
                  else if (strcmp (STRING (ent->v.model) + 9, "w_crossbow_clip.mdl") == 0 && !(weaponCarried == WEAPON_AWP || weaponCarried == WEAPON_G3SG1 || weaponCarried == WEAPON_SCOUT || weaponCarried == WEAPON_SG550))
                     allowPickup = false;
                  else if (strcmp (STRING (ent->v.model) + 9, "w_chainammo.mdl") == 0 && weaponCarried == WEAPON_M249)
                     allowPickup = false;
               }
               else if (m_isVIP || !RateGroundWeapon (ent))
                  allowPickup = false;
               else if (strcmp (STRING (ent->v.model) + 9, "medkit.mdl") == 0 && (pev->health >= 100))
                  allowPickup = false;
               else if ((strcmp (STRING (ent->v.model) + 9, "kevlar.mdl") == 0 || strcmp (STRING (ent->v.model) + 9, "battery.mdl") == 0) && pev->armorvalue >= 100) // armor vest
                  allowPickup = false;
               else if (strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0 && (pev->weapons & (1 << WEAPON_FLASHBANG))) // concussion grenade
                  allowPickup = false;
               else if (strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_EXPLOSIVE))) // explosive grenade
                  allowPickup = false;
               else if (strcmp (STRING (ent->v.model) + 9, "smokegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_SMOKE))) // smoke grenade
                  allowPickup = false;
            }
            else if (pickupType == PICKUP_SHIELD) // found a shield on ground?
            {
               if ((pev->weapons & (1 << WEAPON_ELITE)) || HasShield () || m_isVIP || (HasPrimaryWeapon () && !RateGroundWeapon (ent)))
                  allowPickup = false;
            }
            else if (m_team == TEAM_TF) // terrorist team specific
            {
               if (pickupType == PICKUP_DROPPED_C4)
               {
                  allowPickup = true;
                  m_destOrigin = entityOrigin; // ensure we reached droped bomb

                  ChatterMessage (Chatter_FoundC4); // play info about that
                  DeleteSearchNodes ();
               }
               else if (pickupType == PICKUP_HOSTAGE)
               {
                  m_itemIgnore = ent;
                  allowPickup = false;

                  if (!m_defendHostage && m_difficulty >= 3 && Random.Long (0, 100) < 30 && m_timeCamping + 15.0f < GetWorldTime ())
                  {
                     int index = FindDefendWaypoint (entityOrigin);

                     StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (30.0, 60.0), true); // push camp task on to stack
                     StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + Random.Float (3.0, 6.0), true); // push move command

                     if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
                        m_campButtons |= IN_DUCK;
                     else
                        m_campButtons &= ~IN_DUCK;

                     m_defendHostage = true;

                     ChatterMessage (Chatter_GoingToGuardHostages); // play info about that
                     return;
                  }
               }
               else if (pickupType == PICKUP_PLANTED_C4)
               {
                  allowPickup = false;

                  if (!m_defendedBomb)
                  {
                     m_defendedBomb = true;

                     int index = FindDefendWaypoint (entityOrigin);
                     Path *path = g_waypoint->GetPath (index);

                     float bombTimer = mp_c4timer.GetFloat ();
                     float timeMidBlowup = g_timeBombPlanted + ((bombTimer / 2) + bombTimer / 4) - g_waypoint->GetTravelTime (pev->maxspeed, pev->origin, path->origin);

                     if (timeMidBlowup > GetWorldTime ())
                     {
                        RemoveCertainTask (TASK_MOVETOPOSITION); // remove any move tasks

                        StartTask (TASK_CAMP, TASKPRI_CAMP, -1, timeMidBlowup, true); // push camp task on to stack
                        StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeMidBlowup, true); // push  move command

                        if (path->vis.crouch <= path->vis.stand)
                           m_campButtons |= IN_DUCK;
                        else
                           m_campButtons &= ~IN_DUCK;

                        if (Random.Long (0, 100) < 90)
                           ChatterMessage (Chatter_DefendingBombSite);
                     }
                     else
                        RadioMessage (Radio_ShesGonnaBlow); // issue an additional radio message
                  }
               }
            }
            else if (m_team == TEAM_CF)
            {
               if (pickupType == PICKUP_HOSTAGE)
               {
                  if (IsEntityNull (ent) || (ent->v.health <= 0))
                     allowPickup = false; // never pickup dead hostage
                  else for (int i = 0; i < GetMaxClients (); i++)
                  {
                     if ((bot = g_botManager->GetBot (i)) != NULL && IsAlive (bot->GetEntity ()))
                     {
                        for (int j = 0; j < MAX_HOSTAGES; j++)
                        {
                           if (bot->m_hostages[j] == ent)
                           {
                              allowPickup = false;
                              break;
                           }
                        }
                     }
                  }
               }
               else if (pickupType == PICKUP_PLANTED_C4 && !OutOfBombTimer ())
               {
                  if (m_states & (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY))
                  {
                     allowPickup = false;
                     return;
                  }

                  if (Random.Long (0, 100) < 90)
                     ChatterMessage (Chatter_FoundBombPlace);

                  allowPickup = !IsBombDefusing (g_waypoint->GetBombPosition ()) || m_hasProgressBar;
                  pickupType = PICKUP_PLANTED_C4;

                  if (!m_defendedBomb && !allowPickup)
                  {
                     m_defendedBomb = true;

                     int index = FindDefendWaypoint (entityOrigin);
                     Path *path = g_waypoint->GetPath (index);

                     float timeToExplode = g_timeBombPlanted + mp_c4timer.GetFloat () - g_waypoint->GetTravelTime (pev->maxspeed, pev->origin, path->origin);
        
                     RemoveCertainTask (TASK_MOVETOPOSITION); // remove any move tasks

                     StartTask (TASK_CAMP, TASKPRI_CAMP, -1, timeToExplode, true); // push camp task on to stack
                     StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeToExplode, true); // push move command

                     if (path->vis.crouch <= path->vis.stand)
                        m_campButtons |= IN_DUCK;
                     else
                        m_campButtons &= ~IN_DUCK;

                     if (Random.Long (0, 100) < 90)
                        ChatterMessage (Chatter_DefendingBombSite);
                  }
               }
               else if (pickupType == PICKUP_DROPPED_C4)
               {
                  m_itemIgnore = ent;
                  allowPickup = false;

                  if (!m_defendedBomb && m_difficulty >= 2 && Random.Long (0, 100) < 80)
                  {
                     int index = FindDefendWaypoint (entityOrigin);

                     StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (30.0, 70.0), true); // push camp task on to stack
                     StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + Random.Float (10.0, 30.0), true); // push move command

                     if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
                        m_campButtons |= IN_DUCK;
                     else
                        m_campButtons &= ~IN_DUCK;

                     m_defendedBomb = true;

                     ChatterMessage (Chatter_GoingToGuardDoppedBomb); // play info about that
                     return;
                  }
               }
            }

            // if condition valid
            if (allowPickup)
            {
               minDistance = distance; // update the minimum distance
               pickupOrigin = entityOrigin; // remember location of entity
               pickupItem = ent; // remember this entity

               m_pickupType = pickupType;
            }
            else
               pickupType = PICKUP_NONE;
         }
      }
   } // end of the while loop

   if (!IsEntityNull (pickupItem))
   {
      for (int i = 0; i < GetMaxClients (); i++)
      {
         if ((bot = g_botManager->GetBot (i)) != NULL && IsAlive (bot->GetEntity ()) && bot->m_pickupItem == pickupItem)
         {
            m_pickupItem = NULL;
            m_pickupType = PICKUP_NONE;

            return;
         }
      }

      if (pickupOrigin.z > EyePosition ().z + (m_pickupType == PICKUP_HOSTAGE ? 40.0f : 15.0f)|| IsDeadlyDrop (pickupOrigin)) // check if item is too high to reach, check if getting the item would hurt bot
      {
         m_pickupItem = NULL;
         m_pickupType = PICKUP_NONE;

         return;
      }
      m_pickupItem = pickupItem; // save pointer of picking up entity
   }
}

void Bot::GetCampDirection (Vector *dest)
{
   // this function check if view on last enemy position is blocked - replace with better vector then
   // mostly used for getting a good camping direction vector if not camping on a camp waypoint

   TraceResult tr;
   Vector src = EyePosition ();

   TraceLine (src, *dest, true, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0)
   {
      float length = (tr.vecEndPos - src).GetLengthSquared ();

      if (length > 10000)
         return;

      float minDistance = FLT_MAX;
      float maxDistance = FLT_MAX;

      int enemyIndex = -1, tempIndex = -1;

      // find nearest waypoint to bot and position
      for (int i = 0; i < g_numWaypoints; i++)
      {
         float distance = (g_waypoint->GetPath (i)->origin - pev->origin).GetLengthSquared ();

         if (distance < minDistance)
         {
            minDistance = distance;
            tempIndex = i;
         }
         distance = (g_waypoint->GetPath (i)->origin - *dest).GetLengthSquared ();

         if (distance < maxDistance)
         {
            maxDistance = distance;
            enemyIndex = i;
         }
      }

      if (tempIndex == -1 || enemyIndex == -1)
         return;

      minDistance = FLT_MAX;

      int lookAtWaypoint = -1;
      Path *path = g_waypoint->GetPath (tempIndex);

      for (int i = 0; i < MAX_PATH_INDEX; i++)
      {
         if (path->index[i] == -1)
            continue;

         float distance = g_waypoint->GetPathDistance (path->index[i], enemyIndex);

         if (distance < minDistance)
         {
            minDistance = distance;
            lookAtWaypoint = path->index[i];
         }
      }
      if (lookAtWaypoint != -1 && lookAtWaypoint < g_numWaypoints)
         *dest = g_waypoint->GetPath (lookAtWaypoint)->origin;
   }
}

void Bot::SwitchChatterIcon (bool show)
{
   // this function depending on show boolen, shows/remove chatter, icon, on the head of bot.

   if (g_gameVersion == CSV_OLD || yb_communication_type.GetInt () != 2)
      return;

   for (int i = 0; i < GetMaxClients (); i++)
   { 
      edict_t *ent = EntityOfIndex (i);

      if (!(ent->v.flags & FL_CLIENT) || (ent->v.flags & FL_FAKECLIENT) || GetTeam (ent) != m_team)
         continue;

      MESSAGE_BEGIN (MSG_ONE, g_netMsg->GetId (NETMSG_BOTVOICE), NULL, ent); // begin message
         WRITE_BYTE (show); // switch on/off
         WRITE_BYTE (GetIndex ());
      MESSAGE_END ();
   }
}

void Bot::InstantChatterMessage (int type)
{
   // this function sends instant chatter messages.

   if (yb_communication_type.GetInt () != 2 || g_chatterFactory[type].IsEmpty () || g_gameVersion == CSV_OLD || !g_sendAudioFinished)
      return;

   if (m_notKilled)
      SwitchChatterIcon (true);

   static float reportTime = GetWorldTime ();

   // delay only reportteam
   if (type == Radio_ReportTeam)
   {
      if (reportTime >= GetWorldTime ())
         return;

      reportTime = GetWorldTime () + Random.Float (30.0, 80.0);
   }

   String defaultSound = g_chatterFactory[type].GetRandomElement ().name;
   String painSound = g_chatterFactory[Chatter_DiePain].GetRandomElement ().name;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      edict_t *ent = EntityOfIndex (i);

      if (!IsValidPlayer (ent) || IsValidBot (ent) || GetTeam (ent) != m_team)
         continue;

      g_sendAudioFinished = false;

      MESSAGE_BEGIN (MSG_ONE, g_netMsg->GetId (NETMSG_SENDAUDIO), NULL, ent); // begin message
         WRITE_BYTE (GetIndex ());

         if (pev->deadflag & DEAD_DYING)
            WRITE_STRING (FormatBuffer ("%s/%s.wav", yb_chatter_path.GetString (), painSound.GetBuffer ()));
         else if (!(pev->deadflag & DEAD_DEAD))
            WRITE_STRING (FormatBuffer ("%s/%s.wav", yb_chatter_path.GetString (), defaultSound.GetBuffer ()));

         WRITE_SHORT (m_voicePitch);
      MESSAGE_END ();

      g_sendAudioFinished = true;
   }
}

void Bot::RadioMessage (int message)
{
   // this function inserts the radio message into the message queue

   if (yb_communication_type.GetInt () == 0 || GetNearbyFriendsNearPosition (pev->origin, 9999) == 0)
      return;

   if (g_chatterFactory[message].IsEmpty () || g_gameVersion == CSV_OLD || yb_communication_type.GetInt () != 2)
      g_radioInsteadVoice = true; // use radio instead voice

   m_radioSelect = message;
   PushMessageQueue (GSM_RADIO);
}

void Bot::ChatterMessage (int message)
{
   // this function inserts the voice message into the message queue (mostly same as above)

   if (yb_communication_type.GetInt () != 2 || g_chatterFactory[message].IsEmpty () || GetNearbyFriendsNearPosition (pev->origin, 9999) == 0)
      return;

   bool shouldExecute = false;

   if (m_voiceTimers[message] < GetWorldTime () || m_voiceTimers[message] == FLT_MAX)
   {
      if (m_voiceTimers[message] != FLT_MAX)
         m_voiceTimers[message] = GetWorldTime () + g_chatterFactory[message][0].repeatTime;

      shouldExecute = true;
   }

   if (!shouldExecute)
      return;

   m_radioSelect = message;
   PushMessageQueue (GSM_RADIO);
}

void Bot::CheckMessageQueue (void)
{
   // this function checks and executes pending messages

   // no new message?
   if (m_actMessageIndex == m_pushMessageIndex)
      return;

   // get message from stack
   int currentQueueMessage = GetMessageQueue ();

   // nothing to do?
   if (currentQueueMessage == GSM_IDLE || (currentQueueMessage == GSM_RADIO && yb_csdm_mode.GetInt () == 2))
      return;

   switch (currentQueueMessage)
   {
   case GSM_BUY_STUFF: // general buy message

      // buy weapon
      if (m_nextBuyTime > GetWorldTime ())
      {
         // keep sending message
         PushMessageQueue (GSM_BUY_STUFF);
         return;
      }

      if (!m_inBuyZone || yb_csdm_mode.GetBool ())
      {
         m_buyPending = true;
         m_buyingFinished = true;

         break;
      }

      m_buyPending = false;
      m_nextBuyTime = GetWorldTime () + Random.Float (0.3, 0.8);

      // if bot buying is off then no need to buy
      if (!yb_botbuy.GetBool ())
         m_buyState = 6;

      // if fun-mode no need to buy
      if (yb_jasonmode.GetBool ())
      {
         m_buyState = 6;
         SelectWeaponByName ("weapon_knife");
      }

      // prevent vip from buying
      if ((g_mapType & MAP_AS) && *(INFOKEY_VALUE (GET_INFOKEYBUFFER (GetEntity ()), "model")) == 'v')
      {
         m_isVIP = true;
         m_buyState = 6;
         m_pathType = 0;
      }

      // prevent terrorists from buying on es maps
      if ((g_mapType & MAP_ES) && m_team == TEAM_TF)
         m_buyState = 6;

      // prevent teams from buying on fun maps
      if (g_mapType & (MAP_KA | MAP_FY))
      {
         m_buyState = 6;

         if (g_mapType & MAP_KA)
            yb_jasonmode.SetInt (1);
      }

      if (m_buyState > 5)
      {
         m_buyingFinished = true;
         return;
      }

      PushMessageQueue (GSM_IDLE);
      PerformWeaponPurchase ();

      break;

   case GSM_RADIO: // general radio message issued
     // if last bot radio command (global) happened just a second ago, delay response
      if (g_lastRadioTime[m_team] + 1.0 < GetWorldTime ())
      {
         // if same message like previous just do a yes/no
         if (m_radioSelect != Radio_Affirmative && m_radioSelect != Radio_Negative)
         {
            if (m_radioSelect == g_lastRadio[m_team] && g_lastRadioTime[m_team] + 1.5 > GetWorldTime ())
               m_radioSelect = -1;
            else
            {
               if (m_radioSelect != Radio_ReportingIn)
                  g_lastRadio[m_team] = m_radioSelect;
               else
                  g_lastRadio[m_team] = -1;

               for (int i = 0; i < GetMaxClients (); i++)
               {
                  Bot *bot = g_botManager->GetBot (i);

                  if (bot != NULL)
                  {
                     if (pev != bot->pev && GetTeam (bot->GetEntity ()) == m_team)
                     {
                        bot->m_radioOrder = m_radioSelect;
                        bot->m_radioEntity = GetEntity ();
                     }
                  }
               }
            }
         }

         if (m_radioSelect == Radio_ReportingIn)
         {
            switch (GetTaskId ())
            {
            case TASK_NORMAL:
               if (GetTask ()->data != -1 && Random.Long (0, 100) < 70)
               {
                  Path *path = g_waypoint->GetPath (GetTask ()->data);

                  if (path->flags & FLAG_GOAL)
                  {
                     if ((g_mapType & MAP_DE) && m_team == TEAM_TF && m_hasC4)
                        InstantChatterMessage (Chatter_GoingToPlantBomb);
                     else
                        InstantChatterMessage (Chatter_Nothing);
                  }
                  else if (path->flags & FLAG_RESCUE)
                     InstantChatterMessage (Chatter_RescuingHostages);
                  else if ((path->flags & FLAG_CAMP) && Random.Long (0, 100) > 15)
                     InstantChatterMessage (Chatter_GoingToCamp);
                  else
                     InstantChatterMessage (Chatter_HearSomething);
               }
               else if (Random.Long (0, 100) < 40)
                  InstantChatterMessage (Chatter_ReportingIn);

               break;

            case TASK_MOVETOPOSITION:
               InstantChatterMessage (Chatter_GoingToCamp);
               break;

            case TASK_CAMP:
               if (Random.Long (0, 100) < 40)
               {

                  if (g_bombPlanted && m_team == TEAM_TF)
                     InstantChatterMessage (Chatter_GuardDroppedC4);
                  else if (m_inVIPZone && m_team == TEAM_TF)
                     InstantChatterMessage (Chatter_GuardingVipSafety);
                  else
                     InstantChatterMessage (Chatter_Camp);
               }
               break;

            case TASK_PLANTBOMB:
               InstantChatterMessage (Chatter_PlantingC4);
               break;

            case TASK_DEFUSEBOMB:
               InstantChatterMessage (Chatter_DefusingC4);
               break;

            case TASK_ATTACK:
               InstantChatterMessage (Chatter_InCombat);
               break;

            case TASK_HIDE:
            case TASK_SEEKCOVER:
               InstantChatterMessage (Chatter_SeeksEnemy);
               break;

            default:
               InstantChatterMessage (Chatter_Nothing);
               break;
            }
         }

         if (m_radioSelect != Radio_ReportingIn && g_radioInsteadVoice || yb_communication_type.GetInt () != 2 || g_chatterFactory[m_radioSelect].IsEmpty () || g_gameVersion == CSV_OLD)
         {
            if (m_radioSelect < Radio_GoGoGo)
               FakeClientCommand (GetEntity (), "radio1");
            else if (m_radioSelect < Radio_Affirmative)
            {
               m_radioSelect -= Radio_GoGoGo - 1;
               FakeClientCommand (GetEntity (), "radio2");
            }
            else
            {
               m_radioSelect -= Radio_Affirmative - 1;
               FakeClientCommand (GetEntity (), "radio3");
            }

            // select correct menu item for this radio message
            FakeClientCommand (GetEntity (), "menuselect %d", m_radioSelect);
         }
         else if (m_radioSelect != -1 && m_radioSelect != Radio_ReportingIn)
            InstantChatterMessage (m_radioSelect);

         g_radioInsteadVoice = false; // reset radio to voice
         g_lastRadioTime[m_team] = GetWorldTime (); // store last radio usage
      }
      else
         PushMessageQueue (GSM_RADIO);
      break;

   // team independent saytext
   case GSM_SAY:
      SayText (m_tempStrings);
      break;

   // team dependent saytext
   case GSM_SAY_TEAM:
      TeamSayText (m_tempStrings);
      break;

   case GSM_IDLE:
   default:
      return;
   }
}

bool Bot::IsRestricted (int weaponIndex)
{
   // this function checks for weapon restrictions.

   if (IsNullString (yb_restricted_weapons.GetString ()))
      return false; // no banned weapons

   Array <String> bannedWeapons = String (yb_restricted_weapons.GetString ()).Split (';');

   IterateArray (bannedWeapons, i)
   {
      const char *banned = STRING (GetWeaponReturn (true, NULL, weaponIndex));

      // check is this weapon is banned
      if (strncmp (bannedWeapons[i].GetBuffer (), banned, bannedWeapons[i].GetLength ()) == 0)
         return true;
   }
   return IsRestrictedAMX (weaponIndex);
}

bool Bot::IsRestrictedAMX (int weaponIndex)
{
   // this function checks restriction set by AMX Mod, this function code is courtesy of KWo.

   // check for weapon restrictions
   if ((1 << weaponIndex) & (WEAPON_PRIMARY | WEAPON_SECONDARY | WEAPON_SHIELD))
   {
      const char *restrictedWeapons = CVAR_GET_STRING ("amx_restrweapons");

      if (IsNullString (restrictedWeapons))
         return false;

      int indices[] = {4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (strlen (restrictedWeapons)))
         return false;

      return restrictedWeapons[index] != '0';
   }
   else // check for equipment restrictions
   {
      const char *restrictedEquipment = CVAR_GET_STRING ("amx_restrequipammo");

      if (IsNullString (restrictedEquipment))
         return false;

      int indices[] = {-1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 0, 1, 5};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (strlen (restrictedEquipment)))
         return false;

      return restrictedEquipment[index] != '0';
   }
}

bool Bot::IsMorePowerfulWeaponCanBeBought (void)
{
   // this function determines currently owned primary weapon, and checks if bot has
   // enough money to buy more powerful weapon.

   // if bot is not rich enough or non-standard weapon mode enabled return false
   if (g_weaponSelect[25].teamStandard != 1 || m_moneyAmount < 4000)
      return false;

   if (!IsNullString (yb_restricted_weapons.GetString ()))
   {
      Array <String> bannedWeapons = String (yb_restricted_weapons.GetString ()).Split (';');

      // check if its banned
      IterateArray (bannedWeapons, i)
      {
         if (m_currentWeapon == GetWeaponReturn (false, bannedWeapons[i].GetBuffer ()))
            return true;
      }
   }

   if (m_currentWeapon == WEAPON_SCOUT && m_moneyAmount > 5000)
      return true;
   else if (m_currentWeapon == WEAPON_MP5 && m_moneyAmount > 6000)
      return true;
   else if ((m_currentWeapon == WEAPON_M3 || m_currentWeapon == WEAPON_XM1014) && m_moneyAmount > 4000)
      return true;

   return false;
}

void Bot::PerformWeaponPurchase (void)
{
   // this function does all the work in selecting correct buy menus for most weapons/items

   WeaponSelect *selectedWeapon = NULL;
   m_nextBuyTime = GetWorldTime ();

   int count = 0, foundWeapons = 0;
   int choices[NUM_WEAPONS];

   // select the priority tab for this personality
   int *ptr = g_weaponPrefs[m_personality] + NUM_WEAPONS;

   bool isPistolMode = (g_weaponSelect[25].teamStandard == -1) && (g_weaponSelect[3].teamStandard == 2);
   bool teamEcoValid = g_botManager->EconomicsValid (m_team);

   switch (m_buyState)
   {
   case 0: // if no primary weapon and bot has some money, buy a primary weapon
      if ((!HasShield () && !HasPrimaryWeapon () && teamEcoValid) || (teamEcoValid && IsMorePowerfulWeaponCanBeBought ()))
      {
         do
         {
            bool ignoreWeapon = false;

            ptr--;

            InternalAssert (*ptr > -1);
            InternalAssert (*ptr < NUM_WEAPONS);

            selectedWeapon = &g_weaponSelect[*ptr];
            count++;

            if (selectedWeapon->buyGroup == 1)
               continue;

            // weapon available for every team?
            if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team)
               continue;

            // ignore weapon if this weapon not supported by currently running cs version...
            if (g_gameVersion == CSV_OLD && selectedWeapon->buySelect == -1)
               continue;

            // ignore weapon if this weapon is not targeted to out team....
            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team)
               continue;

            // ignore weapon if this weapon is restricted
            if (IsRestricted (selectedWeapon->id))
               continue;

            int economicsPrice = 0;

            // filter out weapons with bot economics (thanks for idea to nicebot project)
            switch (m_personality)
            {
            case PERSONALITY_RUSHER:
               economicsPrice = g_botBuyEconomyTable[8];
               break;

            case PERSONALITY_CAREFUL:
               economicsPrice = g_botBuyEconomyTable[7];
               break;

            case PERSONALITY_NORMAL:
               economicsPrice = g_botBuyEconomyTable[9];
               break;
            }

            if (m_team == TEAM_CF)
            {
               switch (selectedWeapon->id)
               {
               case WEAPON_TMP:
               case WEAPON_UMP45:
               case WEAPON_P90:
               case WEAPON_MP5:
                  if (m_moneyAmount > g_botBuyEconomyTable[1] + economicsPrice)
                     ignoreWeapon = true;
                  break;
               }

               if (selectedWeapon->id == WEAPON_SHIELD && m_moneyAmount > g_botBuyEconomyTable[10])
                  ignoreWeapon = true;
            }
            else if (m_team == TEAM_TF)
            {
               switch (selectedWeapon->id)
               {
               case WEAPON_UMP45:
               case WEAPON_MAC10:
               case WEAPON_P90:
               case WEAPON_MP5:
               case WEAPON_SCOUT:
                  if (m_moneyAmount > g_botBuyEconomyTable[2] + economicsPrice)
                     ignoreWeapon = true;
                  break;
               }
            }

            switch (selectedWeapon->id)
            {
            case WEAPON_XM1014:
            case WEAPON_M3:
               if (m_moneyAmount < g_botBuyEconomyTable[3] || m_moneyAmount > g_botBuyEconomyTable[4])
                  ignoreWeapon = true;
               break;
            }

            switch (selectedWeapon->id)
            {
            case WEAPON_SG550:
            case WEAPON_G3SG1:
            case WEAPON_AWP:
            case WEAPON_M249:
               if (m_moneyAmount < g_botBuyEconomyTable[5] || m_moneyAmount > g_botBuyEconomyTable[6])
                  ignoreWeapon = true;
               break;
            }

            if (ignoreWeapon && g_weaponSelect[25].teamStandard == 1 && yb_economics_rounds.GetBool ())
               continue;

            int moneySave = Random.Long (900, 1100);

            if (g_botManager->GetLastWinner () == m_team)
               moneySave = 0;

            if (selectedWeapon->price <= (m_moneyAmount - moneySave))
               choices[foundWeapons++] = *ptr;

         } while (count < NUM_WEAPONS && foundWeapons < 4);

         // found a desired weapon?
         if (foundWeapons > 0)
         {
            int chosenWeapon;

            // choose randomly from the best ones...
            if (foundWeapons > 1)
               chosenWeapon = choices[Random.Long (0, foundWeapons - 1)];
            else
               chosenWeapon = choices[foundWeapons - 1];

            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else
            selectedWeapon = NULL;

         if (selectedWeapon != NULL)
         {
            FakeClientCommand (GetEntity (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (g_gameVersion == CSV_OLD)
               FakeClientCommand(GetEntity (), "menuselect %d", selectedWeapon->buySelect);
            else // SteamCS buy menu is different from the old one
            {
               if (m_team == TEAM_TF)
                  FakeClientCommand(GetEntity (), "menuselect %d", selectedWeapon->newBuySelectT);
               else
                  FakeClientCommand (GetEntity (), "menuselect %d", selectedWeapon->newBuySelectCT);
            }
         }
      }
      else if (HasPrimaryWeapon () && !HasShield ())
      {
         m_reloadState = RELOAD_PRIMARY;
         break;
      } 
      else if ((HasSecondaryWeapon () && !HasShield ()) || HasShield())
      {
         m_reloadState = RELOAD_SECONDARY;
         break;
      }   

   case 1: // if armor is damaged and bot has some money, buy some armor
      if (pev->armorvalue < Random.Long (50, 80) && (isPistolMode || (teamEcoValid && HasPrimaryWeapon ())))
      {
         // if bot is rich, buy kevlar + helmet, else buy a single kevlar
         if (m_moneyAmount > 1500 && !IsRestricted (WEAPON_ARMORHELM))
            FakeClientCommand (GetEntity (), "buyequip;menuselect 2");
         else if (!IsRestricted (WEAPON_ARMOR))
            FakeClientCommand (GetEntity (), "buyequip;menuselect 1");
      }
      break;

   case 2: // if bot has still some money, buy a better secondary weapon
      if (isPistolMode || (HasPrimaryWeapon () && (pev->weapons & ((1 << WEAPON_USP) | (1 << WEAPON_GLOCK))) && m_moneyAmount > Random.Long (7500, 9000)))
      {
         do
         {
            ptr--;

            InternalAssert (*ptr > -1);
            InternalAssert (*ptr < NUM_WEAPONS);

            selectedWeapon = &g_weaponSelect[*ptr];
            count++;

            if (selectedWeapon->buyGroup != 1)
               continue;

            // ignore weapon if this weapon is restricted
            if (IsRestricted (selectedWeapon->id))
               continue;

            // weapon available for every team?
            if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team)
               continue;

            if (g_gameVersion == CSV_OLD && selectedWeapon->buySelect == -1)
               continue;

            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team)
               continue;

            if (selectedWeapon->price <= (m_moneyAmount - Random.Long (100, 200)))
               choices[foundWeapons++] = *ptr;

         } while (count < NUM_WEAPONS && foundWeapons < 4);

         // found a desired weapon?
         if (foundWeapons > 0)
         {
            int chosenWeapon;

            // choose randomly from the best ones...
            if (foundWeapons > 1)
               chosenWeapon = choices[Random.Long (0, foundWeapons - 1)];
            else
               chosenWeapon = choices[foundWeapons - 1];

            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else
            selectedWeapon = NULL;

         if (selectedWeapon != NULL)
         {
            FakeClientCommand (GetEntity (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (g_gameVersion == CSV_OLD)
               FakeClientCommand(GetEntity (), "menuselect %d", selectedWeapon->buySelect);
         
            else // steam cs buy menu is different from old one
            {
               if (GetTeam(GetEntity()) == TEAM_TF)
                  FakeClientCommand(GetEntity(), "menuselect %d", selectedWeapon->newBuySelectT);
               else
                  FakeClientCommand(GetEntity(), "menuselect %d", selectedWeapon->newBuySelectCT);
            }
         }
      }
      break;

   case 3: // if bot has still some money, choose if bot should buy a grenade or not
      if (Random.Long (1, 100) < g_grenadeBuyPrecent[0] && m_moneyAmount >= 400 && !IsRestricted (WEAPON_EXPLOSIVE))
      {
         // buy a he grenade
         FakeClientCommand (GetEntity (), "buyequip");
         FakeClientCommand (GetEntity (), "menuselect 4");
      }

      if (Random.Long (1, 100) < g_grenadeBuyPrecent[1] && m_moneyAmount >= 300 && teamEcoValid && !IsRestricted (WEAPON_FLASHBANG))
      {
         // buy a concussion grenade, i.e., 'flashbang'
         FakeClientCommand (GetEntity (), "buyequip");
         FakeClientCommand (GetEntity (), "menuselect 3");
      }

      if (Random.Long (1, 100) < g_grenadeBuyPrecent[2] && m_moneyAmount >= 400 && teamEcoValid && !IsRestricted (WEAPON_SMOKE))
      {
         // buy a smoke grenade
         FakeClientCommand (GetEntity (), "buyequip");
         FakeClientCommand (GetEntity (), "menuselect 5");
      }
      break;

   case 4: // if bot is CT and we're on a bomb map, randomly buy the defuse kit
      if ((g_mapType & MAP_DE) && m_team == TEAM_CF && Random.Long (1, 100) < 80 && m_moneyAmount > 200 && !IsRestricted (WEAPON_DEFUSER))
      {
         if (g_gameVersion == CSV_OLD)
            FakeClientCommand (GetEntity (), "buyequip;menuselect 6");
         else
            FakeClientCommand (GetEntity (), "defuser"); // use alias in SteamCS
      }
      break;

   case 5: // buy enough primary & secondary ammo (do not check for money here)
      for (int i = 0; i <= 5; i++)
         FakeClientCommand (GetEntity (), "buyammo%d", Random.Long (1, 2)); // simulate human

      // buy enough secondary ammo
      if (HasPrimaryWeapon ())
         FakeClientCommand (GetEntity (), "buy;menuselect 7");

      // buy enough primary ammo
      FakeClientCommand (GetEntity (), "buy;menuselect 6");

      // try to reload secondary weapon
      if (m_reloadState != RELOAD_PRIMARY)
         m_reloadState = RELOAD_SECONDARY;

      break;
   }

   m_buyState++;
   PushMessageQueue (GSM_BUY_STUFF);
}

TaskItem *ClampDesire (TaskItem *first, float min, float max)
{
   // this function iven some values min and max, clamp the inputs to be inside the [min, max] range.

   if (first->desire < min)
      first->desire = min;
   else if (first->desire > max)
      first->desire = max;

   return first;
}

TaskItem *MaxDesire (TaskItem *first, TaskItem *second)
{
   // this function returns the behavior having the higher activation level.

   if (first->desire > second->desire)
      return first;

   return second;
}

TaskItem *SubsumeDesire (TaskItem *first, TaskItem *second)
{
   // this function returns the first behavior if its activation level is anything higher than zero.

   if (first->desire > 0)
      return first;

   return second;
}

TaskItem *ThresholdDesire (TaskItem *first, float threshold, float desire)
{
   // this function returns the input behavior if it's activation level exceeds the threshold, or some default
   // behavior otherwise.

   if (first->desire < threshold)
      first->desire = desire;

   return first;
}

float HysteresisDesire (float cur, float min, float max, float old)
{
   // this function clamp the inputs to be the last known value outside the [min, max] range.

   if (cur <= min || cur >= max)
      old = cur;

   return old;
}

void Bot::SetConditions (void)
{
   // this function carried out each frame. does all of the sensing, calculates emotions and finally sets the desired
   // action after applying all of the Filters

   m_aimFlags = 0;

   // slowly increase/decrease dynamic emotions back to their base level
   if (m_nextEmotionUpdate < GetWorldTime ())
   {
      if (m_difficulty == 4)
      {
         m_agressionLevel *= 2;
         m_fearLevel /= 2;
      }

      if (m_agressionLevel > m_baseAgressionLevel)
         m_agressionLevel -= 0.10;
      else
         m_agressionLevel += 0.10;

      if (m_fearLevel > m_baseFearLevel)
         m_fearLevel -= 0.05;
      else
         m_fearLevel += 0.05;

      if (m_agressionLevel < 0.0)
         m_agressionLevel = 0.0;

      if (m_fearLevel < 0.0)
         m_fearLevel = 0.0;

      m_nextEmotionUpdate = GetWorldTime () + 1.0;
   }

   // does bot see an enemy?
   if (LookupEnemy ())
      m_states |= STATE_SEEING_ENEMY;
   else
   {
      m_states &= ~STATE_SEEING_ENEMY;
      m_enemy = NULL;
   }

   // did bot just kill an enemy?
   if (!IsEntityNull (m_lastVictim))
   {
      if (GetTeam (m_lastVictim) != m_team)
      {
         // add some aggression because we just killed somebody
         m_agressionLevel += 0.1;

         if (m_agressionLevel > 1.0)
            m_agressionLevel = 1.0;

         if (Random.Long (1, 100) < 10)
            ChatMessage (CHAT_KILLING);

         if (Random.Long (1, 100) < 10)
            RadioMessage (Radio_EnemyDown);
         else
         {   
            if ((m_lastVictim->v.weapons & (1 << WEAPON_AWP)) || (m_lastVictim->v.weapons & (1 << WEAPON_SCOUT)) ||  (m_lastVictim->v.weapons & (1 << WEAPON_G3SG1)) || (m_lastVictim->v.weapons & (1 << WEAPON_SG550)))
               ChatterMessage (Chatter_SniperKilled);
            else
            { 
               switch (GetNearbyEnemiesNearPosition (pev->origin, 9999))
               { 
               case 0:
                  if (Random.Long (0, 100) < 50)
                     ChatterMessage (Chatter_NoEnemiesLeft);
                  else
                     ChatterMessage (Chatter_EnemyDown);
                  break;

               case 1:
                  ChatterMessage (Chatter_OneEnemyLeft);
                  break;

               case 2:
                  ChatterMessage (Chatter_TwoEnemiesLeft);
                  break;

               case 3:
                  ChatterMessage (Chatter_ThreeEnemiesLeft);
                  break;

               default:
                  ChatterMessage (Chatter_EnemyDown);
               }
            }
         }

         // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
         if (m_team == TEAM_CF && m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0 && g_bombPlanted)
         {
            SelectWeaponByName ("weapon_knife");
            m_plantedBombWptIndex = FindPlantedBomb ();

            if (IsPointOccupied (m_plantedBombWptIndex))
               InstantChatterMessage (Chatter_BombSiteSecured);
             
         }
      }
      else
      {
         ChatMessage (CHAT_TEAMKILL, true);
         ChatterMessage (Chatter_TeamKill);
      }
      m_lastVictim = NULL;
   }

   // check if our current enemy is still valid
   if (!IsEntityNull (m_lastEnemy))
   {
      if (!IsAlive (m_lastEnemy) && m_shootAtDeadTime < GetWorldTime ())
      {
         m_lastEnemyOrigin = nullvec;
         m_lastEnemy = NULL;
      }
   }
   else
   {
      m_lastEnemyOrigin = nullvec;
      m_lastEnemy = NULL;
   }

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (!yb_ignore_enemies.GetBool () && m_soundUpdateTime <= GetWorldTime () && m_blindTime < GetWorldTime ())
   {
      ReactOnSound ();
      m_soundUpdateTime = GetWorldTime () + yb_timersound.GetFloat ();
   }
   else if (m_heardSoundTime < GetWorldTime ())
      m_states &= ~STATE_HEARING_ENEMY;

   if (IsEntityNull (m_enemy) && !IsEntityNull (m_lastEnemy) && m_lastEnemyOrigin != nullvec)
   {
      TraceResult tr;
      TraceLine (EyePosition (), m_lastEnemyOrigin, true, GetEntity (), &tr);

      if ((pev->origin - m_lastEnemyOrigin).GetLength () < 1600.0 && (tr.flFraction >= 0.2 || tr.pHit != g_worldEdict))
      {
         m_aimFlags |= AIM_PREDICT_PATH;

         if (EntityIsVisible (m_lastEnemyOrigin))
            m_aimFlags |= AIM_LAST_ENEMY;
      }
   }

   // check if throwing a grenade is a good thing to do...
   if (m_lastEnemy != NULL && !yb_ignore_enemies.GetBool() && !yb_jasonmode.GetBool() && m_grenadeCheckTime < GetWorldTime() && !m_isUsingGrenade && GetTaskId() != TASK_PLANTBOMB && GetTaskId() != TASK_DEFUSEBOMB && !m_isReloading && IsAlive(m_lastEnemy))
   {
      // check again in some seconds
      m_grenadeCheckTime = GetWorldTime () + yb_timergrenade.GetFloat ();

      // check if we have grenades to throw
      int grenadeToThrow = CheckGrenades ();

      // if we don't have grenades no need to check it this round again
      if (grenadeToThrow == -1)
      {
         m_grenadeCheckTime = GetWorldTime () + 15.0; // changed since, conzero can drop grens from dead players
         m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG);
      }
      else
      {
         // care about different types of grenades
         if ((grenadeToThrow == WEAPON_EXPLOSIVE || grenadeToThrow == WEAPON_SMOKE) && Random.Long (0, 100) < 45 && !(m_states & (STATE_SEEING_ENEMY | STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG)))
         {
            float distance = (m_lastEnemy->v.origin - pev->origin).GetLength ();

            // is enemy to high to throw
            if ((m_lastEnemy->v.origin.z > (pev->origin.z + 650.0)) || !(m_lastEnemy->v.flags & (FL_ONGROUND | FL_DUCKING)))
               distance = FLT_MAX; // just some crazy value

            // enemy is within a good throwing distance ?               
            if (distance > (grenadeToThrow == WEAPON_SMOKE ? 400 : 600) && distance <= 1000)
            {
               // care about different types of grenades
               if (grenadeToThrow == WEAPON_EXPLOSIVE)
               {
                  bool allowThrowing = true;

                  // check for teammates
                  if (GetNearbyFriendsNearPosition (m_lastEnemy->v.origin, 256) > 0)
                     allowThrowing = false;

                  if (allowThrowing && m_seeEnemyTime + 2.0 < GetWorldTime ())
                  {
                     Vector enemyPredict = ((m_lastEnemy->v.velocity * 0.5).SkipZ () + m_lastEnemy->v.origin);
                     int searchTab[4], count = 4;

                     float searchRadius = m_lastEnemy->v.velocity.GetLength2D ();

                     // check the search radius
                     if (searchRadius < 128.0)
                        searchRadius = 128.0;

                     // search waypoints
                     g_waypoint->FindInRadius (enemyPredict, searchRadius, searchTab, &count);

                     while (count > 0)
                     {
                        allowThrowing = true;

                        // check the throwing
                        m_throw = g_waypoint->GetPath (searchTab[count--])->origin;
                        Vector src = CheckThrow (EyePosition (), m_throw);

                        if (src.GetLengthSquared () < 100.0)
                           src = CheckToss (EyePosition (), m_throw);

                        if (src == nullvec)
                           allowThrowing = false;
                        else
                           break;
                     }
                  }

                  // start explosive grenade throwing?
                  if (allowThrowing)
                     m_states |= STATE_THROW_HE;
                  else
                     m_states &= ~STATE_THROW_HE;
               }
               else if (grenadeToThrow == WEAPON_SMOKE)
               {
                  // start smoke grenade throwing?
                  if ((m_states & STATE_SEEING_ENEMY) && GetShootingConeDeviation (m_enemy, &pev->origin) >= 0.70 && m_seeEnemyTime + 2.0 < GetWorldTime ())
                     m_states &= ~STATE_THROW_SG;
                  else
                     m_states |= STATE_THROW_SG;
               }
            }
         }
         else if (IsAlive (m_lastEnemy) && grenadeToThrow == WEAPON_FLASHBANG && (m_lastEnemy->v.origin - pev->origin).GetLength () < 800 && !(m_aimFlags & AIM_ENEMY) && Random.Long (0, 100) < 50)
         {
            bool allowThrowing = true;
            Array <int> inRadius;

            g_waypoint->FindInRadius (inRadius, 256, m_lastEnemy->v.origin + (m_lastEnemy->v.velocity * 0.5).SkipZ ());

            IterateArray (inRadius, i)
            {
               Path *path = g_waypoint->GetPath (i);

               if (GetNearbyFriendsNearPosition (path->origin, 256) != 0 || !(m_difficulty == 4 && GetNearbyFriendsNearPosition (path->origin, 256) != 0))
                  continue;

               m_throw = path->origin;
               Vector src = CheckThrow (EyePosition (), m_throw);

               if (src.GetLengthSquared () < 100)
                  src = CheckToss (EyePosition (), m_throw);

               if (src == nullvec)
                  continue;

               allowThrowing = true;
               break;
            }

            // start concussion grenade throwing?
            if (allowThrowing  && m_seeEnemyTime + 2.0 < GetWorldTime ())
               m_states |= STATE_THROW_FB;
            else
               m_states &= ~STATE_THROW_FB;
         }

         if (m_states & STATE_THROW_HE)
            StartTask (TASK_THROWHEGRENADE, TASKPRI_THROWGRENADE, -1, GetWorldTime () + 3.0, false);
         else if (m_states & STATE_THROW_FB)
            StartTask (TASK_THROWFLASHBANG, TASKPRI_THROWGRENADE, -1, GetWorldTime () + 3.0, false);
         else if (m_states & STATE_THROW_SG)
            StartTask (TASK_THROWSMOKE, TASKPRI_THROWGRENADE, -1, GetWorldTime () + 3.0, false);

         // delay next grenade throw
         if (m_states & (STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG))
            m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;
      }
   }
   else
      m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG);

   // check if there are items needing to be used/collected
   if (m_itemCheckTime < GetWorldTime () || !IsEntityNull (m_pickupItem))
   {
      m_itemCheckTime = GetWorldTime () + yb_timerpickup.GetFloat ();
      FindItem ();
   }

   float tempFear = m_fearLevel;
   float tempAgression = m_agressionLevel;

   // decrease fear if teammates near
   int friendlyNum = 0;

   if (m_lastEnemyOrigin != nullvec)
      friendlyNum = GetNearbyFriendsNearPosition (pev->origin, 500) - GetNearbyEnemiesNearPosition (m_lastEnemyOrigin, 500);

   if (friendlyNum > 0)
      tempFear = tempFear * 0.5;

   // increase/decrease fear/aggression if bot uses a sniping weapon to be more careful
   if (UsesSniper ())
   {
      tempFear = tempFear * 1.5;
      tempAgression = tempAgression * 0.5;
   }

   // initialize & calculate the desire for all actions based on distances, emotions and other stuff
   GetTask ();

   // bot found some item to use?
   if (!IsEntityNull (m_pickupItem) && GetTaskId () != TASK_ESCAPEFROMBOMB)
   {
      m_states |= STATE_PICKUP_ITEM;

      if (m_pickupType == PICKUP_BUTTON)
         g_taskFilters[TASK_PICKUPITEM].desire = 50; // always pickup button
      else
      {
         float distance = (500.0 - (GetEntityOrigin (m_pickupItem) - pev->origin).GetLength ()) * 0.2;

         if (distance > 50)
            distance = 50;

         g_taskFilters[TASK_PICKUPITEM].desire = distance;
      }
   }
   else
   {
      m_states &= ~STATE_PICKUP_ITEM;
      g_taskFilters[TASK_PICKUPITEM].desire = 0.0;
   }

   float desireLevel = 0.0;

   // calculate desire to attack
   if ((m_states & STATE_SEEING_ENEMY) && ReactOnEnemy ())
      g_taskFilters[TASK_ATTACK].desire = TASKPRI_ATTACK;
   else
      g_taskFilters[TASK_ATTACK].desire = 0;

   // calculate desires to seek cover or hunt
   if (IsValidPlayer (m_lastEnemy) && m_lastEnemyOrigin != nullvec)
   {
      float distance = (m_lastEnemyOrigin - pev->origin).GetLength ();

      // retreat level depends on bot health
      float retreatLevel = (100.0 - (pev->health > 100.0 ? 100.0 : pev->health)) * tempFear;
      float timeSeen = m_seeEnemyTime - GetWorldTime ();
      float timeHeard = m_heardSoundTime - GetWorldTime ();
      float ratio = 0.0;

      if (timeSeen > timeHeard)
      {
         timeSeen += 10.0;
         ratio = timeSeen * 0.1;
      }
      else
      {
         timeHeard += 10.0;
         ratio = timeHeard * 0.1;
      }

      if (g_bombPlanted || m_isStuck)
         ratio /= 3; // reduce the seek cover desire if bomb is planted
      else if (m_isVIP || m_isReloading)
         ratio *= 2; // triple the seek cover desire if bot is VIP or reloading

      if (distance > 500.0)
         g_taskFilters[TASK_SEEKCOVER].desire = retreatLevel * ratio;

      // if half of the round is over, allow hunting
      // FIXME: it probably should be also team/map dependant
      if (IsEntityNull (m_enemy) && (g_timeRoundMid < GetWorldTime ()) && !m_isUsingGrenade && m_currentWaypointIndex != g_waypoint->FindNearest (m_lastEnemyOrigin) && m_personality != PERSONALITY_CAREFUL)
      {
         desireLevel = 4096.0 - ((1.0 - tempAgression) * distance);
         desireLevel = (100 * desireLevel) / 4096.0;
         desireLevel -= retreatLevel;

         if (desireLevel > 89)
            desireLevel = 89;

         g_taskFilters[TASK_HUNTENEMY].desire = desireLevel;
      }
      else
         g_taskFilters[TASK_HUNTENEMY].desire = 0;
   }
   else
   {
      g_taskFilters[TASK_SEEKCOVER].desire = 0;
      g_taskFilters[TASK_HUNTENEMY].desire = 0;
   }

   // blinded behaviour
   if (m_blindTime > GetWorldTime ())
      g_taskFilters[TASK_BLINDED].desire = TASKPRI_BLINDED;
   else
      g_taskFilters[TASK_BLINDED].desire = 0.0;

   // now we've initialized all the desires go through the hard work
   // of filtering all actions against each other to pick the most
   // rewarding one to the bot.

   // FIXME: instead of going through all of the actions it might be
   // better to use some kind of decision tree to sort out impossible
   // actions.

   // most of the values were found out by trial-and-error and a helper
   // utility i wrote so there could still be some weird behaviors, it's
   // hard to check them all out.

   m_oldCombatDesire = HysteresisDesire (g_taskFilters[TASK_ATTACK].desire, 40.0, 90.0, m_oldCombatDesire);
   g_taskFilters[TASK_ATTACK].desire = m_oldCombatDesire;

   TaskItem *taskOffensive = &g_taskFilters[TASK_ATTACK];
   TaskItem *taskPickup = &g_taskFilters[TASK_PICKUPITEM];

   // calc survive (cover/hide)
   TaskItem *taskSurvive = ThresholdDesire (&g_taskFilters[TASK_SEEKCOVER], 40.0, 0.0);
   taskSurvive = SubsumeDesire (&g_taskFilters[TASK_HIDE], taskSurvive);

   TaskItem *def = ThresholdDesire (&g_taskFilters[TASK_HUNTENEMY], 41.0, 0.0); // don't allow hunting if desire's 60<
   taskOffensive = SubsumeDesire (taskOffensive, taskPickup); // if offensive task, don't allow picking up stuff

   TaskItem *taskSub = MaxDesire (taskOffensive, def); // default normal & careful tasks against offensive actions
   TaskItem *final = SubsumeDesire (&g_taskFilters[TASK_BLINDED], MaxDesire (taskSurvive, taskSub)); // reason about fleeing instead

   if (!m_tasks.IsEmpty ())
   {
      final = MaxDesire (final, GetTask ());
      StartTask (final->id, final->desire, final->data, final->time, final->resume); // push the final behaviour in our task stack to carry out
   }
}

void Bot::ResetTasks (void)
{
   // this function resets bot tasks stack, by removing all entries from the stack.

   m_tasks.RemoveAll ();
}

void Bot::StartTask (TaskId_t id, float desire, int data, float time, bool resume)
{
   if (!m_tasks.IsEmpty ())
   {
      TaskItem &item = m_tasks.Last ();

      if (item.id == id)
      {
         item.desire = desire;
         return;
      }
   }
   TaskItem item;

   item.id = id;
   item.desire = desire;
   item.data = data;
   item.time = time;
   item.resume = resume;

   m_tasks.Push (item);

   DeleteSearchNodes ();
   m_lastCollTime = GetWorldTime () + 0.5;

   // leader bot?
   if (m_isLeader && GetTaskId () == TASK_SEEKCOVER)
      CommandTeam (); // reorganize team if fleeing

   if (GetTaskId () == TASK_CAMP)
      SelectBestWeapon ();

   // this is best place to handle some voice commands report team some info
   if (Random.Long (0, 100) < 95)
   {
      switch (GetTaskId ())
      {
      case TASK_BLINDED:
         InstantChatterMessage (Chatter_GotBlinded);
         break;

      case TASK_PLANTBOMB:
         InstantChatterMessage (Chatter_PlantingC4);
         break;
      }
   }

   if (Random.Long (0, 100) < 80 && GetTaskId () == TASK_CAMP)
   {
      if ((g_mapType & MAP_DE) && g_bombPlanted)
         ChatterMessage (Chatter_GuardDroppedC4);
      else
         ChatterMessage (Chatter_GoingToCamp);
   }

   if (yb_debug_goal.GetInt () != -1)
      m_chosenGoalIndex = yb_debug_goal.GetInt ();
   else
      m_chosenGoalIndex = GetTask ()->data;

   if (Random.Long (0, 100) < 80 && GetTaskId () == TASK_CAMP && m_team == TEAM_TF && m_inVIPZone)
      ChatterMessage (Chatter_GoingToGuardVIPSafety);
}

TaskItem *Bot::GetTask (void)
{
   if (m_tasks.IsEmpty ())
   {
      m_tasks.RemoveAll ();

      TaskItem task;

      task.id = TASK_NORMAL;
      task.desire = TASKPRI_NORMAL;
      task.data = -1;
      task.time = 0.0;
      task.resume = true;

      m_tasks.Push (task);
   }
   return &m_tasks.Last ();
}

void Bot::RemoveCertainTask (TaskId_t id)
{
   // this function removes one task from the bot task stack.

   if (m_tasks.IsEmpty () || (!m_tasks.IsEmpty () && GetTaskId () == TASK_NORMAL))
      return; // since normal task can be only once on the stack, don't remove it...

   if (GetTaskId () == id)
   {
      DeleteSearchNodes ();
      m_tasks.Pop ();

      return;
   }

   IterateArray (m_tasks, i)
   {
      if (m_tasks[i].id == id)
         m_tasks.RemoveAt (i);
   }
   DeleteSearchNodes ();
}

void Bot::TaskComplete (void)
{
   // this function called whenever a task is completed.

   if (m_tasks.IsEmpty ())
      return;

   do
   {
      m_tasks.Pop ();

   } while (!m_tasks.IsEmpty () && !m_tasks.Last ().resume); 

   DeleteSearchNodes ();
}

bool Bot::EnemyIsThreat (void)
{
   if (IsEntityNull (m_enemy) || GetTaskId () == TASK_SEEKCOVER)
      return false;

   float distance = (m_enemy->v.origin - pev->origin).GetLength ();

   // if bot is camping, he should be firing anyway and not leaving his position
   if (GetTaskId () == TASK_CAMP)
      return false;

   // if enemy is near or facing us directly
   if (distance < 256.0f || IsInViewCone (m_enemy->v.origin))
      return true;

   return false;
}

bool Bot::ReactOnEnemy (void)
{
   // the purpose of this function is check if task has to be interrupted because an enemy is near (run attack actions then)

   if (!EnemyIsThreat ())
      return false;

   if (m_enemyReachableTimer < GetWorldTime ())
   {
      int i = g_waypoint->FindNearest (pev->origin);
      int enemyIndex = g_waypoint->FindNearest (m_enemy->v.origin);
      
      float lineDist = (m_enemy->v.origin - pev->origin).GetLength ();
      float pathDist = g_waypoint->GetPathDistance (i, enemyIndex);

      if (pathDist - lineDist > 112.0)
         m_isEnemyReachable = false;
      else
         m_isEnemyReachable = true;

      m_enemyReachableTimer = GetWorldTime () + 1.0;
   }

   if (m_isEnemyReachable)
   {
      m_navTimeset = GetWorldTime (); // override existing movement by attack movement
      return true;
   }
   return false;
}

bool Bot::IsLastEnemyViewable (void)
{
   // this function checks if line of sight established to last enemy

   if (IsEntityNull (m_lastEnemy) || m_lastEnemyOrigin == nullvec)
   {
      m_lastEnemy = NULL;
      m_lastEnemyOrigin = nullvec;

      return false;
   }

   // trace a line from bot's eyes to destination...
   TraceResult tr;
   TraceLine (EyePosition (), m_lastEnemyOrigin, true, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0;
}

bool Bot::LastEnemyShootable (void)
{
   // don't allow shooting through walls
   if (!(m_aimFlags & AIM_LAST_ENEMY) || IsEntityNull (m_lastEnemy) || GetTaskId () == TASK_PAUSE || m_lastEnemyOrigin != nullvec)
      return false;

   return GetShootingConeDeviation (GetEntity (), &m_lastEnemyOrigin) >= 0.90 && IsShootableThruObstacle (m_lastEnemy->v.origin);
}

void Bot::CheckRadioCommands (void)
{
   // this function handling radio and reactings to it

   float distance = (m_radioEntity->v.origin - pev->origin).GetLength ();

   // don't allow bot listen you if bot is busy
   if ((GetTaskId () == TASK_DEFUSEBOMB || GetTaskId () == TASK_PLANTBOMB || HasHostage () || m_hasC4) && m_radioOrder != Radio_ReportTeam)
   {
      m_radioOrder = 0;
      return;
   }

   switch (m_radioOrder)
   {
   case Radio_CoverMe:
   case Radio_FollowMe: 
   case Radio_StickTogether:
   case Chatter_GoingToPlantBomb:
   case Chatter_CoverMe:
      // check if line of sight to object is not blocked (i.e. visible)
      if ((EntityIsVisible (m_radioEntity->v.origin)) || (m_radioOrder == Radio_StickTogether))
      {
         if (IsEntityNull (m_targetEntity) && IsEntityNull (m_enemy) && Random.Long (0, 100) < (m_personality == PERSONALITY_CAREFUL ? 80 : 20))
         {
            int numFollowers = 0;

            // Check if no more followers are allowed
            for (int i = 0; i < GetMaxClients (); i++)
            {
               Bot *bot = g_botManager->GetBot (i);

               if (bot != NULL)
               {
                  if (IsAlive (bot->GetEntity ()))
                  {
                     if (bot->m_targetEntity == m_radioEntity)
                        numFollowers++;
                  }
               }
            }

            int allowedFollowers = yb_user_max_followers.GetInt ();

            if (m_radioEntity->v.weapons & (1 << WEAPON_C4))
               allowedFollowers = 1;

            if (numFollowers < allowedFollowers)
            {
               RadioMessage (Radio_Affirmative);
               m_targetEntity = m_radioEntity;

               // don't pause/camp/follow anymore
               TaskId_t taskID = GetTaskId ();

               if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
                  GetTask ()->time = GetWorldTime ();

               StartTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, -1, 0.0, true);
            }
            else if (numFollowers > allowedFollowers)
            {
               for (int i = 0; (i < GetMaxClients () && numFollowers > allowedFollowers) ; i++)
               {
                  Bot *bot = g_botManager->GetBot (i);

                  if (bot != NULL)
                  {
                     if (IsAlive (bot->GetEntity ()))
                     {
                        if (bot->m_targetEntity == m_radioEntity)
                        {
                           bot->m_targetEntity = NULL;
                           numFollowers--;
                        }
                     }
                  }
               }
            }
            else if (m_radioOrder != Chatter_GoingToPlantBomb)
               RadioMessage (Radio_Negative);
         }
         else if (m_radioOrder != Chatter_GoingToPlantBomb)
            RadioMessage (Radio_Negative);
      }
      break;

   case Radio_HoldPosition:
      if (!IsEntityNull (m_targetEntity))
      {
         if (m_targetEntity == m_radioEntity)
         {
            m_targetEntity = NULL;
            RadioMessage (Radio_Affirmative);

            m_campButtons = 0;

            StartTask (TASK_PAUSE, TASKPRI_PAUSE, -1, GetWorldTime () + Random.Float (30.0, 60.0), false);
         }
      }
      break;

   case Chatter_NewRound:
       ChatterMessage (Chatter_You_Heard_The_Man);   
       break;

   case Radio_TakingFire:
      if (IsEntityNull (m_targetEntity))
      {
         if (IsEntityNull (m_enemy) && m_seeEnemyTime + 4.0 < GetWorldTime ())
         {
            // Decrease Fear Levels to lower probability of Bot seeking Cover again
            m_fearLevel -= 0.2;

            if (m_fearLevel < 0.0)
               m_fearLevel = 0.0;

            if (Random.Long (0, 100) < 45 && yb_communication_type.GetInt () == 2)
               ChatterMessage (Chatter_OnMyWay);
            else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.GetInt () != 2)
               RadioMessage (Radio_Affirmative);

            TryHeadTowardRadioEntity ();
         }
         else if (Random.Long (0, 100) < 80)
            RadioMessage (Radio_Negative);
      }
      break;

   case Radio_YouTakePoint:
      if (EntityIsVisible (m_radioEntity->v.origin) && m_isLeader)
         RadioMessage (Radio_Affirmative);
      break;

   case Radio_EnemySpotted:
   case Radio_NeedBackup:
   case Chatter_ScaredEmotion:
   case Chatter_Pinned_Down:
      if (((IsEntityNull (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 2048 || !m_moveToC4) && Random.Long (0, 100) > 50 && m_seeEnemyTime + 4.0 < GetWorldTime ())
      {
         m_fearLevel -= 0.1;

         if (m_fearLevel < 0.0)
            m_fearLevel = 0.0;

         if (Random.Long (0, 100) < 45 && yb_communication_type.GetInt () == 2)
            ChatterMessage (Chatter_OnMyWay);
         else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.GetInt () != 2)
            RadioMessage (Radio_Affirmative);

         TryHeadTowardRadioEntity ();
      }
      else if (Random.Long (0, 100) < 80 && m_radioOrder == Radio_NeedBackup)
         RadioMessage (Radio_Negative);
      break;

   case Radio_GoGoGo:
      if (m_radioEntity == m_targetEntity)
      {
         if (Random.Long (0, 100) < 45 && yb_communication_type.GetInt () == 2)
            RadioMessage (Radio_Affirmative);         
         else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.GetInt () != 2)
            RadioMessage (Radio_Affirmative);

         m_targetEntity = NULL;
         m_fearLevel -= 0.2;

         if (m_fearLevel < 0.0)
            m_fearLevel = 0.0;
      }
      else if ((IsEntityNull (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 2048)
      {
         TaskId_t taskID = GetTaskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
         {
            m_fearLevel -= 0.2;

            if (m_fearLevel < 0.0)
               m_fearLevel = 0.0;
         
            RadioMessage (Radio_Affirmative);
            // don't pause/camp anymore
            GetTask ()->time = GetWorldTime ();

            m_targetEntity = NULL;
            MakeVectors (m_radioEntity->v.v_angle);

            m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * Random.Long (1024, 2048);

            DeleteSearchNodes ();
            StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0, true);
         }
      }
      else if (!IsEntityNull (m_doubleJumpEntity))
      {
         RadioMessage (Radio_Affirmative);
         ResetDoubleJumpState ();
      }
      else
         RadioMessage (Radio_Negative);
      break;

   case Radio_ShesGonnaBlow:
      if (IsEntityNull (m_enemy) && distance < 2048 && g_bombPlanted && m_team == TEAM_TF)
      {
         RadioMessage (Radio_Affirmative);

         if (GetTaskId () == TASK_CAMP)
            RemoveCertainTask (TASK_CAMP);

         m_targetEntity = NULL;
         StartTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0, true);
      }
      else
        RadioMessage (Radio_Negative);

      break;

   case Radio_RegroupTeam:
      // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
      if ((m_team == TEAM_CF) && m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0 && g_bombPlanted && GetTaskId () != TASK_DEFUSEBOMB)
      {
         SelectWeaponByName ("weapon_knife");

         DeleteSearchNodes ();
         MoveToVector (g_waypoint->GetBombPosition ());

         RadioMessage (Radio_Affirmative);
      }
      break;

   case Radio_StormTheFront:
      if (((IsEntityNull (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024) && Random.Long (0, 100) > 50)
      {
         RadioMessage (Radio_Affirmative);

         // don't pause/camp anymore
         TaskId_t taskID = GetTaskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
            GetTask ()->time = GetWorldTime ();

         m_targetEntity = NULL;

         MakeVectors (m_radioEntity->v.v_angle);
         m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * Random.Long (1024, 2048);

         DeleteSearchNodes ();
         StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0, true);

         m_fearLevel -= 0.3;

         if (m_fearLevel < 0.0)
            m_fearLevel = 0.0;

         m_agressionLevel += 0.3;

         if (m_agressionLevel > 1.0)
            m_agressionLevel = 1.0;
      }
      break;

   case Radio_Fallback:
      if ((IsEntityNull (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024)
      {
         m_fearLevel += 0.5;

         if (m_fearLevel > 1.0)
            m_fearLevel = 1.0;

         m_agressionLevel -= 0.5;

         if (m_agressionLevel < 0.0)
            m_agressionLevel = 0.0;

         if (GetTaskId () == TASK_CAMP)
            GetTask ()->time += Random.Float (10.0, 15.0);
         else
         {
            // don't pause/camp anymore
            TaskId_t taskID = GetTaskId ();

            if (taskID == TASK_PAUSE)
               GetTask ()->time = GetWorldTime ();

            m_targetEntity = NULL;
            m_seeEnemyTime = GetWorldTime ();

            // if bot has no enemy
            if (m_lastEnemyOrigin == nullvec)
            {
               float nearestDistance = FLT_MAX;

               // take nearest enemy to ordering player
               for (int i = 0; i < GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team == m_team)
                     continue;

                  edict_t *enemy = g_clients[i].ent;
                  float curDist = (m_radioEntity->v.origin - enemy->v.origin).GetLengthSquared ();

                  if (curDist < nearestDistance)
                  {
                     nearestDistance = curDist;
                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            DeleteSearchNodes ();
         }
      }
      break;

   case Radio_ReportTeam:
      if (Random.Long  (0, 100) < 30)
         RadioMessage ((GetNearbyEnemiesNearPosition (pev->origin, 400) == 0 && yb_communication_type.GetInt () != 2) ? Radio_SectorClear : Radio_ReportingIn);
      break;

   case Radio_SectorClear:
      // is bomb planted and it's a ct
      if (g_bombPlanted)
      {
         int bombPoint = -1;

         // check if it's a ct command
         if (GetTeam (m_radioEntity) == TEAM_CF && m_team == TEAM_CF && IsValidBot (m_radioEntity))
         {
            if (g_timeNextBombUpdate < GetWorldTime ())
            {
               float minDistance = 4096.0f;

               // find nearest bomb waypoint to player
               IterateArray (g_waypoint->m_goalPoints, i)
               {
                  distance = (g_waypoint->GetPath (g_waypoint->m_goalPoints[i])->origin - m_radioEntity->v.origin).GetLengthSquared ();

                  if (distance < minDistance)
                  {
                     minDistance = distance;
                     bombPoint = g_waypoint->m_goalPoints[i];
                  }
               }

               // mark this waypoint as restricted point
               if (bombPoint != -1 && !g_waypoint->IsGoalVisited (bombPoint))
                  g_waypoint->SetGoalVisited (bombPoint);

               g_timeNextBombUpdate = GetWorldTime () + 0.5;
            }
            // Does this Bot want to defuse?
            if (GetTaskId () == TASK_NORMAL)
            {
               // Is he approaching this goal?
               if (GetTask ()->data == bombPoint)
               {
                  GetTask ()->data = -1;
                  RadioMessage (Radio_Affirmative);
              
               }
            }
         }
      }
      break;

   case Radio_GetInPosition:
      if ((IsEntityNull (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024)
      {
         RadioMessage (Radio_Affirmative);

         if (GetTaskId () == TASK_CAMP)
            GetTask ()->time = GetWorldTime () + Random.Float (30.0, 60.0);
         else
         {
            // don't pause anymore
            TaskId_t taskID = GetTaskId ();

            if (taskID == TASK_PAUSE)
               GetTask ()->time = GetWorldTime ();

            m_targetEntity = NULL;
            m_seeEnemyTime = GetWorldTime ();

            // if bot has no enemy
            if (m_lastEnemyOrigin == nullvec)
            {
               float nearestDistance = FLT_MAX;

               // take nearest enemy to ordering player
               for (int i = 0; i < GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team == m_team)
                     continue;

                  edict_t *enemy = g_clients[i].ent;
                  float dist = (m_radioEntity->v.origin - enemy->v.origin).GetLengthSquared ();

                  if (dist < nearestDistance)
                  {
                     nearestDistance = dist;
                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            DeleteSearchNodes ();

            int index = FindDefendWaypoint (m_radioEntity->v.origin);

            // push camp task on to stack
            StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (30.0, 60.0), true);
            // push move command
            StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + Random.Float (30.0, 60.0), true);

            if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
               m_campButtons |= IN_DUCK;
            else
               m_campButtons &= ~IN_DUCK;
         }
      }
      break;
   }
   m_radioOrder = 0; // radio command has been handled, reset
}

void Bot::TryHeadTowardRadioEntity (void)
{
   TaskId_t taskID = GetTaskId ();

   if (taskID == TASK_MOVETOPOSITION || m_headedTime + 15.0f < GetWorldTime () || !IsAlive (m_radioEntity) || m_hasC4)
      return;

   if ((IsValidBot (m_radioEntity) && Random.Long (0, 100) < 25 && m_personality == PERSONALITY_NORMAL) || !(m_radioEntity->v.flags & FL_FAKECLIENT))
   {
      if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
         GetTask ()->time = GetWorldTime ();

      m_headedTime = GetWorldTime ();
      m_position = m_radioEntity->v.origin;
      DeleteSearchNodes ();

      StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0, true);
   }
}

void Bot::SelectLeaderEachTeam (int team)
{
   if (g_mapType & MAP_AS)
   {
      if (m_isVIP && !g_leaderChoosen[TEAM_CF])
      {
         // vip bot is the leader
         m_isLeader = true;
                               
         if (Random.Long (1, 100) < 50)
         {
            RadioMessage (Radio_FollowMe);
            m_campButtons = 0;
         }
         g_leaderChoosen[TEAM_CF] = true;
      }
      else if ((team == TEAM_TF) && !g_leaderChoosen[TEAM_TF])
      {
         Bot *botLeader = g_botManager->GetHighestFragsBot(team);
         
         if (botLeader != NULL && IsAlive (botLeader->GetEntity()))
         {
            botLeader->m_isLeader = true;

            if (Random.Long (1, 100) < 45)
               botLeader->RadioMessage (Radio_FollowMe);
         }
         g_leaderChoosen[TEAM_TF] = true;
      }
   }
   else if (g_mapType & MAP_DE)
   {
      if (team == TEAM_TF && !g_leaderChoosen[TEAM_TF])
      {
         if (m_hasC4)
         {
            // bot carrying the bomb is the leader
            m_isLeader = true;

            // terrorist carrying a bomb needs to have some company
            if (Random.Long (1, 100) < 80)
            {
               if (yb_communication_type.GetInt () == 2)
                  ChatterMessage (Chatter_GoingToPlantBomb);
               else
                  ChatterMessage (Radio_FollowMe);

               m_campButtons = 0;
            }
            g_leaderChoosen[TEAM_TF] = true;
         }
      }
      else if (!g_leaderChoosen[TEAM_CF])
      {
         Bot *botLeader = g_botManager->GetHighestFragsBot(team);

         if (botLeader != NULL)               
         {
            botLeader->m_isLeader = true;

            if (Random.Long (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }     
         g_leaderChoosen[TEAM_CF] = true;
      }
   }
   else if (g_mapType & (MAP_ES | MAP_KA | MAP_FY))
   {
      Bot *botLeader = g_botManager->GetHighestFragsBot (team);

      if (botLeader != NULL)
      {
         botLeader->m_isLeader = true;

         if (Random.Long (1, 100) < 30)
            botLeader->RadioMessage (Radio_FollowMe);
      }
   }
   else  
   {
      Bot *botLeader = g_botManager->GetHighestFragsBot(team);

      if (botLeader != NULL)
      {
         botLeader->m_isLeader = true;

         if (Random.Long (1, 100) < (team == TEAM_TF ? 30 : 40))
            botLeader->RadioMessage (Radio_FollowMe);
      }
   }
}

void Bot::ChooseAimDirection (void)
{
   TraceResult tr;
   memset (&tr, 0, sizeof (TraceResult));

   unsigned int flags = m_aimFlags;

   if (!(m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints))
      GetValidWaypoint ();

   // check if last enemy vector valid
   if (m_lastEnemyOrigin != nullvec)
   {
      TraceLine (EyePosition (), m_lastEnemyOrigin, false, true, GetEntity (), &tr);

      if ((pev->origin - m_lastEnemyOrigin).GetLength () >= 1600 && IsEntityNull (m_enemy) && !UsesSniper () || (tr.flFraction <= 0.2 && tr.pHit == g_hostEntity) && m_seeEnemyTime + 7.0 < GetWorldTime ())
      {
         if ((m_aimFlags & (AIM_LAST_ENEMY | AIM_PREDICT_PATH)) && m_wantsToFire)
            m_wantsToFire = false;

         m_lastEnemyOrigin = nullvec;
         m_aimFlags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);

         flags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
      }
   }
   else
   {
      m_aimFlags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
      flags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
   }

   // don't allow bot to look at danger positions under certain circumstances
   if (!(flags & (AIM_GRENADE | AIM_ENEMY | AIM_ENTITY)))
   {
      if (IsOnLadder () || IsInWater () || (m_waypointFlags & FLAG_LADDER) || (m_currentTravelFlags & PATHFLAG_JUMP))
      {
         flags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
         m_canChooseAimDirection = false;
      }
   }

   if (flags & AIM_OVERRIDE)
      m_lookAt = m_camp;
   else if (flags & AIM_GRENADE)
      m_lookAt = m_throw + Vector (0, 0, 1.0 * m_grenade.z);
   else if (flags & AIM_ENEMY)
      FocusEnemy ();
   else if (flags & AIM_ENTITY)
      m_lookAt = m_entity;
   else if (flags & AIM_LAST_ENEMY)
   {
      m_lookAt = m_lastEnemyOrigin;

      // did bot just see enemy and is quite aggressive?
      if (m_seeEnemyTime + 3.0 - m_actualReactionTime + m_baseAgressionLevel > GetWorldTime ())
      {
         // feel free to fire if shootable
         if (!UsesSniper () && LastEnemyShootable ())
            m_wantsToFire = true;
      }
      else // forget an enemy far away
      {
         m_aimFlags &= ~AIM_LAST_ENEMY;

         if ((pev->origin - m_lastEnemyOrigin).GetLength () >= 1600.0)
            m_lastEnemyOrigin = nullvec;
      }
   }
   else if (flags & AIM_PREDICT_PATH)
   {
      if (((pev->origin - m_lastEnemyOrigin).GetLength () < 1600 || UsesSniper ()) && (tr.flFraction >= 0.2 || tr.pHit != g_worldEdict))
      {
         bool recalcPath = true;

         if (!IsEntityNull (m_lastEnemy) && m_trackingEdict == m_lastEnemy && m_timeNextTracking < GetWorldTime ())
            recalcPath = false;

         if (recalcPath)
         {
            m_lookAt = g_waypoint->GetPath (GetAimingWaypoint (m_lastEnemyOrigin))->origin;
            m_camp = m_lookAt;

            m_timeNextTracking = GetWorldTime () + 0.5;
            m_trackingEdict = m_lastEnemy;

            // feel free to fire if shoot able
            if (LastEnemyShootable ())
               m_wantsToFire = true;
         }
         else
            m_lookAt = m_camp;
      }
      else // forget an enemy far away
      {
         m_aimFlags &= ~AIM_PREDICT_PATH;

         if ((pev->origin - m_lastEnemyOrigin).GetLength () >= 1600.0)
            m_lastEnemyOrigin = nullvec;
      }
   }
   else if (flags & AIM_CAMP)
      m_lookAt = m_camp;
   else if (flags & AIM_NAVPOINT)
   {
      m_lookAt = m_destOrigin;

      if (m_canChooseAimDirection && m_currentWaypointIndex != -1 && !(m_currentPath->flags & FLAG_LADDER))
      {
         int index = m_currentWaypointIndex;

         if (m_team == TEAM_TF)
         {
            if ((g_experienceData + (index * g_numWaypoints) + index)->team0DangerIndex != -1)
            {
               Vector dest = g_waypoint->GetPath ((g_experienceData + (index * g_numWaypoints) + index)->team0DangerIndex)->origin;
               TraceLine (pev->origin, dest, true, GetEntity (), &tr);

               if (tr.flFraction > 0.8 || tr.pHit != g_worldEdict)
                  m_lookAt = dest + pev->view_ofs;
            }
         }
         else
         {
            if ((g_experienceData + (index * g_numWaypoints) + index)->team1DangerIndex != -1)
            {
               Vector dest = g_waypoint->GetPath ((g_experienceData + (index * g_numWaypoints) + index)->team1DangerIndex)->origin;
               TraceLine (pev->origin, dest, true, GetEntity (), &tr);

               if (tr.flFraction > 0.8 || tr.pHit != g_worldEdict)
                  m_lookAt = dest + pev->view_ofs;
            }
         }
      }

      if (m_canChooseAimDirection && m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
      {
         Path *path = g_waypoint->GetPath (m_prevWptIndex[0]);

         if (!(path->flags & FLAG_LADDER) && (fabsf (path->origin.z - m_destOrigin.z) < 30.0 || (m_waypointFlags & FLAG_CAMP)))
         {
            // trace forward
            TraceLine (m_destOrigin, m_destOrigin + ((m_destOrigin - path->origin).Normalize () * 96), true, GetEntity (), &tr);

            if (tr.flFraction < 1.0 && tr.pHit == g_worldEdict)
               m_lookAt = path->origin + pev->view_ofs;
         }
      }
   }
   if (m_lookAt == nullvec)
      m_lookAt = m_destOrigin;
}

void Bot::Think (void)
{
   pev->button = 0;
   pev->flags |= FL_FAKECLIENT; // restore fake client bit, if it were removed by some evil action =)

   m_moveSpeed = 0.0;
   m_strafeSpeed = 0.0;
   m_moveAngles = nullvec;

   m_canChooseAimDirection = true;
   m_notKilled = IsAlive (GetEntity ());
   m_team = GetTeam (GetEntity ());

   if (m_team == TEAM_TF)
      m_hasC4 = !!(pev->weapons & (1 << WEAPON_C4));

   // is bot movement enabled
   bool botMovement = false;

   if (m_notStarted) // if the bot hasn't selected stuff to start the game yet, go do that...
      StartGame (); // select team & class
   else if (!m_notKilled)
   {
      // no movement allowed in
      if (m_voteKickIndex != m_lastVoteKick && yb_tkpunish.GetBool ()) // We got a Teamkiller? Vote him away...
      {
         FakeClientCommand (GetEntity (), "vote %d", m_voteKickIndex);
         m_lastVoteKick = m_voteKickIndex;

         // if bot tk punishment is enabled slay the tk
         if (yb_tkpunish.GetInt () != 2 || IsValidBot (EntityOfIndex (m_voteKickIndex)))
            return;

         entvars_t *killer = VARS (EntityOfIndex (m_lastVoteKick));

         MESSAGE_BEGIN (MSG_PAS, SVC_TEMPENTITY, killer->origin);
            WRITE_BYTE (TE_TAREXPLOSION);
            WRITE_COORD (killer->origin.x);
            WRITE_COORD (killer->origin.y);
            WRITE_COORD (killer->origin.z);
         MESSAGE_END ();

         MESSAGE_BEGIN (MSG_PVS, SVC_TEMPENTITY, killer->origin);
            WRITE_BYTE (TE_LAVASPLASH);
            WRITE_COORD (killer->origin.x);
            WRITE_COORD (killer->origin.y);
            WRITE_COORD (killer->origin.z);
         MESSAGE_END ();

         MESSAGE_BEGIN (MSG_ONE, g_netMsg->GetId (NETMSG_SCREENFADE), NULL, ENT (killer));
            WRITE_SHORT (1 << 15);
            WRITE_SHORT (1 << 10);
            WRITE_SHORT (1 << 1);
            WRITE_BYTE (100);
            WRITE_BYTE (0);
            WRITE_BYTE (0);
            WRITE_BYTE (255);
         MESSAGE_END ();

         killer->frags++;
         MDLL_ClientKill (ENT (killer));
      }
      else if (m_voteMap != 0) // host wants the bots to vote for a map?
      {
         FakeClientCommand (GetEntity (), "votemap %d", m_voteMap);
         m_voteMap = 0;
      }
      extern ConVar yb_chat;

      if (yb_chat.GetBool () && !RepliesToPlayer () && m_lastChatTime + 10.0 < GetWorldTime () && g_lastChatTime + 5.0 < GetWorldTime ()) // bot chatting turned on?
      {
         // say a text every now and then
         if (Random.Long (1, 1500) < 2)
         {
            m_lastChatTime = GetWorldTime ();
            g_lastChatTime = GetWorldTime ();

            char *pickedPhrase = const_cast <char *> (g_chatFactory[CHAT_DEAD].GetRandomElement ().GetBuffer ());
            bool sayBufferExists = false;

            // search for last messages, sayed
            IterateArray (m_sayTextBuffer.lastUsedSentences, i)
            {
               if (strncmp (m_sayTextBuffer.lastUsedSentences[i].GetBuffer (), pickedPhrase, m_sayTextBuffer.lastUsedSentences[i].GetLength ()) == 0)
                  sayBufferExists = true;
            }

            if (!sayBufferExists)
            {
               PrepareChatMessage (pickedPhrase);
               PushMessageQueue (GSM_SAY);

               // add to ignore list
               m_sayTextBuffer.lastUsedSentences.Push (pickedPhrase);
            }

            // clear the used line buffer every now and then
            if (m_sayTextBuffer.lastUsedSentences.GetElementNumber () > Random.Long (4, 6))
               m_sayTextBuffer.lastUsedSentences.RemoveAll ();
         }
      }
   }
   else if (m_buyingFinished)
      botMovement = true;

  int team = g_clients[IndexOfEntity (GetEntity ()) - 1].realTeam;;

   // remove voice icon
   if (g_lastRadioTime[team] + Random.Float (0.8, 2.1) < GetWorldTime ())
      SwitchChatterIcon (false); // hide icon

   // check is it time to execute think (called once per second (not frame))
   if (m_timePeriodicUpdate < GetWorldTime ())
   {
      SecondThink ();
     
      // update timer to one second
      m_timePeriodicUpdate = GetWorldTime () + 0.99f;
   }
   CheckMessageQueue (); // check for pending messages

   if (pev->maxspeed < 10 && GetTaskId () != TASK_PLANTBOMB && GetTaskId () != TASK_DEFUSEBOMB)
      botMovement = false;

   CheckSpawnTimeConditions ();

   if (m_notKilled && botMovement && !yb_freeze_bots.GetBool ())
      BotAI (); // execute main code

   RunPlayerMovement (); // run the player movement
}

void Bot::SecondThink (void)
{
   // this function is called from main think function every second (second not frame).

   if (g_bombPlanted && m_team == TEAM_CF && (pev->origin - g_waypoint->GetBombPosition ()).GetLength () < 700 && !IsBombDefusing (g_waypoint->GetBombPosition ()) && !m_hasProgressBar && GetTaskId () != TASK_ESCAPEFROMBOMB)
      ResetTasks ();
}

void Bot::RunTask (void)
{
   // this is core function that handle task execution
   int destIndex, i;

   Vector src, destination;
   TraceResult tr;

   bool exceptionCaught = false;
   float fullDefuseTime, timeToBlowUp, defuseRemainingTime;

   switch (GetTaskId ())
   {
   // normal task
   case TASK_NORMAL:
      m_aimFlags |= AIM_NAVPOINT;

      // user forced a waypoint as a goal?
      if (yb_debug_goal.GetInt () != -1 && GetTask ()->data != yb_debug_goal.GetInt ())
      {
         DeleteSearchNodes ();
         GetTask ()->data = yb_debug_goal.GetInt ();
      }

      // bots rushing with knife, when have no enemy (thanks for idea to nicebot project)
      if (m_currentWeapon == WEAPON_KNIFE && (IsEntityNull (m_lastEnemy) || !IsAlive (m_lastEnemy)) && IsEntityNull (m_enemy) && m_knifeAttackTime < GetWorldTime () && !HasShield () && GetNearbyFriendsNearPosition (pev->origin, 96) == 0)
      {
         if (Random.Long (0, 100) < 40)
            pev->button |= IN_ATTACK;
         else
            pev->button |= IN_ATTACK2;

         m_knifeAttackTime = GetWorldTime () + Random.Float (2.5, 6.0);
      }    

      if (m_reloadState == RELOAD_NONE && GetAmmo () != 0 && GetAmmoInClip () < 5 && g_weaponDefs[m_currentWeapon].ammo1 != -1)
         m_reloadState = RELOAD_PRIMARY;

      // if bomb planted and it's a CT calculate new path to bomb point if he's not already heading for
      if (g_bombPlanted && m_team == TEAM_CF && GetTask ()->data != -1 && !(g_waypoint->GetPath (GetTask ()->data)->flags & FLAG_GOAL) && GetTaskId () != TASK_ESCAPEFROMBOMB)
      {
         DeleteSearchNodes ();
         GetTask ()->data = -1;
      }

      if (!g_bombPlanted && m_currentWaypointIndex != -1 && (m_currentPath->flags & FLAG_GOAL) && Random.Long (0, 100) < 80 && GetNearbyEnemiesNearPosition (pev->origin, 650) == 0)
         RadioMessage (Radio_SectorClear);

      // reached the destination (goal) waypoint?
      if (DoWaypointNav ())
      {
         TaskComplete ();
         m_prevGoalIndex = -1;

         // spray logo sometimes if allowed to do so
         if (m_timeLogoSpray < GetWorldTime () && yb_spraypaints.GetBool () && Random.Long (1, 100) < 80 && m_moveSpeed > GetWalkSpeed ())
            StartTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, -1, GetWorldTime () + 1.0, false);

         // reached waypoint is a camp waypoint
         if ((m_currentPath->flags & FLAG_CAMP) && !yb_csdm_mode.GetBool ())
         {
            // check if bot has got a primary weapon and hasn't camped before
            if (HasPrimaryWeapon () && m_timeCamping + 10.0 < GetWorldTime () && !HasHostage ())
            {
               bool campingAllowed = true;

               // Check if it's not allowed for this team to camp here
               if (m_team == TEAM_TF)
               {
                  if (m_currentPath->flags & FLAG_CF_ONLY)
                     campingAllowed = false;
               }
               else
               {
                  if (m_currentPath->flags & FLAG_TF_ONLY)
                     campingAllowed = false;
               }

               // don't allow vip on as_ maps to camp + don't allow terrorist carrying c4 to camp
               if (((g_mapType & MAP_AS) && *(INFOKEY_VALUE (GET_INFOKEYBUFFER (GetEntity ()), "model")) == 'v') || ((g_mapType & MAP_DE) && m_team == TEAM_TF && !g_bombPlanted && m_hasC4))
                  campingAllowed = false;

               // check if another bot is already camping here
               if (IsPointOccupied (m_currentWaypointIndex))
                  campingAllowed = false;

               if (campingAllowed)
               {
                  // crouched camping here?
                  if (m_currentPath->flags & FLAG_CROUCH)
                     m_campButtons = IN_DUCK;
                  else
                     m_campButtons = 0;

                  SelectBestWeapon ();

                  if (!(m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) && !m_reloadState)
                     m_reloadState = RELOAD_PRIMARY;

                  MakeVectors (pev->v_angle);

                  StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (20.0f, 40.0f), true);

                  m_camp = Vector (m_currentPath->campStartX, m_currentPath->campStartY, 0.0f);
                  m_aimFlags |= AIM_CAMP;
                  m_campDirection = 0;

                  // tell the world we're camping
                  if (Random.Long (0, 100) < 80)
                     RadioMessage (Radio_InPosition);

                  m_moveToGoal = false;
                  m_checkTerrain = false;

                  m_moveSpeed = 0;
                  m_strafeSpeed = 0;
               }
            }
         }
         else
         {
            // some goal waypoints are map dependant so check it out...
            if (g_mapType & MAP_CS)
            {
               // CT Bot has some hostages following?
               if (HasHostage () && m_team == TEAM_CF)
               {
                  // and reached a Rescue Point?
                  if (m_currentPath->flags & FLAG_RESCUE)
                  {
                     for (i = 0; i < MAX_HOSTAGES; i++)
                        m_hostages[i] = NULL; // clear array of hostage pointers
                  }
               }
               else if (m_team == TEAM_TF && Random.Long (0, 100) < 80)
               {
                  int index = FindDefendWaypoint (m_currentPath->origin);

                  StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (60.0, 120.0), true); // push camp task on to stack
                  StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + Random.Float (5.0, 10.0), true); // push move command

                  if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
                     m_campButtons |= IN_DUCK;
                  else
                     m_campButtons &= ~IN_DUCK;

                  ChatterMessage (Chatter_GoingToGuardVIPSafety); // play info about that
               }
            }
            else if ((g_mapType & MAP_DE) && ((m_currentPath->flags & FLAG_GOAL) || m_inBombZone) && m_seeEnemyTime + 1.5f < GetWorldTime ())
            {
               // is it a terrorist carrying the bomb?
               if (m_hasC4)
               {
                  if ((m_states & STATE_SEEING_ENEMY) && GetNearbyFriendsNearPosition (pev->origin, 768) == 0)
                  {
                     // request an help also
                     RadioMessage (Radio_NeedBackup);
                     InstantChatterMessage (Chatter_ScaredEmotion);

                     StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (4.0, 8.0), true);
                  }
                  else
                     StartTask (TASK_PLANTBOMB, TASKPRI_PLANTBOMB, -1, 0.0, false);
               }
               else if (m_team == TEAM_CF)
               {
                  if (!g_bombPlanted && GetNearbyFriendsNearPosition (pev->origin, 360.0f) < 3 && Random.Long (0, 100) < 85 && m_personality != PERSONALITY_RUSHER)
                  {
                     int index = FindDefendWaypoint (m_currentPath->origin);

                     StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (25.0, 40.0), true); // push camp task on to stack
                     StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + Random.Float (5.0f, 11.0f), true); // push move command

                     if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
                        m_campButtons |= IN_DUCK;
                     else
                        m_campButtons &= ~IN_DUCK;

                     ChatterMessage (Chatter_DefendingBombSite); // play info about that
                  }
               }
            }
         }
      }
      // no more nodes to follow - search new ones (or we have a momb)
      else if (!GoalIsValid ())
      {
         m_moveSpeed = pev->maxspeed;
         DeleteSearchNodes ();

         // did we already decide about a goal before?
         if (GetTask ()->data != -1)
            destIndex = GetTask ()->data;
         else
            destIndex = FindGoal ();

         m_prevGoalIndex = destIndex;
         m_chosenGoalIndex = destIndex;

         // remember index
         GetTask ()->data = destIndex;

         // do pathfinding if it's not the current waypoint
         if (destIndex != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, destIndex, ((g_bombPlanted && m_team == TEAM_CF) || yb_debug_goal.GetInt () != -1) ? 0 : m_pathType);
      }
      else
      {
         if (!(pev->flags & FL_DUCKING) && m_minSpeed != pev->maxspeed)
            m_moveSpeed = m_minSpeed;
      }

      if ((yb_walking_allowed.GetBool () && mp_footsteps.GetBool ()) && m_difficulty >= 2 && !(m_aimFlags & AIM_ENEMY) && (m_heardSoundTime + 13.0 >= GetWorldTime () || (m_states & (STATE_HEARING_ENEMY | STATE_SUSPECT_ENEMY))) && GetNearbyEnemiesNearPosition (pev->origin, 1024) >= 1 && !(m_currentTravelFlags & PATHFLAG_JUMP) && !(pev->button & IN_DUCK) && !(pev->flags & FL_DUCKING) && !yb_jasonmode.GetBool () && !g_bombPlanted)
         m_moveSpeed = GetWalkSpeed ();

      // bot hasn't seen anything in a long time and is asking his teammates to report in
      if (m_seeEnemyTime != 0.0 && m_seeEnemyTime + Random.Float (30.0, 80.0) < GetWorldTime () && Random.Long (0, 100) < 70 && g_timeRoundStart + 20.0 < GetWorldTime () && m_askCheckTime + Random.Float (20.0, 30.0) < GetWorldTime ())
      {
         m_askCheckTime = GetWorldTime ();
         RadioMessage (Radio_ReportTeam);
      }

      break;

   // bot sprays messy logos all over the place...
   case TASK_SPRAY:
      m_aimFlags |= AIM_ENTITY;

      // bot didn't spray this round?
      if (m_timeLogoSpray <= GetWorldTime () && GetTask ()->time > GetWorldTime ())
      {
         MakeVectors (pev->v_angle);
         Vector sprayOrigin = EyePosition () + (g_pGlobals->v_forward * 128);

         TraceLine (EyePosition (), sprayOrigin, true, GetEntity (), &tr);

         // no wall in front?
         if (tr.flFraction >= 1.0)
            sprayOrigin.z -= 128.0;

         m_entity = sprayOrigin;

         if (GetTask ()->time - 0.5 < GetWorldTime ())
         {
            // emit spraycan sound
            EMIT_SOUND_DYN2 (GetEntity (), CHAN_VOICE, "player/sprayer.wav", 1.0, ATTN_NORM, 0, 100);
            TraceLine (EyePosition (), EyePosition () + g_pGlobals->v_forward * 128, true, GetEntity (), &tr);

            // paint the actual logo decal
            DecalTrace (pev, &tr, m_logotypeIndex);
            m_timeLogoSpray = GetWorldTime () + Random.Float (30.0, 45.0);
         }
      }
      else
         TaskComplete ();

      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = GetWorldTime ();
      m_moveSpeed = 0;
      m_strafeSpeed = 0.0;

      break;

   // hunt down enemy
   case TASK_HUNTENEMY:
      m_aimFlags |= AIM_NAVPOINT;
      m_checkTerrain = true;

      // if we've got new enemy...
      if (!IsEntityNull (m_enemy) || IsEntityNull (m_lastEnemy))
      {
         // forget about it...
         TaskComplete ();
         m_prevGoalIndex = -1;

         m_lastEnemy = NULL;
         m_lastEnemyOrigin = nullvec;
      }
      else if (GetTeam (m_lastEnemy) == m_team)
      {
         // don't hunt down our teammate...
         RemoveCertainTask (TASK_HUNTENEMY);
         m_prevGoalIndex = -1;
      }
      else if (DoWaypointNav ()) // reached last enemy pos?
      {
         // forget about it...
         TaskComplete ();
         m_prevGoalIndex = -1;

         m_lastEnemy = NULL;
         m_lastEnemyOrigin = nullvec;
      }
      else if (!GoalIsValid ()) // do we need to calculate a new path?
      {
         DeleteSearchNodes ();

         // is there a remembered index?
         if (GetTask ()->data != -1 && GetTask ()->data < g_numWaypoints)
            destIndex = GetTask ()->data;
         else // no. we need to find a new one
            destIndex = g_waypoint->FindNearest (m_lastEnemyOrigin);

         // remember index
         m_prevGoalIndex = destIndex;
         GetTask ()->data = destIndex;

         if (destIndex != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, destIndex, m_pathType);
      }

      // bots skill higher than 60?
      if (yb_walking_allowed.GetBool () && mp_footsteps.GetBool () && m_difficulty >= 1 && !yb_jasonmode.GetBool ())
      {
         // then make him move slow if near enemy
         if (!(m_currentTravelFlags & PATHFLAG_JUMP))
         {
            if (m_currentWaypointIndex != -1)
            {
               if (m_currentPath->radius < 32 && !IsOnLadder () && !IsInWater () && m_seeEnemyTime + 4.0 > GetWorldTime () && m_difficulty == 0)
                  pev->button |= IN_DUCK;
            }

            if ((m_lastEnemyOrigin - pev->origin).GetLength () < 512.0 && !(pev->flags & FL_DUCKING))
               m_moveSpeed = GetWalkSpeed ();
         }
      }
      break;

   // bot seeks cover from enemy
   case TASK_SEEKCOVER:
      m_aimFlags |= AIM_NAVPOINT;

      if (IsEntityNull (m_lastEnemy) || !IsAlive (m_lastEnemy))
      {
         TaskComplete ();
         m_prevGoalIndex = -1;
      }
      else if (DoWaypointNav ()) // reached final cover waypoint?
      {
         // yep. activate hide behaviour
         TaskComplete ();

         m_prevGoalIndex = -1;
         m_pathType = 0;

         // start hide task
         StartTask (TASK_HIDE, TASKPRI_HIDE, -1, GetWorldTime () + Random.Float (5.0, 15.0), false);
         destination = m_lastEnemyOrigin;

         // get a valid look direction
         GetCampDirection (&destination);

         m_aimFlags |= AIM_CAMP;
         m_camp = destination;
         m_campDirection = 0;

         // chosen waypoint is a camp waypoint?
         if (m_currentPath->flags & FLAG_CAMP)
         {
            // use the existing camp wpt prefs
            if (m_currentPath->flags & FLAG_CROUCH)
               m_campButtons = IN_DUCK;
            else
               m_campButtons = 0;
         }
         else
         {
            // choose a crouch or stand pos
            if (m_currentPath->vis.crouch <= m_currentPath->vis.stand)
               m_campButtons = IN_DUCK;
            else
               m_campButtons = 0;

            // enter look direction from previously calculated positions
            m_currentPath->campStartX = destination.x;
            m_currentPath->campStartY = destination.y;

            m_currentPath->campStartX = destination.x;
            m_currentPath->campEndY = destination.y;
         }

         if ((m_reloadState == RELOAD_NONE) && (GetAmmoInClip () < 8) && (GetAmmo () != 0))
            m_reloadState = RELOAD_PRIMARY;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         m_moveToGoal = false;
         m_checkTerrain = true;
      }
      else if (!GoalIsValid ()) // we didn't choose a cover waypoint yet or lost it due to an attack?
      {
         DeleteSearchNodes ();

         if (GetTask ()->data != -1)
            destIndex = GetTask ()->data;
         else
         {
            destIndex = FindCoverWaypoint (1024);

            if (destIndex == -1)
               destIndex = g_waypoint->FindNearest (pev->origin, 500);
         }

         m_campDirection = 0;
         m_prevGoalIndex = destIndex;
         GetTask ()->data = destIndex;

         if (destIndex != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, destIndex, 0);
      }
      break;

   // plain attacking
   case TASK_ATTACK:
      m_moveToGoal = false;
      m_checkTerrain = false;

      if (!IsEntityNull (m_enemy))
      {
         if (IsOnLadder ())
         {
            pev->button |= IN_DUCK;
            DeleteSearchNodes ();
         }
         CombatFight ();
      }
      else
      {
         TaskComplete ();
         FindWaypoint ();

         m_destOrigin = m_lastEnemyOrigin;
      }
      m_navTimeset = GetWorldTime ();
      break;

   // Bot is pausing
   case TASK_PAUSE:
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = GetWorldTime ();
      m_moveSpeed = 0.0;
      m_strafeSpeed = 0.0;

      m_aimFlags |= AIM_NAVPOINT;

      // is bot blinded and above average difficulty?
      if (m_viewDistance < 500.0 && m_difficulty >= 2)
      {
         // go mad!
         m_moveSpeed = -fabsf ((m_viewDistance - 500.0) / 2.0);

         if (m_moveSpeed < -pev->maxspeed)
            m_moveSpeed = -pev->maxspeed;

         MakeVectors (pev->v_angle);
         m_camp = EyePosition () + (g_pGlobals->v_forward * 500);

         m_aimFlags |= AIM_OVERRIDE;
         m_wantsToFire = true;
      }
      else
         pev->button |= m_campButtons;

      // stop camping if time over or gets hurt by something else than bullets
      if (GetTask ()->time < GetWorldTime () || m_lastDamageType > 0)
         TaskComplete ();
      break;

   // blinded (flashbanged) behaviour
   case TASK_BLINDED:
      m_moveToGoal = false;
      m_checkTerrain = false;
      m_navTimeset = GetWorldTime ();

      // if bot remembers last enemy position
      if (m_difficulty >= 2 && m_lastEnemyOrigin != nullvec && IsValidPlayer (m_lastEnemy) && !UsesSniper ())
      {
         m_lookAt = m_lastEnemyOrigin; // face last enemy
         m_wantsToFire = true; // and shoot it
      }

      m_moveSpeed = m_blindMoveSpeed;
      m_strafeSpeed = m_blindSidemoveSpeed;
      pev->button |= m_blindButton;

      if (m_blindTime < GetWorldTime ())
         TaskComplete ();

      break;

   // camping behaviour
   case TASK_CAMP:
      m_aimFlags |= AIM_CAMP;
      m_checkTerrain = false;
      m_moveToGoal = false;

      if (g_bombPlanted && m_defendedBomb && !IsBombDefusing (g_waypoint->GetBombPosition ()) && !OutOfBombTimer () && m_team == TEAM_CF)
      {
         m_defendedBomb = false;
         TaskComplete();
      }

      // half the reaction time if camping because you're more aware of enemies if camping
      SetIdealReactionTimes ();
      m_idealReactionTime /= 2;

      m_navTimeset = GetWorldTime ();
      m_timeCamping = GetWorldTime();

      m_moveSpeed = 0;
      m_strafeSpeed = 0.0;

      GetValidWaypoint ();

      if (m_nextCampDirTime < GetWorldTime ())
      {
         m_nextCampDirTime = GetWorldTime () + Random.Float (2.0, 5.0);

         if (m_currentPath->flags & FLAG_CAMP)
         {
            destination.z = 0;

            // switch from 1 direction to the other
            if (m_campDirection < 1)
            {
               destination.x = m_currentPath->campStartX;
               destination.y = m_currentPath->campStartY;
               m_campDirection ^= 1;
            }
            else
            {
               destination.x = m_currentPath->campEndX;
               destination.y = m_currentPath->campEndY;
               m_campDirection ^= 1;
            }

            // find a visible waypoint to this direction...
            // i know this is ugly hack, but i just don't want to break compatiability :)
            int numFoundPoints = 0;
            int foundPoints[3];
            int distanceTab[3];

            Vector dotA = (destination - pev->origin).Normalize2D ();

            for (i = 0; i < g_numWaypoints; i++)
            {
               // skip invisible waypoints or current waypoint
               if (!g_waypoint->IsVisible (m_currentWaypointIndex, i) || (i == m_currentWaypointIndex))
                  continue;

               Vector dotB = (g_waypoint->GetPath (i)->origin - pev->origin).Normalize2D ();

               if ((dotA | dotB) > 0.9)
               {
                  int distance = static_cast <int> ((pev->origin - g_waypoint->GetPath (i)->origin).GetLength ());

                  if (numFoundPoints >= 3)
                  {
                     for (int j = 0; j < 3; j++)
                     {
                        if (distance > distanceTab[j])
                        {
                           distanceTab[j] = distance;
                           foundPoints[j] = i;

                           break;
                        }
                     }
                  }
                  else
                  {
                     foundPoints[numFoundPoints] = i;
                     distanceTab[numFoundPoints] = distance;

                     numFoundPoints++;
                  }
               }
            }

            if (--numFoundPoints >= 0)
               m_camp = g_waypoint->GetPath (foundPoints[Random.Long (0, numFoundPoints)])->origin;
            else
               m_camp = g_waypoint->GetPath (GetAimingWaypoint ())->origin;
         }
         else
            m_camp = g_waypoint->GetPath (GetAimingWaypoint ())->origin;
      }
      // press remembered crouch button
      pev->button |= m_campButtons;

      // stop camping if time over or gets hurt by something else than bullets
      if ((GetTask ()->time < GetWorldTime ()) || (m_lastDamageType > 0))
         TaskComplete ();
      break;

   // hiding behaviour
   case TASK_HIDE:
      m_aimFlags |= AIM_CAMP;
      m_checkTerrain = false;
      m_moveToGoal = false;

      // half the reaction time if camping
      SetIdealReactionTimes ();
      m_idealReactionTime /= 2;

      m_navTimeset = GetWorldTime ();
      m_moveSpeed = 0;
      m_strafeSpeed = 0.0;

      GetValidWaypoint ();

      if (HasShield () && !m_isReloading)
      {
         if (!IsShieldDrawn ())
            pev->button |= IN_ATTACK2; // draw the shield!
         else
            pev->button |= IN_DUCK; // duck under if the shield is already drawn
      }

      // if we see an enemy and aren't at a good camping point leave the spot
      if ((m_states & STATE_SEEING_ENEMY) || m_inBombZone)
      {
         if (!(m_currentPath->flags & FLAG_CAMP))
         {
            TaskComplete ();

            m_campButtons = 0;
            m_prevGoalIndex = -1;

            if (!IsEntityNull (m_enemy))
               CombatFight ();

            break;
         }
      }
      else if (m_lastEnemyOrigin == nullvec) // If we don't have an enemy we're also free to leave
      {
         TaskComplete ();

         m_campButtons = 0;
         m_prevGoalIndex = -1;

         if (GetTaskId () == TASK_HIDE)
            TaskComplete ();

         break;
      }

      pev->button |= m_campButtons;
      m_navTimeset = GetWorldTime ();

      // stop camping if time over or gets hurt by something else than bullets
      if (GetTask ()->time < GetWorldTime () || m_lastDamageType > 0)
         TaskComplete ();

      break;

   // moves to a position specified in position has a higher priority than task_normal
   case TASK_MOVETOPOSITION:
      m_aimFlags |= AIM_NAVPOINT;

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (DoWaypointNav ()) // reached destination?
      {
         TaskComplete (); // we're done

         m_prevGoalIndex = -1;
         m_position = nullvec;
      }
      else if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         if (GetTask ()->data != -1 && GetTask ()->data < g_numWaypoints)
            destIndex = GetTask ()->data;
         else
            destIndex = g_waypoint->FindNearest (m_position);

         if (destIndex >= 0 && destIndex < g_numWaypoints)
         {
            m_prevGoalIndex = destIndex;
            GetTask ()->data = destIndex;

            FindPath (m_currentWaypointIndex, destIndex, m_pathType);
         }
         else
            TaskComplete ();
      }
      break;

   // planting the bomb right now
   case TASK_PLANTBOMB:
      m_aimFlags |= AIM_CAMP;

      destination = m_lastEnemyOrigin;
      GetCampDirection (&destination);

      if (m_hasC4) // we're still got the C4?
      {
         SelectWeaponByName ("weapon_c4");

         if (IsAlive (m_enemy) || !m_inBombZone)
            TaskComplete ();
         else
         {
            m_moveToGoal = false;
            m_checkTerrain = false;
            m_navTimeset = GetWorldTime ();

            if (m_currentPath->flags & FLAG_CROUCH)
               pev->button |= (IN_ATTACK | IN_DUCK);
            else
               pev->button |= IN_ATTACK;

            m_moveSpeed = 0;
            m_strafeSpeed = 0;
         }
      }
      else // done with planting
      {
         TaskComplete ();

         // tell teammates to move over here...
         if (GetNearbyFriendsNearPosition (pev->origin, 1200) != 0)
            RadioMessage (Radio_NeedBackup);

         DeleteSearchNodes ();
         int index = FindDefendWaypoint (pev->origin);

         float bombTimer = mp_c4timer.GetFloat ();

         // push camp task on to stack
         StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + ((bombTimer / 2) + (bombTimer / 4)), true);
         // push move command
         StartTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, GetWorldTime () + ((bombTimer / 2) + (bombTimer / 4)), true);

         if (g_waypoint->GetPath (index)->vis.crouch <= g_waypoint->GetPath (index)->vis.stand)
            m_campButtons |= IN_DUCK;
         else
            m_campButtons &= ~IN_DUCK;
      }
      break;

   // bomb defusing behaviour
   case TASK_DEFUSEBOMB:
      fullDefuseTime = m_hasDefuser ? 6.0 : 11.0;
      timeToBlowUp = GetBombTimeleft ();
      defuseRemainingTime = fullDefuseTime;

      if (m_hasProgressBar /*&& IsOnFloor ()*/)
         defuseRemainingTime = fullDefuseTime - GetWorldTime();

      // exception: bomb has been defused
      if (g_waypoint->GetBombPosition () == nullvec)
      {
         exceptionCaught = true;
         g_bombPlanted = false;

         if (GetNearbyFriendsNearPosition (pev->origin, 9999) != 0 && Random.Long (0, 100) < 50)
         {
            if (timeToBlowUp <= 3.0)
            {
               if (yb_communication_type.GetInt () == 2)
                  InstantChatterMessage (Chatter_BarelyDefused);
               else if (yb_communication_type. GetInt () == 1)
                  RadioMessage (Radio_SectorClear);
            }
            else
               RadioMessage (Radio_SectorClear);
         }
      }
      else if (defuseRemainingTime > timeToBlowUp) // exception: not time left for defusing
         exceptionCaught = true;
      else if (m_states & STATE_SEEING_ENEMY) // exception: saw/seeing enemy
      {
         if (GetNearbyFriendsNearPosition (pev->origin, 128) == 0)
         {
            if (defuseRemainingTime > 0.75)
            {
               if (GetNearbyFriendsNearPosition (pev->origin, 128) > 0)
                  RadioMessage (Radio_NeedBackup);

               exceptionCaught = true;
            }
         }
         else if (timeToBlowUp > fullDefuseTime + 3.0 && defuseRemainingTime > 1.0)
            exceptionCaught = true;
      }
      else if (m_states & STATE_SUSPECT_ENEMY) // exception: suspect enemy
      {
         if (GetNearbyFriendsNearPosition (pev->origin, 128) == 0)
         {
            if (timeToBlowUp > fullDefuseTime + 10.0)
            {
               if (GetNearbyFriendsNearPosition (pev->origin, 128) > 0)
                  RadioMessage (Radio_NeedBackup);

               exceptionCaught = true;
            }
         }
      }


      // one of exceptions is thrown. finish task.
      if (exceptionCaught)
      {
         m_checkTerrain = true;
         m_moveToGoal = true;

         m_destOrigin = nullvec;
         m_entity = nullvec;

         m_pickupItem = NULL;
         m_pickupType = PICKUP_NONE;

         TaskComplete ();
         break;
      }

      // to revert from pause after reload waiting && just to be sure
      m_moveToGoal = false;
      m_checkTerrain = true;

      m_moveSpeed = pev->maxspeed;
      m_strafeSpeed = 0.0;

      // bot is reloading and we close enough to start defusing
      if (m_isReloading && (g_waypoint->GetBombPosition () - pev->origin).GetLength2D () < 80.0)
      {
         if (GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0 || GetNearbyFriendsNearPosition (pev->origin, 768) > 2 || timeToBlowUp < fullDefuseTime + 7.0 || ((GetAmmoInClip () > 8 && m_reloadState == RELOAD_PRIMARY) || (GetAmmoInClip () > 5 && m_reloadState == RELOAD_SECONDARY)))
         {
            int weaponIndex = GetHighestWeapon ();

            // just select knife and then select weapon
            SelectWeaponByName ("weapon_knife");

            if (weaponIndex > 0 && weaponIndex < NUM_WEAPONS)
               SelectWeaponbyNumber (weaponIndex);

            m_isReloading = false;
         }
         else // just wait here
         {
            m_moveToGoal = false;
            m_checkTerrain = false;

            m_moveSpeed = 0.0;
            m_strafeSpeed = 0.0;
         }
      }

      // head to bomb and press use button
      m_aimFlags |= AIM_ENTITY;

      m_destOrigin = g_waypoint->GetBombPosition ();
      m_entity = g_waypoint->GetBombPosition ();

      pev->button |= IN_USE;

      // if defusing is not already started, maybe crouch before
      if (!m_hasProgressBar && m_duckDefuseCheckTime < GetWorldTime ())
      {
         if (m_difficulty >= 2 && GetNearbyEnemiesNearPosition (pev->origin, 9999.0) != 0)
            m_duckDefuse = true;

         Vector botDuckOrigin, botStandOrigin;

         if (pev->button & IN_DUCK)
         {
            botDuckOrigin = pev->origin;
            botStandOrigin = pev->origin + Vector(0, 0, 18);
         }
         else
         {
            botDuckOrigin = pev->origin - Vector(0, 0, 18);
            botStandOrigin = pev->origin;
         }

         float duckLength = (m_entity - botDuckOrigin).GetLengthSquared ();
         float standLength = (m_entity - botStandOrigin).GetLengthSquared ();

         if (duckLength > 5625 || standLength > 5625)
         {
            if (standLength < duckLength)
               m_duckDefuse = false; // stand
            else
               m_duckDefuse = true; // duck
         }
         m_duckDefuseCheckTime = GetWorldTime () + 1.5;
      }

      // press duck button
      if (m_duckDefuse || (pev->oldbuttons & IN_DUCK))
         pev->button |= IN_DUCK;
      else
         pev->button &= ~IN_DUCK;

      // we are defusing bomb
      if (m_hasProgressBar)
      {
         pev->button |= IN_USE;

         m_reloadState = RELOAD_NONE;
         m_navTimeset = GetWorldTime ();

         // don't move when defusing
         m_moveToGoal = false;
         m_checkTerrain = false;

         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         // notify team
         if (GetNearbyFriendsNearPosition (pev->origin, 9999) != 0)
         {
            ChatterMessage (Chatter_DefusingC4);

            if (GetNearbyFriendsNearPosition (pev->origin, 256) < 2)
               RadioMessage (Radio_NeedBackup);
         }
      }
      break;
      
   // follow user behaviour
   case TASK_FOLLOWUSER:
      if (IsEntityNull (m_targetEntity) || !IsAlive (m_targetEntity))
      {
         m_targetEntity = NULL;
         TaskComplete ();
         break;
      }

      if (m_targetEntity->v.button & IN_ATTACK)
      {
         MakeVectors (m_targetEntity->v.v_angle);
         TraceLine (m_targetEntity->v.origin + m_targetEntity->v.view_ofs, g_pGlobals->v_forward * 500, true, true, GetEntity (), &tr);

         if (!IsEntityNull (tr.pHit) && IsValidPlayer (tr.pHit) && GetTeam (tr.pHit) != m_team)
         {
            m_targetEntity = NULL;
            m_lastEnemy = tr.pHit;
            m_lastEnemyOrigin = tr.pHit->v.origin;

            TaskComplete ();
            break;
         }
      }

      if (m_targetEntity->v.maxspeed != 0 && m_targetEntity->v.maxspeed < pev->maxspeed)
         m_moveSpeed = m_targetEntity->v.maxspeed;

      if (m_reloadState == RELOAD_NONE && GetAmmo () != 0)
         m_reloadState = RELOAD_PRIMARY;

      if ((m_targetEntity->v.origin - pev->origin).GetLength () > 130)
         m_followWaitTime = 0.0;
      else
      {
         m_moveSpeed = 0.0;

         if (m_followWaitTime == 0.0)
            m_followWaitTime = GetWorldTime ();
         else
         {
            if (m_followWaitTime + 3.0 < GetWorldTime ())
            {
               // stop following if we have been waiting too long
               m_targetEntity = NULL;

               RadioMessage (Radio_YouTakePoint);
               TaskComplete ();

               break;
            }
         }
      }
      m_aimFlags |= AIM_NAVPOINT;

      if (yb_walking_allowed.GetBool () && m_targetEntity->v.maxspeed < m_moveSpeed && !yb_jasonmode.GetBool ())
         m_moveSpeed = GetWalkSpeed ();

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (DoWaypointNav ()) // reached destination?
         GetTask ()->data = -1;

      if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         destIndex = g_waypoint->FindNearest (m_targetEntity->v.origin);

         Array <int> points;
         g_waypoint->FindInRadius (points, 200, m_targetEntity->v.origin);

         while (!points.IsEmpty ())
         {
            int newIndex = points.Pop ();

            // if waypoint not yet used, assign it as dest
            if (!IsPointOccupied (newIndex) && (newIndex != m_currentWaypointIndex))
               destIndex = newIndex;
         }

         if (destIndex >= 0 && destIndex < g_numWaypoints && destIndex != m_currentWaypointIndex && m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints)
         {
            m_prevGoalIndex = destIndex;
            GetTask ()->data = destIndex;

            // always take the shortest path
            FindShortestPath (m_currentWaypointIndex, destIndex);
         }
         else
         {
            m_targetEntity = NULL;
            TaskComplete ();
         }
      }
      break;

   // HE grenade throw behaviour
   case TASK_THROWHEGRENADE:
      m_aimFlags |= AIM_GRENADE;
      destination = m_throw;

      if (!(m_states & STATE_SEEING_ENEMY))
      {
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         m_moveToGoal = false;
      }
      else if (!(m_states & STATE_SUSPECT_ENEMY) && !IsEntityNull (m_enemy))
         destination = m_enemy->v.origin + (m_enemy->v.velocity.SkipZ () * 0.5);

      m_isUsingGrenade = true;
      m_checkTerrain = false;

      if ((pev->origin - destination).GetLengthSquared () < 400 * 400)
      {
         // heck, I don't wanna blow up myself
         m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;

         SelectBestWeapon ();
         TaskComplete ();

         break;
      }

      m_grenade = CheckThrow (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () < 100)
         m_grenade = CheckToss (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () <= 100)
      {
         m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;
         m_grenade = m_lookAt;

         SelectBestWeapon ();
         TaskComplete ();
      }
      else
      {
         edict_t *ent = NULL;

         while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
         {
            if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0)
            {
               // set the correct velocity for the grenade
               if (m_grenade.GetLengthSquared () > 100)
                  ent->v.velocity = m_grenade;

               m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;

               SelectBestWeapon ();
               TaskComplete ();

               break;
            }
         }

         if (IsEntityNull (ent))
         {
            if (m_currentWeapon != WEAPON_EXPLOSIVE)
            {
               if (pev->weapons & (1 << WEAPON_EXPLOSIVE))
                  SelectWeaponByName ("weapon_hegrenade");
            }
            else if (!(pev->oldbuttons & IN_ATTACK))
               pev->button |= IN_ATTACK;
         }
      }
      pev->button |= m_campButtons;
      break;

   // flashbang throw behavior (basically the same code like for HE's)
   case TASK_THROWFLASHBANG:
      m_aimFlags |= AIM_GRENADE;
      destination = m_throw;

      if (!(m_states & STATE_SEEING_ENEMY))
      {
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         m_moveToGoal = false;
      }
      else if (!(m_states & STATE_SUSPECT_ENEMY) && !IsEntityNull (m_enemy))
         destination = m_enemy->v.origin + (m_enemy->v.velocity.SkipZ () * 0.5);

      m_isUsingGrenade = true;
      m_checkTerrain = false;

      m_grenade = CheckThrow (EyePosition (), destination);

      if (m_grenade.GetLengthSquared () < 100)
         m_grenade = CheckToss (pev->origin, destination);

      if (m_grenade.GetLengthSquared () <= 100)
      {
         m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;
         m_grenade = m_lookAt;

         SelectBestWeapon ();
         TaskComplete ();
      }
      else
      {
         edict_t *ent = NULL;
         while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
         {
            if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0)
            {
               // set the correct velocity for the grenade
               if (m_grenade.GetLengthSquared () > 100)
                  ent->v.velocity = m_grenade;

               m_grenadeCheckTime = GetWorldTime () + MAX_GRENADE_TIMER;

               SelectBestWeapon ();
               TaskComplete ();
               break;
            }
         }

         if (IsEntityNull (ent))
         {
            if (m_currentWeapon != WEAPON_FLASHBANG)
            {
               if (pev->weapons & (1 << WEAPON_FLASHBANG))
                  SelectWeaponByName ("weapon_flashbang");
            }
            else if (!(pev->oldbuttons & IN_ATTACK))
               pev->button |= IN_ATTACK;
         }
      }
      pev->button |= m_campButtons;
      break;

   // smoke grenade throw behavior
   // a bit different to the others because it mostly tries to throw the sg on the ground
   case TASK_THROWSMOKE:
      m_aimFlags |= AIM_GRENADE;

      if (!(m_states & STATE_SEEING_ENEMY))
      {
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         m_moveToGoal = false;
      }

      m_checkTerrain = false;
      m_isUsingGrenade = true;

      src = m_lastEnemyOrigin - pev->velocity;

      // predict where the enemy is in 0.5 secs
      if (!IsEntityNull (m_enemy))
         src = src + m_enemy->v.velocity * 0.5;

      m_grenade = (src - EyePosition ()).Normalize ();

      if (GetTask ()->time < GetWorldTime () + 0.5)
      {
         m_aimFlags &= ~AIM_GRENADE;
         m_states &= ~STATE_THROW_SG;

         TaskComplete ();
         break;
      }

      if (m_currentWeapon != WEAPON_SMOKE)
      {
         if (pev->weapons & (1 << WEAPON_SMOKE))
         {
            SelectWeaponByName ("weapon_smokegrenade");
            GetTask ()->time = GetWorldTime () + MAX_GRENADE_TIMER;
         }
         else
            GetTask ()->time = GetWorldTime () + 0.1;
      }
      else if (!(pev->oldbuttons & IN_ATTACK))
         pev->button |= IN_ATTACK;
      break;

   // bot helps human player (or other bot) to get somewhere
   case TASK_DOUBLEJUMP:
      if (IsEntityNull (m_doubleJumpEntity) || !IsAlive (m_doubleJumpEntity) || (m_aimFlags & AIM_ENEMY) || (m_travelStartIndex != -1 && GetTask ()->time + (g_waypoint->GetTravelTime (pev->maxspeed, g_waypoint->GetPath (m_travelStartIndex)->origin, m_doubleJumpOrigin) + 11.0) < GetWorldTime ()))
      {
         ResetDoubleJumpState ();
         break;
      }
      m_aimFlags |= AIM_NAVPOINT;

      if (m_jumpReady)
      {
         m_moveToGoal = false;
         m_checkTerrain = false;

         m_navTimeset = GetWorldTime ();
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         if (m_duckForJump < GetWorldTime ())
            pev->button |= IN_DUCK;

         MakeVectors (nullvec);

         Vector dest = EyePosition () + g_pGlobals->v_forward * 500;
         dest.z = 180.0;

         TraceLine (EyePosition (), dest, false, true, GetEntity (), &tr);

         if ((tr.flFraction < 1.0) && (tr.pHit == m_doubleJumpEntity))
         {
            if (m_doubleJumpEntity->v.button & IN_JUMP)
            {
               m_duckForJump = GetWorldTime () + Random.Float (3.0, 5.0);
               GetTask ()->time = GetWorldTime ();
            }
         }
         break;
      }

      if (m_currentWaypointIndex == m_prevGoalIndex)
      {
         m_waypointOrigin = m_doubleJumpOrigin;
         m_destOrigin = m_doubleJumpOrigin;
      }

      if (DoWaypointNav ()) // reached destination?
         GetTask ()->data = -1;

      if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         destIndex = g_waypoint->FindNearest (m_doubleJumpOrigin);

         if (destIndex >= 0 && destIndex < g_numWaypoints)
         {
            m_prevGoalIndex = destIndex;
            GetTask ()->data = destIndex;
            m_travelStartIndex = m_currentWaypointIndex;

            // Always take the shortest path
            FindShortestPath (m_currentWaypointIndex, destIndex);

            if (m_currentWaypointIndex == destIndex)
               m_jumpReady = true;
         }
         else
            ResetDoubleJumpState ();
      }
      break;

   // escape from bomb behaviour
   case TASK_ESCAPEFROMBOMB:
      m_aimFlags |= AIM_NAVPOINT;

      if (!g_bombPlanted)
         TaskComplete ();

      if (IsShieldDrawn ())
         pev->button |= IN_ATTACK2;

      if (m_currentWeapon != WEAPON_KNIFE && GetNearbyEnemiesNearPosition (pev->origin, 9999) == 0)
         SelectWeaponByName ("weapon_knife");

      if (DoWaypointNav ()) // reached destination?
      {
         TaskComplete (); // we're done

         // press duck button if we still have some enemies
         if (GetNearbyEnemiesNearPosition (pev->origin, 2048))
            m_campButtons = IN_DUCK;

         // we're reached destination point so just sit down and camp
         StartTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + 10.0, true);
      }
      else if (!GoalIsValid ()) // didn't choose goal waypoint yet?
      {
         DeleteSearchNodes ();

         int lastSelectedGoal = -1;
         float safeRadius = Random.Float (1024.0, 2048.0), minPathDistance = 4096.0;

         for (i = 0; i < g_numWaypoints; i++)
         {
            if ((g_waypoint->GetPath (i)->origin - g_waypoint->GetBombPosition ()).GetLength () < safeRadius)
               continue;

            float pathDistance = g_waypoint->GetPathDistance (m_currentWaypointIndex, i);

            if (minPathDistance > pathDistance)
            {
               minPathDistance = pathDistance;
               lastSelectedGoal = i;
            }
         }

         if (lastSelectedGoal < 0)
            lastSelectedGoal = g_waypoint->FindFarest (pev->origin, safeRadius);

         m_prevGoalIndex = lastSelectedGoal;
         GetTask ()->data = lastSelectedGoal;

         FindShortestPath (m_currentWaypointIndex, lastSelectedGoal);
      }
      break;

   // shooting breakables in the way action
   case TASK_SHOOTBREAKABLE:
      m_aimFlags |= AIM_OVERRIDE;

      // Breakable destroyed?
      if (IsEntityNull (FindBreakable ()))
      {
         TaskComplete ();
         break;
      }
      pev->button |= m_campButtons;

      m_checkTerrain = false;
      m_moveToGoal = false;
      m_navTimeset = GetWorldTime ();

      src = m_breakable;
      m_camp = src;

      // is bot facing the breakable?
      if (GetShootingConeDeviation (GetEntity (), &src) >= 0.90)
      {
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;

         if (m_currentWeapon == WEAPON_KNIFE)
            SelectBestWeapon ();

         m_wantsToFire = true;
      }
      else
      {
         m_checkTerrain = true;
         m_moveToGoal = true;
      }
      break;

   // picking up items and stuff behaviour
   case TASK_PICKUPITEM:
      if (IsEntityNull (m_pickupItem))
      {
         m_pickupItem = NULL;
         TaskComplete ();

         break;
      }

      if (m_pickupType == PICKUP_HOSTAGE)
         destination = m_pickupItem->v.origin + pev->view_ofs;
      else
         destination = GetEntityOrigin (m_pickupItem);

      m_destOrigin = destination;
      m_entity = destination;

      // find the distance to the item
      float itemDistance = (destination - pev->origin).GetLength ();

      switch (m_pickupType)
      {
      case PICKUP_WEAPON:
         m_aimFlags |= AIM_NAVPOINT;

         // near to weapon?
         if (itemDistance < 50)
         {
            for (i = 0; i < 7; i++)
            {
               if (strcmp (g_weaponSelect[i].modelName, STRING (m_pickupItem->v.model) + 9) == 0)
                  break;
            }

            if (i < 7)
            {
               // secondary weapon. i.e., pistol
               int weaponID = 0;

               for (i = 0; i < 7; i++)
               {
                  if (pev->weapons & (1 << g_weaponSelect[i].id))
                     weaponID = i;
               }

               if (weaponID > 0)
               {
                  SelectWeaponbyNumber (weaponID);
                  FakeClientCommand (GetEntity (), "drop");

                  if (HasShield ()) // If we have the shield...
                     FakeClientCommand (GetEntity (), "drop"); // discard both shield and pistol
               }
               EquipInBuyzone (0);
            }
            else
            {
               // primary weapon
               int weaponID = GetHighestWeapon ();

               if ((weaponID > 6) || HasShield ())
               {
                  SelectWeaponbyNumber (weaponID);
                  FakeClientCommand (GetEntity (), "drop");
               }
               EquipInBuyzone (0);
            }
            CheckSilencer (); // check the silencer
         }
         break;

      case PICKUP_SHIELD:
         m_aimFlags |= AIM_NAVPOINT;

         if (HasShield ())
         {
            m_pickupItem = NULL;
            break;
         }
         else if (itemDistance < 50) // near to shield?
         {
            // get current best weapon to check if it's a primary in need to be dropped
            int weaponID = GetHighestWeapon ();

            if (weaponID > 6)
            {
               SelectWeaponbyNumber (weaponID);
               FakeClientCommand (GetEntity (), "drop");
            }
         }
         break;

      case PICKUP_PLANTED_C4:
         m_aimFlags |= AIM_ENTITY;

         if (m_team == TEAM_CF && itemDistance < 55)
         {
            ChatterMessage (Chatter_DefusingC4);

            // notify team of defusing
            if (GetNearbyFriendsNearPosition (pev->origin, 9999) < 2)
               RadioMessage (Radio_NeedBackup);

            m_moveToGoal = false;
            m_checkTerrain = false;

            m_moveSpeed = 0;
            m_strafeSpeed = 0;

            StartTask (TASK_DEFUSEBOMB, TASKPRI_DEFUSEBOMB, -1, 0.0, false);
         }
         break;

      case PICKUP_HOSTAGE:
         m_aimFlags |= AIM_ENTITY;
         src = EyePosition ();

         if (!IsAlive (m_pickupItem))
         {
            // don't pickup dead hostages
            m_pickupItem = NULL;
            TaskComplete ();

            break;
         }

         if (itemDistance < 50)
         {
            float angleToEntity = InFieldOfView (destination - src);

            if (angleToEntity <= 10) // bot faces hostage?
            {
               // use game dll function to make sure the hostage is correctly 'used'
               MDLL_Use (m_pickupItem, GetEntity ());

               if (Random.Long (0, 100) < 80)
                  ChatterMessage (Chatter_UseHostage);

               for (i = 0; i < MAX_HOSTAGES; i++)
               {
                  if (IsEntityNull (m_hostages[i])) // store pointer to hostage so other bots don't steal from this one or bot tries to reuse it
                  {
                     m_hostages[i] = m_pickupItem;
                     m_pickupItem = NULL;

                     break;
                  }
               }
            }
            m_lastCollTime = GetWorldTime () + 0.1; // also don't consider being stuck
         }
         break;

      case PICKUP_DEFUSEKIT:
         m_aimFlags |= AIM_NAVPOINT;

         if (m_hasDefuser)
         {
            m_pickupItem = NULL;
            m_pickupType = PICKUP_NONE;
         }
         break;

      case PICKUP_BUTTON:
         m_aimFlags |= AIM_ENTITY;

         if (IsEntityNull (m_pickupItem) || m_buttonPushTime < GetWorldTime ()) // it's safer...
         {
            TaskComplete ();
            m_pickupType = PICKUP_NONE;

            break;
         }

         // find angles from bot origin to entity...
         src = EyePosition ();
         float angleToEntity = InFieldOfView (destination - src);

         if (itemDistance < 90) // near to the button?
         {
            m_moveSpeed = 0.0;
            m_strafeSpeed = 0.0;
            m_moveToGoal = false;
            m_checkTerrain = false;

            if (angleToEntity <= 10) // facing it directly?
            {
               MDLL_Use (m_pickupItem, GetEntity ());

               m_pickupItem = NULL;
               m_pickupType = PICKUP_NONE;
               m_buttonPushTime = GetWorldTime () + 3.0;

               TaskComplete ();
            }
         }
         break;
      }
      break;
   }
}

void Bot::CheckSpawnTimeConditions (void)
{
   // this function is called instead of BotAI when buying finished, but freezetime is not yet left.

   // switch to knife if time to do this
   if (m_checkKnifeSwitch && !m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + Random.Float (4.0, 6.5) < GetWorldTime ())
   {
      if (Random.Long (1, 100) < 2 && yb_spraypaints.GetBool ())
         StartTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, -1, GetWorldTime () + 1.0, false);

      if (m_difficulty >= 2 && Random.Long (0, 100) < (m_personality == PERSONALITY_RUSHER ? 99 : 50) && !m_isReloading && (g_mapType & (MAP_CS | MAP_DE | MAP_ES | MAP_AS)))
         SelectWeaponByName ("weapon_knife");

      m_checkKnifeSwitch = false;

      if (Random.Long (0, 100) < yb_user_follow_percent.GetInt () && IsEntityNull (m_targetEntity) && !m_isLeader && !m_hasC4)
         AttachToUser ();
   }

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + Random.Float (2.0, 3.5) < GetWorldTime ())
   {
      if (HasShield () && IsShieldDrawn ())
         pev->button |= IN_ATTACK2;
      else
      {
         switch (m_currentWeapon)
         {
         case WEAPON_M4A1:
         case WEAPON_USP:
            CheckSilencer ();
            break;

         case WEAPON_FAMAS:
         case WEAPON_GLOCK:
            if (Random.Long (0, 100) < 50)
               pev->button |= IN_ATTACK2;
            break;
         }
      }

      // select a leader bot for this team
      SelectLeaderEachTeam (m_team);
      m_checkWeaponSwitch = false;
   }
}

void Bot::BotAI (void)
{
   // this function gets called each frame and is the core of all bot ai. from here all other subroutines are called

   float movedDistance; // length of different vector (distance bot moved)
   TraceResult tr;

   // increase reaction time
   m_actualReactionTime += 0.3;

   if (m_actualReactionTime > m_idealReactionTime)
      m_actualReactionTime = m_idealReactionTime;

   // bot could be blinded by flashbang or smoke, recover from it
   m_viewDistance += 3.0;

   if (m_viewDistance > m_maxViewDistance)
      m_viewDistance = m_maxViewDistance;

   if (m_blindTime > GetWorldTime ())
      m_maxViewDistance = 4096.0;        

   m_moveSpeed = pev->maxspeed;

   if (m_prevTime <= GetWorldTime ())
   {
      // see how far bot has moved since the previous position...
      movedDistance = (m_prevOrigin - pev->origin).GetLength ();

      // save current position as previous
      m_prevOrigin = pev->origin;
      m_prevTime = GetWorldTime () + 0.2;
   }
   else
      movedDistance = 2.0;

   // if there's some radio message to respond, check it
   if (m_radioOrder != 0)
      CheckRadioCommands ();

   // do all sensing, calculate/filter all actions here
   SetConditions ();

   // some stuff required by by chatter engine
   if ((m_states & STATE_SEEING_ENEMY) && !IsEntityNull (m_enemy))
   {
      if (Random.Long (0, 100) < 45 && GetNearbyFriendsNearPosition (pev->origin, 512) == 0 && (m_enemy->v.weapons & (1 << WEAPON_C4)))
         ChatterMessage (Chatter_SpotTheBomber);

      if (Random.Long (0, 100) < 45 && m_team == TEAM_TF && GetNearbyFriendsNearPosition (pev->origin, 512) == 0 && *g_engfuncs.pfnInfoKeyValue (g_engfuncs.pfnGetInfoKeyBuffer (m_enemy), "model") == 'v')
         ChatterMessage (Chatter_VIPSpotted);

      if (Random.Long (0, 100) < 50 && GetNearbyFriendsNearPosition (pev->origin, 450) == 0 && GetTeam (m_enemy) != m_team && IsGroupOfEnemies (m_enemy->v.origin, 2, 384))
         ChatterMessage (Chatter_ScaredEmotion);

      if (Random.Long (0, 100) < 40 && GetNearbyFriendsNearPosition (pev->origin, 1024) == 0 && ((m_enemy->v.weapons & (1 << WEAPON_AWP)) || (m_enemy->v.weapons & (1 << WEAPON_SCOUT)) ||  (m_enemy->v.weapons & (1 << WEAPON_G3SG1)) || (m_enemy->v.weapons & (1 << WEAPON_SG550))))
         ChatterMessage (Chatter_SniperWarning);
   }

   // if bot is trapped under shield yell for help !
   if (GetTaskId () == TASK_CAMP && HasShield() && IsShieldDrawn () && GetNearbyEnemiesNearPosition (pev->origin, 650) >= 2 && IsEnemyViewable(m_enemy))
      InstantChatterMessage(Chatter_Pinned_Down);

   // if bomb planted warn teammates !
   if (g_canSayBombPlanted && g_bombPlanted && GetTeam (GetEntity()) == TEAM_CF)
   {
      g_canSayBombPlanted = false;
      ChatterMessage (Chatter_GottaFindTheBomb);
   }

   Vector src, destination;

   m_checkTerrain = true;
   m_moveToGoal = true;
   m_wantsToFire = false;

   AvoidGrenades (); // avoid flyings grenades
   m_isUsingGrenade = false;

   RunTask (); // execute current task
   ChooseAimDirection (); // choose aim direction
   FacePosition (); // and turn to chosen aim direction

   // the bots wants to fire at something?
   if (m_wantsToFire && !m_isUsingGrenade && m_shootTime <= GetWorldTime ())
      FireWeapon (); // if bot didn't fire a bullet try again next frame

   // check for reloading
   if (m_reloadCheckTime <= GetWorldTime ())
      CheckReload ();

   // set the reaction time (surprise momentum) different each frame according to skill
   SetIdealReactionTimes ();

   // calculate 2 direction vectors, 1 without the up/down component
   Vector directionOld = m_destOrigin - (pev->origin + pev->velocity * m_frameInterval);
   Vector directionNormal = directionOld.Normalize ();

   Vector direction = directionNormal;
   directionNormal.z = 0.0;

   m_moveAngles = directionOld.ToAngles ();

   m_moveAngles.ClampAngles ();
   m_moveAngles.x *= -1.0; // invert for engine

   if (m_difficulty == 4 && ((m_aimFlags & AIM_ENEMY) || (m_states & (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY)) || (GetTaskId () == TASK_SEEKCOVER && (m_isReloading || m_isVIP))) && !yb_jasonmode.GetBool () && GetTaskId () != TASK_CAMP && !IsOnLadder ())
   {
      m_moveToGoal = false; // don't move to goal
      m_navTimeset = GetWorldTime ();

      if (IsValidPlayer (m_enemy))
         CombatFight ();
   }

   // check if we need to escape from bomb
   if ((g_mapType & MAP_DE) && g_bombPlanted && m_notKilled && GetTaskId () != TASK_ESCAPEFROMBOMB && GetTaskId () != TASK_CAMP && OutOfBombTimer ())
   {
      TaskComplete (); // complete current task

      // then start escape from bomb immidiate
      StartTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0, true);
   }

   // allowed to move to a destination position?
   if (m_moveToGoal)
   {
      GetValidWaypoint ();

      // Press duck button if we need to
      if ((m_currentPath->flags & FLAG_CROUCH) && !(m_currentPath->flags & FLAG_CAMP))
         pev->button |= IN_DUCK;

      m_timeWaypointMove = GetWorldTime ();

      if (IsInWater ()) // special movement for swimming here
      {
         // check if we need to go forward or back press the correct buttons
         if (InFieldOfView (m_destOrigin - EyePosition ()) > 90)
            pev->button |= IN_BACK;
         else
            pev->button |= IN_FORWARD;

         if (m_moveAngles.x > 60.0)
            pev->button |= IN_DUCK;
         else if (m_moveAngles.x < -60.0)
            pev->button |= IN_JUMP;
      }
   }

   if (m_checkTerrain) // are we allowed to check blocking terrain (and react to it)?
      CheckTerrain (movedDistance, direction, directionNormal);

   // must avoid a grenade?
   if (m_needAvoidGrenade != 0)
   {
      // Don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * m_needAvoidGrenade;
   }

   // time to reach waypoint
   if (m_navTimeset + GetEstimatedReachTime () < GetWorldTime () && IsEntityNull (m_enemy))
   {
      GetValidWaypoint ();

      // clear these pointers, bot mingh be stuck getting to them
      if (!IsEntityNull (m_pickupItem) && !m_hasProgressBar)
         m_itemIgnore = m_pickupItem;

      m_pickupItem = NULL;
      m_breakableEntity = NULL;
      m_itemCheckTime = GetWorldTime () + 5.0;
      m_pickupType = PICKUP_NONE;
   }

   if (m_duckTime >= GetWorldTime ())
      pev->button |= IN_DUCK;

   if (pev->button & IN_JUMP)
      m_jumpTime = GetWorldTime ();

   if (m_jumpTime + 0.85 > GetWorldTime ())
   {
      if (!IsOnFloor () && !IsInWater ())
         pev->button |= IN_DUCK;
   }

   if (!(pev->button & (IN_FORWARD | IN_BACK)))
   {
      if (m_moveSpeed > 0)
         pev->button |= IN_FORWARD;
      else if (m_moveSpeed < 0)
         pev->button |= IN_BACK;
   }

   if (!(pev->button & (IN_MOVELEFT | IN_MOVERIGHT)))
   {
      if (m_strafeSpeed > 0)
         pev->button |= IN_MOVERIGHT;
      else if (m_strafeSpeed < 0)
         pev->button |= IN_MOVELEFT;
   }

   static float timeDebugUpdate = 0.0;

   if (!IsEntityNull (g_hostEntity) && yb_debug.GetInt () >= 1)
   {
      int specIndex = g_hostEntity->v.iuser2;

      if (specIndex == IndexOfEntity (GetEntity ()))
      {
         static int index, goal, taskID;

         if (!m_tasks.IsEmpty ())
         {
            if (taskID != GetTaskId () || index != m_currentWaypointIndex || goal != GetTask ()->data || timeDebugUpdate < GetWorldTime ())
            {
               taskID = GetTaskId ();
               index = m_currentWaypointIndex;
               goal = GetTask ()->data;

               char taskName [80];
               memset (taskName, 0, sizeof (taskName));

               switch (taskID)
               {
               case TASK_NORMAL:
                  sprintf (taskName, "Normal");
                  break;

               case TASK_PAUSE:
                  sprintf (taskName, "Pause");
                  break;

               case TASK_MOVETOPOSITION:
                  sprintf (taskName, "MoveToPosition");
                  break;

               case TASK_FOLLOWUSER:
                  sprintf (taskName, "FollowUser");
                  break;

               case TASK_WAITFORGO:
                  sprintf (taskName, "WaitForGo");
                  break;

               case TASK_PICKUPITEM:
                  sprintf (taskName, "PickupItem");
                  break;

               case TASK_CAMP:
                  sprintf (taskName, "Camp");
                  break;

               case TASK_PLANTBOMB:
                  sprintf (taskName, "PlantBomb");
                  break;

               case TASK_DEFUSEBOMB:
                  sprintf (taskName, "DefuseBomb");
                  break;

               case TASK_ATTACK:
                  sprintf (taskName, "AttackEnemy");
                  break;

               case TASK_HUNTENEMY:
                  sprintf (taskName, "HuntEnemy");
                  break;

               case TASK_SEEKCOVER:
                  sprintf (taskName, "SeekCover");
                  break;

               case TASK_THROWHEGRENADE:
                  sprintf (taskName, "ThrowExpGrenade");
                  break;

               case TASK_THROWFLASHBANG:
                  sprintf (taskName, "ThrowFlashGrenade");
                  break;

               case TASK_THROWSMOKE:
                  sprintf (taskName, "ThrowSmokeGrenade");
                  break;

               case TASK_DOUBLEJUMP:
                  sprintf (taskName, "PerformDoubleJump");
                  break;

               case TASK_ESCAPEFROMBOMB:
                  sprintf (taskName, "EscapeFromBomb");
                  break;

               case TASK_SHOOTBREAKABLE:
                  sprintf (taskName, "ShootBreakable");
                  break;

               case TASK_HIDE:
                  sprintf (taskName, "Hide");
                  break;

               case TASK_BLINDED:
                  sprintf (taskName, "Blinded");
                  break;

               case TASK_SPRAY:
                  sprintf (taskName, "SprayLogo");
                  break;
               }

               char enemyName[80], weaponName[80], aimFlags[32], botType[32];

               if (!IsEntityNull (m_enemy))
                  strcpy (enemyName, STRING (m_enemy->v.netname));
               else if (!IsEntityNull (m_lastEnemy))
               {
                  strcpy (enemyName, " (L)");
                  strcat (enemyName, STRING (m_lastEnemy->v.netname));
               }
               else
                  strcpy (enemyName, " (null)");

               char pickupName[80];
               memset (pickupName, 0, sizeof (pickupName));

               if (!IsEntityNull (m_pickupItem))
                  strcpy (pickupName, STRING (m_pickupItem->v.classname));
               else
                  strcpy (pickupName, " (null)");

               WeaponSelect *selectTab = &g_weaponSelect[0];
               char weaponCount = 0;

               while (m_currentWeapon != selectTab->id && weaponCount < NUM_WEAPONS)
               {
                  selectTab++;
                  weaponCount++;
               }
               memset (aimFlags, 0, sizeof (aimFlags));

               // set the aim flags
               sprintf (aimFlags, "%s%s%s%s%s%s%s%s",
                  m_aimFlags & AIM_NAVPOINT ? " NavPoint" : "",
                  m_aimFlags & AIM_CAMP ? " CampPoint" : "",
                  m_aimFlags & AIM_PREDICT_PATH ? " PredictPath" : "",
                  m_aimFlags & AIM_LAST_ENEMY ? " LastEnemy" : "",
                  m_aimFlags & AIM_ENTITY ? " Entity" : "",
                  m_aimFlags & AIM_ENEMY ? " Enemy" : "",
                  m_aimFlags & AIM_GRENADE ? " Grenade" : "",
                  m_aimFlags & AIM_OVERRIDE ? " Override" : "");

               // set the bot type
               sprintf (botType, "%s%s%s", m_personality == PERSONALITY_RUSHER ? " Rusher" : "",
                  m_personality == PERSONALITY_CAREFUL ? " Careful" : "",
                  m_personality == PERSONALITY_NORMAL ? " Normal" : "");

               if (weaponCount >= NUM_WEAPONS)
               {
                  // prevent printing unknown message from known weapons
                  switch (m_currentWeapon)
                  {
                  case WEAPON_EXPLOSIVE:
                     strcpy (weaponName, "weapon_hegrenade");
                     break;

                  case WEAPON_FLASHBANG:
                     strcpy (weaponName, "weapon_flashbang");
                     break;

                  case WEAPON_SMOKE:
                     strcpy (weaponName, "weapon_smokegrenade");
                     break;

                  case WEAPON_C4:
                     strcpy (weaponName, "weapon_c4");
                     break;

                  default:
                     sprintf (weaponName, "Unknown! (%d)", m_currentWeapon);
                  }
               }
               else
                  strcpy (weaponName, selectTab->weaponName);

               char outputBuffer[512];
               memset (outputBuffer, 0, sizeof (outputBuffer));

               sprintf (outputBuffer, "\n\n\n\n%s (H:%.1f/A:%.1f)- Task: %d=%s Desire:%.02f\nItem: %s Clip: %d Ammo: %d%s Money: %d AimFlags: %s\nSP=%.02f SSP=%.02f I=%d PG=%d G=%d T: %.02f MT: %d\nEnemy=%s Pickup=%s Type=%s\n", STRING (pev->netname), pev->health, pev->armorvalue, taskID, taskName, GetTask ()->desire, &weaponName[7], GetAmmoInClip (), GetAmmo (), m_isReloading ? " (R)" : "", m_moneyAmount, aimFlags, m_moveSpeed, m_strafeSpeed, index, m_prevGoalIndex, goal, m_navTimeset - GetWorldTime (), pev->movetype, enemyName, pickupName, botType);

               MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, g_hostEntity);
                  WRITE_BYTE (TE_TEXTMESSAGE);
                  WRITE_BYTE (1);
                  WRITE_SHORT (FixedSigned16 (-1, 1 << 13));
                  WRITE_SHORT (FixedSigned16 (0, 1 << 13));
                  WRITE_BYTE (0);
                  WRITE_BYTE (m_team == TEAM_CF ? 0 : 255);
                  WRITE_BYTE (100);
                  WRITE_BYTE (m_team != TEAM_CF ? 0 : 255);
                  WRITE_BYTE (0);
                  WRITE_BYTE (255);
                  WRITE_BYTE (255);
                  WRITE_BYTE (255);
                  WRITE_BYTE (0);
                  WRITE_SHORT (FixedUnsigned16 (0, 1 << 8));
                  WRITE_SHORT (FixedUnsigned16 (0, 1 << 8));
                  WRITE_SHORT (FixedUnsigned16 (1.0, 1 << 8));
                  WRITE_STRING (const_cast <const char *> (&outputBuffer[0]));
               MESSAGE_END ();

               timeDebugUpdate = GetWorldTime () + 1.0;
            }

            // green = destination origin
            // blue = ideal angles
            // red = view angles

            DrawArrow (g_hostEntity, EyePosition (), m_destOrigin, 10, 0, 0, 255, 0, 250, 5, 1);

            MakeVectors (m_idealAngles);
            DrawArrow (g_hostEntity, EyePosition (), EyePosition () + (g_pGlobals->v_forward * 300), 10, 0, 0, 0, 255, 250, 5, 1);

            MakeVectors (pev->v_angle);
            DrawArrow (g_hostEntity, EyePosition (), EyePosition () + (g_pGlobals->v_forward * 300), 10, 0, 255, 0, 0, 250, 5, 1);

            // now draw line from source to destination
            PathNode *node = &m_navNode[0];

            while (node != NULL)
            {
               Vector srcPath = g_waypoint->GetPath (node->index)->origin;
               node = node->next;

               if (node != NULL)
               {
                  Vector dest = g_waypoint->GetPath (node->index)->origin;
                  DrawArrow (g_hostEntity, srcPath, dest, 15, 0, 255, 100, 55, 200, 5, 1);
               }
            }
         }
      }
   }

   // save the previous speed (for checking if stuck)
   m_prevSpeed = fabsf (m_moveSpeed);
   m_lastDamageType = -1; // reset damage

   pev->angles.ClampAngles ();
   pev->v_angle.ClampAngles ();
}

bool Bot::HasHostage (void)
{
   for (int i = 0; i < MAX_HOSTAGES; i++)
   {
      if (!IsEntityNull (m_hostages[i]))
      {
         // don't care about dead hostages
         if (m_hostages[i]->v.health <= 0 || (pev->origin - m_hostages[i]->v.origin).GetLength () > 600)
         {
            m_hostages[i] = NULL;
            continue;
         }
         return true;
      }
   }
   return false;
}

void Bot::ResetCollideState (void)
{
   m_collideTime = 0.0;
   m_probeTime = 0.0;

   m_collisionProbeBits = 0;
   m_collisionState = COLLISION_NOTDECICED;
   m_collStateIndex = 0;

   for (int i = 0; i < 4; i++)
      m_collideMoves[i] = 0;
}

int Bot::GetAmmo (void)
{
   if (g_weaponDefs[m_currentWeapon].ammo1 == -1)
      return 0;

   return m_ammo[g_weaponDefs[m_currentWeapon].ammo1];
}

void Bot::TakeDamage (edict_t *inflictor, int damage, int armor, int bits)
{
   // this function gets called from the network message handler, when bot's gets hurt from any
   // other player.

   m_lastDamageType = bits;
   CollectGoalExperience (damage, m_team);

   if (IsValidPlayer (inflictor))
   {
      if (GetTeam (inflictor) == m_team && yb_tkpunish.GetBool () && !g_botManager->GetBot (inflictor))
      {
         // alright, die you teamkiller!!!
         m_actualReactionTime = 0.0;
         m_seeEnemyTime = GetWorldTime();
         m_enemy = inflictor;

         m_lastEnemy = m_enemy;
         m_lastEnemyOrigin = m_enemy->v.origin;
         m_enemyOrigin = m_enemy->v.origin;

         ChatMessage(CHAT_TEAMATTACK);
         HandleChatterMessage("#Bot_TeamAttack");
         ChatterMessage(Chatter_FriendlyFire);
      }
      else
      {
         // attacked by an enemy
         if (pev->health > 60)
         {
            m_agressionLevel += 0.1;

            if (m_agressionLevel > 1.0)
               m_agressionLevel += 1.0;
         }
         else
         {
            m_fearLevel += 0.03;

            if (m_fearLevel > 1.0)
               m_fearLevel += 1.0;
         }
         RemoveCertainTask (TASK_CAMP);

         if (IsEntityNull (m_enemy) && m_team != GetTeam (inflictor))
         {
            m_lastEnemy = inflictor;
            m_lastEnemyOrigin = inflictor->v.origin;

            // FIXME - Bot doesn't necessary sees this enemy
            m_seeEnemyTime = GetWorldTime ();
         }

         if (yb_csdm_mode.GetInt () == 0)
            CollectExperienceData (inflictor, armor + damage);
      }
   }
   else // hurt by unusual damage like drowning or gas
   {
      // leave the camping/hiding position
      if (!g_waypoint->Reachable (this, g_waypoint->FindNearest (m_destOrigin)))
      {
         DeleteSearchNodes ();
         FindWaypoint ();
      }
   }
}

void Bot::TakeBlinded (const Vector &fade, int alpha)
{
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind froumd the grenade.

   extern ConVar yb_aim_method;

   if (fade.x != 255 || fade.y != 255 || fade.z != 255 || alpha <= 200 || yb_aim_method.GetInt () == 1)
      return;

   m_enemy = NULL;

   m_maxViewDistance = Random.Float (10, 20);
   m_blindTime = GetWorldTime () + static_cast <float> (alpha - 200) / 16;

   if (m_difficulty <= 2)
   {
      m_blindMoveSpeed = 0.0;
      m_blindSidemoveSpeed = 0.0;
      m_blindButton = IN_DUCK;
   }
   else if (m_difficulty > 2)
   {
      m_blindMoveSpeed = -pev->maxspeed;
      m_blindSidemoveSpeed = 0.0;

      float walkSpeed = GetWalkSpeed ();

      if (Random.Long (0, 100) > 50)
         m_blindSidemoveSpeed = walkSpeed;
      else
         m_blindSidemoveSpeed = -walkSpeed;

      if (pev->health < 85)
         m_blindMoveSpeed = -walkSpeed;
      else if (m_personality == PERSONALITY_CAREFUL)
      {
         m_blindMoveSpeed = 0.0;
         m_blindButton = IN_DUCK;
      }
      else
         m_blindMoveSpeed = walkSpeed;
   }
}

void Bot::CollectGoalExperience (int damage, int team)
{
   // gets called each time a bot gets damaged by some enemy. tries to achieve a statistic about most/less dangerous
   // waypoints for a destination goal used for pathfinding

   if (g_numWaypoints < 1 || g_waypointsChanged || m_chosenGoalIndex < 0 || m_prevGoalIndex < 0)
      return;

   // only rate goal waypoint if bot died because of the damage
   // FIXME: could be done a lot better, however this cares most about damage done by sniping or really deadly weapons
   if (pev->health - damage <= 0)
   {
      if (team == TEAM_TF)
      {
         int value = (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team0Value;
         value -= static_cast <int> (pev->health / 20);

         if (value < -MAX_GOAL_VALUE)
            value = -MAX_GOAL_VALUE;

         else if (value > MAX_GOAL_VALUE)
            value = MAX_GOAL_VALUE;

         (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team0Value = static_cast <signed short> (value);
      }
      else
      {
         int value = (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team1Value;
         value -= static_cast <int> (pev->health / 20);

         if (value < -MAX_GOAL_VALUE)
            value = -MAX_GOAL_VALUE;

         else if (value > MAX_GOAL_VALUE)
            value = MAX_GOAL_VALUE;

         (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team1Value = static_cast <signed short> (value);
      }
   }
}

void Bot::CollectExperienceData (edict_t *attacker, int damage)
{
   // this function gets called each time a bot gets damaged by some enemy. sotores the damage (teamspecific) done by victim.

   if (!IsValidPlayer (attacker))
      return;

   int attackerTeam = GetTeam (attacker);
   int victimTeam = m_team;

   if (attackerTeam == victimTeam )
      return;

   // if these are bots also remember damage to rank destination of the bot
   m_goalValue -= static_cast <float> (damage);

   if (g_botManager->GetBot (attacker) != NULL)
      g_botManager->GetBot (attacker)->m_goalValue += static_cast <float> (damage);

   if (damage < 20)
      return; // do not collect damage less than 20

   int attackerIndex = g_waypoint->FindNearest (attacker->v.origin);
   int victimIndex = g_waypoint->FindNearest (pev->origin);

   if (pev->health > 20)
   {
      if (victimTeam == TEAM_TF)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage++;
      else
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage++;

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage > MAX_DAMAGE_VALUE)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage = MAX_DAMAGE_VALUE;

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage > MAX_DAMAGE_VALUE)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage = MAX_DAMAGE_VALUE;
   }

   float fUpdate = IsValidBot (attacker) ? 10.0 : 7.0;

   // store away the damage done
   if (victimTeam == TEAM_TF)
   {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage;
      value += static_cast <int> (damage / fUpdate);

      if (value > MAX_DAMAGE_VALUE)
         value = MAX_DAMAGE_VALUE;

      if (value > g_highestDamageT)
         g_highestDamageT = value;

      (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage = static_cast <unsigned short> (value);
   }
   else
   {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team1Damage;
      value += static_cast <int> (damage / fUpdate);

      if (value > MAX_DAMAGE_VALUE)
         value = MAX_DAMAGE_VALUE;

      if (value > g_highestDamageCT)
         g_highestDamageCT = value;

      (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team1Damage = static_cast <unsigned short> (value);
   }
}

void Bot::HandleChatterMessage (const char *tempMessage)
{
   // this function is added to prevent engine crashes with: 'Message XX started, before message XX ended', or something.

   if (FStrEq (tempMessage, "#CTs_Win") && (m_team == TEAM_CF))
   {
      if (g_timeRoundMid > GetWorldTime ())
         ChatterMessage (Chatter_QuicklyWonTheRound);
      else
         ChatterMessage (Chatter_WonTheRound);
   }

   if (FStrEq (tempMessage, "#Terrorists_Win") && (m_team == TEAM_TF))
   {
      if (g_timeRoundMid > GetWorldTime ())
         ChatterMessage (Chatter_QuicklyWonTheRound);
      else
         ChatterMessage (Chatter_WonTheRound);
   }

   if (FStrEq (tempMessage, "#Bot_TeamAttack"))
      ChatterMessage (Chatter_FriendlyFire);

   if (FStrEq (tempMessage, "#Bot_NiceShotCommander"))
      ChatterMessage (Chatter_NiceshotCommander);

   if (FStrEq (tempMessage, "#Bot_NiceShotPall"))
      ChatterMessage (Chatter_NiceshotPall);
}

void Bot::ChatMessage (int type, bool isTeamSay)
{
   extern ConVar yb_chat;

   if (g_chatFactory[type].IsEmpty () || !yb_chat.GetBool ())
      return;

   const char *pickedPhrase = g_chatFactory[type].GetRandomElement ().GetBuffer ();

   if (IsNullString (pickedPhrase))
      return;

   PrepareChatMessage (const_cast <char *> (pickedPhrase));
   PushMessageQueue (isTeamSay ? GSM_SAY_TEAM : GSM_SAY);
}

void Bot::DiscardWeaponForUser (edict_t *user, bool discardC4)
{
   // this function, asks bot to discard his current primary weapon (or c4) to the user that requsted it with /drop*
   // command, very useful, when i'm don't have money to buy anything... )

   if (IsAlive (user) && m_moneyAmount >= 2000 && HasPrimaryWeapon () && (user->v.origin - pev->origin).GetLength () <= 240)
   {
      m_aimFlags |= AIM_ENTITY;
      m_lookAt = user->v.origin;

      if (discardC4)
      {
         SelectWeaponByName ("weapon_c4");
         FakeClientCommand (GetEntity (), "drop");
      }
      else
      {
         SelectBestWeapon ();
         FakeClientCommand (GetEntity (), "drop");
      }

      m_pickupItem = NULL;
      m_pickupType = PICKUP_NONE;
      m_itemCheckTime = GetWorldTime () + 5.0;

      if (m_inBuyZone)
      {
         m_buyingFinished = false;
         m_buyState = 0;

         PushMessageQueue (GSM_BUY_STUFF);
         m_nextBuyTime = GetWorldTime ();
      }
   }
}

void Bot::ResetDoubleJumpState (void)
{
   TaskComplete ();

   m_doubleJumpEntity = NULL;
   m_duckForJump = 0.0;
   m_doubleJumpOrigin = nullvec;
   m_travelStartIndex = -1;
   m_jumpReady = false;
}

void Bot::DebugMsg (const char *format, ...)
{
   if (yb_debug.GetInt () < 2)
      return;

   va_list ap;
   char buffer[1024];

   va_start (ap, format);
   vsprintf (buffer, format, ap);
   va_end (ap);

   ServerPrintNoTag ("%s: %s", STRING (pev->netname), buffer);

   if (yb_debug.GetInt () >= 3)
      AddLogEntry (false, LL_DEFAULT, "%s: %s", STRING (pev->netname), buffer);
}

Vector Bot::CheckToss (const Vector &start, Vector end)
{
   // this function returns the velocity at which an object should looped from start to land near end.
   // returns null vector if toss is not feasible.

   TraceResult tr;
   float gravity = sv_gravity.GetFloat () * 0.55;

   end = end - pev->velocity;
   end.z -= 15.0;

   if (fabsf (end.z - start.z) > 500.0)
      return nullvec;

   Vector midPoint = start + (end - start) * 0.5;
   TraceHull (midPoint, midPoint + Vector (0, 0, 500), true, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0)
   {
      midPoint = tr.vecEndPos;
      midPoint.z = tr.pHit->v.absmin.z - 1.0;
   }

   if ((midPoint.z < start.z) || (midPoint.z < end.z))
      return nullvec;

   float timeOne = sqrtf ((midPoint.z - start.z) / (0.5 * gravity));
   float timeTwo = sqrtf ((midPoint.z - end.z) / (0.5 * gravity));

   if (timeOne < 0.1)
      return nullvec;

   Vector nadeVelocity = (end - start) / (timeOne + timeTwo);
   nadeVelocity.z = gravity * timeOne;

   Vector apex = start + nadeVelocity * timeOne;
   apex.z = midPoint.z;

   TraceHull (start, apex, false, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0 || tr.fAllSolid)
      return nullvec;

   TraceHull (end, apex, true, head_hull, ENT (pev), &tr);

   if (tr.flFraction != 1.0)
   {
      float dot = -(tr.vecPlaneNormal | (apex - end).Normalize ());

      if (dot > 0.7 || tr.flFraction < 0.8) // 60 degrees
         return nullvec;
   }
   return nadeVelocity * 0.777;
}

Vector Bot::CheckThrow (const Vector &start, Vector end)
{
   // this function returns the velocity vector at which an object should be thrown from start to hit end.
   // returns null vector if throw is not feasible.

   Vector nadeVelocity = (end - start);
   TraceResult tr;

   float gravity = sv_gravity.GetFloat () * 0.55;
   float time = nadeVelocity.GetLength () / 195.0;

   if (time < 0.01)
      return nullvec;
   else if (time > 2.0)
      time = 1.2;

   nadeVelocity = nadeVelocity * (1.0 / time);
   nadeVelocity.z += gravity * time * 0.5;

   Vector apex = start + (end - start) * 0.5;
   apex.z += 0.5 * gravity * (time * 0.5) * (time * 0.5);

   TraceHull (start, apex, false, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0)
      return nullvec;

   TraceHull (end, apex, true, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0 || tr.fAllSolid)
   {
      float dot = -(tr.vecPlaneNormal | (apex - end).Normalize ());

      if (dot > 0.7 || tr.flFraction < 0.8)
         return nullvec;
   }
   return nadeVelocity * 0.7793;
}

Vector Bot::CheckBombAudible (void)
{
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!g_bombPlanted || (GetTaskId () == TASK_ESCAPEFROMBOMB))
      return nullvec; // reliability check

   if (m_difficulty >= 3)
      return g_waypoint->GetBombPosition();

   Vector bombOrigin = g_waypoint->GetBombPosition ();
   
   float timeElapsed = ((GetWorldTime () - g_timeBombPlanted) / mp_c4timer.GetFloat ()) * 100;
   float desiredRadius = 768.0;

   // start the manual calculations
   if (timeElapsed > 85.0)
      desiredRadius = 4096.0;
   else if (timeElapsed > 68.0)
      desiredRadius = 2048.0;
   else if (timeElapsed > 52.0)
      desiredRadius = 1280.0;
   else if (timeElapsed > 28.0)
      desiredRadius = 1024.0;

   // we hear bomb if length greater than radius
   if (desiredRadius < (pev->origin - bombOrigin).GetLength2D ())
      return bombOrigin;

   return nullvec;
}

void Bot::MoveToVector (Vector to)
{
   if (to == nullvec)
      return;

   FindPath (m_currentWaypointIndex, g_waypoint->FindNearest (to), 0);
}

byte Bot::ThrottledMsec (void)
{
   // estimate msec to use for this command based on time passed from the previous command
   float msecVal = (GetWorldTime () - m_lastCommandTime) * 1000.0f;

   int msecRest = 0;
   byte newMsec = static_cast <byte> (msecVal);

   if (newMsec < 10)
   {
      msecVal = msecVal - static_cast <float> (newMsec) + m_msecValRest;
      msecRest = static_cast <int> (msecVal);

      m_msecValRest = msecVal - static_cast <float> (msecRest);
   }
   newMsec += msecRest;

   // bots are going to be slower than they should if this happens.
   if (newMsec > 100)
      newMsec = 100;
   else if (newMsec < 1)
      newMsec = 1;

   return newMsec;
}

void Bot::RunPlayerMovement (void)
{
   // the purpose of this function is to compute, according to the specified computation
   // method, the msec value which will be passed as an argument of pfnRunPlayerMove. This
   // function is called every frame for every bot, since the RunPlayerMove is the function
   // that tells the engine to put the bot character model in movement. This msec value
   // tells the engine how long should the movement of the model extend inside the current
   // frame. It is very important for it to be exact, else one can experience bizarre
   // problems, such as bots getting stuck into each others. That's because the model's
   // bounding boxes, which are the boxes the engine uses to compute and detect all the
   // collisions of the model, only exist, and are only valid, while in the duration of the
   // movement. That's why if you get a pfnRunPlayerMove for one boINFt that lasts a little too
   // short in comparison with the frame's duration, the remaining time until the frame
   // elapses, that bot will behave like a ghost : no movement, but bullets and players can
   // pass through it. Then, when the next frame will begin, the stucking problem will arise !

   m_frameInterval = GetWorldTime () - m_lastCommandTime;

   byte msecVal = ThrottledMsec ();
   m_lastCommandTime = GetWorldTime ();

   (*g_engfuncs.pfnRunPlayerMove) (pev->pContainingEntity, m_moveAngles, m_moveSpeed, m_strafeSpeed, 0.0, pev->button, pev->impulse, msecVal);
}

void Bot::CheckBurstMode (float distance)
{
   // this function checks burst mode, and switch it depending distance to to enemy.

   if (HasShield ())
      return; // no checking when shiled is active

   // if current weapon is glock, disable burstmode on long distances, enable it else
   if (m_currentWeapon == WEAPON_GLOCK && distance < 300.0 && m_weaponBurstMode == BM_OFF)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_GLOCK && distance >= 300 && m_weaponBurstMode == BM_ON)
      pev->button |= IN_ATTACK2;

   // if current weapon is famas, disable burstmode on short distances, enable it else
   if (m_currentWeapon == WEAPON_FAMAS && distance > 400.0 && m_weaponBurstMode == BM_OFF)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_FAMAS && distance <= 400 && m_weaponBurstMode == BM_ON)
      pev->button |= IN_ATTACK2;
}

void Bot::CheckSilencer (void)
{
   if (((m_currentWeapon == WEAPON_USP && m_difficulty < 2) || m_currentWeapon == WEAPON_M4A1) && !HasShield())
   {
      int iRandomNum = (m_personality == PERSONALITY_RUSHER ? 35 : 65);

      // aggressive bots don't like the silencer
      if (Random.Long (1, 100) <= (m_currentWeapon == WEAPON_USP ? iRandomNum / 3 : iRandomNum))
      {
         if (pev->weaponanim > 6) // is the silencer not attached...
            pev->button |= IN_ATTACK2; // attach the silencer
      }
      else
      {
         if (pev->weaponanim <= 6) // is the silencer attached...
            pev->button |= IN_ATTACK2; // detach the silencer
      }
   }
}

float Bot::GetBombTimeleft (void)
{
   if (!g_bombPlanted)
      return 0.0;

   float timeLeft = ((g_timeBombPlanted + mp_c4timer.GetFloat ()) - GetWorldTime ());

   if (timeLeft < 0.0)
      return 0.0;

   return timeLeft;
}

float Bot::GetEstimatedReachTime (void)
{
   float estimatedTime = 2.0f; // time to reach next waypoint

   // calculate 'real' time that we need to get from one waypoint to another
   if (m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints && m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
   {
      float distance = (g_waypoint->GetPath (m_prevWptIndex[0])->origin - m_currentPath->origin).GetLength ();

      // caclulate estimated time
      if (pev->maxspeed <= 0.0)
         estimatedTime = 4.0 * distance / 240.0;
      else
         estimatedTime = 4.0 * distance / pev->maxspeed;

      // check for special waypoints, that can slowdown our movement
      if ((m_currentPath->flags & FLAG_CROUCH) || (m_currentPath->flags & FLAG_LADDER) || (pev->button & IN_DUCK))
         estimatedTime *= 2.0;

      // check for too low values
      if (estimatedTime < 1.0f)
         estimatedTime = 1.0f;

      // check for too high values
      if (estimatedTime > 5.0f)
         estimatedTime = 5.0f;
   }
   return estimatedTime;
}

bool Bot::OutOfBombTimer (void)
{
   if (m_currentWaypointIndex == -1 || ((g_mapType & MAP_DE) && (m_hasProgressBar || GetTaskId () == TASK_ESCAPEFROMBOMB)))
      return false; // if CT bot already start defusing, or already escaping, return false

   // calculate left time
   float timeLeft = GetBombTimeleft ();

   // if time left greater than 13, no need to do other checks
   if (timeLeft > 16)
      return false;

   Vector bombOrigin = g_waypoint->GetBombPosition ();

   // for terrorist, if timer is lower than eleven seconds, return true
   if (static_cast <int> (timeLeft) < 16 && m_team == TEAM_TF && (bombOrigin - pev->origin).GetLength () < 1000)
      return true;

   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (int i = 0; i < GetMaxClients (); i++)
   {
      Bot *bot = NULL; // temporaly pointer to bot

      // search players with defuse kit
      if ((bot = g_botManager->GetBot (i)) != NULL && GetTeam (bot->GetEntity ()) == TEAM_CF && bot->m_hasDefuser && (bombOrigin - bot->pev->origin).GetLength () < 500)
      {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   float reachTime = g_waypoint->GetTravelTime (pev->maxspeed, m_currentPath->origin, bombOrigin);

   // for counter-terrorist check alos is we have time to reach position plus average defuse time
   if ((timeLeft < reachTime + 6 && !m_hasDefuser && !hasTeammatesWithDefuserKit) || (timeLeft < reachTime + 2 && m_hasDefuser))
      return true;

   if (m_hasProgressBar && IsOnFloor () && ((m_hasDefuser ? 10.0 : 15.0) > GetBombTimeleft ()))
      return true;

   return false; // return false otherwise
}

void Bot::ReactOnSound (void)
{
   int ownIndex = GetIndex ();
   float ownSoundLast = 0.0;

   if (g_clients[ownIndex].timeSoundLasting > GetWorldTime ())
   {
      if (g_clients[ownIndex].maxTimeSoundLasting <= 0.0)
         g_clients[ownIndex].maxTimeSoundLasting = 0.5;

      ownSoundLast = (g_clients[ownIndex].hearingDistance * 0.2) * (g_clients[ownIndex].timeSoundLasting - GetWorldTime ()) / g_clients[ownIndex].maxTimeSoundLasting;
   }

   edict_t *player = NULL;

   float maxVolume = 0.0, volume = 0.0;
   int hearEnemyIndex = -1;

   // loop through all enemy clients to check for hearable stuff
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].ent == GetEntity () || g_clients[i].timeSoundLasting < GetWorldTime ())
         continue;

      float distance = (g_clients[i].soundPosition - pev->origin).GetLength ();
      float hearingDistance = g_clients[i].hearingDistance;

      if (distance > hearingDistance)
         continue;

      if (g_clients[i].maxTimeSoundLasting <= 0.0)
         g_clients[i].maxTimeSoundLasting = 0.5;

      if (distance <= 0.5 * hearingDistance)
         volume = hearingDistance * (g_clients[i].timeSoundLasting - GetWorldTime ()) / g_clients[i].maxTimeSoundLasting;
      else
         volume = 2.0 * hearingDistance * (1.0 - distance / hearingDistance) * (g_clients[i].timeSoundLasting - GetWorldTime ()) / g_clients[i].maxTimeSoundLasting;

      if (g_clients[hearEnemyIndex].team == m_team && yb_csdm_mode.GetInt () != 2)
         volume = 0.3 * volume;

      // we will care about the most hearable sound instead of the closest one - KWo
      if (volume < maxVolume)
         continue;

      maxVolume = volume;

      if (volume < ownSoundLast)
         continue;

      hearEnemyIndex = i;
   }

   if (hearEnemyIndex >= 0)
   {
      if (g_clients[hearEnemyIndex].team != m_team && yb_csdm_mode.GetInt () != 2)
         player = g_clients[hearEnemyIndex].ent;
   }

   // did the bot hear someone ?
   if (!IsEntityNull (player))
   {
      // change to best weapon if heard something
      if (!(m_states & STATE_SEEING_ENEMY) && m_seeEnemyTime + 2.5 < GetWorldTime () && IsOnFloor () && m_currentWeapon != WEAPON_C4 && m_currentWeapon != WEAPON_EXPLOSIVE && m_currentWeapon != WEAPON_SMOKE && m_currentWeapon != WEAPON_FLASHBANG && !yb_jasonmode.GetBool ())
         SelectBestWeapon ();

      m_heardSoundTime = GetWorldTime () + 5.0;
      m_states |= STATE_HEARING_ENEMY;

      if ((Random.Long (0, 100) < 25) && IsEntityNull (m_enemy) && IsEntityNull (m_lastEnemy) && m_seeEnemyTime + 7.0 < GetWorldTime ())
         ChatterMessage (Chatter_HeardEnemy);

      m_aimFlags |= AIM_LAST_ENEMY;

      // didn't bot already have an enemy ? take this one...
      if (m_lastEnemyOrigin == nullvec || m_lastEnemy == NULL)
      {
         m_lastEnemy = player;
         m_lastEnemyOrigin = player->v.origin;
      }
      else // bot had an enemy, check if it's the heard one
      {
         if (player == m_lastEnemy)
         {
            // bot sees enemy ? then bail out !
            if (m_states & STATE_SEEING_ENEMY)
               return;

            m_lastEnemyOrigin = player->v.origin;
         }
         else
         {
            // if bot had an enemy but the heard one is nearer, take it instead
            float distance = (m_lastEnemyOrigin - pev->origin).GetLengthSquared ();

            if (distance > (player->v.origin - pev->origin).GetLengthSquared () && m_seeEnemyTime + 2.0 < GetWorldTime ())
            {
               m_lastEnemy = player;
               m_lastEnemyOrigin = player->v.origin;
            }
            else
               return;
         }
      }
      extern ConVar yb_shoots_thru_walls;

      // check if heard enemy can be seen
      if (CheckVisibility (VARS (player), &m_lastEnemyOrigin, &m_visibility))
      {
         m_enemy = player;
         m_lastEnemy = player;
         m_enemyOrigin = m_lastEnemyOrigin;

         m_states |= STATE_SEEING_ENEMY;
         m_seeEnemyTime = GetWorldTime ();
      }
      else if (m_lastEnemy == player && m_seeEnemyTime + 1.0 > GetWorldTime () && yb_shoots_thru_walls.GetBool () && IsShootableThruObstacle (player->v.origin))
      {
         m_enemy = player;
         m_lastEnemy = player;
         m_lastEnemyOrigin = player->v.origin;

         m_states |= STATE_SEEING_ENEMY;
         m_seeEnemyTime = GetWorldTime ();
      }
   }
}

bool Bot::IsShootableBreakable (edict_t *ent)
{
   // this function is checking that pointed by ent pointer obstacle, can be destroyed.

   if (FClassnameIs (ent, "func_breakable") || (FClassnameIs (ent, "func_pushable") && (ent->v.spawnflags & SF_PUSH_BREAKABLE)))
      return (ent->v.takedamage != DAMAGE_NO) && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY) && ent->v.health < 500;

   return false;
}

void Bot::EquipInBuyzone (int buyCount)
{
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (m_lastEquipTime + 15.0 < GetWorldTime () && m_inBuyZone && g_timeRoundStart + Random.Float (10.0, 20.0) + mp_buytime.GetFloat () < GetWorldTime () && !g_bombPlanted && m_moneyAmount > g_botBuyEconomyTable[0])
   {
      m_buyingFinished = false;
      m_buyState = buyCount;

      // push buy message
      PushMessageQueue (GSM_BUY_STUFF);

      m_nextBuyTime = GetWorldTime ();
      m_lastEquipTime = GetWorldTime ();
   }
}

bool Bot::IsBombDefusing (Vector bombOrigin)
{
   // this function finds if somebody currently defusing the bomb.

   // @todo: need to check progress bar for non-bots clients.
   bool defusingInProgress = false;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      Bot *bot = g_botManager->GetBot (i);

      if (bot == NULL || bot == this)
         continue; // skip invalid bots

      if (m_team != GetTeam (bot->GetEntity ()) || bot->GetTaskId () == TASK_ESCAPEFROMBOMB)
         continue; // skip other mess

      if ((bot->pev->origin - bombOrigin).GetLength () < 140.0f && (bot->GetTaskId () == TASK_DEFUSEBOMB || bot->m_hasProgressBar))
      {
         defusingInProgress = true;
         break;
      }

      // take in account peoples too
      if (defusingInProgress || !(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || IsValidBot (g_clients[i].ent))
         continue;

      if ((g_clients[i].ent->v.origin - bombOrigin).GetLength () < 140.0f)
      {
         defusingInProgress = true;
         break;
      }
   }
   return defusingInProgress;
}
