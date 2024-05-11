//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_debug ("debug", "0", "Enables or disables useful messages about bot states. Not required for end users.", true, 0.0f, 4.0f);
ConVar cv_debug_goal ("debug_goal", "-1", "Forces all alive bots to build path and go to the specified here graph node.", true, -1.0f, kMaxNodes);
ConVar cv_user_follow_percent ("user_follow_percent", "20", "Specifies the percent of bots, that can follow leader on each round start.", true, 0.0f, 100.0f);
ConVar cv_user_max_followers ("user_max_followers", "1", "Specifies how many bots can follow a single user.", true, 0.0f, static_cast <float> (kGameMaxPlayers / 4));

ConVar cv_jasonmode ("jasonmode", "0", "If enabled, all bots will be forced only the knife, skipping weapon buying routines.");
ConVar cv_radio_mode ("radio_mode", "2", "Allows bots to use radio or chatter.\nAllowed values: '0', '1', '2'.\nIf '0', radio and chatter is disabled.\nIf '1', only radio allowed.\nIf '2' chatter and radio allowed.", true, 0.0f, 2.0f);

ConVar cv_economics_rounds ("economics_rounds", "1", "Specifies whether bots able to use team economics, like do not buy any weapons for whole team to keep money for better guns.");
ConVar cv_economics_disrespect_percent ("economics_disrespect_percent", "25", "Allows bots to ignore economics and buy weapons with disrespect of economics.", true, 0.0f, 100.0f);

ConVar cv_check_darkness ("check_darkness", "1", "Allows or disallows bot to check environment for darkness, thus allows or not to use flashlights or NVG.");
ConVar cv_avoid_grenades ("avoid_grenades", "1", "Allows bots to partially avoid grenades.");

ConVar cv_tkpunish ("tkpunish", "1", "Allows or disallows bots to take revenge of teamkillers / team attacks.");
ConVar cv_freeze_bots ("freeze_bots", "0", "If enabled, the bots think function is disabled, so bots will not move anywhere from their spawn spots.");
ConVar cv_spraypaints ("spraypaints", "1", "Allows or disallows the use of spray paints.");
ConVar cv_botbuy ("botbuy", "1", "Allows or disallows bots weapon buying routines.");
ConVar cv_destroy_breakables_around ("destroy_breakables_around", "1", "Allows bots to destroy breakables around them, even without touching with them.");

ConVar cv_object_pickup_radius ("object_pickup_radius", "450.0", "The radius on which bot searches world for new objects, items, and weapons.", true, 64.0f, 1024.0f);
ConVar cv_object_destroy_radius ("object_destroy_radius", "400.0", "The radius on which bot destroy breakables around it, when not touching with them.", true, 64.0f, 1024.0f);

ConVar cv_chatter_path ("chatter_path", "sound/radio/bot", "Specifies the paths for the bot chatter sound files.", false);
ConVar cv_restricted_weapons ("restricted_weapons", "", "Specifies semicolon separated list of weapons that are not allowed to buy / pickup.", false);

ConVar cv_attack_monsters ("attack_monsters", "0", "Allows or disallows bots to attack monsters.");
ConVar cv_pickup_custom_items ("pickup_custom_items", "0", "Allows or disallows bots to pickup custom items.");
ConVar cv_pickup_ammo_and_kits ("pickup_ammo_and_kits", "0", "Allows bots pickup mod items like ammo, health kits and suits.");
ConVar cv_pickup_best ("pickup_best", "1", "Allows or disallows bots to pickup best weapons.");
ConVar cv_ignore_objectives ("ignore_objectives", "0", "Allows or disallows bots to do map objectives, i.e. plant/defuse bombs, and save hostages.");
ConVar cv_smoke_grenade_checks ("smoke_grenade_checks", "2", "Affect bot's vision by smoke clouds.", true, 0.0f, 2.0f);

// game console variables
ConVar mp_c4timer ("mp_c4timer", nullptr, Var::GameRef);
ConVar mp_buytime ("mp_buytime", nullptr, Var::GameRef, true, "1");
ConVar mp_startmoney ("mp_startmoney", nullptr, Var::GameRef, true, "800");
ConVar mp_footsteps ("mp_footsteps", nullptr, Var::GameRef);

void Bot::pushMsgQueue (int message) {
   // this function put a message into the bot message queue

   if (message == BotMsg::Say) {
      // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
      const int entityIndex = index ();

      for (const auto &other : bots) {
         if (other->pev != pev) {
            if (m_isAlive == other->m_isAlive) {
               other->m_sayTextBuffer.entityIndex = entityIndex;
               other->m_sayTextBuffer.sayText = m_chatBuffer;
            }
            other->m_sayTextBuffer.timeNextChat = game.time () + other->m_sayTextBuffer.chatDelay;
         }
      }
   }
   m_msgQueue.emplaceLast (message);
}

void Bot::avoidGrenades () {
   // checks if bot 'sees' a grenade, and avoid it

   // check if old pointers to grenade is invalid
   if (game.isNullEntity (m_avoidGrenade)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }
   else if ((m_avoidGrenade->v.flags & FL_ONGROUND) || (m_avoidGrenade->v.effects & EF_NODRAW)) {
      m_avoidGrenade = nullptr;
      m_needAvoidGrenade = 0;
   }

   if (!bots.hasActiveGrenades ()) {
      return;
   }
   const auto &activeGrenades = bots.getActiveGrenades ();

   // find all grenades on the map
   for (auto pent : activeGrenades) {
      if (pent->v.effects & EF_NODRAW) {
         continue;
      }

      // check if visible to the bot
      if (isInFOV (pent->v.origin - getEyesPos ()) > pev->fov * 0.5f && !seesEntity (pent->v.origin)) {
         continue;
      }
      auto model = pent->v.model.str (9);

      if (m_preventFlashing < game.time ()
         && m_personality == Personality::Rusher
         && m_difficulty == Difficulty::Expert
         && model == kFlashbangModelName) {

         // don't look at flash bang
         if (!(m_states & Sense::SeeingEnemy)) {
            m_lookAt.y = cr::wrapAngle ((game.getEntityOrigin (pent) - getEyesPos ()).angles ().y + 180.0f);

            m_canChooseAimDirection = false;
            m_preventFlashing = game.time () + rg (1.0f, 2.0f);
         }
      }
      else if (game.isNullEntity (m_avoidGrenade) && model == kExplosiveModelName) {
         if (game.getTeam (pent->v.owner) == m_team || pent->v.owner == ent ()) {
            continue;
         }

         if (!(pent->v.flags & FL_ONGROUND)) {
            const float distanceSq = pent->v.origin.distanceSq (pev->origin);
            const float distanceMovedSq = pev->origin.distanceSq (pent->v.origin + pent->v.velocity * m_frameInterval);

            if (distanceMovedSq < distanceSq && distanceSq < cr::sqrf (500.0f)) {
               const auto &dirToPoint = (pev->origin - pent->v.origin).normalize2d_apx ();
               const auto &rightSide = pev->v_angle.right ().normalize2d_apx ();

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
      else if (cv_smoke_grenade_checks.as <int> () == 1 && (pent->v.flags & FL_ONGROUND) && model == kSmokeModelName) {
         if (seesEntity (pent->v.origin) && isInFOV (pent->v.origin - getEyesPos ()) < pev->fov / 3.0f) {
            const auto &entOrigin = game.getEntityOrigin (pent);
            const auto &betweenUs = (entOrigin - pev->origin).normalize_apx ();
            const auto &betweenNade = (entOrigin - pev->origin).normalize_apx ();
            const auto &betweenResult = ((betweenNade.get2d () * 150.0f + entOrigin) - pev->origin).normalize_apx ();

            if ((betweenNade | betweenUs) > (betweenNade | betweenResult) && util.isVisible (pent->v.origin, ent ())) {
               const float distance = entOrigin.distance (pev->origin);

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
}

void Bot::checkBreakable (edict_t *touch) {
   if (game.isNullEntity (touch)) {
      m_breakableEntity = lookupBreakable ();
   }
   else {
      m_breakableEntity = touch;
      m_breakableOrigin = game.getEntityOrigin (touch);
   }

   // re-check from previous steps
   if (game.isNullEntity (m_breakableEntity) || m_breakableOrigin.empty ()) {
      return;
   }
   m_campButtons = pev->button & IN_DUCK;
   startTask (Task::ShootBreakable, TaskPri::ShootBreakable, kInvalidNodeIndex, 0.0f, false);
}

void Bot::checkBreakablesAround () {
   if (!m_buyingFinished
      || !cv_destroy_breakables_around
      || usesKnife ()
      || rg.chance (25)
      || !game.hasBreakables ()
      || m_seeEnemyTime + 4.0f > game.time ()
      || !game.isNullEntity (m_enemy)
      || !hasPrimaryWeapon ()) {

      return;
   }
   const auto radius = cv_object_destroy_radius.as <float> ();

   // check if we're have some breakables in 400 units range
   for (const auto &breakable : game.getBreakables ()) {
      bool ignoreBreakable = false;

      // check if it's blacklisted
      for (const auto &ignored : m_ignoredBreakable) {
         if (ignored == breakable) {
            ignoreBreakable = true;
            break;
         }
      }

      // keep searching
      if (ignoreBreakable) {
         continue;
      }

      if (!util.isShootableBreakable (breakable)) {
         continue;
      }

      const auto &origin = game.getEntityOrigin (breakable);
      const auto lengthToObstacleSq = origin.distanceSq (pev->origin);

      // too far, skip it
      if (lengthToObstacleSq > cr::sqrf (radius)) {
         continue;
      }

      // too close, skip it
      if (lengthToObstacleSq < cr::sqrf (100.0f)) {
         continue;
      }

      // maybe time to give up?
      if (m_lastBreakable == breakable && m_breakableTime + 1.0f < game.time ()) {
         m_ignoredBreakable.emplace (breakable);

         m_breakableOrigin = nullptr;
         m_lastBreakable = nullptr;
         m_breakableEntity = nullptr;

         continue;
      }

      if (isInFOV (origin - getEyesPos ()) < pev->fov && seesEntity (origin)) {
         if (m_breakableEntity != breakable) {
            m_breakableTime = game.time ();
            m_lastBreakable = breakable;
         }

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
   game.testLine (pev->origin, pev->origin + (m_destOrigin - pev->origin).normalize_apx () * 72.0f, TraceIgnore::None, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      auto ent = tr.pHit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (util.isShootableBreakable (ent)) {
         m_breakableOrigin = game.getEntityOrigin (ent);
         return ent;
      }
   }
   game.testLine (getEyesPos (), getEyesPos () + (m_destOrigin - getEyesPos ()).normalize_apx () * 72.0f, TraceIgnore::None, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      auto ent = tr.pHit;

      if (util.isShootableBreakable (ent)) {
         m_breakableOrigin = game.getEntityOrigin (ent);
         return ent;
      }
   }
   m_breakableEntity = nullptr;
   m_breakableOrigin = nullptr;

   return nullptr;
}

void Bot::setIdealReactionTimers (bool actual) {
   if (cv_whose_your_daddy) {
      m_idealReactionTime = 0.05f;
      m_actualReactionTime = 0.095f;

      return; // zero out reaction times for extreme mode
   }
   const auto tweak = conf.getDifficultyTweaks (m_difficulty);

   if (actual) {
      m_idealReactionTime = tweak->reaction[0];
      m_actualReactionTime = tweak->reaction[0];

      return;
   }
   m_idealReactionTime = rg (tweak->reaction[0], tweak->reaction[1]);
}

void Bot::updatePickups () {
   // this function finds Items to collect or use in the near of a bot

   // utility to check if this function is currently doesn't allowed to run
   const auto isPickupBlocked = [&] () -> bool {
      // zombie or chickens not allowed to pickup anything
      if (m_isCreature) {
         return true;
      }

      // seeing enemy now, not good time to pickup anything
      else if (m_states & Sense::SeeingEnemy) {
         return true;
      }

      // bots on ladder don't have to search anything
      else if (isOnLadder ()) {
         return true;
      }

      // we're escaping from the bomb, don't bother!
      else if (getCurrentTaskId () == Task::EscapeFromBomb) {
         return true;
      }

      // knife mode is in progress ?
      else if (cv_jasonmode) {
         return true;
      }

      // no interesting entities, how ?
      else if (!bots.hasInterestingEntities ()) {
         return true;
      }
      return false;
   };

   // we're not allowed to run now
   if (isPickupBlocked ()) {
      m_pickupItem = nullptr;
      m_pickupType = Pickup::None;

      return;
   }

   const auto &interesting = bots.getInterestingEntities ();
   const float radius = cr::sqrf (cv_object_pickup_radius.as <float> ());

   if (!game.isNullEntity (m_pickupItem)) {
      bool itemExists = false;
      auto pickupItem = m_pickupItem;

      for (auto &ent : interesting) {

         // in the periods of updating interesting entities we can get fake ones, that already were picked up, so double check if drawn
         if (ent->v.effects & EF_NODRAW) {
            continue;
         }
         const Vector &origin = game.getEntityOrigin (ent);

         // too far from us ?
         if (pev->origin.distanceSq (origin) > radius) {
            continue;
         }

         if (ent == pickupItem) {
            if (seesItem (origin, ent->v.classname.str ())) {
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

   int32_t pickupType = Pickup::None;
   Vector pickupPos = nullptr;

   m_pickupItem = nullptr;
   m_pickupType = Pickup::None;

   for (const auto &ent : interesting) {
      bool allowPickup = false; // assume can't use it until known otherwise

      // get the entity origin
      const auto &origin = game.getEntityOrigin (ent);

      if ((ent->v.effects & EF_NODRAW) || ent == m_itemIgnore || cr::abs (origin.z - pev->origin.z) > 96.0f) {
         continue; // someone owns this weapon or it hasn't re-spawned yet
      }

      // too far from us ?
      if (pev->origin.distanceSq (origin) > radius) {
         continue;
      }

      auto classname = ent->v.classname.str ();
      auto model = ent->v.model.str (9);

      // check if line of sight to object is not blocked (i.e. visible)
      if (seesItem (origin, classname)) {
         const bool isWeaponBox = classname.startsWith ("weaponbox");

         const bool isDemolitionMap = game.mapIs (MapFlags::Demolition);
         const bool isHostageRescueMap = game.mapIs (MapFlags::HostageRescue);
         const bool isCSDM = game.is (GameFlags::CSDM);

         if (isHostageRescueMap && util.isHostageEntity (ent)) {
            allowPickup = true;
            pickupType = Pickup::Hostage;
         }
         else if (isDemolitionMap && isWeaponBox && model == "backpack.mdl") {
            allowPickup = true;
            pickupType = Pickup::DroppedC4;
         }
         else if ((isWeaponBox || classname.startsWith ("armoury_entity") || (isCSDM && classname.startsWith ("csdm")))
            && !m_isUsingGrenade) {

            allowPickup = true;
            pickupType = Pickup::Weapon;

            if (cv_pickup_ammo_and_kits) {
               const int primaryWeaponCarried = bestPrimaryCarried ();
               const int secondaryWeaponCarried = bestSecondaryCarried ();

               const auto &config = conf.getWeapons ();
               const auto &primary = config[primaryWeaponCarried];
               const auto &secondary = config[secondaryWeaponCarried];

               const auto &primaryProp = conf.getWeaponProp (primary.id);
               const auto &secondaryProp = conf.getWeaponProp (secondary.id);

               if (secondaryWeaponCarried < kPrimaryWeaponMinIndex
                  && (getAmmo (secondary.id) > 0.3 * secondaryProp.ammo1Max)
                  && model == "357ammobox.mdl") {

                  allowPickup = false;
               }
               else if (!m_isVIP &&
                  primaryWeaponCarried >= kPrimaryWeaponMinIndex
                  && (getAmmo (primary.id) > 0.3 * primaryProp.ammo1Max)
                  && !m_isUsingGrenade && !hasShield ()) {

                  auto weaponType = conf.getWeaponType (primary.id);

                  const bool isSniperRifle = weaponType == WeaponType::Sniper;
                  const bool isSubmachine = weaponType == WeaponType::SMG;
                  const bool isShotgun = weaponType == WeaponType::Shotgun;
                  const bool isRifle = weaponType == WeaponType::Rifle || weaponType == WeaponType::ZoomRifle;
                  const bool isHeavy = weaponType == WeaponType::Heavy;

                  if (!isRifle && model == "9mmarclip.mdl") {
                     allowPickup = false;
                  }
                  else if (!isShotgun && model == "shotbox.mdl") {
                     allowPickup = false;
                  }
                  else if (!isSubmachine && model == "9mmclip.mdl") {
                     allowPickup = false;
                  }
                  else if (!isSniperRifle && model == "crossbow_clip.mdl") {
                     allowPickup = false;
                  }
                  else if (!isHeavy && model == "chainammo.mdl") {
                     allowPickup = false;
                  }
               }
               else if (m_healthValue >= 100.0f && model == "medkit.mdl") {
                  allowPickup = false;
               }
               else if (pev->armorvalue >= 100.0f && (model == "kevlar.mdl" || model == "battery.mdl" || model == "assault.mdl")) {
                  allowPickup = false;
               }
               else if ((pev->weapons & cr::bit (Weapon::Flashbang)) && model == kFlashbangModelName) {
                  allowPickup = false;
               }
               else if ((pev->weapons & cr::bit (Weapon::Explosive)) && model == kExplosiveModelName) {
                  allowPickup = false;
               }
               else if ((pev->weapons & cr::bit (Weapon::Smoke)) && model == kSmokeModelName) {
                  allowPickup = false;
               }

               if (allowPickup) {
                  pickupType = Pickup::AmmoAndKits;
               }
            }

            // weapon replacement is not allowed
            if (!cv_pickup_best) {
               allowPickup = false;
               pickupType = Pickup::None;
            }
         }
         else if (classname.startsWith ("weapon_shield") && !m_isUsingGrenade) {
            allowPickup = true;
            pickupType = Pickup::Shield;

            // weapon replacement is not allowed
            if (!cv_pickup_best) {
               allowPickup = false;
               pickupType = Pickup::None;
            }
         }
         else if (isDemolitionMap && m_team == Team::CT && !m_hasDefuser && classname.startsWith ("item_thighpack")) {
            allowPickup = true;
            pickupType = Pickup::DefusalKit;
         }
         else if (isDemolitionMap && classname.startsWith ("grenade") && conf.getBombModelName () == model) {
            allowPickup = true;
            pickupType = Pickup::PlantedC4;
         }
         else if (cv_pickup_custom_items && util.isItem (ent) && !classname.startsWith ("item_thighpack")) {
            allowPickup = true;
            pickupType = Pickup::Items;
         }
      }

      // if the bot found something it can pickup...
      if (allowPickup) {

         // found weapon on ground?
         if (pickupType == Pickup::Weapon || pickupType == Pickup::AmmoAndKits) {
            if (m_isVIP) {
               allowPickup = false;
            }
            else if (!rateGroundWeapon (ent)) {
               allowPickup = false;

               // double check if it's ammo/kits
               if (pickupType == Pickup::AmmoAndKits) {
                  const auto &rawWeapons = conf.getWeapons ();

                  // verify that the model is not weapon
                  for (const auto &rw : rawWeapons) {
                     if (rw.model == model) {
                        allowPickup = false;
                        break;
                     }
                     allowPickup = true;
                  }
               }
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

               if (!m_defendHostage && m_personality
                  != Personality::Rusher && m_difficulty >= Difficulty::Normal
                  && rg.chance (15)
                  && m_timeCamping + 15.0f < game.time ()
                  && numFriendsNear (pev->origin, 384.0f) < 3) {

                  const int index = findDefendNode (origin);

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg (cv_camping_time_min.as <float> (), cv_camping_time_max.as <float> ()), true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg (3.0f, 6.0f), true); // push move command

                  // decide to duck or not to duck
                  selectCampButtons (index);
                  m_defendHostage = true;

                  pushChatterMessage (Chatter::GoingToGuardHostages); // play info about that
                  return;
               }
            }
            else if (pickupType == Pickup::PlantedC4) {
               allowPickup = false;

               if (!m_defendedBomb) {
                  m_defendedBomb = true;

                  const int index = findDefendNode (origin);
                  const Path &path = graph[index];

                  const float bombTimer = mp_c4timer.as <float> ();
                  const float timeMidBlowup = bots.getTimeBombPlanted () + (bombTimer * 0.5f + bombTimer * 0.25f) - graph.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                  if (timeMidBlowup > game.time ()) {
                     clearTask (Task::MoveToPosition); // remove any move tasks

                     startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, timeMidBlowup, true); // push camp task on to stack
                     startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, timeMidBlowup, true); // push  move command

                     // decide to duck or not to duck
                     selectCampButtons (index);

                     if (rg.chance (85) && numEnemiesNear (pev->origin, 768.0f) < 4) {
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
                     if (other->m_isAlive) {
                        for (const auto &hostage : other->m_hostages) {
                           if (hostage == ent) {
                              allowPickup = false;
                              break;
                           }
                        }
                     }
                  }

                  // don't steal hostage from human teammate (hack)
                  if (allowPickup) {
                     for (auto &client : util.getClients ()) {
                        if ((client.flags & ClientFlags::Used) && !(client.ent->v.flags & FL_FAKECLIENT) && (client.flags & ClientFlags::Alive) &&
                           client.team == m_team && client.ent->v.origin.distanceSq (ent->v.origin) <= cr::sqrf (240.0f)) {
                           allowPickup = false;
                           break;
                        }
                     }
                  }
               }
            }
            else if (pickupType == Pickup::PlantedC4) {
               if (util.isAlive (m_enemy)) {
                  return;
               }

               if (isOutOfBombTimer ()) {
                  completeTask ();

                  // then start escape from bomb immediate
                  startTask (Task::EscapeFromBomb, TaskPri::EscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
                  return;
               }

               if (rg.chance (70)) {
                  pushChatterMessage (Chatter::FoundC4Plant);
               }
               allowPickup = !isBombDefusing (origin) || m_hasProgressBar;

               if (!m_defendedBomb && !allowPickup) {
                  m_defendedBomb = true;

                  const int index = findDefendNode (origin);
                  const auto &path = graph[index];

                  const float timeToExplode = bots.getTimeBombPlanted () + mp_c4timer.as <float> () - graph.calculateTravelTime (pev->maxspeed, pev->origin, path.origin);

                  clearTask (Task::MoveToPosition); // remove any move tasks

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, timeToExplode, true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, timeToExplode, true); // push move command

                  // decide to duck or not to duck
                  selectCampButtons (index);

                  if (rg.chance (85)) {
                     pushChatterMessage (Chatter::DefendingBombsite);
                  }
               }

               if (pev->origin.distanceSq (origin) > cr::sqrf (60.0f)) {
                  if (!graph.isNodeReacheable (pev->origin, origin)) {
                     allowPickup = false;
                  }
               }
            }
            else if (pickupType == Pickup::DroppedC4) {
               m_itemIgnore = ent;
               allowPickup = false;

               if (!m_defendedBomb && m_difficulty >= Difficulty::Normal && rg.chance (75) && m_healthValue < 60) {
                  const int index = findDefendNode (origin);

                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg (cv_camping_time_min.as <float> (), cv_camping_time_max.as <float> ()), true); // push camp task on to stack
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg (10.0f, 30.0f), true); // push move command

                  // decide to duck or not to duck
                  selectCampButtons (index);
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
         if (other.get () != this && other->m_isAlive && other->m_pickupItem == pickupItem) {
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

void Bot::ensureEntitiesClear () {
   const auto tid = getCurrentTaskId ();

   if (tid == Task::PickupItem || (m_states & Sense::PickupItem)) {
      if (!game.isNullEntity (m_pickupItem) && !m_hasProgressBar) {
         m_itemIgnore = m_pickupItem; // clear these pointers, bot might be stuck getting to them
      }

      m_itemCheckTime = game.time () + 5.0f;
      m_pickupType = Pickup::None;
      m_pickupItem = nullptr;

      if (tid == Task::PickupItem) {
         completeTask ();
      }
   }
   else if (tid == Task::ShootBreakable) {
      if (!game.isNullEntity (m_breakableEntity)) {
         m_ignoredBreakable.emplace (m_breakableEntity);
      }

      m_breakableTime = game.time () + 5.0f;
      m_breakableOrigin = nullptr;
      m_lastBreakable = nullptr;
      m_breakableEntity = nullptr;

      completeTask ();
   }
   findValidNode ();
}

Vector Bot::getCampDirection (const Vector &dest) {
   // this function check if view on last enemy position is blocked - replace with better vector then
   // mostly used for getting a good camping direction vector if not camping on a camp node

   TraceResult tr {};
   const Vector &src = getEyesPos ();

   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      const float lengthSq = tr.vecEndPos.distanceSq (src);

      if (lengthSq > cr::sqrf (1024.0f)) {
         return nullptr;
      }

      const int enemyIndex = graph.getNearest (dest);
      const int tempIndex = graph.getNearest (pev->origin);

      if (tempIndex == kInvalidNodeIndex || enemyIndex == kInvalidNodeIndex) {
         return nullptr;
      }
      float nearestDistance = kInfiniteDistance;

      int lookAtNode = kInvalidNodeIndex;
      const Path &path = graph[tempIndex];

      for (auto &link : path.links) {
         if (link.index == kInvalidNodeIndex) {
            continue;
         }
         const auto distance = planner.dist (link.index, enemyIndex);

         if (distance < nearestDistance) {
            nearestDistance = distance;
            lookAtNode = link.index;
         }
      }

      if (graph.exists (lookAtNode)) {
         return graph[lookAtNode].origin;
      }
   }
   const auto dangerIndex = practice.getIndex (m_team, m_currentNodeIndex, m_currentNodeIndex);

   if (graph.exists (dangerIndex)) {
      return graph[dangerIndex].origin;
   }
   return nullptr;
}

void Bot::showChatterIcon (bool show, bool disconnect) {
   // this function depending on show boolean, shows/remove chatter, icon, on the head of bot.

   if (!game.is (GameFlags::HasBotVoice) || cv_radio_mode.as <int> () != 2) {
      return;
   }

   auto sendBotVoice = [] (bool on, edict_t *ent, int ownId) {
      MessageWriter (MSG_ONE, msgs.id (NetMsg::BotVoice), nullptr, ent) // begin message
         .writeByte (on) // switch on/off
         .writeByte (ownId);
   };
   const int ownIndex = index ();

   // do not respect timers while disconnecting bot
   for (auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }

      // dormants not receiving messages
      if (client.ent->v.flags & FL_DORMANT) {
         continue;
      }

      // do not respect timers while disconnecting bot
      if (!show && (client.iconFlags[ownIndex] & ClientFlags::Icon) && (disconnect || client.iconTimestamp[ownIndex] < game.time ())) {
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
   if (!game.is (GameFlags::HasBotVoice)
      || cv_radio_mode.as <int> () != 2
      || !conf.hasChatterBank (type)
      || !conf.hasChatterBank (Chatter::DiePain)) {

      return;
   }

   const auto &playbackSound = conf.pickRandomFromChatterBank (type);
   const auto &painSound = conf.pickRandomFromChatterBank (Chatter::DiePain);

   if (m_isAlive) {
      showChatterIcon (true);
   }
   MessageWriter msg {};
   const int ownIndex = index ();

   auto writeChatterSound = [&msg] (ChatterItem item) {
      msg.writeString (strings.format ("%s%s%s.wav", cv_chatter_path.as <StringRef> (), kPathSeparator, item.name));
   };

   for (auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || (client.ent->v.flags & FL_FAKECLIENT) || client.team != m_team) {
         continue;
      }
      msg.start (MSG_ONE, msgs.id (NetMsg::SendAudio), nullptr, client.ent); // begin message
      msg.writeByte (ownIndex);

      if (pev->deadflag & DEAD_DYING) {
         client.iconTimestamp[ownIndex] = game.time () + painSound.duration;
         writeChatterSound (painSound);
      }
      else if (m_isAlive) {
         client.iconTimestamp[ownIndex] = game.time () + playbackSound.duration;
         writeChatterSound (playbackSound);
      }
      msg.writeShort (m_voicePitch).end ();
      client.iconFlags[ownIndex] |= ClientFlags::Icon;
   }
}

void Bot::pushRadioMessage (int message) {
   // this function inserts the radio message into the message queue

   if (cv_radio_mode.as <int> () == 0 || m_numFriendsLeft == 0) {
      return;
   }
   m_forceRadio = !game.is (GameFlags::HasBotVoice)
      || !conf.hasChatterBank (message)
      || cv_radio_mode.as <int> () != 2; // use radio instead voice

   m_radioSelect = message;
   pushMsgQueue (BotMsg::Radio);
}

void Bot::pushChatterMessage (int message) {
   // this function inserts the voice message into the message queue (mostly same as above)

   if (!game.is (GameFlags::HasBotVoice) || cv_radio_mode.as <int> () != 2 || !conf.hasChatterBank (message) || m_numFriendsLeft == 0) {
      return;
   }
   bool sendMessage = false;

   const auto messageRepeat = conf.getChatterMessageRepeatInterval (message);
   auto &messageTimer = m_chatterTimes[message];

   if (messageTimer < game.time () || cr::fequal (messageTimer, kMaxChatterRepeatInterval)) {
      if (!cr::fequal (messageRepeat, kMaxChatterRepeatInterval)) {
         messageTimer = game.time () + messageRepeat;
      }
      sendMessage = true;
   }

   if (!sendMessage) {
      m_radioSelect = kInvalidRadioSlot;
      return;
   }
   m_radioSelect = message;
   pushMsgQueue (BotMsg::Radio);
}

void Bot::checkMsgQueue () {
   // this function checks and executes pending messages

   // no new message?
   if (m_msgQueue.empty ()) {
      return;
   }

   // get message from deque
   const auto state = m_msgQueue.popFront ();

   // nothing to do?
   if (state == BotMsg::None || (state == BotMsg::Radio && game.is (GameFlags::FreeForAll))) {
      return;
   }
   float delayResponseTime = 0.0f;

   switch (state) {
   case BotMsg::Buy: // general buy message

      // buy weapon
      if (m_nextBuyTime > game.time ()) {
         // keep sending message
         pushMsgQueue (BotMsg::Buy);
         return;
      }

      if (!m_inBuyZone || game.is (GameFlags::CSDM) || m_isCreature) {
         m_buyPending = true;
         m_buyingFinished = true;

         break;
      }

      m_buyPending = false;
      m_nextBuyTime = game.time () + rg (0.5f, 1.3f);

      // if freezetime is very low do not delay the buy process
      if (mp_freezetime.as <float> () <= 1.0f) {
         m_nextBuyTime = game.time ();
         m_ignoreBuyDelay = true;
      }

      // if bot buying is off then no need to buy
      if (!cv_botbuy) {
         m_buyState = BuyState::Done;
      }

      // if fun-mode no need to buy
      if (cv_jasonmode) {
         m_buyState = BuyState::Done;
         selectWeaponById (Weapon::Knife);
      }

      // prevent vip from buying
      if (m_isVIP) {
         m_buyState = BuyState::Done;
         m_pathType = FindPath::Fast;
      }

      // prevent terrorists from buying on es maps
      if (game.mapIs (MapFlags::Escape) && m_team == Team::Terrorist && !m_inBuyZone) {
         m_buyState = BuyState::Done;
      }

      // prevent teams from buying on fun maps
      if (game.mapIs (MapFlags::KnifeArena)) {
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
      delayResponseTime = rg (1.0f, 3.0f);

      // if last bot radio command (global) happened some a little time ago, delay response
      if (bots.getLastRadioTimestamp (m_team) + delayResponseTime < game.time ()) {

         // if same message like previous just do a yes/no
         if (m_radioSelect != Radio::RogerThat && m_radioSelect != Radio::Negative) {
            if (m_radioSelect == bots.getLastRadio (m_team) && bots.getLastRadioTimestamp (m_team) + delayResponseTime * 0.5f > game.time ()) {
               m_radioSelect = kInvalidRadioSlot;
            }
            else {
               if (m_radioSelect != Radio::ReportingIn) {
                  bots.setLastRadio (m_team, m_radioSelect);
               }
               else {
                  bots.setLastRadio (m_team, kInvalidRadioSlot);
               }

               for (const auto &bot : bots) {
                  if (pev != bot->pev && bot->m_team == m_team) {
                     bot->m_radioOrder = m_radioSelect;
                     bot->m_radioEntity = ent ();
                  }
               }
            }
         }

         if (m_radioSelect != kInvalidRadioSlot) {
            if ((m_radioSelect != Radio::ReportingIn && m_forceRadio)
               || cv_radio_mode.as <int> () != 2
               || !conf.hasChatterBank (m_radioSelect)
               || !game.is (GameFlags::HasBotVoice)) {

               auto radioSlot = m_radioSelect;

               if (m_radioSelect < Radio::GoGoGo) {
                  issueCommand ("radio1");
               }
               else if (m_radioSelect < Radio::RogerThat) {
                  radioSlot -= Radio::GoGoGo - 1;
                  issueCommand ("radio2");
               }
               else {
                  radioSlot -= Radio::RogerThat - 1;
                  issueCommand ("radio3");
               }

               // select correct menu item for this radio message
               issueCommand ("menuselect %d", radioSlot);
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

bool Bot::isWeaponRestricted (int wid) {
   // this function checks for weapon restrictions.

   auto val = cv_restricted_weapons.as <StringRef> ();

   if (val.empty ()) {
      return isWeaponRestrictedAMX (wid); // no banned weapons
   }
   const auto &bannedWeapons = val.split <String> (";");
   const auto &alias = util.weaponIdToAlias (wid);

   for (const auto &ban : bannedWeapons) {
      // check is this weapon is banned
      if (ban == alias) {
         return true;
      }
   }
   return isWeaponRestrictedAMX (wid);
}

bool Bot::isWeaponRestrictedAMX (int wid) {
   // this function checks restriction set by AMX Mod, this function code is courtesy of KWo.

   if (!game.is (GameFlags::Metamod)) {
      return false;
   }

   auto checkRestriction = [&wid] (StringRef cvar, const int *data) -> bool {
      auto restrictedWeapons = game.findCvar (cvar);

      if (restrictedWeapons.empty ()) {
         return false;
      }
      // find the weapon index
      const auto index = data[wid - 1];

      // validate index range
      if (index < 0 || index >= static_cast <int> (restrictedWeapons.length ())) {
         return false;
      }
      return restrictedWeapons[static_cast <size_t> (index)] != '0';
   };

   // check for weapon restrictions
   if (cr::bit (wid) & (kPrimaryWeaponMask | kSecondaryWeaponMask | Weapon::Shield)) {
      constexpr int kIds[] = { 4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11 };

      // verify restrictions
      return checkRestriction ("amx_restrweapons", kIds);
   }

   // check for equipment restrictions
   else {
      constexpr int kIds[] = { -1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1, 0, 1, 5 };

      // verify restrictions
      return checkRestriction ("amx_restrequipammo", kIds);
   }
}

bool Bot::canReplaceWeapon () {
   // this function determines currently owned primary weapon, and checks if bot has
   // enough money to buy more powerful weapon.

   const auto tab = conf.getRawWeapons ();

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

int Bot::pickBestWeapon (Array <int> &vec, int moneySave) {
   // this function picks best available weapon from random choice with money save

   if (vec.length () < 2) {
      return vec.first ();
   }
   const bool needMoreRandomWeapon = (m_personality == Personality::Careful) || (rg.chance (25) && m_personality == Personality::Normal);

   if (needMoreRandomWeapon) {
      auto buyFactor = (static_cast <float> (m_moneyAmount) - static_cast <float> (moneySave)) / (16000.0f - static_cast <float> (moneySave)) * 3.0f;

      if (buyFactor < 1.0f) {
         buyFactor = 1.0f;
      }
      // swap array values
      vec.reverse ();

      return vec[static_cast <int> (static_cast <float> (vec.length <int32_t> () - 1) * cr::log10 (rg (1.0f, cr::powf (10.0f, buyFactor))) / buyFactor + 0.5f)];
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
   const auto &tab = conf.getWeapons ();

   for (const auto &w : vec) {
      const auto &weapon = tab[w];

      // if we have enough money for weapon, buy it
      if (weapon.price + moneySave < m_moneyAmount + rg (50, 200) && rg.chance (chance)) {
         return w;
      }
   }
   return vec.random ();
}

void Bot::buyStuff () {
   // this function does all the work in selecting correct buy menus for most weapons/items

   WeaponInfo *selectedWeapon = nullptr;
   m_nextBuyTime = game.time ();

   if (!m_ignoreBuyDelay) {
      m_nextBuyTime += rg (0.3f, 0.5f);
   }

   int count = 0;
   Array <int32_t> choices {};

   // select the priority tab for this personality
   const int *pref = conf.getWeaponPrefs (m_personality) + kNumWeapons;
   const auto tab = conf.getRawWeapons ();

   const bool isPistolMode = tab[25].teamStandard == -1 && tab[3].teamStandard == 2;
   const bool teamHasGoodEconomics = bots.checkTeamEco (m_team);

   // do this, because xash engine is not capable to run all the features goldsrc, but we have cs 1.6 on it, so buy table must be the same
   const bool isOldGame = game.is (GameFlags::Legacy);

   const bool hasDefaultPistols = (pev->weapons & (cr::bit (Weapon::USP) | cr::bit (Weapon::Glock18)));
   const bool isFirstRound = m_moneyAmount == mp_startmoney.as <int> ();

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

            const int disrespectEconomicsPct = 100 - cv_economics_disrespect_percent.as <int> ();

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
                  if (m_moneyAmount > limit[EcoLimit::SmgCTGreater] + prostock && rg.chance (disrespectEconomicsPct)) {
                     ignoreWeapon = true;
                  }
                  break;
               }

               if (selectedWeapon->id == Weapon::Shield
                  && m_moneyAmount > limit[EcoLimit::ShieldGreater]
                  && rg.chance (disrespectEconomicsPct)) {

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
                  if (m_moneyAmount > limit[EcoLimit::SmgTEGreater] + prostock && rg.chance (disrespectEconomicsPct)) {
                     ignoreWeapon = true;
                  }
                  break;
               }
            }

            switch (selectedWeapon->id) {
            case Weapon::XM1014:
            case Weapon::M3:
               if (m_moneyAmount < limit[EcoLimit::ShotgunLess] && rg.chance (disrespectEconomicsPct)) {
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
               if (m_moneyAmount < limit[EcoLimit::HeavyLess] && rg.chance (85)) {
                  ignoreWeapon = true;

               }

               if (m_moneyAmount >= limit[EcoLimit::HeavyGreater]) {
                  ignoreWeapon = false;
               }
               break;
            }

            if (ignoreWeapon && tab[25].teamStandard == 1 && cv_economics_rounds) {
               continue;
            }

            // save money for grenade for example?
            moneySave = rg (500, 1000);

            if (bots.getLastWinner () == m_team) {
               moneySave = 0;
            }

            if (selectedWeapon->price <= (m_moneyAmount - moneySave)) {
               choices.emplace (*pref);
            }

         } while (count < kNumWeapons && choices.length () < 4);

         // found a desired weapon?
         if (!choices.empty ()) {
            selectedWeapon = &tab[pickBestWeapon (choices, moneySave)];
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
      if (pev->armorvalue < rg (50.0f, 80.0f)
         && teamHasGoodEconomics
         && (isPistolMode || (teamHasGoodEconomics && hasPrimaryWeapon ()))) {

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
      if (isPistolMode
         || (isFirstRound && hasDefaultPistols && rg.chance (60))
         || (hasDefaultPistols && bots.getLastWinner () == m_team && m_moneyAmount > rg (2000, 3000))
         || (hasPrimaryWeapon () && hasDefaultPistols && m_moneyAmount > rg (7500, 9000))) {

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

            if (selectedWeapon->price <= (m_moneyAmount - rg (100, 200))) {
               choices.emplace (*pref);
            }

         } while (count < kNumWeapons && choices.length () < 4);

         // found a desired weapon?
         if (!choices.empty ()) {
            selectedWeapon = &tab[pickBestWeapon (choices, rg (100, 200))];
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
      for (int i = 0; i < 7; ++i) {
         issueCommand ("buyammo%d", rg (1, 2)); // simulate human
      }

      // buy enough ammo
      if (hasPrimaryWeapon ()) {
         issueCommand ("buy;menuselect 6");
      }
      else {
         issueCommand ("buy;menuselect 7");
      }

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
      if (game.mapIs (MapFlags::Demolition)
         && m_team == Team::CT
         && rg.chance (80)
         && m_moneyAmount > 200
         && !isWeaponRestricted (Weapon::Defuser)) {

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
         const float skyColor = illum.getSkyColor ();
         const float lightLevel = m_path->light;

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
      m_agressionLevel -= 0.05f;
   }
   else {
      m_agressionLevel += 0.05f;
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
   const auto tid = getCurrentTaskId ();

   // check if we need to escape from bomb
   if (game.mapIs (MapFlags::Demolition)
      && bots.isBombPlanted ()
      && m_isAlive
      && tid != Task::EscapeFromBomb
      && tid != Task::Camp
      && isOutOfBombTimer ()) {

      completeTask (); // complete current task

      // then start escape from bomb immediate
      startTask (Task::EscapeFromBomb, TaskPri::EscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
   }

   // special handling, if we have a knife in our hands
   if (isKnifeMode () && (util.isPlayer (m_enemy) || (cv_attack_monsters && util.isMonster (m_enemy)))) {
      const float distance2d = pev->origin.distance2d (m_enemy->v.origin);

      // do nodes movement if enemy is not reachable with a knife
      if (distance2d > 250.0f && (m_states & Sense::SeeingEnemy)) {
         const int nearestToEnemyPoint = graph.getNearest (m_enemy->v.origin);

         if (nearestToEnemyPoint != kInvalidNodeIndex
            && nearestToEnemyPoint != m_currentNodeIndex
            && cr::abs (graph[nearestToEnemyPoint].origin.z - m_enemy->v.origin.z) < 16.0f) {

            if (tid != Task::MoveToPosition && !cr::fequal (getTask ()->desire, TaskPri::Hide)) {
               startTask (Task::MoveToPosition, TaskPri::Hide, nearestToEnemyPoint, game.time () + distance2d / (m_moveSpeed * 2.0f), true);
            }
            else {
               if (tid == Task::MoveToPosition && getTask ()->data != nearestToEnemyPoint) {
                  clearTask (Task::MoveToPosition);
                  startTask (Task::MoveToPosition, TaskPri::Hide, nearestToEnemyPoint, game.time () + distance2d / (m_moveSpeed * 2.0f), true);
               }
            }
         }
      }
      else {
         if (distance2d <= 250.0f && (m_states & Sense::SeeingEnemy) && tid == Task::MoveToPosition) {
            clearTask (Task::MoveToPosition); // remove any move tasks
         }
      }
   }

   // special handling for sniping
   if (usesSniper () && (m_states & (Sense::SeeingEnemy | Sense::SuspectEnemy))
      && m_shootTime - 0.4f <= game.time ()
      && m_sniperStopTime > game.time ()) {

      ignoreCollision ();

      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;

      m_navTimeset = game.time ();
   }

   // special handling for reloading
   if (!bots.isRoundOver () &&
      tid == Task::Normal
      && m_reloadState != Reload::None
      && m_isReloading
      && !isDucking ()
      && !isInNarrowPlace ()) {

      if (m_reloadState != Reload::None || m_isReloading) {
         const auto maxClip = conf.findWeaponById (m_currentWeapon).maxClip;
         const auto curClip = getAmmoInClip ();

         // consider not reloading if full ammo in clip
         if (curClip >= maxClip) {
            m_isReloading = false;
         }
      }

      if (m_seeEnemyTime + 2.5f < game.time () && (m_states & (Sense::SuspectEnemy | Sense::HearingEnemy))) {
         m_moveSpeed = m_fearLevel > m_agressionLevel ? 0.0f : getShiftSpeed ();
         m_navTimeset = game.time ();
      }
   }
}

void Bot::syncUpdatePredictedIndex () {
   auto wipePredict = [this] () {
      m_lastPredictIndex = kInvalidNodeIndex;
      m_lastPredictLength = kInfiniteDistanceLong;
   };

   if (!m_predictLock.tryLock ()) {
      return; // allow only single instance of search per-bot
   }
   ScopedUnlock <Mutex> unlock (m_predictLock);

   const auto lastEnemyOrigin = m_lastEnemyOrigin;
   const auto currentNodeIndex = m_currentNodeIndex;
   const auto &botOrigin = pev->origin;

   if (lastEnemyOrigin.empty () || !vistab.isReady () || !util.isAlive (m_lastEnemy)) {
      wipePredict ();
      return;
   }

   int destIndex = graph.getNearest (lastEnemyOrigin);
   int bestIndex = kInvalidNodeIndex;

   if (destIndex == kInvalidNodeIndex) {
      wipePredict ();
      return;
   }
   int pathLength = 0;

   auto result = planner.find (destIndex, currentNodeIndex, [&] (int index) {
      ++pathLength;

      if (vistab.visible (currentNodeIndex, index) && botOrigin.distanceSq (graph[index].origin) > cr::sqrf (256.0f)) {
         bestIndex = index;
         return false;
      }
      return true;
   });

   if (result && bestIndex != kInvalidNodeIndex) {
      m_lastPredictIndex = bestIndex;
      m_lastPredictLength = pathLength;

      return;
   }
   wipePredict ();
}

void Bot::updatePredictedIndex () {
   if (m_lastEnemyOrigin.empty ()) {
      return; // do not run task if no last enemy
   }

   worker.enqueue ([this] () {
      syncUpdatePredictedIndex ();
   });
}

void Bot::refreshEnemyPredict () {
   if (game.isNullEntity (m_enemy) && !game.isNullEntity (m_lastEnemy) && !m_lastEnemyOrigin.empty ()) {
      const auto distanceToLastEnemySq = m_lastEnemyOrigin.distanceSq (pev->origin);

      if (distanceToLastEnemySq > cr::sqrf (256.0f) && (distanceToLastEnemySq < cr::sqrf (2048.0f) || usesSniper ())) {
         m_aimFlags |= AimFlags::PredictPath;
      }
      const bool denyLastEnemy = pev->velocity.lengthSq2d () > 0.0f
         && distanceToLastEnemySq < cr::sqrf (256.0f)
         && m_shootTime + 1.5f > game.time ();

      if (!(m_aimFlags & (AimFlags::Enemy | AimFlags::PredictPath)) && !denyLastEnemy && seesEntity (m_lastEnemyOrigin, true)) {
         m_aimFlags |= AimFlags::LastEnemy;
      }
   }

   if (m_aimFlags & AimFlags::PredictPath) {
      updatePredictedIndex ();
   }
}

void Bot::setLastVictim (edict_t *ent) {
   m_lastVictim = ent;
   m_lastVictimOrigin = ent->v.origin;

   m_forgetLastVictimTimer.start (rg (1.0f, 2.0f));
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
            if (m_lastVictim->v.weapons & kSniperWeaponMask) {

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

         // if no more enemies found AND bomb planted, switch to knife to get to bomb place faster
         if (m_team == Team::CT && !usesKnife () && m_numEnemiesLeft == 0 && bots.isBombPlanted ()) {
            selectWeaponById (Weapon::Knife);
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

   m_numFriendsLeft = numFriendsNear (pev->origin, kInfiniteDistance);
   m_numEnemiesLeft = numEnemiesNear (pev->origin, kInfiniteDistance);

   // check if our current enemy is still valid
   if (!game.isNullEntity (m_lastEnemy)) {
      if (!util.isAlive (m_lastEnemy) && m_shootAtDeadTime < game.time ()) {
         m_lastEnemy = nullptr;
      }
   }
   else {
      m_lastEnemy = nullptr;
   }

   // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
   if (m_soundUpdateTime < game.time ()
      && m_blindTime < game.time ()
      && m_seeEnemyTime + 1.0f < game.time ()) {

      updateHearing ();
      m_soundUpdateTime = game.time () + 0.25f;
   }
   else if (m_heardSoundTime + 10.0f < game.time ()) {
      m_states &= ~Sense::HearingEnemy;

      // clear the last enemy pointers if time has passed or enemy far away
      if (!m_lastEnemyOrigin.empty ()) {
         const auto distanceSq = pev->origin.distanceSq (m_lastEnemyOrigin);

         if (distanceSq > cr::sqrf (2048.0f) || (game.isNullEntity (m_enemy) && m_seeEnemyTime + 10.0f < game.time ())) {
            m_lastEnemyOrigin = nullptr;
            m_lastEnemy = nullptr;

            m_aimFlags &= ~AimFlags::LastEnemy;
         }
      }
   }
   refreshEnemyPredict ();

   // check for grenades depending on difficulty
   if (rg.chance (cr::max (25, m_difficulty * 25)) && !m_isCreature) {
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
         filter[Task::PickupItem].desire = cr::max (50.0f, 500.0f - pev->origin.distance (game.getEntityOrigin (m_pickupItem)) * 0.2f);
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
      float retreatLevel = (100.0f - (m_healthValue > 70.0f ? 100.0f : m_healthValue)) * tempFear; // retreat level depends on bot health

      if (m_numEnemiesLeft > m_numFriendsLeft / 2 && m_retreatTime < game.time () && m_seeEnemyTime - rg (2.0f, 4.0f) < game.time ()) {
         float timeSeen = m_seeEnemyTime - game.time ();
         float timeHeard = m_heardSoundTime - game.time ();
         float ratio = 0.0f;

         m_retreatTime = game.time () + rg (1.0f, 4.0f);

         if (timeSeen > timeHeard) {
            timeSeen += 10.0f;
            ratio = timeSeen * 0.1f;
         }
         else {
            timeHeard += 10.0f;
            ratio = timeHeard * 0.1f;
         }
         const bool lowAmmo = isLowOnAmmo (m_currentWeapon, 0.18f);
         const bool sniping = m_sniperStopTime > game.time () && lowAmmo;

         if (bots.isBombPlanted () || m_isStuck || usesKnife ()) {
            ratio /= 3.0f; // reduce the seek cover desire if bomb is planted
         }
         else if (m_isVIP || m_isReloading || (sniping && usesSniper ())) {
            ratio *= 3.0f; // triple the seek cover desire if bot is VIP or reloading
         }
         else if (m_lastEnemyOrigin.distanceSq2d (pev->origin) < cr::sqrf (200.0f)) {
            ratio *= 3.0f;
         }
         else if (m_isCreature) {
            ratio = 0.0f;
         }
         else if (game.is (GameFlags::CSDM)) {
            ratio = 0.0f;
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
      if (getCurrentTaskId () != Task::EscapeFromBomb
         && game.isNullEntity (m_enemy)
         && !m_isVIP
         && bots.getRoundMidTime () < game.time ()
         && !m_hasHostage
         && !m_isUsingGrenade
         && m_currentNodeIndex != graph.getNearest (m_lastEnemyOrigin)
         && m_personality != Personality::Careful
         && !cv_ignore_enemies) {

         float desireLevel = 4096.0f - ((1.0f - tempAgression) * m_lastEnemyOrigin.distance (pev->origin));

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

   auto def = thresholdDesire (&filter[Task::Hunt], 60.0f, 0.0f); // don't allow hunting if desires 60<
   offensive = subsumeDesire (offensive, pickup); // if offensive task, don't allow picking up stuff

   auto sub = maxDesire (offensive, def); // default normal & careful tasks against offensive actions
   auto finalTask = subsumeDesire (&filter[Task::Blind], maxDesire (survive, sub)); // reason about fleeing instead

   if (!m_tasks.empty ()) {
      finalTask = maxDesire (finalTask, getTask ());
      startTask (finalTask->id, finalTask->desire, finalTask->data, finalTask->time, finalTask->resume); // push the final behavior in our task stack to carry out
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

   const auto tid = getCurrentTaskId ();

   // leader bot?
   if (m_isLeader && tid == Task::SeekCover) {
      updateTeamCommands (); // reorganize team if fleeing
   }

   if (tid == Task::Camp) {
      selectBestWeapon ();
   }

   // this is best place to handle some voice commands report team some info
   if (cv_radio_mode.as <int> () > 1) {
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

   if (cv_debug_goal.as <int> () != kInvalidNodeIndex) {
      m_chosenGoalIndex = cv_debug_goal.as <int> ();
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

bool Bot::isLowOnAmmo (const int id, const float factor) const {
   return static_cast <float> (m_ammoInClip[id]) < static_cast <float> (conf.findWeaponById (id).maxClip) * factor;
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
   if (m_enemy->v.origin.distanceSq (pev->origin) < cr::sqrf (256.0f) || (!usesKnife () && isInViewCone (m_enemy->v.origin))) {
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
      const auto lineDist = m_enemy->v.origin.distance (pev->origin);

      if (isEnemyNoticeable (lineDist)) {
         m_isEnemyReachable = true;
      }
      else {
         int ownIndex = m_currentNodeIndex;

         if (ownIndex == kInvalidNodeIndex) {
            ownIndex = findNearestNode ();
         }
         const auto enemyIndex = graph.getNearest (m_enemy->v.origin);
         const auto pathDist = planner.preciseDistance (ownIndex, enemyIndex);

         if (pathDist - lineDist > 112.0f || isOnLadder ()) {
            m_isEnemyReachable = false;
         }
         else {
            m_isEnemyReachable = true;
         }
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
   if (!(m_aimFlags & (AimFlags::LastEnemy | AimFlags::PredictPath)) || m_lastEnemyOrigin.empty () || game.isNullEntity (m_lastEnemy)) {
      return false;
   }
   return util.getConeDeviation (ent (), m_lastEnemyOrigin) >= 0.90f && isPenetrableObstacle (m_lastEnemyOrigin);
}

void Bot::checkRadioQueue () {
   // this function handling radio and reacting to it


   // don't allow bot listen you if bot is busy
   if (getCurrentTaskId () == Task::DefuseBomb || getCurrentTaskId () == Task::PlantBomb || m_hasHostage || m_hasC4) {
      m_radioOrder = 0;
      return;
   }
   float distanceSq = m_radioEntity->v.origin.distanceSq (pev->origin);

   switch (m_radioOrder) {
   case Radio::CoverMe:
   case Radio::FollowMe:
   case Radio::StickTogetherTeam:
   case Chatter::GoingToPlantBomb:
   case Chatter::CoverMe:
      // check if line of sight to object is not blocked (i.e. visible)
      if (seesEntity (m_radioEntity->v.origin) || m_radioOrder == Radio::StickTogetherTeam) {
         if (game.isNullEntity (m_targetEntity)
            && game.isNullEntity (m_enemy)
            && rg.chance (m_personality == Personality::Careful ? 80 : 20)) {

            int numFollowers = 0;

            // check if no more followers are allowed
            for (const auto &bot : bots) {
               if (bot->m_isAlive) {
                  if (bot->m_targetEntity == m_radioEntity) {
                     ++numFollowers;
                  }
               }
            }
            int allowedFollowers = cv_user_max_followers.as <int> ();

            if (m_radioEntity->v.weapons & cr::bit (Weapon::C4)) {
               allowedFollowers = 1;
            }

            if (numFollowers < allowedFollowers) {
               pushRadioMessage (Radio::RogerThat);
               m_targetEntity = m_radioEntity;

               // don't pause/camp/follow anymore
               const auto tid = getCurrentTaskId ();

               if (tid == Task::Pause || tid == Task::Camp) {
                  getTask ()->time = game.time ();
               }
               startTask (Task::FollowUser, TaskPri::FollowUser, kInvalidNodeIndex, 0.0f, true);
            }
            else if (numFollowers > allowedFollowers) {
               for (int i = 0; (i < game.maxClients () && numFollowers > allowedFollowers); ++i) {
                  auto bot = bots[i];

                  if (bot != nullptr) {
                     if (bot->m_isAlive) {
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
            startTask (Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.time () + rg (30.0f, 60.0f), false);
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

            if (rg.chance (45) && cv_radio_mode.as <int> () == 2) {
               pushChatterMessage (Chatter::OnMyWay);
            }
            else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.as <int> () != 2) {
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
      if (((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distanceSq < cr::sqrf (2048.0f) || !m_moveToC4)
         && rg.chance (50)
         && m_seeEnemyTime + 4.0f < game.time ()) {

         m_fearLevel -= 0.1f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }

         if (rg.chance (45) && cv_radio_mode.as <int> () == 2) {
            pushChatterMessage (Chatter::OnMyWay);
         }
         else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.as <int> () != 2 && rg.chance (50)) {
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
         if (rg.chance (45) && cv_radio_mode.as <int> () == 2) {
            pushRadioMessage (Radio::RogerThat);
         }
         else if (m_radioOrder == Radio::NeedBackup && cv_radio_mode.as <int> () != 2) {
            pushRadioMessage (Radio::RogerThat);
         }

         m_targetEntity = nullptr;
         m_fearLevel -= 0.2f;

         if (m_fearLevel < 0.0f) {
            m_fearLevel = 0.0f;
         }
      }
      else if ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distanceSq < cr::sqrf (2048.0f)) {
         const auto tid = getCurrentTaskId ();

         if (tid == Task::Pause || tid == Task::Camp) {
            m_fearLevel -= 0.2f;

            if (m_fearLevel < 0.0f) {
               m_fearLevel = 0.0f;
            }

            pushRadioMessage (Radio::RogerThat);
            // don't pause/camp anymore
            getTask ()->time = game.time ();

            m_targetEntity = nullptr;
            m_position = m_radioEntity->v.origin + m_radioEntity->v.v_angle.forward () * rg (1024.0f, 2048.0f);

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
      if (game.isNullEntity (m_enemy) && distanceSq < cr::sqrf (2048.0f) && bots.isBombPlanted () && m_team == Team::Terrorist) {
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
         selectWeaponById (Weapon::Knife);
         clearSearchNodes ();

         m_position = graph.getBombOrigin ();
         startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);

         pushRadioMessage (Radio::RogerThat);
      }
      break;

   case Radio::StormTheFront:
      if (((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distanceSq < cr::sqrf (1024.0f)) && rg.chance (50)) {
         pushRadioMessage (Radio::RogerThat);

         // don't pause/camp anymore
         const auto tid = getCurrentTaskId ();

         if (tid == Task::Pause || tid == Task::Camp) {
            getTask ()->time = game.time ();
         }
         m_targetEntity = nullptr;
         m_position = m_radioEntity->v.origin + m_radioEntity->v.v_angle.forward () * rg (1024.0f, 2048.0f);

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
      if ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distanceSq < cr::sqrf (1024.0f)) {
         m_fearLevel += 0.5f;

         if (m_fearLevel > 1.0f) {
            m_fearLevel = 1.0f;
         }
         m_agressionLevel -= 0.5f;

         if (m_agressionLevel < 0.0f) {
            m_agressionLevel = 0.0f;
         }
         if (getCurrentTaskId () == Task::Camp) {
            getTask ()->time += rg (10.0f, 15.0f);
         }
         else {
            // don't pause/camp anymore
            const auto tid = getCurrentTaskId ();

            if (tid == Task::Pause) {
               getTask ()->time = game.time ();
            }
            m_targetEntity = nullptr;
            m_seeEnemyTime = game.time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistanceSq = kInfiniteDistance;

               // take nearest enemy to ordering player
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team) {
                     continue;
                  }

                  auto enemy = client.ent;
                  const float currentDistanceSq = m_radioEntity->v.origin.distanceSq (enemy->v.origin);

                  if (currentDistanceSq < nearestDistanceSq) {
                     nearestDistanceSq = currentDistanceSq;

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
      if (game.getTeam (m_radioEntity) == Team::CT
         && m_team == Team::CT
         && util.isFakeClient (m_radioEntity)
         && bots.getPlantedBombSearchTimestamp () < game.time ()) {

         float nearestDistanceSq = kInfiniteDistance;
         int bombPoint = kInvalidNodeIndex;

         // find nearest bomb node to player
         for (auto &point : graph.m_goalPoints) {
            distanceSq = graph[point].origin.distanceSq (m_radioEntity->v.origin);

            if (distanceSq < nearestDistanceSq) {
               nearestDistanceSq = distanceSq;
               bombPoint = point;
            }
         }

         // mark this node as restricted point
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
      if (!m_isCreature && ((game.isNullEntity (m_enemy) && seesEntity (m_radioEntity->v.origin)) || distanceSq < cr::sqrf (1024.0f))) {
         pushRadioMessage (Radio::RogerThat);

         if (getCurrentTaskId () == Task::Camp) {
            getTask ()->time = game.time () + rg (30.0f, 60.0f);
         }
         else {
            // don't pause anymore
            const auto tid = getCurrentTaskId ();

            if (tid == Task::Pause) {
               getTask ()->time = game.time ();
            }

            m_targetEntity = nullptr;
            m_seeEnemyTime = game.time ();

            // if bot has no enemy
            if (m_lastEnemyOrigin.empty ()) {
               float nearestDistanceSq = kInfiniteDistance;

               // take nearest enemy to ordering player
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team) {
                     continue;
                  }

                  auto enemy = client.ent;
                  const float enemyDistanceSq = m_radioEntity->v.origin.distanceSq (enemy->v.origin);

                  if (enemyDistanceSq < nearestDistanceSq) {
                     nearestDistanceSq = enemyDistanceSq;

                     m_lastEnemy = enemy;
                     m_lastEnemyOrigin = enemy->v.origin;
                  }
               }
            }
            clearSearchNodes ();

            const int index = findDefendNode (m_radioEntity->v.origin);

            // push camp task on to stack
            startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.time () + rg (30.0f, 60.0f), true);

            // push move command
            startTask (Task::MoveToPosition, TaskPri::MoveToPosition, index, game.time () + rg (30.0f, 60.0f), true);

            // decide to duck or not to duck
            selectCampButtons (index);
         }
      }
      break;
   }
   m_radioOrder = 0; // radio command has been handled, reset
}

void Bot::tryHeadTowardRadioMessage () {
   const auto tid = getCurrentTaskId ();

   if (tid == Task::MoveToPosition || m_headedTime + 15.0f < game.time () || !util.isAlive (m_radioEntity) || m_hasC4) {
      return;
   }

   if ((util.isFakeClient (m_radioEntity)
      && rg.chance (25)
      && m_personality == Personality::Normal) || !(m_radioEntity->v.flags & FL_FAKECLIENT)) {

      if (tid == Task::Pause || tid == Task::Camp) {
         getTask ()->time = game.time ();
      }
      m_headedTime = game.time ();
      m_position = m_radioEntity->v.origin;

      clearSearchNodes ();
      startTask (Task::MoveToPosition, TaskPri::MoveToPosition, kInvalidNodeIndex, 0.0f, true);
   }
}

void Bot::checkParachute () {
   static ConVarRef parachute (conf.fetchCustom ("AMXParachuteCvar").chars ());

   // if no cvar or it's not enabled do not bother
   if (parachute.exists () && parachute.value () > 0.0f) {
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
   pev->flags |= FL_CLIENT | FL_FAKECLIENT; // restore fake client bit, just in case

   if (m_thinkDelay.time <= game.time ()) {
      update ();

      // delay next execution for thinking
      m_thinkDelay.time = game.time () + m_thinkDelay.interval;

      // run bot command on twice speed
      if (m_commandDelay.time <= game.time ()) {
         runMovement ();
         m_commandDelay.time = game.time () + m_commandDelay.interval;
      }
   }

   if (m_slowFrameTimestamp > game.time ()) {
      return;
   }

   if (bots.isBombPlanted () && m_team == Team::CT && m_isAlive) {
      const Vector &bombPosition = graph.getBombOrigin ();

      if (!m_hasProgressBar
         && getCurrentTaskId () != Task::EscapeFromBomb
         && pev->origin.distanceSq (bombPosition) < cr::sqrf (1540.0f)
         && !isBombDefusing (bombPosition)) {

         m_itemIgnore = nullptr;
         m_itemCheckTime = game.time ();

         clearTask (getCurrentTaskId ());
      }
   }

   checkSpawnConditions ();
   checkForChat ();
   checkBreakablesAround ();

   if (game.is (GameFlags::HasBotVoice)) {
      showChatterIcon (false); // end voice feedback
   }

   // kick the bot if stay time is over, the quota maintain will add new bot for us later
   if (cv_rotate_bots && m_stayTime < game.time ()) {
      m_kickedByRotation = true; // kicked by rotation, so not save bot name if save bot names is active

      kick ();
      return;
   }
   m_slowFrameTimestamp = game.time () + 0.5f;
}

void Bot::update () {
   const auto tid = getCurrentTaskId ();

   m_canChooseAimDirection = true;
   m_isAlive = util.isAlive (ent ());
   m_team = game.getTeam (ent ());
   m_healthValue = cr::clamp (pev->health, 0.0f, 100.0f);

   if (m_team == Team::Terrorist && game.mapIs (MapFlags::Demolition)) {
      m_hasC4 = !!(pev->weapons & cr::bit (Weapon::C4));

      if (m_hasC4 && (cv_ignore_objectives || cv_jasonmode)) {
         m_hasC4 = false;
      }
   }
   else if (m_team == Team::CT && game.mapIs (MapFlags::HostageRescue)) {
      m_hasHostage = hasHostage ();
   }
   m_isCreature = isCreature ();

   // is bot movement enabled
   bool botMovement = false;

   // for some unknown reason some bots have speed of 1.0 after respawn on csdm
   if (game.is (GameFlags::CSDM) && cr::fequal (pev->maxspeed, 1.0f) && tid == Task::Normal) {
      static ConVarRef sv_maxspeed ("sv_maxspeed");

      // reset max speed to max value, thus allowing bot movement
      pev->maxspeed = sv_maxspeed.value ();
   }

   // if the bot hasn't selected stuff to start the game yet, go do that...
   if (m_notStarted) {
      updateTeamJoin (); // select team & class
   }
   else if (!m_isAlive) {
      // we got a teamkiller? vote him away...
      if (m_voteKickIndex != m_lastVoteKick && cv_tkpunish) {
         issueCommand ("vote %d", m_voteKickIndex);
         m_lastVoteKick = m_voteKickIndex;

         // if bot tk punishment is enabled slay the tk
         if (cv_tkpunish.as <int> () != 2 || util.isFakeClient (game.entityOfIndex (m_voteKickIndex))) {
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
   else if (m_buyingFinished
      && !(pev->maxspeed < 10.0f && tid != Task::PlantBomb && tid != Task::DefuseBomb)
      && !cv_freeze_bots
      && !graph.hasChanged ()) {

      botMovement = true;
   }
   checkMsgQueue ();

   if (!m_isStale && botMovement) {
      logic (); // execute main code
   }
   else if (pev->maxspeed < 10.0f) {
      logicDuringFreezetime ();
   }
   else if (!botMovement) {
      resetMovement ();
   }
}

void Bot::logicDuringFreezetime () {
   pev->button = 0;

   if (m_changeViewTime > game.time ()) {
      return;
   }

   if (rg.chance (15) && m_jumpTime + rg (1.0f, 2.0f) < game.time ()) {
      pev->button |= IN_JUMP;
      m_jumpTime = game.time ();
   }
   static Array <Bot *> teammates {};
   teammates.clear ();

   for (const auto &bot : bots) {
      if (bot->m_isAlive && bot->m_team == m_team && bot.get () != this && seesEntity (bot->pev->origin)) {
         teammates.push (bot.get ());
      }
   }

   if (!teammates.empty ()) {
      auto bot = teammates.random ();

      if (bot) {
         m_lookAt = bot->pev->origin + bot->pev->view_ofs;
      }

      // good time too greet everyone
      if (m_needToSendWelcomeChat) {
         pushChatMessage (Chat::Hello);
         m_needToSendWelcomeChat = false;
      }
   }
   m_changeViewTime = game.time () + rg (1.25f, 2.0f);
}

void Bot::executeTasks () {
   // this is core function that handle task execution

   auto func = getTask ()->func;

   // run the current task
   (this->*func) ();
}

void Bot::checkSpawnConditions () {
   // this function is called instead of ai when buying finished, but freezetime is not yet left.

   // switch to knife if time to do this
   if (m_checkKnifeSwitch && m_buyingFinished && m_spawnTime + rg (5.0f, 7.5f) < game.time ()) {
      if (rg (1, 100) < 2 && cv_spraypaints) {
         startTask (Task::Spraypaint, TaskPri::Spraypaint, kInvalidNodeIndex, game.time () + 1.0f, false);
      }

      if (m_difficulty >= Difficulty::Normal
         && rg.chance (m_personality == Personality::Rusher ? 99 : 50)
         && !m_isReloading
         && game.mapIs (MapFlags::HostageRescue | MapFlags::Demolition | MapFlags::Escape | MapFlags::Assassination)) {

         if (isKnifeMode ()) {
            dropCurrentWeapon ();
         }
         else {
            selectWeaponById (Weapon::Knife);
         }
      }
      m_checkKnifeSwitch = false;

      if (rg.chance (cv_user_follow_percent.as <int> ()) && game.isNullEntity (m_targetEntity) && !m_isLeader && !m_hasC4 && rg.chance (50)) {
         decideFollowUser ();
      }
   }

   // check if we already switched weapon mode
   if (m_checkWeaponSwitch && m_buyingFinished && m_spawnTime + rg (3.0f, 4.5f) < game.time ()) {
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

   resetMovement ();

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
         movedDistance = m_prevOrigin.distance (pev->origin);
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
   if (cv_radio_mode.as <int> () == 2) {
      if ((m_states & Sense::SeeingEnemy) && !game.isNullEntity (m_enemy)) {
         int hasFriendNearby = numFriendsNear (pev->origin, 512.0f);

         if (!hasFriendNearby && rg.chance (45) && (m_enemy->v.weapons & cr::bit (Weapon::C4))) {
            pushChatterMessage (Chatter::SpotTheBomber);
         }
         else if (!hasFriendNearby && rg.chance (45) && m_team == Team::Terrorist && util.isPlayerVIP (m_enemy)) {
            pushChatterMessage (Chatter::VIPSpotted);
         }
         else if (!hasFriendNearby
            && rg.chance (50)
            && game.getTeam (m_enemy) != m_team
            && isGroupOfEnemies (m_enemy->v.origin, 2, 384.0f)) {

            pushChatterMessage (Chatter::ScaredEmotion);
         }
         else if (!hasFriendNearby
            && rg.chance (40)
            && (m_enemy->v.weapons & kSniperWeaponMask)) {

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

   // avoid flyings grenades, if needed
   if (cv_avoid_grenades && !m_isCreature) {
      avoidGrenades ();
   }
   m_isUsingGrenade = false;

   executeTasks (); // execute current task
   setAimDirection (); // choose aim direction
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
   const Vector &dirOld = m_destOrigin - (pev->origin + pev->velocity * m_frameInterval);
   const Vector &dirNormal = dirOld.normalize2d_apx ();

   m_moveAngles = dirOld.angles ();
   m_moveAngles.clampAngles ();
   m_moveAngles.x = -m_moveAngles.x; // invert for engine

   // do some overriding for special cases
   overrideConditions ();

   // allowed to move to a destination position?
   if (m_moveToGoal) {
      moveToGoal ();
   }

   // are we allowed to check blocking terrain (and react to it)?
   if (m_checkTerrain) {
      // check for breakables around bots movement direction
      checkBreakable (nullptr);

      doPlayerAvoidance (dirNormal);
      checkTerrain (movedDistance, dirNormal);
   }

   // if we have fallen from the place of move, the nearest point is allowed
   checkFall ();

   // check the darkness
   if (!m_isCreature && cv_check_darkness) {
      checkDarkness ();
   }

   // must avoid a grenade?
   if (m_needAvoidGrenade != 0) {
      // don't duck to get away faster
      pev->button &= ~IN_DUCK;

      m_moveSpeed = -pev->maxspeed;
      m_strafeSpeed = pev->maxspeed * static_cast <float> (m_needAvoidGrenade);
   }

   // ensure we're not stuck destroying/picking something
   if (m_navTimeset + getEstimatedNodeReachTime () + 1.0f < game.time () && !(m_states & Sense::SeeingEnemy) && m_moveToGoal) {
      ensureEntitiesClear ();
   }

   // check if need to use parachute
   checkParachute ();

   // display some debugging thingy to host entity
   if (cv_debug.as <int> () >= 1) {
      showDebugOverlay ();
   }

   // save the previous speed (for checking if stuck)
   m_prevSpeed = cr::abs (m_moveSpeed);
   m_prevVelocity = cr::abs (pev->velocity.length2d ());
   m_lastDamageType = -1; // reset damage
}

void Bot::spawned () {
   if (game.is (GameFlags::CSDM)) {
      newRound ();
      clearTasks ();
   }
}

void Bot::showDebugOverlay () {
   bool displayDebugOverlay = false;

   if (!graph.hasEditor ()) {
      return;
   }
   auto overlayEntity = graph.getEditor ();

   if (overlayEntity->v.iuser2 == entindex () && overlayEntity->v.origin.distanceSq (pev->origin) < cr::sqrf (256.0f)) {
      displayDebugOverlay = true;
   }

   if (!displayDebugOverlay && cv_debug.as <int> () >= 2) {
      Bot *nearest = nullptr;

      if (util.findNearestPlayer (reinterpret_cast <void **> (&nearest), overlayEntity, 128.0f, false, true, true, true) && nearest == this) {
         displayDebugOverlay = true;
      }
   }

   if (!displayDebugOverlay) {
      return;
   }
   static int index = kInvalidNodeIndex, goal = kInvalidNodeIndex, tid = 0;

   static HashMap <int32_t, StringRef> tasks {
      { Task::Normal, "Normal" },
      { Task::Pause, "Pause" },
      { Task::MoveToPosition, "Move" },
      { Task::FollowUser, "Follow" },
      { Task::PickupItem, "Pickup" },
      { Task::Camp, "Camp" },
      { Task::PlantBomb, "PlantBomb" },
      { Task::DefuseBomb, "DefuseBomb" },
      { Task::Attack, "Attack" },
      { Task::Hunt, "Hunt" },
      { Task::SeekCover, "SeekCover" },
      { Task::ThrowExplosive, "ThrowHE" },
      { Task::ThrowFlashbang, "ThrowFL" },
      { Task::ThrowSmoke, "ThrowSG" },
      { Task::DoubleJump, "DoubleJump" },
      { Task::EscapeFromBomb, "EscapeFromBomb" },
      { Task::ShootBreakable, "DestroyBreakable" },
      { Task::Hide, "Hide" },
      { Task::Blind, "Blind" },
      { Task::Spraypaint, "Spray" }
   };

   static HashMap <int32_t, StringRef> personalities {
      { Personality::Rusher, "Rusher" },
      { Personality::Normal, "Normal" },
      { Personality::Careful, "Careful" }
   };

   static HashMap <int32_t, StringRef> flags {
      { AimFlags::Nav, "Nav" },
      { AimFlags::Camp, "Camp" },
      { AimFlags::PredictPath, "Predict" },
      { AimFlags::LastEnemy, "LastEnemy" },
      { AimFlags::Entity, "Entity" },
      { AimFlags::Enemy, "Enemy" },
      { AimFlags::Grenade, "Grenade" },
      { AimFlags::Override, "Override" },
      { AimFlags::Danger, "Danger" },
   };

   auto boolValue = [] (const bool test) {
      return test ? "Yes" : "No";
   };

   if (m_tasks.empty ()) {
      return;
   }

   if (tid != getCurrentTaskId () || index != m_currentNodeIndex || goal != getTask ()->data || m_timeDebugUpdateTime < game.time ()) {
      tid = getCurrentTaskId ();
      goal = getTask ()->data;
      index = m_currentNodeIndex;

      String enemy = "(none)";

      if (!game.isNullEntity (m_enemy)) {
         enemy = m_enemy->v.netname.str ();
      }
      else if (!game.isNullEntity (m_lastEnemy)) {
         enemy.assignf ("%s (L)", m_lastEnemy->v.netname.str ());
      }
      String pickup = "(none)";

      if (!game.isNullEntity (m_pickupItem)) {
         pickup = m_pickupItem->v.classname.str ();
      }
      String aimFlags {};

      for (uint32_t i = 0u; i < 9u; ++i) {
         auto bit = cr::bit (i);

         if (m_aimFlags & bit) {
            aimFlags.appendf (" %s", flags[static_cast <int32_t> (bit)]);
         }
      }
      StringRef weapon = util.weaponIdToAlias (m_currentWeapon);
      StringRef debugData = strings.format (
         "\n\n\n\n\n\n%s (H:%.1f/A:%.1f)- Task: %d=%s Desire:%.02f\n"
         "Item: %s Clip: %d Ammo: %d%s Money: %d AimFlags: %s\n"
         "SP=%.02f SSP=%.02f I=%d CG=%d PG=%d G=%d T: %.02f MT: %d\n"
         "Enemy=%s Pickup=%s Type=%s Terrain=%s Stuck=%s\n",
         pev->netname.str (), m_healthValue, pev->armorvalue,
         tid, tasks[tid], getTask ()->desire, weapon, getAmmoInClip (),
         getAmmo (), m_isReloading ? " (R)" : "", m_moneyAmount, aimFlags.trim (),
         m_moveSpeed, m_strafeSpeed, index, m_chosenGoalIndex, m_prevGoalIndex, goal, m_navTimeset - game.time (),
         pev->movetype, enemy, pickup, personalities[m_personality], boolValue (m_checkTerrain),
         boolValue (m_isStuck));

      static hudtextparms_t textParams {};

      textParams.channel = 1;
      textParams.x = -1.0f;
      textParams.y = 0.0f;
      textParams.effect = 0;

      textParams.r1 = textParams.r2 = static_cast <uint8_t> (m_team == Team::CT ? 0 : 255);
      textParams.g1 = textParams.g2 = static_cast <uint8_t> (100);
      textParams.b1 = textParams.b2 = static_cast <uint8_t> (m_team != Team::CT ? 0 : 255);
      textParams.a1 = textParams.a2 = static_cast <uint8_t> (1);

      textParams.fadeinTime = 0.0f;
      textParams.fadeoutTime = 0.0f;
      textParams.holdTime = 0.5f;
      textParams.fxTime = 0.0f;

      game.sendHudMessage (overlayEntity, textParams, debugData);
      m_timeDebugUpdateTime = game.time () + 0.5f;
   }

   // green = destination origin
   // blue = ideal angles
   // red = view angles
   constexpr auto kArrowLifeTime = 1;

   game.drawLine (overlayEntity, getEyesPos (), m_destOrigin, 10, 0, { 0, 255, 0 }, 250, 5, kArrowLifeTime, DrawLine::Arrow);
   game.drawLine (overlayEntity, getEyesPos () - Vector (0.0f, 0.0f, 16.0f), getEyesPos () + m_idealAngles.forward () * 300.0f, 10, 0, { 0, 0, 255 }, 250, 5, kArrowLifeTime, DrawLine::Arrow);
   game.drawLine (overlayEntity, getEyesPos () - Vector (0.0f, 0.0f, 32.0f), getEyesPos () + pev->v_angle.forward () * 300.0f, 10, 0, { 255, 0, 0 }, 250, 5, kArrowLifeTime, DrawLine::Arrow);

   // now draw line from source to destination
   for (size_t i = 0; i < m_pathWalk.length () && i + 1 < m_pathWalk.length (); ++i) {
      game.drawLine (overlayEntity, graph[m_pathWalk.at (i)].origin, graph[m_pathWalk.at (i + 1)].origin, 15, 0, { 255, 100, 55 }, 200, 5, kArrowLifeTime, DrawLine::Arrow);
   }
}

bool Bot::hasHostage () {
   if (cv_ignore_objectives || game.mapIs (MapFlags::Demolition)) {
      return false;
   }

   for (auto &hostage : m_hostages) {
      if (!game.isNullEntity (hostage)) {

         // don't care about dead hostages
         if (hostage->v.health <= 0.0f || pev->origin.distanceSq (hostage->v.origin) > cr::sqrf (600.0f)) {
            hostage = nullptr;
            continue;
         }
         return true;
      }
   }
   return false;
}

void Bot::takeDamage (edict_t *inflictor, int damage, int armor, int bits) {
   // this function gets called from the network message handler, when bot's gets hurt from any
   // other player.

   m_lastDamageType = bits;

   if (!game.is (GameFlags::CSDM)) {
      updatePracticeValue (damage);
   }

   if (util.isPlayer (inflictor) || (cv_attack_monsters && util.isMonster (inflictor))) {
      const auto inflictorTeam = game.getTeam (inflictor);

      if (!util.isMonster (inflictor) && cv_tkpunish && inflictorTeam == m_team && !util.isFakeClient (inflictor)) {
         // alright, die you team killer!!!
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

         if (game.isNullEntity (m_enemy) && m_team != inflictorTeam) {
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
         findNextBestNode ();
      }
   }
}

void Bot::takeBlind (int alpha) {
   // this function gets called by network message handler, when screenfade message get's send
   // it's used to make bot blind from the grenade.

   m_viewDistance = rg (10.0f, 20.0f);

   // do not take in effect some unique map effects on round start
   if (bots.getRoundStartTime () + 5.0f < game.time ()) {
      m_viewDistance = m_maxViewDistance;
   }
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
   m_blindNodeIndex = findCoverNode (512.0f);
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
   // nodes for a destination goal used for pathfinding

   if (graph.length () < 1 || graph.hasChanged () || m_chosenGoalIndex < 0 || m_prevGoalIndex < 0) {
      return;
   }
   const auto health = static_cast <int> (m_healthValue);

   // max goal value
   constexpr int kMaxGoalValue = PracticeLimit::Goal;

   // only rate goal node if bot died because of the damage
   // FIXME: could be done a lot better, however this cares most about damage done by sniping or really deadly weapons
   if (health - damage <= 0) {
      practice.setValue (m_team, m_chosenGoalIndex, m_prevGoalIndex, cr::clamp (practice.getValue (m_team, m_chosenGoalIndex, m_prevGoalIndex) - health / 20, -kMaxGoalValue, kMaxGoalValue));
   }
}

void Bot::updatePracticeDamage (edict_t *attacker, int damage) {
   // this function gets called each time a bot gets damaged by some enemy. stores the damage (team-specific) done by victim.

   if (!util.isPlayer (attacker)) {
      return;
   }

   const int attackerTeam = game.getTeam (attacker);
   const int victimTeam = m_team;

   if (attackerTeam == victimTeam) {
      return;
   }
   constexpr int kMaxDamageValue = PracticeLimit::Damage;

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
         practice.setDamage (victimIndex, victimIndex, victimIndex, cr::clamp (practice.getDamage (victimTeam, victimIndex, victimIndex), 0, kMaxDamageValue));
      }
   }
   const auto updateDamage = util.isFakeClient (attacker) ? 10 : 7;

   // store away the damage done
   const auto damageValue = cr::clamp (practice.getDamage (m_team, victimIndex, attackerIndex) + damage / updateDamage, 0, kMaxDamageValue);

   if (damageValue > practice.getHighestDamageForTeam (m_team)) {
      practice.setHighestDamageForTeam (m_team, damageValue);
   }
   practice.setDamage (m_team, victimIndex, attackerIndex, damageValue);
}

void Bot::pushChatMessage (int type, bool isTeamSay) {
   if (!conf.hasChatBank (type) || !cv_chat) {
      return;
   }

   prepareChatMessage (conf.pickRandomFromChatBank (type));
   pushMsgQueue (isTeamSay ? BotMsg::SayTeam : BotMsg::Say);
}

void Bot::dropWeaponForUser (edict_t *user, bool discardC4) {
   // this function, asks bot to discard his current primary weapon (or c4) to the user that requested it with /drop*
   // command, very useful, when i'm don't have money to buy anything... )

   if (util.isAlive (user) && m_moneyAmount >= 2000 && hasPrimaryWeapon () && user->v.origin.distanceSq (pev->origin) <= cr::sqrf (450.0f)) {
      m_aimFlags |= AimFlags::Entity;
      m_lookAt = user->v.origin;

      if (discardC4 && m_hasC4) {
         selectWeaponById (Weapon::C4);
         dropCurrentWeapon ();
      }
      else if (!discardC4) {
         selectBestWeapon ();
         dropCurrentWeapon ();
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
   sendToChat (strings.format ("Ok %s, i will help you!", ent->v.netname.str ()), true);
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

void Bot::debugMsgInternal (StringRef str) {
   if (game.isDedicated ()) {
      return;
   }
   const int level = cv_debug.as <int> ();

   if (level <= 2) {
      return;
   }
   String printBuf {};
   printBuf.assignf ("%s: %s", pev->netname.str (), str);

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

Vector Bot::isBombAudible () {
   // this function checks if bomb is can be heard by the bot, calculations done by manual testing.

   if (!bots.isBombPlanted () || getCurrentTaskId () == Task::EscapeFromBomb) {
      return nullptr; // reliability check
   }

   if (m_difficulty > Difficulty::Hard) {
      return graph.getBombOrigin ();
   }
   const Vector &bombOrigin = graph.getBombOrigin ();

   const float timeElapsed = ((game.time () - bots.getTimeBombPlanted ()) / mp_c4timer.as <float> ()) * 100.0f;
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
   if (cr::sqrf (desiredRadius) < pev->origin.distanceSq2d (bombOrigin)) {
      return bombOrigin;
   }
   return nullptr;
}

bool Bot::canRunHeavyWeight () {
   constexpr auto kInterval = 1.0f / 10.0f;

   if (m_heavyTimestamp + kInterval < game.time ()) {
      m_heavyTimestamp = game.time ();

      return true;
   }
   return false;
}

uint8_t Bot::computeMsec () {
   // estimate msec to use for this command based on time passed from the previous command

   return static_cast <uint8_t> (cr::min (static_cast <int32_t> (cr::roundf ((game.time () - m_previousThinkTime) * 1000.0f)), 255));
}

const Vector &Bot::getRpmAngles () {
   // get angles to pass to run player move function

   if (!m_approachingLadderTimer.elapsed () || getCurrentTaskId () == Task::Attack) {
      return pev->v_angle;
   }
   return m_moveAngles;
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
   // movement. That's why if you get a pfnRunPlayerMove for one bot that lasts a little too
   // short in comparison with the frame's duration, the remaining time until the frame
   // elapses, that bot will behave like a ghost : no movement, but bullets and players can
   // pass through it. Then, when the next frame will begin, the stucking problem will arise !

   m_frameInterval = game.time () - m_previousThinkTime;

   const auto msecVal = computeMsec ();
   m_previousThinkTime = game.time ();

   // translate bot buttons
   translateInput ();

   engfuncs.pfnRunPlayerMove (pev->pContainingEntity,
      getRpmAngles (), m_moveSpeed, m_strafeSpeed,
      0.0f, static_cast <uint16_t> (pev->button), static_cast <uint8_t> (pev->impulse), msecVal);

   // save our own copy of old buttons, since bot bot code is not running every frame now
   m_oldButtons = pev->button;
}

float Bot::getBombTimeleft () const {
   if (!bots.isBombPlanted ()) {
      return 0.0f;
   }
   return cr::max (bots.getTimeBombPlanted () + mp_c4timer.as <float> () - game.time (), 0.0f);
}

bool Bot::isOutOfBombTimer () {
   if (!game.mapIs (MapFlags::Demolition)) {
      return false;
   }

   if (m_currentNodeIndex == kInvalidNodeIndex || (m_hasProgressBar || getCurrentTaskId () == Task::EscapeFromBomb)) {
      return false; // if CT bot already start defusing, or already escaping, return false
   }

   // calculate left time
   const float timeLeft = getBombTimeleft ();

   // if time left greater than 13, no need to do other checks
   if (timeLeft > 13.0f) {
      return false;
   }
   const auto &bombOrigin = graph.getBombOrigin ();

   // for terrorist, if timer is lower than 13 seconds, return true
   if (timeLeft < 13.0f && m_team == Team::Terrorist && bombOrigin.distanceSq (pev->origin) < cr::sqrf (964.0f)) {
      return true;
   }
   bool hasTeammatesWithDefuserKit = false;

   // check if our teammates has defusal kit
   for (const auto &bot : bots) {
      // search players with defuse kit
      if (bot.get () != this && bot->m_team == Team::CT && bot->m_hasDefuser && bombOrigin.distanceSq (bot->pev->origin) < cr::sqrf (512.0f)) {
         hasTeammatesWithDefuserKit = true;
         break;
      }
   }

   // add reach time to left time
   const float reachTime = graph.calculateTravelTime (pev->maxspeed, m_pathOrigin, bombOrigin);

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
   if (game.is (GameFlags::FreeForAll) || m_enemyIgnoreTimer > game.time () || cv_ignore_enemies) {
      return;
   }
   m_hearedEnemy = nullptr;
   float nearestDistanceSq = kInfiniteDistance;

   // setup potential visibility set from engine
   auto set = game.getVisibilitySet (this, false);

   // loop through all enemy clients to check for hearable stuff
   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used)
         || !(client.flags & ClientFlags::Alive)
         || client.ent == ent ()
         || client.team == m_team
         || !client.ent
         || client.noise.last < game.time ()) {

         continue;
      }

      if (!game.checkVisibility (client.ent, set)) {
         continue;
      }
      const float distanceSq = client.noise.pos.distanceSq (pev->origin);

      if (distanceSq > cr::sqrf (client.noise.dist)) {
         continue;
      }

      if (distanceSq < nearestDistanceSq) {
         m_hearedEnemy = client.ent;
         nearestDistanceSq = distanceSq;
      }
   }

   // did the bot hear someone ?
   if (util.isPlayer (m_hearedEnemy)) {
      // change to best weapon if heard something
      if (m_shootTime < game.time () - 5.0f
         && isOnFloor ()
         && m_currentWeapon != Weapon::C4
         && m_currentWeapon != Weapon::Explosive
         && m_currentWeapon != Weapon::Smoke
         && m_currentWeapon != Weapon::Flashbang
         && !isKnifeMode ()) {

         selectBestWeapon ();
      }

      m_heardSoundTime = game.time ();
      m_states |= Sense::HearingEnemy;

      if (rg.chance (15) && game.isNullEntity (m_enemy) && game.isNullEntity (m_lastEnemy) && m_seeEnemyTime + 7.0f < game.time ()) {
         pushChatterMessage (Chatter::HeardTheEnemy);
      }

      auto getHeardOriginWithError = [&] () -> Vector {
         auto error = kSprayDistance * cr::powf (nearestDistanceSq, 0.5f) / 2048.0f;
         auto origin = m_hearedEnemy->v.origin;

         origin.x = origin.x + rg (-error, error);
         origin.y = origin.y + rg (-error, error);

         return origin;
      };

      // didn't bot already have an enemy ? take this one...
      if (m_lastEnemyOrigin.empty () || game.isNullEntity (m_lastEnemy)) {
         m_lastEnemy = m_hearedEnemy;
         m_lastEnemyOrigin = getHeardOriginWithError ();
      }

      // bot had an enemy, check if it's the heard one
      else {
         if (m_hearedEnemy == m_lastEnemy) {
            // bot sees enemy ? then bail out !
            if (m_states & Sense::SeeingEnemy) {
               return;
            }
            m_lastEnemyOrigin = getHeardOriginWithError ();
         }
         else {
            // if bot had an enemy but the heard one is nearer, take it instead
            const float distanceSq = m_lastEnemyOrigin.distanceSq (pev->origin);

            if (distanceSq > m_hearedEnemy->v.origin.distanceSq (pev->origin) && m_seeEnemyTime + 2.0f < game.time ()) {
               m_lastEnemy = m_hearedEnemy;
               m_lastEnemyOrigin = getHeardOriginWithError ();
            }
            else {
               return;
            }
         }
      }

      // check if heard enemy can be seen
      if (checkBodyParts (m_hearedEnemy)) {
         m_enemy = m_hearedEnemy;
         m_lastEnemy = m_hearedEnemy;
         m_lastEnemyOrigin = m_enemyOrigin;

         m_states |= Sense::SeeingEnemy;
         m_seeEnemyTime = game.time ();
      }

      // check if heard enemy can be shoot through some obstacle
      else {
         if (cv_shoots_thru_walls
            && m_lastEnemy == m_hearedEnemy
            && rg.chance (conf.getDifficultyTweaks (m_difficulty)->hearThruPct)
            && m_seeEnemyTime + 3.0f > game.time ()
            && isPenetrableObstacle (m_hearedEnemy->v.origin)) {

            m_enemy = m_hearedEnemy;
            m_lastEnemy = m_hearedEnemy;
            m_enemyOrigin = m_hearedEnemy->v.origin;
            m_lastEnemyOrigin = m_hearedEnemy->v.origin;

            m_states |= (Sense::SeeingEnemy | Sense::SuspectEnemy);
            m_seeEnemyTime = game.time ();
         }
      }
   }
}

void Bot::enteredBuyZone (int buyState) {
   // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

   if (m_isCreature) {
      return; // creatures can't buy anything
   }
   const int *econLimit = conf.getEconLimit ();

   // if bot is in buy zone, try to buy ammo for this weapon...
   if (m_seeEnemyTime + 12.0f < game.time ()
      && m_lastEquipTime + 15.0f < game.time ()
      && m_inBuyZone
      && (bots.getRoundStartTime () + rg (10.0f, 20.0f) + mp_buytime.as <float> () < game.time ())
      && !bots.isBombPlanted ()
      && m_moneyAmount > econLimit[EcoLimit::PrimaryGreater]) {

      m_ignoreBuyDelay = true;
      m_buyingFinished = false;
      m_buyState = buyState;

      // push buy message
      pushMsgQueue (BotMsg::Buy);

      m_nextBuyTime = game.time ();
      m_lastEquipTime = game.time ();
   }
}

void Bot::selectCampButtons (int index) {
   const auto &path = graph[index];

   if (m_personality == Personality::Rusher || pev->health >= 90.0f) {
      if (path.vis.crouch < path.vis.stand && m_fearLevel > m_agressionLevel) {
         m_campButtons |= IN_DUCK;
      }
      else {
         m_campButtons &= ~IN_DUCK;
      }
   }
   else {
      if (path.vis.crouch <= path.vis.stand) {
         m_campButtons |= IN_DUCK;
      }
      else {
         m_campButtons &= ~IN_DUCK;
      }
   }
}

bool Bot::isBombDefusing (const Vector &bombOrigin) {
   // this function finds if somebody currently defusing the bomb.

   if (!bots.isBombPlanted ()) {
      return false;
   }
   bool defusingInProgress = false;
   constexpr auto kDistanceToBomb = cr::sqrf (165.0f);

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive)) {
         continue;
      }

      auto bot = bots[client.ent];
      const auto bombDistanceSq = client.origin.distanceSq (bombOrigin);

      if (bot && !bot->m_isAlive) {
         if (m_team != bot->m_team || bot->getCurrentTaskId () == Task::EscapeFromBomb) {
            continue; // skip other mess
         }

         // if close enough, mark as progressing
         if (bombDistanceSq < kDistanceToBomb && (bot->getCurrentTaskId () == Task::DefuseBomb || bot->m_hasProgressBar)) {
            defusingInProgress = true;
            break;
         }
         continue;
      }

      // take in account peoples too
      if (client.team == m_team) {

         // if close enough, mark as progressing
         if (bombDistanceSq < kDistanceToBomb && ((client.ent->v.button | client.ent->v.oldbuttons) & IN_USE)) {
            defusingInProgress = true;
            break;
         }
         continue;
      }
   }
   return defusingInProgress;
}

float Bot::getShiftSpeed () {
   if (getCurrentTaskId () == Task::SeekCover
      || (m_aimFlags & AimFlags::Enemy)
      || isDucking ()
      || (pev->button & IN_DUCK)
      || (m_oldButtons & IN_DUCK)
      || (m_currentTravelFlags & PathFlag::Jump)
      || (m_pathFlags & NodeFlag::Ladder)
      || isOnLadder ()
      || isInWater ()
      || isKnifeMode ()
      || m_isStuck
      || m_numEnemiesLeft <= 0
      || !m_lostReachableNodeTimer.elapsed ()) {

      return pev->maxspeed;
   }
   return pev->maxspeed * 0.4f;
}

void Bot::refreshModelName (char *infobuffer) {
   if (infobuffer == nullptr) {
      infobuffer = engfuncs.pfnGetInfoKeyBuffer (ent ());
   }
   StringRef modelName = engfuncs.pfnInfoKeyValue (infobuffer, "model");

   // need at least two characters to test model mask
   if (modelName.length () < 2) {
      m_modelMask = 0;
      return;
   }
   union ModelTest {
      char model[2];
      uint16_t mask;
      ModelTest (StringRef m) : model { m[0], m[1] } {}
   } modelTest { modelName };

   // assign our model mask (tests against model done every bot update)
   m_modelMask = modelTest.mask;
}

bool Bot::isCreature () {
   // current creature models are: zombie, chicken
   constexpr auto kModelMaskZombie = (('o' << 8) + 'z');
   constexpr auto kModelMaskChicken = (('h' << 8) + 'c');

   return m_modelMask == kModelMaskZombie || m_modelMask == kModelMaskChicken;
}
