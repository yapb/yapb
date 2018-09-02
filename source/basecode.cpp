//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
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
ConVar yb_best_weapon_picker_type ("yb_best_weapon_picker_type", "1");

// game console variables
ConVar mp_c4timer ("mp_c4timer", nullptr, VT_NOREGISTER);
ConVar mp_buytime ("mp_buytime", nullptr, VT_NOREGISTER, true, "1");
ConVar mp_footsteps ("mp_footsteps", nullptr, VT_NOREGISTER);
ConVar sv_gravity ("sv_gravity", nullptr, VT_NOREGISTER);

int Bot::getMsgQueue (void) {
   // this function get the current message from the bots message queue

   int message = m_messageQueue[m_actMessageIndex++];
   m_actMessageIndex &= 0x1f; // wraparound

   return message;
}

void Bot::pushMsgQueue (int message) {
   // this function put a message into the bot message queue

   if (message == GAME_MSG_SAY_CMD) {
      // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
      int entityIndex = index ();

      for (int i = 0; i < engine.maxClients (); i++) {
         Bot *otherBot = bots.getBot (i);

         if (otherBot != nullptr && otherBot->pev != pev) {
            if (m_notKilled == otherBot->m_notKilled) {
               otherBot->m_sayTextBuffer.entityIndex = entityIndex;
               strcpy (otherBot->m_sayTextBuffer.sayText, m_tempStrings);
            }
            otherBot->m_sayTextBuffer.timeNextChat = engine.timebase () + otherBot->m_sayTextBuffer.chatDelay;
         }
      }
   }
   m_messageQueue[m_pushMessageIndex++] = message;
   m_pushMessageIndex &= 0x1f; // wraparound
}

float Bot::isInFOV (const Vector &destination) {
   float entityAngle = angleMod (destination.toYaw ()); // find yaw angle from source to destination...
   float viewAngle = angleMod (pev->v_angle.y); // get bot's current view angle...

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead, 45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle
   float absoluteAngle = A_abs (viewAngle - entityAngle);

   if (absoluteAngle > 180.0f) {
      absoluteAngle = 360.0f - absoluteAngle;
   }
   return absoluteAngle;
}

bool Bot::isInViewCone (const Vector &origin) {
   // this function returns true if the spatial vector location origin is located inside
   // the field of view cone of the bot entity, false otherwise. It is assumed that entities
   // have a human-like field of view, that is, about 90 degrees.

   return ::isInViewCone (origin, ent ());
}

bool Bot::seesItem (const Vector &destination, const char *itemName) {
   TraceResult tr;

   // trace a line from bot's eyes to destination..
   engine.testLine (eyePos (), destination, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction != 1.0f) {
      return strcmp (STRING (tr.pHit->v.classname), itemName) == 0;
   }
   return true;
}

bool Bot::seesEntity (const Vector &dest, bool fromBody) {
   TraceResult tr;

   // trace a line from bot's eyes to destination...
   engine.testLine (fromBody ? pev->origin : eyePos (), dest, TRACE_IGNORE_EVERYTHING, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0f;
}

void Bot::checkGrenadesThrow (void) {

   // do not check cancel if we have grenade in out hands
   bool checkTasks = taskId () == TASK_PLANTBOMB || taskId () == TASK_DEFUSEBOMB;

   static auto clearThrowStates = [&](void) {
      m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG);
      m_grenadeCheckTime = engine.timebase () + 1.0f;
   };

   // check if throwing a grenade is a good thing to do...
   if (checkTasks || yb_ignore_enemies.boolean () || m_isUsingGrenade || m_isReloading || yb_jasonmode.boolean () || m_grenadeCheckTime >= engine.timebase ()) {
      clearThrowStates ();
      return;
   }

   // check again in some seconds
   m_grenadeCheckTime = engine.timebase () + 0.5f;

   if (!isAlive (m_lastEnemy) || !(m_states & (STATE_SUSPECT_ENEMY | STATE_HEARING_ENEMY))) {
      clearThrowStates ();
      return;
   }

   // check if we have grenades to throw
   int grenadeToThrow = bestGrenadeCarried ();

   // if we don't have grenades no need to check it this round again
   if (grenadeToThrow == -1) {
      m_grenadeCheckTime = engine.timebase () + 15.0f; // changed since, conzero can drop grens from dead players

      clearThrowStates ();
      return;
   }
   else {
      int cancelProb = 20;

      if (grenadeToThrow == WEAPON_FLASHBANG) {
         cancelProb = 10;
      }
      else if (grenadeToThrow == WEAPON_SMOKE) {
         cancelProb = 5;
      }
      if (rng.getInt (0, 100) < cancelProb) {
         clearThrowStates ();
         return;
      }
   }
   float distance = (m_lastEnemyOrigin - pev->origin).length2D ();

   // don't throw grenades at anything that isn't on the ground!
   if (!(m_lastEnemy->v.flags & FL_ONGROUND) && !m_lastEnemy->v.waterlevel && m_lastEnemyOrigin.z > pev->absmax.z) {
      distance = 9999.0f;
   }

   // too high to throw?
   if (m_lastEnemy->v.origin.z > pev->origin.z + 500.0f) {
      distance = 9999.0f;
   }

   // enemy within a good throw distance?
   if (!m_lastEnemyOrigin.empty () && distance > (grenadeToThrow == WEAPON_SMOKE ? 200.0f : 400.0f) && distance < 1200.0f) {
      bool allowThrowing = true;

      // care about different grenades
      switch (grenadeToThrow) {
      case WEAPON_EXPLOSIVE:
         if (numFriendsNear (m_lastEnemy->v.origin, 160.0f) > 0) {
            allowThrowing = false;
         }
         else {
            float radius = m_lastEnemy->v.velocity.length2D ();
            const Vector &pos = (m_lastEnemy->v.velocity * 0.5f).make2D () + m_lastEnemy->v.origin;

            if (radius < 128.0f) {
               radius = 128.0f;
            }

            IntArray predicted;
            waypoints.searchRadius (predicted, radius, pos, 8);

            for (auto &predict : predicted) {
               allowThrowing = TRUE;
               m_throw = waypoints[predict].origin;

               auto throwPos = calcThrow (eyePos (), m_throw);

               if (throwPos.lengthSq () < 100.0f) {
                  throwPos = calcToss (eyePos (), m_throw);
               }

               if (throwPos.empty ()) {
                  allowThrowing = false;
               }
               else {
                  m_throw.z += 110.0f;
                  break;
               }
            }
         }

         if (allowThrowing) {
            m_states |= STATE_THROW_HE;
         }
         else {
            m_states &= ~STATE_THROW_HE;
         }
         break;

      case WEAPON_FLASHBANG:
         m_throw = waypoints[waypoints.getNearest ((m_lastEnemy->v.velocity * 0.5f).make2D () + m_lastEnemy->v.origin)].origin;

         if (numFriendsNear (m_throw, 128.0f) > 0) {
            allowThrowing = false;
         }

         if (allowThrowing) {
            auto throwPos = calcThrow (eyePos (), m_throw);

            if (throwPos.lengthSq () < 100.0f) {
               throwPos = calcToss (eyePos (), m_throw);
            }

            if (throwPos.empty ()) {
               allowThrowing = false;
            }
            else {
               m_throw.z += 110.0f;
            }
         }

         if (allowThrowing) {
            m_states |= STATE_THROW_FB;
         }
         else {
            m_states &= ~STATE_THROW_FB;
         }
         break;

      case WEAPON_SMOKE:
         if (allowThrowing && !engine.isNullEntity (m_lastEnemy)) {
            if (getShootingConeDeviation (m_lastEnemy, pev->origin) >= 0.9f) {
               allowThrowing = false;
            }
         }

         if (allowThrowing) {
            m_states |= STATE_THROW_SG;
         }
         else {
            m_states &= ~STATE_THROW_SG;
         }
         break;
      }
      const float MaxThrowTime = engine.timebase () + 0.3f;

      if (m_states & STATE_THROW_HE) {
         startTask (TASK_THROWHEGRENADE, TASKPRI_THROWGRENADE, INVALID_WAYPOINT_INDEX, MaxThrowTime, false);
      }
      else if (m_states & STATE_THROW_FB) {
         startTask (TASK_THROWFLASHBANG, TASKPRI_THROWGRENADE, INVALID_WAYPOINT_INDEX, MaxThrowTime, false);
      }
      else if (m_states & STATE_THROW_SG) {
         startTask (TASK_THROWSMOKE, TASKPRI_THROWGRENADE, INVALID_WAYPOINT_INDEX, MaxThrowTime, false);
      }
   }
   else {
      clearThrowStates ();
   }
}

void Bot::avoidGrenades (void) {
   // checks if bot 'sees' a grenade, and avoid it

   if (!bots.hasActiveGrenades ()) {
      return;
   }

   // check if old pointers to grenade is invalid
   if (engine.isNullEntity (m_avoidGrenade)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }
   else if ((m_avoidGrenade->v.flags & FL_ONGROUND) || (m_avoidGrenade->v.effects & EF_NODRAW)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }
   auto activeGrenades = bots.searchActiveGrenades ();

   // find all grenades on the map
   for (edict_t *pent : activeGrenades) {
      if (pent->v.effects & EF_NODRAW) {
         continue;
      }

      // check if visible to the bot
      if (!seesEntity (pent->v.origin) && isInFOV (pent->v.origin - eyePos ()) > pev->fov * 0.5f) {
         continue;
      }
      auto model = STRING (pent->v.model) + 9;

      if (m_turnAwayFromFlashbang < engine.timebase () && m_personality == PERSONALITY_RUSHER && m_difficulty == 4 && strcmp (model, "flashbang.mdl") == 0) {
         // don't look at flash bang
         if (!(m_states & STATE_SEEING_ENEMY)) {
            pev->v_angle.y = angleNorm ((engine.getAbsPos (pent) - eyePos ()).toAngles ().y + 180.0f);

            m_canChooseAimDirection = false;
            m_turnAwayFromFlashbang = engine.timebase () + rng.getFloat (1.0f, 2.0f);
         }
      }
      else if (strcmp (model, "hegrenade.mdl") == 0) {
         if (!engine.isNullEntity (m_avoidGrenade)) {
            return;
         }

         if (engine.getTeam (pent->v.owner) == m_team || pent->v.owner == ent ()) {
            return;
         }

         if (!(pent->v.flags & FL_ONGROUND)) {
            float distance = (pent->v.origin - pev->origin).length ();
            float distanceMoved = ((pent->v.origin + pent->v.velocity * calcThinkInterval ()) - pev->origin).length ();

            if (distanceMoved < distance && distance < 500.0f) {
               makeVectors (pev->v_angle);

               const Vector &dirToPoint = (pev->origin - pent->v.origin).normalize2D ();
               const Vector &rightSide = g_pGlobals->v_right.normalize2D ();

               if ((dirToPoint | rightSide) > 0.0f) {
                  m_needAvoidGrenade = -1;
               }
               else {
                  m_needAvoidGrenade = 1;
               }
               m_avoidGrenade = pent;
            }
         }
      }
      else if ((pent->v.flags & FL_ONGROUND) == 0 && strcmp (model, "smokegrenade.mdl") == 0) {
         float distance = (pent->v.origin - pev->origin).length ();

         // shrink bot's viewing distance to smoke grenade's distance
         if (m_viewDistance > distance) {
            m_viewDistance = distance;

            if (rng.getInt (0, 100) < 45) {
               pushChatterMessage (Chatter_BehindSmoke);
            }
         }
      }
   }
}

int Bot::bestPrimaryCarried (void) {
   // this function returns the best weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   // take the shield in account
   if (hasShield ()) {
      weapons |= (1 << WEAPON_SHIELD);
   }

   for (int i = 0; i < NUM_WEAPONS; i++) {
      if (weapons & (1 << weaponTab[*ptr].id)) {
         weaponIndex = i;
      }
      ptr++;
   }
   return weaponIndex;
}

int Bot::bestSecondaryCarried (void) {
   // this function returns the best secondary weapon of this bot (based on personality prefs)

   int *ptr = g_weaponPrefs[m_personality];
   int weaponIndex = 0;
   int weapons = pev->weapons;

   // take the shield in account
   if (hasShield ()) {
      weapons |= (1 << WEAPON_SHIELD);
   }
   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < NUM_WEAPONS; i++) {
      int id = weaponTab[*ptr].id;

      if ((weapons & (1 << weaponTab[*ptr].id)) && (id == WEAPON_USP || id == WEAPON_GLOCK || id == WEAPON_DEAGLE || id == WEAPON_P228 || id == WEAPON_ELITE || id == WEAPON_FIVESEVEN)) {
         weaponIndex = i;
         break;
      }
      ptr++;
   }
   return weaponIndex;
}

bool Bot::rateGroundWeapon (edict_t *ent) {
   // this function compares weapons on the ground to the one the bot is using

   int hasWeapon = 0;
   int groundIndex = 0;
   int *ptr = g_weaponPrefs[m_personality];

   WeaponSelect *weaponTab = &g_weaponSelect[0];

   for (int i = 0; i < NUM_WEAPONS; i++) {
      if (strcmp (weaponTab[*ptr].modelName, STRING (ent->v.model) + 9) == 0) {
         groundIndex = i;
         break;
      }
      ptr++;
   }

   if (groundIndex < 7) {
      hasWeapon = bestSecondaryCarried ();
   }
   else {
      hasWeapon = bestPrimaryCarried ();
   }
   return groundIndex > hasWeapon;
}

void Bot::processBreakables (edict_t *touch) {
   if (!isShootableBreakable (touch)) {
      return;
   }
   m_breakableEntity = lookupBreakable ();

   if (engine.isNullEntity (m_breakableEntity)) {
      return;
   }
   m_campButtons = pev->button & IN_DUCK;

   startTask (TASK_SHOOTBREAKABLE, TASKPRI_SHOOTBREAKABLE, INVALID_WAYPOINT_INDEX, 0.0f, false);
}

edict_t *Bot::lookupBreakable (void) {
   // this function checks if bot is blocked by a shoot able breakable in his moving direction

   TraceResult tr;
   engine.testLine (pev->origin, pev->origin + (m_destOrigin - pev->origin).normalize () * 72.0f, TRACE_IGNORE_NONE, ent (), &tr);

   if (tr.flFraction != 1.0f) {
      edict_t *ent = tr.pHit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (isShootableBreakable (ent)) {
         m_breakableOrigin = engine.getAbsPos (ent);
         return ent;
      }
   }
   engine.testLine (eyePos (), eyePos () + (m_destOrigin - eyePos ()).normalize () * 72.0f, TRACE_IGNORE_NONE, ent (), &tr);

   if (tr.flFraction != 1.0f) {
      edict_t *ent = tr.pHit;

      if (isShootableBreakable (ent)) {
         m_breakableOrigin = engine.getAbsPos (ent);
         return ent;
      }
   }
   m_breakableEntity = nullptr;
   m_breakableOrigin.nullify ();

   return nullptr;
}

void Bot::setIdealReactionTimers (bool actual) {
   static struct ReactionTime {
      float min;
      float max;
   } reactionTimers[] = {{0.8f, 1.0f}, {0.4f, 0.6f}, {0.2f, 0.4f}, {0.1f, 0.3f}, {0.0f, 0.1f}};

   const ReactionTime &reaction = reactionTimers[m_difficulty];

   if (actual) {
      m_idealReactionTime = reaction.min;
      m_actualReactionTime = reaction.min;

      return;
   }
   m_idealReactionTime = rng.getFloat (reaction.min, reaction.max);
}

void Bot::processPickups (void) {
   // this function finds Items to collect or use in the near of a bot

   // don't try to pickup anything while on ladder or trying to escape from bomb...
   if (isOnLadder () || taskId () == TASK_ESCAPEFROMBOMB || yb_jasonmode.boolean ()) {
      m_pickupItem = nullptr;
      m_pickupType = PICKUP_NONE;

      return;
   }

   edict_t *ent = nullptr, *pickupItem = nullptr;
   Bot *bot = nullptr;

   const float radius = 400.0f;
   float minDistance = A_square (radius + 10.0f);

   if (!engine.isNullEntity (m_pickupItem)) {
      bool itemExists = false;
      pickupItem = m_pickupItem;

      while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityInSphere (ent, pev->origin, radius))) {
         if ((ent->v.effects & EF_NODRAW) || isPlayer (ent->v.owner)) {
            continue; // someone owns this weapon or it hasn't re spawned yet
         }
         if (ent == pickupItem) {
            if (seesItem (engine.getAbsPos (ent), STRING (ent->v.classname))) {
               itemExists = true;
            }
            break;
         }
      }

      if (itemExists) {
         return;
      }
      else {
         m_pickupItem = nullptr;
         m_pickupType = PICKUP_NONE;
      }
   }

   ent = nullptr;
   pickupItem = nullptr;

   PickupType pickupType = PICKUP_NONE;

   Vector pickupPos;
   Vector entPos;

   m_pickupItem = nullptr;
   m_pickupType = PICKUP_NONE;

   while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityInSphere (ent, pev->origin, radius))) {
      bool allowPickup = false; // assume can't use it until known otherwise

      if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore) {
         continue; // someone owns this weapon or it hasn't respawned yet
      }

      auto classname = STRING (ent->v.classname);
      auto model = STRING (ent->v.model) + 9;

      entPos = engine.getAbsPos (ent);

      // check if line of sight to object is not blocked (i.e. visible)
      if (seesItem (entPos, classname)) {
         if (strncmp ("hostage_entity", classname, 14) == 0) {
            allowPickup = true;
            pickupType = PICKUP_HOSTAGE;
         }
         else if (strncmp ("weaponbox", classname, 9) == 0 && strcmp (model, "backpack.mdl") == 0) {
            allowPickup = true;
            pickupType = PICKUP_DROPPED_C4;
         }
         else if ((strncmp ("weaponbox", classname, 9) == 0 || strncmp ("armoury_entity", classname, 14) == 0 || strncmp ("csdm", classname, 4) == 0) && !m_isUsingGrenade) {
            allowPickup = true;
            pickupType = PICKUP_WEAPON;
         }
         else if (strncmp ("weapon_shield", classname, 13) == 0 && !m_isUsingGrenade) {
            allowPickup = true;
            pickupType = PICKUP_SHIELD;
         }
         else if (strncmp ("item_thighpack", classname, 14) == 0 && m_team == TEAM_COUNTER && !m_hasDefuser) {
            allowPickup = true;
            pickupType = PICKUP_DEFUSEKIT;
         }
         else if (strncmp ("grenade", classname, 7) == 0 && strcmp (model, "c4.mdl") == 0) {
            allowPickup = true;
            pickupType = PICKUP_PLANTED_C4;
         }
      }

      if (allowPickup) // if the bot found something it can pickup...
      {
         float distance = (entPos - pev->origin).lengthSq ();

         // see if it's the closest item so far...
         if (distance < minDistance) {
            if (pickupType == PICKUP_WEAPON) // found weapon on ground?
            {
               int weaponCarried = bestPrimaryCarried ();
               int secondaryWeaponCarried = bestSecondaryCarried ();

               if (secondaryWeaponCarried < 7 && (m_ammo[g_weaponSelect[secondaryWeaponCarried].id] > 0.3 * g_weaponDefs[g_weaponSelect[secondaryWeaponCarried].id].ammo1Max) && strcmp (model, "w_357ammobox.mdl") == 0) {
                  allowPickup = false;
               }
               else if (!m_isVIP && weaponCarried >= 7 && (m_ammo[g_weaponSelect[weaponCarried].id] > 0.3 * g_weaponDefs[g_weaponSelect[weaponCarried].id].ammo1Max) && strncmp (model, "w_", 2) == 0) {
                  bool isSniperRifle = weaponCarried == WEAPON_AWP || weaponCarried == WEAPON_G3SG1 || weaponCarried == WEAPON_SG550;
                  bool isSubmachine = weaponCarried == WEAPON_MP5 || weaponCarried == WEAPON_TMP || weaponCarried == WEAPON_P90 || weaponCarried == WEAPON_MAC10 || weaponCarried == WEAPON_UMP45;
                  bool isShotgun = weaponCarried == WEAPON_M3;
                  bool isRifle = weaponCarried == WEAPON_FAMAS || weaponCarried == WEAPON_AK47 || weaponCarried == WEAPON_M4A1 || weaponCarried == WEAPON_GALIL || weaponCarried == WEAPON_AUG || weaponCarried == WEAPON_SG552;

                  if (strcmp (model, "w_9mmarclip.mdl") == 0 && !isRifle) {
                     allowPickup = false;
                  }
                  else if (strcmp (model, "w_shotbox.mdl") == 0 && !isShotgun) {
                     allowPickup = false;
                  }
                  else if (strcmp (model, "w_9mmclip.mdl") == 0 && !isSubmachine) {
                     allowPickup = false;
                  }
                  else if (strcmp (model, "w_crossbow_clip.mdl") == 0 && !isSniperRifle) {
                     allowPickup = false;
                  }
                  else if (strcmp (model, "w_chainammo.mdl") == 0 && weaponCarried != WEAPON_M249) {
                     allowPickup = false;
                  }
               }
               else if (m_isVIP || !rateGroundWeapon (ent)) {
                  allowPickup = false;
               }
               else if (strcmp (model, "medkit.mdl") == 0 && pev->health >= 100.0f) {
                  allowPickup = false;
               }
               else if ((strcmp (model, "kevlar.mdl") == 0 || strcmp (model, "battery.mdl") == 0) && pev->armorvalue >= 100.0f) {
                  allowPickup = false;
               }
               else if (strcmp (model, "flashbang.mdl") == 0 && (pev->weapons & (1 << WEAPON_FLASHBANG))) {
                  allowPickup = false;
               }
               else if (strcmp (model, "hegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_EXPLOSIVE))) {
                  allowPickup = false;
               }
               else if (strcmp (model, "smokegrenade.mdl") == 0 && (pev->weapons & (1 << WEAPON_SMOKE))) {
                  allowPickup = false;
               }
            }
            else if (pickupType == PICKUP_SHIELD) // found a shield on ground?
            {
               if ((pev->weapons & (1 << WEAPON_ELITE)) || hasShield () || m_isVIP || (hasPrimaryWeapon () && !rateGroundWeapon (ent))) {
                  allowPickup = false;
               }
            }
            else if (m_team == TEAM_TERRORIST) // terrorist team specific
            {
               if (pickupType == PICKUP_DROPPED_C4) {
                  allowPickup = true;
                  m_destOrigin = entPos; // ensure we reached dropped bomb

                  pushChatterMessage (Chatter_FoundC4); // play info about that
                  clearSearchNodes ();
               }
               else if (pickupType == PICKUP_HOSTAGE) {
                  m_itemIgnore = ent;
                  allowPickup = false;

                  if (!m_defendHostage && m_difficulty >= 3 && rng.getInt (0, 100) < 30 && m_timeCamping + 15.0f < engine.timebase ()) {
                     int index = getDefendPoint (entPos);

                     startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (30.0f, 60.0f), true); // push camp task on to stack
                     startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + rng.getFloat (3.0f, 6.0f), true); // push move command

                     if (waypoints[index].vis.crouch <= waypoints[index].vis.stand) {
                        m_campButtons |= IN_DUCK;
                     }
                     else {
                        m_campButtons &= ~IN_DUCK;
                     }
                     m_defendHostage = true;

                     pushChatterMessage (Chatter_GoingToGuardHostages); // play info about that
                     return;
                  }
               }
               else if (pickupType == PICKUP_PLANTED_C4) {
                  allowPickup = false;

                  if (!m_defendedBomb) {
                     m_defendedBomb = true;

                     int index = getDefendPoint (entPos);
                     Path &path = waypoints[index];

                     float bombTimer = mp_c4timer.flt ();
                     float timeMidBlowup = g_timeBombPlanted + (bombTimer * 0.5f + bombTimer * 0.25f) - waypoints.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                     if (timeMidBlowup > engine.timebase ()) {
                        clearTask (TASK_MOVETOPOSITION); // remove any move tasks

                        startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, timeMidBlowup, true); // push camp task on to stack
                        startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeMidBlowup, true); // push  move command

                        if (path.vis.crouch <= path.vis.stand) {
                           m_campButtons |= IN_DUCK;
                        }
                        else {
                           m_campButtons &= ~IN_DUCK;
                        }
                        if (rng.getInt (0, 100) < 90) {
                           pushChatterMessage (Chatter_DefendingBombSite);
                        }
                     }
                     else {
                        pushRadioMessage (Radio_ShesGonnaBlow); // issue an additional radio message
                     }
                  }
               }
            }
            else if (m_team == TEAM_COUNTER) {
               if (pickupType == PICKUP_HOSTAGE) {
                  if (engine.isNullEntity (ent) || ent->v.health <= 0) {
                     allowPickup = false; // never pickup dead hostage
                  }
                  else
                     for (int i = 0; i < engine.maxClients (); i++) {
                        if ((bot = bots.getBot (i)) != nullptr && bot->m_notKilled) {
                           for (int j = 0; j < MAX_HOSTAGES; j++) {
                              if (bot->m_hostages[j] == ent) {
                                 allowPickup = false;
                                 break;
                              }
                           }
                        }
                     }
               }
               else if (pickupType == PICKUP_PLANTED_C4) {
                  if (isPlayer (m_enemy)) {
                     allowPickup = false;
                     return;
                  }

                  if (isOutOfBombTimer ()) {
                     allowPickup = false;
                     return;
                  }

                  if (rng.getInt (0, 100) < 90) {
                     pushChatterMessage (Chatter_FoundBombPlace);
                  }

                  allowPickup = !isBombDefusing (entPos) || m_hasProgressBar;
                  pickupType = PICKUP_PLANTED_C4;

                  if (!m_defendedBomb && !allowPickup) {
                     m_defendedBomb = true;

                     int index = getDefendPoint (entPos);
                     Path &path = waypoints[index];

                     float timeToExplode = g_timeBombPlanted + mp_c4timer.flt () - waypoints.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                     clearTask (TASK_MOVETOPOSITION); // remove any move tasks

                     startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, timeToExplode, true); // push camp task on to stack
                     startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, timeToExplode, true); // push move command

                     if (path.vis.crouch <= path.vis.stand) {
                        m_campButtons |= IN_DUCK;
                     }
                     else {
                        m_campButtons &= ~IN_DUCK;
                     }

                     if (rng.getInt (0, 100) < 90) {
                        pushChatterMessage (Chatter_DefendingBombSite);
                     }
                  }
               }
               else if (pickupType == PICKUP_DROPPED_C4) {
                  m_itemIgnore = ent;
                  allowPickup = false;

                  if (!m_defendedBomb && m_difficulty >= 2 && rng.getInt (0, 100) < 75 && pev->health < 80) {
                     int index = getDefendPoint (entPos);

                     startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (30.0f, 70.0f), true); // push camp task on to stack
                     startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + rng.getFloat (10.0f, 30.0f), true); // push move command

                     if (waypoints[index].vis.crouch <= waypoints[index].vis.stand) {
                        m_campButtons |= IN_DUCK;
                     }
                     else {
                        m_campButtons &= ~IN_DUCK;
                     }
                     m_defendedBomb = true;

                     pushChatterMessage (Chatter_GoingToGuardDoppedBomb); // play info about that
                     return;
                  }
               }
            }

            // if condition valid
            if (allowPickup) {
               minDistance = distance; // update the minimum distance
               pickupPos = entPos; // remember location of entity
               pickupItem = ent; // remember this entity

               m_pickupType = pickupType;
            }
            else {
               pickupType = PICKUP_NONE;
            }
         }
      }
   } // end of the while loop

   if (!engine.isNullEntity (pickupItem)) {
      for (int i = 0; i < engine.maxClients (); i++) {
         if ((bot = bots.getBot (i)) != nullptr && bot->m_notKilled && bot->m_pickupItem == pickupItem) {
            m_pickupItem = nullptr;
            m_pickupType = PICKUP_NONE;

            return;
         }
      }

      // check if item is too high to reach, check if getting the item would hurt bot
      if (pickupPos.z > eyePos ().z + (m_pickupType == PICKUP_HOSTAGE ? 50.0f : 20.0f) || isDeadlyMove (pickupPos)) {
         m_itemIgnore = m_pickupItem;
         m_pickupItem = nullptr;
         m_pickupType = PICKUP_NONE;

         return;
      }
      m_pickupItem = pickupItem; // save pointer of picking up entity
   }
}

void Bot::getCampDir (Vector *dest) {
   // this function check if view on last enemy position is blocked - replace with better vector then
   // mostly used for getting a good camping direction vector if not camping on a camp waypoint

   TraceResult tr;
   const Vector &src = eyePos ();

   engine.testLine (src, *dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      float length = (tr.vecEndPos - src).lengthSq ();

      if (length > 10000.0f) {
         return;
      }

      float minDistance = 99999.0f;
      float maxDistance = 99999.0f;

      int enemyIndex = INVALID_WAYPOINT_INDEX, tempIndex = INVALID_WAYPOINT_INDEX;

      // find nearest waypoint to bot and position
      for (int i = 0; i < g_numWaypoints; i++) {
         float distance = (waypoints[i].origin - pev->origin).lengthSq ();

         if (distance < minDistance) {
            minDistance = distance;
            tempIndex = i;
         }
         distance = (waypoints[i].origin - *dest).lengthSq ();

         if (distance < maxDistance) {
            maxDistance = distance;
            enemyIndex = i;
         }
      }

      if (tempIndex == INVALID_WAYPOINT_INDEX || enemyIndex == INVALID_WAYPOINT_INDEX) {
         return;
      }
      minDistance = 99999.0f;

      int lookAtWaypoint = INVALID_WAYPOINT_INDEX;
      Path &path = waypoints[tempIndex];

      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         if (path.index[i] == INVALID_WAYPOINT_INDEX) {
            continue;
         }
         float distance = static_cast<float> (waypoints.getPathDist (path.index[i], enemyIndex));

         if (distance < minDistance) {
            minDistance = distance;
            lookAtWaypoint = path.index[i];
         }
      }

      if (lookAtWaypoint != INVALID_WAYPOINT_INDEX && lookAtWaypoint < g_numWaypoints) {
         *dest = waypoints[lookAtWaypoint].origin;
      }
   }
}

void Bot::showChaterIcon (bool show) {
   // this function depending on show boolen, shows/remove chatter, icon, on the head of bot.

   if (!(g_gameFlags & GAME_SUPPORT_BOT_VOICE) || yb_communication_type.integer () != 2) {
      return;
   }

   auto sendBotVoice = [](bool show, edict_t *ent, int ownId) {
      MessageWriter (MSG_ONE, engine.getMessageId (NETMSG_BOTVOICE), Vector::null (), ent) // begin message
         .writeByte (show) // switch on/off
         .writeByte (ownId);
   };

   int ownId = index ();

   for (int i = 0; i < engine.maxClients (); i++) {
      Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }

      if (!show && (client.iconFlags[ownId] & CF_ICON) && client.iconTimestamp[ownId] < engine.timebase ()) {
         sendBotVoice (false, client.ent, ownId);

         client.iconTimestamp[ownId] = 0.0f;
         client.iconFlags[ownId] &= ~CF_ICON;
      }
      else if (show && !(client.iconFlags[ownId] & CF_ICON)) {
         sendBotVoice (true, client.ent, ownId);
      }
   }
}

void Bot::instantChatter (int type) {
   // this function sends instant chatter messages.

   if (!(g_gameFlags & GAME_SUPPORT_BOT_VOICE) || yb_communication_type.integer () != 2 || g_chatterFactory[type].empty ()) {
      return;
   }

   // delay only report team
   if (type == Radio_ReportTeam) {
      if (m_timeRepotingInDelay < engine.timebase ()) {
         return;
      }
      m_timeRepotingInDelay = engine.timebase () + rng.getFloat (30.0f, 60.0f);
   }
   auto playbackSound = g_chatterFactory[type].random ();
   auto painSound = g_chatterFactory[Chatter_DiePain].random ();

   if (m_notKilled) {
      showChaterIcon (true);
   }
   MessageWriter msg;

   for (int i = 0; i < engine.maxClients (); i++) {
      Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }
      msg.start (MSG_ONE, engine.getMessageId (NETMSG_SENDAUDIO), Vector::null (), client.ent) // begin message
      .writeByte (index ());

      if (pev->deadflag & DEAD_DYING) {
         client.iconTimestamp[index ()] = engine.timebase () + painSound.duration;
         msg.writeString (format ("%s/%s.wav", yb_chatter_path.str (), painSound.name.chars ()));
      }
      else if (!(pev->deadflag & DEAD_DEAD)) {
         client.iconTimestamp[index ()] = engine.timebase () + playbackSound.duration;
         msg.writeString (format ("%s/%s.wav", yb_chatter_path.str (), playbackSound.name.chars ()));
      }
      msg.writeShort (m_voicePitch).end ();
      client.iconFlags[index ()] |= CF_ICON;
   }
}

void Bot::pushRadioMessage (int message) {
   // this function inserts the radio message into the message queue

   if (yb_communication_type.integer () == 0 || m_numFriendsLeft == 0) {
      return;
   }
   if (!(g_gameFlags & GAME_SUPPORT_BOT_VOICE) || g_chatterFactory[message].empty () || yb_communication_type.integer () != 2) {
      m_forceRadio = true; // use radio instead voice
   }
   else {
      m_forceRadio = false;
   }
   m_radioSelect = message;
   pushMsgQueue (GAME_MSG_RADIO);
}

void Bot::pushChatterMessage (int message) {
   // this function inserts the voice message into the message queue (mostly same as above)

   if (!(g_gameFlags & GAME_SUPPORT_BOT_VOICE) || yb_communication_type.integer () != 2 || g_chatterFactory[message].empty () || m_numFriendsLeft == 0) {
      return;
   }
   bool sendMessage = false;

   float &messageTimer = m_chatterTimes[message];
   float &messageRepeat = g_chatterFactory[message][0].repeat;

   if (messageTimer < engine.timebase () || Math::isFltEqual (messageTimer, MAX_CHATTER_REPEAT)) {
      if (!Math::isFltEqual (messageTimer, MAX_CHATTER_REPEAT) && !Math::isFltEqual (messageRepeat, MAX_CHATTER_REPEAT)) {
         messageTimer = engine.timebase () + messageRepeat;
      }
      sendMessage = true;
   }

   if (!sendMessage) {
      return;
   }
   m_radioSelect = message;
   pushMsgQueue (GAME_MSG_RADIO);
}

void Bot::checkMsgQueue (void) {
   // this function checks and executes pending messages

   // no new message?
   if (m_actMessageIndex == m_pushMessageIndex) {
      return;
   }
   // get message from stack
   int state = getMsgQueue ();

   // nothing to do?
   if (state == GAME_MSG_NONE || (state == GAME_MSG_RADIO && (g_gameFlags & GAME_CSDM_FFA))) {
      return;
   }

   switch (state) {
   case GAME_MSG_PURCHASE: // general buy message

      // buy weapon
      if (m_nextBuyTime > engine.timebase ()) {
         // keep sending message
         pushMsgQueue (GAME_MSG_PURCHASE);
         return;
      }

      if (!m_inBuyZone || (g_gameFlags & GAME_CSDM)) {
         m_buyPending = true;
         m_buyingFinished = true;

         break;
      }

      m_buyPending = false;
      m_nextBuyTime = engine.timebase () + rng.getFloat (0.5f, 1.3f);

      // if bot buying is off then no need to buy
      if (!yb_botbuy.boolean ()) {
         m_buyState = BUYSTATE_FINISHED;
      }

      // if fun-mode no need to buy
      if (yb_jasonmode.boolean ()) {
         m_buyState = BUYSTATE_FINISHED;
         selectWeaponByName ("weapon_knife");
      }

      // prevent vip from buying
      if (m_isVIP) {
         m_buyState = BUYSTATE_FINISHED;
         m_pathType = SEARCH_PATH_FASTEST;
      }

      // prevent terrorists from buying on es maps
      if ((g_mapType & MAP_ES) && m_team == TEAM_TERRORIST) {
         m_buyState = 6;
      }

      // prevent teams from buying on fun maps
      if (g_mapType & (MAP_KA | MAP_FY)) {
         m_buyState = BUYSTATE_FINISHED;

         if (g_mapType & MAP_KA) {
            yb_jasonmode.setInteger (1);
         }
      }

      if (m_buyState > BUYSTATE_FINISHED - 1) {
         m_buyingFinished = true;
         return;
      }

      pushMsgQueue (GAME_MSG_NONE);
      buyStuff ();

      break;

   case GAME_MSG_RADIO:
      // if last bot radio command (global) happened just a 3 seconds ago, delay response
      if (g_lastRadioTime[m_team] + 3.0f < engine.timebase ()) {
         // if same message like previous just do a yes/no
         if (m_radioSelect != Radio_Affirmative && m_radioSelect != Radio_Negative) {
            if (m_radioSelect == g_lastRadio[m_team] && g_lastRadioTime[m_team] + 1.5f > engine.timebase ())
               m_radioSelect = -1;
            else {
               if (m_radioSelect != Radio_ReportingIn) {
                  g_lastRadio[m_team] = m_radioSelect;
               }
               else {
                  g_lastRadio[m_team] = -1;
               }

               for (int i = 0; i < engine.maxClients (); i++) {
                  Bot *bot = bots.getBot (i);

                  if (bot != nullptr) {
                     if (pev != bot->pev && bot->m_team == m_team) {
                        bot->m_radioOrder = m_radioSelect;
                        bot->m_radioEntity = ent ();
                     }
                  }
               }
            }
         }

         if (m_radioSelect == Radio_ReportingIn) {
            switch (taskId ()) {
            case TASK_NORMAL:
               if (task ()->data != INVALID_WAYPOINT_INDEX && rng.getInt (0, 100) < 70) {
                  Path &path = waypoints[task ()->data];

                  if (path.flags & FLAG_GOAL) {
                     if ((g_mapType & MAP_DE) && m_team == TEAM_TERRORIST && m_hasC4) {
                        instantChatter (Chatter_GoingToPlantBomb);
                     }
                     else {
                        instantChatter (Chatter_Nothing);
                     }
                  }
                  else if (path.flags & FLAG_RESCUE) {
                     instantChatter (Chatter_RescuingHostages);
                  }
                  else if ((path.flags & FLAG_CAMP) && rng.getInt (0, 100) > 15) {
                     instantChatter (Chatter_GoingToCamp);
                  }
                  else {
                     instantChatter (Chatter_HearSomething);
                  }
               }
               else if (rng.getInt (0, 100) < 30) {
                  instantChatter (Chatter_ReportingIn);
               }
               break;

            case TASK_MOVETOPOSITION:
               if (rng.getInt (0, 100) < 20) {
                  instantChatter (Chatter_GoingToCamp);
               }
               break;

            case TASK_CAMP:
               if (rng.getInt (0, 100) < 40) {
                  if (g_bombPlanted && m_team == TEAM_TERRORIST) {
                     instantChatter (Chatter_GuardDroppedC4);
                  }
                  else if (m_inVIPZone && m_team == TEAM_TERRORIST) {
                     instantChatter (Chatter_GuardingVipSafety);
                  }
                  else {
                     instantChatter (Chatter_Camp);
                  }
               }
               break;

            case TASK_PLANTBOMB:
               instantChatter (Chatter_PlantingC4);
               break;

            case TASK_DEFUSEBOMB:
               instantChatter (Chatter_DefusingC4);
               break;

            case TASK_ATTACK:
               instantChatter (Chatter_InCombat);
               break;

            case TASK_HIDE:
            case TASK_SEEKCOVER:
               instantChatter (Chatter_SeeksEnemy);
               break;

            default:
               if (rng.getInt (0, 100) < 50) {
                  instantChatter (Chatter_Nothing);
               }
               break;
            }
         }

         if ((m_radioSelect != Radio_ReportingIn && m_forceRadio) || yb_communication_type.integer () != 2 || g_chatterFactory[m_radioSelect].empty () || !(g_gameFlags & GAME_SUPPORT_BOT_VOICE)) {
            if (m_radioSelect < Radio_GoGoGo) {
               engine.execBotCmd (ent (), "radio1");
            }
            else if (m_radioSelect < Radio_Affirmative) {
               m_radioSelect -= Radio_GoGoGo - 1;
               engine.execBotCmd (ent (), "radio2");
            }
            else {
               m_radioSelect -= Radio_Affirmative - 1;
               engine.execBotCmd (ent (), "radio3");
            }

            // select correct menu item for this radio message
            engine.execBotCmd (ent (), "menuselect %d", m_radioSelect);
         }
         else if (m_radioSelect != -1 && m_radioSelect != Radio_ReportingIn) {
            instantChatter (m_radioSelect);
         }
         m_forceRadio = false; // reset radio to voice
         g_lastRadioTime[m_team] = engine.timebase (); // store last radio usage
      }
      else {
         pushMsgQueue (GAME_MSG_RADIO);
      }
      break;

   // team independent saytext
   case GAME_MSG_SAY_CMD:
      say (m_tempStrings);
      break;

   // team dependent saytext
   case GAME_MSG_SAY_TEAM_MSG:
      sayTeam (m_tempStrings);
      break;

   default:
      return;
   }
}

bool Bot::isWeaponRestricted (int weaponIndex) {
   // this function checks for weapon restrictions.

   if (isEmptyStr (yb_restricted_weapons.str ())) {
      return isWeaponRestrictedAMX (weaponIndex); // no banned weapons
   }
   auto bannedWeapons = String (yb_restricted_weapons.str ()).split (";");

   for (auto &ban : bannedWeapons) {
      const char *banned = STRING (getWeaponData (true, nullptr, weaponIndex));

      // check is this weapon is banned
      if (strncmp (ban.chars (), banned, ban.length ()) == 0) {
         return true;
      }
   }
   return isWeaponRestrictedAMX (weaponIndex);
}

bool Bot::isWeaponRestrictedAMX (int weaponIndex) {
   // this function checks restriction set by AMX Mod, this function code is courtesy of KWo.

   // check for weapon restrictions
   if ((1 << weaponIndex) & (WEAPON_PRIMARY | WEAPON_SECONDARY | WEAPON_SHIELD)) {
      const char *restrictedWeapons = g_engfuncs.pfnCVarGetString ("amx_restrweapons");

      if (isEmptyStr (restrictedWeapons)) {
         return false;
      }
      int indices[] = {4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast<int> (strlen (restrictedWeapons))) {
         return false;
      }
      return restrictedWeapons[index] != '0';
   }
   else // check for equipment restrictions
   {
      const char *restrictedEquipment = g_engfuncs.pfnCVarGetString ("amx_restrequipammo");

      if (isEmptyStr (restrictedEquipment)) {
         return false;
      }
      int indices[] = {-1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 0, 1, 5};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast<int> (strlen (restrictedEquipment))) {
         return false;
      }
      return restrictedEquipment[index] != '0';
   }
}

bool Bot::canReplaceWeapon (void) {
   // this function determines currently owned primary weapon, and checks if bot has
   // enough money to buy more powerful weapon.

   // if bot is not rich enough or non-standard weapon mode enabled return false
   if (g_weaponSelect[25].teamStandard != 1 || m_moneyAmount < 4000) {
      return false;
   }

   if (!isEmptyStr (yb_restricted_weapons.str ())) {
      auto bannedWeapons = String (yb_restricted_weapons.str ()).split (";");

      // check if its banned
      for (auto &ban : bannedWeapons) {
         if (m_currentWeapon == getWeaponData (false, ban.chars ())) {
            return true;
         }
      }
   }

   if (m_currentWeapon == WEAPON_SCOUT && m_moneyAmount > 5000) {
      return true;
   }
   else if (m_currentWeapon == WEAPON_MP5 && m_moneyAmount > 6000) {
      return true;
   }
   else if ((m_currentWeapon == WEAPON_M3 || m_currentWeapon == WEAPON_XM1014) && m_moneyAmount > 4000) {
      return true;
   }
   return false;
}

int Bot::pickBestWeapon (int *vec, int count, int moneySave) {
   // this function picks best available weapon from random choice with money save

   if (yb_best_weapon_picker_type.integer () == 1) {
      float buyFactor = (m_moneyAmount - static_cast<float> (moneySave)) / (16000.0f - static_cast<float> (moneySave)) * 3.0f;

      if (buyFactor < 1.0f) {
         buyFactor = 1.0f;
      }

      // swap array values
      for (int *begin = vec, *end = vec + count - 1; begin < end; ++begin, --end) {
         int swap = *end;

         *end = *begin;
         *begin = swap;
      }
      return vec[static_cast<int> (static_cast<float> (count - 1) * log10f (rng.getFloat (1, A_powf (10.0f, buyFactor))) / buyFactor + 0.5f)];
   }

   int chance = 95;

   // high skilled bots almost always prefer best weapon
   if (m_difficulty < 4) {
      if (m_personality == PERSONALITY_NORMAL) {
         chance = 50;
      }
      else if (m_personality == PERSONALITY_CAREFUL) {
         chance = 75;
      }
   }

   for (int i = 0; i < count; i++) {
      auto weapon = &g_weaponSelect[vec[i]];

      // if wea have enough money for weapon buy it
      if (weapon->price + moneySave < m_moneyAmount + rng.getInt (50, 200) && rng.getInt (0, 100) < chance) {
         return vec[i];
      }
   }
   return vec[rng.getInt (0, count - 1)];
}

void Bot::buyStuff (void) {
   // this function does all the work in selecting correct buy menus for most weapons/items

   WeaponSelect *selectedWeapon = nullptr;
   m_nextBuyTime = engine.timebase ();

   if (!m_ignoreBuyDelay) {
      m_nextBuyTime += rng.getFloat (0.3f, 0.5f);
   }

   int count = 0, weaponCount = 0;
   int choices[NUM_WEAPONS];

   // select the priority tab for this personality
   int *ptr = g_weaponPrefs[m_personality] + NUM_WEAPONS;

   bool isPistolMode = g_weaponSelect[25].teamStandard == -1 && g_weaponSelect[3].teamStandard == 2;
   bool teamEcoValid = bots.checkTeamEco (m_team);

   // do this, because xash engine is not capable to run all the features goldsrc, but we have cs 1.6 on it, so buy table must be the same
   bool isOldGame = (g_gameFlags & GAME_LEGACY) && !(g_gameFlags & GAME_XASH_ENGINE);

   switch (m_buyState) {
   case BUYSTATE_PRIMARY_WEAPON: // if no primary weapon and bot has some money, buy a primary weapon
      if ((!hasShield () && !hasPrimaryWeapon () && teamEcoValid) || (teamEcoValid && canReplaceWeapon ())) {
         int moneySave = 0;

         do {
            bool ignoreWeapon = false;

            ptr--;

            assert (*ptr > -1);
            assert (*ptr < NUM_WEAPONS);

            selectedWeapon = &g_weaponSelect[*ptr];
            count++;

            if (selectedWeapon->buyGroup == 1) {
               continue;
            }

            // weapon available for every team?
            if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team) {
               continue;
            }

            // ignore weapon if this weapon not supported by currently running cs version...
            if (isOldGame && selectedWeapon->buySelect == -1) {
               continue;
            }

            // ignore weapon if this weapon is not targeted to out team....
            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team) {
               continue;
            }

            // ignore weapon if this weapon is restricted
            if (isWeaponRestricted (selectedWeapon->id)) {
               continue;
            }

            int *limit = g_botBuyEconomyTable;
            int prostock = 0;

            // filter out weapons with bot economics
            switch (m_personality) {
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

            if (m_team == TEAM_COUNTER) {
               switch (selectedWeapon->id) {
               case WEAPON_TMP:
               case WEAPON_UMP45:
               case WEAPON_P90:
               case WEAPON_MP5:
                  if (m_moneyAmount > limit[ECO_SMG_GT_CT] + prostock) {
                     ignoreWeapon = true;
                  }
                  break;
               }

               if (selectedWeapon->id == WEAPON_SHIELD && m_moneyAmount > limit[ECO_SHIELDGUN_GT]) {
                  ignoreWeapon = true;
               }
            }
            else if (m_team == TEAM_TERRORIST) {
               switch (selectedWeapon->id) {
               case WEAPON_UMP45:
               case WEAPON_MAC10:
               case WEAPON_P90:
               case WEAPON_MP5:
               case WEAPON_SCOUT:
                  if (m_moneyAmount > limit[ECO_SMG_GT_TE] + prostock) {
                     ignoreWeapon = true;
                  }
                  break;
               }
            }

            switch (selectedWeapon->id) {
            case WEAPON_XM1014:
            case WEAPON_M3:
               if (m_moneyAmount < limit[ECO_SHOTGUN_LT]) {
                  ignoreWeapon = true;
               }

               if (m_moneyAmount >= limit[ECO_SHOTGUN_GT]) {
                  ignoreWeapon = false;

               }
               break;
            }

            switch (selectedWeapon->id) {
            case WEAPON_SG550:
            case WEAPON_G3SG1:
            case WEAPON_AWP:
            case WEAPON_M249:
               if (m_moneyAmount < limit[ECO_HEAVY_LT]) {
                  ignoreWeapon = true;

               }

               if (m_moneyAmount >= limit[ECO_HEAVY_GT]) {
                  ignoreWeapon = false;
               }
               break;
            }

            if (ignoreWeapon && g_weaponSelect[25].teamStandard == 1 && yb_economics_rounds.boolean ()) {
               continue;
            }

            // save money for grenade for example?
            moneySave = rng.getInt (500, 1000);

            if (bots.getLastWinner () == m_team) {
               moneySave = 0;
            }

            if (selectedWeapon->price <= (m_moneyAmount - moneySave)) {
               choices[weaponCount++] = *ptr;
            }

         } while (count < NUM_WEAPONS && weaponCount < 4);

         // found a desired weapon?
         if (weaponCount > 0) {
            int chosenWeapon;

            // choose randomly from the best ones...
            if (weaponCount > 1) {
               chosenWeapon = pickBestWeapon (choices, weaponCount, moneySave);
            }
            else {
               chosenWeapon = choices[weaponCount - 1];
            }
            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else {
            selectedWeapon = nullptr;
         }

         if (selectedWeapon != nullptr) {
            engine.execBotCmd (ent (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame) {
               engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->buySelect);
            }
            else {
               if (m_team == TEAM_TERRORIST) {
                  engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->newBuySelectT);
               }
               else {
                  engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->newBuySelectCT);
               }
            }
         }
      }
      else if (hasPrimaryWeapon () && !hasShield ()) {
         m_reloadState = RELOAD_PRIMARY;
         break;
      }
      else if ((hasSecondaryWeapon () && !hasShield ()) || hasShield ()) {
         m_reloadState = RELOAD_SECONDARY;
         break;
      }
      break;

   case BUYSTATE_ARMOR_VESTHELM: // if armor is damaged and bot has some money, buy some armor
      if (pev->armorvalue < rng.getInt (50, 80) && (isPistolMode || (teamEcoValid && hasPrimaryWeapon ()))) {
         // if bot is rich, buy kevlar + helmet, else buy a single kevlar
         if (m_moneyAmount > 1500 && !isWeaponRestricted (WEAPON_ARMORHELM)) {
            engine.execBotCmd (ent (), "buyequip;menuselect 2");
         }
         else if (!isWeaponRestricted (WEAPON_ARMOR)) {
            engine.execBotCmd (ent (), "buyequip;menuselect 1");
         }
      }
      break;

   case BUYSTATE_SECONDARY_WEAPON: // if bot has still some money, buy a better secondary weapon
      if (isPistolMode || (hasPrimaryWeapon () && (pev->weapons & ((1 << WEAPON_USP) | (1 << WEAPON_GLOCK))) && m_moneyAmount > rng.getInt (7500, 9000))) {
         do {
            ptr--;

            assert (*ptr > -1);
            assert (*ptr < NUM_WEAPONS);

            selectedWeapon = &g_weaponSelect[*ptr];
            count++;

            if (selectedWeapon->buyGroup != 1) {
               continue;
            }

            // ignore weapon if this weapon is restricted
            if (isWeaponRestricted (selectedWeapon->id)) {
               continue;
            }

            // weapon available for every team?
            if ((g_mapType & MAP_AS) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team) {
               continue;
            }

            if (isOldGame && selectedWeapon->buySelect == -1) {
               continue;
            }

            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team) {
               continue;
            }

            if (selectedWeapon->price <= (m_moneyAmount - rng.getInt (100, 200))) {
               choices[weaponCount++] = *ptr;
            }

         } while (count < NUM_WEAPONS && weaponCount < 4);

         // found a desired weapon?
         if (weaponCount > 0) {
            int chosenWeapon;

            // choose randomly from the best ones...
            if (weaponCount > 1) {
               chosenWeapon = pickBestWeapon (choices, weaponCount, rng.getInt (100, 200));
            }
            else {
               chosenWeapon = choices[weaponCount - 1];
            }
            selectedWeapon = &g_weaponSelect[chosenWeapon];
         }
         else {
            selectedWeapon = nullptr;
         }

         if (selectedWeapon != nullptr) {
            engine.execBotCmd (ent (), "buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame) {
               engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->buySelect);
            } 
            else {
               if (m_team == TEAM_TERRORIST) {
                  engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->newBuySelectT);
               }
               else {
                  engine.execBotCmd (ent (), "menuselect %d", selectedWeapon->newBuySelectCT);
               }
            }
         }
      }
      break;

   case BUYSTATE_GRENADES: // if bot has still some money, choose if bot should buy a grenade or not
      if (rng.getInt (1, 100) < g_grenadeBuyPrecent[0] && m_moneyAmount >= 400 && !isWeaponRestricted (WEAPON_EXPLOSIVE)) {
         // buy a he grenade
         engine.execBotCmd (ent (), "buyequip");
         engine.execBotCmd (ent (), "menuselect 4");
      }

      if (rng.getInt (1, 100) < g_grenadeBuyPrecent[1] && m_moneyAmount >= 300 && teamEcoValid && !isWeaponRestricted (WEAPON_FLASHBANG)) {
         // buy a concussion grenade, i.e., 'flashbang'
         engine.execBotCmd (ent (), "buyequip");
         engine.execBotCmd (ent (), "menuselect 3");
      }

      if (rng.getInt (1, 100) < g_grenadeBuyPrecent[2] && m_moneyAmount >= 400 && teamEcoValid && !isWeaponRestricted (WEAPON_SMOKE)) {
         // buy a smoke grenade
         engine.execBotCmd (ent (), "buyequip");
         engine.execBotCmd (ent (), "menuselect 5");
      }
      break;

   case BUYSTATE_DEFUSER: // if bot is CT and we're on a bomb map, randomly buy the defuse kit
      if ((g_mapType & MAP_DE) && m_team == TEAM_COUNTER && rng.getInt (1, 100) < 80 && m_moneyAmount > 200 && !isWeaponRestricted (WEAPON_DEFUSER)) {
         if (isOldGame) {
            engine.execBotCmd (ent (), "buyequip;menuselect 6");
         }
         else {
            engine.execBotCmd (ent (), "defuser"); // use alias in steamcs
         }
      }
      break;

   case BUYSTATE_AMMO: // buy enough primary & secondary ammo (do not check for money here)
      for (int i = 0; i <= 5; i++) {
         engine.execBotCmd (ent (), "buyammo%d", rng.getInt (1, 2)); // simulate human
      }

      // buy enough secondary ammo
      if (hasPrimaryWeapon ()) {
         engine.execBotCmd (ent (), "buy;menuselect 7");
      }

      // buy enough primary ammo
      engine.execBotCmd (ent (), "buy;menuselect 6");

      // try to reload secondary weapon
      if (m_reloadState != RELOAD_PRIMARY) {
         m_reloadState = RELOAD_SECONDARY;
      }
      m_ignoreBuyDelay = false;
      break;
   }

   m_buyState++;
   pushMsgQueue (GAME_MSG_PURCHASE);
}

void Bot::updateEmotions (void) {
   // slowly increase/decrease dynamic emotions back to their base level
   if (m_nextEmotionUpdate > engine.timebase ()) {
      return;
   }

   if (m_agressionLevel > m_baseAgressionLevel) {
      m_agressionLevel -= 0.10f;
   }
   else {
      m_agressionLevel += 0.10f;
   }

   if (m_fearLevel > m_baseFearLevel) {
      m_fearLevel -= 0.05f;
   }
   else {
      m_fearLevel += 0.05f;
   }

   if (m_agressionLevel < 0.0f) {
      m_agressionLevel = 0.0f;
   }

   if (m_fearLevel < 0.0f) {
      m_fearLevel = 0.0f;
   }
   m_nextEmotionUpdate = engine.timebase () + 1.0f;
}

void Bot::overrideConditions (void) {
   if (m_currentWeapon != WEAPON_KNIFE && m_difficulty > 3 && ((m_aimFlags & AIM_ENEMY) || (m_states & STATE_SEEING_ENEMY)) && !yb_jasonmode.boolean () && taskId () != TASK_CAMP && taskId () != TASK_SEEKCOVER && !isOnLadder ()) {
      m_moveToGoal = false; // don't move to goal
      m_navTimeset = engine.timebase ();

      if (isPlayer (m_enemy)) {
         attackMovement ();
      }
   }

   // check if we need to escape from bomb
   if ((g_mapType & MAP_DE) && g_bombPlanted && m_notKilled && taskId () != TASK_ESCAPEFROMBOMB && taskId () != TASK_CAMP && isOutOfBombTimer ()) {
      completeTask (); // complete current task

      // then start escape from bomb immidiate
      startTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, INVALID_WAYPOINT_INDEX, 0.0f, true);
   }

   // special handling, if we have a knife in our hands
   if ((g_timeRoundStart + 10.0f > engine.timebase () || !(pev->weapons & (WEAPON_PRIMARY | WEAPON_SECONDARY))) && m_currentWeapon == WEAPON_KNIFE && isPlayer (m_enemy) && (taskId () != TASK_MOVETOPOSITION || task ()->desire != TASKPRI_HIDE)) {
      float length = (pev->origin - m_enemy->v.origin).length2D ();

      // do waypoint movement if enemy is not reacheable with a knife
      if (length > 100.0f && (m_states & STATE_SEEING_ENEMY)) {
         int nearestToEnemyPoint = waypoints.getNearest (m_enemy->v.origin);

         if (nearestToEnemyPoint != INVALID_WAYPOINT_INDEX && nearestToEnemyPoint != m_currentWaypointIndex && A_abs (waypoints[nearestToEnemyPoint].origin.z - m_enemy->v.origin.z) < 16.0f) {
            startTask (TASK_MOVETOPOSITION, TASKPRI_HIDE, nearestToEnemyPoint, engine.timebase () + rng.getFloat (5.0f, 10.0f), true);

            m_isEnemyReachable = false;
            m_enemy = nullptr;

            m_enemyIgnoreTimer = engine.timebase () + ((length / pev->maxspeed) * 0.5f);
         }
      }
   }
}

void Bot::setConditions (void) {
   // this function carried out each frame. does all of the sensing, calculates emotions and finally sets the desired
   // action after applying all of the Filters

   m_aimFlags = 0;

   updateEmotions ();

   // does bot see an enemy?
   if (lookupEnemies ()) {
      m_states |= STATE_SEEING_ENEMY;
   }
   else {
      m_states &= ~STATE_SEEING_ENEMY;
      m_enemy = nullptr;
   }

   // did bot just kill an enemy?
   if (!engine.isNullEntity (m_lastVictim)) {
      if (engine.getTeam (m_lastVictim) != m_team) {
         // add some aggression because we just killed somebody
         m_agressionLevel += 0.1f;

         if (m_agressionLevel > 1.0f) {
            m_agressionLevel = 1.0f;
         }

         if (rng.getInt (1, 100) < 10) {
            pushChatMessage (CHAT_KILLING);
         }

         if (rng.getInt (1, 100) < 10) {
            pushRadioMessage (Radio_EnemyDown);
         }
         else {
            if ((m_lastVictim->v.weapons & (1 << WEAPON_AWP)) || (m_lastVictim->v.weapons & (1 << WEAPON_SCOUT)) || (m_lastVictim->v.weapons & (1 << WEAPON_G3SG1)) || (m_lastVictim->v.weapons & (1 << WEAPON_SG550))) {
               pushChatterMessage (Chatter_SniperKilled);
            }
            else {
               switch (numEnemiesNear (pev->origin, 99999.0f)) {
               case 0:
                  if (rng.getInt (0, 100) < 50) {
                     pushChatterMessage (Chatter_NoEnemiesLeft);
                  }
                  else {
                     pushChatterMessage (Chatter_EnemyDown);
                  }
                  break;

               case 1:
                  pushChatterMessage (Chatter_OneEnemyLeft);
                  break;

               case 2:
                  pushChatterMessage (Chatter_TwoEnemiesLeft);
                  break;

               case 3:
                  pushChatterMessage (Chatter_ThreeEnemiesLeft);
                  break;

               default:
                  pushChatterMessage (Chatter_EnemyDown);
               }
            }
         }

         // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
         if (m_team == TEAM_COUNTER && m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0 && g_bombPlanted) {
            selectWeaponByName ("weapon_knife");
            m_plantedBombWptIndex = locatePlantedC4 ();

            if (isOccupiedPoint (m_plantedBombWptIndex)) {
               instantChatter (Chatter_BombSiteSecured);
            }
         }
      }
      else {
         pushChatMessage (CHAT_TEAMKILL, true);
         pushChatterMessage (Chatter_TeamKill);
      }
      m_lastVictim = nullptr;
   }

   // check if our current enemy is still valid
   if (!engine.isNullEntity (m_lastEnemy)) {
      if (!isAlive (m_lastEnemy) && m_shootAtDeadTime < engine.timebase ()) {
         m_lastEnemyOrigin.nullify ();
         m_lastEnemy = nullptr;
      }
   }
   else {
      m_lastEnemyOrigin.nullify ();
      m_lastEnemy = nullptr;
   }

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (!yb_ignore_enemies.boolean () && m_soundUpdateTime < engine.timebase () && m_blindTime < engine.timebase () && m_seeEnemyTime + 1.0f < engine.timebase ()) {
      processHearing ();
      m_soundUpdateTime = engine.timebase () + 0.25f;
   }
   else if (m_heardSoundTime < engine.timebase ()) {
      m_states &= ~STATE_HEARING_ENEMY;
   }

   if (engine.isNullEntity (m_enemy) && !engine.isNullEntity (m_lastEnemy) && !m_lastEnemyOrigin.empty ()) {
      m_aimFlags |= AIM_PREDICT_PATH;

      if (seesEntity (m_lastEnemyOrigin, true)) {
         m_aimFlags |= AIM_LAST_ENEMY;
      }
   }
   checkGrenadesThrow ();

   // check if there are items needing to be used/collected
   if (m_itemCheckTime < engine.timebase () || !engine.isNullEntity (m_pickupItem)) {
      m_itemCheckTime = engine.timebase () + 0.5f;
      processPickups ();
   }
   filterTasks ();
}

void Bot::filterTasks (void) {
   // initialize & calculate the desire for all actions based on distances, emotions and other stuff
   task ();

   float tempFear = m_fearLevel;
   float tempAgression = m_agressionLevel;

   // decrease fear if teammates near
   int friendlyNum = 0;

   if (!m_lastEnemyOrigin.empty ()) {
      friendlyNum = numFriendsNear (pev->origin, 500.0f) - numEnemiesNear (m_lastEnemyOrigin, 500.0f);
   }

   if (friendlyNum > 0) {
      tempFear = tempFear * 0.5f;
   }

   // increase/decrease fear/aggression if bot uses a sniping weapon to be more careful
   if (usesSniper ()) {
      tempFear = tempFear * 1.2f;
      tempAgression = tempAgression * 0.6f;
   }

   // bot found some item to use?
   if (!engine.isNullEntity (m_pickupItem) && taskId () != TASK_ESCAPEFROMBOMB) {
      m_states |= STATE_PICKUP_ITEM;

      if (m_pickupType == PICKUP_BUTTON) {
         g_taskFilters[TASK_PICKUPITEM].desire = 50.0f; // always pickup button
      }
      else {
         float distance = (500.0f - (engine.getAbsPos (m_pickupItem) - pev->origin).length ()) * 0.2f;

         if (distance > 50.0f) {
            distance = 50.0f;
         }
         g_taskFilters[TASK_PICKUPITEM].desire = distance;
      }
   }
   else {
      m_states &= ~STATE_PICKUP_ITEM;
      g_taskFilters[TASK_PICKUPITEM].desire = 0.0f;
   }

   // calculate desire to attack
   if ((m_states & STATE_SEEING_ENEMY) && reactOnEnemy ()) {
      g_taskFilters[TASK_ATTACK].desire = TASKPRI_ATTACK;
   }
   else {
      g_taskFilters[TASK_ATTACK].desire = 0.0f;
   }
   float &seekCoverDesire = g_taskFilters[TASK_SEEKCOVER].desire;
   float &huntEnemyDesire = g_taskFilters[TASK_HUNTENEMY].desire;
   float &blindedDesire = g_taskFilters[TASK_BLINDED].desire;

   // calculate desires to seek cover or hunt
   if (isPlayer (m_lastEnemy) && !m_lastEnemyOrigin.empty () && !m_hasC4) {
      float retreatLevel = (100.0f - (pev->health > 50.0f ? 100.0f : pev->health)) * tempFear; // retreat level depends on bot health

      if (m_numEnemiesLeft > m_numFriendsLeft * 0.5f && m_retreatTime < engine.timebase () && m_seeEnemyTime - rng.getFloat (2.0f, 4.0f) < engine.timebase ()) {

         float timeSeen = m_seeEnemyTime - engine.timebase ();
         float timeHeard = m_heardSoundTime - engine.timebase ();
         float ratio = 0.0f;

         m_retreatTime = engine.timebase () + rng.getFloat (3.0f, 15.0f);

         if (timeSeen > timeHeard) {
            timeSeen += 10.0f;
            ratio = timeSeen * 0.1f;
         }
         else {
            timeHeard += 10.0f;
            ratio = timeHeard * 0.1f;
         }

         if (g_bombPlanted || m_isStuck || m_currentWeapon == WEAPON_KNIFE) {
            ratio /= 3.0f; // reduce the seek cover desire if bomb is planted
         }
         else if (m_isVIP || m_isReloading || (m_ammoInClip[m_currentWeapon] < getMaxClip (m_currentWeapon) * 0.18f && !usesSniper ())) {
            ratio *= 2.0f; // triple the seek cover desire if bot is VIP or reloading
         }
         else {
            ratio /= 2.0f; // reduce seek cover otherwise
         }
         seekCoverDesire = retreatLevel * ratio;
      }
      else {
         seekCoverDesire = 0.0f;
      }
   
      // if half of the round is over, allow hunting
      if (taskId () != TASK_ESCAPEFROMBOMB && engine.isNullEntity (m_enemy) && g_timeRoundMid < engine.timebase () && !m_isUsingGrenade && m_currentWaypointIndex != waypoints.getNearest (m_lastEnemyOrigin) && m_personality != PERSONALITY_CAREFUL && !yb_ignore_enemies.boolean ()) {
         float desireLevel = 4096.0f - ((1.0f - tempAgression) * (m_lastEnemyOrigin - pev->origin).length ());

         desireLevel = (100.0f * desireLevel) / 4096.0f;
         desireLevel -= retreatLevel;

         if (desireLevel > 89.0f) {
            desireLevel = 89.0f;
         }
         huntEnemyDesire = desireLevel;
      }
      else {
         huntEnemyDesire = 0.0f;
      }
   }
   else {
      huntEnemyDesire = 0.0f;
      seekCoverDesire = 0.0f;
   }

   // blinded behavior
   blindedDesire = m_blindTime > engine.timebase () ? TASKPRI_BLINDED : 0.0f;


   // now we've initialized all the desires go through the hard work
   // of filtering all actions against each other to pick the most
   // rewarding one to the bot.

   // FIXME: instead of going through all of the actions it might be
   // better to use some kind of decision tree to sort out impossible
   // actions.

   // most of the values were found out by trial-and-error and a helper
   // utility i wrote so there could still be some weird behaviors, it's
   // hard to check them all out.

   // this function returns the behavior having the higher activation level
   static auto maxDesire = [] (Task *first, Task *second) {
      if (first->desire > second->desire) {
         return first;
      }
      return second;
   };

   // this function returns the first behavior if its activation level is anything higher than zero
   static auto subsumeDesire = [] (Task *first, Task *second) {
      if (first->desire > 0) {
         return first;
      }
      return second;
   };

   // this function returns the input behavior if it's activation level exceeds the threshold, or some default behavior otherwise
   static auto thresholdDesire = [] (Task *first, float threshold, float desire) {
      if (first->desire < threshold) {
         first->desire = desire;
      }
      return first;
   };

   // this function clamp the inputs to be the last known value outside the [min, max] range.
   static auto hysteresisDesire = [] (float cur, float min, float max, float old) {
      if (cur <= min || cur >= max) {
         old = cur;
      }
      return old;
   };

   m_oldCombatDesire = hysteresisDesire (g_taskFilters[TASK_ATTACK].desire, 40.0f, 90.0f, m_oldCombatDesire);
   g_taskFilters[TASK_ATTACK].desire = m_oldCombatDesire;

   auto offensive = &g_taskFilters[TASK_ATTACK];
   auto pickup = &g_taskFilters[TASK_PICKUPITEM];

   // calc survive (cover/hide)
   auto survive = thresholdDesire (&g_taskFilters[TASK_SEEKCOVER], 40.0f, 0.0f);
   survive = subsumeDesire (&g_taskFilters[TASK_HIDE], survive);

   auto def = thresholdDesire (&g_taskFilters[TASK_HUNTENEMY], 41.0f, 0.0f); // don't allow hunting if desires 60<
   offensive = subsumeDesire (offensive, pickup); // if offensive task, don't allow picking up stuff

   auto sub = maxDesire (offensive, def); // default normal & careful tasks against offensive actions
   auto final = subsumeDesire (&g_taskFilters[TASK_BLINDED], maxDesire (survive, sub)); // reason about fleeing instead

   if (!m_tasks.empty ()) {
      final = maxDesire (final, task ());
      startTask (final->id, final->desire, final->data, final->time, final->resume); // push the final behavior in our task stack to carry out
   }
}

void Bot::resetTasks (void) {
   // this function resets bot tasks stack, by removing all entries from the stack.

   ignoreCollision ();
   m_tasks.clear ();
}

void Bot::startTask (TaskID id, float desire, int data, float time, bool resume) {
   for (auto &task : m_tasks) {
      if (task.id == id) {
         if (!Math::isFltEqual (task.desire, desire)) {
            task.desire = desire;
         }
         return;
      }
   }
   m_tasks.push ({ id, desire, data, time, resume });

   clearSearchNodes ();
   ignoreCollision ();

   int tid = taskId ();

   // leader bot?
   if (m_isLeader && tid == TASK_SEEKCOVER) {
      processTeamCommands (); // reorganize team if fleeing
   }

   if (tid == TASK_CAMP) {
      selectBestWeapon ();
   }

   // this is best place to handle some voice commands report team some info
   if (rng.getInt (0, 100) < 95) {
      if (tid == TASK_BLINDED) {
         instantChatter (Chatter_GotBlinded);
      }
      else if (tid == TASK_PLANTBOMB) {
         instantChatter (Chatter_PlantingC4);
      }
   }

   if (rng.getInt (0, 100) < 80 && tid == TASK_CAMP) {
      if ((g_mapType & MAP_DE) && g_bombPlanted) {
         pushChatterMessage (Chatter_GuardDroppedC4);
      }
      else {
         pushChatterMessage (Chatter_GoingToCamp);
      }
   }

   if (yb_debug_goal.integer () != INVALID_WAYPOINT_INDEX) {
      m_chosenGoalIndex = yb_debug_goal.integer ();
   }
   else {
      m_chosenGoalIndex = task ()->data;
   }

   if (rng.getInt (0, 100) < 80 && tid == TASK_CAMP && m_team == TEAM_TERRORIST && m_inVIPZone) {
      pushChatterMessage (Chatter_GoingToGuardVIPSafety);
   }
}

Task *Bot::task (void) {
   if (m_tasks.empty ()) {
      m_tasks.push ({ TASK_NORMAL, TASKPRI_NORMAL, INVALID_WAYPOINT_INDEX, 0.0f, true });
   }
   return &m_tasks.back ();
}

void Bot::clearTask (TaskID id) {
   // this function removes one task from the bot task stack.

   if (m_tasks.empty () || taskId () == TASK_NORMAL) {
      return; // since normal task can be only once on the stack, don't remove it...
   }

   if (taskId () == id) {
      clearSearchNodes ();
      ignoreCollision ();

      m_tasks.pop ();
      return;
   }

   for (auto &task : m_tasks) {
      if (task.id == id) {
         m_tasks.erase (task);
      }
   }

   ignoreCollision ();
   clearSearchNodes ();
}

void Bot::completeTask (void) {
   // this function called whenever a task is completed.

   ignoreCollision ();

   if (m_tasks.empty ()) {
      return;
   }

   do {
      m_tasks.pop ();
   } while (!m_tasks.empty () && !m_tasks.back ().resume);

   clearSearchNodes ();
}

bool Bot::isEnemyThreat (void) {
   if (engine.isNullEntity (m_enemy) || taskId () == TASK_SEEKCOVER) {
      return false;
   }

   // if bot is camping, he should be firing anyway and not leaving his position
   if (taskId () == TASK_CAMP) {
      return false;
   }

   // if enemy is near or facing us directly
   if ((m_enemy->v.origin - pev->origin).lengthSq () < A_square (256.0f) || isInViewCone (m_enemy->v.origin)) {
      return true;
   }
   return false;
}

bool Bot::reactOnEnemy (void) {
   // the purpose of this function is check if task has to be interrupted because an enemy is near (run attack actions then)

   if (!isEnemyThreat ()) {
      return false;
   }

   if (m_enemyReachableTimer < engine.timebase ()) {
      int ownIndex = m_currentWaypointIndex;

      if (ownIndex == INVALID_WAYPOINT_INDEX) {
         ownIndex = getNearestPoint ();
      }
      int enemyIndex = waypoints.getNearest (m_enemy->v.origin);

      float lineDist = (m_enemy->v.origin - pev->origin).length ();
      float pathDist = static_cast<float> (waypoints.getPathDist (ownIndex, enemyIndex));

      if (pathDist - lineDist > 112.0f) {
         m_isEnemyReachable = false;
      }
      else {
         m_isEnemyReachable = true;
      }
      m_enemyReachableTimer = engine.timebase () + 1.0f;
   }

   if (m_isEnemyReachable) {
      m_navTimeset = engine.timebase (); // override existing movement by attack movement
      return true;
   }
   return false;
}

bool Bot::lastEnemyShootable (void) {
   // don't allow shooting through walls
   if (!(m_aimFlags & AIM_LAST_ENEMY) || m_lastEnemyOrigin.empty () || engine.isNullEntity (m_lastEnemy)) {
      return false;
   }
   return getShootingConeDeviation (ent (), m_lastEnemyOrigin) >= 0.90f && isPenetrableObstacle (m_lastEnemyOrigin);
}

void Bot::checkRadioQueue (void) {
   // this function handling radio and reacting to it

   float distance = (m_radioEntity->v.origin - pev->origin).length ();

   // don't allow bot listen you if bot is busy
   if ((taskId () == TASK_DEFUSEBOMB || taskId () == TASK_PLANTBOMB || hasHostage () || m_hasC4) && m_radioOrder != Radio_ReportTeam) {
      m_radioOrder = 0;
      return;
   }

   switch (m_radioOrder) {
   case Radio_CoverMe:
   case Radio_FollowMe:
   case Radio_StickTogether:
   case Chatter_GoingToPlantBomb:
   case Chatter_CoverMe:
      // check if line of sight to object is not blocked (i.e. visible)
      if ((seesEntity (m_radioEntity->v.origin)) || (m_radioOrder == Radio_StickTogether)) {
         if (engine.isNullEntity (m_targetEntity) && engine.isNullEntity (m_enemy) && rng.getInt (0, 100) < (m_personality == PERSONALITY_CAREFUL ? 80 : 20)) {
            int numFollowers = 0;

            // Check if no more followers are allowed
            for (int i = 0; i < engine.maxClients (); i++) {
               Bot *bot = bots.getBot (i);

               if (bot != nullptr) {
                  if (bot->m_notKilled) {
                     if (bot->m_targetEntity == m_radioEntity) {
                        numFollowers++;
                     }
                  }
               }
            }
            int allowedFollowers = yb_user_max_followers.integer ();

            if (m_radioEntity->v.weapons & (1 << WEAPON_C4)) {
               allowedFollowers = 1;
            }

            if (numFollowers < allowedFollowers) {
               pushRadioMessage (Radio_Affirmative);
               m_targetEntity = m_radioEntity;

               // don't pause/camp/follow anymore
               TaskID taskID = taskId ();

               if (taskID == TASK_PAUSE || taskID == TASK_CAMP) {
                  task ()->time = engine.timebase ();
               }
               startTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, INVALID_WAYPOINT_INDEX, 0.0f, true);
            }
            else if (numFollowers > allowedFollowers) {
               for (int i = 0; (i < engine.maxClients () && numFollowers > allowedFollowers); i++) {
                  Bot *bot = bots.getBot (i);

                  if (bot != nullptr) {
                     if (bot->m_notKilled) {
                        if (bot->m_targetEntity == m_radioEntity) {
                           bot->m_targetEntity = nullptr;
                           numFollowers--;
                        }
                     }
                  }
               }
            }
            else if (m_radioOrder != Chatter_GoingToPlantBomb && rng.getInt (0, 100) < 15) {
               pushRadioMessage (Radio_Negative);
            }
         }
         else if (m_radioOrder != Chatter_GoingToPlantBomb && rng.getInt (0, 100) < 25) {
            pushRadioMessage (Radio_Negative);
         }
      }
      break;

   case Radio_HoldPosition:
      if (!engine.isNullEntity (m_targetEntity)) {
         if (m_targetEntity == m_radioEntity) {
            m_targetEntity = nullptr;
            pushRadioMessage (Radio_Affirmative);

            m_campButtons = 0;

            startTask (TASK_PAUSE, TASKPRI_PAUSE, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (30.0f, 60.0f), false);
         }
      }
      break;

   case Chatter_NewRound:
      pushChatterMessage (Chatter_You_Heard_The_Man);
      break;

   case Radio_TakingFire:
      if (engine.isNullEntity (m_targetEntity)) {
         if (engine.isNullEntity (m_enemy) && m_seeEnemyTime + 4.0f < engine.timebase ()) {
            // decrease fear levels to lower probability of bot seeking cover again
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f) {
               m_fearLevel = 0.0f;
            }

            if (rng.getInt (0, 100) < 45 && yb_communication_type.integer () == 2) {
               pushChatterMessage (Chatter_OnMyWay);
            }
            else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.integer () != 2) {
               pushRadioMessage (Radio_Affirmative);
            }
            tryHeadTowardRadioMessage ();
         }
         else if (rng.getInt (0, 100) < 25) {
            pushRadioMessage (Radio_Negative);
         }
      }
      break;

   case Radio_YouTakePoint:
      if (seesEntity (m_radioEntity->v.origin) && m_isLeader) {
         pushRadioMessage (Radio_Affirmative);
      }
      break;

   case Radio_EnemySpotted:
   case Radio_NeedBackup:
   case Chatter_ScaredEmotion:
   case Chatter_Pinned_Down:
      if (((engine.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 2048.0f || !m_moveToC4) && rng.getInt (0, 100) > 50 && m_seeEnemyTime + 4.0f < engine.timebase ()) {
         m_fearLevel -= 0.1f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }

         if (rng.getInt (0, 100) < 45 && yb_communication_type.integer () == 2) {
            pushChatterMessage (Chatter_OnMyWay);
         }
         else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.integer () != 2) {
            pushRadioMessage (Radio_Affirmative);
         }
         tryHeadTowardRadioMessage ();
      }
      else if (rng.getInt (0, 100) < 30 && m_radioOrder == Radio_NeedBackup) {
         pushRadioMessage (Radio_Negative);
      }
      break;

   case Radio_GoGoGo:
      if (m_radioEntity == m_targetEntity) {
         if (rng.getInt (0, 100) < 45 && yb_communication_type.integer () == 2) {
            pushRadioMessage (Radio_Affirmative);
         }
         else if (m_radioOrder == Radio_NeedBackup && yb_communication_type.integer () != 2) {
            pushRadioMessage (Radio_Affirmative);
         }

         m_targetEntity = nullptr;
         m_fearLevel -= 0.2f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }
      }
      else if ((engine.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 2048.0f) {
         TaskID taskID = taskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP) {
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f) {
               m_fearLevel = 0.0f;
            }

            pushRadioMessage (Radio_Affirmative);
            // don't pause/camp anymore
            task ()->time = engine.timebase ();

            m_targetEntity = nullptr;
            makeVectors (m_radioEntity->v.v_angle);

            m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * rng.getFloat (1024.0f, 2048.0f);

            clearSearchNodes ();
            startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);
         }
      }
      else if (!engine.isNullEntity (m_doubleJumpEntity)) {
         pushRadioMessage (Radio_Affirmative);
         resetDoubleJump ();
      }
      else if (rng.getInt (0, 100) < 35) {
         pushRadioMessage (Radio_Negative);
      }
      break;

   case Radio_ShesGonnaBlow:
      if (engine.isNullEntity (m_enemy) && distance < 2048.0f && g_bombPlanted && m_team == TEAM_TERRORIST) {
         pushRadioMessage (Radio_Affirmative);

         if (taskId () == TASK_CAMP) {
            clearTask (TASK_CAMP);
         }
         m_targetEntity = nullptr;
         startTask (TASK_ESCAPEFROMBOMB, TASKPRI_ESCAPEFROMBOMB, INVALID_WAYPOINT_INDEX, 0.0f, true);
      }
      else if (rng.getInt (0, 100) < 35) {
         pushRadioMessage (Radio_Negative);
      }
      break;

   case Radio_RegroupTeam:
      // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
      if (m_team == TEAM_COUNTER && m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0 && g_bombPlanted && taskId () != TASK_DEFUSEBOMB) {
         selectWeaponByName ("weapon_knife");

         clearSearchNodes ();

         m_position = waypoints.getBombPos ();
         startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);

         pushRadioMessage (Radio_Affirmative);
      }
      break;

   case Radio_StormTheFront:
      if (((engine.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) && rng.getInt (0, 100) > 50) {
         pushRadioMessage (Radio_Affirmative);

         // don't pause/camp anymore
         TaskID taskID = taskId ();

         if (taskID == TASK_PAUSE || taskID == TASK_CAMP) {
            task ()->time = engine.timebase ();
         }
         m_targetEntity = nullptr;

         makeVectors (m_radioEntity->v.v_angle);
         m_position = m_radioEntity->v.origin + g_pGlobals->v_forward * rng.getFloat (1024.0f, 2048.0f);

         clearSearchNodes ();
         startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);

         m_fearLevel -= 0.3f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }
         m_agressionLevel += 0.3f;

         if (m_agressionLevel > 1.0f) {
            m_agressionLevel = 1.0f;
         }
      }
      break;

   case Radio_Fallback:
      if ((engine.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) {
         m_fearLevel += 0.5f;

         if (m_fearLevel > 1.0f) {
            m_fearLevel = 1.0f;
         }
         m_agressionLevel -= 0.5f;

         if (m_agressionLevel < 0.0f) {
            m_agressionLevel = 0.0f;
         }
         if (taskId () == TASK_CAMP) {
            task ()->time += rng.getFloat (10.0f, 15.0f);
         }
         else {
            // don't pause/camp anymore
            TaskID taskID = taskId ();

            if (taskID == TASK_PAUSE) {
               task ()->time = engine.timebase ();
            }
            m_targetEntity = nullptr;
            m_seeEnemyTime = engine.timebase ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistance = 99999.0f;

               // take nearest enemy to ordering player
               for (int i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team) {
                     continue;
                  }
                  edict_t *enemy = client.ent;
                  float curDist = (m_radioEntity->v.origin - enemy->v.origin).lengthSq ();

                  if (curDist < nearestDistance) {
                     nearestDistance = curDist;

                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            clearSearchNodes ();
         }
      }
      break;

   case Radio_ReportTeam:
      if (rng.getInt (0, 100) < 30) {
         pushRadioMessage ((numEnemiesNear (pev->origin, 400.0f) == 0 && yb_communication_type.integer () != 2) ? Radio_SectorClear : Radio_ReportingIn);
      }
      break;

   case Radio_SectorClear:
      // is bomb planted and it's a ct
      if (!g_bombPlanted) {
         break;
      }

      // check if it's a ct command
      if (engine.getTeam (m_radioEntity) == TEAM_COUNTER && m_team == TEAM_COUNTER && isFakeClient (m_radioEntity) && g_timeNextBombUpdate < engine.timebase ()) {
         float minDistance = 99999.0f;
         int bombPoint = INVALID_WAYPOINT_INDEX;

         // find nearest bomb waypoint to player
         for (auto &point : waypoints.m_goalPoints) {
            distance = (waypoints[point].origin - m_radioEntity->v.origin).lengthSq ();

            if (distance < minDistance) {
               minDistance = distance;
               bombPoint = point;
            }
         }

         // mark this waypoint as restricted point
         if (bombPoint != INVALID_WAYPOINT_INDEX && !waypoints.isVisited (bombPoint)) {
            // does this bot want to defuse?
            if (taskId () == TASK_NORMAL) {
               // is he approaching this goal?
               if (task ()->data == bombPoint) {
                  task ()->data = INVALID_WAYPOINT_INDEX;
                  pushRadioMessage (Radio_Affirmative);
               }
            }
            waypoints.setVisited (bombPoint);
         }
         g_timeNextBombUpdate = engine.timebase () + 0.5f;
      }
      break;

   case Radio_GetInPosition:
      if ((engine.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) {
         pushRadioMessage (Radio_Affirmative);

         if (taskId () == TASK_CAMP) {
            task ()->time = engine.timebase () + rng.getFloat (30.0f, 60.0f);
         }
         else {
            // don't pause anymore
            TaskID taskID = taskId ();

            if (taskID == TASK_PAUSE) {
               task ()->time = engine.timebase ();
            }

            m_targetEntity = nullptr;
            m_seeEnemyTime = engine.timebase ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistance = 99999.0f;

               // take nearest enemy to ordering player
               for (int i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team)
                     continue;

                  edict_t *enemy = client.ent;
                  float dist = (m_radioEntity->v.origin - enemy->v.origin).lengthSq ();

                  if (dist < nearestDistance) {
                     nearestDistance = dist;
                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            clearSearchNodes ();

            int index = getDefendPoint (m_radioEntity->v.origin);

            // push camp task on to stack
            startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (30.0f, 60.0f), true);
            // push move command
            startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + rng.getFloat (30.0f, 60.0f), true);

            if (waypoints[index].vis.crouch <= waypoints[index].vis.stand) {
               m_campButtons |= IN_DUCK;
            }
            else {
               m_campButtons &= ~IN_DUCK;
            }
         }
      }
      break;
   }
   m_radioOrder = 0; // radio command has been handled, reset
}

void Bot::tryHeadTowardRadioMessage (void) {
   TaskID taskID = taskId ();

   if (taskID == TASK_MOVETOPOSITION || m_headedTime + 15.0f < engine.timebase () || !isAlive (m_radioEntity) || m_hasC4)
      return;

   if ((isFakeClient (m_radioEntity) && rng.getInt (0, 100) < 25 && m_personality == PERSONALITY_NORMAL) || !(m_radioEntity->v.flags & FL_FAKECLIENT)) {
      if (taskID == TASK_PAUSE || taskID == TASK_CAMP) {
         task ()->time = engine.timebase ();
      }
      m_headedTime = engine.timebase ();
      m_position = m_radioEntity->v.origin;

      clearSearchNodes ();
      startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, INVALID_WAYPOINT_INDEX, 0.0f, true);
   }
}

void Bot::updateAimDir (void) {
   unsigned int flags = m_aimFlags;

   // don't allow bot to look at danger positions under certain circumstances
   if (!(flags & (AIM_GRENADE | AIM_ENEMY | AIM_ENTITY))) {
      if (isOnLadder () || isInWater () || (m_waypointFlags & FLAG_LADDER) || (m_currentTravelFlags & PATHFLAG_JUMP)) {
         flags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
         m_canChooseAimDirection = false;
      }
   }

   if (flags & AIM_OVERRIDE) {
      m_lookAt = m_camp;
   }
   else if (flags & AIM_GRENADE) {
      m_lookAt = m_throw;

      float throwDistance = (m_throw - pev->origin).length ();
      float coordCorrection = 0.0f;
      float angleCorrection = 0.0f;

      if (throwDistance > 100.0f && throwDistance < 800.0f) {
         angleCorrection = 0.0f;
         coordCorrection = 0.25f * (m_throw.z - pev->origin.z);
      }
      else if (throwDistance >= 800.0f) {
         angleCorrection = 37.0f * (throwDistance - 800.0f) / 800.0f;

         if (angleCorrection > 45.0f) {
            angleCorrection = 45.0f;
         }
         coordCorrection = throwDistance * A_tanf (angleCorrection * MATH_D2R) + 0.25f * (m_throw.z - pev->origin.z);
      }
      m_lookAt.z += coordCorrection * 0.5f;
   }
   else if (flags & AIM_ENEMY) {
      FocusEnemy ();
   }
   else if (flags & AIM_ENTITY) {
      m_lookAt = m_entity;
   }
   else if (flags & AIM_LAST_ENEMY) {
      m_lookAt = m_lastEnemyOrigin;

      // did bot just see enemy and is quite aggressive?
      if (m_seeEnemyTime + 1.0f - m_actualReactionTime + m_baseAgressionLevel > engine.timebase ()) {

         // feel free to fire if shootable
         if (!usesSniper () && lastEnemyShootable ()) {
            m_wantsToFire = true;
         }
      }
   }
   else if (flags & AIM_PREDICT_PATH) {
      bool changePredictedEnemy = true;

      if (m_trackingEdict == m_lastEnemy && isAlive (m_lastEnemy) && m_timeNextTracking < engine.timebase ()) {
         changePredictedEnemy = false;
      }

      if (changePredictedEnemy) {
         int aimPoint = searchAimingPoint (m_lastEnemyOrigin);

         if (aimPoint > 0) {
            m_lookAt = waypoints[aimPoint].origin;
            m_camp = m_lookAt;

            m_timeNextTracking = engine.timebase () + 1.25f;
            m_trackingEdict = m_lastEnemy;
         }
         else {
            m_aimFlags &= ~AIM_PREDICT_PATH;
            m_destOrigin = m_destOrigin;
         }
      }
      else {
         m_lookAt = m_camp;
      }
   }
   else if (flags & AIM_CAMP) {
      m_lookAt = m_camp;
   }
   else if (flags & AIM_NAVPOINT) {
      m_lookAt = m_destOrigin;

      if (m_canChooseAimDirection && m_currentWaypointIndex != INVALID_WAYPOINT_INDEX && !(m_currentPath->flags & FLAG_LADDER)) {
         auto experience = (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + m_currentWaypointIndex);

         if (m_team == TEAM_TERRORIST) {
            if (experience->team0DangerIndex != INVALID_WAYPOINT_INDEX && waypoints.isVisible (m_currentWaypointIndex, experience->team0DangerIndex)) {
               m_lookAt = waypoints[experience->team0DangerIndex].origin;
            }
         }
         else {
            if (experience->team1DangerIndex != INVALID_WAYPOINT_INDEX && waypoints.isVisible (m_currentWaypointIndex, experience->team1DangerIndex)) {
               m_lookAt = waypoints[experience->team1DangerIndex].origin;
            }
         }
      }
   }

   if (m_lookAt.empty ()) {
      m_lookAt = m_destOrigin;
   }
}

void Bot::framePeriodic (void) {
   if (m_thinkFps <= engine.timebase ()) {
      // execute delayed think
      processFrameThink ();

      // skip some frames
      m_thinkFps = engine.timebase () + m_thinkInterval;
   }
   else {
      processLookAngles ();
   }
}

void Bot::processFrameThink (void) {
   pev->button = 0;
   pev->flags |= FL_FAKECLIENT; // restore fake client bit, if it were removed by some evil action =)

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;
   m_moveAngles.nullify ();

   m_canChooseAimDirection = true;
   m_notKilled = isAlive (ent ());
   m_team = engine.getTeam (ent ());

   if ((g_mapType & MAP_AS) && !m_isVIP) {
      m_isVIP = isPlayerVIP (ent ());
   }

   if (m_team == TEAM_TERRORIST && (g_mapType & MAP_DE)) {
      m_hasC4 = !!(pev->weapons & (1 << WEAPON_C4));
   }

   // is bot movement enabled
   bool botMovement = false;

   // if the bot hasn't selected stuff to start the game yet, go do that...
   if (m_notStarted) {
      processTeamJoin (); // select team & class
   }
   else if (!m_notKilled) {
      // no movement allowed in

       // we got a teamkiller? vote him away...
      if (m_voteKickIndex != m_lastVoteKick && yb_tkpunish.boolean ()) {
         engine.execBotCmd (ent (), "vote %d", m_voteKickIndex);
         m_lastVoteKick = m_voteKickIndex;

         // if bot tk punishment is enabled slay the tk
         if (yb_tkpunish.integer () != 2 || isFakeClient (engine.entityOfIndex (m_voteKickIndex))) {
            return;
         }
         edict_t *killer = engine.entityOfIndex (m_lastVoteKick);

         killer->v.frags++;
         MDLL_ClientKill (killer);
      }

      // host wants us to kick someone
      else if (m_voteMap != 0) {
         engine.execBotCmd (ent (), "votemap %d", m_voteMap);
         m_voteMap = 0;
      }
   }
   else if (m_buyingFinished && !(pev->maxspeed < 10.0f && taskId () != TASK_PLANTBOMB && taskId () != TASK_DEFUSEBOMB) && !yb_freeze_bots.boolean () && !waypoints.hasChanged ()) {
      botMovement = true;
   }
   checkMsgQueue (); // check for pending messages

   if (botMovement) {
      ai (); // execute main code
   }
   processMovement (); // run the player movement
}

void Bot::frame (void) {
   if (m_timePeriodicUpdate > engine.timebase ()) {
      return;
   }

   m_numFriendsLeft = numFriendsNear (pev->origin, 99999.0f);
   m_numEnemiesLeft = numEnemiesNear (pev->origin, 99999.0f);

   if (g_bombPlanted && m_team == TEAM_COUNTER) {
      const Vector &bombPosition = waypoints.getBombPos ();

      if (!m_hasProgressBar && taskId () != TASK_ESCAPEFROMBOMB && (pev->origin - bombPosition).length () < 700.0f && !isBombDefusing (bombPosition)) {
         resetTasks ();
      }
   }
   checkSpawnConditions ();

   extern ConVar yb_chat;

   // bot chatting turned on?
   if (!m_notKilled && yb_chat.boolean () && m_lastChatTime + 10.0 < engine.timebase () && g_lastChatTime + 5.0f < engine.timebase () && !isReplyingToChat ()) {
      // say a text every now and then
      if (rng.getInt (1, 1500) < 50) {
         m_lastChatTime = engine.timebase ();
         g_lastChatTime = engine.timebase ();

         const String &phrase = g_chatFactory[CHAT_DEAD].random ();
         bool sayBufferExists = false;

         // search for last messages, sayed
         for (auto &sentence : m_sayTextBuffer.lastUsedSentences) {
            if (strncmp (sentence.chars (), phrase.chars (), sentence.length ()) == 0) {
               sayBufferExists = true;
               break;
            }
         }

         if (!sayBufferExists) {
            prepareChatMessage (const_cast<char *> (phrase.chars ()));
            pushMsgQueue (GAME_MSG_SAY_CMD);

            // add to ignore list
            m_sayTextBuffer.lastUsedSentences.push (phrase);
         }

         // clear the used line buffer every now and then
         if (static_cast<int> (m_sayTextBuffer.lastUsedSentences.length ()) > rng.getInt (4, 6)) {
            m_sayTextBuffer.lastUsedSentences.clear ();
         }
      }
   }

   if (g_gameFlags & GAME_SUPPORT_BOT_VOICE) {
      showChaterIcon (false); // end voice feedback
   }

   // clear enemy far away
   if (!m_lastEnemyOrigin.empty () && !engine.isNullEntity (m_lastEnemy) && (pev->origin - m_lastEnemyOrigin).length () >= 1600.0f) {
      m_lastEnemy = nullptr;
      m_lastEnemyOrigin.nullify ();
   }
   m_timePeriodicUpdate = engine.timebase () + 0.5f;
}

void Bot::normal_ (void) {
   m_aimFlags |= AIM_NAVPOINT;

   // user forced a waypoint as a goal?
   if (yb_debug_goal.integer () != INVALID_WAYPOINT_INDEX && task ()->data != yb_debug_goal.integer ()) {
      clearSearchNodes ();
      task ()->data = yb_debug_goal.integer ();
   }

   // bots rushing with knife, when have no enemy (thanks for idea to nicebot project)
   if (m_currentWeapon == WEAPON_KNIFE && (engine.isNullEntity (m_lastEnemy) || !isAlive (m_lastEnemy)) && engine.isNullEntity (m_enemy) && m_knifeAttackTime < engine.timebase () && !hasShield () && numFriendsNear (pev->origin, 96.0f) == 0) {
      if (rng.getInt (0, 100) < 40) {
         pev->button |= IN_ATTACK;
      }
      else {
         pev->button |= IN_ATTACK2;
      }
      m_knifeAttackTime = engine.timebase () + rng.getFloat (2.5f, 6.0f);
   }

   if (m_reloadState == RELOAD_NONE && ammo () != 0 && ammoClip () < 5 && g_weaponDefs[m_currentWeapon].ammo1 != -1) {
      m_reloadState = RELOAD_PRIMARY;
   }

   // if bomb planted and it's a CT calculate new path to bomb point if he's not already heading for
   if (!m_bombSearchOverridden && g_bombPlanted && m_team == TEAM_COUNTER && task ()->data != INVALID_WAYPOINT_INDEX && !(waypoints[task ()->data].flags & FLAG_GOAL) && taskId () != TASK_ESCAPEFROMBOMB) {
      clearSearchNodes ();
      task ()->data = INVALID_WAYPOINT_INDEX;
   }

   if (!g_bombPlanted && m_currentWaypointIndex != INVALID_WAYPOINT_INDEX && (m_currentPath->flags & FLAG_GOAL) && rng.getInt (0, 100) < 50 && numEnemiesNear (pev->origin, 650.0f) == 0) {
      pushRadioMessage (Radio_SectorClear);
   }

   // reached the destination (goal) waypoint?
   if (processNavigation ()) {
      completeTask ();
      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;

      // spray logo sometimes if allowed to do so
      if (m_timeLogoSpray < engine.timebase () && yb_spraypaints.boolean () && rng.getInt (1, 100) < 60 && m_moveSpeed > getShiftSpeed () && engine.isNullEntity (m_pickupItem)) {
         if (!((g_mapType & MAP_DE) && g_bombPlanted && m_team == TEAM_COUNTER)) {
            startTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, INVALID_WAYPOINT_INDEX, engine.timebase () + 1.0f, false);
         }
      }

      // reached waypoint is a camp waypoint
      if ((m_currentPath->flags & FLAG_CAMP) && !(g_gameFlags & GAME_CSDM) && yb_camping_allowed.boolean ()) {

         // check if bot has got a primary weapon and hasn't camped before
         if (hasPrimaryWeapon () && m_timeCamping + 10.0f < engine.timebase () && !hasHostage ()) {
            bool campingAllowed = true;

            // Check if it's not allowed for this team to camp here
            if (m_team == TEAM_TERRORIST) {
               if (m_currentPath->flags & FLAG_CF_ONLY) {
                  campingAllowed = false;
               }
            }
            else {
               if (m_currentPath->flags & FLAG_TF_ONLY) {
                  campingAllowed = false;
               }
            }

            // don't allow vip on as_ maps to camp + don't allow terrorist carrying c4 to camp
            if (campingAllowed && (m_isVIP || ((g_mapType & MAP_DE) && m_team == TEAM_TERRORIST && !g_bombPlanted && m_hasC4))) {
               campingAllowed = false;
            }

            // check if another bot is already camping here
            if (campingAllowed && isOccupiedPoint (m_currentWaypointIndex)) {
               campingAllowed = false;
            }

            if (campingAllowed) {
               // crouched camping here?
               if (m_currentPath->flags & FLAG_CROUCH) {
                  m_campButtons = IN_DUCK;
               }
               else {
                  m_campButtons = 0;
               }
               selectBestWeapon ();

               if (!(m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) && !m_reloadState) {
                  m_reloadState = RELOAD_PRIMARY;
               }
               makeVectors (pev->v_angle);

               m_timeCamping = engine.timebase () + rng.getFloat (10.0f, 25.0f);
               startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, m_timeCamping, true);

               m_camp = Vector (m_currentPath->campStartX, m_currentPath->campStartY, 0.0f);
               m_aimFlags |= AIM_CAMP;
               m_campDirection = 0;

               // tell the world we're camping
               if (rng.getInt (0, 100) < 40) {
                  pushRadioMessage (Radio_InPosition);
               }
               m_moveToGoal = false;
               m_checkTerrain = false;

               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;
            }
         }
      }
      else {
         // some goal waypoints are map dependant so check it out...
         if (g_mapType & MAP_CS) {
            // CT Bot has some hostages following?
            if (m_team == TEAM_COUNTER && hasHostage ()) {
               // and reached a Rescue Point?
               if (m_currentPath->flags & FLAG_RESCUE) {
                  for (int i = 0; i < MAX_HOSTAGES; i++) {
                     m_hostages[i] = nullptr; // clear array of hostage pointers
                  }
               }
            }
            else if (m_team == TEAM_TERRORIST && rng.getInt (0, 100) < 75) {
               int index = getDefendPoint (m_currentPath->origin);

               startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (60.0f, 120.0f), true); // push camp task on to stack
               startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + rng.getFloat (5.0f, 10.0f), true); // push move command

               auto &path = waypoints[index];

               // decide to duck or not to duck
               if (path.vis.crouch <= path.vis.stand) {
                  m_campButtons |= IN_DUCK;
               }
               else {
                  m_campButtons &= ~IN_DUCK;
               }
               pushChatterMessage (Chatter_GoingToGuardVIPSafety); // play info about that
            }
         }
         else if ((g_mapType & MAP_DE) && ((m_currentPath->flags & FLAG_GOAL) || m_inBombZone)) {
            // is it a terrorist carrying the bomb?
            if (m_hasC4) {
               if ((m_states & STATE_SEEING_ENEMY) && numFriendsNear (pev->origin, 768.0f) == 0) {
                  // request an help also
                  pushRadioMessage (Radio_NeedBackup);
                  instantChatter (Chatter_ScaredEmotion);

                  startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (4.0f, 8.0f), true);
               }
               else {
                  startTask (TASK_PLANTBOMB, TASKPRI_PLANTBOMB, INVALID_WAYPOINT_INDEX, 0.0f, false);
               }
            }
            else if (m_team == TEAM_COUNTER) {
               if (!g_bombPlanted && numFriendsNear (pev->origin, 210.0f) < 4) {
                  int index = getDefendPoint (m_currentPath->origin);

                  float campTime = rng.getFloat (25.0f, 40.f);

                  // rusher bots don't like to camp too much
                  if (m_personality == PERSONALITY_RUSHER) {
                     campTime *= 0.5f;
                  }
                  startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + campTime, true); // push camp task on to stack
                  startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + rng.getFloat (5.0f, 11.0f), true); // push move command

                  auto &path = waypoints[index];

                  // decide to duck or not to duck
                  if (path.vis.crouch <= path.vis.stand) {
                     m_campButtons |= IN_DUCK;
                  }
                  else {
                     m_campButtons &= ~IN_DUCK;
                  }
                  pushChatterMessage (Chatter_DefendingBombSite); // play info about that
               }
            }
         }
      }
   }
   // no more nodes to follow - search new ones (or we have a bomb)
   else if (!isGoalValid ()) {
      m_moveSpeed = 0.0f;
      clearSearchNodes ();

      // did we already decide about a goal before?
      int destIndex = task ()->data != INVALID_WAYPOINT_INDEX ? task ()->data : getGoal ();

      m_prevGoalIndex = destIndex;
      m_chosenGoalIndex = destIndex;

      // remember index
      task ()->data = destIndex;

      // do pathfinding if it's not the current waypoint
      if (destIndex != m_currentWaypointIndex) {
         searchPath (m_currentWaypointIndex, destIndex, m_pathType);
      }
   }
   else {
      if (!(pev->flags & FL_DUCKING) && m_minSpeed != pev->maxspeed) {
         m_moveSpeed = m_minSpeed;
      }
   }

   if ((yb_walking_allowed.boolean () && mp_footsteps.boolean ()) && m_difficulty >= 3 && !(m_aimFlags & AIM_ENEMY) && (m_heardSoundTime + 6.0f >= engine.timebase () || (m_states & STATE_SUSPECT_ENEMY)) && numEnemiesNear (pev->origin, 768.0f) >= 1 && !yb_jasonmode.boolean () && !g_bombPlanted) {
      m_moveSpeed = getShiftSpeed ();
   }

   // bot hasn't seen anything in a long time and is asking his teammates to report in
   if (m_seeEnemyTime + rng.getFloat (45.0f, 80.0f) < engine.timebase () && rng.getInt (0, 100) < 30 && g_timeRoundStart + 20.0f < engine.timebase () && m_askCheckTime < engine.timebase ()) {
      m_askCheckTime = engine.timebase () + rng.getFloat (45.0f, 80.0f);
      pushRadioMessage (Radio_ReportTeam);
   }
}

void Bot::spraypaint_ (void) {
   m_aimFlags |= AIM_ENTITY;

   // bot didn't spray this round?
   if (m_timeLogoSpray < engine.timebase () && task ()->time > engine.timebase ()) {
      makeVectors (pev->v_angle);
      Vector sprayOrigin = eyePos () + g_pGlobals->v_forward * 128.0f;

      TraceResult tr;
      engine.testLine (eyePos (), sprayOrigin, TRACE_IGNORE_MONSTERS, ent (), &tr);

      // no wall in front?
      if (tr.flFraction >= 1.0f)
         sprayOrigin.z -= 128.0f;

      m_entity = sprayOrigin;

      if (task ()->time - 0.5f < engine.timebase ()) {
         // emit spraycan sound
         g_engfuncs.pfnEmitSound (ent (), CHAN_VOICE, "player/sprayer.wav", 1.0f, ATTN_NORM, 0, 100);
         engine.testLine (eyePos (), eyePos () + g_pGlobals->v_forward * 128.0f, TRACE_IGNORE_MONSTERS, ent (), &tr);

         // paint the actual logo decal
         traceDecals (pev, &tr, m_logotypeIndex);
         m_timeLogoSpray = engine.timebase () + rng.getFloat (60.0f, 90.0f);
      }
   }
   else {
      completeTask ();
   }
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = engine.timebase ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   ignoreCollision ();
}

void Bot::huntEnemy_ (void) {
   m_aimFlags |= AIM_NAVPOINT;
   
   // if we've got new enemy...
   if (!engine.isNullEntity (m_enemy) || engine.isNullEntity (m_lastEnemy)) {
      // forget about it...
      clearTask (TASK_HUNTENEMY);
      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
   }
   else if (engine.getTeam (m_lastEnemy) == m_team) {
      // don't hunt down our teammate...
      clearTask (TASK_HUNTENEMY);
      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
      m_lastEnemy = nullptr;
   }
   else if (processNavigation ()) // reached last enemy pos?
   {
      // forget about it...
      completeTask ();

      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
      m_lastEnemyOrigin.nullify ();
   }
   else if (!isGoalValid ()) // do we need to calculate a new path?
   {
      clearSearchNodes ();

      int destIndex = INVALID_WAYPOINT_INDEX;

      // is there a remembered index?
      if (task ()->data != INVALID_WAYPOINT_INDEX && task ()->data < g_numWaypoints) {
         destIndex = task ()->data;
      }

      // find new one instead
      else {
         destIndex = waypoints.getNearest (m_lastEnemyOrigin);
      }

      // remember index
      m_prevGoalIndex = destIndex;
      task ()->data = destIndex;

      if (destIndex != m_currentWaypointIndex) {
         searchPath (m_currentWaypointIndex, destIndex, m_pathType);
      }
   }

   // bots skill higher than 60?
   if (yb_walking_allowed.boolean () && mp_footsteps.boolean () && m_difficulty >= 2 && !yb_jasonmode.boolean ()) {
      // then make him move slow if near enemy
      if (!(m_currentTravelFlags & PATHFLAG_JUMP)) {
         if (m_currentWaypointIndex != INVALID_WAYPOINT_INDEX) {
            if (m_currentPath->radius < 32.0f && !isOnLadder () && !isInWater () && m_seeEnemyTime + 4.0f > engine.timebase () && m_difficulty < 2) {
               pev->button |= IN_DUCK;
            }
         }

         if ((m_lastEnemyOrigin - pev->origin).length () < 512.0f) {
            m_moveSpeed = getShiftSpeed ();
         }
      }
   }
}

void Bot::seekCover_ (void) {
   m_aimFlags |= AIM_NAVPOINT;

   if (!isAlive (m_lastEnemy)) {
      completeTask ();
      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
   }

   // reached final waypoint?
   else if (processNavigation ()) {
      // yep. activate hide behaviour
      completeTask ();

      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
      m_pathType = SEARCH_PATH_FASTEST;

      // start hide task
      startTask (TASK_HIDE, TASKPRI_HIDE, INVALID_WAYPOINT_INDEX, engine.timebase () + rng.getFloat (3.0f, 12.0f), false);
      Vector dest = m_lastEnemyOrigin;

      // get a valid look direction
      getCampDir (&dest);

      m_aimFlags |= AIM_CAMP;
      m_camp = dest;
      m_campDirection = 0;

      // chosen waypoint is a camp waypoint?
      if (m_currentPath->flags & FLAG_CAMP) {
         // use the existing camp wpt prefs
         if (m_currentPath->flags & FLAG_CROUCH) {
            m_campButtons = IN_DUCK;
         }
         else {
            m_campButtons = 0;
         }
      }
      else {
         // choose a crouch or stand pos
         if (m_currentPath->vis.crouch <= m_currentPath->vis.stand) {
            m_campButtons = IN_DUCK;
         }
         else {
            m_campButtons = 0;
         }

         // enter look direction from previously calculated positions
         m_currentPath->campStartX = dest.x;
         m_currentPath->campStartY = dest.y;

         m_currentPath->campEndX = dest.x;
         m_currentPath->campEndY = dest.y;
      }

      if (m_reloadState == RELOAD_NONE && ammoClip () < 5 && ammo () != 0) {
         m_reloadState = RELOAD_PRIMARY;
      }
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_moveToGoal = false;
      m_checkTerrain = false;
   }
   else if (!isGoalValid ()) // we didn't choose a cover waypoint yet or lost it due to an attack?
   {
      clearSearchNodes ();
      int destIndex = INVALID_WAYPOINT_INDEX;

      if (task ()->data != INVALID_WAYPOINT_INDEX) {
         destIndex = task ()->data;
      }
      else {
         destIndex = getCoverPoint (usesSniper () ? 256.0f : 512.0f);

         if (destIndex == INVALID_WAYPOINT_INDEX) {
            m_retreatTime = engine.timebase () + rng.getFloat (5.0f, 10.0f);
            m_prevGoalIndex = INVALID_WAYPOINT_INDEX;

            completeTask ();
            return;
         }
      }
      m_campDirection = 0;

      m_prevGoalIndex = destIndex;
      task ()->data = destIndex;

      if (destIndex != m_currentWaypointIndex) {
         searchPath (m_currentWaypointIndex, destIndex, SEARCH_PATH_FASTEST);
      }
   }
}

void Bot::attackEnemy_ (void) {
   m_moveToGoal = false;
   m_checkTerrain = false;

   if (!engine.isNullEntity (m_enemy)) {
      ignoreCollision ();

      if (isOnLadder ()) {
         pev->button |= IN_JUMP;
         clearSearchNodes ();
      }
      attackMovement ();

      if (m_currentWeapon == WEAPON_KNIFE && !m_lastEnemyOrigin.empty ()) {
         m_destOrigin = m_lastEnemyOrigin;
      }
   }
   else {
      completeTask ();
      m_destOrigin = m_lastEnemyOrigin;
   }
   m_navTimeset = engine.timebase ();
}

void Bot::pause_ (void) {
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = engine.timebase ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   m_aimFlags |= AIM_NAVPOINT;

   // is bot blinded and above average difficulty?
   if (m_viewDistance < 500.0f && m_difficulty >= 2) {
      // go mad!
      m_moveSpeed = -A_abs ((m_viewDistance - 500.0f) * 0.5f);

      if (m_moveSpeed < -pev->maxspeed) {
         m_moveSpeed = -pev->maxspeed;
      }
      makeVectors (pev->v_angle);
      m_camp = eyePos () + g_pGlobals->v_forward * 500.0f;

      m_aimFlags |= AIM_OVERRIDE;
      m_wantsToFire = true;
   }
   else {
      pev->button |= m_campButtons;
   }

   // stop camping if time over or gets hurt by something else than bullets
   if (task ()->time < engine.timebase () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::blind_ (void) {
   m_moveToGoal = false;
   m_checkTerrain = false;
   m_navTimeset = engine.timebase ();

   // if bot remembers last enemy position
   if (m_difficulty >= 2 && !m_lastEnemyOrigin.empty () && isPlayer (m_lastEnemy) && !usesSniper ()) {
      m_lookAt = m_lastEnemyOrigin; // face last enemy
      m_wantsToFire = true; // and shoot it
   }

   m_moveSpeed = m_blindMoveSpeed;
   m_strafeSpeed = m_blindSidemoveSpeed;
   pev->button |= m_blindButton;

   if (m_blindTime < engine.timebase ()) {
      completeTask ();
   }
}

void Bot::camp_ (void) {
   if (!yb_camping_allowed.boolean ()) {
      completeTask ();
      return;
   }

   m_aimFlags |= AIM_CAMP;
   m_checkTerrain = false;
   m_moveToGoal = false;

   if (m_team == TEAM_COUNTER && g_bombPlanted && m_defendedBomb && !isBombDefusing (waypoints.getBombPos ()) && !isOutOfBombTimer ()) {
      m_defendedBomb = false;
      completeTask ();
   }
   ignoreCollision ();

   // half the reaction time if camping because you're more aware of enemies if camping
   setIdealReactionTimers ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = engine.timebase ();
   m_timeCamping = engine.timebase ();

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   getValidPoint ();

   if (m_nextCampDirTime < engine.timebase ()) {
      m_nextCampDirTime = engine.timebase () + rng.getFloat (2.0f, 5.0f);

      if (m_currentPath->flags & FLAG_CAMP) {
         Vector dest;

         // switch from 1 direction to the other
         if (m_campDirection < 1) {
            dest.x = m_currentPath->campStartX;
            dest.y = m_currentPath->campStartY;

            m_campDirection ^= 1;
         }
         else {
            dest.x = m_currentPath->campEndX;
            dest.y = m_currentPath->campEndY;
            m_campDirection ^= 1;
         }
         dest.z = 0.0f;

         // find a visible waypoint to this direction...
         // i know this is ugly hack, but i just don't want to break compatibility :)
         int numFoundPoints = 0;

         int campPoints[3] = { 0, };
         int distances[3] = { 0, };

         const Vector &dotA = (dest - pev->origin).normalize2D ();

         for (int i = 0; i < g_numWaypoints; i++) {
            // skip invisible waypoints or current waypoint
            if (!waypoints.isVisible (m_currentWaypointIndex, i) || (i == m_currentWaypointIndex)) {
               continue;
            }
            const Vector &dotB = (waypoints[i].origin - pev->origin).normalize2D ();

            if ((dotA | dotB) > 0.9f) {
               int distance = static_cast<int> ((pev->origin - waypoints[i].origin).length ());

               if (numFoundPoints >= 3) {
                  for (int j = 0; j < 3; j++) {
                     if (distance > distances[j]) {
                        distances[j] = distance;
                        campPoints[j] = i;

                        break;
                     }
                  }
               }
               else {
                  campPoints[numFoundPoints] = i;
                  distances[numFoundPoints] = distance;

                  numFoundPoints++;
               }
            }
         }

         if (--numFoundPoints >= 0) {
            m_camp = waypoints[campPoints[rng.getInt (0, numFoundPoints)]].origin;
         }
         else {
            m_camp = waypoints[searchCampDirectionPoint ()].origin;
         }
      }
      else
         m_camp = waypoints[searchCampDirectionPoint ()].origin;
   }
   // press remembered crouch button
   pev->button |= m_campButtons;

   // stop camping if time over or gets hurt by something else than bullets
   if (task ()->time < engine.timebase () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::hide_ (void) {
   m_aimFlags |= AIM_CAMP;
   m_checkTerrain = false;
   m_moveToGoal = false;

   // half the reaction time if camping
   setIdealReactionTimers ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = engine.timebase ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   getValidPoint ();

   if (hasShield () && !m_isReloading) {
      if (!isShieldDrawn ()) {
         pev->button |= IN_ATTACK2; // draw the shield!
      }
      else {
         pev->button |= IN_DUCK; // duck under if the shield is already drawn
      }
   }

   // if we see an enemy and aren't at a good camping point leave the spot
   if ((m_states & STATE_SEEING_ENEMY) || m_inBombZone) {
      if (!(m_currentPath->flags & FLAG_CAMP)) {
         completeTask ();

         m_campButtons = 0;
         m_prevGoalIndex = INVALID_WAYPOINT_INDEX;

         if (!engine.isNullEntity (m_enemy)) {
            attackMovement ();
         }
         return;
      }
   }

   // if we don't have an enemy we're also free to leave
   else if (m_lastEnemyOrigin.empty ()) {
      completeTask ();

      m_campButtons = 0;
      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;

      if (taskId () == TASK_HIDE) {
         completeTask ();
      }
      return;
   }

   pev->button |= m_campButtons;
   m_navTimeset = engine.timebase ();

   // stop camping if time over or gets hurt by something else than bullets
   if (task ()->time < engine.timebase () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::moveToPos_ (void) {
   m_aimFlags |= AIM_NAVPOINT;

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   // reached destination?
   if (processNavigation ()) {
      completeTask (); // we're done

      m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
      m_position.nullify ();
   }

   // didn't choose goal waypoint yet?
   else if (!isGoalValid ()) {
      clearSearchNodes ();

      int destIndex = INVALID_WAYPOINT_INDEX;

      if (task ()->data != INVALID_WAYPOINT_INDEX && task ()->data < g_numWaypoints) {
         destIndex = task ()->data;
      }
      else {
         destIndex = waypoints.getNearest (m_position);
      }
      if (destIndex >= 0 && destIndex < g_numWaypoints) {
         m_prevGoalIndex = destIndex;
         task ()->data = destIndex;

         searchPath (m_currentWaypointIndex, destIndex, m_pathType);
      }
      else {
         completeTask ();
      }
   }
}

void Bot::plantBomb_ (void) {
   m_aimFlags |= AIM_CAMP;

   // we're still got the C4?
   if (m_hasC4) {
      selectWeaponByName ("weapon_c4");

      if (isAlive (m_enemy) || !m_inBombZone) {
         completeTask ();
      }
      else {
         m_moveToGoal = false;
         m_checkTerrain = false;
         m_navTimeset = engine.timebase ();

         if (m_currentPath->flags & FLAG_CROUCH) {
            pev->button |= (IN_ATTACK | IN_DUCK);
         }
         else {
            pev->button |= IN_ATTACK;
         }
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
      }
   }

   // done with planting
   else {
      completeTask ();

      // tell teammates to move over here...
      if (numFriendsNear (pev->origin, 1200.0f) != 0) {
         pushRadioMessage (Radio_NeedBackup);
      }
      clearSearchNodes ();
      int index = getDefendPoint (pev->origin);

      float guardTime = mp_c4timer.flt () * 0.5f + mp_c4timer.flt () * 0.25f;

      // push camp task on to stack
      startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + guardTime, true);

      // push move command
      startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, index, engine.timebase () + guardTime, true);

      if (waypoints[index].vis.crouch <= waypoints[index].vis.stand) {
         m_campButtons |= IN_DUCK;
      }
      else {
         m_campButtons &= ~IN_DUCK;
      }
   }
}

void Bot::bombDefuse_ (void) {
   float fullDefuseTime = m_hasDefuser ? 7.0f : 12.0f;
   float timeToBlowUp = getBombTimeleft ();
   float defuseRemainingTime = fullDefuseTime;

   if (m_hasProgressBar /*&& isOnFloor ()*/) {
      defuseRemainingTime = fullDefuseTime - engine.timebase ();
   }

   bool pickupExists = !engine.isNullEntity (m_pickupItem);
   const Vector &bombPos = pickupExists ? m_pickupItem->v.origin : waypoints.getBombPos ();

   if (pickupExists) {
      if (waypoints.getBombPos () != bombPos) {
         waypoints.setBombPos (bombPos);
      }
   }
   bool defuseError = false;

   // exception: bomb has been defused
   if (bombPos.empty ()) {
      defuseError = true;
      g_bombPlanted = false;

      if (rng.getInt (0, 100) < 50 && m_numFriendsLeft != 0) {
         if (timeToBlowUp <= 3.0) {
            if (yb_communication_type.integer () == 2) {
               instantChatter (Chatter_BarelyDefused);
            }
            else if (yb_communication_type.integer () == 1) {
               pushRadioMessage (Radio_SectorClear);
            }
         }
         else {
            pushRadioMessage (Radio_SectorClear);
         }
      }
   }
   else if (defuseRemainingTime > timeToBlowUp) {
      defuseError = true;
   }
   else if (m_states & STATE_SEEING_ENEMY) {
      int friends = numFriendsNear (pev->origin, 768.0f);

      if (friends < 2 && defuseRemainingTime < timeToBlowUp) {
         defuseError = true;

         if (defuseRemainingTime + 2.0f > timeToBlowUp) {
            defuseError = false;
         }

         if (m_numFriendsLeft > friends) {
            pushRadioMessage (Radio_NeedBackup);
         }
      }
   }

   // one of exceptions is thrown. finish task.
   if (defuseError) {
      m_checkTerrain = true;
      m_moveToGoal = true;

      m_destOrigin.nullify ();
      m_entity.nullify ();

      m_pickupItem = nullptr;
      m_pickupType = PICKUP_NONE;

      completeTask ();
      return;
   }

   // to revert from pause after reload  ting && just to be sure
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_moveSpeed = pev->maxspeed;
   m_strafeSpeed = 0.0f;

   // bot is reloading and we close enough to start defusing
   if (m_isReloading && (bombPos - pev->origin).length2D () < 80.0f) {
      if (m_numEnemiesLeft == 0 || timeToBlowUp < fullDefuseTime + 7.0f || ((ammoClip () > 8 && m_reloadState == RELOAD_PRIMARY) || (ammoClip () > 5 && m_reloadState == RELOAD_SECONDARY))) {
         int weaponIndex = bestWeaponCarried ();

         // just select knife and then select weapon
         selectWeaponByName ("weapon_knife");

         if (weaponIndex > 0 && weaponIndex < NUM_WEAPONS) {
            selectWeaponById (weaponIndex);
         }
         m_isReloading = false;
      }
      else {
         m_moveToGoal = false;
         m_checkTerrain = false;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
      }
   }

   // head to bomb and press use button
   m_aimFlags |= AIM_ENTITY;

   m_destOrigin = bombPos;
   m_entity = bombPos;

   pev->button |= IN_USE;

   // if defusing is not already started, maybe crouch before
   if (!m_hasProgressBar && m_duckDefuseCheckTime < engine.timebase ()) {
      if (m_difficulty >= 2 && m_numEnemiesLeft != 0) {
         m_duckDefuse = true;
      }
      Vector botDuckOrigin, botStandOrigin;

      if (pev->button & IN_DUCK) {
         botDuckOrigin = pev->origin;
         botStandOrigin = pev->origin + Vector (0.0f, 0.0f, 18.0f);
      }
      else {
         botDuckOrigin = pev->origin - Vector (0.0f, 0.0f, 18.0f);
         botStandOrigin = pev->origin;
      }

      float duckLength = (m_entity - botDuckOrigin).lengthSq ();
      float standLength = (m_entity - botStandOrigin).lengthSq ();

      if (duckLength > 5625.0f || standLength > 5625.0f) {
         if (standLength < duckLength) {
            m_duckDefuse = false; // stand
         }
         else {
            m_duckDefuse = true; // duck
         }
      }
      m_duckDefuseCheckTime = engine.timebase () + 1.5f;
   }

   // press duck button
   if (m_duckDefuse || (m_oldButtons & IN_DUCK)) {
      pev->button |= IN_DUCK;
   }
   else {
      pev->button &= ~IN_DUCK;
   }

   // we are defusing bomb
   if (m_hasProgressBar || pickupExists || (m_oldButtons & IN_USE)) {
      pev->button |= IN_USE;

      m_reloadState = RELOAD_NONE;
      m_navTimeset = engine.timebase ();

      // don't move when defusing
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      // notify team
      if (m_numFriendsLeft != 0) {
         pushChatterMessage (Chatter_DefusingC4);

         if (numFriendsNear (pev->origin, 512.0f) < 2) {
            pushRadioMessage (Radio_NeedBackup);
         }
      }
   }
   else
      completeTask ();
}

void Bot::followUser_ (void) {
   if (engine.isNullEntity (m_targetEntity) || !isAlive (m_targetEntity)) {
      m_targetEntity = nullptr;
      completeTask ();

      return;
   }

   if (m_targetEntity->v.button & IN_ATTACK) {
      makeVectors (m_targetEntity->v.v_angle);

      TraceResult tr;
      engine.testLine (m_targetEntity->v.origin + m_targetEntity->v.view_ofs, g_pGlobals->v_forward * 500.0f, TRACE_IGNORE_EVERYTHING, ent (), &tr);

      if (!engine.isNullEntity (tr.pHit) && isPlayer (tr.pHit) && engine.getTeam (tr.pHit) != m_team) {
         m_targetEntity = nullptr;
         m_lastEnemy = tr.pHit;
         m_lastEnemyOrigin = tr.pHit->v.origin;

         completeTask ();
         return;
      }
   }

   if (m_targetEntity->v.maxspeed != 0 && m_targetEntity->v.maxspeed < pev->maxspeed) {
      m_moveSpeed = m_targetEntity->v.maxspeed;
   }

   if (m_reloadState == RELOAD_NONE && ammo () != 0) {
      m_reloadState = RELOAD_PRIMARY;
   }

   if ((m_targetEntity->v.origin - pev->origin).length () > 130) {
      m_followWaitTime = 0.0f;
   }
   else {
      m_moveSpeed = 0.0f;

      if (m_followWaitTime == 0.0f) {
         m_followWaitTime = engine.timebase ();
      }
      else {
         if (m_followWaitTime + 3.0f < engine.timebase ()) {
            // stop following if we have been waiting too long
            m_targetEntity = nullptr;

            pushRadioMessage (Radio_YouTakePoint);
            completeTask ();

            return;
         }
      }
   }
   m_aimFlags |= AIM_NAVPOINT;

   if (yb_walking_allowed.boolean () && m_targetEntity->v.maxspeed < m_moveSpeed && !yb_jasonmode.boolean ()) {
      m_moveSpeed = getShiftSpeed ();
   }

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   // reached destination?
   if (processNavigation ()) {
      task ()->data = INVALID_WAYPOINT_INDEX;
   }

   // didn't choose goal waypoint yet?
   if (!isGoalValid ()) {
      clearSearchNodes ();

      int destIndex = waypoints.getNearest (m_targetEntity->v.origin);

      IntArray points;
      waypoints.searchRadius (points, 200.0f, m_targetEntity->v.origin);

      while (!points.empty ()) {
         int newIndex = points.pop ();

         // if waypoint not yet used, assign it as dest
         if (!isOccupiedPoint (newIndex) && newIndex != m_currentWaypointIndex) {
            destIndex = newIndex;
         }
      }

      if (destIndex >= 0 && destIndex < g_numWaypoints && destIndex != m_currentWaypointIndex && m_currentWaypointIndex >= 0 && m_currentWaypointIndex < g_numWaypoints) {
         m_prevGoalIndex = destIndex;
         task ()->data = destIndex;

         // always take the shortest path
         searchShortestPath (m_currentWaypointIndex, destIndex);
      }
      else {
         m_targetEntity = nullptr;
         completeTask ();
      }
   }
}

void Bot::throwExplosive_ (void) {
   m_aimFlags |= AIM_GRENADE;
   Vector dest = m_throw;

   if (!(m_states & STATE_SEEING_ENEMY)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }
   else if (!(m_states & STATE_SUSPECT_ENEMY) && !engine.isNullEntity (m_enemy)) {
      dest = m_enemy->v.origin + m_enemy->v.velocity.make2D () * 0.55f;
   }
   m_isUsingGrenade = true;
   m_checkTerrain = false;

   ignoreCollision ();

   if ((pev->origin - dest).lengthSq () < A_square (400.0f)) {
      // heck, I don't wanna blow up myself
      m_grenadeCheckTime = engine.timebase () + MAX_GRENADE_TIMER;

      selectBestWeapon ();
      completeTask ();

      return;
   }
   m_grenade = calcThrow (eyePos (), dest);

   if (m_grenade.lengthSq () < 100.0f) {
      m_grenade = calcToss (pev->origin, dest);
   }

   if (m_grenade.lengthSq () <= 100.0f) {
      m_grenadeCheckTime = engine.timebase () + MAX_GRENADE_TIMER;

      selectBestWeapon ();
      completeTask ();
   }
   else {
      auto grenade = correctGrenadeVelocity ("hegrenade.mdl");

      if (engine.isNullEntity (grenade)) {
         if (m_currentWeapon != WEAPON_EXPLOSIVE) {
            if (pev->weapons & (1 << WEAPON_EXPLOSIVE)) {
               selectWeaponByName ("weapon_hegrenade");
            }
            else {
               selectBestWeapon ();
               completeTask ();

               return;
            }
         }
         else if (!(m_oldButtons & IN_ATTACK)) {
            pev->button |= IN_ATTACK;
         }
      }
   }
   pev->button |= m_campButtons;
}

void Bot::throwFlashbang_ (void) {
   m_aimFlags |= AIM_GRENADE;
   Vector dest = m_throw;

   if (!(m_states & STATE_SEEING_ENEMY)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }
   else if (!(m_states & STATE_SUSPECT_ENEMY) && !engine.isNullEntity (m_enemy)) {
      dest = m_enemy->v.origin + m_enemy->v.velocity.make2D () * 0.55f;
   }

   m_isUsingGrenade = true;
   m_checkTerrain = false;

   ignoreCollision ();

   if ((pev->origin - dest).lengthSq () < A_square (400.0f)) {
      // heck, I don't wanna blow up myself
      m_grenadeCheckTime = engine.timebase () + MAX_GRENADE_TIMER;

      selectBestWeapon ();
      completeTask ();

      return;
   }
   m_grenade = calcThrow (eyePos (), dest);

   if (m_grenade.lengthSq () < 100.0f) {
      m_grenade = calcToss (pev->origin, dest);
   }

   if (m_grenade.lengthSq () <= 100.0f) {
      m_grenadeCheckTime = engine.timebase () + MAX_GRENADE_TIMER;

      selectBestWeapon ();
      completeTask ();
   }
   else {
      auto grenade = correctGrenadeVelocity ("flashbang.mdl");

      if (engine.isNullEntity (grenade)) {
         if (m_currentWeapon != WEAPON_FLASHBANG) {
            if (pev->weapons & (1 << WEAPON_FLASHBANG)) {
               selectWeaponByName ("weapon_flashbang");
            }
            else {
               selectBestWeapon ();
               completeTask ();

               return;
            }
         }
         else if (!(m_oldButtons & IN_ATTACK)) {
            pev->button |= IN_ATTACK;
         }
      }
   }
   pev->button |= m_campButtons;
}

void Bot::throwSmoke_ (void) {
   m_aimFlags |= AIM_GRENADE;

   if (!(m_states & STATE_SEEING_ENEMY)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }

   m_checkTerrain = false;
   m_isUsingGrenade = true;

   ignoreCollision ();

   Vector src = m_lastEnemyOrigin - pev->velocity;

   // predict where the enemy is in 0.5 secs
   if (!engine.isNullEntity (m_enemy))
      src = src + m_enemy->v.velocity * 0.5f;

   m_grenade = (src - eyePos ()).normalize ();

   if (task ()->time < engine.timebase ()) {
      completeTask ();
      return;
   }

   if (m_currentWeapon != WEAPON_SMOKE) {
      if (pev->weapons & (1 << WEAPON_SMOKE)) {
         selectWeaponByName ("weapon_smokegrenade");
         task ()->time = engine.timebase () + 1.2f;
      }
      else {
         task ()->time = engine.timebase () + 0.1f;
      }
   }
   else if (!(m_oldButtons & IN_ATTACK)) {
      pev->button |= IN_ATTACK;
   }
}

void Bot::doublejump_ (void) {
   if (!isAlive (m_doubleJumpEntity) || (m_aimFlags & AIM_ENEMY) || (m_travelStartIndex != INVALID_WAYPOINT_INDEX && task ()->time + (waypoints.calculateTravelTime (pev->maxspeed, waypoints[m_travelStartIndex].origin, m_doubleJumpOrigin) + 11.0f) < engine.timebase ())) {
      resetDoubleJump ();
      return;
   }
   m_aimFlags |= AIM_NAVPOINT;

   if (m_jumpReady) {
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = engine.timebase ();
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      bool inJump = (m_doubleJumpEntity->v.button & IN_JUMP) || (m_doubleJumpEntity->v.oldbuttons & IN_JUMP);

      if (m_duckForJump < engine.timebase ()) {
         pev->button |= IN_DUCK;
      }
      else if (inJump && !(m_oldButtons & IN_JUMP)) {
         pev->button |= IN_JUMP;
      }
      makeVectors (Vector (0.0f, pev->angles.y, 0.0f));

      Vector src = pev->origin + Vector (0.0f, 0.0f, 45.0f);
      Vector dest = src + g_pGlobals->v_up * 256.0f;

      TraceResult tr;
      engine.testLine (src, dest, TRACE_IGNORE_NONE, ent (), &tr);

      if (tr.flFraction < 1.0f && tr.pHit == m_doubleJumpEntity && inJump) {
         m_duckForJump = engine.timebase () + rng.getFloat (3.0f, 5.0f);
         task ()->time = engine.timebase ();
      }
      return;
   }

   if (m_currentWaypointIndex == m_prevGoalIndex) {
      m_waypointOrigin = m_doubleJumpOrigin;
      m_destOrigin = m_doubleJumpOrigin;
   }

   if (processNavigation ()) {
      task ()->data = INVALID_WAYPOINT_INDEX;
   }

   // didn't choose goal waypoint yet?
   if (!isGoalValid ())  {
      clearSearchNodes ();

      int destIndex = waypoints.getNearest (m_doubleJumpOrigin);

      if (destIndex >= 0 && destIndex < g_numWaypoints) {
         m_prevGoalIndex = destIndex;
         task ()->data = destIndex;
         m_travelStartIndex = m_currentWaypointIndex;

         // always take the shortest path
         searchShortestPath (m_currentWaypointIndex, destIndex);

         if (m_currentWaypointIndex == destIndex) {
            m_jumpReady = true;
         }
      }
      else {
         resetDoubleJump ();
      }
   }
}

void Bot::escapeFromBomb_ (void) {
   m_aimFlags |= AIM_NAVPOINT;

   if (!g_bombPlanted) {
      completeTask ();
   }

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   if (m_currentWeapon != WEAPON_KNIFE && m_numEnemiesLeft == 0) {
      selectWeaponByName ("weapon_knife");
   }

   // reached destination?
   if (processNavigation ()) {
      completeTask (); // we're done

      // press duck button if we still have some enemies
      if (numEnemiesNear (pev->origin, 2048.0f)) {
         m_campButtons = IN_DUCK;
      }

      // we're reached destination point so just sit down and camp
      startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + 10.0f, true);
   }

   // didn't choose goal waypoint yet?
   else if (!isGoalValid ()) {
      clearSearchNodes ();

      int lastSelectedGoal = INVALID_WAYPOINT_INDEX, minPathDistance = 99999;
      float safeRadius = rng.getFloat (1248.0f, 2048.0f);

      for (int i = 0; i < g_numWaypoints; i++) {
         if ((waypoints[i].origin - waypoints.getBombPos ()).length () < safeRadius || isOccupiedPoint (i)) {
            continue;
         }
         int pathDistance = waypoints.getPathDist (m_currentWaypointIndex, i);

         if (minPathDistance > pathDistance) {
            minPathDistance = pathDistance;
            lastSelectedGoal = i;
         }
      }

      if (lastSelectedGoal < 0) {
         lastSelectedGoal = waypoints.getFarest (pev->origin, safeRadius);
      }

      // still no luck?
      if (lastSelectedGoal < 0) {
         completeTask (); // we're done

         // we have no destination point, so just sit down and camp
         startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, engine.timebase () + 10.0f, true);
         return;
      }
      m_prevGoalIndex = lastSelectedGoal;
      task ()->data = lastSelectedGoal;

      searchShortestPath (m_currentWaypointIndex, lastSelectedGoal);
   }
}

void Bot::shootBreakable_ (void) {
   m_aimFlags |= AIM_OVERRIDE;

   // Breakable destroyed?
   if (engine.isNullEntity (lookupBreakable ())) {
      completeTask ();
      return;
   }
   pev->button |= m_campButtons;

   m_checkTerrain = false;
   m_moveToGoal = false;
   m_navTimeset = engine.timebase ();

   Vector src = m_breakableOrigin;
   m_camp = src;

   // is bot facing the breakable?
   if (getShootingConeDeviation (ent (), src) >= 0.90f) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      if (m_currentWeapon == WEAPON_KNIFE) {
         selectBestWeapon ();
      }
      m_wantsToFire = true;
   }
   else {
      m_checkTerrain = true;
      m_moveToGoal = true;
   }
}

void Bot::pickupItem_ () {
   if (engine.isNullEntity (m_pickupItem)) {
      m_pickupItem = nullptr;
      completeTask ();

      return;
   }
   Vector dest = engine.getAbsPos (m_pickupItem);

   m_destOrigin = dest;
   m_entity = dest;

   // find the distance to the item
   float itemDistance = (dest - pev->origin).length ();

   switch (m_pickupType) {
   case PICKUP_DROPPED_C4:
   case PICKUP_NONE:
      break;

   case PICKUP_WEAPON:
      m_aimFlags |= AIM_NAVPOINT;

      // near to weapon?
      if (itemDistance < 50.0f) {
         int id = 0;

         for (id = 0; id < 7; id++) {
            if (strcmp (g_weaponSelect[id].modelName, STRING (m_pickupItem->v.model) + 9) == 0) {
               break;
            }
         }

         if (id < 7) {
            // secondary weapon. i.e., pistol
            int wid = 0;

            for (id = 0; id < 7; id++) {
               if (pev->weapons & (1 << g_weaponSelect[id].id)) {
                  wid = id;
               }
            }

            if (wid > 0) {
               selectWeaponById (wid);
               engine.execBotCmd (ent (), "drop");

               if (hasShield ()) {
                  engine.execBotCmd (ent (), "drop"); // discard both shield and pistol
               }
            }
            processBuyzoneEntering (BUYSTATE_PRIMARY_WEAPON);
         }
         else {
            // primary weapon
            int wid = bestWeaponCarried ();

            if (wid > 6 || hasShield ()) {
               selectWeaponById (wid);
               engine.execBotCmd (ent (), "drop");
            }
            processBuyzoneEntering (BUYSTATE_PRIMARY_WEAPON);
         }
         checkSilencer (); // check the silencer
      }
      break;

   case PICKUP_SHIELD:
      m_aimFlags |= AIM_NAVPOINT;

      if (hasShield ()) {
         m_pickupItem = nullptr;
         break;
      }

      // near to shield?
      else if (itemDistance < 50.0f) {
         // get current best weapon to check if it's a primary in need to be dropped
         int wid = bestWeaponCarried ();

         if (wid > 6) {
            selectWeaponById (wid);
            engine.execBotCmd (ent (), "drop");
         }
      }
      break;

   case PICKUP_PLANTED_C4:
      m_aimFlags |= AIM_ENTITY;

      if (m_team == TEAM_COUNTER && itemDistance < 80.0f) {
         pushChatterMessage (Chatter_DefusingC4);

         // notify team of defusing
         if (m_numFriendsLeft < 3) {
            pushRadioMessage (Radio_NeedBackup);
         }
         m_moveToGoal = false;
         m_checkTerrain = false;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         startTask (TASK_DEFUSEBOMB, TASKPRI_DEFUSEBOMB, INVALID_WAYPOINT_INDEX, 0.0f, false);
      }
      break;

   case PICKUP_HOSTAGE:
      m_aimFlags |= AIM_ENTITY;

      if (!isAlive (m_pickupItem)) {
         // don't pickup dead hostages
         m_pickupItem = nullptr;
         completeTask ();

         break;
      }

      if (itemDistance < 50.0f) {
         float angleToEntity = isInFOV (dest - eyePos ());

         // bot faces hostage?
         if (angleToEntity <= 10.0f) {
            // use game dll function to make sure the hostage is correctly 'used'
            MDLL_Use (m_pickupItem, ent ());

            if (rng.getInt (0, 100) < 80) {
               pushChatterMessage (Chatter_UseHostage);
            }

            for (int i = 0; i < MAX_HOSTAGES; i++) {

               // store pointer to hostage so other bots don't steal from this one or bot tries to reuse it
               if (engine.isNullEntity (m_hostages[i])) {
                  m_hostages[i] = m_pickupItem;
                  m_pickupItem = nullptr;

                  break;
               }
            }
         }
         ignoreCollision (); // also don't consider being stuck
      }
      break;

   case PICKUP_DEFUSEKIT:
      m_aimFlags |= AIM_NAVPOINT;

      if (m_hasDefuser) {
         m_pickupItem = nullptr;
         m_pickupType = PICKUP_NONE;
      }
      break;

   case PICKUP_BUTTON:
      m_aimFlags |= AIM_ENTITY;

      if (engine.isNullEntity (m_pickupItem) || m_buttonPushTime < engine.timebase ()) {
         completeTask ();
         m_pickupType = PICKUP_NONE;

         break;
      }

      // find angles from bot origin to entity...
      float angleToEntity = isInFOV (dest - eyePos ());

      // near to the button?
      if (itemDistance < 90.0f) {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
         m_moveToGoal = false;
         m_checkTerrain = false;

         // facing it directly?
         if (angleToEntity <= 10.0f) {
            MDLL_Use (m_pickupItem, ent ());

            m_pickupItem = nullptr;
            m_pickupType = PICKUP_NONE;
            m_buttonPushTime = engine.timebase () + 3.0f;

            completeTask ();
         }
      }
      break;
   }
}

void Bot::processTasks (void) {
   // this is core function that handle task execution

   switch (taskId ()) {
   // normal task
   default:
   case TASK_NORMAL:
      normal_ ();
      break;

   // bot sprays messy logos all over the place...
   case TASK_SPRAY:
      spraypaint_ ();
      break;

   // hunt down enemy
   case TASK_HUNTENEMY:
      huntEnemy_ ();
      break;

   // bot seeks cover from enemy
   case TASK_SEEKCOVER:
      seekCover_ ();
      break;

   // plain attacking
   case TASK_ATTACK:
      attackEnemy_ ();
      break;

   // Bot is pausing
   case TASK_PAUSE:
      pause_ ();
      break;

   // blinded (flashbanged) behaviour
   case TASK_BLINDED:
      blind_ ();
      break;

   // camping behaviour
   case TASK_CAMP:
      camp_ ();
      break;

   // hiding behaviour
   case TASK_HIDE:
      hide_ ();
      break;

   // moves to a position specified in position has a higher priority than task_normal
   case TASK_MOVETOPOSITION:
      moveToPos_ ();
      break;

   // planting the bomb right now
   case TASK_PLANTBOMB:
      plantBomb_ ();
      break;

   // bomb defusing behaviour
   case TASK_DEFUSEBOMB:
      bombDefuse_ ();
      break;

   // follow user behaviour
   case TASK_FOLLOWUSER:
      followUser_ ();
      break;

   // HE grenade throw behaviour
   case TASK_THROWHEGRENADE:
      throwExplosive_ ();
      break;

   // flashbang throw behavior (basically the same code like for HE's)
   case TASK_THROWFLASHBANG:
      throwFlashbang_ ();
      break;

   // smoke grenade throw behavior
   // a bit different to the others because it mostly tries to throw the sg on the ground
   case TASK_THROWSMOKE:
      throwSmoke_ ();
      break;

   // bot helps human player (or other bot) to get somewhere
   case TASK_DOUBLEJUMP:
      doublejump_ ();
      break;

   // escape from bomb behaviour
   case TASK_ESCAPEFROMBOMB:
      escapeFromBomb_ ();
      break;

   // shooting breakables in the way action
   case TASK_SHOOTBREAKABLE:
      shootBreakable_ ();
      break;

   // picking up items and stuff behaviour
   case TASK_PICKUPITEM:
      pickupItem_ ();
      break;
   }
}

void Bot::checkSpawnConditions (void) {
   // this function is called instead of ai when buying finished, but freezetime is not yet left.

   // switch to knife if time to do this
   if (m_checkKnifeSwitch && !m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + rng.getFloat (4.0f, 6.5f) < engine.timebase ()) {
      if (rng.getInt (1, 100) < 2 && yb_spraypaints.boolean ()) {
         startTask (TASK_SPRAY, TASKPRI_SPRAYLOGO, INVALID_WAYPOINT_INDEX, engine.timebase () + 1.0f, false);
      }

      if (m_difficulty >= 2 && rng.getInt (0, 100) < (m_personality == PERSONALITY_RUSHER ? 99 : 50) && !m_isReloading && (g_mapType & (MAP_CS | MAP_DE | MAP_ES | MAP_AS))) {
         if (yb_jasonmode.boolean ()) {
            selectSecondary ();
            engine.execBotCmd (ent (), "drop");
         }
         else {
            selectWeaponByName ("weapon_knife");
         }
      }
      m_checkKnifeSwitch = false;

      if (rng.getInt (0, 100) < yb_user_follow_percent.integer () && engine.isNullEntity (m_targetEntity) && !m_isLeader && !m_hasC4 && rng.getInt (0, 100) > 50) {
         decideFollowUser ();
      }
   }

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + rng.getFloat (2.0f, 3.5f) < engine.timebase ()) {
      if (hasShield () && isShieldDrawn ()) {
         pev->button |= IN_ATTACK2;
      }
      else {
         switch (m_currentWeapon) {
         case WEAPON_M4A1:
         case WEAPON_USP:
            checkSilencer ();
            break;

         case WEAPON_FAMAS:
         case WEAPON_GLOCK:
            if (rng.getInt (0, 100) < 50) {
               pev->button |= IN_ATTACK2;
            }
            break;
         }
      }
      m_checkWeaponSwitch = false;
   }
}

void Bot::ai (void) {
   // this function gets called each frame and is the core of all bot ai. from here all other subroutines are called

   float movedDistance = 2.0f; // length of different vector (distance bot moved)

   // increase reaction time
   m_actualReactionTime += 0.3f;

   if (m_actualReactionTime > m_idealReactionTime) {
      m_actualReactionTime = m_idealReactionTime;
   }

   // bot could be blinded by flashbang or smoke, recover from it
   m_viewDistance += 3.0f;

   if (m_viewDistance > m_maxViewDistance) {
      m_viewDistance = m_maxViewDistance;
   }

   if (m_blindTime > engine.timebase ()) {
      m_maxViewDistance = 4096.0f;
   }
   m_moveSpeed = pev->maxspeed;

   if (m_prevTime <= engine.timebase ()) {
      // see how far bot has moved since the previous position...
      movedDistance = (m_prevOrigin - pev->origin).length ();

      // save current position as previous
      m_prevOrigin = pev->origin;
      m_prevTime = engine.timebase () + 0.2f;
   }

   // if there's some radio message to respond, check it
   if (m_radioOrder != 0) {
      checkRadioQueue ();
   }

   // do all sensing, calculate/filter all actions here
   setConditions ();

   // some stuff required by by chatter engine
   if (yb_communication_type.integer () == 2) {
      if ((m_states & STATE_SEEING_ENEMY) && !engine.isNullEntity (m_enemy)) {
         int hasFriendNearby = numFriendsNear (pev->origin, 512.0f);

         if (!hasFriendNearby && rng.getInt (0, 100) < 45 && (m_enemy->v.weapons & (1 << WEAPON_C4))) {
            pushChatterMessage (Chatter_SpotTheBomber);
         }
         else if (!hasFriendNearby && rng.getInt (0, 100) < 45 && m_team == TEAM_TERRORIST && isPlayerVIP (m_enemy)) {
            pushChatterMessage (Chatter_VIPSpotted);
         }
         else if (!hasFriendNearby && rng.getInt (0, 100) < 50 && engine.getTeam (m_enemy) != m_team && isGroupOfEnemies (m_enemy->v.origin, 2, 384)) {
            pushChatterMessage (Chatter_ScaredEmotion);
         }
         else if (!hasFriendNearby && rng.getInt (0, 100) < 40 && ((m_enemy->v.weapons & (1 << WEAPON_AWP)) || (m_enemy->v.weapons & (1 << WEAPON_SCOUT)) || (m_enemy->v.weapons & (1 << WEAPON_G3SG1)) || (m_enemy->v.weapons & (1 << WEAPON_SG550)))) {
            pushChatterMessage (Chatter_SniperWarning);
         }

         // if bot is trapped under shield yell for help !
         if (taskId () == TASK_CAMP && hasShield () && isShieldDrawn () && hasFriendNearby >= 2 && seesEnemy (m_enemy)) {
            instantChatter (Chatter_Pinned_Down);
         }
      }

      // if bomb planted warn teammates !
      if (g_canSayBombPlanted && g_bombPlanted && m_team == TEAM_COUNTER) {
         g_canSayBombPlanted = false;
         pushChatterMessage (Chatter_GottaFindTheBomb);
      }
   }
   Vector src, destination;

   m_checkTerrain = true;
   m_moveToGoal = true;
   m_wantsToFire = false;

   avoidGrenades (); // avoid flyings grenades
   m_isUsingGrenade = false;

   processTasks (); // execute current task
   updateAimDir (); // choose aim direction
   processLookAngles (); // and turn to chosen aim direction

   // the bots wants to fire at something?
   if (m_wantsToFire && !m_isUsingGrenade && m_shootTime <= engine.timebase ()) {
      fireWeapons (); // if bot didn't fire a bullet try again next frame
   }

   // check for reloading
   if (m_reloadCheckTime <= engine.timebase ()) {
      checkReload ();
   }

   // set the reaction time (surprise momentum) different each frame according to skill
   setIdealReactionTimers ();

   // calculate 2 direction vectors, 1 without the up/down component
   const Vector &dirOld = m_destOrigin - (pev->origin + pev->velocity * calcThinkInterval ());
   const Vector &dirNormal = dirOld.normalize2D ();

   m_moveAngles = dirOld.toAngles ();
   m_moveAngles.clampAngles ();
   m_moveAngles.x *= -1.0f; // invert for engine

   overrideConditions ();

   // allowed to move to a destination position?
   if (m_moveToGoal) {
      getValidPoint ();

      // press duck button if we need to
      if ((m_currentPath->flags & FLAG_CROUCH) && !(m_currentPath->flags & (FLAG_CAMP | FLAG_GOAL))) {
         pev->button |= IN_DUCK;
      }
      m_timeWaypointMove = engine.timebase ();

      // special movement for swimming here
      if (isInWater ()) {
         // check if we need to go forward or back press the correct buttons
         if (isInFOV (m_destOrigin - eyePos ()) > 90.0f) {
            pev->button |= IN_BACK;
         }
         else {
            pev->button |= IN_FORWARD;
         }

         if (m_moveAngles.x > 60.0f) {
            pev->button |= IN_DUCK;
         }
         else if (m_moveAngles.x < -60.0f) {
            pev->button |= IN_JUMP;
         }
      }
   }

   // are we allowed to check blocking terrain (and react to it)?
   if (m_checkTerrain) {
      checkTerrain (movedDistance, dirNormal);
   }

   // must avoid a grenade?
   if (m_needAvoidGrenade != 0) {
      // don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * m_needAvoidGrenade;
   }

   // time to reach waypoint
   if (m_navTimeset + getReachTime () < engine.timebase () && engine.isNullEntity (m_enemy)) {
      m_currentWaypointIndex = INVALID_WAYPOINT_INDEX;
      getValidPoint ();

      // clear these pointers, bot mingh be stuck getting to them
      if (!engine.isNullEntity (m_pickupItem) && !m_hasProgressBar) {
         m_itemIgnore = m_pickupItem;
      }

      m_pickupItem = nullptr;
      m_breakableEntity = nullptr;
      m_itemCheckTime = engine.timebase () + 5.0f;
      m_pickupType = PICKUP_NONE;
   }

   if (m_duckTime >= engine.timebase ()) {
      pev->button |= IN_DUCK;
   }

   if (pev->button & IN_JUMP) {
      m_jumpTime = engine.timebase ();
   }

   if (m_jumpTime + 0.85f > engine.timebase ()) {
      if (!isOnFloor () && !isInWater ()) {
         pev->button |= IN_DUCK;
      }
   }

   if (!(pev->button & (IN_FORWARD | IN_BACK))) {
      if (m_moveSpeed > 0.0f) {
         pev->button |= IN_FORWARD;
      }
      else if (m_moveSpeed < 0.0f) {
         pev->button |= IN_BACK;
      }
   }

   if (!(pev->button & (IN_MOVELEFT | IN_MOVERIGHT))) {
      if (m_strafeSpeed > 0.0f) {
         pev->button |= IN_MOVERIGHT;
      }
      else if (m_strafeSpeed < 0.0f) {
         pev->button |= IN_MOVELEFT;
      }
   }

   // display some debugging thingy to host entity
   if (!engine.isNullEntity (g_hostEntity) && yb_debug.integer () >= 1) {
      showDebugOverlay ();
   }

   // save the previous speed (for checking if stuck)
   m_prevSpeed = A_abs (m_moveSpeed);
   m_lastDamageType = -1; // reset damage
}

void Bot::showDebugOverlay (void) {
   bool displayDebugOverlay = false;

   if (g_hostEntity->v.iuser2 == index ()) {
      displayDebugOverlay = true;
   }

   if (!displayDebugOverlay && yb_debug.integer () >= 2) {
      Bot *nearest = nullptr;

      if (findNearestPlayer (reinterpret_cast<void **> (&nearest), g_hostEntity, 128.0f, false, true, true, true) && nearest == this) {
         displayDebugOverlay = true;
      }
   }

   if (displayDebugOverlay) {
      static float timeDebugUpdate = 0.0f;
      static int index, goal, taskID;

      if (!m_tasks.empty ()) {
         if (taskID != taskId () || index != m_currentWaypointIndex || goal != task ()->data || timeDebugUpdate < engine.timebase ()) {
            taskID = taskId ();
            index = m_currentWaypointIndex;
            goal = task ()->data;

            char taskName[80];
            memset (taskName, 0, sizeof (taskName));

            switch (taskID) {
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

            if (!engine.isNullEntity (m_enemy)) {
               strncpy (enemyName, STRING (m_enemy->v.netname), A_bufsize (enemyName));
            }
            else if (!engine.isNullEntity (m_lastEnemy)) {
               snprintf (enemyName, A_bufsize (enemyName), "%s (L)", STRING (m_lastEnemy->v.netname));
            }
            else {
               strcpy (enemyName, " (null)");
            }

            char pickupName[80];
            memset (pickupName, 0, sizeof (pickupName));

            if (!engine.isNullEntity (m_pickupItem)) {
               strncpy (pickupName, STRING (m_pickupItem->v.classname), A_bufsize (pickupName));
            }
            else {
               strcpy (pickupName, " (null)");
            }

            WeaponSelect *tab = &g_weaponSelect[0];
            char weaponCount = 0;

            while (m_currentWeapon != tab->id && weaponCount < NUM_WEAPONS) {
               tab++;
               weaponCount++;
            }
            memset (aimFlags, 0, sizeof (aimFlags));

            // set the aim flags
            snprintf (aimFlags, A_bufsize (aimFlags), "%s%s%s%s%s%s%s%s", (m_aimFlags & AIM_NAVPOINT) ? " NavPoint" : "", (m_aimFlags & AIM_CAMP) ? " CampPoint" : "", (m_aimFlags & AIM_PREDICT_PATH) ? " PredictPath" : "", (m_aimFlags & AIM_LAST_ENEMY) ? " LastEnemy" : "", (m_aimFlags & AIM_ENTITY) ? " Entity" : "", (m_aimFlags & AIM_ENEMY) ? " Enemy" : "", (m_aimFlags & AIM_GRENADE) ? " Grenade" : "", (m_aimFlags & AIM_OVERRIDE) ? " Override" : "");

            // set the bot type
            snprintf (botType, A_bufsize (botType), "%s%s%s", m_personality == PERSONALITY_RUSHER ? " Rusher" : "", m_personality == PERSONALITY_CAREFUL ? " Careful" : "", m_personality == PERSONALITY_NORMAL ? " Normal" : "");

            if (weaponCount >= NUM_WEAPONS) {
               // prevent printing unknown message from known weapons
               switch (m_currentWeapon) {
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
            else {
               strncpy (weaponName, tab->weaponName, A_bufsize (weaponName));
            }
            char outputBuffer[512];
            memset (outputBuffer, 0, sizeof (outputBuffer));

            snprintf (outputBuffer, A_bufsize (outputBuffer), "\n\n\n\n\n%s (H:%.1f/A:%.1f)- Task: %d=%s Desire:%.02f\nItem: %s Clip: %d Ammo: %d%s Money: %d AimFlags: %s\nSP=%.02f SSP=%.02f I=%d PG=%d G=%d T: %.02f MT: %d\nEnemy=%s Pickup=%s Type=%s\n", STRING (pev->netname), pev->health, pev->armorvalue, taskID, taskName, task ()->desire, &weaponName[7], ammoClip (), ammo (), m_isReloading ? " (R)" : "", m_moneyAmount, aimFlags, m_moveSpeed, m_strafeSpeed, index, m_prevGoalIndex, goal, m_navTimeset - engine.timebase (), pev->movetype, enemyName, pickupName, botType);

            MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, Vector::null (), g_hostEntity)
               .writeByte (TE_TEXTMESSAGE)
               .writeByte (1)
               .writeShort (MessageWriter::fs16 (-1, 1 << 13))
               .writeShort (MessageWriter::fs16 (0, 1 << 13))
               .writeByte (0)
               .writeByte (m_team == TEAM_COUNTER ? 0 : 255)
               .writeByte (100)
               .writeByte (m_team != TEAM_COUNTER ? 0 : 255)
               .writeByte (0)
               .writeByte (255)
               .writeByte (255)
               .writeByte (255)
               .writeByte (0)
               .writeShort (MessageWriter::fu16 (0, 1 << 8))
               .writeShort (MessageWriter::fu16 (0, 1 << 8))
               .writeShort (MessageWriter::fu16 (1.0, 1 << 8))
               .writeString (const_cast<const char *> (&outputBuffer[0]));

            timeDebugUpdate = engine.timebase () + 1.0f;
         }

         // green = destination origin
         // blue = ideal angles
         // red = view angles

         engine.drawLine (g_hostEntity, eyePos (), m_destOrigin, 10, 0, 0, 255, 0, 250, 5, 1, DRAW_ARROW);

         makeVectors (m_idealAngles);
         engine.drawLine (g_hostEntity, eyePos () - Vector (0.0f, 0.0f, 16.0f), eyePos () + g_pGlobals->v_forward * 300.0f, 10, 0, 0, 0, 255, 250, 5, 1, DRAW_ARROW);

         makeVectors (pev->v_angle);
         engine.drawLine (g_hostEntity, eyePos () - Vector (0.0f, 0.0f, 32.0f), eyePos () + g_pGlobals->v_forward * 300.0f, 10, 0, 255, 0, 0, 250, 5, 1, DRAW_ARROW);

         // now draw line from source to destination
         PathNode *node = &m_navNode[0];

         while (node != nullptr) {
            const Vector &srcPath = waypoints[node->index].origin;
            node = node->next;

            if (node != nullptr) {
               engine.drawLine (g_hostEntity, srcPath, waypoints[node->index].origin, 15, 0, 255, 100, 55, 200, 5, 1, DRAW_ARROW);
            }
         }
      }
   }
}

bool Bot::hasHostage (void) {
   for (int i = 0; i < MAX_HOSTAGES; i++) {
      if (!engine.isNullEntity (m_hostages[i])) {
         // don't care about dead hostages
         if (m_hostages[i]->v.health <= 0.0f || (pev->origin - m_hostages[i]->v.origin).length () > 600.0f) {
            m_hostages[i] = nullptr;
            continue;
         }
         return true;
      }
   }
   return false;
}

int Bot::ammo (void) {
   if (g_weaponDefs[m_currentWeapon].ammo1 == -1 || g_weaponDefs[m_currentWeapon].ammo1 > 31) {
      return 0;
   }
   return m_ammo[g_weaponDefs[m_currentWeapon].ammo1];
}

void Bot::processDamage (edict_t *inflictor, int damage, int armor, int bits) {
   // this function gets called from the network message handler, when bot's gets hurt from any
   // other player.

   m_lastDamageType = bits;
   collectGoalExperience (damage, m_team);

   if (isPlayer (inflictor)) {
      if (yb_tkpunish.boolean () && engine.getTeam (inflictor) == m_team && !isFakeClient (inflictor)) {
         // alright, die you teamkiller!!!
         m_actualReactionTime = 0.0f;
         m_seeEnemyTime = engine.timebase ();
         m_enemy = inflictor;

         m_lastEnemy = m_enemy;
         m_lastEnemyOrigin = m_enemy->v.origin;
         m_enemyOrigin = m_enemy->v.origin;

         pushChatMessage (CHAT_TEAMATTACK);
         processChatterMessage ("#Bot_TeamAttack");
         pushChatterMessage (Chatter_FriendlyFire);
      }
      else {
         // attacked by an enemy
         if (pev->health > 60.0f) {
            m_agressionLevel += 0.1f;

            if (m_agressionLevel > 1.0f) {
               m_agressionLevel += 1.0f;
            }
         }
         else {
            m_fearLevel += 0.03f;

            if (m_fearLevel > 1.0f) {
               m_fearLevel += 1.0f;
            }
         }
         clearTask (TASK_CAMP);

         if (engine.isNullEntity (m_enemy) && m_team != engine.getTeam (inflictor)) {
            m_lastEnemy = inflictor;
            m_lastEnemyOrigin = inflictor->v.origin;

            // FIXME - Bot doesn't necessary sees this enemy
            m_seeEnemyTime = engine.timebase ();
         }

         if (!(g_gameFlags & GAME_CSDM)) {
            collectDataExperience (inflictor, armor + damage);
         }
      }
   }
   // hurt by unusual damage like drowning or gas
   else {
      // leave the camping/hiding position
      if (!waypoints.isReachable (this, waypoints.getNearest (m_destOrigin))) {
         clearSearchNodes ();
         findPoint ();
      }
   }
}

void Bot::processBlind (int alpha) {
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind from the grenade.

   m_maxViewDistance = rng.getFloat (10.0f, 20.0f);
   m_blindTime = engine.timebase () + static_cast<float> (alpha - 200) / 16.0f;

   if (m_blindTime < engine.timebase ()) {
      return;
   }
   m_enemy = nullptr;

   if (m_difficulty <= 2) {
      m_blindMoveSpeed = 0.0f;
      m_blindSidemoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;

      return;
   }

   m_blindMoveSpeed = -pev->maxspeed;
   m_blindSidemoveSpeed = 0.0f;

   if (rng.getInt (0, 100) > 50) {
      m_blindSidemoveSpeed = pev->maxspeed;
   }
   else {
      m_blindSidemoveSpeed = -pev->maxspeed;
   }

   if (pev->health < 85.0f) {
      m_blindMoveSpeed = -pev->maxspeed;
   }
   else if (m_personality == PERSONALITY_CAREFUL) {
      m_blindMoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;
   }
   else {
      m_blindMoveSpeed = pev->maxspeed;
   }
}

void Bot::collectGoalExperience (int damage, int team) {
   // gets called each time a bot gets damaged by some enemy. tries to achieve a statistic about most/less dangerous
   // waypoints for a destination goal used for pathfinding

   if (g_numWaypoints < 1 || waypoints.hasChanged () || m_chosenGoalIndex < 0 || m_prevGoalIndex < 0) {
      return;
   }

   // only rate goal waypoint if bot died because of the damage
   // FIXME: could be done a lot better, however this cares most about damage done by sniping or really deadly weapons
   if (pev->health - damage <= 0) {
      if (team == TEAM_TERRORIST) {
         int value = (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team0Value;
         value -= static_cast<int> (pev->health / 20);

         if (value < -MAX_GOAL_VALUE) {
            value = -MAX_GOAL_VALUE;
         }
         else if (value > MAX_GOAL_VALUE) {
            value = MAX_GOAL_VALUE;
         }
         (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team0Value = static_cast<int16> (value);
      }
      else {
         int value = (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team1Value;
         value -= static_cast<int> (pev->health / 20);

         if (value < -MAX_GOAL_VALUE) {
            value = -MAX_GOAL_VALUE;
         }
         else if (value > MAX_GOAL_VALUE) {
            value = MAX_GOAL_VALUE;
         }
         (g_experienceData + (m_chosenGoalIndex * g_numWaypoints) + m_prevGoalIndex)->team1Value = static_cast<int16> (value);
      }
   }
}

void Bot::collectDataExperience (edict_t *attacker, int damage) {
   // this function gets called each time a bot gets damaged by some enemy. sotores the damage (teamspecific) done by victim.

   if (!isPlayer (attacker)) {
      return;
   }

   int attackerTeam = engine.getTeam (attacker);
   int victimTeam = m_team;

   if (attackerTeam == victimTeam) {
      return;
   }

   // if these are bots also remember damage to rank destination of the bot
   m_goalValue -= static_cast<float> (damage);

   if (bots.getBot (attacker) != nullptr) {
      bots.getBot (attacker)->m_goalValue += static_cast<float> (damage);
   }

   if (damage < 20) {
      return; // do not collect damage less than 20
   }

   int attackerIndex = waypoints.getNearest (attacker->v.origin);
   int victimIndex = m_currentWaypointIndex;

   if (victimIndex == INVALID_WAYPOINT_INDEX) {
      victimIndex = getNearestPoint ();
   }

   if (pev->health > 20.0f) {
      if (victimTeam == TEAM_TERRORIST) {
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage++;
      }
      else {
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage++;
      }

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage > MAX_DAMAGE_VALUE) {
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team0Damage = MAX_DAMAGE_VALUE;
      }

      if ((g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage > MAX_DAMAGE_VALUE) {
         (g_experienceData + (victimIndex * g_numWaypoints) + victimIndex)->team1Damage = MAX_DAMAGE_VALUE;
      }
   }
   float updateDamage = isFakeClient (attacker) ? 10.0f : 7.0f;

   // store away the damage done
   if (victimTeam == TEAM_TERRORIST) {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage;
      value += static_cast<int> (damage / updateDamage);

      if (value > MAX_DAMAGE_VALUE) {
         value = MAX_DAMAGE_VALUE;
      }

      if (value > g_highestDamageT) {
         g_highestDamageT = value;
      }
      (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team0Damage = static_cast<uint16> (value);
   }
   else {
      int value = (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team1Damage;
      value += static_cast<int> (damage / updateDamage);

      if (value > MAX_DAMAGE_VALUE) {
         value = MAX_DAMAGE_VALUE;
      }

      if (value > g_highestDamageCT) {
         g_highestDamageCT = value;
      }
      (g_experienceData + (victimIndex * g_numWaypoints) + attackerIndex)->team1Damage = static_cast<uint16> (value);
   }
}

void Bot::processChatterMessage (const char *tempMessage) {
   // this function is added to prevent engine crashes with: 'Message XX started, before message XX ended', or something.

   if ((m_team == TEAM_COUNTER && strcmp (tempMessage, "#CTs_Win") == 0) || (m_team == TEAM_TERRORIST && strcmp (tempMessage, "#Terrorists_Win") == 0)) {
      if (g_timeRoundMid > engine.timebase ()) {
         pushChatterMessage (Chatter_QuicklyWonTheRound);
      }
      else {
         pushChatterMessage (Chatter_WonTheRound);
      }
   }

   else if (strcmp (tempMessage, "#Bot_TeamAttack") == 0) {
      pushChatterMessage (Chatter_FriendlyFire);
   }
   else if (strcmp (tempMessage, "#Bot_NiceShotCommander") == 0) {
      pushChatterMessage (Chatter_NiceshotCommander);
   }
   else if (strcmp (tempMessage, "#Bot_NiceShotPall") == 0) {
      pushChatterMessage (Chatter_NiceshotPall);
   }
}

void Bot::pushChatMessage (int type, bool isTeamSay) {
   extern ConVar yb_chat;

   if (g_chatFactory[type].empty () || !yb_chat.boolean ()) {
      return;
   }
   const char *pickedPhrase = g_chatFactory[type].random ().chars ();

   if (isEmptyStr (pickedPhrase)) {
      return;
   }

   prepareChatMessage (const_cast<char *> (pickedPhrase));
   pushMsgQueue (isTeamSay ? GAME_MSG_SAY_TEAM_MSG : GAME_MSG_SAY_CMD);
}

void Bot::dropWeaponForUser (edict_t *user, bool discardC4) {
   // this function, asks bot to discard his current primary weapon (or c4) to the user that requsted it with /drop*
   // command, very useful, when i'm don't have money to buy anything... )

   if (isAlive (user) && m_moneyAmount >= 2000 && hasPrimaryWeapon () && (user->v.origin - pev->origin).length () <= 450.0f) {
      m_aimFlags |= AIM_ENTITY;
      m_lookAt = user->v.origin;

      if (discardC4) {
         selectWeaponByName ("weapon_c4");
         engine.execBotCmd (ent (), "drop");
      }
      else {
         selectBestWeapon ();
         engine.execBotCmd (ent (), "drop");
      }

      m_pickupItem = nullptr;
      m_pickupType = PICKUP_NONE;
      m_itemCheckTime = engine.timebase () + 5.0f;

      if (m_inBuyZone) {
         m_ignoreBuyDelay = true;
         m_buyingFinished = false;
         m_buyState = BUYSTATE_PRIMARY_WEAPON;

         pushMsgQueue (GAME_MSG_PURCHASE);
         m_nextBuyTime = engine.timebase ();
      }
   }
}

void Bot::startDoubleJump (edict_t *ent) {
   resetDoubleJump ();

   m_doubleJumpOrigin = ent->v.origin;
   m_doubleJumpEntity = ent;

   startTask (TASK_DOUBLEJUMP, TASKPRI_DOUBLEJUMP, INVALID_WAYPOINT_INDEX, engine.timebase (), true);
   sayTeam (format ("Ok %s, i will help you!", STRING (ent->v.netname)));
}

void Bot::resetDoubleJump (void) {
   completeTask ();

   m_doubleJumpEntity = nullptr;
   m_duckForJump = 0.0f;
   m_doubleJumpOrigin.nullify ();
   m_travelStartIndex = INVALID_WAYPOINT_INDEX;
   m_jumpReady = false;
}

void Bot::sayDebug (const char *format, ...) {
   int level = yb_debug.integer ();

   if (level <= 2) {
      return;
   }
   va_list ap;
   char buffer[MAX_PRINT_BUFFER];

   va_start (ap, format);
   vsnprintf (buffer, A_bufsize (buffer), format, ap);
   va_end (ap);

   char printBuf[MAX_PRINT_BUFFER];
   snprintf (printBuf, A_bufsize (printBuf), "%s: %s", STRING (pev->netname), buffer);

   bool playMessage = false;

   if (level == 3 && !engine.isNullEntity (g_hostEntity) && g_hostEntity->v.iuser2 == index ()) {
      playMessage = true;
   }
   else if (level != 3) {
      playMessage = true;
   }
   if (playMessage && level > 3) {
      logEntry (false, LL_DEFAULT, printBuf);
   }
   if (playMessage) {
      engine.print (printBuf);
      say (printBuf);
   }
}

Vector Bot::calcToss (const Vector &start, const Vector &stop) {
   // this function returns the velocity at which an object should looped from start to land near end.
   // returns null vector if toss is not feasible.

   TraceResult tr;
   float gravity = sv_gravity.flt () * 0.55f;

   Vector end = stop - pev->velocity;
   end.z -= 15.0f;

   if (A_abs (end.z - start.z) > 500.0f) {
      return Vector::null ();
   }
   Vector midPoint = start + (end - start) * 0.5f;
   engine.testHull (midPoint, midPoint + Vector (0.0f, 0.0f, 500.0f), TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

   if (tr.flFraction < 1.0f) {
      midPoint = tr.vecEndPos;
      midPoint.z = tr.pHit->v.absmin.z - 1.0f;
   }

   if (midPoint.z < start.z || midPoint.z < end.z) {
      return Vector::null ();
   }
   float timeOne = A_sqrtf ((midPoint.z - start.z) / (0.5f * gravity));
   float timeTwo = A_sqrtf ((midPoint.z - end.z) / (0.5f * gravity));

   if (timeOne < 0.1f) {
      return Vector::null ();
   }
   Vector velocity = (end - start) / (timeOne + timeTwo);
   velocity.z = gravity * timeOne;

   Vector apex = start + velocity * timeOne;
   apex.z = midPoint.z;

   engine.testHull (start, apex, TRACE_IGNORE_NONE, head_hull, ent (), &tr);

   if (tr.flFraction < 1.0f || tr.fAllSolid) {
      return Vector::null ();
   }
   engine.testHull (end, apex, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

   if (tr.flFraction != 1.0f) {
      float dot = -(tr.vecPlaneNormal | (apex - end).normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) {
         return Vector::null ();
      }
   }
   return velocity * 0.777f;
}

Vector Bot::calcThrow (const Vector &start, const Vector &stop) {
   // this function returns the velocity vector at which an object should be thrown from start to hit end.
   // returns null vector if throw is not feasible.

   Vector velocity = stop - start;
   TraceResult tr;

   float gravity = sv_gravity.flt () * 0.55f;
   float time = velocity.length () / 195.0f;

   if (time < 0.01f) {
      return Vector::null ();
   }
   else if (time > 2.0f) {
      time = 1.2f;
   }
   velocity = velocity * (1.0f / time);
   velocity.z += gravity * time * 0.5f;

   Vector apex = start + (stop - start) * 0.5f;
   apex.z += 0.5f * gravity * (time * 0.5f) * (time * 0.5f);

   engine.testHull (start, apex, TRACE_IGNORE_NONE, head_hull, ent (), &tr);

   if (tr.flFraction != 1.0f) {
      return Vector::null ();
   }
   engine.testHull (stop, apex, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

   if (tr.flFraction != 1.0 || tr.fAllSolid) {
      float dot = -(tr.vecPlaneNormal | (apex - stop).normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) {
         return Vector::null ();
      }
   }
   return velocity * 0.7793f;
}

edict_t *Bot::correctGrenadeVelocity (const char *model) {
   edict_t *pent = nullptr;

   while (!engine.isNullEntity (pent = g_engfuncs.pfnFindEntityByString (pent, "classname", "grenade"))) {
      if (pent->v.owner == ent () && strcmp (STRING (pent->v.model) + 9, model) == 0) {
         // set the correct velocity for the grenade
         if (m_grenade.lengthSq () > 100.0f) {
            pent->v.velocity = m_grenade;
         }
         m_grenadeCheckTime = engine.timebase () + MAX_GRENADE_TIMER;

         selectBestWeapon ();
         completeTask ();

         break;
      }
   }
   return pent;
}

Vector Bot::isBombAudible (void) {
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!g_bombPlanted || taskId () == TASK_ESCAPEFROMBOMB) {
      return Vector::null (); // reliability check
   }

   if (m_difficulty > 3) {
      return waypoints.getBombPos ();
   }
   const Vector &bombOrigin = waypoints.getBombPos ();

   float timeElapsed = ((engine.timebase () - g_timeBombPlanted) / mp_c4timer.flt ()) * 100.0f;
   float desiredRadius = 768.0f;

   // start the manual calculations
   if (timeElapsed > 85.0f) {
      desiredRadius = 4096.0f;
   }
   else if (timeElapsed > 68.0f) {
      desiredRadius = 2048.0f;
   }
   else if (timeElapsed > 52.0f) {
      desiredRadius = 1280.0f;
   }
   else if (timeElapsed > 28.0f) {
      desiredRadius = 1024.0f;
   }

   // we hear bomb if length greater than radius
   if (desiredRadius < (pev->origin - bombOrigin).length2D ()) {
      return bombOrigin;
   }
   return Vector::null ();
}

uint8 Bot::computeMsec (void) {
   // estimate msec to use for this command based on time passed from the previous command

   return static_cast<uint8> ((engine.timebase () - m_lastCommandTime) * 1000.0f);
}

void Bot::processMovement (void) {
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

   m_frameInterval = engine.timebase () - m_lastCommandTime;

   uint8 msecVal = computeMsec ();
   m_lastCommandTime = engine.timebase ();
   //pev->button &= ~IN_ATTACK;
   g_engfuncs.pfnRunPlayerMove (pev->pContainingEntity, m_moveAngles, m_moveSpeed, m_strafeSpeed, 0.0f, static_cast<uint16> (pev->button), static_cast<uint8> (pev->impulse), msecVal);

   // save our own copy of old buttons, since bot ai code is not running every frame now
   m_oldButtons = pev->button;
}

void Bot::checkBurstMode (float distance) {
   // this function checks burst mode, and switch it depending distance to to enemy.

   if (hasShield ()) {
      return; // no checking when shield is active
   }

   // if current weapon is glock, disable burstmode on long distances, enable it else
   if (m_currentWeapon == WEAPON_GLOCK && distance < 300.0f && m_weaponBurstMode == BM_OFF) {
      pev->button |= IN_ATTACK2;
   }
   else if (m_currentWeapon == WEAPON_GLOCK && distance >= 300.0f && m_weaponBurstMode == BM_ON) {
      pev->button |= IN_ATTACK2;
   }

   // if current weapon is famas, disable burstmode on short distances, enable it else
   if (m_currentWeapon == WEAPON_FAMAS && distance > 400.0f && m_weaponBurstMode == BM_OFF) {
      pev->button |= IN_ATTACK2;
   }
   else if (m_currentWeapon == WEAPON_FAMAS && distance <= 400.0f && m_weaponBurstMode == BM_ON) {
      pev->button |= IN_ATTACK2;
   }
}

void Bot::checkSilencer (void) {
   if (((m_currentWeapon == WEAPON_USP && m_difficulty < 2) || m_currentWeapon == WEAPON_M4A1) && !hasShield ()) {
      int prob = (m_personality == PERSONALITY_RUSHER ? 35 : 65);

      // aggressive bots don't like the silencer
      if (rng.getInt (1, 100) <= (m_currentWeapon == WEAPON_USP ? prob / 3 : prob)) {

         // is the silencer not attached...
         if (pev->weaponanim > 6) {
            pev->button |= IN_ATTACK2; // attach the silencer
         }
      }
      else {

         // is the silencer attached...
         if (pev->weaponanim <= 6) {
            pev->button |= IN_ATTACK2; // detach the silencer
         }
      }
   }
}

float Bot::getBombTimeleft (void) {
   if (!g_bombPlanted) {
      return 0.0f;
   }
   float timeLeft = ((g_timeBombPlanted + mp_c4timer.flt ()) - engine.timebase ());

   if (timeLeft < 0.0f) {
      return 0.0f;
   }
   return timeLeft;
}

bool Bot::isOutOfBombTimer (void) {
   if (!(g_mapType & MAP_DE)) {
      return false;
   }

   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX || (m_hasProgressBar || taskId () == TASK_ESCAPEFROMBOMB)) {
      return false; // if CT bot already start defusing, or already escaping, return false
   }

   // calculate left time
   float timeLeft = getBombTimeleft ();

   // if time left greater than 13, no need to do other checks
   if (timeLeft > 13.0f) {
      return false;
   }
   const Vector &bombOrigin = waypoints.getBombPos ();

   // for terrorist, if timer is lower than 13 seconds, return true
   if (timeLeft < 13.0f && m_team == TEAM_TERRORIST && (bombOrigin - pev->origin).length () < 1000.0f) {
      return true;
   }
   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (int i = 0; i < engine.maxClients (); i++) {
      auto *bot = bots.getBot (i);

      // search players with defuse kit
      if (bot != nullptr && bot != this && bot->m_team == TEAM_COUNTER && bot->m_hasDefuser && (bombOrigin - bot->pev->origin).length () < 500.0f) {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   float reachTime = waypoints.calculateTravelTime (pev->maxspeed, m_currentPath->origin, bombOrigin);

   // for counter-terrorist check alos is we have time to reach position plus average defuse time
   if ((timeLeft < reachTime + 8.0f && !m_hasDefuser && !hasTeammatesWithDefuserKit) || (timeLeft < reachTime + 4.0f && m_hasDefuser)) {
      return true;
   }

   if (m_hasProgressBar && isOnFloor () && ((m_hasDefuser ? 10.0f : 15.0f) > getBombTimeleft ())) {
      return true;
   }
   return false; // return false otherwise
}

void Bot::processHearing (void) {
   int hearEnemyIndex = INVALID_WAYPOINT_INDEX;
   float minDistance = 99999.0f;

   // loop through all enemy clients to check for hearable stuff
   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.ent == ent () || client.team == m_team || client.timeSoundLasting < engine.timebase ()) {
         continue;
      }
      float distance = (client.soundPos - pev->origin).length ();

      if (distance > client.hearingDistance) {
         continue;
      }

      if (distance < minDistance) {
         hearEnemyIndex = i;
         minDistance = distance;
      }
   }
   edict_t *player = nullptr;

   if (hearEnemyIndex >= 0 && g_clients[hearEnemyIndex].team != m_team && !(g_gameFlags & GAME_CSDM_FFA)) {
      player = g_clients[hearEnemyIndex].ent;
   }

   // did the bot hear someone ?
   if (player != nullptr && isPlayer (player)) {
      // change to best weapon if heard something
      if (m_shootTime < engine.timebase () - 5.0f && isOnFloor () && m_currentWeapon != WEAPON_C4 && m_currentWeapon != WEAPON_EXPLOSIVE && m_currentWeapon != WEAPON_SMOKE && m_currentWeapon != WEAPON_FLASHBANG && !yb_jasonmode.boolean ()) {
         selectBestWeapon ();
      }

      m_heardSoundTime = engine.timebase ();
      m_states |= STATE_HEARING_ENEMY;

      if (rng.getInt (0, 100) < 15 && engine.isNullEntity (m_enemy) && engine.isNullEntity (m_lastEnemy) && m_seeEnemyTime + 7.0f < engine.timebase ()) {
         pushChatterMessage (Chatter_HeardEnemy);
      }

      // didn't bot already have an enemy ? take this one...
      if (m_lastEnemyOrigin.empty () || m_lastEnemy == nullptr) {
         m_lastEnemy = player;
         m_lastEnemyOrigin = player->v.origin;
      }

      // bot had an enemy, check if it's the heard one
      else  {
         if (player == m_lastEnemy) {
            // bot sees enemy ? then bail out !
            if (m_states & STATE_SEEING_ENEMY) {
               return;
            }
            m_lastEnemyOrigin = player->v.origin;
         }
         else {
            // if bot had an enemy but the heard one is nearer, take it instead
            float distance = (m_lastEnemyOrigin - pev->origin).lengthSq ();

            if (distance > (player->v.origin - pev->origin).lengthSq () && m_seeEnemyTime + 2.0f < engine.timebase ()) {
               m_lastEnemy = player;
               m_lastEnemyOrigin = player->v.origin;
            }
            else {
               return;
            }
         }
      }
      extern ConVar yb_shoots_thru_walls;

      // check if heard enemy can be seen
      if (checkBodyParts (player, &m_enemyOrigin, &m_visibility)) {
         m_enemy = player;
         m_lastEnemy = player;
         m_lastEnemyOrigin = m_enemyOrigin;

         m_states |= STATE_SEEING_ENEMY;
         m_seeEnemyTime = engine.timebase ();
      }

      // check if heard enemy can be shoot through some obstacle
      else {
         if (m_difficulty > 2 && m_lastEnemy == player && m_seeEnemyTime + 3.0 > engine.timebase () && yb_shoots_thru_walls.boolean () && isPenetrableObstacle (player->v.origin)) {
            m_enemy = player;
            m_lastEnemy = player;
            m_enemyOrigin = player->v.origin;
            m_lastEnemyOrigin = player->v.origin;

            m_states |= (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY);
            m_seeEnemyTime = engine.timebase ();
         }
      }
   }
}

bool Bot::isShootableBreakable (edict_t *ent) {
   // this function is checking that pointed by ent pointer obstacle, can be destroyed.

   if ((strcmp (STRING (ent->v.classname), "func_breakable") == 0 || strcmp (STRING (ent->v.classname), "func_pushable") == 0) && (ent->v.spawnflags & SF_PUSH_BREAKABLE)) {
      return (ent->v.takedamage != DAMAGE_NO) && ent->v.impulse <= 0 && !(ent->v.flags & FL_WORLDBRUSH) && !(ent->v.spawnflags & SF_BREAK_TRIGGER_ONLY) && ent->v.health < 500.0f;
   }
   return false;
}

void Bot::processBuyzoneEntering (int buyState) {
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (m_seeEnemyTime + 12.0f < engine.timebase () && m_lastEquipTime + 15.0f < engine.timebase () && m_inBuyZone && (g_timeRoundStart + rng.getFloat (10.0f, 20.0f) + mp_buytime.flt () < engine.timebase ()) && !g_bombPlanted && m_moneyAmount > g_botBuyEconomyTable[0]) {
      m_ignoreBuyDelay = true;
      m_buyingFinished = false;
      m_buyState = buyState;

      // push buy message
      pushMsgQueue (GAME_MSG_PURCHASE);

      m_nextBuyTime = engine.timebase ();
      m_lastEquipTime = engine.timebase ();
   }
}

bool Bot::isBombDefusing (const Vector &bombOrigin) {
   // this function finds if somebody currently defusing the bomb.

   if (!g_bombPlanted)
      return false;

   bool defusingInProgress = false;

   for (int i = 0; i < engine.maxClients (); i++) {
      Bot *bot = bots.getBot (i);

      if (bot == nullptr || bot == this) {
         continue; // skip invalid bots
      }

      if (m_team != bot->m_team || bot->taskId () == TASK_ESCAPEFROMBOMB) {
         continue; // skip other mess
      }

      if ((bot->pev->origin - bombOrigin).length () < 140.0f && (bot->taskId () == TASK_DEFUSEBOMB || bot->m_hasProgressBar)) {
         defusingInProgress = true;
         break;
      }
      const Client &client = g_clients[i];

      // take in account peoples too
      if (defusingInProgress || !(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || isFakeClient (client.ent)) {
         continue;
      }

      if ((client.ent->v.origin - bombOrigin).length () < 140.0f && ((client.ent->v.button | client.ent->v.oldbuttons) & IN_USE)) {
         defusingInProgress = true;
         break;
      }
   }
   return defusingInProgress;
}

float Bot::getShiftSpeed (void) {
   if (taskId () == TASK_SEEKCOVER || (pev->flags & FL_DUCKING) || (pev->button & IN_DUCK) || (m_oldButtons & IN_DUCK) || (m_currentTravelFlags & PATHFLAG_JUMP) || (m_currentPath != nullptr && m_currentPath->flags & FLAG_LADDER) || isOnLadder () || isInWater ()) {
      return pev->maxspeed;
   }
   return static_cast<float> (pev->maxspeed * 0.4f);
}
