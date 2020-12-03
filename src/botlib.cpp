//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_debug ("yb_debug", "0", "Enables or disables useful messages about bot states. Not required for end users.", true, 0.0f, 4.0f);
ConVar cv_debug_goal ("yb_debug_goal", "-1", "Forces all alive bots to build path and go to the specified here graph node.", true, -1.0f, kMaxNodes);
ConVar cv_user_follow_percent ("yb_user_follow_percent", "20", "Specifies the percent of bots, than can follow leader on each round start.", true, 0.0f, 100.0f);
ConVar cv_user_max_followers ("yb_user_max_followers", "1", "Specifies how many bots can follow a single user.", true, 0.0f, static_cast <float> (kGameMaxPlayers / 2));

ConVar cv_jasonmode ("yb_jasonmode", "0", "If enabled, all bots will be forced only the knife, skipping weapon buying routines.");
ConVar cv_radio_mode ("yb_radio_mode", "2", "Allows bots to use radio or chattter.\nAllowed values: '0', '1', '2'.\nIf '0', radio and chatter is disabled.\nIf '1', only radio allowed.\nIf '2' chatter and radio allowed.", true, 0.0f, 2.0f);

ConVar cv_economics_rounds ("yb_economics_rounds", "1", "Specifies whether bots able to use team economics, like do not buy any weapons for whole team to keep money for better guns.");
ConVar cv_walking_allowed ("yb_walking_allowed", "1", "Specifies whether bots able to use 'shift' if they thinks that enemy is near.");
ConVar cv_camping_allowed ("yb_camping_allowed", "1", "Allows or disallows bots to camp. Doesn't affects bomb/hostage defending tasks");

ConVar cv_tkpunish ("yb_tkpunish", "1", "Allows or disallows bots to take revenge of teamkillers / team attacks.");
ConVar cv_freeze_bots ("yb_freeze_bots", "0", "If enabled the bots think function is disabled, so bots will not move anywhere from their spawn spots.");
ConVar cv_spraypaints ("yb_spraypaints", "1", "Allows or disallows the use of spay paints.");
ConVar cv_botbuy ("yb_botbuy", "1", "Allows or disallows bots weapon buying routines.");
ConVar cv_destroy_breakables_around ("yb_destroy_breakables_around", "1", "Allows bots to destroy breakables around him, even without touching with them.");

ConVar cv_chatter_path ("yb_chatter_path", "sound/radio/bot", "Specifies the paths for the bot chatter sound files.", false);
ConVar cv_restricted_weapons ("yb_restricted_weapons", "", "Specifies semicolon separated list of weapons that are not allowed to buy / pickup.", false);

ConVar cv_attack_monsters ("yb_attack_monsters", "0", "Allows or disallows bots to take attack monsters.");
ConVar cv_pickup_custom_items ("yb_pickup_custom_items", "0", "Allows or disallows bots to pickup custom items.");

// game console variables
ConVar mp_c4timer ("mp_c4timer", nullptr, Var::GameRef);
ConVar mp_flashlight ("mp_flashlight", nullptr, Var::GameRef);
ConVar mp_buytime ("mp_buytime", nullptr, Var::GameRef, true, "1");
ConVar mp_startmoney ("mp_startmoney", nullptr, Var::GameRef, true, "800");
ConVar mp_footsteps ("mp_footsteps", nullptr, Var::GameRef);

ConVar sv_gravity ("sv_gravity", nullptr, Var::GameRef);

void Bot::pushMsgQueue (int message) {
   // this function put a message into the bot message queue

   if (message == BotMsg::Say) {
      // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
      int entityIndex = index ();

      for (const auto &other : bots) {
         if (other->pev != pev) {
            if (m_notKilled == other->m_notKilled) {
               other->m_sayTextBuffer.entityIndex = entityIndex;
               other->m_sayTextBuffer.sayText = m_chatBuffer;
            }
            other->m_sayTextBuffer.timeNextChat = game.time () + other->m_sayTextBuffer.chatDelay;
         }
      }
   }
   m_msgQueue.emplaceLast (message);
}

float Bot::isInFOV (const Vector &destination) {
   float entityAngle = cr::modAngles (destination.yaw ()); // find yaw angle from source to destination...
   float viewAngle = cr::modAngles (pev->v_angle.y); // get bot's current view angle...

   // return the absolute value of angle to destination entity
   // zero degrees means straight ahead, 45 degrees to the left or
   // 45 degrees to the right is the limit of the normal view angle
   float absoluteAngle = cr::abs (viewAngle - entityAngle);

   if (absoluteAngle > 180.0f) {
      absoluteAngle = 360.0f - absoluteAngle;
   }
   return absoluteAngle;
}

bool Bot::isInViewCone (const Vector &origin) {
   // this function returns true if the spatial vector location origin is located inside
   // the field of view cone of the bot entity, false otherwise. It is assumed that entities
   // have a human-like field of view, that is, about 90 degrees.

   return util.isInViewCone (origin, ent ());
}

bool Bot::seesItem (const Vector &destination, const char *classname) {
   TraceResult tr {};

   // trace a line from bot's eyes to destination..
   game.testLine (getEyesPos (), destination, TraceIgnore::None, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   if (tr.flFraction >= 0.95f && tr.pHit && tr.pHit != game.getStartEntity ()) {
      return strcmp (tr.pHit->v.classname.chars (), classname) == 0;
   }
   return true;
}

bool Bot::seesEntity (const Vector &dest, bool fromBody) {
   TraceResult tr {};

   // trace a line from bot's eyes to destination...
   game.testLine (fromBody ? pev->origin : getEyesPos (), dest, TraceIgnore::Everything, ent (), &tr);

   // check if line of sight to object is not blocked (i.e. visible)
   return tr.flFraction >= 1.0f;
}

void Bot::checkGrenadesThrow () {

   // do not check cancel if we have grenade in out hands
   bool preventibleTasks = getCurrentTaskId () == Task::PlantBomb || getCurrentTaskId () == Task::DefuseBomb;

   auto clearThrowStates = [] (uint32 &states) {
      states &= ~(Sense::ThrowExplosive | Sense::ThrowFlashbang | Sense::ThrowSmoke);
   };

   // check if throwing a grenade is a good thing to do...
   if (preventibleTasks || isInNarrowPlace () || cv_ignore_enemies.bool_ () || m_isUsingGrenade || m_grenadeRequested || m_isReloading || cv_jasonmode.bool_ () || m_grenadeCheckTime >= game.time ()) {
      clearThrowStates (m_states);
      return;
   }

   // check again in some seconds
   m_grenadeCheckTime = game.time () + 0.5f;

   if (!util.isAlive (m_lastEnemy) || !(m_states & (Sense::SuspectEnemy | Sense::HearingEnemy))) {
      clearThrowStates (m_states);
      return;
   }

   // check if we have grenades to throw
   int grenadeToThrow = bestGrenadeCarried ();

   // if we don't have grenades no need to check it this round again
   if (grenadeToThrow == -1) {
      m_grenadeCheckTime = game.time () + 15.0f; // changed since, conzero can drop grens from dead players

      clearThrowStates (m_states);
      return;
   }
   else {
      int cancelProb = 20;

      if (grenadeToThrow == Weapon::Flashbang) {
         cancelProb = 25;
      }
      else if (grenadeToThrow == Weapon::Smoke) {
         cancelProb = 35;
      }
      if (rg.chance (cancelProb)) {
         clearThrowStates (m_states);
         return;
      }
   }
   float distance = (m_lastEnemyOrigin - pev->origin).length2d ();

   // don't throw grenades at anything that isn't on the ground!
   if (!(m_lastEnemy->v.flags & FL_ONGROUND) && !m_lastEnemy->v.waterlevel && m_lastEnemyOrigin.z > pev->absmax.z) {
      distance = kInfiniteDistance;
   }

   // too high to throw?
   if (m_lastEnemy->v.origin.z > pev->origin.z + 500.0f) {
      distance = kInfiniteDistance;
   }

   // enemy within a good throw distance?
   if (!m_lastEnemyOrigin.empty () && distance > (grenadeToThrow == Weapon::Smoke ? 200.0f : 400.0f) && distance < 1200.0f) {
      bool allowThrowing = true;

      // care about different grenades
      switch (grenadeToThrow) {
      case Weapon::Explosive:
         if (numFriendsNear (m_lastEnemy->v.origin, 256.0f) > 0) {
            allowThrowing = false;
         }
         else {
            float radius = m_lastEnemy->v.velocity.length2d ();
            const Vector &pos = (m_lastEnemy->v.velocity * 0.5f).get2d () + m_lastEnemy->v.origin;

            if (radius < 164.0f) {
               radius = 164.0f;
            }
            auto predicted = graph.searchRadius (radius, pos, 12);

            if (predicted.empty ()) {
               m_states &= ~Sense::ThrowExplosive;
               break;
            }

            for (const auto predict : predicted) {
               allowThrowing = true;

               if (!graph.exists (predict)) {
                  allowThrowing = false;
                  continue;
               }
               m_throw = graph[predict].origin;

               auto throwPos = calcThrow (getEyesPos (), m_throw);

               if (throwPos.lengthSq () < 100.0f) {
                  throwPos = calcToss (getEyesPos (), m_throw);
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
            m_states |= Sense::ThrowExplosive;
         }
         else {
            m_states &= ~Sense::ThrowExplosive;
         }
         break;

      case Weapon::Flashbang: {
         int nearest = graph.getNearest ((m_lastEnemy->v.velocity * 0.5f).get2d () + m_lastEnemy->v.origin);

         if (nearest != kInvalidNodeIndex) {
            m_throw = graph[nearest].origin;

            if (numFriendsNear (m_throw, 256.0f) > 0) {
               allowThrowing = false;
            }
         }
         else {
            allowThrowing = false;
         }

         if (allowThrowing) {
            auto throwPos = calcThrow (getEyesPos (), m_throw);

            if (throwPos.lengthSq () < 100.0f) {
               throwPos = calcToss (getEyesPos (), m_throw);
            }

            if (throwPos.empty ()) {
               allowThrowing = false;
            }
            else {
               m_throw.z += 110.0f;
            }
         }

         if (allowThrowing) {
            m_states |= Sense::ThrowFlashbang;
         }
         else {
            m_states &= ~Sense::ThrowFlashbang;
         }
         break;
      }

      case Weapon::Smoke:
         if (allowThrowing && !game.isNullEntity (m_lastEnemy)) {
            if (util.getShootingCone (m_lastEnemy, pev->origin) >= 0.9f) {
               allowThrowing = false;
            }
         }

         if (allowThrowing) {
            m_states |= Sense::ThrowSmoke;
         }
         else {
            m_states &= ~Sense::ThrowSmoke;
         }
         break;

      default:
         clearThrowStates (m_states);
         return;
      }
      const float MaxThrowTime = game.time () + 0.3f;

      if (m_states & Sense::ThrowExplosive) {
         startTask (Task::ThrowExplosive, TaskPri::Throw, kInvalidNodeIndex, MaxThrowTime, false);
      }
      else if (m_states & Sense::ThrowFlashbang) {
         startTask (Task::ThrowFlashbang, TaskPri::Throw, kInvalidNodeIndex, MaxThrowTime, false);
      }
      else if (m_states & Sense::ThrowSmoke) {
         startTask (Task::ThrowSmoke, TaskPri::Throw, kInvalidNodeIndex, MaxThrowTime, false);
      }
   }
   else {
      clearThrowStates (m_states);
   }
}

void Bot::avoidGrenades () {
   // checks if bot 'sees' a grenade, and avoid it

   if (!bots.hasActiveGrenades ()) {
      return;
   }

   // check if old pointers to grenade is invalid
   if (game.isNullEntity (m_avoidGrenade)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }
   else if ((m_avoidGrenade->v.flags & FL_ONGROUND) || (m_avoidGrenade->v.effects & EF_NODRAW)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }
   auto &activeGrenades = bots.getActiveGrenades ();

   // find all grenades on the map
   for (auto pent : activeGrenades) {
      if (pent->v.effects & EF_NODRAW) {
         continue;
      }

      // check if visible to the bot
      if (!seesEntity (pent->v.origin) && isInFOV (pent->v.origin - getEyesPos ()) > pev->fov * 0.5f) {
         continue;
      }
      auto model = pent->v.model.chars (9);

      if (m_preventFlashing < game.time () && m_personality == Personality::Rusher && m_difficulty == Difficulty::Expert && strcmp (model, "flashbang.mdl") == 0) {
         // don't look at flash bang
         if (!(m_states & Sense::SeeingEnemy)) {
            pev->v_angle.y = cr::normalizeAngles ((game.getEntityWorldOrigin (pent) - getEyesPos ()).angles ().y + 180.0f);

            m_canChooseAimDirection = false;
            m_preventFlashing = game.time () + rg.get (1.0f, 2.0f);
         }
      }
      else if (strcmp (model, "hegrenade.mdl") == 0) {
         if (!game.isNullEntity (m_avoidGrenade)) {
            return;
         }

         if (game.getTeam (pent->v.owner) == m_team || pent->v.owner == ent ()) {
            return;
         }

         if (!(pent->v.flags & FL_ONGROUND)) {
            float distance = (pent->v.origin - pev->origin).lengthSq ();
            float distanceMoved = ((pent->v.origin + pent->v.velocity * getFrameInterval ()) - pev->origin).lengthSq ();

            if (distanceMoved < distance && distance < cr::square (500.0f)) {
               const auto &dirToPoint = (pev->origin - pent->v.origin).normalize2d ();
               const auto &rightSide = pev->v_angle.right ().normalize2d ();

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
      else if ((pent->v.flags & FL_ONGROUND) && strcmp (model, "smokegrenade.mdl") == 0) {
         if (isInFOV (pent->v.origin - getEyesPos ()) < pev->fov - 7.0f) {
            float distance = (pent->v.origin - pev->origin).length ();

            // shrink bot's viewing distance to smoke grenade's distance
            if (m_viewDistance > distance) {
               m_viewDistance = distance;

               if (rg.chance (45)) {
                  pushChatterMessage (Chatter::BehindSmoke);
               }
            }
         }
      }
   }
}

void Bot::checkBreakable (edict_t *touch) {
   if (!game.isShootableBreakable (touch)) {
      return;
   }
   m_breakableEntity = lookupBreakable ();

   if (game.isNullEntity (m_breakableEntity)) {
      return;
   }
   m_campButtons = pev->button & IN_DUCK;
   startTask (Task::ShootBreakable, TaskPri::ShootBreakable, kInvalidNodeIndex, 0.0f, false);
}

void Bot::checkBreakablesAround () {
   if (!cv_destroy_breakables_around.bool_ () || usesKnife () || rg.chance (25) || !game.hasBreakables () || m_seeEnemyTime + 4.0f > game.time () || !game.isNullEntity (m_enemy) || !hasPrimaryWeapon ()) {
      return;
   }

   // check if we're have some breakbles in 400 units range
   for (const auto &breakable : game.getBreakables ()) {
      if (!game.isShootableBreakable (breakable)) {
         continue;
      }
      const auto &origin = game.getEntityWorldOrigin (breakable);
      const auto lengthToObstacle = (origin - pev->origin).lengthSq ();

      // too far, skip it
      if (lengthToObstacle > cr::square (400.0f)) {
         continue;
      }

      // too close, skip it
      if (lengthToObstacle < cr::square (100.0f)) {
         continue;
      }

      if (isInFOV (origin - getEyesPos ()) < pev->fov && seesEntity (origin)) {
         m_breakableOrigin = origin;
         m_breakableEntity = breakable;
         m_campButtons = pev->button & IN_DUCK;

         startTask (Task::ShootBreakable, TaskPri::ShootBreakable, kInvalidNodeIndex, 0.0f, false);
         break;
      }
   }
}

edict_t *Bot::lookupBreakable () {
   // this function checks if bot is blocked by a shoot able breakable in his moving direction

   TraceResult tr {};
   game.testLine (pev->origin, pev->origin + (m_destOrigin - pev->origin).normalize () * 72.0f, TraceIgnore::None, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      auto ent = tr.pHit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (game.isShootableBreakable (ent)) {
         m_breakableOrigin = game.getEntityWorldOrigin (ent);
         return ent;
      }
   }
   game.testLine (getEyesPos (), getEyesPos () + (m_destOrigin - getEyesPos ()).normalize () * 72.0f, TraceIgnore::None, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      auto ent = tr.pHit;

      if (game.isShootableBreakable (ent)) {
         m_breakableOrigin = game.getEntityWorldOrigin (ent);
         return ent;
      }
   }
   m_breakableEntity = nullptr;
   m_breakableOrigin = nullptr;

   return nullptr;
}

void Bot::setIdealReactionTimers (bool actual) {

   // zero out reaction times for extreme mode
   if (cv_whose_your_daddy.bool_ ()) {
      m_idealReactionTime = 0.05f;
      m_actualReactionTime = 0.095f;

      return;
   }
   const auto tweak = conf.getDifficultyTweaks (m_difficulty);

   if (actual) {
      m_idealReactionTime = tweak->reaction[0];
      m_actualReactionTime = tweak->reaction[0];

      return;
   }
   m_idealReactionTime = rg.get (tweak->reaction[0], tweak->reaction[1]);
}

void Bot::updatePickups () {
   // this function finds Items to collect or use in the near of a bot

   // don't try to pickup anything while on ladder or trying to escape from bomb...
   if (isOnLadder () || getCurrentTaskId () == Task::EscapeFromBomb || cv_jasonmode.bool_ () || !bots.hasIntrestingEntities ()) {
      m_pickupItem = nullptr;
      m_pickupType = Pickup::None;

      return;
   }

   const auto &intresting = bots.getIntrestingEntities ();
   const float radius = cr::square (500.0f);

   if (!game.isNullEntity (m_pickupItem)) {
      bool itemExists = false;
      auto pickupItem = m_pickupItem;

      for (auto &ent : intresting) {

         // in the periods of updating intresting entities we can get fake ones, that already were picked up, so double check if drawn
         if (ent->v.effects & EF_NODRAW) {
            continue;
         }
         const Vector &origin = game.getEntityWorldOrigin (ent);

         // too far from us ?
         if ((pev->origin - origin).lengthSq () > radius) {
            continue;
         }

         if (ent == pickupItem) {
            if (seesItem (origin, ent->v.classname.chars ())) {
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
         m_pickupType = Pickup::None;
      }
   }
   edict_t *pickupItem = nullptr;

   Pickup pickupType = Pickup::None;
   Vector pickupPos = nullptr;

   m_pickupItem = nullptr;
   m_pickupType = Pickup::None;

   for (const auto &ent : intresting) {
      bool allowPickup = false; // assume can't use it until known otherwise

      // get the entity origin
      const auto &origin = game.getEntityWorldOrigin (ent);
     
      if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore || cr::abs (origin.z - pev->origin.z) > 96.0f) {
         continue; // someone owns this weapon or it hasn't respawned yet
      }

      // too far from us ?
      if ((pev->origin - origin).lengthSq () > radius) {
         continue;
      }

      auto classname = ent->v.classname.chars ();
      auto model = ent->v.model.chars (9);

      // check if line of sight to object is not blocked (i.e. visible)
      if (seesItem (origin, classname)) {
         if (strncmp ("hostage_entity", classname, 14) == 0) {
            allowPickup = true;
            pickupType = Pickup::Hostage;
         }
         else if (strncmp ("weaponbox", classname, 9) == 0 && strcmp (model, "backpack.mdl") == 0) {
            allowPickup = true;
            pickupType = Pickup::DroppedC4;
         }
         else if ((strncmp ("weaponbox", classname, 9) == 0 || strncmp ("armoury_entity", classname, 14) == 0 || strncmp ("csdm", classname, 4) == 0) && !m_isUsingGrenade) {
            allowPickup = true;
            pickupType = Pickup::Weapon;
         }
         else if (strncmp ("weapon_shield", classname, 13) == 0 && !m_isUsingGrenade) {
            allowPickup = true;
            pickupType = Pickup::Shield;
         }
         else if (strncmp ("item_thighpack", classname, 14) == 0 && m_team == Team::CT && !m_hasDefuser) {
            allowPickup = true;
            pickupType = Pickup::DefusalKit;
         }
         else if (strncmp ("grenade", classname, 7) == 0 && strcmp (model, "c4.mdl") == 0) {
            allowPickup = true;
            pickupType = Pickup::PlantedC4;
         }
         else if (cv_pickup_custom_items.bool_ () && util.isItem(ent) && strncmp ("item_thighpack", classname, 14) != 0) {
            allowPickup = true;
            pickupType = Pickup::None;
         }
      }
      
      // if the bot found something it can pickup...
      if (allowPickup) {

         // found weapon on ground?
         if (pickupType == Pickup::Weapon) {
            int primaryWeaponCarried = bestPrimaryCarried ();
            int secondaryWeaponCarried = bestSecondaryCarried ();

            const auto &config = conf.getWeapons ();
            const auto &primary = config[primaryWeaponCarried];
            const auto &secondary = config[secondaryWeaponCarried];
            const auto &primaryProp = conf.getWeaponProp (primary.id);
            const auto &secondaryProp = conf.getWeaponProp (secondary.id);

            if (secondaryWeaponCarried < 7 && (m_ammo[secondary.id] > 0.3 * secondaryProp.ammo1Max) && strcmp (model, "w_357ammobox.mdl") == 0) {
               allowPickup = false;
            }
            else if (!m_isVIP && primaryWeaponCarried >= 7 && (m_ammo[primary.id] > 0.3 * primaryProp.ammo1Max) && strncmp (model, "w_", 2) == 0) {
               auto weaponType = conf.getWeaponType (primaryWeaponCarried);
               
               const bool isSniperRifle = weaponType == WeaponType::Sniper;
               const bool isSubmachine = weaponType == WeaponType::SMG;
               const bool isShotgun = weaponType == WeaponType::Shotgun;
               const bool isRifle = weaponType == WeaponType::Rifle || weaponType == WeaponType::ZoomRifle;

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
               else if (strcmp (model, "w_chainammo.mdl") == 0 && primaryWeaponCarried != Weapon::M249) {
                  allowPickup = false;
               }
            }
            else if (m_isVIP || !rateGroundWeapon (ent)) {
               allowPickup = false;
            }
            else if (strcmp (model, "medkit.mdl") == 0 && m_healthValue >= 100.0f) {
               allowPickup = false;
            }
            else if ((strcmp (model, "kevlar.mdl") == 0 || strcmp (model, "battery.mdl") == 0) && pev->armorvalue >= 100.0f) {
               allowPickup = false;
            }
            else if (strcmp (model, "flashbang.mdl") == 0 && (pev->weapons & cr::bit (Weapon::Flashbang))) {
               allowPickup = false;
            }
            else if (strcmp (model, "hegrenade.mdl") == 0 && (pev->weapons & cr::bit (Weapon::Explosive))) {
               allowPickup = false;
            }
            else if (strcmp (model, "smokegrenade.mdl") == 0 && (pev->weapons & cr::bit (Weapon::Smoke))) {
               allowPickup = false;
            }
         }

         // found a shield on ground?
         else if (pickupType == Pickup::Shield) {
            if ((pev->weapons & cr::bit (Weapon::Elite)) || hasShield () || m_isVIP || (hasPrimaryWeapon () && !rateGroundWeapon (ent))) {
               allowPickup = false;
            }
         }

         // terrorist team specific
         else if (m_team == Team::Terrorist) {
            if (pickupType == Pickup::DroppedC4) {
               m_destOrigin = origin; // ensure we reached dropped bomb

               pushChatterMessage (Chatter::FoundC4); // play info about that
               clearSearchNodes ();
            }
            else if (pickupType == Pickup::Hostage) {
               m_itemIgnore = ent;
               allowPickup = false;

               if (!m_defendHostage && m_personality != Personality::Rusher && m_difficulty > Difficulty::Normal && rg.chance (15) && m_timeCamping + 15.0f < game.time ()) {
                  int index = findDefendNode (origin);

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (30.0f, 60.0f), true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg.get (3.0f, 6.0f), true); // push move command

                  if (graph[index].vis.crouch <= graph[index].vis.stand) {
                     m_campButtons |= IN_DUCK;
                  }
                  else {
                     m_campButtons &= ~IN_DUCK;
                  }
                  m_defendHostage = true;

                  pushChatterMessage (Chatter::GoingToGuardHostages); // play info about that
                  return;
               }
            }
            else if (pickupType == Pickup::PlantedC4) {
               allowPickup = false;

               if (!m_defendedBomb) {
                  m_defendedBomb = true;

                  int index = findDefendNode (origin);
                  const Path &path = graph[index];

                  float bombTimer = mp_c4timer.float_ ();
                  float timeMidBlowup = bots.getTimeBombPlanted () + (bombTimer * 0.5f + bombTimer * 0.25f) - graph.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                  if (timeMidBlowup > game.time ()) {
                     clearTask (Task::MoveToPosition); // remove any move tasks

                     startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, timeMidBlowup, true); // push camp task on to stack
                     startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, timeMidBlowup, true); // push  move command

                     if (path.vis.crouch <= path.vis.stand) {
                        m_campButtons |= IN_DUCK;
                     }
                     else {
                        m_campButtons &= ~IN_DUCK;
                     }
                     if (rg.chance (90)) {
                        pushChatterMessage (Chatter::DefendingBombsite);
                     }
                  }
                  else {
                     pushRadioMessage (Radio::ShesGonnaBlow); // issue an additional radio message
                  }
               }
            }
         }
         else if (m_team == Team::CT) {
            if (pickupType == Pickup::Hostage) {
               if (game.isNullEntity (ent) || ent->v.health <= 0) {
                  allowPickup = false; // never pickup dead hostage
               }
               else {
                  for (const auto &other : bots) {
                     if (other->m_notKilled) {
                        for (const auto &hostage : other->m_hostages) {
                           if (hostage == ent) {
                              allowPickup = false;
                              break;
                           }
                        }
                     }
                  }
               }
            }
            else if (pickupType == Pickup::PlantedC4) {
               if (util.isAlive (m_enemy)) {
                  allowPickup = false;
                  return;
               }

               if (isOutOfBombTimer ()) {
                  completeTask ();

                  // then start escape from bomb immediate
                  startTask (Task::EscapeFromBomb, TaskPri::EscapeFromBomb, kInvalidNodeIndex, 0.0f, true);

                  // and no pickup
                  allowPickup = false;
                  return;
               }

               if (rg.chance (70)) {
                  pushChatterMessage (Chatter::FoundC4Plant);
               }
               allowPickup = !isBombDefusing (origin) || m_hasProgressBar;

               if (!m_defendedBomb && !allowPickup) {
                  m_defendedBomb = true;

                  int index = findDefendNode (origin);
                  const auto &path = graph[index];

                  float timeToExplode = bots.getTimeBombPlanted () + mp_c4timer.float_ () - graph.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                  clearTask (Task::MoveToPosition); // remove any move tasks

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, timeToExplode, true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, timeToExplode, true); // push move command

                  if (path.vis.crouch <= path.vis.stand) {
                     m_campButtons |= IN_DUCK;
                  }
                  else {
                     m_campButtons &= ~IN_DUCK;
                  }

                  if (rg.chance (85)) {
                     pushChatterMessage (Chatter::DefendingBombsite);
                  }
               }

               if ((pev->origin - origin).lengthSq () > cr::square (60.0f)) {
                  
                  if (!graph.isNodeReacheable (pev->origin, origin)) {
                     allowPickup = false;
                  }
               }
            }
            else if (pickupType == Pickup::DroppedC4) {
               m_itemIgnore = ent;
               allowPickup = false;

               if (!m_defendedBomb && m_difficulty >= Difficulty::Normal && rg.chance (75) && m_healthValue < 60) {
                  int index = findDefendNode (origin);

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (30.0f, 70.0f), true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg.get (10.0f, 30.0f), true); // push move command

                  if (graph[index].vis.crouch <= graph[index].vis.stand) {
                     m_campButtons |= IN_DUCK;
                  }
                  else {
                     m_campButtons &= ~IN_DUCK;
                  }
                  m_defendedBomb = true;

                  pushChatterMessage (Chatter::GoingToGuardDroppedC4); // play info about that
                  return;
               }
            }
         }

         // if condition valid
         if (allowPickup) {
            pickupPos = origin; // remember location of entity
            pickupItem = ent; // remember this entity

            m_pickupType = pickupType;
            break;
         }
         else {
            pickupType = Pickup::None;
         }
      }
   } // end of the while loop

   if (!game.isNullEntity (pickupItem)) {
      for (const auto &other : bots) {
         if (other.get () != this && other->m_notKilled && other->m_pickupItem == pickupItem) {
            m_pickupItem = nullptr;
            m_pickupType = Pickup::None;

            return;
         }
      }
      const float highOffset = (m_pickupType == Pickup::Hostage || m_pickupType == Pickup::PlantedC4) ? 50.0f : 20.0f;

      // check if item is too high to reach, check if getting the item would hurt bot
      if (pickupPos.z > getEyesPos ().z + highOffset || isDeadlyMove (pickupPos)) {
         m_itemIgnore = m_pickupItem;
         m_pickupItem = nullptr;
         m_pickupType = Pickup::None;

         return;
      }
      m_pickupItem = pickupItem; // save pointer of picking up entity
   }
}

void Bot::getCampDirection (Vector *dest) {
   // this function check if view on last enemy position is blocked - replace with better vector then
   // mostly used for getting a good camping direction vector if not camping on a camp waypoint

   TraceResult tr {};
   const Vector &src = getEyesPos ();

   game.testLine (src, *dest, TraceIgnore::Monsters, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      float length = (tr.vecEndPos - src).lengthSq ();

      if (length > 10000.0f) {
         return;
      }

      int enemyIndex = graph.getNearest (*dest);
      int tempIndex = graph.getNearest (pev->origin);

      if (tempIndex == kInvalidNodeIndex || enemyIndex == kInvalidNodeIndex) {
         return;
      }
      float minDistance = kInfiniteDistance;

      int lookAtWaypoint = kInvalidNodeIndex;
      const Path &path = graph[tempIndex];

      for (auto &link : path.links) {
         if (link.index == kInvalidNodeIndex) {
            continue;
         }
         auto distance = static_cast <float> (graph.getPathDist (link.index, enemyIndex));

         if (distance < minDistance) {
            minDistance = distance;
            lookAtWaypoint = link.index;
         }
      }

      if (graph.exists (lookAtWaypoint)) {
         *dest = graph[lookAtWaypoint].origin;
      }
   }
}

void Bot::showChaterIcon (bool show) {
   // this function depending on show boolen, shows/remove chatter, icon, on the head of bot.

   if (!game.is (GameFlags::HasBotVoice) || cv_radio_mode.int_ () != 2) {
      return;
   }

   auto sendBotVoice = [](bool show, edict_t *ent, int ownId) {
      MessageWriter (MSG_ONE, msgs.id (NetMsg::BotVoice), nullptr, ent) // begin message
         .writeByte (show) // switch on/off
         .writeByte (ownId);
   };

   int ownIndex = index ();

   for (auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }

      if (!show && (client.iconFlags[ownIndex] & ClientFlags::Icon) && client.iconTimestamp[ownIndex] < game.time ()) {
         sendBotVoice (false, client.ent, entindex ());

         client.iconTimestamp[ownIndex] = 0.0f;
         client.iconFlags[ownIndex] &= ~ClientFlags::Icon;
      }
      else if (show && !(client.iconFlags[ownIndex] & ClientFlags::Icon)) {
         sendBotVoice (true, client.ent, entindex ());
      }
   }
}

void Bot::instantChatter (int type) {
   // this function sends instant chatter messages.
   if (!game.is (GameFlags::HasBotVoice) || cv_radio_mode.int_ () != 2 || !conf.hasChatterBank (type) || !conf.hasChatterBank (Chatter::DiePain)) {
      return;
   }

   auto playbackSound = conf.pickRandomFromChatterBank (type);
   auto painSound = conf.pickRandomFromChatterBank (Chatter::DiePain);

   if (m_notKilled) {
      showChaterIcon (true);
   }
   MessageWriter msg;
   int ownIndex = index ();

   for (auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }
      msg.start (MSG_ONE, msgs.id (NetMsg::SendAudio), nullptr, client.ent); // begin message
      msg.writeByte (ownIndex);

      if (pev->deadflag & DEAD_DYING) {
         client.iconTimestamp[ownIndex] = game.time () + painSound.duration;
         msg.writeString (strings.format ("%s/%s.wav", cv_chatter_path.str (), painSound.name));
      }
      else if (!(pev->deadflag & DEAD_DEAD)) {
         client.iconTimestamp[ownIndex] = game.time () + playbackSound.duration;
         msg.writeString (strings.format ("%s/%s.wav", cv_chatter_path.str (), playbackSound.name));
      }
      msg.writeShort (m_voicePitch).end ();
      client.iconFlags[ownIndex] |= ClientFlags::Icon;
   }
}

void Bot::pushRadioMessage (int message) {
   // this function inserts the radio message into the message queue

   if (cv_radio_mode.int_ () == 0 || m_numFriendsLeft == 0) {
      return;
   }
   m_forceRadio = !game.is (GameFlags::HasBotVoice) || !conf.hasChatterBank (message) || cv_radio_mode.int_ () != 2; // use radio instead voice

   m_radioSelect = message;
   pushMsgQueue (BotMsg::Radio);
}

void Bot::pushChatterMessage (int message) {
   // this function inserts the voice message into the message queue (mostly same as above)

   if (!game.is (GameFlags::HasBotVoice) || cv_radio_mode.int_ () != 2 || !conf.hasChatterBank (message) || m_numFriendsLeft == 0) {
      return;
   }
   bool sendMessage = false;

   auto messageRepeat = conf.getChatterMessageRepeatInterval (message);
   auto &messageTimer = m_chatterTimes[message];

   if (messageTimer < game.time () || cr::fequal (messageTimer, kMaxChatterRepeatInteval)) {
      if (!cr::fequal (messageRepeat, kMaxChatterRepeatInteval)) {
         messageTimer = game.time () + messageRepeat;
      }
      sendMessage = true;
   }

   if (!sendMessage) {
      m_radioSelect = -1;
      return;
   }
   m_radioSelect = message;
   pushMsgQueue (BotMsg::Radio);
}

void Bot::checkMsgQueue () {
   // this function checks and executes pending messages

   extern ConVar mp_freezetime;

   // no new message?
   if (m_msgQueue.empty ()) {
      return;
   }

   // get message from deque
   auto state = m_msgQueue.popFront ();

   // nothing to do?
   if (state == BotMsg::None || (state == BotMsg::Radio && game.is (GameFlags::FreeForAll))) {
      return;
   }

   switch (state) {
   case BotMsg::Buy: // general buy message

      // buy weapon
      if (m_nextBuyTime > game.time ()) {
         // keep sending message
         pushMsgQueue (BotMsg::Buy);
         return;
      }

      if (!m_inBuyZone || game.is (GameFlags::CSDM)) {
         m_buyPending = true;
         m_buyingFinished = true;

         break;
      }

      m_buyPending = false;
      m_nextBuyTime = game.time () + rg.get (0.5f, 1.3f);

      // if freezetime is very low do not delay the buy process
      if (mp_freezetime.float_ () <= 1.0f) {
         m_nextBuyTime = game.time ();
         m_ignoreBuyDelay = true;
      }

      // if bot buying is off then no need to buy
      if (!cv_botbuy.bool_ ()) {
         m_buyState = BuyState::Done;
      }

      // if fun-mode no need to buy
      if (cv_jasonmode.bool_ ()) {
         m_buyState = BuyState::Done;
         selectWeaponByName ("weapon_knife");
      }

      // prevent vip from buying
      if (m_isVIP) {
         m_buyState = BuyState::Done;
         m_pathType = FindPath::Fast;
      }

      // prevent terrorists from buying on es maps
      if (game.mapIs (MapFlags::Escape) && m_team == Team::Terrorist) {
         m_buyState = BuyState::Done;
      }

      // prevent teams from buying on fun maps
      if (game.mapIs (MapFlags::KnifeArena | MapFlags::Fun)) {
         m_buyState = BuyState::Done;

         if (game.mapIs (MapFlags::KnifeArena)) {
            cv_jasonmode.set (1);
         }
      }

      if (m_buyState > BuyState::Done - 1) {
         m_buyingFinished = true;
         return;
      }

      pushMsgQueue (BotMsg::None);
      buyStuff ();

      break;

   case BotMsg::Radio:
      // if last bot radio command (global) happened just a 3 seconds ago, delay response
      if (bots.getLastRadioTimestamp (m_team) + 3.0f < game.time ()) {
         // if same message like previous just do a yes/no
         if (m_radioSelect != Radio::RogerThat && m_radioSelect != Radio::Negative) {
            if (m_radioSelect == bots.getLastRadio (m_team) && bots.getLastRadioTimestamp (m_team) + 1.5f > game.time ()) {
               m_radioSelect = -1;
            }
            else {
               if (m_radioSelect != Radio::ReportingIn) {
                  bots.setLastRadio (m_team, m_radioSelect);
               }
               else {
                  bots.setLastRadio (m_team, -1);
               }

               for (const auto &bot : bots) {
                  if (pev != bot->pev && bot->m_team == m_team) {
                     bot->m_radioOrder = m_radioSelect;
                     bot->m_radioEntity = ent ();
                  }
               }
            }
         }

         if (m_radioSelect != -1) {
            if ((m_radioSelect != Radio::ReportingIn && m_forceRadio) || cv_radio_mode.int_ () != 2 || !conf.hasChatterBank (m_radioSelect) || !game.is (GameFlags::HasBotVoice)) {
               if (m_radioSelect < Radio::GoGoGo) {
                  issueCommand ("radio1");
               }
               else if (m_radioSelect < Radio::RogerThat) {
                  m_radioSelect -= Radio::GoGoGo - 1;
                  issueCommand ("radio2");
               }
               else {
                  m_radioSelect -= Radio::RogerThat - 1;
                  issueCommand ("radio3");
               }

               // select correct menu item for this radio message
               issueCommand ("menuselect %d", m_radioSelect);
            }
            else if (m_radioSelect != Radio::ReportingIn) {
               instantChatter (m_radioSelect);
            }
         }
         m_forceRadio = false; // reset radio to voice
         bots.setLastRadioTimestamp (m_team, game.time ()); // store last radio usage
      }
      else {
         pushMsgQueue (BotMsg::Radio);
      }
      break;

   // team independent saytext
   case BotMsg::Say:
      sendToChat (m_chatBuffer, false);
      break;

   // team dependent saytext
   case BotMsg::SayTeam:
      sendToChat (m_chatBuffer, true);
      break;

   default:
      return;
   }
}

bool Bot::isWeaponRestricted (int weaponIndex) {
   // this function checks for weapon restrictions.

   if (strings.isEmpty (cv_restricted_weapons.str ())) {
      return isWeaponRestrictedAMX (weaponIndex); // no banned weapons
   }
   const auto &bannedWeapons = String (cv_restricted_weapons.str ()).split (";");
   const auto &alias = util.weaponIdToAlias (weaponIndex);

   for (const auto &ban : bannedWeapons) {
      // check is this weapon is banned
      if (ban == alias) {
         return true;
      }
   }
   return isWeaponRestrictedAMX (weaponIndex);
}

bool Bot::isWeaponRestrictedAMX (int weaponIndex) {
   // this function checks restriction set by AMX Mod, this function code is courtesy of KWo.

   if (!game.is (GameFlags::Metamod)) {
      return false;
   }


   // check for weapon restrictions
   if (cr::bit (weaponIndex) & (kPrimaryWeaponMask | kSecondaryWeaponMask | Weapon::Shield)) {
      auto restrictedWeapons = game.findCvar ("amx_restrweapons");

      if (restrictedWeapons.empty ()) {
         return false;
      }
      constexpr int indices[] = {4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (restrictedWeapons.length ())) {
         return false;
      }
      return restrictedWeapons[index] != '0';
   }

   // check for equipment restrictions
   else {
      auto restrictedEquipment = game.findCvar ("amx_restrequipammo");

      if (restrictedEquipment.empty ()) {
         return false;
      }
      constexpr int indices[] = {-1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 0, 1, 5};

      // find the weapon index
      int index = indices[weaponIndex - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (restrictedEquipment.length ())) {
         return false;
      }
      return restrictedEquipment[index] != '0';
   }
}

bool Bot::canReplaceWeapon () {
   // this function determines currently owned primary weapon, and checks if bot has
   // enough money to buy more powerful weapon.

   auto tab = conf.getRawWeapons ();

   // if bot is not rich enough or non-standard weapon mode enabled return false
   if (tab[25].teamStandard != 1 || m_moneyAmount < 4000) {
      return false;
   }

   if (m_currentWeapon == Weapon::Scout && m_moneyAmount > 5000) {
      return true;
   }
   else if (m_currentWeapon == Weapon::MP5 && m_moneyAmount > 6000) {
      return true;
   }
   else if (usesShotgun () && m_moneyAmount > 4000) {
      return true;
   }
   return isWeaponRestricted (m_currentWeapon);
}

int Bot::pickBestWeapon (int *vec, int count, int moneySave) {
   // this function picks best available weapon from random choice with money save

   bool needMoreRandomWeapon = (m_personality == Personality::Careful) || (rg.chance (25) && m_personality == Personality::Normal);

   if (needMoreRandomWeapon) {
      auto pick = [] (const float factor) -> float {
         union {
            unsigned int u;
            float f;
         } cast {};
         cast.f = factor;

         return (static_cast <int> ((cast.u >> 23) & 0xff) - 127) * 0.3010299956639812f;
      };

      float buyFactor = (m_moneyAmount - static_cast <float> (moneySave)) / (16000.0f - static_cast <float> (moneySave)) * 3.0f;

      if (buyFactor < 1.0f) {
         buyFactor = 1.0f;
      }

      // swap array values
      for (int *begin = vec, *end = vec + count - 1; begin < end; ++begin, --end) {
         cr::swap (*end, *begin);
      }
      return vec[static_cast <int> (static_cast <float> (count - 1) * pick (rg.get (1.0f, cr::powf (10.0f, buyFactor))) / buyFactor + 0.5f)];
   }

   int chance = 95;

   // high skilled bots almost always prefer best weapon
   if (m_difficulty < Difficulty::Expert) {
      if (m_personality == Personality::Normal) {
         chance = 50;
      }
      else if (m_personality == Personality::Careful) {
         chance = 75;
      }
   }
   auto &info = conf.getWeapons ();

   for (int i = 0; i < count; ++i) {
      auto &weapon = info[vec[i]];

      // if wea have enough money for weapon buy it
      if (weapon.price + moneySave < m_moneyAmount + rg.get (50, 200) && rg.chance (chance)) {
         return vec[i];
      }
   }
   return vec[rg.get (0, count - 1)];
}

void Bot::buyStuff () {
   // this function does all the work in selecting correct buy menus for most weapons/items

   WeaponInfo *selectedWeapon = nullptr;
   m_nextBuyTime = game.time ();

   if (!m_ignoreBuyDelay) {
      m_nextBuyTime += rg.get (0.3f, 0.5f);
   }

   int count = 0, weaponCount = 0;
   int choices[kNumWeapons];

   // select the priority tab for this personality
   const int *pref = conf.getWeaponPrefs (m_personality) + kNumWeapons;
   auto tab = conf.getRawWeapons ();

   const bool isPistolMode = tab[25].teamStandard == -1 && tab[3].teamStandard == 2;
   const bool teamHasGoodEconomics = bots.checkTeamEco (m_team);

   // do this, because xash engine is not capable to run all the features goldsrc, but we have cs 1.6 on it, so buy table must be the same
   const bool isOldGame = game.is (GameFlags::Legacy) && !game.is (GameFlags::Xash3D);

   const bool hasDefaultPistols = (pev->weapons & (cr::bit (Weapon::USP) | cr::bit (Weapon::Glock18)));
   const bool isFirstRound = m_moneyAmount == mp_startmoney.int_ ();

   switch (m_buyState) {
   case BuyState::PrimaryWeapon: // if no primary weapon and bot has some money, buy a primary weapon
      if ((!hasShield () && !hasPrimaryWeapon () && teamHasGoodEconomics) || (teamHasGoodEconomics && canReplaceWeapon ())) {
         int moneySave = 0;

         do {
            bool ignoreWeapon = false;

            pref--;

            assert (*pref > -1);
            assert (*pref < kNumWeapons);

            selectedWeapon = &tab[*pref];
            ++count;

            if (selectedWeapon->buyGroup == 1) {
               continue;
            }

            // weapon available for every team?
            if (game.mapIs (MapFlags::Assassination) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team) {
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

            const int *limit = conf.getEconLimit ();
            int prostock = 0;

            // filter out weapons with bot economics
            switch (m_personality) {
            case Personality::Rusher:
               prostock = limit[EcoLimit::ProstockRusher];
               break;

            case Personality::Careful:
               prostock = limit[EcoLimit::ProstockCareful];
               break;

            case Personality::Normal:
            default:
               prostock = limit[EcoLimit::ProstockNormal];
               break;
            }

            if (m_team == Team::CT) {
               switch (selectedWeapon->id) {
               case Weapon::TMP:
               case Weapon::UMP45:
               case Weapon::P90:
               case Weapon::MP5:
                  if (m_moneyAmount > limit[EcoLimit::SmgCTGreater] + prostock) {
                     ignoreWeapon = true;
                  }
                  break;
               }

               if (selectedWeapon->id == Weapon::Shield && m_moneyAmount > limit[EcoLimit::ShieldGreater]) {
                  ignoreWeapon = true;
               }
            }
            else if (m_team == Team::Terrorist) {
               switch (selectedWeapon->id) {
               case Weapon::UMP45:
               case Weapon::MAC10:
               case Weapon::P90:
               case Weapon::MP5:
               case Weapon::Scout:
                  if (m_moneyAmount > limit[EcoLimit::SmgTEGreater] + prostock) {
                     ignoreWeapon = true;
                  }
                  break;
               }
            }

            switch (selectedWeapon->id) {
            case Weapon::XM1014:
            case Weapon::M3:
               if (m_moneyAmount < limit[EcoLimit::ShotgunLess]) {
                  ignoreWeapon = true;
               }

               if (m_moneyAmount >= limit[EcoLimit::ShotgunGreater]) {
                  ignoreWeapon = false;

               }
               break;
            }

            switch (selectedWeapon->id) {
            case Weapon::SG550:
            case Weapon::G3SG1:
            case Weapon::AWP:
            case Weapon::M249:
               if (m_moneyAmount < limit[EcoLimit::HeavyLess]) {
                  ignoreWeapon = true;

               }

               if (m_moneyAmount >= limit[EcoLimit::HeavyGreater]) {
                  ignoreWeapon = false;
               }
               break;
            }

            if (ignoreWeapon && tab[25].teamStandard == 1 && cv_economics_rounds.bool_ ()) {
               continue;
            }

            // save money for grenade for example?
            moneySave = rg.get (500, 1000);

            if (bots.getLastWinner () == m_team) {
               moneySave = 0;
            }

            if (selectedWeapon->price <= (m_moneyAmount - moneySave)) {
               choices[weaponCount++] = *pref;
            }

         } while (count < kNumWeapons && weaponCount < 4);

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
            selectedWeapon = &tab[chosenWeapon];
         }
         else {
            selectedWeapon = nullptr;
         }

         if (selectedWeapon != nullptr) {
            issueCommand ("buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame) {
               issueCommand ("menuselect %d", selectedWeapon->buySelect);
            }
            else {
               if (m_team == Team::Terrorist) {
                  issueCommand ("menuselect %d", selectedWeapon->buySelectT);
               }
               else {
                  issueCommand ("menuselect %d", selectedWeapon->buySelectCT);
               }
            }
         }
      }
      else if (hasPrimaryWeapon () && !hasShield ()) {
         m_reloadState = Reload::Primary;
         break;
      }
      else if ((hasSecondaryWeapon () && !hasShield ()) || hasShield ()) {
         m_reloadState = Reload::Secondary;
         break;
      }
      break;

   case BuyState::ArmorVestHelm: // if armor is damaged and bot has some money, buy some armor
      if (pev->armorvalue < rg.get (50, 80) && teamHasGoodEconomics && (isPistolMode || (teamHasGoodEconomics && hasPrimaryWeapon ()))) {
         // if bot is rich, buy kevlar + helmet, else buy a single kevlar
         if (m_moneyAmount > 1500 && !isWeaponRestricted (Weapon::ArmorHelm)) {
            issueCommand ("buyequip;menuselect 2");
         }
         else if (!isWeaponRestricted (Weapon::Armor)) {
            issueCommand ("buyequip;menuselect 1");
         }
      }
      break;

   case BuyState::SecondaryWeapon: // if bot has still some money, buy a better secondary weapon
      if (isPistolMode || (isFirstRound && hasDefaultPistols && rg.chance (50)) || (hasDefaultPistols && bots.getLastWinner () == m_team && m_moneyAmount > rg.get (2000, 3000)) || (hasPrimaryWeapon () && hasDefaultPistols && m_moneyAmount > rg.get (7500, 9000))) {
         do {
            pref--;

            assert (*pref > -1);
            assert (*pref < kNumWeapons);

            selectedWeapon = &tab[*pref];
            ++count;

            if (selectedWeapon->buyGroup != 1) {
               continue;
            }

            // ignore weapon if this weapon is restricted
            if (isWeaponRestricted (selectedWeapon->id)) {
               continue;
            }

            // weapon available for every team?
            if (game.mapIs (MapFlags::Assassination) && selectedWeapon->teamAS != 2 && selectedWeapon->teamAS != m_team) {
               continue;
            }

            if (isOldGame && selectedWeapon->buySelect == -1) {
               continue;
            }

            if (selectedWeapon->teamStandard != 2 && selectedWeapon->teamStandard != m_team) {
               continue;
            }

            if (selectedWeapon->price <= (m_moneyAmount - rg.get (100, 200))) {
               choices[weaponCount++] = *pref;
            }

         } while (count < kNumWeapons && weaponCount < 4);

         // found a desired weapon?
         if (weaponCount > 0) {
            int chosenWeapon;

            // choose randomly from the best ones...
            if (weaponCount > 1) {
               chosenWeapon = pickBestWeapon (choices, weaponCount, rg.get (100, 200));
            }
            else {
               chosenWeapon = choices[weaponCount - 1];
            }
            selectedWeapon = &tab[chosenWeapon];
         }
         else {
            selectedWeapon = nullptr;
         }

         if (selectedWeapon != nullptr) {
            issueCommand ("buy;menuselect %d", selectedWeapon->buyGroup);

            if (isOldGame) {
               issueCommand ("menuselect %d", selectedWeapon->buySelect);
            }
            else {
               if (m_team == Team::Terrorist) {
                  issueCommand ("menuselect %d", selectedWeapon->buySelectT);
               }
               else {
                  issueCommand ("menuselect %d", selectedWeapon->buySelectCT);
               }
            }
         }
      }
      break;


   case BuyState::Ammo: // buy enough primary & secondary ammo (do not check for money here)
      for (int i = 0; i <= 5; ++i) {
         issueCommand ("buyammo%d", rg.get (1, 2)); // simulate human
      }

      // buy enough secondary ammo
      if (hasPrimaryWeapon ()) {
         issueCommand ("buy;menuselect 7");
      }

      // buy enough primary ammo
      issueCommand ("buy;menuselect 6");

      // try to reload secondary weapon
      if (m_reloadState != Reload::Primary) {
         m_reloadState = Reload::Secondary;
      }
      m_ignoreBuyDelay = false;
      break;

   case BuyState::Grenades: // if bot has still some money, choose if bot should buy a grenade or not

      if (teamHasGoodEconomics) {
         // buy a he grenade
         if (conf.chanceToBuyGrenade (0) && m_moneyAmount >= 400 && !isWeaponRestricted (Weapon::Explosive)) {
            issueCommand ("buyequip");
            issueCommand ("menuselect 4");
         }

         // buy a concussion grenade, i.e., 'flashbang'
         if (conf.chanceToBuyGrenade (1) && m_moneyAmount >= 300 && !isWeaponRestricted (Weapon::Flashbang)) {
            issueCommand ("buyequip");
            issueCommand ("menuselect 3");
         }

         // buy a smoke grenade
         if (conf.chanceToBuyGrenade (2) && m_moneyAmount >= 400 && !isWeaponRestricted (Weapon::Smoke)) {
            issueCommand ("buyequip");
            issueCommand ("menuselect 5");
         }
      }
      break;

   case BuyState::DefusalKit: // if bot is CT and we're on a bomb map, randomly buy the defuse kit
      if (game.mapIs (MapFlags::Demolition) && m_team == Team::CT && rg.chance (80) && m_moneyAmount > 200 && !isWeaponRestricted (Weapon::Defuser)) {
         if (isOldGame) {
            issueCommand ("buyequip;menuselect 6");
         }
         else {
            issueCommand ("defuser"); // use alias in steamcs
         }
      }
      break;

   case BuyState::NightVision:
      if (teamHasGoodEconomics && m_moneyAmount > 2500 && !m_hasNVG && rg.chance (30) && m_path) {
         float skyColor = illum.getSkyColor ();
         float lightLevel = m_path->light;

         // if it's somewhat darkm do buy nightvision goggles
         if ((skyColor >= 50.0f && lightLevel <= 15.0f) || (skyColor < 50.0f && lightLevel < 40.0f)) {
            if (isOldGame) {
               issueCommand ("buyequip;menuselect 7");
            }
            else {
               issueCommand ("nvgs"); // use alias in steamcs
            }
         }
      }
      break;
   }

   ++m_buyState;
   pushMsgQueue (BotMsg::Buy);
}

void Bot::updateEmotions () {
   // slowly increase/decrease dynamic emotions back to their base level
   if (m_nextEmotionUpdate > game.time ()) {
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
   m_nextEmotionUpdate = game.time () + 1.0f;
}

void Bot::overrideConditions () {

   if (!usesKnife () && m_difficulty > Difficulty::Normal && ((m_aimFlags & AimFlags::Enemy) || (m_states & Sense::SeeingEnemy)) && !cv_jasonmode.bool_ () && getCurrentTaskId () != Task::Camp && getCurrentTaskId () != Task::SeekCover && !isOnLadder ()) {
      m_moveToGoal = false; // don't move to goal
      m_navTimeset = game.time ();

      if (util.isPlayer (m_enemy) || (cv_attack_monsters.bool_ () && util.isMonster (m_enemy))) {
         attackMovement ();
      }
   }

   // check if we need to escape from bomb
   if (game.mapIs (MapFlags::Demolition) && bots.isBombPlanted () && m_notKilled && getCurrentTaskId () != Task::EscapeFromBomb && getCurrentTaskId () != Task::Camp && isOutOfBombTimer ()) {
      completeTask (); // complete current task

      // then start escape from bomb immediate
      startTask (Task::EscapeFromBomb, TaskPri::EscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
   }

   // special handling, if we have a knife in our hands
   if ((bots.getRoundStartTime () + 6.0f > game.time () || !hasAnyWeapons ()) && usesKnife () && (util.isPlayer (m_enemy) || (cv_attack_monsters.bool_ () && util.isMonster (m_enemy)))) {
      float length = (pev->origin - m_enemy->v.origin).length2d ();

      // do waypoint movement if enemy is not reachable with a knife
      if (length > 100.0f && (m_states & Sense::SeeingEnemy)) {
         int nearestToEnemyPoint = graph.getNearest (m_enemy->v.origin);

         if (nearestToEnemyPoint != kInvalidNodeIndex && nearestToEnemyPoint != m_currentNodeIndex && cr::abs (graph[nearestToEnemyPoint].origin.z - m_enemy->v.origin.z) < 16.0f) {
            float taskTime = game.time () + length / pev->maxspeed * 0.5f;

            if (getCurrentTaskId () != Task::MoveToPosition && !cr::fequal (getTask ()->desire, TaskPri::Hide)) {
               startTask (Task::MoveToPosition, TaskPri::Hide, nearestToEnemyPoint, taskTime, true);
            }
            m_isEnemyReachable = false;
            m_enemy = nullptr;

            m_enemyIgnoreTimer = taskTime;
         }
      }
   }

   // special handling for sniping
   if (usesSniper () && (m_states & (Sense::SeeingEnemy | Sense::SuspectEnemy)) && m_sniperStopTime > game.time () && getCurrentTaskId () != Task::SeekCover) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_navTimeset = game.time ();
   }

   // special handling for reloading
   if (m_reloadState != Reload::None && m_isReloading && ((pev->button | m_oldButtons) & IN_RELOAD))  {
      if (m_seeEnemyTime + 4.0f < game.time () && (m_states & Sense::SuspectEnemy)) {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         m_navTimeset = game.time ();
      }
   }
}

void Bot::setConditions () {
   // this function carried out each frame. does all of the sensing, calculates emotions and finally sets the desired
   // action after applying all of the Filters

   m_aimFlags = 0;
   updateEmotions ();

   // does bot see an enemy?
   trackEnemies ();

   // did bot just kill an enemy?
   if (!game.isNullEntity (m_lastVictim)) {
      if (game.getTeam (m_lastVictim) != m_team) {
         // add some aggression because we just killed somebody
         m_agressionLevel += 0.1f;

         if (m_agressionLevel > 1.0f) {
            m_agressionLevel = 1.0f;
         }

         if (rg.chance (10)) {
            pushChatMessage (Chat::Kill);
         }

         if (rg.chance (10)) {
            pushRadioMessage (Radio::EnemyDown);
         }
         else if (rg.chance (60)) {
            if ((m_lastVictim->v.weapons & cr::bit (Weapon::AWP)) || (m_lastVictim->v.weapons & cr::bit (Weapon::Scout)) || (m_lastVictim->v.weapons & cr::bit (Weapon::G3SG1)) || (m_lastVictim->v.weapons & cr::bit (Weapon::SG550))) {
               pushChatterMessage (Chatter::SniperKilled);
            }
            else {
               switch (numEnemiesNear (pev->origin, kInfiniteDistance)) {
               case 0:
                  if (rg.chance (50)) {
                     pushChatterMessage (Chatter::NoEnemiesLeft);
                  }
                  else {
                     pushChatterMessage (Chatter::EnemyDown);
                  }
                  break;

               case 1:
                  pushChatterMessage (Chatter::OneEnemyLeft);
                  break;

               case 2:
                  pushChatterMessage (Chatter::TwoEnemiesLeft);
                  break;

               case 3:
                  pushChatterMessage (Chatter::ThreeEnemiesLeft);
                  break;

               default:
                  pushChatterMessage (Chatter::EnemyDown);
               }
            }
         }

         // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
         if (m_team == Team::CT && !usesKnife () && m_numEnemiesLeft == 0 && bots.isBombPlanted ()) {
            selectWeaponByName ("weapon_knife");
            m_plantedBombNodeIndex = getNearestToPlantedBomb ();

            if (isOccupiedNode (m_plantedBombNodeIndex)) {
               pushChatterMessage (Chatter::BombsiteSecured);
            }
         }
      }
      else {
         pushChatMessage (Chat::TeamKill, true);
         pushChatterMessage (Chatter::FriendlyFire);
      }
      m_lastVictim = nullptr;
   }

   auto clearLastEnemy = [&] () {
      m_lastEnemyOrigin = nullptr;
      m_lastEnemy = nullptr;
   };

   // check if our current enemy is still valid
   if (!game.isNullEntity (m_lastEnemy)) {
      if (!util.isAlive (m_lastEnemy) && m_shootAtDeadTime < game.time ()) {
         clearLastEnemy ();
      }
   }
   else {
      clearLastEnemy ();
   }

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (!cv_ignore_enemies.bool_ () && m_soundUpdateTime < game.time () && m_blindTime < game.time () && m_seeEnemyTime + 1.0f < game.time ()) {
      updateHearing ();
      m_soundUpdateTime = game.time () + 0.25f;
   }
   else if (m_heardSoundTime < game.time ()) {
      m_states &= ~Sense::HearingEnemy;
   }

   if (game.isNullEntity (m_enemy) && !game.isNullEntity (m_lastEnemy) && !m_lastEnemyOrigin.empty ()) {
      m_aimFlags |= AimFlags::PredictPath;

      if (seesEntity (m_lastEnemyOrigin, true)) {
         m_aimFlags |= AimFlags::LastEnemy;
      }
   }

   // check for grenades depending on difficulty
   if (rg.chance (cr::max (25, m_difficulty * 25))) {
      checkGrenadesThrow ();
   }

   // check if there are items needing to be used/collected
   if (m_itemCheckTime < game.time () || !game.isNullEntity (m_pickupItem)) {
      updatePickups ();
      m_itemCheckTime = game.time () + 0.5f;
   }
   filterTasks ();
}

void Bot::filterTasks () {
   // initialize & calculate the desire for all actions based on distances, emotions and other stuff
   getTask ();

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
   auto &filter = bots.getFilters ();

   // bot found some item to use?
   if (!game.isNullEntity (m_pickupItem) && getCurrentTaskId () != Task::EscapeFromBomb) {
      m_states |= Sense::PickupItem;

      if (m_pickupType == Pickup::Button) {
         filter[Task::PickupItem].desire = 50.0f; // always pickup button
      }
      else {
         float distance = (500.0f - (game.getEntityWorldOrigin (m_pickupItem) - pev->origin).length ()) * 0.2f;

         if (distance > 50.0f) {
            distance = 50.0f;
         }
         filter[Task::PickupItem].desire = distance;
      }
   }
   else {
      m_states &= ~Sense::PickupItem;
      filter[Task::PickupItem].desire = 0.0f;
   }

   // calculate desire to attack
   if ((m_states & Sense::SeeingEnemy) && reactOnEnemy ()) {
      filter[Task::Attack].desire = TaskPri::Attack;
   }
   else {
      filter[Task::Attack].desire = 0.0f;
   }
   float &seekCoverDesire = filter[Task::SeekCover].desire;
   float &huntEnemyDesire = filter[Task::Hunt].desire;
   float &blindedDesire = filter[Task::Blind].desire;

   // calculate desires to seek cover or hunt
   if (util.isPlayer (m_lastEnemy) && !m_lastEnemyOrigin.empty () && !m_hasC4) {
      float retreatLevel = (100.0f - (m_healthValue > 50.0f ? 100.0f : m_healthValue)) * tempFear; // retreat level depends on bot health

      if (m_numEnemiesLeft > m_numFriendsLeft / 2 && m_retreatTime < game.time () && m_seeEnemyTime - rg.get (2.0f, 4.0f) < game.time ()) {

         float timeSeen = m_seeEnemyTime - game.time ();
         float timeHeard = m_heardSoundTime - game.time ();
         float ratio = 0.0f;

         m_retreatTime = game.time () + rg.get (3.0f, 15.0f);

         if (timeSeen > timeHeard) {
            timeSeen += 10.0f;
            ratio = timeSeen * 0.1f;
         }
         else {
            timeHeard += 10.0f;
            ratio = timeHeard * 0.1f;
         }
         bool lowAmmo = m_ammoInClip[m_currentWeapon] < conf.findWeaponById (m_currentWeapon).maxClip * 0.18f;
         bool sniping = m_sniperStopTime <= game.time () && lowAmmo;

         if (bots.isBombPlanted () || m_isStuck || usesKnife ()) {
            ratio /= 3.0f; // reduce the seek cover desire if bomb is planted
         }
         else if (m_isVIP || m_isReloading || (sniping && usesSniper ())) {
            ratio *= 3.0f; // triple the seek cover desire if bot is VIP or reloading
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
      if (getCurrentTaskId () != Task::EscapeFromBomb && game.isNullEntity (m_enemy) && bots.getRoundMidTime () < game.time () && !m_isUsingGrenade && m_currentNodeIndex != graph.getNearest (m_lastEnemyOrigin) && m_personality != Personality::Careful && !cv_ignore_enemies.bool_ ()) {
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
   blindedDesire = m_blindTime > game.time () ? TaskPri::Blind : 0.0f;

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
   auto maxDesire = [] (BotTask *first, BotTask *second) {
      if (first->desire > second->desire) {
         return first;
      }
      return second;
   };

   // this function returns the first behavior if its activation level is anything higher than zero
   auto subsumeDesire = [] (BotTask *first, BotTask *second) {
      if (first->desire > 0) {
         return first;
      }
      return second;
   };

   // this function returns the input behavior if it's activation level exceeds the threshold, or some default behavior otherwise
   auto thresholdDesire = [] (BotTask *first, float threshold, float desire) {
      if (first->desire < threshold) {
         first->desire = desire;
      }
      return first;
   };

   // this function clamp the inputs to be the last known value outside the [min, max] range.
   auto hysteresisDesire = [] (float cur, float min, float max, float old) {
      if (cur <= min || cur >= max) {
         old = cur;
      }
      return old;
   };

   m_oldCombatDesire = hysteresisDesire (filter[Task::Attack].desire, 40.0f, 90.0f, m_oldCombatDesire);
   filter[Task::Attack].desire = m_oldCombatDesire;

   auto offensive = &filter[Task::Attack];
   auto pickup = &filter[Task::PickupItem];

   // calc survive (cover/hide)
   auto survive = thresholdDesire (&filter[Task::SeekCover], 40.0f, 0.0f);
   survive = subsumeDesire (&filter[Task::Hide], survive);

   auto def = thresholdDesire (&filter[Task::Hunt], 41.0f, 0.0f); // don't allow hunting if desires 60<
   offensive = subsumeDesire (offensive, pickup); // if offensive task, don't allow picking up stuff

   auto sub = maxDesire (offensive, def); // default normal & careful tasks against offensive actions
   auto final = subsumeDesire (&filter[Task::Blind], maxDesire (survive, sub)); // reason about fleeing instead

   if (!m_tasks.empty ()) {
      final = maxDesire (final, getTask ());
      startTask (final->id, final->desire, final->data, final->time, final->resume); // push the final behavior in our task stack to carry out
   }
}

void Bot::clearTasks () {
   // this function resets bot tasks stack, by removing all entries from the stack.

   m_tasks.clear ();
}

void Bot::startTask (Task id, float desire, int data, float time, bool resume) {
   static const auto &filter = bots.getFilters ();

   for (auto &task : m_tasks) {
      if (task.id == id) {
         if (!cr::fequal (task.desire, desire)) {
            task.desire = desire;
         }
         return;
      }
   }
   m_tasks.emplace (filter[id].func, id, desire, data, time, resume);

   clearSearchNodes ();
   ignoreCollision ();

   int tid = getCurrentTaskId ();

   // leader bot?
   if (m_isLeader && tid == Task::SeekCover) {
      updateTeamCommands (); // reorganize team if fleeing
   }

   if (tid == Task::Camp) {
      selectBestWeapon ();
   }

   // this is best place to handle some voice commands report team some info
   if (cv_radio_mode.int_ () > 1) {
      if (rg.chance (90)) {
         if (tid == Task::Blind) {
            pushChatterMessage (Chatter::Blind);
         }
         else if (tid == Task::PlantBomb) {
            pushChatterMessage (Chatter::PlantingBomb);
         }
      }

      if (rg.chance (25) && tid == Task::Camp) {
         if (game.mapIs (MapFlags::Demolition) && bots.isBombPlanted ()) {
            pushChatterMessage (Chatter::GuardingDroppedC4);
         }
         else {
            pushChatterMessage (Chatter::GoingToCamp);
         }
      }

      if (rg.chance (75) && tid == Task::Camp && m_team == Team::Terrorist && m_inVIPZone) {
         pushChatterMessage (Chatter::GoingToGuardVIPSafety);
      }
   }

   if (cv_debug_goal.int_ () != kInvalidNodeIndex) {
      m_chosenGoalIndex = cv_debug_goal.int_ ();
   }
   else {
      m_chosenGoalIndex = getTask ()->data;
   }
}

BotTask *Bot::getTask () {
   if (m_tasks.empty ()) {
      startTask (Task::Normal, TaskPri::Normal, kInvalidNodeIndex, 0.0f, true);
   }
   return &m_tasks.last ();
}

void Bot::clearTask (Task id) {
   // this function removes one task from the bot task stack.

   if (m_tasks.empty () || getCurrentTaskId () == Task::Normal) {
      return; // since normal task can be only once on the stack, don't remove it...
   }

   if (getCurrentTaskId () == id) {
      clearSearchNodes ();
      ignoreCollision ();

      m_tasks.pop ();
      return;
   }

   for (auto &task : m_tasks) {
      if (task.id == id) {
         m_tasks.remove (task);
      }
   }
   ignoreCollision ();
   clearSearchNodes ();
}

void Bot::completeTask () {
   // this function called whenever a task is completed.

   ignoreCollision ();

   if (m_tasks.empty ()) {
      return;
   }

   do {
      m_tasks.pop ();
   } while (!m_tasks.empty () && !m_tasks.last ().resume);

   clearSearchNodes ();
}

bool Bot::isEnemyThreat () {
   if (game.isNullEntity (m_enemy) || getCurrentTaskId () == Task::SeekCover) {
      return false;
   }

   // if bot is camping, he should be firing anyway and not leaving his position
   if (getCurrentTaskId () == Task::Camp) {
      return false;
   }

   // if enemy is near or facing us directly
   if ((m_enemy->v.origin - pev->origin).lengthSq () < cr::square (256.0f) || isInViewCone (m_enemy->v.origin)) {
      return true;
   }
   return false;
}

bool Bot::reactOnEnemy () {
   // the purpose of this function is check if task has to be interrupted because an enemy is near (run attack actions then)

   if (!isEnemyThreat ()) {
      return false;
   }

   if (m_enemyReachableTimer < game.time ()) {
      int ownIndex = m_currentNodeIndex;

      if (ownIndex == kInvalidNodeIndex) {
         ownIndex = findNearestNode ();
      }
      int enemyIndex = graph.getNearest (m_enemy->v.origin);

      auto lineDist = (m_enemy->v.origin - pev->origin).length ();
      auto pathDist = static_cast <float> (graph.getPathDist (ownIndex, enemyIndex));

      if (pathDist - lineDist > 112.0f || isOnLadder ()) {
         m_isEnemyReachable = false;
      }
      else {
         m_isEnemyReachable = true;
      }
      m_enemyReachableTimer = game.time () + 1.0f;
   }

   if (m_isEnemyReachable) {
      m_navTimeset = game.time (); // override existing movement by attack movement
      return true;
   }
   return false;
}

bool Bot::lastEnemyShootable () {
   // don't allow shooting through walls
   if (!(m_aimFlags & AimFlags::LastEnemy) || m_lastEnemyOrigin.empty () || game.isNullEntity (m_lastEnemy)) {
      return false;
   }
   return util.getShootingCone (ent (), m_lastEnemyOrigin) >= 0.90f && isPenetrableObstacle (m_lastEnemyOrigin);
}

void Bot::checkRadioQueue () {
   // this function handling radio and reacting to it


   // don't allow bot listen you if bot is busy
   if (getCurrentTaskId () == Task::DefuseBomb || getCurrentTaskId () == Task::PlantBomb || hasHostage () || m_hasC4) {
      m_radioOrder = 0;
      return;
   }
   float distance = (m_radioEntity->v.origin - pev->origin).length ();

   switch (m_radioOrder) {
   case Radio::CoverMe:
   case Radio::FollowMe:
   case Radio::StickTogetherTeam:
   case Chatter::GoingToPlantBomb:
   case Chatter::CoverMe:
      // check if line of sight to object is not blocked (i.e. visible)
      if (seesEntity (m_radioEntity->v.origin) || m_radioOrder == Radio::StickTogetherTeam) {
         if (game.isNullEntity (m_targetEntity) && game.isNullEntity (m_enemy) && rg.chance (m_personality == Personality::Careful ? 80 : 20)) {
            int numFollowers = 0;

            // check if no more followers are allowed
            for (const auto &bot : bots) {
               if (bot->m_notKilled) {
                  if (bot->m_targetEntity == m_radioEntity) {
                     ++numFollowers;
                  }
               }
            }
            int allowedFollowers = cv_user_max_followers.int_ ();

            if (m_radioEntity->v.weapons & cr::bit (Weapon::C4)) {
               allowedFollowers = 1;
            }

            if (numFollowers < allowedFollowers) {
               pushRadioMessage (Radio::RogerThat);
               m_targetEntity = m_radioEntity;

               // don't pause/camp/follow anymore
               Task taskID = getCurrentTaskId ();

               if (taskID == Task::Pause || taskID == Task::Camp) {
                  getTask ()->time = game.time ();
               }
               startTask (Task::FollowUser, TaskPri::FollowUser, kInvalidNodeIndex, 0.0f, true);
            }
            else if (numFollowers > allowedFollowers) {
               for (int i = 0; (i < game.maxClients () && numFollowers > allowedFollowers); ++i) {
                  auto bot = bots[i];

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
            else if (m_radioOrder != Chatter::GoingToPlantBomb && rg.chance (25)) {
               pushRadioMessage (Radio::Negative);
            }
         }
         else if (m_radioOrder != Chatter::GoingToPlantBomb && rg.chance (35)) {
            pushRadioMessage (Radio::Negative);
         }
      }
      break;

   case Radio::HoldThisPosition:
      if (!game.isNullEntity (m_targetEntity)) {
         if (m_targetEntity == m_radioEntity) {
            m_targetEntity = nullptr;
            pushRadioMessage (Radio::RogerThat);

            m_campButtons = 0;
            startTask (Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.time () + rg.get (30.0f, 60.0f), false);
         }
      }
      break;

   case Chatter::NewRound:
      pushChatterMessage (Chatter::YouHeardTheMan);
      break;

   case Radio::TakingFireNeedAssistance:
      if (game.isNullEntity (m_targetEntity)) {
         if (game.isNullEntity (m_enemy) && m_seeEnemyTime + 4.0f < game.time ()) {
            // decrease fear levels to lower probability of bot seeking cover again
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f) {
               m_fearLevel = 0.0f;
            }

            if (rg.chance (45) && cv_radio_mode.int_ () == 2) {
               pushChatterMessage (Chatter::OnMyWay);
            }
            else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.int_ () != 2) {
               pushRadioMessage (Radio::RogerThat);
            }
            tryHeadTowardRadioMessage ();
         }
         else if (rg.chance (25)) {
            pushRadioMessage (Radio::Negative);
         }
      }
      break;

   case Radio::YouTakeThePoint:
      if (seesEntity (m_radioEntity->v.origin) && m_isLeader) {
         pushRadioMessage (Radio::RogerThat);
      }
      break;

   case Radio::EnemySpotted:
   case Radio::NeedBackup:
   case Chatter::ScaredEmotion:
   case Chatter::PinnedDown:
      if (((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 2048.0f || !m_moveToC4) && rg.chance (50) && m_seeEnemyTime + 4.0f < game.time ()) {
         m_fearLevel -= 0.1f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }

         if (rg.chance (45) && cv_radio_mode.int_ () == 2) {
            pushChatterMessage (Chatter::OnMyWay);
         }
         else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.int_ () != 2 && rg.chance (50)) {
            pushRadioMessage (Radio::RogerThat);
         }
         tryHeadTowardRadioMessage ();
      }
      else if (rg.chance (30) && m_radioOrder == Radio::NeedBackup) {
         pushRadioMessage (Radio::Negative);
      }
      break;

   case Radio::GoGoGo:
      if (m_radioEntity == m_targetEntity) {
         if (rg.chance (45) && cv_radio_mode.int_ () == 2) {
            pushRadioMessage (Radio::RogerThat);
         }
         else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.int_ () != 2) {
            pushRadioMessage (Radio::RogerThat);
         }

         m_targetEntity = nullptr;
         m_fearLevel -= 0.2f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }
      }
      else if ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 2048.0f) {
         Task taskID = getCurrentTaskId ();

         if (taskID == Task::Pause || taskID == Task::Camp) {
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f) {
               m_fearLevel = 0.0f;
            }

            pushRadioMessage (Radio::RogerThat);
            // don't pause/camp anymore
            getTask ()->time = game.time ();

            m_targetEntity = nullptr;
            m_position = m_radioEntity->v.origin + m_radioEntity->v.v_angle.forward () * rg.get (1024.0f, 2048.0f);

            clearSearchNodes ();
            startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);
         }
      }
      else if (!game.isNullEntity (m_doubleJumpEntity)) {
         pushRadioMessage (Radio::RogerThat);
         resetDoubleJump ();
      }
      else if (rg.chance (35)) {
         pushRadioMessage (Radio::Negative);
      }
      break;

   case Radio::ShesGonnaBlow:
      if (game.isNullEntity (m_enemy) && distance < 2048.0f && bots.isBombPlanted () && m_team == Team::Terrorist) {
         pushRadioMessage (Radio::RogerThat);

         if (getCurrentTaskId () == Task::Camp) {
            clearTask (Task::Camp);
         }
         m_targetEntity = nullptr;
         startTask (Task::EscapeFromBomb, TaskPri::EscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
      }
      else if (rg.chance (35)) {
         pushRadioMessage (Radio::Negative);
      }
      break;

   case Radio::RegroupTeam:
      // if no more enemies found AND bomb planted, switch to knife to get to bombplace faster
      if (m_team == Team::CT && !usesKnife () && m_numEnemiesLeft == 0 && bots.isBombPlanted () && getCurrentTaskId () != Task::DefuseBomb) {
         selectWeaponByName ("weapon_knife");

         clearSearchNodes ();

         m_position = graph.getBombOrigin ();
         startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);

         pushRadioMessage (Radio::RogerThat);
      }
      break;

   case Radio::StormTheFront:
      if (((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) && rg.chance (50)) {
         pushRadioMessage (Radio::RogerThat);

         // don't pause/camp anymore
         Task taskID = getCurrentTaskId ();

         if (taskID == Task::Pause || taskID == Task::Camp) {
            getTask ()->time = game.time ();
         }
         m_targetEntity = nullptr;
         m_position = m_radioEntity->v.origin + m_radioEntity->v.v_angle.forward () * rg.get (1024.0f, 2048.0f);

         clearSearchNodes ();
         startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);

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

   case Radio::TeamFallback:
      if ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) {
         m_fearLevel += 0.5f;

         if (m_fearLevel > 1.0f) {
            m_fearLevel = 1.0f;
         }
         m_agressionLevel -= 0.5f;

         if (m_agressionLevel < 0.0f) {
            m_agressionLevel = 0.0f;
         }
         if (getCurrentTaskId () == Task::Camp) {
            getTask ()->time += rg.get (10.0f, 15.0f);
         }
         else {
            // don't pause/camp anymore
            Task taskID = getCurrentTaskId ();

            if (taskID == Task::Pause) {
               getTask ()->time = game.time ();
            }
            m_targetEntity = nullptr;
            m_seeEnemyTime = game.time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistance = kInfiniteDistance;

               // take nearest enemy to ordering player
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team) {
                     continue;
                  }

                  auto enemy = client.ent;
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

   case Radio::ReportInTeam:
      switch (getCurrentTaskId ()) {
      case Task::Normal:
         if (getTask ()->data != kInvalidNodeIndex && rg.chance (70)) {
            const Path &path = graph[getTask ()->data];

            if (path.flags & NodeFlag::Goal) {
               if (game.mapIs (MapFlags::Demolition) && m_team == Team::Terrorist && m_hasC4) {
                  pushChatterMessage (Chatter::GoingToPlantBomb);
               }
               else {
                  pushChatterMessage (Chatter::Nothing);
               }
            }
            else if (path.flags & NodeFlag::Rescue) {
               pushChatterMessage (Chatter::RescuingHostages);
            }
            else if ((path.flags & NodeFlag::Camp) && rg.chance (75)) {
               pushChatterMessage (Chatter::GoingToCamp);
            }
            else {
               pushChatterMessage (Chatter::HeardNoise);
            }
         }
         else if (rg.chance (30)) {
            pushChatterMessage (Chatter::ReportingIn);
         }
         break;

      case Task::MoveToPosition:
         if (rg.chance (2)) {
            pushChatterMessage (Chatter::GoingToCamp);
         }
         break;

      case Task::Camp:
         if (rg.chance (40)) {
            if (bots.isBombPlanted () && m_team == Team::Terrorist) {
               pushChatterMessage (Chatter::GuardingDroppedC4);
            }
            else if (m_inVIPZone && m_team == Team::Terrorist) {
               pushChatterMessage (Chatter::GuardingVIPSafety);
            }
            else {
               pushChatterMessage (Chatter::Camping);
            }
         }
         break;

      case Task::PlantBomb:
         pushChatterMessage (Chatter::PlantingBomb);
         break;

      case Task::DefuseBomb:
         pushChatterMessage (Chatter::DefusingBomb);
         break;

      case Task::Attack:
         pushChatterMessage (Chatter::InCombat);
         break;

      case Task::Hide:
      case Task::SeekCover:
         pushChatterMessage (Chatter::SeekingEnemies);
         break;

      default:
         if (rg.chance (50)) {
            pushChatterMessage (Chatter::Nothing);
         }
         break;
      }
      break;

   case Radio::SectorClear:
      // is bomb planted and it's a ct
      if (!bots.isBombPlanted ()) {
         break;
      }

      // check if it's a ct command
      if (game.getTeam (m_radioEntity) == Team::CT && m_team == Team::CT && util.isFakeClient (m_radioEntity) && bots.getPlantedBombSearchTimestamp () < game.time ()) {
         float minDistance = kInfiniteDistance;
         int bombPoint = kInvalidNodeIndex;

         // find nearest bomb waypoint to player
         for (auto &point : graph.m_goalPoints) {
            distance = (graph[point].origin - m_radioEntity->v.origin).lengthSq ();

            if (distance < minDistance) {
               minDistance = distance;
               bombPoint = point;
            }
         }

         // mark this waypoint as restricted point
         if (bombPoint != kInvalidNodeIndex && !graph.isVisited (bombPoint)) {
            // does this bot want to defuse?
            if (getCurrentTaskId () == Task::Normal) {
               // is he approaching this goal?
               if (getTask ()->data == bombPoint) {
                  getTask ()->data = kInvalidNodeIndex;
                  pushRadioMessage (Radio::RogerThat);
               }
            }
            graph.setVisited (bombPoint);
         }
         bots.setPlantedBombSearchTimestamp (game.time () + 0.5f);
      }
      break;

   case Radio::GetInPositionAndWaitForGo:
      if ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distance < 1024.0f) {
         pushRadioMessage (Radio::RogerThat);

         if (getCurrentTaskId () == Task::Camp) {
            getTask ()->time = game.time () + rg.get (30.0f, 60.0f);
         }
         else {
            // don't pause anymore
            Task taskID = getCurrentTaskId ();

            if (taskID == Task::Pause) {
               getTask ()->time = game.time ();
            }

            m_targetEntity = nullptr;
            m_seeEnemyTime = game.time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistance = kInfiniteDistance;

               // take nearest enemy to ordering player
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team) {
                     continue;
                  }

                  auto enemy = client.ent;
                  float dist = (m_radioEntity->v.origin - enemy->v.origin).lengthSq ();

                  if (dist < nearestDistance) {
                     nearestDistance = dist;
                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            clearSearchNodes ();

            int index = findDefendNode (m_radioEntity->v.origin);

            // push camp task on to stack
            startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (30.0f, 60.0f), true);
            // push move command
            startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg.get (30.0f, 60.0f), true);

            if (graph[index].vis.crouch <= graph[index].vis.stand) {
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

void Bot::tryHeadTowardRadioMessage () {
   Task taskID = getCurrentTaskId ();

   if (taskID == Task::MoveToPosition || m_headedTime + 15.0f < game.time () || !util.isAlive (m_radioEntity) || m_hasC4) {
      return;
   }

   if ((util.isFakeClient (m_radioEntity) && rg.chance (25) && m_personality == Personality::Normal) || !(m_radioEntity->v.flags & FL_FAKECLIENT)) {
      if (taskID == Task::Pause || taskID == Task::Camp) {
         getTask ()->time = game.time ();
      }
      m_headedTime = game.time ();
      m_position = m_radioEntity->v.origin;

      clearSearchNodes ();
      startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);
   }
}

void Bot::updateAimDir () {
   uint32 flags = m_aimFlags;

   // don't allow bot to look at danger positions under certain circumstances
   if (!(flags & (AimFlags::Grenade | AimFlags::Enemy | AimFlags::Entity))) {

      // check if narrow place and we're duck, do not predict enemies in that situation
      const bool duckedInNarrowPlace = isInNarrowPlace () && ((m_pathFlags & NodeFlag::Crouch) || (pev->button & IN_DUCK));

      if (duckedInNarrowPlace || isOnLadder () || isInWater () || (m_pathFlags & NodeFlag::Ladder) || (m_currentTravelFlags & PathFlag::Jump)) {
         flags &= ~(AimFlags::LastEnemy | AimFlags::PredictPath);
         m_canChooseAimDirection = false;
      }
   }
   
   if (flags & AimFlags::Override) {
      m_lookAt = m_camp;
   }
   else if (flags & AimFlags::Grenade) {
      m_lookAt = m_throw;

      float throwDistance = (m_throw - pev->origin).length ();
      float coordCorrection = 0.0f;

      if (throwDistance > 100.0f && throwDistance < 800.0f) {
         coordCorrection = 0.25f * (m_throw.z - pev->origin.z);
      }
      else if (throwDistance >= 800.0f) {
         float angleCorrection = 37.0f * (throwDistance - 800.0f) / 800.0f;

         if (angleCorrection > 45.0f) {
            angleCorrection = 45.0f;
         }
         coordCorrection = throwDistance * cr::tanf (cr::deg2rad (angleCorrection)) + 0.25f * (m_throw.z - pev->origin.z);
      }
      m_lookAt.z += coordCorrection * 0.5f;
   }
   else if (flags & AimFlags::Enemy) {
      focusEnemy ();
   }
   else if (flags & AimFlags::Entity) {
      m_lookAt = m_entity;
   }
   else if (flags & AimFlags::LastEnemy) {
      m_lookAt = m_lastEnemyOrigin;

      // did bot just see enemy and is quite aggressive?
      if (m_seeEnemyTime + 1.0f - m_actualReactionTime + m_baseAgressionLevel > game.time ()) {

         // feel free to fire if shootable
         if (!usesSniper () && lastEnemyShootable ()) {
            m_wantsToFire = true;
         }
      }
   }
   else if (flags & AimFlags::PredictPath) {
      bool changePredictedEnemy = true;
  
      if (m_timeNextTracking > game.time () && m_trackingEdict == m_lastEnemy && util.isAlive (m_lastEnemy)) {
         changePredictedEnemy = false;
      }

      if (changePredictedEnemy) {
         auto aimPoint = findAimingNode (m_lastEnemyOrigin);

         if (aimPoint != kInvalidNodeIndex) {
            m_lookAt = graph[aimPoint].origin;
            m_camp = m_lookAt;

            m_timeNextTracking = game.time () + 0.5f;
            m_trackingEdict = m_lastEnemy;
         }
         else {
            m_aimFlags &= ~AimFlags::PredictPath;

            m_lookAt = m_destOrigin;
            m_timeNextTracking = game.time () + 1.5f;
            m_trackingEdict = nullptr;
         }
      }
      else {
         m_lookAt = m_camp;
      }
   }
   else if (flags & AimFlags::Camp) {
      m_lookAt = m_camp;
   }
   else if (flags & AimFlags::Nav) {
      auto smoothView = [&] (int32 index) -> Vector {
         auto radius = graph[index].radius;

         if (radius > 0.0f) {
            return Vector (pev->angles.x, cr::normalizeAngles (pev->angles.y + rg.get (-90.0f, 90.0f)), 0.0f).forward () * rg.get (2.0f, 4.0f);
         }
         return nullptr;
      };

      if (m_moveToGoal && !m_isStuck && m_moveSpeed > getShiftSpeed () && !(pev->button & IN_DUCK) && m_currentNodeIndex != kInvalidNodeIndex && !(m_path->flags & (NodeFlag::Ladder | NodeFlag::Crouch)) && m_pathWalk.hasNext () && (pev->origin - m_destOrigin).lengthSq () < cr::square (160.0f)) {
         auto nextPathIndex = m_pathWalk.next ();

         if (graph.isVisible (m_currentNodeIndex, nextPathIndex)) {
            m_lookAt = graph[nextPathIndex].origin + pev->view_ofs + smoothView (nextPathIndex);
         }
         else {
            m_lookAt = m_destOrigin;
         }
      }
      else {
         m_lookAt = m_destOrigin;
      }

      if (m_canChooseAimDirection && m_currentNodeIndex != kInvalidNodeIndex && !(m_path->flags & NodeFlag::Ladder)) {
         auto dangerIndex = graph.getDangerIndex (m_team, m_currentNodeIndex, m_currentNodeIndex);

         if (graph.exists (dangerIndex) && graph.isVisible (m_currentNodeIndex, dangerIndex) && !(graph[dangerIndex].flags & NodeFlag::Crouch)) {
            if ((graph[dangerIndex].origin - pev->origin).lengthSq () < cr::square (160.0f)) {
               m_lookAt = m_destOrigin;
            }
            else {
               m_lookAt = graph[dangerIndex].origin + pev->view_ofs + smoothView (dangerIndex);

               // add danger flags
               m_aimFlags |= AimFlags::Danger;
            }
         }
      }
   }

   if (m_lookAt.empty ()) {
      m_lookAt = m_destOrigin;
   }
}

void Bot::checkDarkness () {

   // do not check for darkness at the start of the round
   if (m_spawnTime + 5.0f > game.time () || !graph.exists (m_currentNodeIndex) || cr::fzero (m_path->light)) {
      return;
   }

   // do not check every frame
   if (m_checkDarkTime + 5.0f > game.time ()) {
      return;
   }
   auto skyColor = illum.getSkyColor ();

   if (mp_flashlight.bool_ () && !m_hasNVG) {
      auto task = Task ();

      if (!(pev->effects & EF_DIMLIGHT) && task != Task::Camp && task != Task::Attack && m_heardSoundTime + 3.0f < game.time () && m_flashLevel > 30.0f && ((skyColor > 50.0f && m_path->light < 10.0f) || (skyColor <= 50.0f && m_path->light < 40.0f))) {
         pev->impulse = 100;
      }
      else if ((pev->effects & EF_DIMLIGHT) && (((m_path->light > 15.0f && skyColor > 50.0f) || (m_path->light > 45.0f && skyColor <= 50.0f)) || task == Task::Camp || task == Task::Attack || m_flashLevel <= 0 || m_heardSoundTime + 3.0f >= game.time ())) {
         pev->impulse = 100;
      }
   }
   else if (m_hasNVG) {
      if (pev->effects & EF_DIMLIGHT) {
         pev->impulse = 100;
      }
      else if (!m_usesNVG && ((skyColor > 50.0f && m_path->light < 15.0f) || (skyColor <= 50.0f && m_path->light < 40.0f))) {
         issueCommand ("nightvision");
      }
      else if (m_usesNVG  && ((m_path->light > 20.0f && skyColor > 50.0f) || (m_path->light > 45.0f && skyColor <= 50.0f))) {
         issueCommand ("nightvision");
      }
   }
   m_checkDarkTime = game.time ();
}

void Bot::checkParachute () {
   static auto parachute = engfuncs.pfnCVarGetPointer ("sv_parachute");

   // if no cvar or it's not enabled do not bother
   if (parachute && parachute->value > 0.0f) {
      if (isOnLadder () || pev->velocity.z > -50.0f || isOnFloor ()) {
         m_fallDownTime = 0.0f;
      }
      else if (cr::fzero (m_fallDownTime)) {
         m_fallDownTime = game.time ();
      }

      // press use anyway
      if (!cr::fzero (m_fallDownTime) && m_fallDownTime + 0.35f < game.time ()) {
         pev->button |= IN_USE;
      }
   }
}

void Bot::frame () {
   pev->flags |= FL_FAKECLIENT; // restore fake client bit

   if (m_updateTime <= game.time ()) {
      update ();
   }
   else if (m_notKilled) {
      updateLookAngles ();
   }

   if (m_slowFrameTimestamp > game.time ()) {
      return;
   }
   m_numFriendsLeft = numFriendsNear (pev->origin, kInfiniteDistance);
   m_numEnemiesLeft = numEnemiesNear (pev->origin, kInfiniteDistance);

   if (bots.isBombPlanted () && m_team == Team::CT && m_notKilled) {
      const Vector &bombPosition = graph.getBombOrigin ();

      if (!m_hasProgressBar && getCurrentTaskId () != Task::EscapeFromBomb && (pev->origin - bombPosition).lengthSq () < cr::square (1540.0f) && !isBombDefusing (bombPosition)) {
         m_itemIgnore = nullptr;
         m_itemCheckTime = game.time ();

         clearTask (getCurrentTaskId ());
      }
   }

   checkSpawnConditions ();
   checkForChat ();
   checkBreakablesAround ();

   if (game.is (GameFlags::HasBotVoice)) {
      showChaterIcon (false); // end voice feedback
   }

   // clear enemy far away
   if (!m_lastEnemyOrigin.empty () && !game.isNullEntity (m_lastEnemy) && (pev->origin - m_lastEnemyOrigin).lengthSq () >= cr::square (1600.0f)) {
      m_lastEnemy = nullptr;
      m_lastEnemyOrigin = nullptr;
   }
   m_slowFrameTimestamp = game.time () + 0.5f;
}

void Bot::update () {
   pev->button = 0;

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;
   m_moveAngles = nullptr;

   m_canChooseAimDirection = true;
   m_notKilled = util.isAlive (ent ());
   m_team = game.getTeam (ent ());
   m_healthValue = cr::clamp (pev->health, 0.0f, 100.0f);

   if (game.mapIs (MapFlags::Assassination) && !m_isVIP) {
      m_isVIP = util.isPlayerVIP (ent ());
   }

   if (m_team == Team::Terrorist && game.mapIs (MapFlags::Demolition)) {
      m_hasC4 = !!(pev->weapons & cr::bit (Weapon::C4));
   }

   // is bot movement enabled
   bool botMovement = false;

   // if the bot hasn't selected stuff to start the game yet, go do that...
   if (m_notStarted) {
      updateTeamJoin (); // select team & class
   }
   else if (!m_notKilled) {
       // we got a teamkiller? vote him away...
      if (m_voteKickIndex != m_lastVoteKick && cv_tkpunish.bool_ ()) {
         issueCommand ("vote %d", m_voteKickIndex);
         m_lastVoteKick = m_voteKickIndex;

         // if bot tk punishment is enabled slay the tk
         if (cv_tkpunish.int_ () != 2 || util.isFakeClient (game.entityOfIndex (m_voteKickIndex))) {
            return;
         }
         auto killer = game.entityOfIndex (m_lastVoteKick);

         ++killer->v.frags;
         MDLL_ClientKill (killer);
      }

      // host wants us to kick someone
      else if (m_voteMap != 0) {
         issueCommand ("votemap %d", m_voteMap);
         m_voteMap = 0;
      }
   }
   else if (m_buyingFinished && !(pev->maxspeed < 10.0f && getCurrentTaskId () != Task::PlantBomb && getCurrentTaskId () != Task::DefuseBomb) && !cv_freeze_bots.bool_ () && !graph.hasChanged ()) {
      botMovement = true;
   }
   checkMsgQueue ();

   if (botMovement) {
      logic (); // execute main code
   }
   else if (pev->maxspeed < 10.0f) {
      choiceFreezetimeEntity ();
   }
   runMovement ();

   // delay next execution
   m_updateTime = game.time () + m_updateInterval;
}

void Bot::choiceFreezetimeEntity () {
   if (m_changeViewTime > game.time ()) {
      return;
   }

   if (rg.chance (15)) {
      pev->button |= IN_JUMP;
   }
   Array <Bot *> teammates;

   for (const auto &bot : bots) {
      if (bot->m_notKilled && bot->m_team == m_team && seesEntity (bot->pev->origin) && bot.get () != this) {
         teammates.push (bot.get ());
      }
   }

   if (!teammates.empty ()) {
      auto bot = teammates.random ();

      if (bot) {
         m_lookAt = bot->pev->origin + bot->pev->view_ofs;
      }
   }
   m_changeViewTime = game.time () + rg.get (1.25f, 2.0f);
}

void Bot::normal_ () {
   m_aimFlags |= AimFlags::Nav;

   int debugGoal = cv_debug_goal.int_ ();

   // user forced a waypoint as a goal?
   if (debugGoal != kInvalidNodeIndex && getTask ()->data != debugGoal) {
      clearSearchNodes ();
      getTask ()->data = debugGoal;
   }
   
   // stand still if reached debug goal
   else if (m_currentNodeIndex == debugGoal) {
      pev->button = 0;
      ignoreCollision ();

      m_moveSpeed = 0.0;
      m_strafeSpeed = 0.0f;

      return;
   }

   // bots rushing with knife, when have no enemy (thanks for idea to nicebot project)
   if (usesKnife () && (game.isNullEntity (m_lastEnemy) || !util.isAlive (m_lastEnemy)) && game.isNullEntity (m_enemy) && m_knifeAttackTime < game.time () && !hasShield () && numFriendsNear (pev->origin, 96.0f) == 0) {
      if (rg.chance (40)) {
         pev->button |= IN_ATTACK;
      }
      else {
         pev->button |= IN_ATTACK2;
      }
      m_knifeAttackTime = game.time () + rg.get (2.5f, 6.0f);
   }
   const auto &prop = conf.getWeaponProp (m_currentWeapon);

   if (m_reloadState == Reload::None && getAmmo () != 0 && getAmmoInClip () < 5 && prop.ammo1 != -1) {
      m_reloadState = Reload::Primary;
   }

   // if bomb planted and it's a CT calculate new path to bomb point if he's not already heading for
   if (!m_bombSearchOverridden && bots.isBombPlanted () && m_team == Team::CT && getTask ()->data != kInvalidNodeIndex && !(graph[getTask ()->data].flags & NodeFlag::Goal) && getCurrentTaskId () != Task::EscapeFromBomb) {
      clearSearchNodes ();
      getTask ()->data = kInvalidNodeIndex;
   }

   // reached the destination (goal) waypoint?
   if (updateNavigation ()) {

      // if we're reached the goal, and there is not enemies, notify the team
      if (!bots.isBombPlanted () && m_currentNodeIndex != kInvalidNodeIndex && (m_path->flags & NodeFlag::Goal) && rg.chance (15) && numEnemiesNear (pev->origin, 650.0f) == 0) {
         pushRadioMessage (Radio::SectorClear);
      }
      
      completeTask ();
      m_prevGoalIndex = kInvalidNodeIndex;
      
      // spray logo sometimes if allowed to do so
      if (!(m_states & (Sense::SeeingEnemy | Sense::SuspectEnemy)) && m_seeEnemyTime + 5.0f < game.time () && !m_reloadState && m_timeLogoSpray < game.time () && cv_spraypaints.bool_ () && rg.chance (50) && m_moveSpeed > getShiftSpeed () && game.isNullEntity (m_pickupItem)) {
         if (!(game.mapIs (MapFlags::Demolition) && bots.isBombPlanted () && m_team == Team::CT)) {
            startTask (Task::Spraypaint, TaskPri::Spraypaint, kInvalidNodeIndex, game.time () + 1.0f, false);
         }
      }

      // reached waypoint is a camp waypoint
      if ((m_path->flags & NodeFlag::Camp) && !game.is (GameFlags::CSDM) && cv_camping_allowed.bool_ ()) {

         // check if bot has got a primary weapon and hasn't camped before
         if (hasPrimaryWeapon () && m_timeCamping + 10.0f < game.time () && !hasHostage ()) {
            bool campingAllowed = true;

            // Check if it's not allowed for this team to camp here
            if (m_team == Team::Terrorist) {
               if (m_path->flags & NodeFlag::CTOnly) {
                  campingAllowed = false;
               }
            }
            else {
               if (m_path->flags & NodeFlag::TerroristOnly) {
                  campingAllowed = false;
               }
            }

            // don't allow vip on as_ maps to camp + don't allow terrorist carrying c4 to camp
            if (campingAllowed && (m_isVIP || (game.mapIs (MapFlags::Demolition) && m_team == Team::Terrorist && !bots.isBombPlanted () && m_hasC4))) {
               campingAllowed = false;
            }

            // check if another bot is already camping here
            if (campingAllowed && isOccupiedNode (m_currentNodeIndex)) {
               campingAllowed = false;
            }

            if (campingAllowed) {
               // crouched camping here?
               if (m_path->flags & NodeFlag::Crouch) {
                  m_campButtons = IN_DUCK;
               }
               else {
                  m_campButtons = 0;
               }
               selectBestWeapon ();

               if (!(m_states & (Sense::SeeingEnemy | Sense::HearingEnemy)) && !m_reloadState) {
                  m_reloadState = Reload::Primary;
               }
               m_timeCamping = game.time () + rg.get (10.0f, 25.0f);
               startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, m_timeCamping, true);

               m_camp = m_path->origin + m_path->start.forward () * 500.0f;
               m_aimFlags |= AimFlags::Camp;
               m_campDirection = 0;

               // tell the world we're camping
               if (rg.chance (40)) {
                  pushRadioMessage (Radio::ImInPosition);
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
         if (game.mapIs (MapFlags::HostageRescue)) {
            // CT Bot has some hostages following?
            if (m_team == Team::CT && hasHostage ()) {
               // and reached a Rescue Point?
               if (m_path->flags & NodeFlag::Rescue) {
                  m_hostages.clear ();
               }
            }
            else if (m_team == Team::Terrorist && rg.chance (75)) {
               int index = findDefendNode (m_path->origin);

               startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (60.0f, 120.0f), true); // push camp task on to stack
               startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg.get (5.0f, 10.0f), true); // push move command

               auto &path = graph[index];

               // decide to duck or not to duck
               if (path.vis.crouch <= path.vis.stand) {
                  m_campButtons |= IN_DUCK;
               }
               else {
                  m_campButtons &= ~IN_DUCK;
               }
               pushChatterMessage (Chatter::GoingToGuardVIPSafety); // play info about that
            }
         }
         else if (game.mapIs (MapFlags::Demolition) && ((m_path->flags & NodeFlag::Goal) || m_inBombZone)) {
            // is it a terrorist carrying the bomb?
            if (m_hasC4) {
               if ((m_states & Sense::SeeingEnemy) && numFriendsNear (pev->origin, 768.0f) == 0) {
                  // request an help also
                  pushRadioMessage (Radio::NeedBackup);
                  pushChatterMessage (Chatter::ScaredEmotion);

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (4.0f, 8.0f), true);
               }
               else {
                  startTask (Task::PlantBomb, TaskPri::PlantBomb, kInvalidNodeIndex, 0.0f, false);
               }
            }
            else if (m_team == Team::CT) {
               if (!bots.isBombPlanted () && numFriendsNear (pev->origin, 210.0f) < 4) {
                  int index = findDefendNode (m_path->origin);

                  float campTime = rg.get (25.0f, 40.f);

                  // rusher bots don't like to camp too much
                  if (m_personality == Personality::Rusher) {
                     campTime *= 0.5f;
                  }
                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + campTime, true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg.get (5.0f, 11.0f), true); // push move command

                  auto &path = graph[index];

                  // decide to duck or not to duck
                  if (path.vis.crouch <= path.vis.stand) {
                     m_campButtons |= IN_DUCK;
                  }
                  else {
                     m_campButtons &= ~IN_DUCK;
                  }
                  pushChatterMessage (Chatter::DefendingBombsite); // play info about that
               }
            }
         }
      }
   }
   // no more nodes to follow - search new ones (or we have a bomb)
   else if (!hasActiveGoal ()) {
      m_moveSpeed = pev->maxspeed;
      
      clearSearchNodes ();
      ignoreCollision ();

      // did we already decide about a goal before?
      auto destIndex = graph.exists (getTask ()->data) ? getTask ()->data : findBestGoal ();

      // check for existance (this is failover, for i.e. csdm, this should be not true with normal gameplay, only when spawned outside of waypointed area)
      if (!graph.exists (destIndex)) {
         destIndex = graph.getFarest (pev->origin, 512.0f);
      }

      m_prevGoalIndex = destIndex;

      // remember index
      getTask ()->data = destIndex;

      auto pathSearchType = m_pathType;

      // override with fast path
      if (game.mapIs (MapFlags::Demolition) && bots.isBombPlanted ()) {
         pathSearchType = FindPath::Fast;
      }

      // do pathfinding if it's not the current waypoint
      if (destIndex != m_currentNodeIndex) {
         findPath (m_currentNodeIndex, destIndex, pathSearchType);
      }
   }
   else {
      if (!(pev->flags & FL_DUCKING) && !cr::fequal (m_minSpeed, pev->maxspeed) && m_minSpeed > 1.0f) {
         m_moveSpeed = m_minSpeed;
      }
   }
   float shiftSpeed = getShiftSpeed ();

   if ((!cr::fzero (m_moveSpeed) && m_moveSpeed > shiftSpeed) && (cv_walking_allowed.bool_ () && mp_footsteps.bool_ ()) && m_difficulty > Difficulty::Normal && !(m_aimFlags & AimFlags::Enemy) && (m_heardSoundTime + 6.0f >= game.time () || (m_states & Sense::SuspectEnemy)) && numEnemiesNear (pev->origin, 768.0f) >= 1 && !cv_jasonmode.bool_ () && !bots.isBombPlanted ()) {
      m_moveSpeed = shiftSpeed;
   }

   // bot hasn't seen anything in a long time and is asking his teammates to report in
   if (cv_radio_mode.int_ () > 1 && bots.getLastRadio (m_team) != Radio::ReportInTeam && bots.getRoundStartTime () + 20.0f < game.time () && m_askCheckTime < game.time () && rg.chance (15) && m_seeEnemyTime + rg.get (45.0f, 80.0f) < game.time () && numFriendsNear (pev->origin, 1024.0f) == 0) {
      pushRadioMessage (Radio::ReportInTeam);

      m_askCheckTime = game.time () + rg.get (45.0f, 80.0f);

      // make sure everyone else will not ask next few moments
      for (const auto &bot : bots) {
         if (bot->m_notKilled) {
            bot->m_askCheckTime = game.time () + rg.get (5.0f, 30.0f);
         }
      }
   }
}

void Bot::spraypaint_ () {
   m_aimFlags |= AimFlags::Entity;

   // bot didn't spray this round?
   if (m_timeLogoSpray < game.time () && getTask ()->time > game.time ()) {
      const auto &forward = pev->v_angle.forward ();
      Vector sprayOrigin = getEyesPos () + forward * 128.0f;

      TraceResult tr {};
      game.testLine (getEyesPos (), sprayOrigin, TraceIgnore::Monsters, ent (), &tr);

      // no wall in front?
      if (tr.flFraction >= 1.0f) {
         sprayOrigin.z -= 128.0f;
      }
      m_entity = sprayOrigin;

      if (getTask ()->time - 0.5f < game.time ()) {
         // emit spraycan sound
         engfuncs.pfnEmitSound (ent (), CHAN_VOICE, "player/sprayer.wav", 1.0f, ATTN_NORM, 0, 100);
         game.testLine (getEyesPos (), getEyesPos () + forward * 128.0f, TraceIgnore::Monsters, ent (), &tr);

         // paint the actual logo decal
         util.traceDecals (pev, &tr, m_logotypeIndex);
         m_timeLogoSpray = game.time () + rg.get (60.0f, 90.0f);
      }
   }
   else {
      completeTask ();
   }
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = game.time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   ignoreCollision ();
}

void Bot::huntEnemy_ () {
   m_aimFlags |= AimFlags::Nav;
   
   // if we've got new enemy...
   if (!game.isNullEntity (m_enemy) || game.isNullEntity (m_lastEnemy)) {

      // forget about it...
      clearTask (Task::Hunt);
      m_prevGoalIndex = kInvalidNodeIndex;
   }
   else if (game.getTeam (m_lastEnemy) == m_team) {

      // don't hunt down our teammate...
      clearTask (Task::Hunt);

      m_prevGoalIndex = kInvalidNodeIndex;
      m_lastEnemy = nullptr;
   }
   else if (updateNavigation ()) // reached last enemy pos?
   {
      // forget about it...
      completeTask ();

      m_prevGoalIndex = kInvalidNodeIndex;
      m_lastEnemyOrigin = nullptr;
   }

   // do we need to calculate a new path?
   else if (!hasActiveGoal ())  {
      clearSearchNodes ();

      int destIndex = kInvalidNodeIndex;
      int goal = getTask ()->data;

      // is there a remembered index?
      if (graph.exists (goal)) {
         destIndex = goal;
      }

      // find new one instead
      else {
         destIndex = graph.getNearest (m_lastEnemyOrigin);
      }

      // remember index
      m_prevGoalIndex = destIndex;
      getTask ()->data = destIndex;

      if (destIndex != m_currentNodeIndex) {
         findPath (m_currentNodeIndex, destIndex, FindPath::Fast);
      }
   }

   // bots skill higher than 60?
   if (cv_walking_allowed.bool_ () && mp_footsteps.bool_ () && m_difficulty > Difficulty::Normal && !cv_jasonmode.bool_ ()) {
      // then make him move slow if near enemy
      if (!(m_currentTravelFlags & PathFlag::Jump)) {
         if (m_currentNodeIndex != kInvalidNodeIndex) {
            if (m_path->radius < 32.0f && !isOnLadder () && !isInWater () && m_seeEnemyTime + 4.0f > game.time () && m_difficulty < Difficulty::Hard) {
               pev->button |= IN_DUCK;
            }
         }

         if ((m_lastEnemyOrigin - pev->origin).lengthSq () < cr::square (512.0f)) {
            m_moveSpeed = getShiftSpeed ();
         }
      }
   }
}

void Bot::seekCover_ () {
   m_aimFlags |= AimFlags::Nav;

   if (!util.isAlive (m_lastEnemy)) {
      completeTask ();
      m_prevGoalIndex = kInvalidNodeIndex;
   }

   // reached final waypoint?
   else if (updateNavigation ()) {
      // yep. activate hide behaviour
      completeTask ();
      m_prevGoalIndex = kInvalidNodeIndex;

      // start hide task
      startTask (Task::Hide, TaskPri::Hide, kInvalidNodeIndex, game.time () + rg.get (3.0f, 12.0f), false);
      Vector dest = m_lastEnemyOrigin;

      // get a valid look direction
      getCampDirection (&dest);

      m_aimFlags |= AimFlags::Camp;
      m_camp = dest;
      m_campDirection = 0;

      // chosen waypoint is a camp waypoint?
      if (m_path->flags & NodeFlag::Camp) {
         // use the existing camp node prefs
         if (m_path->flags & NodeFlag::Crouch) {
            m_campButtons = IN_DUCK;
         }
         else {
            m_campButtons = 0;
         }
      }
      else {
         // choose a crouch or stand pos
         if (m_path->vis.crouch <= m_path->vis.stand) {
            m_campButtons = IN_DUCK;
         }
         else {
            m_campButtons = 0;
         }

         // enter look direction from previously calculated positions
         m_path->start = dest;
         m_path->end = dest;
      }

      if (m_reloadState == Reload::None && getAmmoInClip () < 5 && getAmmo () != 0) {
         m_reloadState = Reload::Primary;
      }
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_moveToGoal = false;
      m_checkTerrain = false;
   }
   else if (!hasActiveGoal ()) {
      clearSearchNodes ();
      int destIndex = kInvalidNodeIndex;

      if (getTask ()->data != kInvalidNodeIndex) {
         destIndex = getTask ()->data;
      }
      else {
         destIndex = findCoverNode (usesSniper () ? 256.0f : 512.0f);

         if (destIndex == kInvalidNodeIndex) {
            m_retreatTime = game.time () + rg.get (5.0f, 10.0f);
            m_prevGoalIndex = kInvalidNodeIndex;

            completeTask ();
            return;
         }
      }
      m_campDirection = 0;

      m_prevGoalIndex = destIndex;
      getTask ()->data = destIndex;

      if (destIndex != m_currentNodeIndex) {
         findPath (m_currentNodeIndex, destIndex, FindPath::Fast);
      }
   }
}

void Bot::attackEnemy_ () {
   m_moveToGoal = false;
   m_checkTerrain = false;

   if (!game.isNullEntity (m_enemy)) {
      ignoreCollision ();
      attackMovement ();

      if (usesKnife () && !m_lastEnemyOrigin.empty ()) {
         m_destOrigin = m_lastEnemyOrigin;
      }
   }
   else {
      completeTask ();
      m_destOrigin = m_lastEnemyOrigin;
   }
   m_navTimeset = game.time ();
}

void Bot::pause_ () {
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_navTimeset = game.time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   m_aimFlags |= AimFlags::Nav;

   // is bot blinded and above average difficulty?
   if (m_viewDistance < 500.0f && m_difficulty >= Difficulty::Normal) {
      // go mad!
      m_moveSpeed = -cr::abs ((m_viewDistance - 500.0f) * 0.5f);

      if (m_moveSpeed < -pev->maxspeed) {
         m_moveSpeed = -pev->maxspeed;
      }
      m_camp = getEyesPos () + pev->v_angle.forward () * 500.0f;

      m_aimFlags |= AimFlags::Override;
      m_wantsToFire = true;
   }
   else {
      pev->button |= m_campButtons;
   }

   // stop camping if time over or gets hurt by something else than bullets
   if (getTask ()->time < game.time () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::blind_ () {
   m_moveToGoal = false;
   m_checkTerrain = false;
   m_navTimeset = game.time ();

   // if bot remembers last enemy position
   if (m_difficulty >= Difficulty::Normal && !m_lastEnemyOrigin.empty () && util.isPlayer (m_lastEnemy) && !usesSniper ()) {
      m_lookAt = m_lastEnemyOrigin; // face last enemy
      m_wantsToFire = true; // and shoot it
   }

   m_moveSpeed = m_blindMoveSpeed;
   m_strafeSpeed = m_blindSidemoveSpeed;
   pev->button |= m_blindButton;

   if (m_blindTime < game.time ()) {
      completeTask ();
   }
}

void Bot::camp_ () {
   if (!cv_camping_allowed.bool_ ()) {
      completeTask ();
      return;
   }

   m_aimFlags |= AimFlags::Camp;
   m_checkTerrain = false;
   m_moveToGoal = false;

   if (m_team == Team::CT && bots.isBombPlanted () && m_defendedBomb && !isBombDefusing (graph.getBombOrigin ()) && !isOutOfBombTimer ()) {
      m_defendedBomb = false;
      completeTask ();
   }
   ignoreCollision ();

   // half the reaction time if camping because you're more aware of enemies if camping
   setIdealReactionTimers ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = game.time ();
   m_timeCamping = game.time ();

   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   findValidNode ();

   if (m_nextCampDirTime < game.time ()) {
      m_nextCampDirTime = game.time () + rg.get (2.0f, 5.0f);

      if (m_path->flags & NodeFlag::Camp) {
         Vector dest;

         // switch from 1 direction to the other
         if (m_campDirection < 1) {
            dest = m_path->start;
            m_campDirection ^= 1;
         }
         else {
            dest = m_path->end;
            m_campDirection ^= 1;
         }
         dest.z = 0.0f;

         // find a visible waypoint to this direction...
         // i know this is ugly hack, but i just don't want to break compatibility :)
         int numFoundPoints = 0;

         int campPoints[3] = { 0, };
         int distances[3] = { 0, };

         const Vector &dotA = (dest - pev->origin).normalize2d ();

         for (int i = 0; i < graph.length (); ++i) {
            // skip invisible waypoints or current waypoint
            if (!graph.isVisible (m_currentNodeIndex, i) || i == m_currentNodeIndex) {
               continue;
            }
            const Vector &dotB = (graph[i].origin - pev->origin).normalize2d ();

            if ((dotA | dotB) > 0.9f) {
               int distance = static_cast <int> ((pev->origin - graph[i].origin).length ());

               if (numFoundPoints >= 3) {
                  for (int j = 0; j < 3; ++j) {
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

                  ++numFoundPoints;
               }
            }
         }

         if (--numFoundPoints >= 0) {
            m_camp = graph[campPoints[rg.get (0, numFoundPoints)]].origin;
         }
         else {
            m_camp = graph[findCampingDirection ()].origin;
         }
      }
      else {
         m_camp = graph[findCampingDirection ()].origin;
      }
   }
   // press remembered crouch button
   pev->button |= m_campButtons;

   // stop camping if time over or gets hurt by something else than bullets
   if (getTask ()->time < game.time () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::hide_ () {
   m_aimFlags |= AimFlags::Camp;
   m_checkTerrain = false;
   m_moveToGoal = false;

   // half the reaction time if camping
   setIdealReactionTimers ();
   m_idealReactionTime *= 0.5f;

   m_navTimeset = game.time ();
   m_moveSpeed = 0.0f;
   m_strafeSpeed = 0.0f;

   findValidNode ();

   if (hasShield () && !m_isReloading) {
      if (!isShieldDrawn ()) {
         pev->button |= IN_ATTACK2; // draw the shield!
      }
      else {
         pev->button |= IN_DUCK; // duck under if the shield is already drawn
      }
   }

   // if we see an enemy and aren't at a good camping point leave the spot
   if ((m_states & Sense::SeeingEnemy) || m_inBombZone) {
      if (!(m_path->flags & NodeFlag::Camp)) {
         completeTask ();

         m_campButtons = 0;
         m_prevGoalIndex = kInvalidNodeIndex;

         if (!game.isNullEntity (m_enemy)) {
            attackMovement ();
         }
         return;
      }
   }

   // if we don't have an enemy we're also free to leave
   else if (m_lastEnemyOrigin.empty ()) {
      completeTask ();

      m_campButtons = 0;
      m_prevGoalIndex = kInvalidNodeIndex;

      if (getCurrentTaskId () == Task::Hide) {
         completeTask ();
      }
      return;
   }

   pev->button |= m_campButtons;
   m_navTimeset = game.time ();

   if (!m_isReloading) {
      checkReload ();
   }

   // stop camping if time over or gets hurt by something else than bullets
   if (getTask ()->time < game.time () || m_lastDamageType > 0) {
      completeTask ();
   }
}

void Bot::moveToPos_ () {
   m_aimFlags |= AimFlags::Nav;

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   // reached destination?
   if (updateNavigation ()) {
      completeTask (); // we're done

      m_prevGoalIndex = kInvalidNodeIndex;
      m_position = nullptr;
   }

   // didn't choose goal waypoint yet?
   else if (!hasActiveGoal ()) {
      clearSearchNodes ();

      int destIndex = kInvalidNodeIndex;
      int goal = getTask ()->data;

      if (graph.exists (goal)) {
         destIndex = goal;
      }
      else {
         destIndex = graph.getNearest (m_position);
      }

      if (graph.exists (destIndex)) {
         m_prevGoalIndex = destIndex;
         getTask ()->data = destIndex;

         findPath (m_currentNodeIndex, destIndex, m_pathType);
      }
      else {
         completeTask ();
      }
   }
}

void Bot::plantBomb_ () {
   m_aimFlags |= AimFlags::Camp;

   // we're still got the C4?
   if (m_hasC4) {
      if (m_currentWeapon != Weapon::C4) {
         selectWeaponByName ("weapon_c4");
      }

      if (util.isAlive (m_enemy) || !m_inBombZone) {
         completeTask ();
      }
      else {
         m_moveToGoal = false;
         m_checkTerrain = false;
         m_navTimeset = game.time ();

         if (m_path->flags & NodeFlag::Crouch) {
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
         pushRadioMessage (Radio::NeedBackup);
      }
      auto index = findDefendNode (pev->origin);

      float guardTime = mp_c4timer.float_ () * 0.5f + mp_c4timer.float_ () * 0.25f;

      // push camp task on to stack
      startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + guardTime, true);

      // push move command
      startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + guardTime, true);

      if (graph[index].vis.crouch <= graph[index].vis.stand) {
         m_campButtons |= IN_DUCK;
      }
      else {
         m_campButtons &= ~IN_DUCK;
      }
   }
}

void Bot::defuseBomb_ () {
   float fullDefuseTime = m_hasDefuser ? 7.0f : 12.0f;
   float timeToBlowUp = getBombTimeleft ();
   float defuseRemainingTime = fullDefuseTime;

   if (m_hasProgressBar /*&& isOnFloor ()*/) {
      defuseRemainingTime = fullDefuseTime - game.time ();
   }

   const Vector &bombPos = graph.getBombOrigin ();
   bool defuseError = false;

   // exception: bomb has been defused
   if (bombPos.empty ()) {
      // fix for stupid behaviour of CT's when bot is defused
      for (const auto &bot : bots) {
         if (bot->m_team == m_team && bot->m_notKilled) {
            auto defendPoint = graph.getFarest (bot->pev->origin);

            startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg.get (30.0f, 60.0f), true); // push camp task on to stack
            startTask (Task::MoveToPosition, TaskPri::MoveToPosition, defendPoint, game.time () + rg.get (3.0f, 6.0f), true); // push move command
         }
      }
      graph.setBombOrigin (true);

      if (m_numFriendsLeft != 0 && rg.chance (50)) {
         if (timeToBlowUp <= 3.0) {
            if (cv_radio_mode.int_ () == 2) {
               pushChatterMessage (Chatter::BarelyDefused);
            }
            else if (cv_radio_mode.int_ () == 1) {
               pushRadioMessage (Radio::SectorClear);
            }
         }
         else {
            pushRadioMessage (Radio::SectorClear);
         }
      }
      return;
   }
   else if (defuseRemainingTime > timeToBlowUp) {
      defuseError = true;
   }
   else if (m_states & Sense::SeeingEnemy) {
      int friends = numFriendsNear (pev->origin, 768.0f);

      if (friends < 2 && defuseRemainingTime < timeToBlowUp) {
         defuseError = true;

         if (defuseRemainingTime + 2.0f > timeToBlowUp) {
            defuseError = false;
         }

         if (m_numFriendsLeft > friends) {
            pushRadioMessage (Radio::NeedBackup);
         }
      }
   }

   // one of exceptions is thrown. finish task.
   if (defuseError) {
      m_entity = nullptr;

      m_pickupItem = nullptr;
      m_pickupType = Pickup::None;

      selectBestWeapon ();
      resetCollision ();

      completeTask ();

      return;
   }

   // to revert from pause after reload  ting && just to be sure
   m_moveToGoal = false;
   m_checkTerrain = false;

   m_moveSpeed = pev->maxspeed;
   m_strafeSpeed = 0.0f;

   // bot is reloading and we close enough to start defusing
   if (m_isReloading && (bombPos - pev->origin).length2d () < 80.0f) {
      if (m_numEnemiesLeft == 0 || timeToBlowUp < fullDefuseTime + 7.0f || ((getAmmoInClip () > 8 && m_reloadState == Reload::Primary) || (getAmmoInClip () > 5 && m_reloadState == Reload::Secondary))) {
         int weaponIndex = bestWeaponCarried ();

         // just select knife and then select weapon
         selectWeaponByName ("weapon_knife");

         if (weaponIndex > 0 && weaponIndex < kNumWeapons) {
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
   m_aimFlags |= AimFlags::Entity;

   m_destOrigin = bombPos;
   m_entity = bombPos;

   pev->button |= IN_USE;

   // if defusing is not already started, maybe crouch before
   if (!m_hasProgressBar && m_duckDefuseCheckTime < game.time ()) {
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
            m_duckDefuse = m_difficulty >= Difficulty::Normal && m_numEnemiesLeft != 0; // duck
         }
      }
      m_duckDefuseCheckTime = game.time () + 5.0f;
   }

   // press duck button
   if (m_duckDefuse || (m_oldButtons & IN_DUCK)) {
      pev->button |= IN_DUCK;
   }
   else {
      pev->button &= ~IN_DUCK;
   }

   // we are defusing bomb
   if (m_hasProgressBar || (m_oldButtons & IN_USE) || !game.isNullEntity (m_pickupItem)) {
      pev->button |= IN_USE;

      if (!game.isNullEntity (m_pickupItem)) {
         MDLL_Use (m_pickupItem, ent ());
      }

      m_reloadState = Reload::None;
      m_navTimeset = game.time ();

      // don't move when defusing
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      // notify team
      if (m_numFriendsLeft != 0) {
         pushChatterMessage (Chatter::DefusingBomb);

         if (numFriendsNear (pev->origin, 512.0f) < 2) {
            pushRadioMessage (Radio::NeedBackup);
         }
      }
   }
   else {
      completeTask ();
   }
}

void Bot::followUser_ () {
   if (game.isNullEntity (m_targetEntity) || !util.isAlive (m_targetEntity)) {
      m_targetEntity = nullptr;
      completeTask ();

      return;
   }

   if (m_targetEntity->v.button & IN_ATTACK) {
      TraceResult tr {};
      game.testLine (m_targetEntity->v.origin + m_targetEntity->v.view_ofs, m_targetEntity->v.v_angle.forward () * 500.0f, TraceIgnore::Everything, ent (), &tr);

      if (!game.isNullEntity (tr.pHit) && util.isPlayer (tr.pHit) && game.getTeam (tr.pHit) != m_team) {
         m_targetEntity = nullptr;
         m_lastEnemy = tr.pHit;
         m_lastEnemyOrigin = tr.pHit->v.origin;

         completeTask ();
         return;
      }
   }

   if (!cr::fzero (m_targetEntity->v.maxspeed) && m_targetEntity->v.maxspeed < pev->maxspeed) {
      m_moveSpeed = m_targetEntity->v.maxspeed;
   }

   if (m_reloadState == Reload::None && getAmmo () != 0) {
      m_reloadState = Reload::Primary;
   }

   if ((m_targetEntity->v.origin - pev->origin).lengthSq () > cr::square (130.0f)) {
      m_followWaitTime = 0.0f;
   }
   else {
      m_moveSpeed = 0.0f;

      if (cr::fzero (m_followWaitTime)) {
         m_followWaitTime = game.time ();
      }
      else {
         if (m_followWaitTime + 3.0f < game.time ()) {
            // stop following if we have been waiting too long
            m_targetEntity = nullptr;

            pushRadioMessage (Radio::YouTakeThePoint);
            completeTask ();

            return;
         }
      }
   }
   m_aimFlags |= AimFlags::Nav;

   if (cv_walking_allowed.bool_ () && m_targetEntity->v.maxspeed < m_moveSpeed && !cv_jasonmode.bool_ ()) {
      m_moveSpeed = getShiftSpeed ();
   }

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   // reached destination?
   if (updateNavigation ()) {
      getTask ()->data = kInvalidNodeIndex;
   }

   // didn't choose goal waypoint yet?
   if (!hasActiveGoal ()) {
      clearSearchNodes ();

      int destIndex = graph.getNearest (m_targetEntity->v.origin);
      auto points = graph.searchRadius (200.0f, m_targetEntity->v.origin);

      for (auto &newIndex : points) {
         // if waypoint not yet used, assign it as dest
         if (newIndex != m_currentNodeIndex && !isOccupiedNode (newIndex)) {
            destIndex = newIndex;
         }
      }

      if (graph.exists (destIndex) && graph.exists (m_currentNodeIndex)) {
         m_prevGoalIndex = destIndex;
         getTask ()->data = destIndex;

         // always take the shortest path
         findPath (m_currentNodeIndex, destIndex, FindPath::Fast);
      }
      else {
         m_targetEntity = nullptr;
         completeTask ();
      }
   }
}

void Bot::throwExplosive_ () {
   Vector dest = m_throw;

   if (!(m_states & Sense::SeeingEnemy)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }
   else if (!(m_states & Sense::SuspectEnemy) && !game.isNullEntity (m_enemy)) {
      dest = m_enemy->v.origin + m_enemy->v.velocity.get2d () * 0.55f;
   }
   m_isUsingGrenade = true;
   m_checkTerrain = false;

   ignoreCollision ();

   if ((pev->origin - dest).lengthSq () < cr::square (400.0f)) {
      // heck, I don't wanna blow up myself
      m_grenadeCheckTime = game.time () + kGrenadeCheckTime;

      selectBestWeapon ();
      completeTask ();

      return;
   }
   m_grenade = calcThrow (getEyesPos (), dest);

   if (m_grenade.lengthSq () < 100.0f) {
      m_grenade = calcToss (pev->origin, dest);
   }

   if (m_grenade.lengthSq () <= 100.0f) {
      m_grenadeCheckTime = game.time () + kGrenadeCheckTime;

      selectBestWeapon ();
      completeTask ();
   }
   else {
      m_aimFlags |= AimFlags::Grenade;

      auto grenade = correctGrenadeVelocity ("hegrenade.mdl");

      if (game.isNullEntity (grenade)) {
         if (m_currentWeapon != Weapon::Explosive && !m_grenadeRequested) {
            if (pev->weapons & cr::bit (Weapon::Explosive)) {
               m_grenadeRequested = true;
               selectWeaponByName ("weapon_hegrenade");
            }
            else {
               m_grenadeRequested = false;

               selectBestWeapon ();
               completeTask ();

               return;
            }
         }
         else if (!(m_oldButtons & IN_ATTACK)) {
            pev->button |= IN_ATTACK;
            m_grenadeRequested = false;
         }
      }
   }
   pev->button |= m_campButtons;
}

void Bot::throwFlashbang_ () {
   Vector dest = m_throw;

   if (!(m_states & Sense::SeeingEnemy)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }
   else if (!(m_states & Sense::SuspectEnemy) && !game.isNullEntity (m_enemy)) {
      dest = m_enemy->v.origin + m_enemy->v.velocity.get2d () * 0.55f;
   }

   m_isUsingGrenade = true;
   m_checkTerrain = false;

   ignoreCollision ();

   if ((pev->origin - dest).lengthSq () < cr::square (400.0f)) {
      m_grenadeCheckTime = game.time () + kGrenadeCheckTime; // heck, I don't wanna blow up myself

      selectBestWeapon ();
      completeTask ();

      return;
   }
   m_grenade = calcThrow (getEyesPos (), dest);

   if (m_grenade.lengthSq () < 100.0f) {
      m_grenade = calcToss (pev->origin, dest);
   }

   if (m_grenade.lengthSq () <= 100.0f) {
      m_grenadeCheckTime = game.time () + kGrenadeCheckTime;

      selectBestWeapon ();
      completeTask ();
   }
   else {
      m_aimFlags |= AimFlags::Grenade;

      auto grenade = correctGrenadeVelocity ("flashbang.mdl");

      if (game.isNullEntity (grenade)) {
         if (m_currentWeapon != Weapon::Flashbang  && !m_grenadeRequested) {
            if (pev->weapons & cr::bit (Weapon::Flashbang)) {
               m_grenadeRequested = true;
               selectWeaponByName ("weapon_flashbang");
            }
            else {
               m_grenadeRequested = false;

               selectBestWeapon ();
               completeTask ();

               return;
            }
         }
         else if (!(m_oldButtons & IN_ATTACK)) {
            pev->button |= IN_ATTACK;
            m_grenadeRequested = false;
         }
      }
   }
   pev->button |= m_campButtons;
}

void Bot::throwSmoke_ () {
   if (!(m_states & Sense::SeeingEnemy)) {
      m_strafeSpeed = 0.0f;
      m_moveSpeed = 0.0f;
      m_moveToGoal = false;
   }

   m_checkTerrain = false;
   m_isUsingGrenade = true;

   ignoreCollision ();

   Vector src = m_lastEnemyOrigin - pev->velocity;

   // predict where the enemy is in 0.5 secs
   if (!game.isNullEntity (m_enemy)) {
      src = src + m_enemy->v.velocity * 0.5f;
   }

   m_grenade = (src - getEyesPos ()).normalize ();

   if (getTask ()->time < game.time ()) {
      completeTask ();
      return;
   }

   if (m_currentWeapon != Weapon::Smoke && !m_grenadeRequested) {
      m_aimFlags |= AimFlags::Grenade;

      if (pev->weapons & cr::bit (Weapon::Smoke)) {
         m_grenadeRequested = true;

         selectWeaponByName ("weapon_smokegrenade");
         getTask ()->time = game.time () + 1.2f;
      }
      else {
         m_grenadeRequested = false;

         selectBestWeapon ();
         completeTask ();

         return;
      }
   }
   else if (!(m_oldButtons & IN_ATTACK)) {
      pev->button |= IN_ATTACK;
      m_grenadeRequested = false;
   }
   pev->button |= m_campButtons;
}

void Bot::doublejump_ () {
   if (!util.isAlive (m_doubleJumpEntity) || (m_aimFlags & AimFlags::Enemy) || (m_travelStartIndex != kInvalidNodeIndex && getTask ()->time + (graph.calculateTravelTime (pev->maxspeed, graph[m_travelStartIndex].origin, m_doubleJumpOrigin) + 11.0f) < game.time ())) {
      resetDoubleJump ();
      return;
   }
   m_aimFlags |= AimFlags::Nav;

   if (m_jumpReady) {
      m_moveToGoal = false;
      m_checkTerrain = false;

      m_navTimeset = game.time ();
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      bool inJump = (m_doubleJumpEntity->v.button & IN_JUMP) || (m_doubleJumpEntity->v.oldbuttons & IN_JUMP);

      if (m_duckForJump < game.time ()) {
         pev->button |= IN_DUCK;
      }
      else if (inJump && !(m_oldButtons & IN_JUMP)) {
         pev->button |= IN_JUMP;
      }

      const auto &src = pev->origin + Vector (0.0f, 0.0f, 45.0f);
      const auto &dest = src + Vector (0.0f, pev->angles.y, 0.0f).upward () * 256.0f;

      TraceResult tr {};
      game.testLine (src, dest, TraceIgnore::None, ent (), &tr);

      if (tr.flFraction < 1.0f && tr.pHit == m_doubleJumpEntity && inJump) {
         m_duckForJump = game.time () + rg.get (3.0f, 5.0f);
         getTask ()->time = game.time ();
      }
      return;
   }

   if (m_currentNodeIndex == m_prevGoalIndex) {
      m_pathOrigin = m_doubleJumpOrigin;
      m_destOrigin = m_doubleJumpOrigin;
   }

   if (updateNavigation ()) {
      getTask ()->data = kInvalidNodeIndex;
   }

   // didn't choose goal waypoint yet?
   if (!hasActiveGoal ())  {
      clearSearchNodes ();

      int destIndex = graph.getNearest (m_doubleJumpOrigin);

      if (graph.exists (destIndex)) {
         m_prevGoalIndex = destIndex;
         m_travelStartIndex = m_currentNodeIndex;

         getTask ()->data = destIndex;
         
         // always take the shortest path
         findPath (m_currentNodeIndex, destIndex, FindPath::Fast);

         if (m_currentNodeIndex == destIndex) {
            m_jumpReady = true;
         }
      }
      else {
         resetDoubleJump ();
      }
   }
}

void Bot::escapeFromBomb_ () {
   m_aimFlags |= AimFlags::Nav;

   if (!bots.isBombPlanted ()) {
      completeTask ();
   }

   if (isShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
   }

   if (!usesKnife () && m_numEnemiesLeft == 0) {
      selectWeaponByName ("weapon_knife");
   }

   // reached destination?
   if (updateNavigation ()) {
      completeTask (); // we're done

      // press duck button if we still have some enemies
      if (numEnemiesNear (pev->origin, 2048.0f)) {
         m_campButtons = IN_DUCK;
      }

      // we're reached destination point so just sit down and camp
      startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + 10.0f, true);
   }

   // didn't choose goal waypoint yet?
   else if (!hasActiveGoal ()) {
      clearSearchNodes ();

      int lastSelectedGoal = kInvalidNodeIndex, minPathDistance = kInfiniteDistanceLong;
      float safeRadius = rg.get (1513.0f, 2048.0f);

      for (int i = 0; i < graph.length (); ++i) {
         if ((graph[i].origin - graph.getBombOrigin ()).length () < safeRadius || isOccupiedNode (i)) {
            continue;
         }
         int pathDistance = graph.getPathDist (m_currentNodeIndex, i);

         if (minPathDistance > pathDistance) {
            minPathDistance = pathDistance;
            lastSelectedGoal = i;
         }
      }

      if (lastSelectedGoal < 0) {
         lastSelectedGoal = graph.getFarest (pev->origin, safeRadius);
      }

      // still no luck?
      if (lastSelectedGoal < 0) {
         completeTask (); // we're done

         // we have no destination point, so just sit down and camp
         startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + 10.0f, true);
         return;
      }
      m_prevGoalIndex = lastSelectedGoal;
      getTask ()->data = lastSelectedGoal;

      findPath (m_currentNodeIndex, lastSelectedGoal, FindPath::Fast);
   }
}

void Bot::shootBreakable_ () {
   m_aimFlags |= AimFlags::Override;

   // breakable destroyed?
   if (!game.isShootableBreakable (m_breakableEntity)) {
      completeTask ();
      return;
   }
   pev->button |= m_campButtons;

   m_checkTerrain = false;
   m_moveToGoal = false;
   m_navTimeset = game.time ();
   m_camp = m_breakableOrigin;

   // is bot facing the breakable?
   if (util.getShootingCone (ent (), m_breakableOrigin) >= 0.90f) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      if (usesKnife ()) {
         selectBestWeapon ();
      }
      m_wantsToFire = true;
      m_shootTime = game.time ();
   }
   else {
      m_checkTerrain = true;
      m_moveToGoal = true;

      completeTask ();
   }
}

void Bot::pickupItem_ () {
   if (game.isNullEntity (m_pickupItem)) {
      m_pickupItem = nullptr;
      completeTask ();

      return;
   }
   const Vector &dest = game.getEntityWorldOrigin (m_pickupItem);

   m_destOrigin = dest;
   m_entity = dest;

   // find the distance to the item
   float itemDistance = (dest - pev->origin).length ();

   switch (m_pickupType) {
   case Pickup::DroppedC4:
   case Pickup::None:
      break;

   case Pickup::Weapon:
      m_aimFlags |= AimFlags::Nav;

      // near to weapon?
      if (itemDistance < 50.0f) {
         int index = 0;
         auto &info = conf.getWeapons ();

         for (index = 0; index < 7; ++index) {
            if (strcmp (info[index].model, m_pickupItem->v.model.chars (9)) == 0) {
               break;
            }
         }

         if (index < 7) {
            // secondary weapon. i.e., pistol
            int wid = 0;

            for (index = 0; index < 7; ++index) {
               if (pev->weapons & cr::bit (info[index].id)) {
                  wid = index;
               }
            }

            if (wid > 0) {
               selectWeaponById (wid);
               issueCommand ("drop");

               if (hasShield ()) {
                  issueCommand ("drop"); // discard both shield and pistol
               }
            }
            enteredBuyZone (BuyState::PrimaryWeapon);
         }
         else {
            // primary weapon
            int wid = bestWeaponCarried ();
            
            if (wid == Weapon::Shield || wid > 6 || hasShield ()) {
               selectWeaponById (wid);
               issueCommand ("drop");
            }

            if (!wid) {
               m_itemIgnore = m_pickupItem;
               m_pickupItem = nullptr;
               m_pickupType = Pickup::None;

               break;
            }
            enteredBuyZone (BuyState::PrimaryWeapon);
         }
         checkSilencer (); // check the silencer
      }
      break;

   case Pickup::Shield:
      m_aimFlags |= AimFlags::Nav;

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
            issueCommand ("drop");
         }
      }
      break;

   case Pickup::PlantedC4:
      m_aimFlags |= AimFlags::Entity;

      if (m_team == Team::CT && itemDistance < 80.0f) {
         pushChatterMessage (Chatter::DefusingBomb);

         // notify team of defusing
         if (m_numFriendsLeft < 3) {
            pushRadioMessage (Radio::NeedBackup);
         }
         m_moveToGoal = false;
         m_checkTerrain = false;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;

         startTask (Task::DefuseBomb, TaskPri::DefuseBomb, kInvalidNodeIndex, 0.0f, false);
      }
      break;

   case Pickup::Hostage:
      m_aimFlags |= AimFlags::Entity;

      if (!util.isAlive (m_pickupItem)) {
         // don't pickup dead hostages
         m_pickupItem = nullptr;
         completeTask ();

         break;
      }

      if (itemDistance < 50.0f) {
         float angleToEntity = isInFOV (dest - getEyesPos ());

         // bot faces hostage?
         if (angleToEntity <= 10.0f) {
            // use game dll function to make sure the hostage is correctly 'used'
            MDLL_Use (m_pickupItem, ent ());

            if (rg.chance (80)) {
               pushChatterMessage (Chatter::UsingHostages);
            }
            m_hostages.push (m_pickupItem);
            m_pickupItem = nullptr;
         }
         ignoreCollision (); // also don't consider being stuck
      }
      break;

   case Pickup::DefusalKit:
      m_aimFlags |= AimFlags::Nav;

      if (m_hasDefuser) {
         m_pickupItem = nullptr;
         m_pickupType = Pickup::None;
      }
      break;

   case Pickup::Button:
      m_aimFlags |= AimFlags::Entity;

      if (game.isNullEntity (m_pickupItem) || m_buttonPushTime < game.time ()) {
         completeTask ();
         m_pickupType = Pickup::None;

         break;
      }

      // find angles from bot origin to entity...
      float angleToEntity = isInFOV (dest - getEyesPos ());

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
            m_pickupType = Pickup::None;
            m_buttonPushTime = game.time () + 3.0f;

            completeTask ();
         }
      }
      break;
   }
}

void Bot::executeTasks () {
   // this is core function that handle task execution

   auto func = getTask ()->func;

   // run the current task
   if (func != nullptr) {
      (this->*func) ();
   }
   else {
      logger.error ("Missing callback function of Task %d.", getCurrentTaskId ());
      kick (); // drop the player, as it's fatal for bot
   }
}

void Bot::checkSpawnConditions () {
   // this function is called instead of ai when buying finished, but freezetime is not yet left.

   // switch to knife if time to do this
   if (m_checkKnifeSwitch && m_buyingFinished && m_spawnTime + rg.get (5.0f, 7.5f) < game.time ()) {
      if (rg.get (1, 100) < 2 && cv_spraypaints.bool_ ()) {
         startTask (Task::Spraypaint, TaskPri::Spraypaint, kInvalidNodeIndex, game.time () + 1.0f, false);
      }

      if (m_difficulty >= Difficulty::Normal && rg.chance (m_personality == Personality::Rusher ? 99 : 50) && !m_isReloading && game.mapIs (MapFlags::HostageRescue | MapFlags::Demolition | MapFlags::Escape | MapFlags::Assassination)) {
         if (cv_jasonmode.bool_ ()) {
            selectSecondary ();
            issueCommand ("drop");
         }
         else {
            selectWeaponByName ("weapon_knife");
         }
      }
      m_checkKnifeSwitch = false;

      if (rg.chance (cv_user_follow_percent.int_ ()) && game.isNullEntity (m_targetEntity) && !m_isLeader && !m_hasC4 && rg.chance (50)) {
         decideFollowUser ();
      }
   }

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + rg.get (3.0f, 4.5f) < game.time ()) {
      if (hasShield () && isShieldDrawn ()) {
         pev->button |= IN_ATTACK2;
      }
      else {
         switch (m_currentWeapon) {
         case Weapon::M4A1:
         case Weapon::USP:
            checkSilencer ();
            break;

         case Weapon::Famas:
         case Weapon::Glock18:
            if (rg.chance (50)) {
               pev->button |= IN_ATTACK2;
            }
            break;
         }
      }

      // movement in freezetime is disabled, so fire movement action if button was hit
      if (pev->button & IN_ATTACK2) {
         runMovement ();
      }
      m_checkWeaponSwitch = false;
   }
}

void Bot::logic () {
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

   if (m_blindTime > game.time ()) {
      m_maxViewDistance = 4096.0f;
   }
   m_moveSpeed = pev->maxspeed;

   if (m_prevTime <= game.time ()) {

      // see how far bot has moved since the previous position...
      if (m_checkTerrain) {
         movedDistance = (m_prevOrigin - pev->origin).length ();
      }

      // save current position as previous
      m_prevOrigin = pev->origin;
      m_prevTime = game.time () + 0.2f;
   }

   // if there's some radio message to respond, check it
   if (m_radioOrder != 0) {
      checkRadioQueue ();
   }

   // do all sensing, calculate/filter all actions here
   if (canRunHeavyWeight ()) {
      setConditions ();
   }
   else if (!game.isNullEntity (m_enemy)) {
      trackEnemies ();
   }

   // some stuff required by by chatter engine
   if (cv_radio_mode.int_ () == 2) {
      if ((m_states & Sense::SeeingEnemy) && !game.isNullEntity (m_enemy)) {
         int hasFriendNearby = numFriendsNear (pev->origin, 512.0f);

         if (!hasFriendNearby && rg.chance (45) && (m_enemy->v.weapons & cr::bit (Weapon::C4))) {
            pushChatterMessage (Chatter::SpotTheBomber);
         }
         else if (!hasFriendNearby && rg.chance (45) && m_team == Team::Terrorist && util.isPlayerVIP (m_enemy)) {
            pushChatterMessage (Chatter::VIPSpotted);
         }
         else if (!hasFriendNearby && rg.chance (50) && game.getTeam (m_enemy) != m_team && isGroupOfEnemies (m_enemy->v.origin, 2, 384.0f)) {
            pushChatterMessage (Chatter::ScaredEmotion);
         }
         else if (!hasFriendNearby && rg.chance (40) && ((m_enemy->v.weapons & cr::bit (Weapon::AWP)) || (m_enemy->v.weapons & cr::bit (Weapon::Scout)) || (m_enemy->v.weapons & cr::bit (Weapon::G3SG1)) || (m_enemy->v.weapons & cr::bit (Weapon::SG550)))) {
            pushChatterMessage (Chatter::SniperWarning);
         }

         // if bot is trapped under shield yell for help !
         if (getCurrentTaskId () == Task::Camp && hasShield () && isShieldDrawn () && hasFriendNearby >= 2) {
            pushChatterMessage (Chatter::PinnedDown);
         }
      }

      // if bomb planted warn teammates !
      if (bots.hasBombSay (BombPlantedSay::Chatter) && bots.isBombPlanted () && m_team == Team::CT) {
         pushChatterMessage (Chatter::GottaFindC4);
         bots.clearBombSay (BombPlantedSay::Chatter);
      }
   }

   m_checkTerrain = true;
   m_moveToGoal = true;
   m_wantsToFire = false;

   avoidGrenades (); // avoid flyings grenades
   m_isUsingGrenade = false;

   executeTasks (); // execute current task
   updateAimDir (); // choose aim direction
   updateLookAngles (); // and turn to chosen aim direction

   // the bots wants to fire at something?
   if (m_shootAtDeadTime > game.time () || (m_wantsToFire && !m_isUsingGrenade && m_shootTime <= game.time ())) {
      fireWeapons (); // if bot didn't fire a bullet try again next frame
   }

   // check for reloading
   if (m_reloadCheckTime <= game.time ()) {
      checkReload ();
   }

   // set the reaction time (surprise momentum) different each frame according to skill
   setIdealReactionTimers ();

   // calculate 2 direction vectors, 1 without the up/down component
   const Vector &dirOld = m_destOrigin - (pev->origin + pev->velocity * getFrameInterval ());
   const Vector &dirNormal = dirOld.normalize2d ();

   m_moveAngles = dirOld.angles ();
   m_moveAngles.clampAngles ();
   m_moveAngles.x *= -1.0f; // invert for engine

   // do some overriding for special cases
   overrideConditions ();

   // allowed to move to a destination position?
   if (m_moveToGoal) {
      findValidNode ();

      // press duck button if we need to
      if ((m_path->flags & NodeFlag::Crouch) && !(m_path->flags & (NodeFlag::Camp | NodeFlag::Goal))) {
         pev->button |= IN_DUCK;
      }
      m_lastUsedNodesTime = game.time ();

      // special movement for swimming here
      if (isInWater ()) {
         // check if we need to go forward or back press the correct buttons
         if (isInFOV (m_destOrigin - getEyesPos ()) > 90.0f) {
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

   // check the darkness
   checkDarkness ();

   // must avoid a grenade?
   if (m_needAvoidGrenade != 0) {
      // don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * m_needAvoidGrenade;
   }

   // time to reach waypoint
   if (m_navTimeset + getReachTime () < game.time () && game.isNullEntity (m_enemy)) {
      findValidNode ();

      m_breakableEntity = nullptr;

      if (getCurrentTaskId () == Task::PickupItem || (m_states & Sense::PickupItem)) {
         // clear these pointers, bot mingh be stuck getting to them
         if (!game.isNullEntity (m_pickupItem) && !m_hasProgressBar) {
            m_itemIgnore = m_pickupItem;
         }

         m_itemCheckTime = game.time () + 2.0f;
         m_pickupType = Pickup::None;
         m_pickupItem = nullptr;
      }
   }

   if (m_duckTime >= game.time ()) {
      pev->button |= IN_DUCK;
   }

   if (pev->button & IN_JUMP) {
      m_jumpTime = game.time ();
   }

   if (m_jumpTime + 0.85f > game.time ()) {
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

   // check if need to use parachute
   checkParachute ();

   // display some debugging thingy to host entity
   if (!game.isDedicated () && cv_debug.int_ () >= 1) {
      showDebugOverlay ();
   }

   // save the previous speed (for checking if stuck)
   m_prevSpeed = cr::abs (m_moveSpeed);
   m_lastDamageType = -1; // reset damage
}

void Bot::spawned () {
   if (game.is (GameFlags::CSDM)) {
      newRound ();
   }
}

void Bot::showDebugOverlay () {
   bool displayDebugOverlay = false;

   if (game.getLocalEntity ()->v.iuser2 == entindex ()) {
      displayDebugOverlay = true;
   }

   if (!displayDebugOverlay && cv_debug.int_ () >= 2) {
      Bot *nearest = nullptr;

      if (util.findNearestPlayer (reinterpret_cast <void **> (&nearest), game.getLocalEntity (), 128.0f, false, true, true, true) && nearest == this) {
         displayDebugOverlay = true;
      }
   }

   if (!displayDebugOverlay) {
      return;
   }
   static float timeDebugUpdate = 0.0f;
   static int index, goal, taskID;

   static HashMap <int32, String> tasks;
   static HashMap <int32, String> personalities;
   static HashMap <int32, String> flags;

   if (tasks.empty ()) {
      tasks[Task::Normal] = "Normal";
      tasks[Task::Pause] = "Pause";
      tasks[Task::MoveToPosition] = "Move";
      tasks[Task::FollowUser] = "Follow";
      tasks[Task::PickupItem] = "Pickup";
      tasks[Task::Camp] = "Camp";
      tasks[Task::PlantBomb] = "PlantBomb";
      tasks[Task::DefuseBomb] = "DefuseBomb";
      tasks[Task::Attack] = "Attack";
      tasks[Task::Hunt] = "Hunt";
      tasks[Task::SeekCover] = "SeekCover";
      tasks[Task::ThrowExplosive] = "ThrowHE";
      tasks[Task::ThrowFlashbang] = "ThrowFL";
      tasks[Task::ThrowSmoke] = "ThrowSG";
      tasks[Task::DoubleJump] = "DoubleJump";
      tasks[Task::EscapeFromBomb] = "EscapeFromBomb";
      tasks[Task::ShootBreakable] = "DestroyBreakable";
      tasks[Task::Hide] = "Hide";
      tasks[Task::Blind] = "Blind";
      tasks[Task::Spraypaint] = "Spray";

      personalities[Personality::Rusher] = "Rusher";
      personalities[Personality::Normal] = "Normal";
      personalities[Personality::Careful] = "Careful";

      flags[AimFlags::Nav] = "Nav";
      flags[AimFlags::Camp] = "Camp";
      flags[AimFlags::PredictPath] = "Predict";
      flags[AimFlags::LastEnemy] = "LastEnemy";
      flags[AimFlags::Entity] = "Entity";
      flags[AimFlags::Enemy] = "Enemy";
      flags[AimFlags::Grenade] = "Grenade";
      flags[AimFlags::Override] = "Override";
      flags[AimFlags::Danger] = "Danger";
   }

   if (m_tasks.empty ()) {
      return;
   }

   if (taskID != getCurrentTaskId () || index != m_currentNodeIndex || goal != getTask ()->data || timeDebugUpdate < game.time ()) {
      taskID = getCurrentTaskId ();
      index = m_currentNodeIndex;
      goal = getTask ()->data;

      String enemy = "(none)";

      if (!game.isNullEntity (m_enemy)) {
         enemy = m_enemy->v.netname.chars ();
      }
      else if (!game.isNullEntity (m_lastEnemy)) {
         enemy.assignf ("%s (L)", m_lastEnemy->v.netname.chars ());
      }
      String pickup = "(none)";

      if (!game.isNullEntity (m_pickupItem)) {
         pickup = m_pickupItem->v.classname.chars ();
      }
      String aimFlags;

      for (int i = 0; i < 9; ++i) {
         bool hasFlag = m_aimFlags & cr::bit (i);

         if (hasFlag) {
            aimFlags.appendf (" %s", flags[cr::bit (i)]);
         }
      }
      auto weapon = util.weaponIdToAlias (m_currentWeapon);

      String debugData;
      debugData.assignf ("\n\n\n\n\n%s (H:%.1f/A:%.1f)- Task: %d=%s Desire:%.02f\nItem: %s Clip: %d Ammo: %d%s Money: %d AimFlags: %s\nSP=%.02f SSP=%.02f I=%d PG=%d G=%d T: %.02f MT: %d\nEnemy=%s Pickup=%s Type=%s\n", pev->netname.chars (), m_healthValue, pev->armorvalue, taskID, tasks[taskID], getTask ()->desire, weapon, getAmmoInClip (), getAmmo (), m_isReloading ? " (R)" : "", m_moneyAmount, aimFlags.trim (), m_moveSpeed, m_strafeSpeed, index, m_prevGoalIndex, goal, m_navTimeset - game.time (), pev->movetype, enemy, pickup, personalities[m_personality]);

      MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, game.getLocalEntity ())
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (1)
         .writeShort (MessageWriter::fs16 (-1.0f, 13.0f))
         .writeShort (MessageWriter::fs16 (0.0f, 13.0f))
         .writeByte (0)
         .writeByte (m_team == Team::CT ? 0 : 255)
         .writeByte (100)
         .writeByte (m_team != Team::CT ? 0 : 255)
         .writeByte (0)
         .writeByte (255)
         .writeByte (255)
         .writeByte (255)
         .writeByte (0)
         .writeShort (MessageWriter::fu16 (0.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (0.0f, 8.0f))
         .writeShort (MessageWriter::fu16 (0.15f, 8.0f))
         .writeString (debugData.chars ());

      timeDebugUpdate = game.time () + 0.1f;
   }

   // green = destination origin
   // blue = ideal angles
   // red = view angles
   game.drawLine (game.getLocalEntity (), getEyesPos (), m_destOrigin, 10, 0, { 0, 255, 0 }, 250, 5, 1, DrawLine::Arrow);
   game.drawLine (game.getLocalEntity (), getEyesPos () - Vector (0.0f, 0.0f, 16.0f), getEyesPos () + m_idealAngles.forward () * 300.0f, 10, 0, { 0, 0, 255 }, 250, 5, 1, DrawLine::Arrow);
   game.drawLine (game.getLocalEntity (), getEyesPos () - Vector (0.0f, 0.0f, 32.0f), getEyesPos () + pev->v_angle.forward () * 300.0f, 10, 0, {255, 0, 0}, 250, 5, 1, DrawLine::Arrow);

   // now draw line from source to destination
   for (size_t i = 0; i < m_pathWalk.length () && i + 1 < m_pathWalk.length (); ++i) {
      game.drawLine (game.getLocalEntity (), graph[m_pathWalk.at (i)].origin, graph[m_pathWalk.at (i + 1)].origin, 15, 0, { 255, 100, 55 }, 200, 5, 1, DrawLine::Arrow);
   }
}

bool Bot::hasHostage () {
   for (auto hostage : m_hostages) {
      if (!game.isNullEntity (hostage)) {

         // don't care about dead hostages
         if (hostage->v.health <= 0.0f || (pev->origin - hostage->v.origin).lengthSq () > cr::square (600.0f)) {
            hostage = nullptr;
            continue;
         }
         return true;
      }
   }
   return false;
}

int Bot::getAmmo () {
   const auto &prop = conf.getWeaponProp (m_currentWeapon);

   if (prop.ammo1 == -1 || prop.ammo1 > kMaxWeapons - 1) {
      return 0;
   }
   return m_ammo[prop.ammo1];
}

void Bot::takeDamage (edict_t *inflictor, int damage, int armor, int bits) {
   // this function gets called from the network message handler, when bot's gets hurt from any
   // other player.

   m_lastDamageType = bits;
   updatePracticeValue (damage);

   if (util.isPlayer (inflictor) || (cv_attack_monsters.bool_ () && util.isMonster (inflictor))) {
      if (!util.isMonster (inflictor) && cv_tkpunish.bool_ () && game.getTeam (inflictor) == m_team && !util.isFakeClient (inflictor)) {
         // alright, die you teamkiller!!!
         m_actualReactionTime = 0.0f;
         m_seeEnemyTime = game.time ();
         m_enemy = inflictor;

         m_lastEnemy = m_enemy;
         m_lastEnemyOrigin = m_enemy->v.origin;
         m_enemyOrigin = m_enemy->v.origin;

         pushChatMessage (Chat::TeamAttack);
         pushChatterMessage (Chatter::FriendlyFire);
      }
      else {
         // attacked by an enemy
         if (m_healthValue > 60.0f) {
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
         clearTask (Task::Camp);

         if (game.isNullEntity (m_enemy) && m_team != game.getTeam (inflictor)) {
            m_lastEnemy = inflictor;
            m_lastEnemyOrigin = inflictor->v.origin;

            // FIXME - Bot doesn't necessary sees this enemy
            m_seeEnemyTime = game.time ();
         }

         if (!game.is (GameFlags::CSDM)) {
            updatePracticeDamage (inflictor, armor + damage);
         }
      }
   }
   // hurt by unusual damage like drowning or gas
   else {
      // leave the camping/hiding position
      if (!isReachableNode (graph.getNearest (m_destOrigin))) {
         clearSearchNodes ();
         findBestNearestNode ();
      }
   }
}

void Bot::takeBlind (int alpha) {
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind from the grenade.

   m_maxViewDistance = rg.get (10.0f, 20.0f);
   m_blindTime = game.time () + static_cast <float> (alpha - 200) / 16.0f;

   if (m_blindTime < game.time ()) {
      return;
   }
   m_enemy = nullptr;

   if (m_difficulty <= Difficulty::Normal) {
      m_blindMoveSpeed = 0.0f;
      m_blindSidemoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;

      return;
   }

   m_blindMoveSpeed = -pev->maxspeed;
   m_blindSidemoveSpeed = 0.0f;

   if (rg.chance (50)) {
      m_blindSidemoveSpeed = pev->maxspeed;
   }
   else {
      m_blindSidemoveSpeed = -pev->maxspeed;
   }

   if (m_healthValue < 85.0f) {
      m_blindMoveSpeed = -pev->maxspeed;
   }
   else if (m_personality == Personality::Careful) {
      m_blindMoveSpeed = 0.0f;
      m_blindButton = IN_DUCK;
   }
   else {
      m_blindMoveSpeed = pev->maxspeed;
   }
}

void Bot::updatePracticeValue (int damage) {
   // gets called each time a bot gets damaged by some enemy. tries to achieve a statistic about most/less dangerous
   // waypoints for a destination goal used for pathfinding

   if (graph.length () < 1 || graph.hasChanged () || m_chosenGoalIndex < 0 || m_prevGoalIndex < 0) {
      return;
   }

   // only rate goal waypoint if bot died because of the damage
   // FIXME: could be done a lot better, however this cares most about damage done by sniping or really deadly weapons
   if (m_healthValue - damage <= 0) {
      graph.setDangerValue (m_team, m_chosenGoalIndex, m_prevGoalIndex, cr::clamp (graph.getDangerValue (m_team, m_chosenGoalIndex, m_prevGoalIndex) - static_cast <int> (m_healthValue / 20), -kMaxPracticeGoalValue, kMaxPracticeGoalValue));
   }
}

void Bot::updatePracticeDamage (edict_t *attacker, int damage) {
   // this function gets called each time a bot gets damaged by some enemy. sotores the damage (teamspecific) done by victim.

   if (!util.isPlayer (attacker)) {
      return;
   }

   int attackerTeam = game.getTeam (attacker);
   int victimTeam = m_team;

   if (attackerTeam == victimTeam) {
      return;
   }

   // if these are bots also remember damage to rank destination of the bot
   m_goalValue -= static_cast <float> (damage);

   if (bots[attacker] != nullptr) {
      bots[attacker]->m_goalValue += static_cast <float> (damage);
   }

   if (damage < 20) {
      return; // do not collect damage less than 20
   }

   int attackerIndex = graph.getNearest (attacker->v.origin);
   int victimIndex = m_currentNodeIndex;

   if (victimIndex == kInvalidNodeIndex) {
      victimIndex = findNearestNode ();
   }

   if (m_healthValue > 20.0f) {
      if (victimTeam == Team::Terrorist || victimTeam == Team::CT) {
         graph.setDangerDamage (victimIndex, victimIndex, victimIndex, cr::clamp (graph.getDangerDamage (victimTeam, victimIndex, victimIndex), 0, kMaxPracticeDamageValue));
      }
   }
   float updateDamage = util.isFakeClient (attacker) ? 10.0f : 7.0f;

   // store away the damage done
   int damageValue = cr::clamp (graph.getDangerDamage (m_team, victimIndex, attackerIndex) + static_cast <int> (damage / updateDamage), 0, kMaxPracticeDamageValue);

   if (damageValue > graph.getHighestDamageForTeam (m_team)) {
      graph.setHighestDamageForTeam (m_team, damageValue);
   }
   graph.setDangerDamage (m_team, victimIndex, attackerIndex, damageValue);
}

void Bot::pushChatMessage (int type, bool isTeamSay) {
   if (!conf.hasChatBank (type) || !cv_chat.bool_ ()) {
      return;
   }

   prepareChatMessage (conf.pickRandomFromChatBank (type));
   pushMsgQueue (isTeamSay ? BotMsg::SayTeam : BotMsg::Say);
}

void Bot::dropWeaponForUser (edict_t *user, bool discardC4) {
   // this function, asks bot to discard his current primary weapon (or c4) to the user that requsted it with /drop*
   // command, very useful, when i'm don't have money to buy anything... )

   if (util.isAlive (user) && m_moneyAmount >= 2000 && hasPrimaryWeapon () && (user->v.origin - pev->origin).length () <= 450.0f) {
      m_aimFlags |= AimFlags::Entity;
      m_lookAt = user->v.origin;

      if (discardC4) {
         selectWeaponByName ("weapon_c4");
         issueCommand ("drop");
      }
      else {
         selectBestWeapon ();
         issueCommand ("drop");
      }

      m_pickupItem = nullptr;
      m_pickupType = Pickup::None;
      m_itemCheckTime = game.time () + 5.0f;

      if (m_inBuyZone) {
         m_ignoreBuyDelay = true;
         m_buyingFinished = false;
         m_buyState = BuyState::PrimaryWeapon;

         pushMsgQueue (BotMsg::Buy);
         m_nextBuyTime = game.time ();
      }
   }
}

void Bot::startDoubleJump (edict_t *ent) {
   resetDoubleJump ();

   m_doubleJumpOrigin = ent->v.origin;
   m_doubleJumpEntity = ent;

   startTask (Task::DoubleJump, TaskPri::DoubleJump, kInvalidNodeIndex, game.time (), true);
   sendToChat (strings.format ("Ok %s, i will help you!", ent->v.netname.chars ()), true);
}

void Bot::sendBotToOrigin (const Vector &origin) {
   m_position = origin;
   m_chosenGoalIndex = graph.getNearestNoBuckets (origin);

   getTask ()->data = m_chosenGoalIndex;

   startTask (Task::MoveToPosition, TaskPri::Hide, m_chosenGoalIndex, 0.0f, true);
}

void Bot::resetDoubleJump () {
   completeTask ();

   m_doubleJumpEntity = nullptr;
   m_duckForJump = 0.0f;
   m_doubleJumpOrigin = nullptr;
   m_travelStartIndex = kInvalidNodeIndex;
   m_jumpReady = false;
}

void Bot::debugMsgInternal (const char *str) {
   if (game.isDedicated ()) {
      return;
   }
   int level = cv_debug.int_ ();

   if (level <= 2) {
      return;
   }
   String printBuf;
   printBuf.assignf ("%s: %s", pev->netname.chars (), str);

   bool playMessage = false;

   if (level == 3 && !game.isNullEntity (game.getLocalEntity ()) && game.getLocalEntity ()->v.iuser2 == entindex ()) {
      playMessage = true;
   }
   else if (level != 3) {
      playMessage = true;
   }
   if (playMessage && level > 3) {
      logger.message (printBuf.chars ());
   }
   if (playMessage) {
      ctrl.msg (printBuf.chars ());
      sendToChat (printBuf, false);
   }
}

Vector Bot::calcToss (const Vector &start, const Vector &stop) {
   // this function returns the velocity at which an object should looped from start to land near end.
   // returns null vector if toss is not feasible.

   TraceResult tr {};
   float gravity = sv_gravity.float_ () * 0.55f;

   Vector end = stop - pev->velocity;
   end.z -= 15.0f;

   if (cr::abs (end.z - start.z) > 500.0f) {
      return nullptr;
   }
   Vector midPoint = start + (end - start) * 0.5f;
   game.testHull (midPoint, midPoint + Vector (0.0f, 0.0f, 500.0f), TraceIgnore::Monsters, head_hull, ent (), &tr);

   if (tr.flFraction < 1.0f && tr.pHit) {
      midPoint = tr.vecEndPos;
      midPoint.z = tr.pHit->v.absmin.z - 1.0f;
   }

   if (midPoint.z < start.z || midPoint.z < end.z) {
      return nullptr;
   }
   float timeOne = cr::sqrtf ((midPoint.z - start.z) / (0.5f * gravity));
   float timeTwo = cr::sqrtf ((midPoint.z - end.z) / (0.5f * gravity));

   if (timeOne < 0.1f) {
      return nullptr;
   }
   Vector velocity = (end - start) / (timeOne + timeTwo);
   velocity.z = gravity * timeOne;

   Vector apex = start + velocity * timeOne;
   apex.z = midPoint.z;

   game.testHull (start, apex, TraceIgnore::None, head_hull, ent (), &tr);

   if (tr.flFraction < 1.0f || tr.fAllSolid) {
      return nullptr;
   }
   game.testHull (end, apex, TraceIgnore::Monsters, head_hull, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      float dot = -(tr.vecPlaneNormal | (apex - end).normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) {
         return nullptr;
      }
   }
   return velocity * 0.777f;
}

Vector Bot::calcThrow (const Vector &start, const Vector &stop) {
   // this function returns the velocity vector at which an object should be thrown from start to hit end.
   // returns null vector if throw is not feasible.

   Vector velocity = stop - start;
   TraceResult tr {};

   float gravity = sv_gravity.float_ () * 0.55f;
   float time = velocity.length () / 195.0f;

   if (time < 0.01f) {
      return nullptr;
   }
   else if (time > 2.0f) {
      time = 1.2f;
   }
   float half = time * 0.5f;

   velocity = velocity * (1.0f / time);
   velocity.z += gravity * half;

   Vector apex = start + (stop - start) * 0.5f;
   apex.z += 0.5f * gravity * half * half;

   game.testHull (start, apex, TraceIgnore::None, head_hull, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      return nullptr;
   }
   game.testHull (stop, apex, TraceIgnore::Monsters, head_hull, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0) || tr.fAllSolid) {
      float dot = -(tr.vecPlaneNormal | (apex - stop).normalize ());

      if (dot > 0.7f || tr.flFraction < 0.8f) {
         return nullptr;
      }
   }
   return velocity * 0.7793f;
}

edict_t *Bot::correctGrenadeVelocity (const char *model) {
   edict_t *result = nullptr;

   game.searchEntities ("classname", "grenade", [&] (edict_t *ent) {
      if (ent->v.owner == this->ent () && strcmp (ent->v.model.chars (9), model) == 0) {
         result = ent;

         // set the correct velocity for the grenade
         if (m_grenade.lengthSq () > 100.0f) {
            ent->v.velocity = m_grenade;
         }
         m_grenadeCheckTime = game.time () + kGrenadeCheckTime;

         selectBestWeapon ();
         completeTask ();

         return EntitySearchResult::Break;
      }
      return EntitySearchResult::Continue;
   });
   return result;
}

Vector Bot::isBombAudible () {
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!bots.isBombPlanted () || getCurrentTaskId () == Task::EscapeFromBomb) {
      return nullptr; // reliability check
   }

   if (m_difficulty > Difficulty::Normal) {
      return graph.getBombOrigin ();
   }
   const Vector &bombOrigin = graph.getBombOrigin ();

   float timeElapsed = ((game.time () - bots.getTimeBombPlanted ()) / mp_c4timer.float_ ()) * 100.0f;
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
   if (desiredRadius < (pev->origin - bombOrigin).length2d ()) {
      return bombOrigin;
   }
   return nullptr;
}

uint8 Bot::computeMsec () {
   // estimate msec to use for this command based on time passed from the previous command

   return static_cast <uint8> ((game.time () - m_lastCommandTime) * 1000.0f);
}

bool Bot::canRunHeavyWeight () {
   constexpr auto interval = 1.0f / 10.0f;

   if (m_heavyTimestamp + interval < game.time ()) {
      m_heavyTimestamp = game.time ();

      return true;
   }
   return false;
}

bool Bot::canSkipNextTrace (TraceChannel channel) {
   // for optmization purposes skip every second traceline fired, this doesn't affect ai
   // behaviour too much, but saves alot of cpu cycles.

   constexpr auto kSkipTrace = 3;

   // check if we're have to skip
   if ((++m_traceSkip[channel] % kSkipTrace) == 0) {
      m_traceSkip[channel] = 0;
      return true;
   }
   return false;
}

void Bot::runMovement () {
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

   m_frameInterval = game.time () - m_lastCommandTime;

   uint8 msecVal = computeMsec ();
   m_lastCommandTime = game.time ();

   engfuncs.pfnRunPlayerMove (pev->pContainingEntity, m_moveAngles, m_moveSpeed, m_strafeSpeed, 0.0f, static_cast <uint16> (pev->button), static_cast <uint8> (pev->impulse), msecVal);

   // save our own copy of old buttons, since bot ai code is not running every frame now
   m_oldButtons = pev->button;
}

void Bot::checkBurstMode (float distance) {
   // this function checks burst mode, and switch it depending distance to to enemy.

   if (hasShield ()) {
      return; // no checking when shield is active
   }

   // if current weapon is glock, disable burstmode on long distances, enable it else
   if (m_currentWeapon == Weapon::Glock18 && distance < 300.0f && m_weaponBurstMode == BurstMode::Off) {
      pev->button |= IN_ATTACK2;
   }
   else if (m_currentWeapon == Weapon::Glock18 && distance >= 300.0f && m_weaponBurstMode == BurstMode::On) {
      pev->button |= IN_ATTACK2;
   }

   // if current weapon is famas, disable burstmode on short distances, enable it else
   if (m_currentWeapon == Weapon::Famas && distance > 400.0f && m_weaponBurstMode == BurstMode::Off) {
      pev->button |= IN_ATTACK2;
   }
   else if (m_currentWeapon == Weapon::Famas && distance <= 400.0f && m_weaponBurstMode == BurstMode::On) {
      pev->button |= IN_ATTACK2;
   }
}

void Bot::checkSilencer () {
   if ((m_currentWeapon == Weapon::USP || m_currentWeapon == Weapon::M4A1) && !hasShield ()) {
      int prob = (m_personality == Personality::Rusher ? 35 : 65);

      // aggressive bots don't like the silencer
      if (rg.chance (m_currentWeapon == Weapon::USP ? prob / 2 : prob)) {
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

float Bot::getBombTimeleft () {
   if (!bots.isBombPlanted ()) {
      return 0.0f;
   }
   float timeLeft = ((bots.getTimeBombPlanted () + mp_c4timer.float_ ()) - game.time ());

   if (timeLeft < 0.0f) {
      return 0.0f;
   }
   return timeLeft;
}

bool Bot::isOutOfBombTimer () {
   if (!game.mapIs (MapFlags::Demolition)) {
      return false;
   }

   if (m_currentNodeIndex == kInvalidNodeIndex || (m_hasProgressBar || getCurrentTaskId () == Task::EscapeFromBomb)) {
      return false; // if CT bot already start defusing, or already escaping, return false
   }

   // calculate left time
   float timeLeft = getBombTimeleft ();

   // if time left greater than 13, no need to do other checks
   if (timeLeft > 13.0f) {
      return false;
   }
   const Vector &bombOrigin = graph.getBombOrigin ();

   // for terrorist, if timer is lower than 13 seconds, return true
   if (timeLeft < 13.0f && m_team == Team::Terrorist && (bombOrigin - pev->origin).lengthSq () < cr::square (964.0f)) {
      return true;
   }
   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (const auto &bot : bots) {
      // search players with defuse kit
      if (bot.get () != this && bot->m_team == Team::CT && bot->m_hasDefuser && (bombOrigin - bot->pev->origin).lengthSq () < cr::square (512.0f)) {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   float reachTime = graph.calculateTravelTime (pev->maxspeed, m_path->origin, bombOrigin);

   // for counter-terrorist check alos is we have time to reach position plus average defuse time
   if ((timeLeft < reachTime + 8.0f && !m_hasDefuser && !hasTeammatesWithDefuserKit) || (timeLeft < reachTime + 4.0f && m_hasDefuser)) {
      return true;
   }

   if (m_hasProgressBar && isOnFloor () && ((m_hasDefuser ? 10.0f : 15.0f) > getBombTimeleft ())) {
      return true;
   }
   return false; // return false otherwise
}

void Bot::updateHearing () {
   int hearEnemyIndex = kInvalidNodeIndex;
   float minDistance = kInfiniteDistance;

   // setup potential visibility set from engine
   auto set = game.getVisibilitySet (this, false);

   // loop through all enemy clients to check for hearable stuff
   for (int i = 0; i < game.maxClients (); ++i) {
      const Client &client = util.getClient (i);

      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.ent == ent () || client.team == m_team || client.noise.last < game.time ()) {
         continue;
      }

      if (!game.checkVisibility (client.ent, set)) {
         continue;
      }
      float distance = (client.noise.pos - pev->origin).length ();

      if (distance > client.noise.dist) {
         continue;
      }

      if (distance < minDistance) {
         hearEnemyIndex = i;
         minDistance = distance;
      }
   }
   edict_t *player = nullptr;

   if (hearEnemyIndex >= 0 && util.getClient (hearEnemyIndex).team != m_team && !game.is (GameFlags::FreeForAll)) {
      player = util.getClient (hearEnemyIndex).ent;
   }

   // did the bot hear someone ?
   if (player != nullptr && util.isPlayer (player)) {
      // change to best weapon if heard something
      if (m_shootTime < game.time () - 5.0f && isOnFloor () && m_currentWeapon != Weapon::C4 && m_currentWeapon != Weapon::Explosive && m_currentWeapon != Weapon::Smoke && m_currentWeapon != Weapon::Flashbang && !cv_jasonmode.bool_ ()) {
         selectBestWeapon ();
      }

      m_heardSoundTime = game.time ();
      m_states |= Sense::HearingEnemy;

      if (rg.chance (15) && game.isNullEntity (m_enemy) && game.isNullEntity (m_lastEnemy) && m_seeEnemyTime + 7.0f < game.time ()) {
         pushChatterMessage (Chatter::HeardTheEnemy);
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
            if (m_states & Sense::SeeingEnemy) {
               return;
            }
            m_lastEnemyOrigin = player->v.origin;
         }
         else {
            // if bot had an enemy but the heard one is nearer, take it instead
            float distance = (m_lastEnemyOrigin - pev->origin).lengthSq ();

            if (distance > (player->v.origin - pev->origin).lengthSq () && m_seeEnemyTime + 2.0f < game.time ()) {
               m_lastEnemy = player;
               m_lastEnemyOrigin = player->v.origin;
            }
            else {
               return;
            }
         }
      }
      extern ConVar cv_shoots_thru_walls;

      // check if heard enemy can be seen
      if (checkBodyParts (player)) {
         m_enemy = player;
         m_lastEnemy = player;
         m_lastEnemyOrigin = m_enemyOrigin;

         m_states |= Sense::SeeingEnemy;
         m_seeEnemyTime = game.time ();
      }

      // check if heard enemy can be shoot through some obstacle
      else {
         if (cv_shoots_thru_walls.bool_ () && m_difficulty > Difficulty::Normal && m_lastEnemy == player && rg.chance (conf.getDifficultyTweaks (m_difficulty)->hearThruPct) && m_seeEnemyTime + 3.0f > game.time () && isPenetrableObstacle (player->v.origin)) {
            m_enemy = player;
            m_lastEnemy = player;
            m_enemyOrigin = player->v.origin;
            m_lastEnemyOrigin = player->v.origin;

            m_states |= (Sense::SeeingEnemy | Sense::SuspectEnemy);
            m_seeEnemyTime = game.time ();
         }
      }
   }
}

void Bot::enteredBuyZone (int buyState) {
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   const int *econLimit = conf.getEconLimit ();

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (m_seeEnemyTime + 12.0f < game.time () && m_lastEquipTime + 15.0f < game.time () && m_inBuyZone && (bots.getRoundStartTime () + rg.get (10.0f, 20.0f) + mp_buytime.float_ () < game.time ()) && !bots.isBombPlanted () && m_moneyAmount > econLimit[EcoLimit::PrimaryGreater]) {
      m_ignoreBuyDelay = true;
      m_buyingFinished = false;
      m_buyState = buyState;

      // push buy message
      pushMsgQueue (BotMsg::Buy);

      m_nextBuyTime = game.time ();
      m_lastEquipTime = game.time ();
   }
}

bool Bot::isBombDefusing (const Vector &bombOrigin) {
   // this function finds if somebody currently defusing the bomb.

   if (!bots.isBombPlanted ()) {
      return false;
   }
   bool defusingInProgress = false;
   constexpr auto distanceToBomb = cr::square (140.0f);

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive)) {
         continue;
      }

      auto bot = bots[client.ent];
      auto bombDistance = (client.origin - bombOrigin).lengthSq ();

      if (bot && !bot->m_notKilled) {
         if (m_team != bot->m_team || bot->getCurrentTaskId () == Task::EscapeFromBomb) {
            continue; // skip other mess
         }
         
         // if close enough, mark as progressing
         if (bombDistance < distanceToBomb && (bot->getCurrentTaskId () == Task::DefuseBomb || bot->m_hasProgressBar)) {
            defusingInProgress = true;
            break;
         }
         continue;
      }

      // take in account peoples too
      if (client.team == m_team) {

         // if close enough, mark as progressing
         if (bombDistance < distanceToBomb && ((client.ent->v.button | client.ent->v.oldbuttons) & IN_USE)) {
            defusingInProgress = true;
            break;
         }
         continue;
      }
   }
   return defusingInProgress;
}

float Bot::getShiftSpeed () {
   if (getCurrentTaskId () == Task::SeekCover || (pev->flags & FL_DUCKING) || (pev->button & IN_DUCK) || (m_oldButtons & IN_DUCK) || (m_currentTravelFlags & PathFlag::Jump) || (m_path != nullptr && m_path->flags & NodeFlag::Ladder) || isOnLadder () || isInWater () || m_isStuck) {
      return pev->maxspeed;
   }
   return static_cast <float> (pev->maxspeed * 0.4f);
}

void Bot::calculateFrustum () {
   // this function updates bot view frustum

   Vector forward, right, up;
   pev->v_angle.angleVectors (&forward, &right, &up);

   static Vector fc, nc, fbl, fbr, ftl, ftr, nbl, nbr, ntl, ntr;

   fc = getEyesPos () + forward * frustum.MaxView;
   nc = getEyesPos () + forward * frustum.MinView;

   fbl = fc + (up * frustum.farHeight * 0.5f) - (right * frustum.farWidth * 0.5f);
   fbr = fc + (up * frustum.farHeight * 0.5f) + (right * frustum.farWidth * 0.5f);
   ftl = fc - (up * frustum.farHeight * 0.5f) - (right * frustum.farWidth * 0.5f);
   ftr = fc - (up * frustum.farHeight * 0.5f) + (right * frustum.farWidth * 0.5f);
   nbl = nc + (up * frustum.nearHeight * 0.5f) - (right * frustum.nearWidth * 0.5f);
   nbr = nc + (up * frustum.nearHeight * 0.5f) + (right * frustum.nearWidth * 0.5f);
   ntl = nc - (up * frustum.nearHeight * 0.5f) - (right * frustum.nearWidth * 0.5f);
   ntr = nc - (up * frustum.nearHeight * 0.5f) + (right * frustum.nearWidth * 0.5f);

   auto setPlane = [&] (FrustumSide side, const Vector &v1, const Vector &v2, const Vector &v3) {
      auto &plane = m_frustum[side];

      plane.normal = ((v2 - v1) ^ (v3 - v1)).normalize ();
      plane.point = v2;

      plane.result = -(plane.normal | plane.point);
   };

   setPlane (FrustumSide::Top, ftl, ntl, ntr);
   setPlane (FrustumSide::Bottom, fbr, nbr, nbl);
   setPlane (FrustumSide::Left, fbl, nbl, ntl);
   setPlane (FrustumSide::Right, ftr, ntr, nbr);
   setPlane (FrustumSide::Near, nbr, ntr, ntl);
   setPlane (FrustumSide::Far, fbl, ftl, ftr);
}

bool Bot::isEnemyInFrustum (edict_t *enemy) {
   const Vector &origin = enemy->v.origin - Vector (0.0f, 0.0f, 5.0f);

   for (auto &plane : m_frustum) {
      if (!util.isObjectInsidePlane (plane, origin, 60.0f, 16.0f)) {
         return false;
      }
   }
   return true;
}
