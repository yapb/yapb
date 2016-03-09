//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_debug ("yb_debug", "0");
ConVar yb_debug_goal ("yb_debug_goal", "-1");
ConVar yb_user_follow_percent ("yb_user_follow_percent", "20");
ConVar yb_user_max_followers ("yb_user_max_followers", "1");

ConVar yb_jasonmode ("yb_jasonmode", "0");
ConVar yb_communication_type ("yb_communication_type", "2");
ConVar yb_economics_rounds ("yb_economics_rounds", "1");
ConVar yb_walking_allowed ("yb_walking_allowed", "1");
ConVar yb_camping_allowed ("yb_camping_allowed", "1");

ConVar yb_tkpunish ("yb_tkpunish", "1");
ConVar yb_freeze_bots ("yb_freeze_bots", "0");
ConVar yb_spraypaints ("yb_spraypaints", "1");
ConVar yb_botbuy ("yb_botbuy", "1");

ConVar yb_chatter_path ("yb_chatter_path", "sound/radio/bot");
ConVar yb_restricted_weapons ("yb_restricted_weapons", "");

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

      for (int i = 0; i < engine.MaxClients (); i++)
      {
         Bot *otherBot = bots.GetBot (i);

         if (otherBot != NULL && otherBot->pev != pev)
         {
            if (m_notKilled == IsAlive (otherBot->GetEntity ()))
            {
               otherBot->m_sayTextBuffer.entityIndex = entityIndex;
               strcpy (otherBot->m_sayTextBuffer.sayText, m_tempStrings);
            }
            otherBot->m_sayTextBuffer.timeNextChat = engine.Time () + otherBot->m_sayTextBuffer.chatDelay;
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

bool Bot::ItemIsVisible (const Vector &destination, char *itemName) 
{
   TraceResult tr;

   // trace a line from bot's eyes to destination..
   engine.TestLine (EyePosition (), destination, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction != 1.0f)
   {
      // check for standard items
      if (strcmp (STRING (tr.pHit->v.classname), itemName) == 0)
         return true;

      if (tr.flFraction > 0.98f && (yb_csdm_mode.GetBool () && strncmp (STRING (tr.pHit->v.classname), "csdmw_", 6) == 0))
         return true;

      return false;
   }
   return true;
}

bool Bot::EntityIsVisible (const Vector &dest, bool fromBody)
{
   TraceResult tr;

   // trace a line from bot's eyes to destination...
   engine.TestLine (fromBody ? pev->origin - Vector (0.0f, 0.0f, 1.0f) : EyePosition (), dest, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0f;
}

void Bot::CheckGrenadeThrow (void)
{
   // check if throwing a grenade is a good thing to do...
   if (m_lastEnemy == NULL || yb_ignore_enemies.GetBool () || yb_jasonmode.GetBool () || m_grenadeCheckTime > engine.Time () || m_isUsingGrenade || GetTaskId () == TASK_PLANTBOMB || GetTaskId () == TASK_DEFUSEBOMB || m_isReloading || !IsAlive (m_lastEnemy))
   {
      m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG);
      return;
   }

   // check again in some seconds
   m_grenadeCheckTime = engine.Time () + 0.5f;

   // check if we have grenades to throw
   int grenadeToThrow = CheckGrenades ();

   // if we don't have grenades no need to check it this round again
   if (grenadeToThrow == -1)
   {
      m_grenadeCheckTime = engine.Time () + 15.0f; // changed since, conzero can drop grens from dead players
      m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG);

      return;
   }
   // care about different types of grenades
   if ((grenadeToThrow == WEAPON_EXPLOSIVE || grenadeToThrow == WEAPON_SMOKE) && Random.Long (0, 100) < 45 && !(m_states & (STATE_SEEING_ENEMY | STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG)))
   {
      float distance = (m_lastEnemy->v.origin - pev->origin).GetLength ();

      // is enemy to high to throw
      if ((m_lastEnemy->v.origin.z > (pev->origin.z + 650.0)) || !(m_lastEnemy->v.flags & (FL_ONGROUND | FL_DUCKING)))
         distance = 99999.0f; // just some crazy value

      // enemy is within a good throwing distance ?               
      if (distance > (grenadeToThrow == WEAPON_SMOKE ? 400 : 600) && distance <= 1000)
      {
         // care about different types of grenades
         if (grenadeToThrow == WEAPON_EXPLOSIVE)
         {
            bool allowThrowing = true;

            // check for teammates
            if (GetNearbyFriendsNearPosition (m_lastEnemy->v.origin, 256.0f) > 0)
               allowThrowing = false;

            if (allowThrowing && m_seeEnemyTime + 2.0 < engine.Time ())
            {
               const Vector &enemyPredict = ((m_lastEnemy->v.velocity * 0.5f).Get2D () + m_lastEnemy->v.origin);
               float searchRadius = m_lastEnemy->v.velocity.GetLength2D ();

               // check the search radius
               if (searchRadius < 128.0f)
                  searchRadius = 128.0f;

               Array <int> predictedPoints;

               // search waypoints
               waypoints.FindInRadius (predictedPoints, searchRadius, enemyPredict, 5);

               FOR_EACH_AE (predictedPoints, it)
               {
                  allowThrowing = true;

                  // check the throwing
                  m_throw = waypoints.GetPath (predictedPoints[it])->origin;
                  Vector src = CheckThrow (EyePosition (), m_throw);

                  if (src.GetLengthSquared () < 100.0f)
                     src = CheckToss (EyePosition (), m_throw);

                  if (src.IsZero ())
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
            if ((m_states & STATE_SEEING_ENEMY) && GetShootingConeDeviation (m_enemy, &pev->origin) >= 0.70f && m_seeEnemyTime + 2.0f < engine.Time ())
               m_states &= ~STATE_THROW_SG;
            else
               m_states |= STATE_THROW_SG;
         }
      }
   }
   else if (IsAlive (m_lastEnemy) && grenadeToThrow == WEAPON_FLASHBANG && (m_lastEnemy->v.origin - pev->origin).GetLength () < 800.0f && !(m_aimFlags & AIM_ENEMY) && Random.Long (0, 100) < 50)
   {
      bool allowThrowing = true;
      Array <int> inRadius;

      waypoints.FindInRadius (inRadius, 256.0f, m_lastEnemy->v.origin + (m_lastEnemy->v.velocity * 0.5f).Get2D ());

      FOR_EACH_AE (inRadius, i)
      {
         Path *path = waypoints.GetPath (i);

         int friendCount = GetNearbyFriendsNearPosition (path->origin, 256.0f);

         if (friendCount != 0 || !(m_difficulty == 4 && friendCount != 0))
            continue;

         m_throw = path->origin;
         Vector src = CheckThrow (EyePosition (), m_throw);

         if (src.GetLengthSquared () < 100.0f)
            src = CheckToss (EyePosition (), m_throw);

         if (src.IsZero ())
            continue;

         allowThrowing = true;
         break;
      }

      // start concussion grenade throwing?
      if (allowThrowing  && m_seeEnemyTime + 2.0f < engine.Time ())
         m_states |= STATE_THROW_FB;
      else
         m_states &= ~STATE_THROW_FB;
   }

   if (m_states & STATE_THROW_HE)
      PushTask (TASK_THROWHEGRENADE, TASKPRI_THROWGRENADE, -1, engine.Time () + 3.0f, false);
   else if (m_states & STATE_THROW_FB)
      PushTask (TASK_THROWFLASHBANG, TASKPRI_THROWGRENADE, -1, engine.Time () + 3.0f, false);
   else if (m_states & STATE_THROW_SG)
      PushTask (TASK_THROWSMOKE, TASKPRI_THROWGRENADE, -1, engine.Time () + 3.0f, false);

   // delay next grenade throw
   if (m_states & (STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG))
   {
      m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;
      m_maxThrowTimer = engine.Time () + MAX_GRENADE_TIMER * 2.0f;
   }
}

void Bot::AvoidGrenades (void)
{
   // checks if bot 'sees' a grenade, and avoid it

   if (!bots.HasActiveGrenades ())
      return;

  // check if old pointers to grenade is invalid
   if (engine.IsNullEntity (m_avoidGrenade))
   {
      m_avoidGrenade = NULL;
      m_needAvoidGrenade = 0;
   }
   else if ((m_avoidGrenade->v.flags & FL_ONGROUND) || (m_avoidGrenade->v.effects & EF_NODRAW))
   {
      m_avoidGrenade = NULL;
      m_needAvoidGrenade = 0;
   }
   Array <entity_t> activeGrenades = bots.GetActiveGrenades ();

   // find all grenades on the map
   FOR_EACH_AE (activeGrenades, it)
   {
      edict_t *ent = activeGrenades[it];

      if (ent->v.effects & EF_NODRAW)
         continue;

      // check if visible to the bot
      if (!EntityIsVisible (ent->v.origin) && InFieldOfView (ent->v.origin - EyePosition ()) > pev->fov * 0.5f)
         continue;

      if (m_turnAwayFromFlashbang < engine.Time () && m_personality == PERSONALITY_RUSHER && m_difficulty == 4 && strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0)
      {
         // don't look at flash bang
         if (!(m_states & STATE_SEEING_ENEMY))
         {
            pev->v_angle.y = AngleNormalize ((engine.GetAbsOrigin (ent) - EyePosition ()).ToAngles ().y + 180.0f);

            m_canChooseAimDirection = false;
            m_turnAwayFromFlashbang = engine.Time () + Random.Float (1.0f, 2.0f);
         }
      }
      else if (strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0)
      {
         if (!engine.IsNullEntity (m_avoidGrenade))
            return;

         if (engine.GetTeam (ent->v.owner) == m_team && ent->v.owner != GetEntity ())
            return;

         if ((ent->v.flags & FL_ONGROUND) == 0)
         {
            float distance = (ent->v.origin - pev->origin).GetLength ();
            float distanceMoved = ((ent->v.origin + ent->v.velocity * m_frameInterval) - pev->origin).GetLength ();

            if (distanceMoved < distance && distance < 500.0f)
            {
               MakeVectors (pev->v_angle);

               const Vector &dirToPoint = (pev->origin - ent->v.origin).Normalize2D ();
               const Vector &rightSide = g_pGlobals->v_right.Normalize2D ();

               if ((dirToPoint | rightSide) > 0.0f)
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
   if (!bots.HasActiveGrenades ())
      return false;

   const Vector &betweenUs = (ent->v.origin - pev->origin).Normalize ();
   Array <entity_t> activeGrenades = bots.GetActiveGrenades ();

   // find all grenades on the map
   FOR_EACH_AE (activeGrenades, it)
   {
      edict_t *grenade = activeGrenades[it];

      if (grenade->v.effects & EF_NODRAW)
         continue;

      // if grenade is invisible don't care for it
      if (!(grenade->v.flags & (FL_ONGROUND | FL_PARTIALGROUND)) || strcmp (STRING (grenade->v.model) + 9, "smokegrenade.mdl"))
         continue;

      // check if visible to the bot
      if (!EntityIsVisible (ent->v.origin) && InFieldOfView (ent->v.origin - EyePosition ()) > pev->fov / 3.0f)
         continue;

      const Vector &entityOrigin = engine.GetAbsOrigin (grenade);
      const Vector &betweenNade = (entityOrigin - pev->origin).Normalize ();
      const Vector &betweenResult = ((betweenNade.Get2D () * 150.0f + entityOrigin) - pev->origin).Normalize ();

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
   if (!IsShootableBreakable (touch))
      return;

   m_breakableEntity = FindBreakable ();

   if (engine.IsNullEntity (m_breakableEntity))
      return;

   m_campButtons = pev->button & IN_DUCK;

   PushTask (TASK_SHOOTBREAKABLE, TASKPRI_SHOOTBREAKABLE, -1, 0.0f, false);
}

edict_t *Bot::FindBreakable (void)
{
   // this function checks if bot is blocked by a shoot able breakable in his moving direction

   TraceResult tr;
   engine.TestLine (pev->origin, pev->origin + (m_destOrigin - pev->origin).Normalize () * 64.0f, TRACE_IGNORE_NONE, GetEntity (), &tr);

   if (tr.flFraction != 1.0f)
   {
      edict_t *ent = tr.pHit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (IsShootableBreakable (ent))
      {
         m_breakable = tr.vecEndPos;
         return ent;
      }
   }
   engine.TestLine (EyePosition (), EyePosition () + (m_destOrigin - EyePosition ()).Normalize () * 64.0f, TRACE_IGNORE_NONE, GetEntity (), &tr);

   if (tr.flFraction != 1.0f)
   {
      edict_t *ent = tr.pHit;

      if (IsShootableBreakable (ent))
      {
         m_breakable = tr.vecEndPos;
         return ent;
      }
   }
   m_breakableEntity = NULL;
   m_breakable.Zero ();

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
   if (IsOnLadder () || GetTaskId () == TASK_ESCAPEFROMBOMB || yb_jasonmode.GetBool ())
   {
      m_pickupItem = NULL;
      m_pickupType = PICKUP_NONE;

      return;
   }

   edict_t *ent = NULL, *pickupItem = NULL;
   Bot *bot = NULL;

   float distance, minDistance = 341.0f;

   const float searchRadius = 340.0f;

   if (!engine.IsNullEntity (m_pickupItem))
   {
      bool itemExists = false;
      pickupItem = m_pickupItem;

      while (!engine.IsNullEntity (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, searchRadius)))
      {
         if ((ent->v.effects & EF_NODRAW) || IsValidPlayer (ent->v.owner))
            continue; // someone owns this weapon or it hasn't re spawned yet

         if (ent == pickupItem)
         {
            if (ItemIsVisible (engine.GetAbsOrigin (ent), const_cast <char *> (STRING (ent->v.classname))))
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

   Vector pickupOrigin;
   Vector entityOrigin;

   m_pickupItem = NULL;
   m_pickupType = PICKUP_NONE;

   while (!engine.IsNullEntity (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, searchRadius)))
   {
      bool allowPickup = false;  // assume can't use it until known otherwise

      if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore)
         continue; // someone owns this weapon or it hasn't respawned yet

      entityOrigin = engine.GetAbsOrigin (ent);

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
         else if (strncmp ("item_thighpack", STRING (ent->v.classname), 14) == 0 && m_team == CT && !m_hasDefuser)
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
               else if (strcmp (STRING (ent->v.model) + 9, "medkit.mdl") == 0 && pev->health >= 100.0f)
                  allowPickup = false;
               else if ((strcmp (STRING (ent->v.model) + 9, "kevlar.mdl") == 0 || strcmp (STRING (ent->v.model) + 9, "battery.mdl") == 0) && pev->armorvalue >= 100.0f) // armor vest
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
            else if (m_team == TERRORIST) // terrorist team specific
            {
               if (pickupType == PICKUP_DROPPED_C4)
               {
                  allowPickup = true;
                  m_destOrigin = entityOrigin; // ensure we reached dropped bomb

                  ChatterMessage (Chatter_FoundC4); // play info about that
                  DeleteSearchNodes ();
               }
               else if (pickupType == PICKUP_HOSTAGE)
               {
                  m_itemIgnore = ent;
                  allowPickup = false;

                  if (!m_defendHostage && m_difficulty >= 3 && Random.Long (0, 100) < 30 && m_timeCamping + 15.0f < engine.Time ())
                  {
                     int index = FindDefendWaypoint (entityOrigin);

                     PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (30.0f, 60.0f), true); // push camp task on to stack
                     PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + Random.Float (3.0f, 6.0f), true); // push move command

                     if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
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
                     Path *path = waypoints.GetPath (index);

                     float bombTimer = mp_c4timer.GetFloat ();
                     float timeMidBlowup = g_timeBombPlanted + (bombTimer * 0.5f + bombTimer * 0.25f) - waypoints.GetTravelTime (pev->maxspeed, pev->origin, path->origin);

                     if (timeMidBlowup > engine.Time ())
                     {
                        RemoveCertainTask (TASK_MOVETOPOSITION); // remove any move tasks

                        PushTask (TASK_CAMP, TASKPRI_CAMP, -1, timeMidBlowup, true); // push camp task on to stack
                        PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeMidBlowup, true); // push  move command

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
            else if (m_team == CT)
            {
               if (pickupType == PICKUP_HOSTAGE)
               {
                  if (engine.IsNullEntity (ent) || ent->v.health <= 0)
                     allowPickup = false; // never pickup dead hostage
                  else for (int i = 0; i < engine.MaxClients (); i++)
                  {
                     if ((bot = bots.GetBot (i)) != NULL && bot->m_notKilled)
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
                  if (IsValidPlayer (m_enemy))
                  {
                     allowPickup = false;
                     return;
                  }

                  if (Random.Long (0, 100) < 90)
                     ChatterMessage (Chatter_FoundBombPlace);

                  allowPickup = !IsBombDefusing (waypoints.GetBombPosition ()) || m_hasProgressBar;
                  pickupType = PICKUP_PLANTED_C4;

                  if (!m_defendedBomb && !allowPickup)
                  {
                     m_defendedBomb = true;

                     int index = FindDefendWaypoint (entityOrigin);
                     Path *path = waypoints.GetPath (index);

                     float timeToExplode = g_timeBombPlanted + mp_c4timer.GetFloat () - waypoints.GetTravelTime (pev->maxspeed, pev->origin, path->origin);
        
                     RemoveCertainTask (TASK_MOVETOPOSITION); // remove any move tasks

                     PushTask (TASK_CAMP, TASKPRI_CAMP, -1, timeToExplode, true); // push camp task on to stack
                     PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeToExplode, true); // push move command

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

                     PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (30.0f, 70.0f), true); // push camp task on to stack
                     PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + Random.Float (10.0f, 30.0f), true); // push move command

                     if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
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

   if (!engine.IsNullEntity (pickupItem))
   {
      for (int i = 0; i < engine.MaxClients (); i++)
      {
         if ((bot = bots.GetBot (i)) != NULL && bot->m_notKilled && bot->m_pickupItem == pickupItem)
         {
            m_pickupItem = NULL;
            m_pickupType = PICKUP_NONE;

            return;
         }
      }

      if (pickupOrigin.z > EyePosition ().z + (m_pickupType == PICKUP_HOSTAGE ? 40.0f : 15.0f) || IsDeadlyDrop (pickupOrigin)) // check if item is too high to reach, check if getting the item would hurt bot
      {
         m_itemIgnore = m_pickupItem;
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
   const Vector &src = EyePosition ();

   engine.TestLine (src, *dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
   {
      float length = (tr.vecEndPos - src).GetLengthSquared ();

      if (length > 10000.0f)
         return;

      float minDistance = 99999.0f;
      float maxDistance = 99999.0f;

      int enemyIndex = -1, tempIndex = -1;

      // find nearest waypoint to bot and position
      for (int i = 0; i < g_numWaypoints; i++)
      {
         float distance = (waypoints.GetPath (i)->origin - pev->origin).GetLengthSquared ();

         if (distance < minDistance)
         {
            minDistance = distance;
            tempIndex = i;
         }
         distance = (waypoints.GetPath (i)->origin - *dest).GetLengthSquared ();

         if (distance < maxDistance)
         {
            maxDistance = distance;
            enemyIndex = i;
         }
      }

      if (tempIndex == -1 || enemyIndex == -1)
         return;

      minDistance = 99999.0f;

      int lookAtWaypoint = -1;
      Path *path = waypoints.GetPath (tempIndex);

      for (int i = 0; i < MAX_PATH_INDEX; i++)
      {
         if (path->index[i] == -1)
            continue;

         float distance = static_cast <float> (waypoints.GetPathDistance (path->index[i], enemyIndex));

         if (distance < minDistance)
         {
            minDistance = distance;
            lookAtWaypoint = path->index[i];
         }
      }
      if (lookAtWaypoint != -1 && lookAtWaypoint < g_numWaypoints)
         *dest = waypoints.GetPath (lookAtWaypoint)->origin;
   }
}

void Bot::SwitchChatterIcon (bool show)
{
   // this function depending on show boolen, shows/remove chatter, icon, on the head of bot.

   if ((g_gameFlags & GAME_LEGACY) || yb_communication_type.GetInt () != 2)
      return;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || (g_clients[i].ent->v.flags & FL_FAKECLIENT) || g_clients[i].team != m_team)
         continue;

      MESSAGE_BEGIN (MSG_ONE, netmsg.GetId (NETMSG_BOTVOICE), NULL, g_clients[i].ent); // begin message
         WRITE_BYTE (show); // switch on/off
         WRITE_BYTE (GetIndex ());
      MESSAGE_END ();
   }
}

void Bot::InstantChatterMessage (int type)
{
   // this function sends instant chatter messages.

   if (yb_communication_type.GetInt () != 2 || g_chatterFactory[type].IsEmpty () || (g_gameFlags & GAME_LEGACY) || !g_sendAudioFinished)
      return;

   if (m_notKilled)
      SwitchChatterIcon (true);

   // delay only reportteam
   if (type == Radio_ReportTeam && m_timeRepotingInDelay < engine.Time ())
   {
      if (m_timeRepotingInDelay < engine.Time ())
         return;

      m_timeRepotingInDelay = engine.Time () + Random.Float (30.0f, 60.0f);
   }

   String defaultSound = g_chatterFactory[type].GetRandomElement ().name;
   String painSound = g_chatterFactory[Chatter_DiePain].GetRandomElement ().name;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      edict_t *ent = engine.EntityOfIndex (i);

      if (!IsValidPlayer (ent) || IsValidBot (ent) || engine.GetTeam (ent) != m_team)
         continue;

      g_sendAudioFinished = false;

      MESSAGE_BEGIN (MSG_ONE, netmsg.GetId (NETMSG_SENDAUDIO), NULL, ent); // begin message
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

   if (yb_communication_type.GetInt () == 0 || m_numFriendsLeft == 0)
      return;

   if (g_chatterFactory[message].IsEmpty () || (g_gameFlags & GAME_LEGACY) || yb_communication_type.GetInt () != 2)
      g_radioInsteadVoice = true; // use radio instead voice

   m_radioSelect = message;
   PushMessageQueue (GSM_RADIO);
}

void Bot::ChatterMessage (int message)
{
   // this function inserts the voice message into the message queue (mostly same as above)

   if ((g_gameFlags & GAME_LEGACY) || yb_communication_type.GetInt () != 2 || g_chatterFactory[message].IsEmpty () || m_numFriendsLeft == 0)
      return;

   bool shouldExecute = false;

   if (m_chatterTimes[message] < engine.Time () || m_chatterTimes[message] == 99999.0f)
   {
      if (m_chatterTimes[message] != 99999.0f)
         m_chatterTimes[message] = engine.Time () + g_chatterFactory[message][0].repeatTime;

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
      if (m_nextBuyTime > engine.Time ())
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
      m_nextBuyTime = engine.Time () + Random.Float (0.5f, 1.3f);

      // if bot buying is off then no need to buy
      if (!yb_botbuy.GetBool ())
         m_buyState = BUYSTATE_FINISHED;

      // if fun-mode no need to buy
      if (yb_jasonmode.GetBool ())
      {
         m_buyState = BUYSTATE_FINISHED;
         SelectWeaponByName ("weapon_knife");
      }

      // prevent vip from buying
      if (IsPlayerVIP (GetEntity ()))
      {
         m_isVIP = true;
         m_buyState = BUYSTATE_FINISHED;
         m_pathType = SEARCH_PATH_FASTEST;
      }

      // prevent terrorists from buying on es maps
      if ((g_mapType & MAP_ES) && m_team == TERRORIST)
         m_buyState = 6;

      // prevent teams from buying on fun maps
      if (g_mapType & (MAP_KA | MAP_FY))
      {
         m_buyState = BUYSTATE_FINISHED;

         if (g_mapType & MAP_KA)
            yb_jasonmode.SetInt (1);
      }

      if (m_buyState > BUYSTATE_FINISHED - 1)
      {
         m_buyingFinished = true;
         return;
      }

      PushMessageQueue (GSM_IDLE);
      PurchaseWeapons ();

      break;

   case GSM_RADIO: // general radio message issued
     // if last bot radio command (global) happened just a second ago, delay response
      if (g_lastRadioTime[m_team] + 1.0f < engine.Time ())
      {
         // if same message like previous just do a yes/no
         if (m_radioSelect != Radio_Affirmative && m_radioSelect != Radio_Negative)
         {
            if (m_radioSelect == g_lastRadio[m_team] && g_lastRadioTime[m_team] + 1.5f > engine.Time ())
               m_radioSelect = -1;
            else
            {
               if (m_radioSelect != Radio_ReportingIn)
                  g_lastRadio[m_team] = m_radioSelect;
               else
                  g_lastRadio[m_team] = -1;

               for (int i = 0; i < engine.MaxClients (); i++)
               {
                  Bot *bot = bots.GetBot (i);

                  if (bot != NULL)
                  {
                     if (pev != bot->pev && bot->m_team == m_team)
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
                  Path *path = waypoints.GetPath (GetTask ()->data);

                  if (path->flags & FLAG_GOAL)
                  {
                     if ((g_mapType & MAP_DE) && m_team == TERRORIST && m_hasC4)
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

                  if (g_bombPlanted && m_team == TERRORIST)
                     InstantChatterMessage (Chatter_GuardDroppedC4);
                  else if (m_inVIPZone && m_team == TERRORIST)
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

         if ((m_radioSelect != Radio_ReportingIn && g_radioInsteadVoice) || yb_communication_type.GetInt () != 2 || g_chatterFactory[m_radioSelect].IsEmpty () || (g_gameFlags & GAME_LEGACY))
         {
            if (m_radioSelect < Radio_GoGoGo)
               engine.IssueBotCommand (GetEntity (), "radio1");
            else if (m_radioSelect < Radio_Affirmative)
            {
               m_radioSelect -= Radio_GoGoGo - 1;
               engine.IssueBotCommand (GetEntity (), "radio2");
            }
            else
            {
               m_radioSelect -= Radio_Affirmative - 1;
               engine.IssueBotCommand (GetEntity (), "radio3");
            }

            // select correct menu item for this radio message
            engine.IssueBotCommand (GetEntity (), "menuselect %d", m_radioSelect);
         }
         else if (m_radioSelect != -1 && m_radioSelect != Radio_ReportingIn)
            InstantChatterMessage (m_radioSelect);

         g_radioInsteadVoice = false; // reset radio to voice
         g_lastRadioTime[m_team] = engine.Time (); // store last radio usage
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

   default:
      return;
   }
}

bool Bot::IsRestricted (int weaponIndex)
{
   // this function checks for weapon restrictions.

   if (IsNullString (yb_restricted_weapons.GetString ()))
      return IsRestrictedAMX (weaponIndex); // no banned weapons

   Array <String> bannedWeapons = String (yb_restricted_weapons.GetString ()).Split (';');

   FOR_EACH_AE (bannedWeapons, i)
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
      FOR_EACH_AE (bannedWeapons, i)
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

int Bot::PickBestFromRandom(int *random, int count, int moneySave)
{
   // this function taken from gina bot, it's picks best available weapon from random choice

   float buyFactor = (m_moneyAmount - static_cast <float> (moneySave)) / (16000.0f - static_cast <float> (moneySave)) * 3.0f;

   if (buyFactor < 1.0f)
      buyFactor = 1.0f;

   return random[static_cast <int> (static_cast <float> (count - 1) * log10f (Random.Float (1, powf (10.0f, buyFactor))) / buyFactor + 0.5f)];
}

void Bot::PurchaseWeapons (void)
{
   // this function does all the work in selecting correct buy menus for most weapons/items

   WeaponSelect *selectedWeapon = NULL;
   m_nextBuyTime = engine.Time () + Random.Float (0.3f, 0.5f);

   int count = 0, foundWeapons = 0;
   int choices[NUM_WEAPONS];

   // select the priority tab for this personality
   int *ptr = g_weaponPrefs[m_personality] + NUM_WEAPONS;

   bool isPistolMode = g_weaponSelect[25].teamStandard == -1 && g_weaponSelect[3].teamStandard == 2;
   bool teamEcoValid = bots.EconomicsValid (m_team);

   // do this, because xash engine is not capable to run all the features goldsrc, but we have cs 1.6 on it, so buy table must be the same
   bool isOldGame = (g_gameFlags & GAME_LEGACY) && !(g_gameFlags & GAME_XASH);

   switch (m_buyState)
   {
   case BUYSTATE_PRIMARY_WEAPON: // if no primary weapon and bot has some money, buy a primary weapon
      if ((!HasShield () && !HasPrimaryWeapon () && teamEcoValid) || (teamEcoValid && IsMorePowerfulWeaponCanBeBought ()))
      {
         int moneySave = 0;

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
            if (isOldGame && selectedWeapon->buySelect == -1)
               continue;

            // ignore weapon if this weapon is not targeted to out team....
            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team)
               continue;

            // ignore weapon if this weapon is restricted
            if (IsRestricted (selectedWeapon->id))
               continue;

            int *limit = g_botBuyEconomyTable;
            int prostock = 0;

            // filter out weapons with bot economics
            switch (m_personality)
            {
            case PERSONALITY_RUSHER:
               prostock = limit[ECO_PROSTOCK_RUSHER];
               break;

            case PERSONALITY_CAREFUL:
               prostock = limit[ECO_PROSTOCK_CAREFUL];
               break;

            case PERSONALITY_NORMAL:
               prostock = limit[ECO_PROSTOCK_NORMAL];
               break;
            }

            if (m_team == CT)
            {
               switch (selectedWeapon->id)
               {
               case WEAPON_TMP:
               case WEAPON_UMP45:
               case WEAPON_P90:
               case WEAPON_MP5:
                  if (m_moneyAmount > limit[ECO_SMG_GT_CT] + prostock)
                     ignoreWeapon = true;
                  break;
               }

               if (selectedWeapon->id == WEAPON_SHIELD && m_moneyAmount > limit[ECO_SHIELDGUN_GT])
                  ignoreWeapon = true;
            }
            else if (m_team == TERRORIST)
            {
               switch (selectedWeapon->id)
               {
               case WEAPON_UMP45:
               case WEAPON_MAC10:
               case WEAPON_P90:
               case WEAPON_MP5:
               case WEAPON_SCOUT:
                  if (m_moneyAmount > limit[ECO_SMG_GT_TE] + prostock)
                     ignoreWeapon = true;
                  break;
               }
            }

            switch (selectedWeapon->id)
            {
            case WEAPON_XM1014:
            case WEAPON_M3:
               if (m_moneyAmount < limit[ECO_SHOTGUN_LT])
                  ignoreWeapon = true;

               if (m_moneyAmount >= limit[ECO_SHOTGUN_GT])
                  ignoreWeapon = false;

               break;
            }

            switch (selectedWeapon->id)
            {
            case WEAPON_SG550:
            case WEAPON_G3SG1:
            case WEAPON_AWP:  
            case WEAPON_M249:
               if (m_moneyAmount < limit[ECO_HEAVY_LT])
                  ignoreWeapon = true;

               if (m_moneyAmount >= limit[ECO_HEAVY_GT])
                  ignoreWeapon = false;

               break;
            }

            if (ignoreWeapon && g_weaponSelect[25].teamStandard == 1 && yb_economics_rounds.GetBool ())
               continue;

            // save money for grenade for example?
            moneySave = Random.Long (300, 600);

            if (bots.GetLastWinner () == m_team)
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
               chosenWeapon = PickBestFromRandom (choices, foundWeapons, moneySave);
            else
               chosenWeapon = choices[foundWeapons - 1];

            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else
            selectedWeapon = NULL;

         if (selectedWeapon != NULL)
         {
            engine.IssueBotCommand (GetEntity (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame)
               engine.IssueBotCommand(GetEntity (), "menuselect %d", selectedWeapon->buySelect);
            else // SteamCS buy menu is different from the old one
            {
               if (m_team == TERRORIST)
                  engine.IssueBotCommand(GetEntity (), "menuselect %d", selectedWeapon->newBuySelectT);
               else
                  engine.IssueBotCommand (GetEntity (), "menuselect %d", selectedWeapon->newBuySelectCT);
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

   case BUYSTATE_ARMOR_VESTHELM: // if armor is damaged and bot has some money, buy some armor
      if (pev->armorvalue < Random.Long (50, 80) && (isPistolMode || (teamEcoValid && HasPrimaryWeapon ())))
      {
         // if bot is rich, buy kevlar + helmet, else buy a single kevlar
         if (m_moneyAmount > 1500 && !IsRestricted (WEAPON_ARMORHELM))
            engine.IssueBotCommand (GetEntity (), "buyequip;menuselect 2");
         else if (!IsRestricted (WEAPON_ARMOR))
            engine.IssueBotCommand (GetEntity (), "buyequip;menuselect 1");
      }
      break;

   case BUYSTATE_SECONDARY_WEAPON: // if bot has still some money, buy a better secondary weapon
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

            if (isOldGame && selectedWeapon->buySelect == -1)
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
               chosenWeapon = PickBestFromRandom (choices, foundWeapons, Random.Long (100, 200));
            else
               chosenWeapon = choices[foundWeapons - 1];

            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else
            selectedWeapon = NULL;

         if (selectedWeapon != NULL)
         {
            engine.IssueBotCommand (GetEntity (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame)
               engine.IssueBotCommand (GetEntity (), "menuselect %d", selectedWeapon->buySelect);

            else // steam cs buy menu is different from old one
            {
               if (engine.GetTeam (GetEntity ()) == TERRORIST)
                  engine.IssueBotCommand (GetEntity (), "menuselect %d", selectedWeapon->newBuySelectT);
               else
                  engine.IssueBotCommand (GetEntity (), "menuselect %d", selectedWeapon->newBuySelectCT);
            }
         }
      }
      break;

   case BUYSTATE_GRENADES: // if bot has still some money, choose if bot should buy a grenade or not
      if (Random.Long (1, 100) < g_grenadeBuyPrecent[0] && m_moneyAmount >= 400 && !IsRestricted (WEAPON_EXPLOSIVE))
      {
         // buy a he grenade
         engine.IssueBotCommand (GetEntity (), "buyequip");
         engine.IssueBotCommand (GetEntity (), "menuselect 4");
      }

      if (Random.Long (1, 100) < g_grenadeBuyPrecent[1] && m_moneyAmount >= 300 && teamEcoValid && !IsRestricted (WEAPON_FLASHBANG))
      {
         // buy a concussion grenade, i.e., 'flashbang'
         engine.IssueBotCommand (GetEntity (), "buyequip");
         engine.IssueBotCommand (GetEntity (), "menuselect 3");
      }

      if (Random.Long (1, 100) < g_grenadeBuyPrecent[2] && m_moneyAmount >= 400 && teamEcoValid && !IsRestricted (WEAPON_SMOKE))
      {
         // buy a smoke grenade
         engine.IssueBotCommand (GetEntity (), "buyequip");
         engine.IssueBotCommand (GetEntity (), "menuselect 5");
      }
      break;

   case BUYSTATE_DEFUSER: // if bot is CT and we're on a bomb map, randomly buy the defuse kit
      if ((g_mapType & MAP_DE) && m_team == CT && Random.Long (1, 100) < 80 && m_moneyAmount > 200 && !IsRestricted (WEAPON_DEFUSER))
      {
         if (isOldGame)
            engine.IssueBotCommand (GetEntity (), "buyequip;menuselect 6");
         else
            engine.IssueBotCommand (GetEntity (), "defuser"); // use alias in SteamCS
      }
      break;

   case BUYSTATE_AMMO: // buy enough primary & secondary ammo (do not check for money here)
      for (int i = 0; i <= 5; i++)
         engine.IssueBotCommand (GetEntity (), "buyammo%d", Random.Long (1, 2)); // simulate human

      // buy enough secondary ammo
      if (HasPrimaryWeapon ())
         engine.IssueBotCommand (GetEntity (), "buy;menuselect 7");

      // buy enough primary ammo
      engine.IssueBotCommand (GetEntity (), "buy;menuselect 6");

      // try to reload secondary weapon
      if (m_reloadState != RELOAD_PRIMARY)
         m_reloadState = RELOAD_SECONDARY;

      break;
   }

   m_buyState++;
   PushMessageQueue (GSM_BUY_STUFF);
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

void Bot::UpdateEmotions (void)
{
   // slowly increase/decrease dynamic emotions back to their base level
   if (m_nextEmotionUpdate > engine.Time ())
      return;

   if (m_agressionLevel > m_baseAgressionLevel)
      m_agressionLevel -= 0.10f;
   else
      m_agressionLevel += 0.10f;

   if (m_fearLevel > m_baseFearLevel)
      m_fearLevel -= 0.05f;
   else
      m_fearLevel += 0.05f;

   if (m_agressionLevel < 0.0f)
      m_agressionLevel = 0.0f;

   if (m_fearLevel < 0.0f)
      m_fearLevel = 0.0f;

   m_nextEmotionUpdate = engine.Time () + 1.0f;
}

void Bot::SetConditionsOverride (void)
{
   if (m_currentWeapon != WEAPON_KNIFE && m_difficulty > 3 && ((m_aimFlags & AIM_ENEMY) || (m_states & (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY)) || (GetTaskId () == TASK_SEEKCOVER && (m_isReloading || m_isVIP))) && !yb_jasonmode.GetBool () && GetTaskId () != TASK_CAMP && !IsOnLadder ())
   {
      m_moveToGoal = false; // don't move to goal
      m_navTimeset = engine.Time ();

      if (IsValidPlayer (m_enemy))
         CombatFight ();
   }

   // check if we need to escape from bomb
   if ((g_mapType & MAP_DE) && g_bombPlanted && m_notKilled && GetTaskId () != TASK_ESCAPEFROMBOMB && GetTaskId () != TASK_CAMP && OutOfBombTimer ())
   {
      TaskComplete (); // complete current task

      // then start escape from bomb immidiate
      PushTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0f, true);
   }

   // special handling, if we have a knife in our hands
   if (m_currentWeapon == WEAPON_KNIFE && IsValidPlayer (m_enemy) && (GetTaskId () != TASK_MOVETOPOSITION || GetTask ()->desire != TASKPRI_HIDE))
   {
      float length = (pev->origin - m_enemy->v.origin).GetLength2D ();

      // do waypoint movement if enemy is not reacheable with a knife
      if (length > 100.0f && (m_states & STATE_SEEING_ENEMY))
      {
         int nearestToEnemyPoint = waypoints.FindNearest (m_enemy->v.origin);

         if (nearestToEnemyPoint != -1 && nearestToEnemyPoint != m_currentWaypointIndex && fabsf (waypoints.GetPath (nearestToEnemyPoint)->origin.z - m_enemy->v.origin.z) < 16.0f)
         {
            PushTask (TASK_MOVETOPOSITION, TASKPRI_HIDE, nearestToEnemyPoint, engine.Time () + Random.Float (5.0f, 10.0f), true);

            m_isEnemyReachable = false;
            m_enemy = NULL;

            m_enemyIgnoreTimer = engine.Time () + ((length / pev->maxspeed) * 0.5f);
         }
      }
   }
}

void Bot::SetConditions (void)
{
   // this function carried out each frame. does all of the sensing, calculates emotions and finally sets the desired
   // action after applying all of the Filters

   m_aimFlags = 0;

   UpdateEmotions ();

   // does bot see an enemy?
   if (LookupEnemy ())
      m_states |= STATE_SEEING_ENEMY;
   else
   {
      m_states &= ~STATE_SEEING_ENEMY;
      m_enemy = NULL;
   }

   // did bot just kill an enemy?
   if (!engine.IsNullEntity (m_lastVictim))
   {
      if (engine.GetTeam (m_lastVictim) != m_team)
      {
         // add some aggression because we just killed somebody
         m_agressionLevel += 0.1f;

         if (m_agressionLevel > 1.0f)
            m_agressionLevel = 1.0f;

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
               switch (GetNearbyEnemiesNearPosition (pev->origin, 99999.0f))
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
         if (m_team == CT && m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0 && g_bombPlanted)
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
   if (!engine.IsNullEntity (m_lastEnemy))
   {
      if (!IsAlive (m_lastEnemy) && m_shootAtDeadTime < engine.Time ())
      {
         m_lastEnemyOrigin.Zero ();
         m_lastEnemy = NULL;
      }
   }
   else
   {
      m_lastEnemyOrigin.Zero ();
      m_lastEnemy = NULL;
   }

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (!yb_ignore_enemies.GetBool () && m_soundUpdateTime < engine.Time () && m_blindTime < engine.Time () && m_seeEnemyTime + 1.0f < engine.Time ())
   {
      ReactOnSound ();
      m_soundUpdateTime = engine.Time () + 0.25f;
   }
   else if (m_heardSoundTime < engine.Time ())
      m_states &= ~STATE_HEARING_ENEMY;

   if (engine.IsNullEntity (m_enemy) && !engine.IsNullEntity (m_lastEnemy) && !m_lastEnemyOrigin.IsZero ())
   {
      m_aimFlags |= AIM_PREDICT_PATH;

      if (EntityIsVisible (m_lastEnemyOrigin))
         m_aimFlags |= AIM_LAST_ENEMY;
   }
   CheckGrenadeThrow ();

   // check if there are items needing to be used/collected
   if (m_itemCheckTime < engine.Time () || !engine.IsNullEntity (m_pickupItem))
   {
      m_itemCheckTime = engine.Time () + 0.4f;
      FindItem ();
   }
   ApplyTaskFilters ();
}

void Bot::ApplyTaskFilters (void)
{
   // initialize & calculate the desire for all actions based on distances, emotions and other stuff
   GetTask ();

   float tempFear = m_fearLevel;
   float tempAgression = m_agressionLevel;

   // decrease fear if teammates near
   int friendlyNum = 0;

   if (!m_lastEnemyOrigin.IsZero ())
      friendlyNum = GetNearbyFriendsNearPosition (pev->origin, 500.0f) - GetNearbyEnemiesNearPosition (m_lastEnemyOrigin, 500.0f);

   if (friendlyNum > 0)
      tempFear = tempFear * 0.5f;

   // increase/decrease fear/aggression if bot uses a sniping weapon to be more careful
   if (UsesSniper ())
   {
      tempFear = tempFear * 1.5f;
      tempAgression = tempAgression * 0.5f;
   }

   // bot found some item to use?
   if (!engine.IsNullEntity (m_pickupItem) && GetTaskId () != TASK_ESCAPEFROMBOMB)
   {
      m_states |= STATE_PICKUP_ITEM;

      if (m_pickupType == PICKUP_BUTTON)
         g_taskFilters[TASK_PICKUPITEM].desire = 50.0f; // always pickup button
      else
      {
         float distance = (500.0f - (engine.GetAbsOrigin (m_pickupItem) - pev->origin).GetLength ()) * 0.2f;

         if (distance > 50.0f)
            distance = 50.0f;

         g_taskFilters[TASK_PICKUPITEM].desire = distance;
      }
   }
   else
   {
      m_states &= ~STATE_PICKUP_ITEM;
      g_taskFilters[TASK_PICKUPITEM].desire = 0.0f;
   }

   // calculate desire to attack
   if ((m_states & STATE_SEEING_ENEMY) && ReactOnEnemy ())
      g_taskFilters[TASK_ATTACK].desire = TASKPRI_ATTACK;
   else
      g_taskFilters[TASK_ATTACK].desire = 0.0f;

   // calculate desires to seek cover or hunt
   if (IsValidPlayer (m_lastEnemy) && !m_lastEnemyOrigin.IsZero () && !m_hasC4)
   {
      float distance = (m_lastEnemyOrigin - pev->origin).GetLength ();

      // retreat level depends on bot health
      float retreatLevel = (100.0f - (pev->health > 100.0f ? 100.0f : pev->health)) * tempFear;
      float timeSeen = m_seeEnemyTime - engine.Time ();
      float timeHeard = m_heardSoundTime - engine.Time ();
      float ratio = 0.0f;

      if (timeSeen > timeHeard)
      {
         timeSeen += 10.0f;
         ratio = timeSeen * 0.1f;
      }
      else
      {
         timeHeard += 10.0f;
         ratio = timeHeard * 0.1f;
      }

      if (g_bombPlanted || m_isStuck || m_currentWeapon == WEAPON_KNIFE)
         ratio /= 3.0f; // reduce the seek cover desire if bomb is planted
      else if (m_isVIP || m_isReloading)
         ratio *= 3.0f; // triple the seek cover desire if bot is VIP or reloading

      if (distance > 500.0f)
         g_taskFilters[TASK_SEEKCOVER].desire = retreatLevel * ratio;

      // if half of the round is over, allow hunting
      // FIXME: it probably should be also team/map dependant
      if (GetTaskId () != TASK_ESCAPEFROMBOMB && engine.IsNullEntity (m_enemy) && g_timeRoundMid < engine.Time () && !m_isUsingGrenade && m_currentWaypointIndex != waypoints.FindNearest (m_lastEnemyOrigin) && m_personality != PERSONALITY_CAREFUL)
      {
         float desireLevel = 4096.0f - ((1.0f - tempAgression) * distance);

         desireLevel = (100.0f * desireLevel) / 4096.0;
         desireLevel -= retreatLevel;

         if (desireLevel > 89.0f)
            desireLevel = 89.0f;

         g_taskFilters[TASK_HUNTENEMY].desire = desireLevel;
      }
      else
         g_taskFilters[TASK_HUNTENEMY].desire = 0.0f;
   }
   else
   {
      g_taskFilters[TASK_SEEKCOVER].desire = 0.0f;
      g_taskFilters[TASK_HUNTENEMY].desire = 0.0f;
   }

   // blinded behavior
   if (m_blindTime > engine.Time ())
      g_taskFilters[TASK_BLINDED].desire = TASKPRI_BLINDED;
   else
      g_taskFilters[TASK_BLINDED].desire = 0.0f;

   // now we've initialized all the desires go through the hard work
   // of filtering all actions against each other to pick the most
   // rewarding one to the bot.

   // FIXME: instead of going through all of the actions it might be
   // better to use some kind of decision tree to sort out impossible
   // actions.

   // most of the values were found out by trial-and-error and a helper
   // utility i wrote so there could still be some weird behaviors, it's
   // hard to check them all out.

   m_oldCombatDesire = HysteresisDesire (g_taskFilters[TASK_ATTACK].desire, 40.0f, 90.0f, m_oldCombatDesire);
   g_taskFilters[TASK_ATTACK].desire = m_oldCombatDesire;

   TaskItem *taskOffensive = &g_taskFilters[TASK_ATTACK];
   TaskItem *taskPickup = &g_taskFilters[TASK_PICKUPITEM];

   // calc survive (cover/hide)
   TaskItem *taskSurvive = ThresholdDesire (&g_taskFilters[TASK_SEEKCOVER], 40.0f, 0.0f);
   taskSurvive = SubsumeDesire (&g_taskFilters[TASK_HIDE], taskSurvive);

   TaskItem *def = ThresholdDesire (&g_taskFilters[TASK_HUNTENEMY], 41.0f, 0.0f); // don't allow hunting if desires 60<
   taskOffensive = SubsumeDesire (taskOffensive, taskPickup); // if offensive task, don't allow picking up stuff

   TaskItem *taskSub = MaxDesire (taskOffensive, def); // default normal & careful tasks against offensive actions
   TaskItem *final = SubsumeDesire (&g_taskFilters[TASK_BLINDED], MaxDesire (taskSurvive, taskSub)); // reason about fleeing instead

   if (!m_tasks.IsEmpty ())
   {
      final = MaxDesire (final, GetTask ());
      PushTask (final->id, final->desire, final->data, final->time, final->resume); // push the final behavior in our task stack to carry out
   }
}

void Bot::ResetTasks (void)
{
   // this function resets bot tasks stack, by removing all entries from the stack.

   m_tasks.RemoveAll ();
}

void Bot::PushTask (TaskID id, float desire, int data, float time, bool resume)
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
   IgnoreCollisionShortly ();

   int taskId = GetTaskId ();

   // leader bot?
   if (m_isLeader && taskId == TASK_SEEKCOVER)
      CommandTeam (); // reorganize team if fleeing

   if (taskId == TASK_CAMP)
      SelectBestWeapon ();

   // this is best place to handle some voice commands report team some info
   if (Random.Long (0, 100) < 95)
   {
      if (taskId == TASK_BLINDED)
         InstantChatterMessage (Chatter_GotBlinded);
      else if (taskId == TASK_PLANTBOMB)
         InstantChatterMessage (Chatter_PlantingC4);
   }

   if (Random.Long (0, 100) < 80 && taskId == TASK_CAMP)
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

   if (Random.Long (0, 100) < 80 && GetTaskId () == TASK_CAMP && m_team == TERRORIST && m_inVIPZone)
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
      task.time = 0.0f;
      task.resume = true;

      m_tasks.Push (task);
   }
   return &m_tasks.Last ();
}

void Bot::RemoveCertainTask (TaskID id)
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

   FOR_EACH_AE (m_tasks, i)
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
   if (engine.IsNullEntity (m_enemy) || GetTaskId () == TASK_SEEKCOVER)
      return false;

   // if bot is camping, he should be firing anyway and not leaving his position
   if (GetTaskId () == TASK_CAMP)
      return false;

   // if enemy is near or facing us directly
   if ((m_enemy->v.origin - pev->origin).GetLength () < 256.0f || IsInViewCone (m_enemy->v.origin))
      return true;

   return false;
}

bool Bot::ReactOnEnemy (void)
{
   // the purpose of this function is check if task has to be interrupted because an enemy is near (run attack actions then)

   if (!EnemyIsThreat ())
      return false;

   if (m_enemyReachableTimer < engine.Time ())
   {
      int i = waypoints.FindNearest (pev->origin);
      int enemyIndex = waypoints.FindNearest (m_enemy->v.origin);
      
      float lineDist = (m_enemy->v.origin - pev->origin).GetLength ();
      float pathDist = static_cast <float> (waypoints.GetPathDistance (i, enemyIndex));

      if (pathDist - lineDist > 112.0f)
         m_isEnemyReachable = false;
      else
         m_isEnemyReachable = true;

      m_enemyReachableTimer = engine.Time () + 1.0f;
   }

   if (m_isEnemyReachable)
   {
      m_navTimeset = engine.Time (); // override existing movement by attack movement
      return true;
   }
   return false;
}

bool Bot::LastEnemyShootable (void)
{
   // don't allow shooting through walls
   if (!(m_aimFlags & AIM_LAST_ENEMY) || m_lastEnemyOrigin.IsZero () || engine.IsNullEntity (m_lastEnemy))
      return false;

   return GetShootingConeDeviation (GetEntity (), &m_lastEnemyOrigin) >= 0.90f && IsShootableThruObstacle (m_lastEnemyOrigin);
}

void Bot::CheckRadioCommands (void)
{
   // this function handling radio and reacting to it

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
         if (engine.IsNullEntity (m_targetEntity) && engine.IsNullEntity (m_enemy) && Random.Long (0, 100) < (m_personality == PERSONALITY_CAREFUL ? 80 : 20))
         {
            int numFollowers = 0;

            // Check if no more followers are allowed
            for (int i = 0; i < engine.MaxClients (); i++)
            {
               Bot *bot = bots.GetBot (i);

               if (bot != NULL)
               {
                  if (bot->m_notKilled)
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
               TaskID taskID = GetTaskId ();

               if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
                  GetTask ()->time = engine.Time ();

               PushTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, -1, 0.0f, true);
            }
            else if (numFollowers > allowedFollowers)
            {
               for (int i = 0; (i < engine.MaxClients () && numFollowers > allowedFollowers); i++)
               {
                  Bot *bot = bots.GetBot (i);

                  if (bot != NULL)
                  {
                     if (bot->m_notKilled)
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
            else if (m_radioOrder != Chatter_GoingToPlantBomb && Random.Long (0, 100) < 50)
               RadioMessage (Radio_Negative);
         }
         else if (m_radioOrder != Chatter_GoingToPlantBomb && Random.Long (0, 100) < 50)
            RadioMessage (Radio_Negative);
      }
      break;

   case Radio_HoldPosition:
      if (!engine.IsNullEntity (m_targetEntity))
      {
         if (m_targetEntity == m_radioEntity)
         {
            m_targetEntity = NULL;
            RadioMessage (Radio_Affirmative);

            m_campButtons = 0;

            PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, engine.Time () + Random.Float (30.0f, 60.0f), false);
         }
      }
      break;

   case Chatter_NewRound:
       ChatterMessage (Chatter_You_Heard_The_Man);   
       break;

   case Radio_TakingFire:
      if (engine.IsNullEntity (m_targetEntity))
      {
         if (engine.IsNullEntity (m_enemy) && m_seeEnemyTime + 4.0f < engine.Time ())
         {
            // decrease fear levels to lower probability of bot seeking cover again
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f)
               m_fearLevel = 0.0f;

            if (Random.Long (0, 100) < 45 && yb_communication_type.GetInt () == 2)
               ChatterMessage (Chatter_OnMyWay);
            else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.GetInt () != 2)
               RadioMessage (Radio_Affirmative);

            TryHeadTowardRadioEntity ();
         }
         else if (Random.Long (0, 100) < 70)
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
      if (((engine.IsNullEntity (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 2048.0f || !m_moveToC4) && Random.Long (0, 100) > 50 && m_seeEnemyTime + 4.0f < engine.Time ())
      {
         m_fearLevel -= 0.1f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;

         if (Random.Long (0, 100) < 45 && yb_communication_type.GetInt () == 2)
            ChatterMessage (Chatter_OnMyWay);
         else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.GetInt () != 2)
            RadioMessage (Radio_Affirmative);

         TryHeadTowardRadioEntity ();
      }
      else if (Random.Long (0, 100) < 60 && m_radioOrder == Radio_NeedBackup)
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
         m_fearLevel -= 0.2f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;
      }
      else if ((engine.IsNullEntity (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 2048.0f)
      {
         TaskID taskID = GetTaskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
         {
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f)
               m_fearLevel = 0.0f;
         
            RadioMessage (Radio_Affirmative);
            // don't pause/camp anymore
            GetTask ()->time = engine.Time ();

            m_targetEntity = NULL;
            MakeVectors (m_radioEntity->v.v_angle);

            m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * Random.Float (1024.0f, 2048.0f);

            DeleteSearchNodes ();
            PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);
         }
      }
      else if (!engine.IsNullEntity (m_doubleJumpEntity))
      {
         RadioMessage (Radio_Affirmative);
         ResetDoubleJumpState ();
      }
      else
         RadioMessage (Radio_Negative);

      break;

   case Radio_ShesGonnaBlow:
      if (engine.IsNullEntity (m_enemy) && distance < 2048.0f && g_bombPlanted && m_team == TERRORIST)
      {
         RadioMessage (Radio_Affirmative);

         if (GetTaskId () == TASK_CAMP)
            RemoveCertainTask (TASK_CAMP);

         m_targetEntity = NULL;
         PushTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, -1, 0.0f, true);
      }
      else
        RadioMessage (Radio_Negative);

      break;

   case Radio_RegroupTeam:
      // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
      if (m_team == CT && m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0 && g_bombPlanted && GetTaskId () != TASK_DEFUSEBOMB)
      {
         SelectWeaponByName ("weapon_knife");

         DeleteSearchNodes ();
         MoveToVector (waypoints.GetBombPosition ());

         RadioMessage (Radio_Affirmative);
      }
      break;

   case Radio_StormTheFront:
      if (((engine.IsNullEntity (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024.0f) && Random.Long (0, 100) > 50)
      {
         RadioMessage (Radio_Affirmative);

         // don't pause/camp anymore
         TaskID taskID = GetTaskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
            GetTask ()->time = engine.Time ();

         m_targetEntity = NULL;

         MakeVectors (m_radioEntity->v.v_angle);
         m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * Random.Float (1024.0f, 2048.0f);

         DeleteSearchNodes ();
         PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);

         m_fearLevel -= 0.3f;

         if (m_fearLevel < 0.0f)
            m_fearLevel = 0.0f;

         m_agressionLevel += 0.3f;

         if (m_agressionLevel > 1.0f)
            m_agressionLevel = 1.0f;
      }
      break;

   case Radio_Fallback:
      if ((engine.IsNullEntity (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024.0f)
      {
         m_fearLevel += 0.5f;

         if (m_fearLevel > 1.0f)
            m_fearLevel = 1.0f;

         m_agressionLevel -= 0.5f;

         if (m_agressionLevel < 0.0f)
            m_agressionLevel = 0.0f;

         if (GetTaskId () == TASK_CAMP)
            GetTask ()->time += Random.Float (10.0f, 15.0f);
         else
         {
            // don't pause/camp anymore
            TaskID taskID = GetTaskId ();

            if (taskID == TASK_PAUSE)
               GetTask ()->time = engine.Time ();

            m_targetEntity = NULL;
            m_seeEnemyTime = engine.Time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.IsZero ())
            {
               float nearestDistance = 99999.0f;

               // take nearest enemy to ordering player
               for (int i = 0; i < engine.MaxClients (); i++)
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
         RadioMessage ((GetNearbyEnemiesNearPosition (pev->origin, 400.0f) == 0 && yb_communication_type.GetInt () != 2) ? Radio_SectorClear : Radio_ReportingIn);
      break;

   case Radio_SectorClear:
      // is bomb planted and it's a ct
      if (g_bombPlanted)
      {
         int bombPoint = -1;

         // check if it's a ct command
         if (engine.GetTeam (m_radioEntity) == CT && m_team == CT && IsValidBot (m_radioEntity) && g_timeNextBombUpdate < engine.Time ())
         {
            float minDistance = 99999.0f;

            // find nearest bomb waypoint to player
            FOR_EACH_AE (waypoints.m_goalPoints, i)
            {
               distance = (waypoints.GetPath (waypoints.m_goalPoints[i])->origin - m_radioEntity->v.origin).GetLengthSquared ();

               if (distance < minDistance)
               {
                  minDistance = distance;
                  bombPoint = waypoints.m_goalPoints[i];
               }
            }

            // mark this waypoint as restricted point
            if (bombPoint != -1 && !waypoints.IsGoalVisited (bombPoint))
            {
               // does this bot want to defuse?
               if (GetTaskId () == TASK_NORMAL)
               {
                  // is he approaching this goal?
                  if (GetTask ()->data == bombPoint)
                  {
                     GetTask ()->data = -1;
                     RadioMessage (Radio_Affirmative);

                  }
               }
               waypoints.SetGoalVisited (bombPoint);
            }
            g_timeNextBombUpdate = engine.Time () + 0.5f;
         }
      }
      break;

   case Radio_GetInPosition:
      if ((engine.IsNullEntity (m_enemy) && EntityIsVisible (m_radioEntity->v.origin)) || distance < 1024.0f)
      {
         RadioMessage (Radio_Affirmative);

         if (GetTaskId () == TASK_CAMP)
            GetTask ()->time = engine.Time () + Random.Float (30.0f, 60.0f);
         else
         {
            // don't pause anymore
            TaskID taskID = GetTaskId ();

            if (taskID == TASK_PAUSE)
               GetTask ()->time = engine.Time ();

            m_targetEntity = NULL;
            m_seeEnemyTime = engine.Time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.IsZero ())
            {
               float nearestDistance = 99999.0f;

               // take nearest enemy to ordering player
               for (int i = 0; i < engine.MaxClients (); i++)
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
            PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (30.0f, 60.0f), true);
            // push move command
            PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + Random.Float (30.0f, 60.0f), true);

            if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
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
   TaskID taskID = GetTaskId ();

   if (taskID == TASK_MOVETOPOSITION || m_headedTime + 15.0f < engine.Time () || !IsAlive (m_radioEntity) || m_hasC4)
      return;

   if ((IsValidBot (m_radioEntity) && Random.Long (0, 100) < 25 && m_personality == PERSONALITY_NORMAL) || !(m_radioEntity->v.flags & FL_FAKECLIENT))
   {
      if (taskID == TASK_PAUSE || taskID == TASK_CAMP)
         GetTask ()->time = engine.Time ();

      m_headedTime = engine.Time ();
      m_position = m_radioEntity->v.origin;
      DeleteSearchNodes ();

      PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, -1, 0.0f, true);
   }
}

void Bot::SelectLeaderEachTeam (int team)
{
   if (g_mapType & MAP_AS)
   {
      if (m_isVIP && !g_leaderChoosen[CT])
      {
         // vip bot is the leader
         m_isLeader = true;
                               
         if (Random.Long (1, 100) < 50)
         {
            RadioMessage (Radio_FollowMe);
            m_campButtons = 0;
         }
         g_leaderChoosen[CT] = true;
      }
      else if (team == TERRORIST && !g_leaderChoosen[TERRORIST])
      {
         Bot *botLeader = bots.GetHighestFragsBot(team);
         
         if (botLeader != NULL && botLeader->m_notKilled)
         {
            botLeader->m_isLeader = true;

            if (Random.Long (1, 100) < 45)
               botLeader->RadioMessage (Radio_FollowMe);
         }
         g_leaderChoosen[TERRORIST] = true;
      }
   }
   else if (g_mapType & MAP_DE)
   {
      if (team == TERRORIST && !g_leaderChoosen[TERRORIST])
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
            g_leaderChoosen[TERRORIST] = true;
         }
      }
      else if (!g_leaderChoosen[CT])
      {
         Bot *botLeader = bots.GetHighestFragsBot(team);

         if (botLeader != NULL)               
         {
            botLeader->m_isLeader = true;

            if (Random.Long (1, 100) < 30)
               botLeader->RadioMessage (Radio_FollowMe);
         }     
         g_leaderChoosen[CT] = true;
      }
   }
   else if (g_mapType & (MAP_ES | MAP_KA | MAP_FY))
   {
      Bot *botLeader = bots.GetHighestFragsBot (team);

      if (botLeader != NULL)
      {
         botLeader->m_isLeader = true;

         if (Random.Long (1, 100) < 30)
            botLeader->RadioMessage (Radio_FollowMe);
      }
   }
   else  
   {
      Bot *botLeader = bots.GetHighestFragsBot(team);

      if (botLeader != NULL)
      {
         botLeader->m_isLeader = true;

         if (Random.Long (1, 100) < (team == TERRORIST ? 30 : 40))
            botLeader->RadioMessage (Radio_FollowMe);
      }
   }
}

void Bot::ChooseAimDirection (void)
{
   unsigned int flags = m_aimFlags;

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
      m_lookAt = m_throw + Vector (0.0f, 0.0f, 1.0f * m_grenade.z);
   else if (flags & AIM_ENEMY)
      FocusEnemy ();
   else if (flags & AIM_ENTITY)
      m_lookAt = m_entity;
   else if (flags & AIM_LAST_ENEMY)
   {
      m_lookAt = m_lastEnemyOrigin;

      // did bot just see enemy and is quite aggressive?
      if (m_seeEnemyTime + 2.0f - m_actualReactionTime + m_baseAgressionLevel > engine.Time ())
      {
         // feel free to fire if shootable
         if (!UsesSniper () && LastEnemyShootable ())
            m_wantsToFire = true;
      }
   }
   else if (flags & AIM_PREDICT_PATH)
   {
      bool changePredictedEnemy = true;

      if (m_trackingEdict == m_lastEnemy)
      {
         if (m_timeNextTracking < engine.Time ())
            changePredictedEnemy = IsAlive (m_lastEnemy);
      }

      if (changePredictedEnemy)
      {
         m_lookAt = waypoints.GetPath (GetAimingWaypoint (m_lastEnemyOrigin))->origin;
         m_camp = m_lookAt;

         m_timeNextTracking = engine.Time () + 2.0f;
         m_trackingEdict = m_lastEnemy;
      }
      else
         m_lookAt = m_camp;
   }
   else if (flags & AIM_CAMP)
      m_lookAt = m_camp;
   else if (flags & AIM_NAVPOINT)
   {
      m_lookAt = m_destOrigin;

      if (m_canChooseAimDirection && m_currentWaypointIndex != -1 && !(m_currentPath->flags & FLAG_LADDER))
      {
         int index = m_currentWaypointIndex;

         if (m_team == TERRORIST)
         {
            if ((g_experienceData + (index * g_numWaypoints) + index)->team0DangerIndex != -1)
               m_lookAt = waypoints.GetPath ((g_experienceData + (index * g_numWaypoints) + index)->team0DangerIndex)->origin;
         }
         else
         {
            if ((g_experienceData + (index * g_numWaypoints) + index)->team1DangerIndex != -1)
               m_lookAt = waypoints.GetPath ((g_experienceData + (index * g_numWaypoints) + index)->team1DangerIndex)->origin;
         }
      }
   }

   if (m_lookAt.IsZero ())
      m_lookAt = m_destOrigin;
}

void Bot::Think (void)
{
   if (m_thinkFps <= engine.Time ())
   {
      // execute delayed think
      ThinkDelayed ();

      // skip some frames
      m_thinkFps = engine.Time () + m_thinkInterval;
   }
   else
      UpdateLookAngles ();
}

void Bot::ThinkDelayed (void)
{
   pev->button = 0;
   pev->flags |= FL_FAKECLIENT; // restore fake client bit, if it were removed by some evil action =)

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;
   m_moveAngles.Zero ();

   m_canChooseAimDirection = true;
   m_notKilled = IsAlive (GetEntity ());
   m_team = engine.GetTeam (GetEntity ());

   if (m_team == TERRORIST && (g_mapType & MAP_DE))
      m_hasC4 = !!(pev->weapons & (1 << WEAPON_C4));

   // is bot movement enabled
   bool botMovement = false;

   if (m_notStarted) // if the bot hasn't selected stuff to start the game yet, go do that...
      StartGame (); // select team & class
   else if (!m_notKilled)
   {
      // no movement allowed in
      if (m_voteKickIndex != m_lastVoteKick && yb_tkpunish.GetBool ()) // we got a teamkiller? vote him away...
      {
         engine.IssueBotCommand (GetEntity (), "vote %d", m_voteKickIndex);
         m_lastVoteKick = m_voteKickIndex;

         // if bot tk punishment is enabled slay the tk
         if (yb_tkpunish.GetInt () != 2 || IsValidBot (engine.EntityOfIndex (m_voteKickIndex)))
            return;

         edict_t *killer = engine.EntityOfIndex (m_lastVoteKick);

         killer->v.frags++;
         MDLL_ClientKill (killer);
      }
      else if (m_voteMap != 0) // host wants the bots to vote for a map?
      {
         engine.IssueBotCommand (GetEntity (), "votemap %d", m_voteMap);
         m_voteMap = 0;
      }
      extern ConVar yb_chat;

      if (yb_chat.GetBool () && !RepliesToPlayer () && m_lastChatTime + 10.0 < engine.Time () && g_lastChatTime + 5.0f < engine.Time ()) // bot chatting turned on?
      {
         // say a text every now and then
         if (Random.Long (1, 1500) < 2)
         {
            m_lastChatTime = engine.Time ();
            g_lastChatTime = engine.Time ();

            char *pickedPhrase = const_cast <char *> (g_chatFactory[CHAT_DEAD].GetRandomElement ().GetBuffer ());
            bool sayBufferExists = false;

            // search for last messages, sayed
            FOR_EACH_AE (m_sayTextBuffer.lastUsedSentences, i)
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
   else if (m_notKilled && m_buyingFinished && !(pev->maxspeed < 10.0f && GetTaskId () != TASK_PLANTBOMB && GetTaskId () != TASK_DEFUSEBOMB) && !yb_freeze_bots.GetBool ())
      botMovement = true;

#ifdef XASH_CSDM
   if (m_notKilled)
      botMovement = true;
#endif

   CheckMessageQueue (); // check for pending messages

   // remove voice icon
   if (g_lastRadioTime[g_clients[engine.IndexOfEntity (GetEntity ()) - 1].realTeam] + Random.Float (0.8f, 2.1f) < engine.Time ())
      SwitchChatterIcon (false); // hide icon

   if (botMovement)
      BotAI (); // execute main code

   RunPlayerMovement ();  // run the player movement
}

void Bot::PeriodicThink (void)
{
   if (m_timePeriodicUpdate > engine.Time ())
      return;

   // this function is called from main think function

   m_numFriendsLeft = GetNearbyFriendsNearPosition (pev->origin, 99999.0f);
   m_numEnemiesLeft = GetNearbyEnemiesNearPosition (pev->origin, 99999.0f);

   if (g_bombPlanted && m_team == CT && (pev->origin - waypoints.GetBombPosition ()).GetLength () < 700 && !IsBombDefusing (waypoints.GetBombPosition ()) && !m_hasProgressBar && GetTaskId () != TASK_ESCAPEFROMBOMB)
      ResetTasks ();

   CheckSpawnTimeConditions ();

   // clear enemy far away
   if (!m_lastEnemyOrigin.IsZero () && !engine.IsNullEntity (m_lastEnemy) && (pev->origin - m_lastEnemyOrigin).GetLength () >= 1600.0f)
   {
      m_lastEnemy = NULL;
      m_lastEnemyOrigin.Zero ();
   }
   m_timePeriodicUpdate = engine.Time () + 0.5f;
}

void Bot::RunTask_Normal (void)
{
   m_aimFlags |= AIM_NAVPOINT;

   // user forced a waypoint as a goal?
   if (yb_debug_goal.GetInt () != -1 && GetTask ()->data != yb_debug_goal.GetInt ())
   {
      DeleteSearchNodes ();
      GetTask ()->data = yb_debug_goal.GetInt ();
   }

   // bots rushing with knife, when have no enemy (thanks for idea to nicebot project)
   if (m_currentWeapon == WEAPON_KNIFE && (engine.IsNullEntity (m_lastEnemy) || !IsAlive (m_lastEnemy)) && engine.IsNullEntity (m_enemy) && m_knifeAttackTime < engine.Time () && !HasShield () && GetNearbyFriendsNearPosition (pev->origin, 96) == 0)
   {
      if (Random.Long (0, 100) < 40)
         pev->button |= IN_ATTACK;
      else
         pev->button |= IN_ATTACK2;

      m_knifeAttackTime = engine.Time () + Random.Float (2.5f, 6.0f);
   }

   if (m_reloadState == RELOAD_NONE && GetAmmo () != 0 && GetAmmoInClip () < 5 && g_weaponDefs[m_currentWeapon].ammo1 != -1)
      m_reloadState = RELOAD_PRIMARY;

   // if bomb planted and it's a CT calculate new path to bomb point if he's not already heading for
   if (g_bombPlanted && m_team == CT && GetTask ()->data != -1 && !(waypoints.GetPath (GetTask ()->data)->flags & FLAG_GOAL) && GetTaskId () != TASK_ESCAPEFROMBOMB)
   {
      DeleteSearchNodes ();
      GetTask ()->data = -1;
   }

   if (!g_bombPlanted && m_currentWaypointIndex != -1 && (m_currentPath->flags & FLAG_GOAL) && Random.Long (0, 100) < 80 && GetNearbyEnemiesNearPosition (pev->origin, 650.0f) == 0)
      RadioMessage (Radio_SectorClear);

   // reached the destination (goal) waypoint?
   if (DoWaypointNav ())
   {
      TaskComplete ();
      m_prevGoalIndex = -1;

      // spray logo sometimes if allowed to do so
      if (m_timeLogoSpray < engine.Time () && yb_spraypaints.GetBool () && Random.Long (1, 100) < 60 && m_moveSpeed > GetWalkSpeed () && engine.IsNullEntity (m_pickupItem))
      {
         if (!((g_mapType & MAP_DE) && g_bombPlanted && m_team == CT))
            PushTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, -1, engine.Time () + 1.0f, false);
      }

      // reached waypoint is a camp waypoint
      if ((m_currentPath->flags & FLAG_CAMP) && !yb_csdm_mode.GetBool () && yb_camping_allowed.GetBool ())
      {
         // check if bot has got a primary weapon and hasn't camped before
         if (HasPrimaryWeapon () && m_timeCamping + 10.0f < engine.Time () && !HasHostage ())
         {
            bool campingAllowed = true;

            // Check if it's not allowed for this team to camp here
            if (m_team == TERRORIST)
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
            if (campingAllowed && IsPlayerVIP (GetEntity ()) || ((g_mapType & MAP_DE) && m_team == TERRORIST && !g_bombPlanted && m_hasC4))
               campingAllowed = false;

            // check if another bot is already camping here
            if (campingAllowed && IsPointOccupied (m_currentWaypointIndex))
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

               PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (20.0f, 40.0f), true);

               m_camp = Vector (m_currentPath->campStartX, m_currentPath->campStartY, 0.0f);
               m_aimFlags |= AIM_CAMP;
               m_campDirection = 0;

               // tell the world we're camping
               if (Random.Long (0, 100) < 60)
                  RadioMessage (Radio_InPosition);

               m_moveToGoal = false;
               m_checkTerrain = false;

               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;
            }
         }
      }
      else
      {
         // some goal waypoints are map dependant so check it out...
         if (g_mapType & MAP_CS)
         {
            // CT Bot has some hostages following?
            if (m_team == CT && HasHostage ())
            {
               // and reached a Rescue Point?
               if (m_currentPath->flags & FLAG_RESCUE)
               {
                  for (int i = 0; i < MAX_HOSTAGES; i++)
                     m_hostages[i] = NULL; // clear array of hostage pointers
               }
            }
            else if (m_team == TERRORIST && Random.Long (0, 100) < 80)
            {
               int index = FindDefendWaypoint (m_currentPath->origin);

               PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (60.0f, 120.0f), true); // push camp task on to stack
               PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + Random.Float (5.0f, 10.0f), true); // push move command

               if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
                  m_campButtons |= IN_DUCK;
               else
                  m_campButtons &= ~IN_DUCK;

               ChatterMessage (Chatter_GoingToGuardVIPSafety); // play info about that
            }
         }
         else if ((g_mapType & MAP_DE) && ((m_currentPath->flags & FLAG_GOAL) || m_inBombZone))
         {
            // is it a terrorist carrying the bomb?
            if (m_hasC4)
            {
               if ((m_states & STATE_SEEING_ENEMY) && GetNearbyFriendsNearPosition (pev->origin, 768.0f) == 0)
               {
                  // request an help also
                  RadioMessage (Radio_NeedBackup);
                  InstantChatterMessage (Chatter_ScaredEmotion);

                  PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (4.0f, 8.0f), true);
               }
               else
                  PushTask (TASK_PLANTBOMB, TASKPRI_PLANTBOMB, -1, 0.0f, false);
            }
            else if (m_team == CT)
            {
               if (!g_bombPlanted && GetNearbyFriendsNearPosition (pev->origin, 210.0f) < 4)
               {
                  int index = FindDefendWaypoint (m_currentPath->origin);

                  float campTime = Random.Float (25.0f, 40.f);

                  // rusher bots don't like to camp too much
                  if (m_personality == PERSONALITY_RUSHER)
                     campTime *= 0.5f;

                  PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + campTime, true); // push camp task on to stack
                  PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + Random.Float (5.0f, 11.0f), true); // push move command

                  if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
                     m_campButtons |= IN_DUCK;
                  else
                     m_campButtons &= ~IN_DUCK;

                  ChatterMessage (Chatter_DefendingBombSite); // play info about that
               }
            }
         }
      }
   }
   // no more nodes to follow - search new ones (or we have a bomb)
   else if (!GoalIsValid ())
   {
      m_moveSpeed = 0.0f;
      DeleteSearchNodes ();

      // did we already decide about a goal before?
      int destIndex = GetTask ()->data != -1 ? GetTask ()->data : FindGoal ();

      m_prevGoalIndex = destIndex;
      m_chosenGoalIndex = destIndex;

      // remember index
      GetTask ()->data = destIndex;

      // do pathfinding if it's not the current waypoint
      if (destIndex != m_currentWaypointIndex)
         FindPath (m_currentWaypointIndex, destIndex, ((g_bombPlanted && m_team == CT) || yb_debug_goal.GetInt () != -1) ? SEARCH_PATH_FASTEST : m_pathType);
   }
   else
   {
      if (!(pev->flags & FL_DUCKING) && m_minSpeed != pev->maxspeed)
         m_moveSpeed = m_minSpeed;
   }

   if ((yb_walking_allowed.GetBool () && mp_footsteps.GetBool ()) && m_difficulty >= 2 && !(m_aimFlags & AIM_ENEMY) && (m_heardSoundTime + 8.0f >= engine.Time () || (m_states & (STATE_HEARING_ENEMY | STATE_SUSPECT_ENEMY))) && GetNearbyEnemiesNearPosition (pev->origin, 1024.0f) >= 1 && !yb_jasonmode.GetBool () && !g_bombPlanted)
      m_moveSpeed = GetWalkSpeed ();

   // bot hasn't seen anything in a long time and is asking his teammates to report in
   if (m_seeEnemyTime + Random.Float (45.0f, 80.0f) < engine.Time () && Random.Long (0, 100) < 30 && g_timeRoundStart + 20.0f < engine.Time () && m_askCheckTime < engine.Time ())
   {
      m_askCheckTime = engine.Time () + Random.Float (45.0f, 80.0f);
      RadioMessage (Radio_ReportTeam);
   }
}

void Bot::RunTask_Spray (void)
{
   m_aimFlags |= AIM_ENTITY;

   // bot didn't spray this round?
   if (m_timeLogoSpray < engine.Time () && GetTask ()->time > engine.Time ())
   {
      MakeVectors (pev->v_angle);
      Vector sprayOrigin = EyePosition () + g_pGlobals->v_forward * 128.0f;

      TraceResult tr;
      engine.TestLine (EyePosition (), sprayOrigin, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

      // no wall in front?
      if (tr.flFraction >= 1.0f)
         sprayOrigin.z -= 128.0f;

      m_entity = sprayOrigin;

      if (GetTask ()->time - 0.5f < engine.Time ())
      {
         // emit spraycan sound
         EMIT_SOUND_DYN2 (GetEntity (), CHAN_VOICE, "player/sprayer.wav", 1.0, ATTN_NORM, 0, 100.0f);
         engine.TestLine (EyePosition (), EyePosition () + g_pGlobals->v_forward * 128.0f, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

         // paint the actual logo decal
         DecalTrace (pev, &tr, m_logotypeIndex);
         m_timeLogoSpray = engine.Time () + Random.Float (60.0f, 90.0f);
      }
   }
   else
      TaskComplete ();

   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = engine.Time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   IgnoreCollisionShortly ();
}

void Bot::RunTask_HuntEnemy (void)
{
   m_aimFlags |= AIM_NAVPOINT;
   m_checkTerrain = true;

   // if we've got new enemy...
   if (!engine.IsNullEntity (m_enemy) || engine.IsNullEntity (m_lastEnemy))
   {
      // forget about it...
      TaskComplete ();
      m_prevGoalIndex = -1;

      m_lastEnemy = NULL;
      m_lastEnemyOrigin.Zero ();
   }
   else if (engine.GetTeam (m_lastEnemy) == m_team)
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
      m_lastEnemyOrigin.Zero ();
   }
   else if (!GoalIsValid ()) // do we need to calculate a new path?
   {
      DeleteSearchNodes ();

      int destIndex = -1;

      // is there a remembered index?
      if (GetTask ()->data != -1 && GetTask ()->data < g_numWaypoints)
         destIndex = GetTask ()->data;
      else // no. we need to find a new one
         destIndex = waypoints.FindNearest (m_lastEnemyOrigin);

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
            if (m_currentPath->radius < 32 && !IsOnLadder () && !IsInWater () && m_seeEnemyTime + 4.0f > engine.Time () && m_difficulty < 2)
               pev->button |= IN_DUCK;
         }

         if ((m_lastEnemyOrigin - pev->origin).GetLength () < 512.0f)
            m_moveSpeed = GetWalkSpeed ();
      }
   }
}

void Bot::RunTask_SeekCover (void)
{
   m_aimFlags |= AIM_NAVPOINT;

   if (engine.IsNullEntity (m_lastEnemy) || !IsAlive (m_lastEnemy))
   {
      TaskComplete ();
      m_prevGoalIndex = -1;
   }
   else if (DoWaypointNav ()) // reached final cover waypoint?
   {
      // yep. activate hide behaviour
      TaskComplete ();

      m_prevGoalIndex = -1;
      m_pathType = SEARCH_PATH_FASTEST;

      // start hide task
      PushTask (TASK_HIDE, TASKPRI_HIDE, -1, engine.Time () + Random.Float (5.0f, 15.0f), false);
      Vector dest = m_lastEnemyOrigin;

      // get a valid look direction
      GetCampDirection (&dest);

      m_aimFlags |= AIM_CAMP;
      m_camp = dest;
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
         m_currentPath->campStartX = dest.x;
         m_currentPath->campStartY = dest.y;

         m_currentPath->campStartX = dest.x;
         m_currentPath->campEndY = dest.y;
      }

      if (m_reloadState == RELOAD_NONE && GetAmmoInClip () < 8 && GetAmmo () != 0)
         m_reloadState = RELOAD_PRIMARY;

      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_moveToGoal = false;
      m_checkTerrain = true;
   }
   else if (!GoalIsValid ()) // we didn't choose a cover waypoint yet or lost it due to an attack?
   {
      DeleteSearchNodes ();

      int destIndex = -1;

      if (GetTask ()->data != -1)
         destIndex = GetTask ()->data;
      else
      {
         destIndex = FindCoverWaypoint (1024.0f);

         if (destIndex == -1)
            destIndex = waypoints.FindNearest (pev->origin, 512.0f);
      }
      m_campDirection = 0;

      m_prevGoalIndex = destIndex;
      GetTask ()->data = destIndex;

      if (destIndex != m_currentWaypointIndex)
         FindPath (m_currentWaypointIndex, destIndex, SEARCH_PATH_FASTEST);
   }
}

void Bot::RunTask_Attack (void)
{
   m_moveToGoal = false;
   m_checkTerrain = false;

   if (!engine.IsNullEntity (m_enemy))
   {
      IgnoreCollisionShortly ();

      if (IsOnLadder ())
      {
         pev->button |= IN_JUMP;
         DeleteSearchNodes ();
      }
      CombatFight ();

      if (m_currentWeapon == WEAPON_KNIFE && !m_lastEnemyOrigin.IsZero ())
         m_destOrigin = m_lastEnemyOrigin;
   }
   else
   {
      TaskComplete ();
      m_destOrigin = m_lastEnemyOrigin;
   }
   m_navTimeset = engine.Time ();
}

void Bot::RunTask_Pause (void)
{
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = engine.Time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   m_aimFlags |= AIM_NAVPOINT;

   // is bot blinded and above average difficulty?
   if (m_viewDistance < 500.0f && m_difficulty >= 2)
   {
      // go mad!
      m_moveSpeed = -fabsf ((m_viewDistance - 500.0f) * 0.5f);

      if (m_moveSpeed < -pev->maxspeed)
         m_moveSpeed = -pev->maxspeed;

      MakeVectors (pev->v_angle);
      m_camp = EyePosition () + g_pGlobals->v_forward * 500.0f;

      m_aimFlags |= AIM_OVERRIDE;
      m_wantsToFire = true;
   }
   else
      pev->button |= m_campButtons;

   // stop camping if time over or gets hurt by something else than bullets
   if (GetTask ()->time < engine.Time () || m_lastDamageType > 0)
      TaskComplete ();
}

void Bot::RunTask_Blinded (void)
{
   m_moveToGoal = false;
   m_checkTerrain = false;
   m_navTimeset = engine.Time ();

   // if bot remembers last enemy position
   if (m_difficulty >= 2 && !m_lastEnemyOrigin.IsZero () && IsValidPlayer (m_lastEnemy) && !UsesSniper ())
   {
      m_lookAt = m_lastEnemyOrigin; // face last enemy
      m_wantsToFire = true; // and shoot it
   }

   m_moveSpeed = m_blindMoveSpeed;
   m_strafeSpeed = m_blindSidemoveSpeed;
   pev->button |= m_blindButton;

   if (m_blindTime < engine.Time ())
      TaskComplete ();
}

void Bot::RunTask_Camp (void)
{
   if (!yb_camping_allowed.GetBool ())
   {
      TaskComplete ();
      return;
   }

   m_aimFlags |= AIM_CAMP;
   m_checkTerrain = false;
   m_moveToGoal = false;

   if (m_team == CT && g_bombPlanted && m_defendedBomb && !IsBombDefusing (waypoints.GetBombPosition ()) && !OutOfBombTimer ())
   {
      m_defendedBomb = false;
      TaskComplete ();
   }

   // half the reaction time if camping because you're more aware of enemies if camping
   SetIdealReactionTimes ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = engine.Time ();
   m_timeCamping = engine.Time ();

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   GetValidWaypoint ();

   if (m_nextCampDirTime < engine.Time ())
   {
      m_nextCampDirTime = engine.Time () + Random.Float (2.0f, 5.0f);

      if (m_currentPath->flags & FLAG_CAMP)
      {
         Vector dest;

         // switch from 1 direction to the other
         if (m_campDirection < 1)
         {
            dest.x = m_currentPath->campStartX;
            dest.y = m_currentPath->campStartY;

            m_campDirection ^= 1;
         }
         else
         {
            dest.x = m_currentPath->campEndX;
            dest.y = m_currentPath->campEndY;
            m_campDirection ^= 1;
         }
         dest.z = 0.0f;

         // find a visible waypoint to this direction...
         // i know this is ugly hack, but i just don't want to break compatiability :)
         int numFoundPoints = 0;
         int foundPoints[3];
         int distanceTab[3];

         const Vector &dotA = (dest - pev->origin).Normalize2D ();

         for (int i = 0; i < g_numWaypoints; i++)
         {
            // skip invisible waypoints or current waypoint
            if (!waypoints.IsVisible (m_currentWaypointIndex, i) || (i == m_currentWaypointIndex))
               continue;

            const Vector &dotB = (waypoints.GetPath (i)->origin - pev->origin).Normalize2D ();

            if ((dotA | dotB) > 0.9)
            {
               int distance = static_cast <int> ((pev->origin - waypoints.GetPath (i)->origin).GetLength ());

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
            m_camp = waypoints.GetPath (foundPoints[Random.Long (0, numFoundPoints)])->origin;
         else
            m_camp = waypoints.GetPath (GetAimingWaypoint ())->origin;
      }
      else
         m_camp = waypoints.GetPath (GetAimingWaypoint ())->origin;
   }
   // press remembered crouch button
   pev->button |= m_campButtons;

   // stop camping if time over or gets hurt by something else than bullets
   if (GetTask ()->time < engine.Time () || m_lastDamageType > 0)
      TaskComplete ();
}

void Bot::RunTask_Hide (void)
{
   m_aimFlags |= AIM_CAMP;
   m_checkTerrain = false;
   m_moveToGoal = false;

   // half the reaction time if camping
   SetIdealReactionTimes ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = engine.Time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

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

         if (!engine.IsNullEntity (m_enemy))
            CombatFight ();

         return;
      }
   }
   else if (m_lastEnemyOrigin.IsZero ()) // If we don't have an enemy we're also free to leave
   {
      TaskComplete ();

      m_campButtons = 0;
      m_prevGoalIndex = -1;

      if (GetTaskId () == TASK_HIDE)
         TaskComplete ();

      return;
   }

   pev->button |= m_campButtons;
   m_navTimeset = engine.Time ();

   // stop camping if time over or gets hurt by something else than bullets
   if (GetTask ()->time < engine.Time () || m_lastDamageType > 0)
      TaskComplete ();
}

void Bot::RunTask_MoveToPos (void)
{
   m_aimFlags |= AIM_NAVPOINT;

   if (IsShieldDrawn ())
      pev->button |= IN_ATTACK2;

   if (DoWaypointNav ()) // reached destination?
   {
      TaskComplete (); // we're done

      m_prevGoalIndex = -1;
      m_position.Zero ();
   }
   else if (!GoalIsValid ()) // didn't choose goal waypoint yet?
   {
      DeleteSearchNodes ();

      int destIndex = -1;

      if (GetTask ()->data != -1 && GetTask ()->data < g_numWaypoints)
         destIndex = GetTask ()->data;
      else
         destIndex = waypoints.FindNearest (m_position);

      if (destIndex >= 0 && destIndex < g_numWaypoints)
      {
         m_prevGoalIndex = destIndex;
         GetTask ()->data = destIndex;

         FindPath (m_currentWaypointIndex, destIndex, m_pathType);
      }
      else
         TaskComplete ();
   }
}

void Bot::RunTask_PlantBomb (void)
{
   m_aimFlags |= AIM_CAMP;

   if (m_hasC4) // we're still got the C4?
   {
      SelectWeaponByName ("weapon_c4");

      if (IsAlive (m_enemy) || !m_inBombZone)
         TaskComplete ();
      else
      {
         m_moveToGoal = false;
         m_checkTerrain = false;
         m_navTimeset = engine.Time ();

         if (m_currentPath->flags & FLAG_CROUCH)
            pev->button |= (IN_ATTACK | IN_DUCK);
         else
            pev->button |= IN_ATTACK;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
      }
   }
   else // done with planting
   {
      TaskComplete ();

      // tell teammates to move over here...
      if (GetNearbyFriendsNearPosition (pev->origin, 1200.0f) != 0)
         RadioMessage (Radio_NeedBackup);

      DeleteSearchNodes ();
      int index = FindDefendWaypoint (pev->origin);

      float guardTime = mp_c4timer.GetFloat () * 0.5f + mp_c4timer.GetFloat () * 0.25f;

      // push camp task on to stack
      PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + guardTime, true);
      // push move command
      PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.Time () + guardTime, true);

      if (waypoints.GetPath (index)->vis.crouch <= waypoints.GetPath (index)->vis.stand)
         m_campButtons |= IN_DUCK;
      else
         m_campButtons &= ~IN_DUCK;
   }
}

void Bot::RunTask_DefuseBomb (void)
{
   float fullDefuseTime = m_hasDefuser ? 7.0f : 12.0f;
   float timeToBlowUp = GetBombTimeleft ();
   float defuseRemainingTime = fullDefuseTime;

   if (m_hasProgressBar /*&& IsOnFloor ()*/)
      defuseRemainingTime = fullDefuseTime - engine.Time ();

   bool defuseError = false;

   // exception: bomb has been defused
   if (waypoints.GetBombPosition ().IsZero ())
   {
      defuseError = true;
      g_bombPlanted = false;

      if (Random.Long (0, 100) < 50 && m_numFriendsLeft != 0)
      {
         if (timeToBlowUp <= 3.0)
         {
            if (yb_communication_type.GetInt () == 2)
               InstantChatterMessage (Chatter_BarelyDefused);
            else if (yb_communication_type.GetInt () == 1)
               RadioMessage (Radio_SectorClear);
         }
         else
            RadioMessage (Radio_SectorClear);
      }
   }
   else if (defuseRemainingTime > timeToBlowUp) // exception: not time left for defusing
      defuseError = true;
   else if (IsValidPlayer (m_enemy))
   {
      int friends = GetNearbyFriendsNearPosition (pev->origin, 768.0f);

      if (friends < 2 && defuseRemainingTime < timeToBlowUp)
      {
         defuseError = true;

         if (defuseRemainingTime + 2.0f > timeToBlowUp)
            defuseError = false;

         if (m_numFriendsLeft > friends)
            RadioMessage (Radio_NeedBackup);
      }
   }

   // one of exceptions is thrown. finish task.
   if (defuseError)
   {
      m_checkTerrain = true;
      m_moveToGoal = true;

      m_destOrigin.Zero ();
      m_entity.Zero ();

      m_pickupItem = NULL;
      m_pickupType = PICKUP_NONE;

      TaskComplete ();
      return;
   }

   // to revert from pause after reload waiting && just to be sure
   m_moveToGoal = false;
   m_checkTerrain = true;

   m_moveSpeed = pev->maxspeed;
   m_strafeSpeed = 0.0f;

   // bot is reloading and we close enough to start defusing
   if (m_isReloading && (waypoints.GetBombPosition () - pev->origin).GetLength2D () < 80.0f)
   {
      if (m_numEnemiesLeft == 0 || timeToBlowUp < fullDefuseTime + 7.0f || ((GetAmmoInClip () > 8 && m_reloadState == RELOAD_PRIMARY) || (GetAmmoInClip () > 5 && m_reloadState == RELOAD_SECONDARY)))
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

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
      }
   }

   // head to bomb and press use button
   m_aimFlags |= AIM_ENTITY;

   m_destOrigin = waypoints.GetBombPosition ();
   m_entity = waypoints.GetBombPosition ();

   pev->button |= IN_USE;

   // if defusing is not already started, maybe crouch before
   if (!m_hasProgressBar && m_duckDefuseCheckTime < engine.Time ())
   {
      if (m_difficulty >= 2 && m_numEnemiesLeft != 0)
         m_duckDefuse = true;

      Vector botDuckOrigin, botStandOrigin;

      if (pev->button & IN_DUCK)
      {
         botDuckOrigin = pev->origin;
         botStandOrigin = pev->origin + Vector (0.0f, 0.0f, 18.0f);
      }
      else
      {
         botDuckOrigin = pev->origin - Vector (0.0f, 0.0f, 18.0f);
         botStandOrigin = pev->origin;
      }

      float duckLength = (m_entity - botDuckOrigin).GetLengthSquared ();
      float standLength = (m_entity - botStandOrigin).GetLengthSquared ();

      if (duckLength > 5625.0f || standLength > 5625.0f)
      {
         if (standLength < duckLength)
            m_duckDefuse = false; // stand
         else
            m_duckDefuse = true; // duck
      }
      m_duckDefuseCheckTime = engine.Time () + 1.5f;
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
      m_navTimeset = engine.Time ();

      // don't move when defusing
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      // notify team
      if (m_numFriendsLeft != 0)
      {
         ChatterMessage (Chatter_DefusingC4);

         if (GetNearbyFriendsNearPosition (pev->origin, 512.0f) < 2)
            RadioMessage (Radio_NeedBackup);
      }
   }
}

void Bot::RunTask_FollowUser (void)
{
   if (engine.IsNullEntity (m_targetEntity) || !IsAlive (m_targetEntity))
   {
      m_targetEntity = NULL;
      TaskComplete ();

      return;
   }

   if (m_targetEntity->v.button & IN_ATTACK)
   {
      MakeVectors (m_targetEntity->v.v_angle);

      TraceResult tr;
      engine.TestLine (m_targetEntity->v.origin + m_targetEntity->v.view_ofs, g_pGlobals->v_forward * 500.0f, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

      if (!engine.IsNullEntity (tr.pHit) && IsValidPlayer (tr.pHit) && engine.GetTeam (tr.pHit) != m_team)
      {
         m_targetEntity = NULL;
         m_lastEnemy = tr.pHit;
         m_lastEnemyOrigin = tr.pHit->v.origin;

         TaskComplete ();
         return;
      }
   }

   if (m_targetEntity->v.maxspeed != 0 && m_targetEntity->v.maxspeed < pev->maxspeed)
      m_moveSpeed = m_targetEntity->v.maxspeed;

   if (m_reloadState == RELOAD_NONE && GetAmmo () != 0)
      m_reloadState = RELOAD_PRIMARY;

   if ((m_targetEntity->v.origin - pev->origin).GetLength () > 130)
      m_followWaitTime = 0.0f;
   else
   {
      m_moveSpeed = 0.0f;

      if (m_followWaitTime == 0.0f)
         m_followWaitTime = engine.Time ();
      else
      {
         if (m_followWaitTime + 3.0f < engine.Time ())
         {
            // stop following if we have been waiting too long
            m_targetEntity = NULL;

            RadioMessage (Radio_YouTakePoint);
            TaskComplete ();

            return;
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

      int destIndex = waypoints.FindNearest (m_targetEntity->v.origin);

      Array <int> points;
      waypoints.FindInRadius (points, 200.0f, m_targetEntity->v.origin);

      while (!points.IsEmpty ())
      {
         int newIndex = points.Pop ();

         // if waypoint not yet used, assign it as dest
         if (!IsPointOccupied (newIndex) && newIndex != m_currentWaypointIndex)
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
}

void Bot::RunTask_Throw_HE (void)
{
   m_aimFlags |= AIM_GRENADE;
   Vector dest = m_throw;

   if (!(m_states & STATE_SEEING_ENEMY))
   {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }
   else if (!(m_states & STATE_SUSPECT_ENEMY) && !engine.IsNullEntity (m_enemy))
      dest = m_enemy->v.origin + m_enemy->v.velocity.Get2D () * 0.5f;

   m_isUsingGrenade = true;
   m_checkTerrain = false;

   IgnoreCollisionShortly ();

   if (m_maxThrowTimer < engine.Time () || (pev->origin - dest).GetLengthSquared () < GET_SQUARE (400.0f))
   {
      // heck, I don't wanna blow up myself
      m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;

      SelectBestWeapon ();
      TaskComplete ();

      return;
   }
   m_grenade = CheckThrow (EyePosition (), dest);

   if (m_grenade.GetLengthSquared () < 100.0f)
      m_grenade = CheckToss (EyePosition (), dest);

   if (m_grenade.GetLengthSquared () <= 100.0f)
   {
      m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;
      m_grenade = m_lookAt;

      SelectBestWeapon ();
      TaskComplete ();
   }
   else
   {
      edict_t *ent = NULL;

      while (!engine.IsNullEntity (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
      {
         if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "hegrenade.mdl") == 0)
         {
            // set the correct velocity for the grenade
            if (m_grenade.GetLengthSquared () > 100.0f)
               ent->v.velocity = m_grenade;

            m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;

            SelectBestWeapon ();
            TaskComplete ();

            break;
         }
      }

      if (engine.IsNullEntity (ent))
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
}

void Bot::RunTask_Throw_FL (void)
{
   m_aimFlags |= AIM_GRENADE;
   Vector dest = m_throw;

   if (!(m_states & STATE_SEEING_ENEMY))
   {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
   }
   else if (!(m_states & STATE_SUSPECT_ENEMY) && !engine.IsNullEntity (m_enemy))
      dest = m_enemy->v.origin + m_enemy->v.velocity.Get2D () * 0.5;

   m_isUsingGrenade = true;
   m_checkTerrain = false;

   IgnoreCollisionShortly ();

   m_grenade = CheckThrow (EyePosition (), dest);

   if (m_grenade.GetLengthSquared () < 100.0f)
      m_grenade = CheckToss (pev->origin, dest);

   if (m_maxThrowTimer < engine.Time () || m_grenade.GetLengthSquared () <= 100.0f)
   {
      m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;
      m_grenade = m_lookAt;

      SelectBestWeapon ();
      TaskComplete ();
   }
   else
   {
      edict_t *ent = NULL;
      while (!engine.IsNullEntity (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
      {
         if (ent->v.owner == GetEntity () && strcmp (STRING (ent->v.model) + 9, "flashbang.mdl") == 0)
         {
            // set the correct velocity for the grenade
            if (m_grenade.GetLengthSquared () > 100.0f)
               ent->v.velocity = m_grenade;

            m_grenadeCheckTime = engine.Time () + MAX_GRENADE_TIMER;

            SelectBestWeapon ();
            TaskComplete ();
            break;
         }
      }

      if (engine.IsNullEntity (ent))
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
}

void Bot::RunTask_Throw_SG (void)
{
   m_aimFlags |= AIM_GRENADE;

   if (!(m_states & STATE_SEEING_ENEMY))
   {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
   }

   m_checkTerrain = false;
   m_isUsingGrenade = true;

   IgnoreCollisionShortly ();

   Vector src = m_lastEnemyOrigin - pev->velocity;

   // predict where the enemy is in 0.5 secs
   if (!engine.IsNullEntity (m_enemy))
      src = src + m_enemy->v.velocity * 0.5f;

   m_grenade = (src - EyePosition ()).Normalize ();

   if (m_maxThrowTimer < engine.Time () || GetTask ()->time < engine.Time () + 0.5f)
   {
      m_aimFlags &= ~AIM_GRENADE;
      m_states &= ~STATE_THROW_SG;

      TaskComplete ();
      return;
   }

   if (m_currentWeapon != WEAPON_SMOKE)
   {
      if (pev->weapons & (1 << WEAPON_SMOKE))
      {
         SelectWeaponByName ("weapon_smokegrenade");
         GetTask ()->time = engine.Time () + MAX_GRENADE_TIMER;
      }
      else
         GetTask ()->time = engine.Time () + 0.1f;
   }
   else if (!(pev->oldbuttons & IN_ATTACK))
      pev->button |= IN_ATTACK;
}

void Bot::RunTask_DoubleJump (void)
{
   if (!IsAlive (m_doubleJumpEntity) || (m_aimFlags & AIM_ENEMY) || (m_travelStartIndex != -1 && GetTask ()->time + (waypoints.GetTravelTime (pev->maxspeed, waypoints.GetPath (m_travelStartIndex)->origin, m_doubleJumpOrigin) + 11.0) < engine.Time ()))
   {
      ResetDoubleJumpState ();
      return;
   }
   m_aimFlags |= AIM_NAVPOINT;

   if (m_jumpReady)
   {
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = engine.Time ();
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      if (m_duckForJump < engine.Time ())
         pev->button |= IN_DUCK;

      MakeVectors (Vector::GetZero ());

      Vector dest = EyePosition () + g_pGlobals->v_forward * 500.0f;
      dest.z = 180.0f;

      TraceResult tr;
      engine.TestLine (EyePosition (), dest, TRACE_IGNORE_GLASS, GetEntity (), &tr);

      if (tr.flFraction < 1.0f && tr.pHit == m_doubleJumpEntity)
      {
         if (m_doubleJumpEntity->v.button & IN_JUMP)
         {
            m_duckForJump = engine.Time () + Random.Float (3.0f, 5.0f);
            GetTask ()->time = engine.Time ();
         }
      }
      return;
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

      int destIndex = waypoints.FindNearest (m_doubleJumpOrigin);

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
}

void Bot::RunTask_EscapeFromBomb (void)
{
   m_aimFlags |= AIM_NAVPOINT;

   if (!g_bombPlanted)
      TaskComplete ();

   if (IsShieldDrawn ())
      pev->button |= IN_ATTACK2;

   if (m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0)
      SelectWeaponByName ("weapon_knife");

   if (DoWaypointNav ()) // reached destination?
   {
      TaskComplete (); // we're done

      // press duck button if we still have some enemies
      if (GetNearbyEnemiesNearPosition (pev->origin, 2048.0f))
         m_campButtons = IN_DUCK;

      // we're reached destination point so just sit down and camp
      PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + 10.0f, true);
   }
   else if (!GoalIsValid ()) // didn't choose goal waypoint yet?
   {
      DeleteSearchNodes ();

      int lastSelectedGoal = -1, minPathDistance = 99999;
      float safeRadius = Random.Float (1248.0f, 2048.0f);

      for (int i = 0; i < g_numWaypoints; i++)
      {
         if ((waypoints.GetPath (i)->origin - waypoints.GetBombPosition ()).GetLength () < safeRadius || IsPointOccupied (i))
            continue;

         int pathDistance = waypoints.GetPathDistance (m_currentWaypointIndex, i);

         if (minPathDistance > pathDistance)
         {
            minPathDistance = pathDistance;
            lastSelectedGoal = i;
         }
      }

      if (lastSelectedGoal < 0)
         lastSelectedGoal = waypoints.FindFarest (pev->origin, safeRadius);

      m_prevGoalIndex = lastSelectedGoal;
      GetTask ()->data = lastSelectedGoal;

      FindShortestPath (m_currentWaypointIndex, lastSelectedGoal);
   }
}

void Bot::RunTask_ShootBreakable (void)
{
   m_aimFlags |= AIM_OVERRIDE;

   // Breakable destroyed?
   if (engine.IsNullEntity (FindBreakable ()))
   {
      TaskComplete ();
      return;
   }
   pev->button |= m_campButtons;

   m_checkTerrain = false;
   m_moveToGoal = false;
   m_navTimeset = engine.Time ();

   Vector src = m_breakable;
   m_camp = src;

   // is bot facing the breakable?
   if (GetShootingConeDeviation (GetEntity (), &src) >= 0.90f)
   {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      if (m_currentWeapon == WEAPON_KNIFE)
         SelectBestWeapon ();

      m_wantsToFire = true;
   }
   else
   {
      m_checkTerrain = true;
      m_moveToGoal = true;
   }
}

void Bot::RunTask_PickupItem ()
{
   if (engine.IsNullEntity (m_pickupItem))
   {
      m_pickupItem = NULL;
      TaskComplete ();

      return;
   }
   Vector dest = engine.GetAbsOrigin (m_pickupItem);

   m_destOrigin = dest;
   m_entity = dest;

   // find the distance to the item
   float itemDistance = (dest - pev->origin).GetLength ();

   switch (m_pickupType)
   {
   case PICKUP_DROPPED_C4:
   case PICKUP_NONE:
      break;

   case PICKUP_WEAPON:
      m_aimFlags |= AIM_NAVPOINT;

      // near to weapon?
      if (itemDistance < 50.0f)
      {
         int id = 0;
         
         for (id = 0; id < 7; id++)
         {
            if (strcmp (g_weaponSelect[id].modelName, STRING (m_pickupItem->v.model) + 9) == 0)
               break;
         }

         if (id < 7)
         {
            // secondary weapon. i.e., pistol
            int wid = 0;

            for (id = 0; id < 7; id++)
            {
               if (pev->weapons & (1 << g_weaponSelect[id].id))
                  wid = id;
            }

            if (wid > 0)
            {
               SelectWeaponbyNumber (wid);
               engine.IssueBotCommand (GetEntity (), "drop");

               if (HasShield ()) // If we have the shield...
                  engine.IssueBotCommand (GetEntity (), "drop"); // discard both shield and pistol
            }
            EquipInBuyzone (BUYSTATE_PRIMARY_WEAPON);
         }
         else
         {
            // primary weapon
            int wid = GetHighestWeapon ();

            if (wid > 6 || HasShield ())
            {
               SelectWeaponbyNumber (wid);
               engine.IssueBotCommand (GetEntity (), "drop");
            }
            EquipInBuyzone (BUYSTATE_PRIMARY_WEAPON);
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
      else if (itemDistance < 50.0f) // near to shield?
      {
         // get current best weapon to check if it's a primary in need to be dropped
         int wid = GetHighestWeapon ();

         if (wid > 6)
         {
            SelectWeaponbyNumber (wid);
            engine.IssueBotCommand (GetEntity (), "drop");
         }
      }
      break;

   case PICKUP_PLANTED_C4:
      m_aimFlags |= AIM_ENTITY;

      if (m_team == CT && itemDistance < 80.0f)
      {
         ChatterMessage (Chatter_DefusingC4);

         // notify team of defusing
         if (m_numFriendsLeft < 3)
            RadioMessage (Radio_NeedBackup);

         m_moveToGoal = false;
         m_checkTerrain = false;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         PushTask (TASK_DEFUSEBOMB, TASKPRI_DEFUSEBOMB, -1, 0.0f, false);
      }
      break;

   case PICKUP_HOSTAGE:
      m_aimFlags |= AIM_ENTITY;

      if (!IsAlive (m_pickupItem))
      {
         // don't pickup dead hostages
         m_pickupItem = NULL;
         TaskComplete ();

         break;
      }

      if (itemDistance < 50.0f)
      {
         float angleToEntity = InFieldOfView (dest - EyePosition ());

         if (angleToEntity <= 10.0f) // bot faces hostage?
         {
            // use game dll function to make sure the hostage is correctly 'used'
            MDLL_Use (m_pickupItem, GetEntity ());

            if (Random.Long (0, 100) < 80)
               ChatterMessage (Chatter_UseHostage);

            for (int i = 0; i < MAX_HOSTAGES; i++)
            {
               if (engine.IsNullEntity (m_hostages[i])) // store pointer to hostage so other bots don't steal from this one or bot tries to reuse it
               {
                  m_hostages[i] = m_pickupItem;
                  m_pickupItem = NULL;

                  break;
               }
            }
         }
         IgnoreCollisionShortly (); // also don't consider being stuck
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

      if (engine.IsNullEntity (m_pickupItem) || m_buttonPushTime < engine.Time ()) // it's safer...
      {
         TaskComplete ();
         m_pickupType = PICKUP_NONE;

         break;
      }

      // find angles from bot origin to entity...
      float angleToEntity = InFieldOfView (dest - EyePosition ());

      if (itemDistance < 90.0f) // near to the button?
      {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
         m_moveToGoal = false;
         m_checkTerrain = false;

         if (angleToEntity <= 10.0f) // facing it directly?
         {
            MDLL_Use (m_pickupItem, GetEntity ());

            m_pickupItem = NULL;
            m_pickupType = PICKUP_NONE;
            m_buttonPushTime = engine.Time () + 3.0f;

            TaskComplete ();
         }
      }
      break;
   }
}

void Bot::RunTask (void)
{
   // this is core function that handle task execution

   switch (GetTaskId ())
   {
   // normal task
   default:
   case TASK_NORMAL:
      RunTask_Normal ();
      break;

   // bot sprays messy logos all over the place...
   case TASK_SPRAY:
      RunTask_Spray ();
      break;

   // hunt down enemy
   case TASK_HUNTENEMY:
      RunTask_HuntEnemy ();
      break;

   // bot seeks cover from enemy
   case TASK_SEEKCOVER:
      RunTask_SeekCover ();
      break;

   // plain attacking
   case TASK_ATTACK:
      RunTask_Attack ();
      break;

   // Bot is pausing
   case TASK_PAUSE:
      RunTask_Pause ();
      break;

   // blinded (flashbanged) behaviour
   case TASK_BLINDED:
      RunTask_Blinded ();
      break;

   // camping behaviour
   case TASK_CAMP:
      RunTask_Camp ();
      break;

   // hiding behaviour
   case TASK_HIDE:
      RunTask_Hide ();
      break;

   // moves to a position specified in position has a higher priority than task_normal
   case TASK_MOVETOPOSITION:
      RunTask_MoveToPos ();
      break;

   // planting the bomb right now
   case TASK_PLANTBOMB:
      RunTask_PlantBomb ();
      break;

   // bomb defusing behaviour
   case TASK_DEFUSEBOMB:
      RunTask_DefuseBomb ();
      break;
      
   // follow user behaviour
   case TASK_FOLLOWUSER:
      RunTask_FollowUser ();
      break;

   // HE grenade throw behaviour
   case TASK_THROWHEGRENADE:
      RunTask_Throw_HE ();
      break;

   // flashbang throw behavior (basically the same code like for HE's)
   case TASK_THROWFLASHBANG:
      RunTask_Throw_FL ();
      break;

   // smoke grenade throw behavior
   // a bit different to the others because it mostly tries to throw the sg on the ground
   case TASK_THROWSMOKE:
      RunTask_Throw_SG ();
      break;

   // bot helps human player (or other bot) to get somewhere
   case TASK_DOUBLEJUMP:
      RunTask_DoubleJump ();
      break;

   // escape from bomb behaviour
   case TASK_ESCAPEFROMBOMB:
      RunTask_EscapeFromBomb ();
      break;

   // shooting breakables in the way action
   case TASK_SHOOTBREAKABLE:
      RunTask_ShootBreakable ();
      break;

   // picking up items and stuff behaviour
   case TASK_PICKUPITEM:
      RunTask_PickupItem ();
      break;
   }
}

void Bot::CheckSpawnTimeConditions (void)
{
   // this function is called instead of BotAI when buying finished, but freezetime is not yet left.

   // switch to knife if time to do this
   if (m_checkKnifeSwitch && !m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + Random.Float (4.0f, 6.5f) < engine.Time ())
   {
      if (Random.Long (1, 100) < 2 && yb_spraypaints.GetBool ())
         PushTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, -1, engine.Time () + 1.0f, false);

      if (m_difficulty >= 2 && Random.Long (0, 100) < (m_personality == PERSONALITY_RUSHER ? 99 : 50) && !m_isReloading && (g_mapType & (MAP_CS | MAP_DE | MAP_ES | MAP_AS)))
      {
         if (yb_jasonmode.GetBool ())
         {
            SelectPistol ();
            engine.IssueBotCommand (GetEntity (), "drop");
         }
         else
            SelectWeaponByName ("weapon_knife");
      }
      m_checkKnifeSwitch = false;

      if (Random.Long (0, 100) < yb_user_follow_percent.GetInt () && engine.IsNullEntity (m_targetEntity) && !m_isLeader && !m_hasC4)
         AttachToUser ();
   }

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + Random.Float (2.0f, 3.5f) < engine.Time ())
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

   float movedDistance = 2.0f; // length of different vector (distance bot moved)

   // increase reaction time
   m_actualReactionTime += 0.3f;

   if (m_actualReactionTime > m_idealReactionTime)
      m_actualReactionTime = m_idealReactionTime;

   // bot could be blinded by flashbang or smoke, recover from it
   m_viewDistance += 3.0f;

   if (m_viewDistance > m_maxViewDistance)
      m_viewDistance = m_maxViewDistance;

   if (m_blindTime > engine.Time ())
      m_maxViewDistance = 4096.0f;        

   m_moveSpeed = pev->maxspeed;

   if (m_prevTime <= engine.Time ())
   {
      // see how far bot has moved since the previous position...
      movedDistance = (m_prevOrigin - pev->origin).GetLength ();

      // save current position as previous
      m_prevOrigin = pev->origin;
      m_prevTime = engine.Time () + 0.2f;
   }

   // if there's some radio message to respond, check it
   if (m_radioOrder != 0)
      CheckRadioCommands ();

   // do all sensing, calculate/filter all actions here
   SetConditions ();

   // some stuff required by by chatter engine
   if (yb_communication_type.GetInt () == 2)
   {
      if ((m_states & STATE_SEEING_ENEMY) && !engine.IsNullEntity (m_enemy))
      {
         int hasFriendNearby = GetNearbyFriendsNearPosition (pev->origin, 512.0f);

         if (!hasFriendNearby && Random.Long (0, 100) < 45 && (m_enemy->v.weapons & (1 << WEAPON_C4)))
            ChatterMessage (Chatter_SpotTheBomber);

         else if (!hasFriendNearby && Random.Long (0, 100) < 45 && m_team == TERRORIST && IsPlayerVIP (m_enemy))
            ChatterMessage (Chatter_VIPSpotted);

         else if (!hasFriendNearby && Random.Long (0, 100) < 50 && engine.GetTeam (m_enemy) != m_team && IsGroupOfEnemies (m_enemy->v.origin, 2, 384.0f))
            ChatterMessage (Chatter_ScaredEmotion);

         else if (!hasFriendNearby && Random.Long (0, 100) < 40 && ((m_enemy->v.weapons & (1 << WEAPON_AWP)) || (m_enemy->v.weapons & (1 << WEAPON_SCOUT)) || (m_enemy->v.weapons & (1 << WEAPON_G3SG1)) || (m_enemy->v.weapons & (1 << WEAPON_SG550))))
            ChatterMessage (Chatter_SniperWarning);

         // if bot is trapped under shield yell for help !
         if (GetTaskId () == TASK_CAMP && HasShield () && IsShieldDrawn () && hasFriendNearby >= 2 && IsEnemyViewable (m_enemy))
            InstantChatterMessage (Chatter_Pinned_Down);
      }

      // if bomb planted warn teammates !
      if (g_canSayBombPlanted && g_bombPlanted && engine.GetTeam (GetEntity ()) == CT)
      {
         g_canSayBombPlanted = false;
         ChatterMessage (Chatter_GottaFindTheBomb);
      }
   }
   Vector src, destination;

   m_checkTerrain = true;
   m_moveToGoal = true;
   m_wantsToFire = false;

   AvoidGrenades (); // avoid flyings grenades
   m_isUsingGrenade = false;

   RunTask (); // execute current task
   ChooseAimDirection (); // choose aim direction
   UpdateLookAngles (); // and turn to chosen aim direction

   // the bots wants to fire at something?
   if (m_wantsToFire && !m_isUsingGrenade && m_shootTime <= engine.Time ())
      FireWeapon (); // if bot didn't fire a bullet try again next frame

   // check for reloading
   if (m_reloadCheckTime <= engine.Time ())
      CheckReload ();

   // set the reaction time (surprise momentum) different each frame according to skill
   SetIdealReactionTimes ();

   // calculate 2 direction vectors, 1 without the up/down component
   const Vector &dirOld = m_destOrigin - (pev->origin + pev->velocity * m_frameInterval);
   const Vector &dirNormal = dirOld.Normalize2D ();

   m_moveAngles = dirOld.ToAngles ();

   m_moveAngles.ClampAngles ();
   m_moveAngles.x *= -1.0f; // invert for engine

   SetConditionsOverride ();

   // allowed to move to a destination position?
   if (m_moveToGoal)
   {
      GetValidWaypoint ();

      // Press duck button if we need to
      if ((m_currentPath->flags & FLAG_CROUCH) && !(m_currentPath->flags & FLAG_CAMP))
         pev->button |= IN_DUCK;

      m_timeWaypointMove = engine.Time ();

      if (IsInWater ()) // special movement for swimming here
      {
         // check if we need to go forward or back press the correct buttons
         if (InFieldOfView (m_destOrigin - EyePosition ()) > 90.0f)
            pev->button |= IN_BACK;
         else
            pev->button |= IN_FORWARD;

         if (m_moveAngles.x > 60.0f)
            pev->button |= IN_DUCK;
         else if (m_moveAngles.x < -60.0f)
            pev->button |= IN_JUMP;
      }
   }

   if (m_checkTerrain) // are we allowed to check blocking terrain (and react to it)?
      CheckTerrain (movedDistance, dirNormal);

   // must avoid a grenade?
   if (m_needAvoidGrenade != 0)
   {
      // Don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * m_needAvoidGrenade;
   }

   // time to reach waypoint
   if (m_navTimeset + GetEstimatedReachTime () < engine.Time () && engine.IsNullEntity (m_enemy))
   {
      GetValidWaypoint ();

      // clear these pointers, bot mingh be stuck getting to them
      if (!engine.IsNullEntity (m_pickupItem) && !m_hasProgressBar)
         m_itemIgnore = m_pickupItem;

      m_pickupItem = NULL;
      m_breakableEntity = NULL;
      m_itemCheckTime = engine.Time () + 5.0f;
      m_pickupType = PICKUP_NONE;
   }

   if (m_duckTime >= engine.Time ())
      pev->button |= IN_DUCK;

   if (pev->button & IN_JUMP)
      m_jumpTime = engine.Time ();

   if (m_jumpTime + 0.85f > engine.Time ())
   {
      if (!IsOnFloor () && !IsInWater ())
         pev->button |= IN_DUCK;
   }

   if (!(pev->button & (IN_FORWARD | IN_BACK)))
   {
      if (m_moveSpeed > 0.0f)
         pev->button |= IN_FORWARD;
      else if (m_moveSpeed < 0.0f)
         pev->button |= IN_BACK;
   }

   if (!(pev->button & (IN_MOVELEFT | IN_MOVERIGHT)))
   {
      if (m_strafeSpeed > 0.0f)
         pev->button |= IN_MOVERIGHT;
      else if (m_strafeSpeed < 0.0f)
         pev->button |= IN_MOVELEFT;
   }

   // display some debugging thingy to host entity
   if (!engine.IsNullEntity (g_hostEntity) && yb_debug.GetInt () >= 1)
      DisplayDebugOverlay ();
   
   // save the previous speed (for checking if stuck)
   m_prevSpeed = fabsf (m_moveSpeed);
   m_lastDamageType = -1; // reset damage
}

void Bot::DisplayDebugOverlay (void)
{
   bool displayDebugOverlay = false;

   if (g_hostEntity->v.iuser2 == engine.IndexOfEntity (GetEntity ()))
      displayDebugOverlay = true;

   if (!displayDebugOverlay && yb_debug.GetInt () >= 2)
   {
      Bot *nearest = NULL;

      if (FindNearestPlayer (reinterpret_cast <void **> (&nearest), g_hostEntity, 128.0f, true, true, true, true) && nearest == this)
         displayDebugOverlay = true;
   }

   if (displayDebugOverlay)
   {
      static float timeDebugUpdate = 0.0f;
      static int index, goal, taskID;

      if (!m_tasks.IsEmpty ())
      {
         if (taskID != GetTaskId () || index != m_currentWaypointIndex || goal != GetTask ()->data || timeDebugUpdate < engine.Time ())
         {
            taskID = GetTaskId ();
            index = m_currentWaypointIndex;
            goal = GetTask ()->data;

            char taskName[80];
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

            char enemyName[80], weaponName[80], aimFlags[64], botType[32];

            if (!engine.IsNullEntity (m_enemy))
               strncpy (enemyName, STRING (m_enemy->v.netname), SIZEOF_CHAR (enemyName));
            else if (!engine.IsNullEntity (m_lastEnemy))
            {
               strcpy (enemyName, " (L)");
               strncat (enemyName, STRING (m_lastEnemy->v.netname), SIZEOF_CHAR (enemyName));
            }
            else
               strcpy (enemyName, " (null)");

            char pickupName[80];
            memset (pickupName, 0, sizeof (pickupName));

            if (!engine.IsNullEntity (m_pickupItem))
               strncpy (pickupName, STRING (m_pickupItem->v.classname), SIZEOF_CHAR (pickupName));
            else
               strcpy (pickupName, " (null)");

            WeaponSelect *tab = &g_weaponSelect[0];
            char weaponCount = 0;

            while (m_currentWeapon != tab->id && weaponCount < NUM_WEAPONS)
            {
               tab++;
               weaponCount++;
            }
            memset (aimFlags, 0, sizeof (aimFlags));

            // set the aim flags
            sprintf (aimFlags, "%s%s%s%s%s%s%s%s",
               (m_aimFlags & AIM_NAVPOINT) ? " NavPoint" : "",
               (m_aimFlags & AIM_CAMP) ? " CampPoint" : "",
               (m_aimFlags & AIM_PREDICT_PATH) ? " PredictPath" : "",
               (m_aimFlags & AIM_LAST_ENEMY) ? " LastEnemy" : "",
               (m_aimFlags & AIM_ENTITY) ? " Entity" : "",
               (m_aimFlags & AIM_ENEMY) ? " Enemy" : "",
               (m_aimFlags & AIM_GRENADE) ? " Grenade" : "",
               (m_aimFlags & AIM_OVERRIDE) ? " Override" : "");

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
               strncpy (weaponName, tab->weaponName, SIZEOF_CHAR (weaponName));

            char outputBuffer[512];
            memset (outputBuffer, 0, sizeof (outputBuffer));

            sprintf (outputBuffer, "\n\n\n\n%s (H:%.1f/A:%.1f)- Task: %d=%s Desire:%.02f\nItem: %s Clip: %d Ammo: %d%s Money: %d AimFlags: %s\nSP=%.02f SSP=%.02f I=%d PG=%d G=%d T: %.02f MT: %d\nEnemy=%s Pickup=%s Type=%s\n", STRING (pev->netname), pev->health, pev->armorvalue, taskID, taskName, GetTask ()->desire, &weaponName[7], GetAmmoInClip (), GetAmmo (), m_isReloading ? " (R)" : "", m_moneyAmount, aimFlags, m_moveSpeed, m_strafeSpeed, index, m_prevGoalIndex, goal, m_navTimeset - engine.Time (), pev->movetype, enemyName, pickupName, botType);

            MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, g_hostEntity);
            WRITE_BYTE (TE_TEXTMESSAGE);
            WRITE_BYTE (1);
            WRITE_SHORT (FixedSigned16 (-1, 1 << 13));
            WRITE_SHORT (FixedSigned16 (0, 1 << 13));
            WRITE_BYTE (0);
            WRITE_BYTE (m_team == CT ? 0 : 255);
            WRITE_BYTE (100);
            WRITE_BYTE (m_team != CT ? 0 : 255);
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

            timeDebugUpdate = engine.Time () + 1.0;
         }

         // green = destination origin
         // blue = ideal angles
         // red = view angles

         engine.DrawLine (g_hostEntity, EyePosition (), m_destOrigin, 10, 0, 0, 255, 0, 250, 5, 1, DRAW_ARROW);

         MakeVectors (m_idealAngles);
         engine.DrawLine (g_hostEntity, EyePosition () - Vector (0.0f, 0.0f, 16.0f), EyePosition () + g_pGlobals->v_forward * 300.0f, 10, 0, 0, 0, 255, 250, 5, 1, DRAW_ARROW);

         MakeVectors (pev->v_angle);
         engine.DrawLine (g_hostEntity, EyePosition () - Vector (0.0f, 0.0f, 32.0f), EyePosition () + g_pGlobals->v_forward * 300.0f, 10, 0, 255, 0, 0, 250, 5, 1, DRAW_ARROW);

         // now draw line from source to destination
         PathNode *node = &m_navNode[0];

         while (node != NULL)
         {
            const Vector &srcPath = waypoints.GetPath (node->index)->origin;
            node = node->next;

            if (node != NULL)
            {
               const Vector &dstPath = waypoints.GetPath (node->index)->origin;
               engine.DrawLine (g_hostEntity, srcPath, dstPath, 15, 0, 255, 100, 55, 200, 5, 1, DRAW_ARROW);
            }
         }
      }
   }
}

bool Bot::HasHostage (void)
{
   for (int i = 0; i < MAX_HOSTAGES; i++)
   {
      if (!engine.IsNullEntity (m_hostages[i]))
      {
         // don't care about dead hostages
         if (m_hostages[i]->v.health <= 0.0f || (pev->origin - m_hostages[i]->v.origin).GetLength () > 600.0f)
         {
            m_hostages[i] = NULL;
            continue;
         }
         return true;
      }
   }
   return false;
}

int Bot::GetAmmo (void)
{
   if (g_weaponDefs[m_currentWeapon].ammo1 == -1 || g_weaponDefs[m_currentWeapon].ammo1 > 31)
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
      if (yb_tkpunish.GetBool () && engine.GetTeam (inflictor) == m_team && !IsValidBot (inflictor))
      {
         // alright, die you teamkiller!!!
         m_actualReactionTime = 0.0f;
         m_seeEnemyTime = engine.Time();
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
         if (pev->health > 60.0f)
         {
            m_agressionLevel += 0.1f;

            if (m_agressionLevel > 1.0f)
               m_agressionLevel += 1.0f;
         }
         else
         {
            m_fearLevel += 0.03f;

            if (m_fearLevel > 1.0f)
               m_fearLevel += 1.0f;
         }
         RemoveCertainTask (TASK_CAMP);

         if (engine.IsNullEntity (m_enemy) && m_team != engine.GetTeam (inflictor))
         {
            m_lastEnemy = inflictor;
            m_lastEnemyOrigin = inflictor->v.origin;

            // FIXME - Bot doesn't necessary sees this enemy
            m_seeEnemyTime = engine.Time ();
         }

         if (yb_csdm_mode.GetInt () == 0)
            CollectExperienceData (inflictor, armor + damage);
      }
   }
   else // hurt by unusual damage like drowning or gas
   {
      // leave the camping/hiding position
      if (!waypoints.Reachable (this, waypoints.FindNearest (m_destOrigin)))
      {
         DeleteSearchNodes ();
         FindWaypoint ();
      }
   }
}

void Bot::TakeBlinded (const Vector &fade, int alpha)
{
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind from the grenade.

   if (fade.x != 255.0f || fade.y != 255.0f || fade.z != 255.0f || alpha <= 170.0f)
      return;

   m_enemy = NULL;

   m_maxViewDistance = Random.Float (10.0f, 20.0f);
   m_blindTime = engine.Time () + static_cast <float> (alpha - 200.0f) / 16.0f;

   if (m_difficulty <= 2)
   {
      m_blindMoveSpeed = 0.0f;
      m_blindSidemoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;

      return;
   }

   m_blindMoveSpeed = -pev->maxspeed;
   m_blindSidemoveSpeed = 0.0f;

   float walkSpeed = GetWalkSpeed ();

   if (Random.Long (0, 100) > 50)
      m_blindSidemoveSpeed = walkSpeed;
   else
      m_blindSidemoveSpeed = -walkSpeed;

   if (pev->health < 85.0f)
      m_blindMoveSpeed = -walkSpeed;
   else if (m_personality == PERSONALITY_CAREFUL)
   {
      m_blindMoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;
   }
   else
      m_blindMoveSpeed = walkSpeed;
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
      if (team == TERRORIST)
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

   int attackerTeam = engine.GetTeam (attacker);
   int victimTeam = m_team;

   if (attackerTeam == victimTeam )
      return;

   // if these are bots also remember damage to rank destination of the bot
   m_goalValue -= static_cast <float> (damage);

   if (bots.GetBot (attacker) != NULL)
      bots.GetBot (attacker)->m_goalValue += static_cast <float> (damage);

   if (damage < 20)
      return; // do not collect damage less than 20

   int attackerIndex = waypoints.FindNearest (attacker->v.origin);
   int victimIndex = waypoints.FindNearest (pev->origin);

   if (pev->health > 20.0f)
   {
      if (victimTeam == TERRORIST)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage++;
      else
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage++;

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage > MAX_DAMAGE_VALUE)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage = MAX_DAMAGE_VALUE;

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage > MAX_DAMAGE_VALUE)
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage = MAX_DAMAGE_VALUE;
   }

   float updateDamage = IsValidBot (attacker) ? 10.0f : 7.0f;

   // store away the damage done
   if (victimTeam == TERRORIST)
   {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage;
      value += static_cast <int> (damage / updateDamage);

      if (value > MAX_DAMAGE_VALUE)
         value = MAX_DAMAGE_VALUE;

      if (value > g_highestDamageT)
         g_highestDamageT = value;

      (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage = static_cast <unsigned short> (value);
   }
   else
   {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team1Damage;
      value += static_cast <int> (damage / updateDamage);

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

   if (FStrEq (tempMessage, "#CTs_Win") && m_team == CT)
   {
      if (g_timeRoundMid > engine.Time ())
         ChatterMessage (Chatter_QuicklyWonTheRound);
      else
         ChatterMessage (Chatter_WonTheRound);
   }

   if (FStrEq (tempMessage, "#Terrorists_Win") && m_team == TERRORIST)
   {
      if (g_timeRoundMid > engine.Time ())
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

   if (IsAlive (user) && m_moneyAmount >= 2000 && HasPrimaryWeapon () && (user->v.origin - pev->origin).GetLength () <= 240.0f)
   {
      m_aimFlags |= AIM_ENTITY;
      m_lookAt = user->v.origin;

      if (discardC4)
      {
         SelectWeaponByName ("weapon_c4");
         engine.IssueBotCommand (GetEntity (), "drop");
      }
      else
      {
         SelectBestWeapon ();
         engine.IssueBotCommand (GetEntity (), "drop");
      }

      m_pickupItem = NULL;
      m_pickupType = PICKUP_NONE;
      m_itemCheckTime = engine.Time () + 5.0f;

      if (m_inBuyZone)
      {
         m_buyingFinished = false;
         m_buyState = BUYSTATE_PRIMARY_WEAPON;

         PushMessageQueue (GSM_BUY_STUFF);
         m_nextBuyTime = engine.Time ();
      }
   }
}

void Bot::ResetDoubleJumpState (void)
{
   TaskComplete ();

   m_doubleJumpEntity = NULL;
   m_duckForJump = 0.0f;
   m_doubleJumpOrigin.Zero ();
   m_travelStartIndex = -1;
   m_jumpReady = false;
}

void Bot::DebugMsg (const char *format, ...)
{
   int level = yb_debug.GetInt ();

   if (level <= 2)
      return;

   va_list ap;
   char buffer[1024];

   va_start (ap, format);
   vsprintf (buffer, format, ap);
   va_end (ap);

   char printBuf[1024];
   sprintf (printBuf, "%s: %s", STRING (pev->netname), buffer);

   bool playMessage = false;

   if (level == 3 && !engine.IsNullEntity (g_hostEntity) && g_hostEntity->v.iuser2 == engine.IndexOfEntity (GetEntity ()))
      playMessage = true;
   else if (level != 3)
      playMessage = true;

   if (playMessage && level > 3)
      AddLogEntry (false, LL_DEFAULT, printBuf);

   if (playMessage)
   {
      engine.Printf (printBuf);
      SayText (printBuf);
   }
}

Vector Bot::CheckToss(const Vector &start, const Vector &stop)
{
   // this function returns the velocity at which an object should looped from start to land near end.
   // returns null vector if toss is not feasible.

   TraceResult tr;
   float gravity = sv_gravity.GetFloat () * 0.55f;

   Vector end = stop - pev->velocity;
   end.z -= 15.0f;

   if (fabsf (end.z - start.z) > 500.0f)
      return Vector::GetZero ();

   Vector midPoint = start + (end - start) * 0.5f;
   engine.TestHull (midPoint, midPoint + Vector (0.0f, 0.0f, 500.0f), TRACE_IGNORE_MONSTERS, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0f)
   {
      midPoint = tr.vecEndPos;
      midPoint.z = tr.pHit->v.absmin.z - 1.0f;
   }

   if ((midPoint.z < start.z) || (midPoint.z < end.z))
      return Vector::GetZero ();

   float timeOne = sqrtf ((midPoint.z - start.z) / (0.5f * gravity));
   float timeTwo = sqrtf ((midPoint.z - end.z) / (0.5f * gravity));

   if (timeOne < 0.1f)
      return Vector::GetZero ();

   Vector nadeVelocity = (end - start) / (timeOne + timeTwo);
   nadeVelocity.z = gravity * timeOne;

   Vector apex = start + nadeVelocity * timeOne;
   apex.z = midPoint.z;

   engine.TestHull (start, apex, TRACE_IGNORE_NONE, head_hull, ENT (pev), &tr);

   if (tr.flFraction < 1.0f || tr.fAllSolid)
      return Vector::GetZero ();

   engine.TestHull (end, apex, TRACE_IGNORE_MONSTERS, head_hull, ENT (pev), &tr);

   if (tr.flFraction != 1.0f)
   {
      float dot = -(tr.vecPlaneNormal | (apex - end).Normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) // 60 degrees
         return Vector::GetZero ();
   }
   return nadeVelocity * 0.777f;
}

Vector Bot::CheckThrow(const Vector &start, const Vector &stop)
{
   // this function returns the velocity vector at which an object should be thrown from start to hit end.
   // returns null vector if throw is not feasible.

   Vector nadeVelocity = (stop - start);
   TraceResult tr;

   float gravity = sv_gravity.GetFloat () * 0.55f;
   float time = nadeVelocity.GetLength () / 195.0f;

   if (time < 0.01f)
      return Vector::GetZero ();
   else if (time > 2.0f)
      time = 1.2f;

   nadeVelocity = nadeVelocity * (1.0f / time);
   nadeVelocity.z += gravity * time * 0.5f;

   Vector apex = start + (stop - start) * 0.5f;
   apex.z += 0.5f * gravity * (time * 0.5f) * (time * 0.5f);

   engine.TestHull (start, apex, TRACE_IGNORE_NONE, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0f)
      return Vector::GetZero ();

   engine.TestHull (stop, apex, TRACE_IGNORE_MONSTERS, head_hull, GetEntity (), &tr);

   if (tr.flFraction != 1.0 || tr.fAllSolid)
   {
      float dot = -(tr.vecPlaneNormal | (apex - stop).Normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f)
         return Vector::GetZero ();
   }
   return nadeVelocity * 0.7793f;
}

Vector Bot::CheckBombAudible (void)
{
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!g_bombPlanted || GetTaskId () == TASK_ESCAPEFROMBOMB)
      return Vector::GetZero (); // reliability check

   if (m_difficulty >= 3)
      return waypoints.GetBombPosition();

   const Vector &bombOrigin = waypoints.GetBombPosition ();
   
   float timeElapsed = ((engine.Time () - g_timeBombPlanted) / mp_c4timer.GetFloat ()) * 100.0f;
   float desiredRadius = 768.0f;

   // start the manual calculations
   if (timeElapsed > 85.0f)
      desiredRadius = 4096.0f;
   else if (timeElapsed > 68.0f)
      desiredRadius = 2048.0f;
   else if (timeElapsed > 52.0f)
      desiredRadius = 1280.0f;
   else if (timeElapsed > 28.0f)
      desiredRadius = 1024.0f;

   // we hear bomb if length greater than radius
   if (desiredRadius < (pev->origin - bombOrigin).GetLength2D ())
      return bombOrigin;

   return Vector::GetZero ();
}

void Bot::MoveToVector (const Vector &to)
{
   if (to.IsZero ())
      return;

   FindPath (m_currentWaypointIndex, waypoints.FindNearest (to), SEARCH_PATH_FASTEST);
}

byte Bot::ThrottledMsec (void)
{
   // estimate msec to use for this command based on time passed from the previous command

   return static_cast <byte> ((engine.Time () - m_lastCommandTime) * 1000.0f);
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

   m_frameInterval = engine.Time () - m_lastCommandTime;

   byte msecVal = ThrottledMsec ();
   m_lastCommandTime = engine.Time ();

   (*g_engfuncs.pfnRunPlayerMove) (pev->pContainingEntity, m_moveAngles, m_moveSpeed, m_strafeSpeed, 0.0f, pev->button, pev->impulse, msecVal);
}

void Bot::CheckBurstMode (float distance)
{
   // this function checks burst mode, and switch it depending distance to to enemy.

   if (HasShield ())
      return; // no checking when shiled is active

   // if current weapon is glock, disable burstmode on long distances, enable it else
   if (m_currentWeapon == WEAPON_GLOCK && distance < 300.0f && m_weaponBurstMode == BM_OFF)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_GLOCK && distance >= 300.0f && m_weaponBurstMode == BM_ON)
      pev->button |= IN_ATTACK2;

   // if current weapon is famas, disable burstmode on short distances, enable it else
   if (m_currentWeapon == WEAPON_FAMAS && distance > 400.0f && m_weaponBurstMode == BM_OFF)
      pev->button |= IN_ATTACK2;
   else if (m_currentWeapon == WEAPON_FAMAS && distance <= 400.0f && m_weaponBurstMode == BM_ON)
      pev->button |= IN_ATTACK2;
}

void Bot::CheckSilencer (void)
{
   if (((m_currentWeapon == WEAPON_USP && m_difficulty < 2) || m_currentWeapon == WEAPON_M4A1) && !HasShield())
   {
      int random = (m_personality == PERSONALITY_RUSHER ? 35 : 65);

      // aggressive bots don't like the silencer
      if (Random.Long (1, 100) <= (m_currentWeapon == WEAPON_USP ? random / 3 : random))
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
      return 0.0f;

   float timeLeft = ((g_timeBombPlanted + mp_c4timer.GetFloat ()) - engine.Time ());

   if (timeLeft < 0.0f)
      return 0.0f;

   return timeLeft;
}

float Bot::GetEstimatedReachTime (void)
{
   float estimatedTime = 2.0f; // time to reach next waypoint

   // calculate 'real' time that we need to get from one waypoint to another
   if (m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints && m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
   {
      float distance = (waypoints.GetPath (m_prevWptIndex[0])->origin - m_currentPath->origin).GetLength ();

      // caclulate estimated time
      if (pev->maxspeed <= 0.0f)
         estimatedTime = 4.0f * distance / 240.0f;
      else
         estimatedTime = 4.0f * distance / pev->maxspeed;

      bool longTermReachability = (m_currentPath->flags & FLAG_CROUCH) || (m_currentPath->flags & FLAG_LADDER) || (pev->button & IN_DUCK);

      // check for special waypoints, that can slowdown our movement
      if (longTermReachability)
         estimatedTime *= 3.0f;

      // check for too low values
      if (estimatedTime < 1.0f)
         estimatedTime = 1.0f;

      const float maxReachTime = longTermReachability ? 10.0f : 5.0f;

      // check for too high values
      if (estimatedTime > maxReachTime)
         estimatedTime = maxReachTime;
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
   if (timeLeft > 13.0f)
      return false;

   const Vector &bombOrigin = waypoints.GetBombPosition ();

   // for terrorist, if timer is lower than 13 seconds, return true
   if (static_cast <int> (timeLeft) < 13 && m_team == TERRORIST && (bombOrigin - pev->origin).GetLength () < 1000.0f)
      return true;

   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = NULL; // temporaly pointer to bot

      // search players with defuse kit
      if ((bot = bots.GetBot (i)) != NULL && bot->m_team == CT && bot->m_hasDefuser && (bombOrigin - bot->pev->origin).GetLength () < 500.0f)
      {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   float reachTime = waypoints.GetTravelTime (pev->maxspeed, m_currentPath->origin, bombOrigin);

   // for counter-terrorist check alos is we have time to reach position plus average defuse time
   if ((timeLeft < reachTime + 6.0f && !m_hasDefuser && !hasTeammatesWithDefuserKit) || (timeLeft < reachTime + 2.0f && m_hasDefuser))
      return true;

   if (m_hasProgressBar && IsOnFloor () && ((m_hasDefuser ? 10.0f : 15.0f) > GetBombTimeleft ()))
      return true;

   return false; // return false otherwise
}

void Bot::ReactOnSound (void)
{
   int hearEnemyIndex = -1;

   Vector pasOrg = EyePosition ();

   if (pev->flags & FL_DUCKING)
      pasOrg = pasOrg + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);

   byte *pas = ENGINE_SET_PVS (reinterpret_cast <float *> (&pasOrg));

   float minDistance = 99999.0f;

   // loop through all enemy clients to check for hearable stuff
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].ent == GetEntity () || g_clients[i].team == m_team || g_clients[i].timeSoundLasting < engine.Time ())
         continue;

      float distance = (g_clients[i].soundPosition - pev->origin).GetLength ();
     
      if (distance > g_clients[i].hearingDistance)
         continue;

      if (!ENGINE_CHECK_VISIBILITY (g_clients[i].ent, pas))
         continue;

      if (distance < minDistance)
      {
         hearEnemyIndex = i;
         minDistance = distance;
      }
   }
   edict_t *player = NULL;

   if (hearEnemyIndex >= 0 && g_clients[hearEnemyIndex].team != m_team && yb_csdm_mode.GetInt () != 2)
      player = g_clients[hearEnemyIndex].ent;

   // did the bot hear someone ?
   if (IsValidPlayer (player))
   {
      // change to best weapon if heard something
      if (m_shootTime < engine.Time () - 5.0f && IsOnFloor () && m_currentWeapon != WEAPON_C4 && m_currentWeapon != WEAPON_EXPLOSIVE && m_currentWeapon != WEAPON_SMOKE && m_currentWeapon != WEAPON_FLASHBANG && !yb_jasonmode.GetBool ())
         SelectBestWeapon ();

      m_heardSoundTime = engine.Time ();
      m_states |= STATE_HEARING_ENEMY;

      if ((Random.Long (0, 100) < 15) && engine.IsNullEntity (m_enemy) && engine.IsNullEntity (m_lastEnemy) && m_seeEnemyTime + 7.0f < engine.Time ())
         ChatterMessage (Chatter_HeardEnemy);

      // didn't bot already have an enemy ? take this one...
      if (m_lastEnemyOrigin.IsZero () || m_lastEnemy == NULL)
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

            if (distance > (player->v.origin - pev->origin).GetLengthSquared () && m_seeEnemyTime + 2.0f < engine.Time ())
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
      if (CheckVisibility (player, &m_lastEnemyOrigin, &m_visibility))
      {
         m_enemy = player;
         m_lastEnemy = player;
         m_enemyOrigin = m_lastEnemyOrigin;

         m_states |= STATE_SEEING_ENEMY;
         m_seeEnemyTime = engine.Time ();
      }
   }
}

bool Bot::IsShootableBreakable (edict_t *ent)
{
   // this function is checking that pointed by ent pointer obstacle, can be destroyed.

   if (FClassnameIs (ent, "func_breakable") || (FClassnameIs (ent, "func_pushable") && (ent->v.spawnflags & SF_PUSH_BREAKABLE)))
      return (ent->v.takedamage != DAMAGE_NO) && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY) && ent->v.health < 500.0f;

   return false;
}

void Bot::EquipInBuyzone (int buyState)
{
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   bool checkBuyTime = false;

   if (mp_buytime.m_eptr != NULL)
      checkBuyTime = (g_timeRoundStart + Random.Float (10.0f, 20.0f) + mp_buytime.GetFloat () < engine.Time ());

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (m_seeEnemyTime + 5.0f < engine.Time () && m_lastEquipTime + 15.0f < engine.Time () && m_inBuyZone && checkBuyTime && !g_bombPlanted && m_moneyAmount > g_botBuyEconomyTable[0])
   {
      m_buyingFinished = false;
      m_buyState = buyState;

      // push buy message
      PushMessageQueue (GSM_BUY_STUFF);

      m_nextBuyTime = engine.Time ();
      m_lastEquipTime = engine.Time ();
   }
}

bool Bot::IsBombDefusing (const Vector &bombOrigin)
{
   // this function finds if somebody currently defusing the bomb.

   bool defusingInProgress = false;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = bots.GetBot (i);

      if (bot == NULL || bot == this)
         continue; // skip invalid bots

      if (m_team != bot->m_team || bot->GetTaskId () == TASK_ESCAPEFROMBOMB)
         continue; // skip other mess

      if ((bot->pev->origin - bombOrigin).GetLength () < 140.0f && (bot->GetTaskId () == TASK_DEFUSEBOMB || bot->m_hasProgressBar))
      {
         defusingInProgress = true;
         break;
      }
      Client *client = &g_clients[i];

      // take in account peoples too
      if (defusingInProgress || !(client->flags & CF_USED) || !(client->flags & CF_ALIVE) || client->team != m_team || IsValidBot (g_clients[i].ent))
         continue;

      if ((client->ent->v.origin - bombOrigin).GetLength () < 140.0f && ((client->ent->v.button | client->ent->v.oldbuttons) & IN_USE))
      {
         defusingInProgress = true;
         break;
      }
   }
   return defusingInProgress;
}

float Bot::GetWalkSpeed (void)
{
   if ((GetTaskId () == TASK_SEEKCOVER) || (pev->flags & FL_DUCKING) || (pev->button & IN_DUCK) || (pev->oldbuttons & IN_DUCK) || (m_currentTravelFlags & PATHFLAG_JUMP) || (m_currentPath != NULL && m_currentPath->flags & FLAG_LADDER) || IsOnLadder () || IsInWater ())
      return pev->maxspeed;

   return static_cast <float> ((static_cast <int> (pev->maxspeed) * 0.5f) + (static_cast <int> (pev->maxspeed) / 50.0f)) - 18.0f;
}