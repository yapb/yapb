//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_shoots_thru_walls ("yb_shoots_thru_walls", "2", "Specifies whether bots able to fire at enemies behind the wall, if they hearing or suspecting them.", true, 0.0f, 3.0f);
ConVar cv_ignore_enemies ("yb_ignore_enemies", "0", "Enables or disables searching world for enemies.");
ConVar cv_check_enemy_rendering ("yb_check_enemy_rendering", "0", "Enables or disables checking enemy rendering flags. Useful for some mods.");
ConVar cv_check_enemy_invincibility ("yb_check_enemy_invincibility", "0", "Enables or disables checking enemy invincibility. Useful for some mods.");
ConVar cv_stab_close_enemies ("yb_stab_close_enemies", "1", "Enables or disables bot ability to stab the enemy with knife if bot is in good condition.");

ConVar mp_friendlyfire ("mp_friendlyfire", nullptr, Var::GameRef);
ConVar sv_gravity ("sv_gravity", nullptr, Var::GameRef);

int Bot::numFriendsNear (const Vector &origin, float radius) {
   int count = 0;

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team != m_team || client.ent == ent ()) {
         continue;
      }

      if (client.origin.distanceSq (origin) < cr::square (radius)) {
         count++;
      }
   }
   return count;
}

int Bot::numEnemiesNear (const Vector &origin, float radius) {
   int count = 0;

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team) {
         continue;
      }

      if (client.origin.distanceSq (origin) < cr::square (radius)) {
         count++;
      }
   }
   return count;
}

bool Bot::isEnemyHidden (edict_t *enemy) {
   if (!cv_check_enemy_rendering.bool_ () || game.isNullEntity (enemy)) {
      return false;
   }
   entvars_t &v = enemy->v;

   bool enemyHasGun = (v.weapons & kPrimaryWeaponMask) || (v.weapons & kSecondaryWeaponMask);
   bool enemyGunfire = (v.button & IN_ATTACK) || (v.oldbuttons & IN_ATTACK);

   if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && (!enemyGunfire || !enemyHasGun)) {
      return true;
   }

   if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && enemyGunfire && enemyHasGun) {
      return false;
   }

   if (v.renderfx != kRenderFxHologram && v.renderfx != kRenderFxExplode && v.rendermode != kRenderNormal) {
      if (v.renderfx == kRenderFxGlowShell) {
         if (v.renderamt <= 20.0f && v.rendercolor.x <= 20.0f && v.rendercolor.y <= 20.0f && v.rendercolor.z <= 20.0f) {
            if (!enemyGunfire || !enemyHasGun) {
               return true;
            }
            return false;
         }
         else if (!enemyGunfire && v.renderamt <= 60.0f && v.rendercolor.x <= 60.f && v.rendercolor.y <= 60.0f && v.rendercolor.z <= 60.0f) {
            return true;
         }
      }
      else if (v.renderamt <= 20.0f) {
         if (!enemyGunfire || !enemyHasGun) {
            return true;
         }
         return false;
      }
      else if (!enemyGunfire && v.renderamt <= 60.0f) {
         return true;
      }
   }
   return false;
}

bool Bot::isEnemyInvincible (edict_t *enemy) {
   if (!cv_check_enemy_invincibility.bool_ () || game.isNullEntity (enemy)) {
      return false;
   }
   const entvars_t &v = enemy->v;

   if (v.solid < SOLID_BBOX) {
      return true;
   }

   if (v.flags & FL_GODMODE) {
      return true;
   }

   if (cr::fequal (v.takedamage, DAMAGE_NO)) {
      return true;
   }

   return false;
}

bool Bot::isEnemyNoTarget (edict_t *enemy) {
   if (game.isNullEntity (enemy)) {
      return false;
   }
   return !!(enemy->v.flags & FL_NOTARGET);
}

bool Bot::checkBodyParts (edict_t *target) {
   // this function checks visibility of a bot target.

   if (isEnemyHidden (target) || isEnemyInvincible (target) || isEnemyNoTarget (target)) {
      m_enemyParts = Visibility::None;
      m_enemyOrigin = nullptr;

      return false;
   }

   TraceResult result {};
   auto eyes = getEyesPos ();

   auto spot = target->v.origin;
   auto self = pev->pContainingEntity;

   m_enemyParts = Visibility::None;
   game.testLine (eyes, spot, TraceIgnore::Everything, self, &result);

   if (result.flFraction >= 1.0f) {
      m_enemyParts |= Visibility::Body;
      m_enemyOrigin = result.vecEndPos;
   }

   // check top of head
   spot.z += 25.0f;
   game.testLine (eyes, spot, TraceIgnore::Everything, self, &result);

   if (result.flFraction >= 1.0f) {
      m_enemyParts |= Visibility::Head;
      m_enemyOrigin = result.vecEndPos;
   }

   if (m_enemyParts != Visibility::None) {
      return true;
   }

   constexpr auto standFeet = 34.0f;
   constexpr auto crouchFeet = 14.0f;

   if (target->v.flags & FL_DUCKING) {
      spot.z = target->v.origin.z - crouchFeet;
   }
   else {
      spot.z = target->v.origin.z - standFeet;
   }
   game.testLineChannel (TraceChannel::Enemy, eyes, spot, TraceIgnore::Everything, self, result);

   if (result.flFraction >= 1.0f) {
      m_enemyParts |= Visibility::Other;
      m_enemyOrigin = result.vecEndPos;

      return true;
   }

   const float edgeOffset = 13.0f;
   Vector dir = (target->v.origin - pev->origin).normalize2d ();

   Vector perp (-dir.y, dir.x, 0.0f);
   spot = target->v.origin + Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   game.testLineChannel (TraceChannel::Enemy, eyes, spot, TraceIgnore::Everything, self, result);

   if (result.flFraction >= 1.0f) {
      m_enemyParts |= Visibility::Other;
      m_enemyOrigin = result.vecEndPos;

      return true;
   }
   spot = target->v.origin - Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   game.testLineChannel (TraceChannel::Enemy, eyes, spot, TraceIgnore::Everything, self, result);

   if (result.flFraction >= 1.0f) {
      m_enemyParts |= Visibility::Other;
      m_enemyOrigin = result.vecEndPos;

      return true;
   }
   return false;
}

bool Bot::seesEnemy (edict_t *player, bool ignoreFOV) {
   if (game.isNullEntity (player)) {
      return false;
   }

   if ((ignoreFOV || isInViewCone (player->v.origin)) && isEnemyInFrustum (player) && checkBodyParts (player)) {
      m_seeEnemyTime = game.time ();
      m_lastEnemy = player;
      m_lastEnemyOrigin = m_enemyOrigin;

      return true;
   }
   return false;
}

void Bot::trackEnemies () {
   if (lookupEnemies ()) {
      m_states |= Sense::SeeingEnemy;
   }
   else {
      m_states &= ~Sense::SeeingEnemy;
      m_enemy = nullptr;
   }
}

bool Bot::lookupEnemies () {
   // this function tries to find the best suitable enemy for the bot

   // do not search for enemies while we're blinded, or shooting disabled by user
   if (m_enemyIgnoreTimer > game.time () || m_blindTime > game.time () || cv_ignore_enemies.bool_ ()) {
      return false;
   }
   edict_t *player, *newEnemy = nullptr;
   float nearestDistance = cr::square (m_viewDistance);

   // clear suspected flag
   if (!game.isNullEntity (m_enemy) && (m_states & Sense::SeeingEnemy)) {
      m_states &= ~Sense::SuspectEnemy;
   }
   else if (game.isNullEntity (m_enemy) && m_seeEnemyTime + 1.0f > game.time () && util.isAlive (m_lastEnemy)) {
      m_states |= Sense::SuspectEnemy;
      m_aimFlags |= AimFlags::LastEnemy;
   }
   m_enemyParts = Visibility::None;
   m_enemyOrigin = nullptr;

   if (!game.isNullEntity (m_enemy)) {
      player = m_enemy;

      // is player is alive
      if (m_enemyUpdateTime > game.time () && m_enemy->v.origin.distanceSq (pev->origin) < nearestDistance && util.isAlive (player) && seesEnemy (player)) {
         newEnemy = player;
      }
   }

   // the old enemy is no longer visible or
   if (game.isNullEntity (newEnemy)) {
      auto set = game.getVisibilitySet (this, true); // setup potential visibility set from engine

      // ignore shielded enemies, while we have real one
      edict_t *shieldEnemy = nullptr;

      if (cv_attack_monsters.bool_ ()) {
         // search the world for monsters...
         for (const auto &intresting : bots.getIntrestingEntities ()) {
            if (!util.isMonster (intresting)) {
               continue;
            }

            // check the engine PVS
            if (!isEnemyInFrustum (intresting) || !game.checkVisibility (intresting, set)) {
               continue;
            }

            // see if bot can see the monster...
            if (seesEnemy (intresting)) {
               // higher priority for big monsters
               float scaleFactor = (1.0f / calculateScaleFactor (intresting));
               float distance = intresting->v.origin.distanceSq (pev->origin) * scaleFactor;

               if (distance < nearestDistance) {
                  nearestDistance = distance;
                  newEnemy = intresting;
               }
            }
         }
      }

      // search the world for players...
      for (const auto &client : util.getClients ()) {
         if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team == m_team || client.ent == ent () || !client.ent) {
            continue;
         }
         player = client.ent;

         // check the engine PVS
         if (!isEnemyInFrustum (player) || !game.checkVisibility (player, set)) {
            continue;
         }

         // extra skill player can see thru smoke... if beeing attacked
         if ((player->v.button & (IN_ATTACK | IN_ATTACK2)) && m_viewDistance < m_maxViewDistance && m_difficulty == Difficulty::Expert) {
            nearestDistance = cr::square (m_maxViewDistance);
         }

         // see if bot can see the player...
         if (seesEnemy (player)) {
            if (isEnemyBehindShield (player)) {
               shieldEnemy = player;
               continue;
            }
            float distance = player->v.origin.distanceSq (pev->origin);

            if (distance < nearestDistance) {
               nearestDistance = distance;
               newEnemy = player;

               // aim VIP first on AS maps...
               if (game.is (MapFlags::Assassination) && util.isPlayerVIP (newEnemy)) {
                  break;
               }
            }
         }
      }
      m_enemyUpdateTime = game.time () + (usesKnife () ? 1.0f : 0.75f);
      
      if (game.isNullEntity (newEnemy) && !game.isNullEntity (shieldEnemy)) {
         newEnemy = shieldEnemy;
      }
   }

   if (newEnemy != nullptr && (util.isPlayer (newEnemy) || (cv_attack_monsters.bool_ () && util.isMonster (newEnemy)))) {
      bots.setCanPause (true);

      m_aimFlags |= AimFlags::Enemy;
      m_states |= Sense::SeeingEnemy;

      // if enemy is still visible and in field of view, keep it keep track of when we last saw an enemy
      if (newEnemy == m_enemy) {
         m_seeEnemyTime = game.time ();

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
      else {
         if (m_seeEnemyTime + 3.0f < game.time () && (m_hasC4 || hasHostage () || !game.isNullEntity (m_targetEntity))) {
            pushRadioMessage (Radio::EnemySpotted);
         }
         m_targetEntity = nullptr; // stop following when we see an enemy...
         m_enemySurpriseTime = game.time () + m_actualReactionTime;

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;
         m_enemyReachableTimer = 0.0f;

         // keep track of when we last saw an enemy
         m_seeEnemyTime = game.time ();

         if (!(m_oldButtons & IN_ATTACK)) {
            return true;
         }

         // now alarm all teammates who see this bot & don't have an actual enemy of the bots enemy should simulate human players seeing a teammate firing
         for (const auto &other : bots) {
            if (!other->m_notKilled || other->m_team != m_team || other.get () == this) {
               continue;
            }

            if (other->m_seeEnemyTime + 2.0f < game.time () && game.isNullEntity (other->m_lastEnemy) && util.isVisible (pev->origin, other->ent ()) && other->isInViewCone (pev->origin)) {
               other->m_lastEnemy = newEnemy;
               other->m_lastEnemyOrigin = m_lastEnemyOrigin;
               other->m_seeEnemyTime = game.time ();
               other->m_states |= (Sense::SuspectEnemy | Sense::HearingEnemy);
               other->m_aimFlags |= AimFlags::LastEnemy;
            }
         }
         return true;
      }
   }
   else if (!game.isNullEntity (m_enemy)) {
      newEnemy = m_enemy;
      m_lastEnemy = newEnemy;

      if (!util.isAlive (newEnemy)) {
         m_enemy = nullptr;

         // shoot at dying players if no new enemy to give some more human-like illusion
         if (m_seeEnemyTime + 0.1f > game.time ()) {
            if (!usesSniper ()) {
               m_shootAtDeadTime = game.time () + cr::clamp (m_agressionLevel * 1.25f, 0.25f, 0.45f);
               m_actualReactionTime = 0.0f;
               m_states |= Sense::SuspectEnemy;

               return true;
            }
            return false;
         }

         else if (m_shootAtDeadTime > game.time ()) {
            m_actualReactionTime = 0.0f;
            m_states |= Sense::SuspectEnemy;

            return true;
         }
         return false;
      }

      // if no enemy visible check if last one shoot able through wall
      if (cv_shoots_thru_walls.bool_ () && rg.chance (conf.getDifficultyTweaks (m_difficulty)->seenThruPct) && m_difficulty >= Difficulty::Normal && isPenetrableObstacle (newEnemy->v.origin)) {
         m_seeEnemyTime = game.time ();

         m_states |= Sense::SuspectEnemy;
         m_aimFlags |= AimFlags::LastEnemy;

         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
   }

   // check if bots should reload...
   if ((m_aimFlags <= AimFlags::PredictPath && m_seeEnemyTime + 3.0f < game.time () && !(m_states & (Sense::SeeingEnemy | Sense::HearingEnemy)) && game.isNullEntity (m_lastEnemy) && game.isNullEntity (m_enemy) && getCurrentTaskId () != Task::ShootBreakable && getCurrentTaskId () != Task::PlantBomb && getCurrentTaskId () != Task::DefuseBomb) || bots.isRoundOver ()) {
      if (!m_reloadState) {
         m_reloadState = Reload::Primary;
      }
   }

   // is the bot using a sniper rifle or a zoomable rifle?
   if ((usesSniper () || usesZoomableRifle ()) && m_zoomCheckTime + 1.0f < game.time ()) {
      if (pev->fov < 90.0f) {
         pev->button |= IN_ATTACK2;
      }
      else {
         m_zoomCheckTime = 0.0f;
      }
   }
   return false;
}

Vector Bot::getBodyOffsetError (float distance) {
   if (game.isNullEntity (m_enemy)) {
      return nullptr;
   }

   if (m_aimErrorTime < game.time ()) {
      const float hitError = distance / (cr::clamp (m_difficulty, 1, 4) * 1000.0f);
      auto &maxs = m_enemy->v.maxs, &mins = m_enemy->v.mins;

      m_aimLastError = Vector (rg.get (mins.x * hitError, maxs.x * hitError), rg.get (mins.y * hitError, maxs.y * hitError), rg.get (mins.z * hitError, maxs.z * hitError));

      auto &aimError = conf.getDifficultyTweaks (m_difficulty) ->aimError;
      m_aimLastError += Vector (rg.get (-aimError.x, aimError.x), rg.get (-aimError.y, aimError.y), rg.get (-aimError.z, aimError.z));

      m_aimErrorTime = game.time () + rg.get (1.5f, 2.0f);
   }
   return m_aimLastError;
}

 Vector Bot::getEnemyBodyOffset () {
   // the purpose of this function, is to make bot aiming not so ideal. it's mutate m_enemyOrigin enemy vector
   // returned from visibility check function.

   // if no visibility data, use last one
   if (!m_enemyParts) {
      return m_enemyOrigin;
   }
   float distance = m_enemy->v.origin.distance (pev->origin);

   // do not aim at head, at long distance (only if not using sniper weapon)
   if ((m_enemyParts & Visibility::Body) && !usesSniper () && distance > (m_difficulty >= Difficulty::Normal ? 2000.0f : 1000.0f)) {
      m_enemyParts &= ~Visibility::Head;
   }

   // do not aim at head while close enough to enemy and having sniper
   else if (distance < 800.0f && usesSniper ()) {
      m_enemyParts &= ~Visibility::Head;
   }

   Vector spot = m_enemy->v.origin;
   Vector compensation = nullptr;

   if (!usesSniper () && !usesKnife () && distance > kSprayDistance) {
      compensation = 1.0f * m_frameInterval * m_enemy->v.velocity - 1.0f * m_frameInterval * pev->velocity;
      compensation.z = 0.0f;
   }
   else {
      compensation = nullptr;
   }

   // if we only suspect an enemy behind a wall take the worst skill
   if (!m_enemyParts && (m_states & Sense::SuspectEnemy)) {
      spot += getBodyOffsetError (distance);
   }
   else if (util.isPlayer (m_enemy)) {
      // now take in account different parts of enemy body
      if (m_enemyParts & (Visibility::Head | Visibility::Body)) {
         auto headshotPct = conf.getDifficultyTweaks (m_difficulty)->headshotPct;

         // now check is our skill match to aim at head, else aim at enemy body
         if (rg.chance (headshotPct)) {
            spot = m_enemyOrigin + getCustomHeight (distance);
         }
         else {
            spot = m_enemy->v.origin;
            spot.z -= 3.0f;
         }
      }
      else if (m_enemyParts & Visibility::Body) {
         spot = m_enemy->v.origin;
      }
      else if (m_enemyParts & Visibility::Other) {
         spot = m_enemyOrigin;
      }
      else if (m_enemyParts & Visibility::Head) {
         spot = m_enemyOrigin + getCustomHeight (distance);
      }
   }
   Vector newSpot = spot;

   if (m_difficulty < Difficulty::Expert && isEnemyInSight (newSpot)) {
      spot = newSpot + ((spot - newSpot) * 0.01f); // gradually adjust the aiming direction
   }
   spot += compensation;

   if (usesKnife () && m_difficulty >= Difficulty::Normal) {
      spot = m_enemyOrigin;
   }
   m_lastEnemyOrigin = spot;

   // add some error to unskilled bots
   if (m_difficulty < Difficulty::Hard) {
      spot += getBodyOffsetError (distance);
   }
   return spot;
}

Vector Bot::getCustomHeight (float distance) {
   enum DistanceIndex {
      Long, Middle, Short
   };

   static constexpr float offsetRanges[9][3] = {
      { 0.0f,  0.0f,   0.0f }, // none
      { 0.0f,  0.0f,   0.0f }, // melee
      { 0.5f, -3.0f,  -4.5f }, // pistol
      { 6.5f,  6.0f,  -2.0f }, // shotgun
      { 0.5f  -7.5f,  -9.5f }, // zoomrifle
      { 0.5f, -7.5f,  -9.5f }, // rifle
      { 0.5f, -7.5f,  -9.5f }, // smg
      { 0.0f, -2.5f,  -6.0f }, // sniper
      { 1.5f, -4.0f,  -9.0f }  // heavy
   };

   // only highskilled bots do that 
   if (m_difficulty != Difficulty::Expert || (m_enemy->v.flags & FL_DUCKING)) {
      return 0.0f;
   }

   // default distance index is short
   auto distanceIndex = DistanceIndex::Short;

   // set distance index appropriate to distance
   if (distance < 2048.0f && distance > kDoubleSprayDistance) {
      distanceIndex = DistanceIndex::Long;
   }
   else if (distance > kSprayDistance && distance <= kDoubleSprayDistance) {
      distanceIndex = DistanceIndex::Middle;
   }
   return { 0.0f, 0.0f, offsetRanges[m_weaponType][distanceIndex] };
}

bool Bot::isFriendInLineOfFire (float distance) {
   // bot can't hurt teammates, if friendly fire is not enabled...
   if (!mp_friendlyfire.bool_ () || game.is (GameFlags::CSDM)) {
      return false;
   }

   TraceResult tr {};
   game.testLine (getEyesPos (), getEyesPos () + distance * pev->v_angle.normalize (), TraceIgnore::None, ent (), &tr);

   // check if we hit something
   if (util.isPlayer (tr.pHit) && tr.pHit != ent ()) {
      auto hit = tr.pHit;

      // check valid range
      if (game.getTeam (hit) == m_team && util.isAlive (hit)) {
         return true;
      }
   }

   // doesn't seems to be have any effect
#if 0
   // search the world for players
   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team != m_team || client.ent == ent ()) {
         continue;
      }
      auto friendDistance = client.ent->v.origin.distanceSq (pev->origin);

      if (friendDistance <= distance && util.getShootingCone (ent (), client.ent->v.origin) > friendDistance / (friendDistance + 1089.0f)) {
         return true;
      }
   }
#endif
   return false;
}

bool Bot::isPenetrableObstacle (const Vector &dest) {
   // this function returns true if enemy can be shoot through some obstacle, false otherwise.
   // credits goes to Immortal_BLG

   auto method = cv_shoots_thru_walls.int_ ();

   if (method == 2) {
      return isPenetrableObstacle2 (dest);
   }
   else if (method == 3) {
      return isPenetrableObstacle3 (dest);
   }

   if (m_isUsingGrenade || m_difficulty < Difficulty::Normal) {
      return false;
   }
   auto power = conf.findWeaponById (m_currentWeapon).penetratePower;

   if (power == 0) {
      return false;
   }
   TraceResult tr {};

   float obstacleDistance = 0.0f;
   game.testLine (getEyesPos (), dest, TraceIgnore::Monsters, ent (), &tr);

   if (tr.fStartSolid) {
      const Vector &source = tr.vecEndPos;
      game.testLine (dest, source, TraceIgnore::Monsters, ent (), &tr);

      if (!cr::fequal (tr.flFraction, 1.0f)) {
         if (tr.vecEndPos.distanceSq (dest) > cr::square (800.0f)) {
            return false;
         }

         if (tr.vecEndPos.z >= dest.z + 200.0f) {
            return false;
         }
         obstacleDistance = tr.vecEndPos.distanceSq (source);
      }
   }
   const float distance = cr::square (75.0f);

   if (obstacleDistance > 0.0f) {
      while (power > 0) {
         if (obstacleDistance > distance) {
            obstacleDistance -= distance;
            power--;

            continue;
         }
         return true;
      }
   }
   return false;
}

bool Bot::isPenetrableObstacle2 (const Vector &dest) {
   // this function returns if enemy can be shoot through some obstacle

   if (m_isUsingGrenade || m_difficulty < Difficulty::Normal || !conf.findWeaponById (m_currentWeapon).penetratePower) {
      return false;
   }

   const Vector &source = getEyesPos ();
   const Vector &direction = (dest - source).normalize (); // 1 unit long

   int thikness = 0;
   int numHits = 0;

   Vector point;
   TraceResult tr {};

   game.testLine (source, dest, TraceIgnore::Everything, ent (), &tr);

   while (!cr::fequal (tr.flFraction, 1.0f) && numHits < 3) {
      numHits++;
      thikness++;

      point = tr.vecEndPos + direction;

      while (engfuncs.pfnPointContents (point) == CONTENTS_SOLID && thikness < 98) {
         point = point + direction;
         thikness++;
      }
      game.testLine (point, dest, TraceIgnore::Everything, ent (), &tr);
   }

   if (numHits < 3 && thikness < 98) {
      if (dest.distanceSq (point) < cr::square (112.0f)) {
         return true;
      }
   }
   return false;
}

bool Bot::isPenetrableObstacle3 (const Vector &dest) {
   // this function returns if enemy can be shoot through some obstacle

   if (m_isUsingGrenade || m_difficulty < Difficulty::Normal || !conf.findWeaponById (m_currentWeapon).penetratePower) {
      return false;
   }
   auto power = conf.findWeaponById (m_currentWeapon).penetratePower;

   if (power == 0) {
      return false;
   }
   TraceResult tr {};

   Vector source = getEyesPos ();
   const auto &dir = (dest - source).normalize () * 8.0f;

   for (;;) {
      game.testLine (source, dest, TraceIgnore::Monsters, ent (), &tr);

      if (tr.fStartSolid) {
         if (tr.fAllSolid) {
            return false;
         }
         source += dir;
      }
      else {
         // check if line hit anything
         if (cr::fequal (tr.flFraction, 1.0f)) {
            return true;
         }

         if (--power == 0) {
            return false;
         }
         source = tr.vecEndPos + dir;
      }
   }
}

bool Bot::needToPauseFiring (float distance) {
   // returns true if bot needs to pause between firing to compensate for punchangle & weapon spread

   if (usesSniper () || m_isUsingGrenade) {
      return false;
   }

   if (m_firePause > game.time ()) {
      return true;
   }

   if ((m_aimFlags & AimFlags::Enemy) && !m_enemyOrigin.empty ()) {
      if (util.getShootingCone (ent (), m_enemyOrigin) > 0.92f && isEnemyBehindShield (m_enemy)) {
         return true;
      }
   }
   float offset = 4.25f;

   if (distance < kSprayDistance) {
      return false;
   }
   else if (distance < kDoubleSprayDistance) {
      offset = 2.75f;
   }
   const float xPunch = cr::square (cr::deg2rad (pev->punchangle.x));
   const float yPunch = cr::square (cr::deg2rad (pev->punchangle.y));

   const float interval = m_frameInterval;
   const float tolerance = (100.0f - static_cast <float> (m_difficulty) * 25.0f) / 99.0f;
   const float baseTime = distance > kDoubleSprayDistance ? 0.65f : 0.48f;
   const float maxRecoil = static_cast <float> (conf.getDifficultyTweaks (m_difficulty)->maxRecoil);

   // check if we need to compensate recoil
   if (cr::tanf (cr::sqrtf (cr::abs (xPunch) + cr::abs (yPunch))) * distance > offset + maxRecoil + tolerance) {
      if (m_firePause < game.time ()) {
         m_firePause = game.time () + rg.get (baseTime, baseTime + maxRecoil * 0.01f * tolerance) - interval;
      }
      return true;
   }
   return false;
}

void Bot::selectWeapons (float distance, int index, int id, int choosen) {
   auto tab = conf.getRawWeapons ();

   // we want to fire weapon, don't reload now
   if (!m_isReloading) {
      m_reloadState = Reload::None;
      m_reloadCheckTime = game.time () + 3.0f;
   }

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id) {
      selectWeaponById (id);

      // reset burst fire variables
      m_firePause = 0.0f;
      m_timeLastFired = 0.0f;

      return;
   }

   if (tab[choosen].id != id) {
      choosen = 0;

      // loop through all the weapons until terminator is found...
      while (tab[choosen].id) {
         if (tab[choosen].id == id) {
            break;
         }
         choosen++;
      }
   }

   // if we're have a glock or famas vary burst fire mode
   checkBurstMode (distance);

   if (hasShield () && m_shieldCheckTime < game.time () && getCurrentTaskId () != Task::Camp) // better shield gun usage
   {
      if (distance >= 750.0f && !isShieldDrawn ()) {
         pev->button |= IN_ATTACK2; // draw the shield
      }
      else if (isShieldDrawn () || (!game.isNullEntity (m_enemy) && ((m_enemy->v.button & IN_RELOAD) || !seesEntity (m_enemy->v.origin)))) {
         pev->button |= IN_ATTACK2; // draw out the shield
      }
      m_shieldCheckTime = game.time () + 1.0f;
   }

   // is the bot holding a sniper rifle?
   if (usesSniper () && m_zoomCheckTime < game.time ()) {
      // should the bot switch to the long-range zoom?
      if (distance > 1500.0f && pev->fov >= 40.0f) {
         pev->button |= IN_ATTACK2;
      }

      // else should the bot switch to the close-range zoom ?
      else if (distance > 150.0f && pev->fov >= 90.0f) {
         pev->button |= IN_ATTACK2;
      }

      // else should the bot restore the normal view ?
      else if (distance <= 150.0f && pev->fov < 90.0f) {
         pev->button |= IN_ATTACK2;
      }
      m_zoomCheckTime = game.time () + 0.25f;
   }

   // else is the bot holding a zoomable rifle?
   else if (m_difficulty < Difficulty::Hard && usesZoomableRifle () && m_zoomCheckTime < game.time ()) {
      // should the bot switch to zoomed mode?
      if (distance > 800.0f && pev->fov >= 90.0f) {
         pev->button |= IN_ATTACK2;
      }

      // else should the bot restore the normal view?
      else if (distance <= 800.0f && pev->fov < 90.0f) {
         pev->button |= IN_ATTACK2;
      }
      m_zoomCheckTime = game.time () + 0.5f;
   }

   // we're should stand still before firing sniper weapons, else sniping is useless..
   if (usesSniper () && (m_aimFlags & (AimFlags::Enemy | AimFlags::LastEnemy)) && !m_isReloading && pev->velocity.lengthSq () > 0.0f && getCurrentTaskId () != Task::SeekCover) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;
      m_navTimeset = game.time ();

      if (cr::abs (pev->velocity.x) > 5.0f || cr::abs (pev->velocity.y) > 5.0f || cr::abs (pev->velocity.z) > 5.0f) {
         m_sniperStopTime = game.time () + 2.0f;
         return;
      }
   }

   // need to care for burst fire?
   if (distance < kSprayDistance || m_blindTime > game.time () || usesKnife ()) {
      if (id == Weapon::Knife) {
         if (distance < 64.0f) {
            const auto primaryAttackChance = (m_oldButtons & IN_ATTACK2) ? 80 : 40;

            if (rg.chance (primaryAttackChance) || hasShield ()) {
               pev->button |= IN_ATTACK; // use primary attack
            }
            else {
               pev->button |= IN_ATTACK2; // use secondary attack
            }
         }
      }
      else {
         // if automatic weapon press attack
         if (tab[choosen].primaryFireHold && getAmmo (tab[index].id) > tab[index].minPrimaryAmmo) {
            pev->button |= IN_ATTACK;
         }

         // if not, toggle
         else {
            if ((m_oldButtons & IN_ATTACK) == 0) {
               pev->button |= IN_ATTACK;
            }
         }
      }
      m_shootTime = game.time ();
   }
   else {
      // don't attack with knife over long distance
      if (id == Weapon::Knife) {
         m_shootTime = game.time ();
         return;
      }

      if (needToPauseFiring (distance)) {
         return;
      }

      if (tab[choosen].primaryFireHold) {
         m_shootTime = game.time ();
         m_zoomCheckTime = game.time ();

         pev->button |= IN_ATTACK; // use primary attack
      }
      else {
         if ((m_oldButtons & IN_ATTACK) == 0) {
            pev->button |= IN_ATTACK;
         }

         constexpr float minDelay[] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f };
         constexpr float maxDelay[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.7f };

         const int offset = cr::abs <int> (m_difficulty * 25 / 20 - 5);

         m_shootTime = game.time () + 0.1f + rg.get (minDelay[offset], maxDelay[offset]);
         m_zoomCheckTime = game.time ();
      }
   }
}

void Bot::fireWeapons () {
   // this function will return true if weapon was fired, false otherwise
   // 
   // do not handle this if with grenade, as it's done it throw grendate task
   if (m_isUsingGrenade) {
      return;
   }
   float distance = m_lookAt.distance (getEyesPos ()); // how far away is the enemy?

   // or if friend in line of fire, stop this too but do not update shoot time
   if (isFriendInLineOfFire (distance)) {
      m_fightStyle = Fight::Strafe;
      m_lastFightStyleCheck = game.time ();

      return;
   }

   auto tab = conf.getRawWeapons ();
   auto enemy = m_enemy;

   int selectId = Weapon::Knife, selectIndex = 0, choosenWeapon = 0;
   int weapons = pev->weapons;

   // if jason mode use knife only
   if (isKnifeMode ()) {
      selectWeapons (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // use knife if near and good difficulty (l33t dude!)
   if (cv_stab_close_enemies.bool_ () && m_difficulty >= Difficulty::Normal && m_healthValue > 80.0f && !game.isNullEntity (enemy) && m_healthValue >= enemy->v.health && distance < 100.0f && !isOnLadder () && !isGroupOfEnemies (pev->origin)) {
      selectWeapons (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id) {
      // is the bot carrying this weapon?
      if (weapons & cr::bit (tab[selectIndex].id)) {

         // is enough ammo available to fire AND check is better to use pistol in our current situation...
         if (m_ammoInClip[tab[selectIndex].id] > 0 && !isWeaponBadAtDistance (selectIndex, distance)) {
            choosenWeapon = selectIndex;
         }
      }
      selectIndex++;
   }
   selectId = tab[choosenWeapon].id;

   // if no available weapon...
   if (choosenWeapon == 0) {
      selectIndex = 0;

      // loop through all the weapons until terminator is found...
      while (tab[selectIndex].id) {
         int id = tab[selectIndex].id;

         // is the bot carrying this weapon?
         if (weapons & cr::bit (id)) {
            if (getAmmo (id) >= tab[selectIndex].minPrimaryAmmo) {

               // available ammo found, reload weapon
               if (m_reloadState == Reload::None || m_reloadCheckTime > game.time ()) {
                  m_isReloading = true;
                  m_reloadState = Reload::Primary;
                  m_reloadCheckTime = game.time ();

                  if (rg.chance (cr::abs (m_difficulty * 25 - 100)) && rg.chance (20)) {
                     pushRadioMessage (Radio::NeedBackup);
                  }
               }
               return;
            }
         }
         selectIndex++;
      }
      selectId = Weapon::Knife; // no available ammo, use knife!
   }
   selectWeapons (distance, selectIndex, selectId, choosenWeapon);
}

bool Bot::isWeaponBadAtDistance (int weaponIndex, float distance) {
   // this function checks, is it better to use pistol instead of current primary weapon
   // to attack our enemy, since current weapon is not very good in this situation.

   // do not switch weapons when crossing the distance line
   auto &info = conf.getWeapons ();

   if (m_difficulty < Difficulty::Normal || !hasSecondaryWeapon ()) {
      return false;
   }
   auto weaponType = info[weaponIndex].type;

   if (weaponType == WeaponType::Melee) {
      return false;
   }

   // check is ammo available for secondary weapon
   if (m_ammoInClip[info[bestSecondaryCarried ()].id] >= 3) {
      return false;
   }

   // better use pistol in short range distances, when using sniper weapons
   if (weaponType == WeaponType::Sniper && distance < 400.0f) {
      return true;
   }

   // shotguns is too inaccurate at long distances, so weapon is bad
   if (weaponType == WeaponType::Shotgun && distance > 750.0f) {
      return true;
   }
   return false;
}

void Bot::focusEnemy () {
   if (game.isNullEntity (m_enemy)) {
      return;
   }

   // aim for the head and/or body
   m_lookAt = getEnemyBodyOffset ();

   if (m_enemySurpriseTime > game.time ()) {
      return;
   }
   float distance = m_lookAt.distance2d (getEyesPos ()); // how far away is the enemy scum?

   if (distance < 128.0f && !usesSniper ()) {
      if (usesKnife ()) {
         if (distance < 72.0f) {
            m_wantsToFire = true;
         }
         else if (distance > 90.0f) {
            m_wantsToFire = false;
         }
      }
      else {
         m_wantsToFire = true;
      }
   }
   else {
      float dot = util.getShootingCone (ent (), m_enemyOrigin);

      if (dot < 0.90f) {
         m_wantsToFire = false;
      }
      else {
         float enemyDot = util.getShootingCone (m_enemy, pev->origin);

         // enemy faces bot?
         if (enemyDot >= 0.90f) {
            m_wantsToFire = true;
         }
         else {
            if (dot > 0.99f) {
               m_wantsToFire = true;
            }
            else {
               m_wantsToFire = false;
            }
         }
      }
   }
}

void Bot::attackMovement () {
   // no enemy? no need to do strafing
   if (game.isNullEntity (m_enemy)) {
      return;
   }

   // use enemy as dest origin if with knife
   if (usesKnife ()) {
      m_destOrigin = m_enemy->v.origin;
   }

   if (m_lastUsedNodesTime - m_frameInterval > game.time ()) {
      return;
   }

   auto approach = 0;
   auto distance = m_lookAt.distance2d (getEyesPos ()); // how far away is the enemy scum?

   if (usesKnife ()) {
      approach = 100;
   }
   else if ((m_states & Sense::SuspectEnemy) && !(m_states & Sense::SeeingEnemy)) {
      approach = 49;
   }
   else if (m_isReloading || m_isVIP) {
      approach = 29;
   }
   else {
      approach = static_cast <int> (m_healthValue * m_agressionLevel);

      if (usesSniper () && approach > 49) {
         approach = 49;
      }
   }

   // only take cover when bomb is not planted and enemy can see the bot or the bot is VIP
   if ((m_states & Sense::SeeingEnemy) && approach < 30 && !bots.isBombPlanted () && (isInViewCone (m_enemy->v.origin) || m_isVIP)) {
      m_moveSpeed = -pev->maxspeed;
      startTask (Task::SeekCover, TaskPri::SeekCover, kInvalidNodeIndex, 0.0f, true);
   }
   else if (approach < 50) {
      m_moveSpeed = 0.0f;
   }
   else {
      m_moveSpeed = pev->maxspeed;
   }

   if (m_lastFightStyleCheck + 3.0f < game.time ()) {
      if (usesSniper ()) {
         m_fightStyle = Fight::Stay;
      }
      else if (usesRifle () || usesSubmachine () || usesHeavy ()) {
         int rand = rg.get (1, 100);

         if (distance < 500.0f) {
            m_fightStyle = Fight::Strafe;
         }
         else if (distance < 1024.0f) {
            if (rand < (usesSubmachine () ? 50 : 30)) {
               m_fightStyle = Fight::Strafe;
            }
            else {
               m_fightStyle = Fight::Stay;
            }
         }
         else {
            if (rand < (usesSubmachine () ? 80 : 90)) {
               m_fightStyle = Fight::Stay;
            }
            else {
               m_fightStyle = Fight::Strafe;
            }
         }
      }
      else if (rg.get (0, 100) < (isInNarrowPlace () ? 25 : 75) || usesKnife ()) {
         m_fightStyle = Fight::Strafe;
      }
      else {
         m_fightStyle = Fight::Stay;
      }

      // do not try to strafe while ducking
      if (isDucking () || isInNarrowPlace ()) {
         m_fightStyle = Fight::Stay;
      }
      m_lastFightStyleCheck = game.time ();
   }

   if (distance < 96.0f && !usesKnife ()) {
      m_moveSpeed = -pev->maxspeed;
   }

   if (usesKnife ()) {
      m_fightStyle = Fight::None;
      m_lastFightStyleCheck = game.time ();
   }

   if (m_fightStyle == Fight::Strafe) {
      auto swapStrafeCombatDir = [&] () {
         m_combatStrafeDir = (m_combatStrafeDir == Dodge::Left ? Dodge::Right : Dodge::Left);
      };

      auto strafeUpdateTime = [] () {
         return game.time () + rg.get (0.5f, 1.0f);
      };

      // to start strafing, we have to first figure out if the target is on the left side or right side
      if (m_strafeSetTime < game.time ()) {
         const auto &dirToPoint = (pev->origin - m_enemy->v.origin).normalize2d ();
         const auto &rightSide = m_enemy->v.v_angle.right ().normalize2d ();

         if ((dirToPoint | rightSide) < 0.0f) {
            m_combatStrafeDir = Dodge::Left;
         }
         else {
            m_combatStrafeDir = Dodge::Right;
         }

         if (rg.chance (30)) {
            swapStrafeCombatDir ();
         }
         m_strafeSetTime = strafeUpdateTime ();
      }

      if (m_combatStrafeDir == Dodge::Right) {
         if (!checkWallOnLeft ()) {
            m_strafeSpeed = -pev->maxspeed;
         }
         else if (!checkWallOnRight ()) {
            swapStrafeCombatDir ();
            m_strafeSetTime = strafeUpdateTime ();

            m_strafeSpeed = pev->maxspeed;
         }
         else {
            m_strafeSpeed = 0.0f;
            m_strafeSetTime = strafeUpdateTime ();
         }
      }
      else {
         if (!checkWallOnRight ()) {
            m_strafeSpeed = pev->maxspeed;
         }
         else if (!checkWallOnLeft ()) {
            swapStrafeCombatDir ();
            m_strafeSetTime = strafeUpdateTime ();

            m_strafeSpeed = -pev->maxspeed;
         }
         else {
            m_strafeSpeed = 0.0f;
            m_strafeSetTime = strafeUpdateTime ();
         }
      }

      if (m_difficulty >= Difficulty::Normal && (m_jumpTime + 5.0f < game.time () && isOnFloor () && rg.get (0, 1000) < (m_isReloading ? 8 : 2) && pev->velocity.length2d () > 150.0f) && !usesSniper ()) {
         pev->button |= IN_JUMP;
      }
   }
   else if (m_fightStyle == Fight::Stay) {
      const bool alreadyDucking = m_duckTime > game.time () || isDucking ();

      if (alreadyDucking) {
         m_duckTime = game.time () + m_frameInterval * 2.0f;
      }
      else if ((distance > kDoubleSprayDistance && hasPrimaryWeapon ()) && (m_enemyParts & (Visibility::Head | Visibility::Body)) && getCurrentTaskId () != Task::SeekCover && getCurrentTaskId () != Task::Hunt) {
         int enemyNearestIndex = graph.getNearest (m_enemy->v.origin);

         if (graph.isVisible (m_currentNodeIndex, enemyNearestIndex) && graph.isDuckVisible (m_currentNodeIndex, enemyNearestIndex) && graph.isDuckVisible (enemyNearestIndex, m_currentNodeIndex)) {
            m_duckTime = game.time () + m_frameInterval * 2.0f;
         }
      }
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;
   }

   if (m_difficulty >= Difficulty::Normal && isOnFloor () && m_duckTime < game.time ()) {
      if (distance < 768.0f) {
         if (rg.get (0, 1000) < rg.get (7, 12) && pev->velocity.length2d () > 150.0f && isInViewCone (m_enemy->v.origin)) {
            pev->button |= IN_JUMP;
         }
      }
   }

   if (m_isReloading) {
      m_moveSpeed = -pev->maxspeed;
      m_duckTime = game.time () - 1.0f;
   }

   if (!isInWater () && !isOnLadder () && (m_moveSpeed > 0.0f || m_strafeSpeed >= 0.0f)) {
      Vector right, forward;
      pev->v_angle.angleVectors (&forward, &right, nullptr);

      const auto &front = right * m_moveSpeed * 0.2f;
      const auto &side = right * m_strafeSpeed * 0.2f;
      const auto &spot = pev->origin + front + side + pev->velocity * m_frameInterval;

      if (isDeadlyMove (spot)) {
         m_strafeSpeed = -m_strafeSpeed;
         m_moveSpeed = -m_moveSpeed;

         pev->button &= ~IN_JUMP;
      }
   }
   ignoreCollision ();
}

bool Bot::hasPrimaryWeapon () {
   // this function returns returns true, if bot has a primary weapon

   return (pev->weapons & kPrimaryWeaponMask) != 0;
}

bool Bot::hasSecondaryWeapon () {
   // this function returns returns true, if bot has a secondary weapon

   return (pev->weapons & kSecondaryWeaponMask) != 0;
}

bool Bot::hasShield () {
   // this function returns true, if bot has a tactical shield

   return strncmp (pev->viewmodel.chars (14), "v_shield_", 9) == 0;
}

bool Bot::isShieldDrawn () {
   // this function returns true, is the tactical shield is drawn

   if (!hasShield ()) {
      return false;
   }
   return pev->weaponanim == 6 || pev->weaponanim == 7;
}

bool Bot::isEnemyBehindShield (edict_t *enemy) {
   // this function returns true, if enemy protected by the shield

   if (game.isNullEntity (enemy) || isShieldDrawn ()) {
      return false;
   }

   // check if enemy has shield and this shield is drawn
   if ((enemy->v.weaponanim == 6 || enemy->v.weaponanim == 7) && strncmp (enemy->v.viewmodel.chars (14), "v_shield_", 9) == 0) {
      if (util.isInViewCone (pev->origin, enemy)) {
         return true;
      }
   }
   return false;
}

bool Bot::usesSniper () {
   // this function returns true, if returns if bot is using a sniper rifle

   return m_weaponType == WeaponType::Sniper;
}

bool Bot::usesRifle () {
   return usesZoomableRifle () || m_weaponType == WeaponType::Rifle;
}

bool Bot::usesZoomableRifle () {
   return m_weaponType == WeaponType::ZoomRifle;
}

bool Bot::usesPistol () {
   return m_weaponType == WeaponType::Pistol;
}

bool Bot::usesSubmachine () {
   return m_weaponType == WeaponType::SMG;
}

bool Bot::usesShotgun () {
   return m_weaponType == WeaponType::Shotgun;
}

bool Bot::usesHeavy () {
   return m_weaponType == WeaponType::Heavy;
}

bool Bot::usesBadWeapon () {
   return usesShotgun () || m_currentWeapon == Weapon::UMP45 || m_currentWeapon == Weapon::MAC10 || m_currentWeapon == Weapon::TMP;
}

bool Bot::usesCampGun () {
   return usesSubmachine () || usesRifle () || usesSniper () || usesHeavy ();
}

bool Bot::usesKnife () {
   return m_weaponType == WeaponType::Melee;
}

int Bot::bestPrimaryCarried () {
   // this function returns the best weapon of this bot (based on personality prefs)

   const int *pref = conf.getWeaponPrefs (m_personality);

   int weaponIndex = 0;
   int weapons = pev->weapons;

   const auto &tab = conf.getWeapons ();

   // take the shield in account
   if (hasShield ()) {
      weapons |= cr::bit (Weapon::Shield);
   }

   for (int i = 0; i < kNumWeapons; ++i) {
      if (weapons & cr::bit (tab[*pref].id)) {
         weaponIndex = i;
      }
      pref++;
   }
   return weaponIndex;
}

int Bot::bestSecondaryCarried () {
   // this function returns the best secondary weapon of this bot (based on personality prefs)

   const int *pref = conf.getWeaponPrefs (m_personality);

   int weaponIndex = 0;
   int weapons = pev->weapons;

   // take the shield in account
   if (hasShield ()) {
      weapons |= cr::bit (Weapon::Shield);
   }
   const auto tab = conf.getRawWeapons ();

   for (int i = 0; i < kNumWeapons; ++i) {
      int id = tab[*pref].id;

      if ((weapons & cr::bit (tab[*pref].id)) && (id == Weapon::USP || id == Weapon::Glock18 || id == Weapon::Deagle || id == Weapon::P228 || id == Weapon::Elite || id == Weapon::FiveSeven)) {
         weaponIndex = i;
         break;
      }
      pref++;
   }
   return weaponIndex;
}

int Bot::bestGrenadeCarried () {
   if (pev->weapons & cr::bit (Weapon::Explosive)) {
      return Weapon::Explosive;
   }
   else if (pev->weapons & cr::bit (Weapon::Smoke)) {
      return Weapon::Smoke;
   }
   else if (pev->weapons & cr::bit (Weapon::Flashbang)) {
      return Weapon::Flashbang;
   }
   return -1;
}

bool Bot::rateGroundWeapon (edict_t *ent) {
   // this function compares weapons on the ground to the one the bot is using

   int groundIndex = 0;

   const int *pref = conf.getWeaponPrefs (m_personality);
   auto tab = conf.getRawWeapons ();

   for (int i = 0; i < kNumWeapons; ++i) {
      if (strcmp (tab[*pref].model, ent->v.model.chars (9)) == 0) {
         groundIndex = i;
         break;
      }
      pref++;
   }
   int hasWeapon = 0;

   if (groundIndex < 7) {
      hasWeapon = bestSecondaryCarried ();
   }
   else {
      hasWeapon = bestPrimaryCarried ();
   }
   return groundIndex > hasWeapon;
}

bool Bot::hasAnyWeapons () {
   return !!(pev->weapons & (kPrimaryWeaponMask | kSecondaryWeaponMask));
}

bool Bot::isKnifeMode () {
   return cv_jasonmode.bool_ () || (usesKnife () && !hasAnyWeapons ());
}

void Bot::selectBestWeapon () {
   // this function chooses best weapon, from weapons that bot currently own, and change
   // current weapon to best one.

   // if knife mode activated, force bot to use knife
   if (isKnifeMode ()) {
      selectWeaponById (Weapon::Knife);
      return;
   }

   if (m_isReloading) {
      return;
   }
   auto tab = conf.getRawWeapons ();

   int selectIndex = 0;
   int chosenWeaponIndex = 0;

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id) {
      // is the bot NOT carrying this weapon?
      if (!(pev->weapons & cr::bit (tab[selectIndex].id))) {
         selectIndex++; // skip to next weapon
         continue;
      }

      int id = tab[selectIndex].id;
      bool ammoLeft = false;

      // is the bot already holding this weapon and there is still ammo in clip?
      if (tab[selectIndex].id == m_currentWeapon && (getAmmoInClip () < 0 || getAmmoInClip () >= tab[selectIndex].minPrimaryAmmo)) {
         ammoLeft = true;
      }

      // is no ammo required for this weapon OR enough ammo available to fire
      if (getAmmo (id) >= tab[selectIndex].minPrimaryAmmo) {
         ammoLeft = true;
      }

      if (ammoLeft) {
         chosenWeaponIndex = selectIndex;
      }
      selectIndex++;
   }

   chosenWeaponIndex %= kNumWeapons + 1;
   selectIndex = chosenWeaponIndex;

   const int id = tab[selectIndex].id;

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id) {
      selectWeaponById (tab[selectIndex].id);
   }
   m_isReloading = false;
   m_reloadState = Reload::None;
}

void Bot::selectSecondary () {
   int oldWeapons = pev->weapons;

   pev->weapons &= ~kPrimaryWeaponMask;
   selectBestWeapon ();

   pev->weapons = oldWeapons;
}

int Bot::bestWeaponCarried () {
   auto tab = conf.getRawWeapons ();

   int weapons = pev->weapons;
   int num = 0;
   int i = 0;

   // loop through all the weapons until terminator is found...
   while (tab->id) {
      // is the bot carrying this weapon?
      if (weapons & cr::bit (tab->id)) {
         num = i;
      }
      ++i;
      tab++;
   }
   return num;
}

void Bot::decideFollowUser () {
   // this function forces bot to follow user
   Array <edict_t *> users;

   // search friends near us
   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team != m_team || client.ent == ent ()) {
         continue;
      }

      if (seesEntity (client.origin) && !util.isFakeClient (client.ent)) {
         users.push (client.ent);
      }
   }

   if (users.empty ()) {
      return;
   }
   m_targetEntity = users.random ();

   pushChatterMessage (Chatter::LeadOnSir);
   startTask (Task::FollowUser, TaskPri::FollowUser, kInvalidNodeIndex, 0.0f, true);
}

void Bot::updateTeamCommands () {
   // prevent spamming
   if (m_timeTeamOrder > game.time () + 2.0f || game.is (GameFlags::FreeForAll) || !cv_radio_mode.int_ ()) {
      return;
   }

   bool memberNear = false;
   bool memberExists = false;

   // search teammates seen by this bot
   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team != m_team || client.ent == ent ()) {
         continue;
      }
      memberExists = true;

      if (seesEntity (client.origin)) {
         memberNear = true;
         break;
      }
   }

   // has teammates?
   if (memberNear) {
      if (m_personality == Personality::Rusher && cv_radio_mode.int_ () == 2) {
         pushRadioMessage (Radio::StormTheFront);
      }
      else if (m_personality != Personality::Rusher && cv_radio_mode.int_ () == 2) {
         pushRadioMessage (Radio::TeamFallback);
      }
   }
   else if (memberExists && cv_radio_mode.int_ () == 1) {
      pushRadioMessage (Radio::TakingFireNeedAssistance);
   }
   else if (memberExists && cv_radio_mode.int_ () == 2) {
      pushChatterMessage (Chatter::ScaredEmotion);
   }
   m_timeTeamOrder = game.time () + rg.get (15.0f, 30.0f);
}

bool Bot::isGroupOfEnemies (const Vector &location, int numEnemies, float radius) {
   int numPlayers = 0;

   // search the world for enemy players...
   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.ent == ent ()) {
         continue;
      }

      if (client.ent->v.origin.distanceSq (location) < cr::square (radius)) {
         // don't target our teammates...
         if (client.team == m_team) {
            return false;
         }

         if (numPlayers++ > numEnemies) {
            return true;
         }
      }
   }
   return false;
}

void Bot::checkReload () {
   // check the reload state
   auto task = getCurrentTaskId ();

   // we're should not reload, while doing next tasks
   bool uninterruptibleTask = (task == Task::PlantBomb || task == Task::DefuseBomb || task == Task::PickupItem || task == Task::ThrowExplosive || task == Task::ThrowFlashbang || task == Task::ThrowSmoke);

   // do not check for reload
   if (uninterruptibleTask || m_isUsingGrenade || usesKnife ()) {
      m_reloadState = Reload::None;
      return;
   }

   m_isReloading = false; // update reloading status
   m_reloadCheckTime = game.time () + 3.0f;

   if (m_reloadState != Reload::None) {
      int wid = 0;
      int weapons = pev->weapons;

      if (m_reloadState == Reload::Primary) {
         weapons &= kPrimaryWeaponMask;
      }
      else if (m_reloadState == Reload::Secondary) {
         weapons &= kSecondaryWeaponMask;
      }

      if (weapons == 0) {
         m_reloadState++;

         if (m_reloadState > Reload::Secondary) {
            m_reloadState = Reload::None;
         }
         return;
      }

      for (int i = 1; i < kMaxWeapons; ++i) {
         if (weapons & cr::bit (i)) {
            wid = i;
            break;
         }
      }
      const auto &prop = conf.getWeaponProp (wid);

      if (isLowOnAmmo (prop.id, 0.75f) && getAmmo (prop.id) > 0) {
         if (m_currentWeapon != prop.id) {
            selectWeaponById (prop.id);
         }
         pev->button &= ~IN_ATTACK;

         if ((m_oldButtons & IN_RELOAD) == Reload::None) {
            pev->button |= IN_RELOAD; // press reload button
         }
         m_isReloading = true;
      }
      else {
         // if we have enemy don't reload next weapon
         if ((m_states & (Sense::SeeingEnemy | Sense::HearingEnemy)) || m_seeEnemyTime + 5.0f > game.time ()) {
            m_reloadState = Reload::None;
            return;
         }
         m_reloadState++;

         if (m_reloadState > Reload::Secondary) {
            m_reloadState = Reload::None;
         }
         return;
      }
   }
}

float Bot::calculateScaleFactor (edict_t *ent) {
   Vector entSize = ent->v.maxs - ent->v.mins;
   float entArea = 2 * (entSize.x * entSize.y + entSize.y * entSize.z + entSize.x * entSize.z);

   Vector botSize = pev->maxs - pev->mins;
   float botArea = 2 * (botSize.x * botSize.y + botSize.y * botSize.z + botSize.x * botSize.z);

   return entArea / botArea;
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

      if (dot > 0.75f || tr.flFraction < 0.8f) {
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
   const float half = time * 0.5f;

   velocity = velocity * (1.0f / time);
   velocity.z += gravity * half * half;

   Vector apex = start + (stop - start) * 0.5f;
   apex.z += 0.5f * gravity * half;

   game.testHull (start, apex, TraceIgnore::None, head_hull, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0f)) {
      return nullptr;
   }
   game.testHull (stop, apex, TraceIgnore::Monsters, head_hull, ent (), &tr);

   if (!cr::fequal (tr.flFraction, 1.0) || tr.fAllSolid) {
      float dot = -(tr.vecPlaneNormal | (apex - stop).normalize ());

      if (dot > 0.75f || tr.flFraction < 0.8f) {
         return nullptr;
      }
   }
   return velocity * 0.7793f;
}

edict_t *Bot::setCorrectGrenadeVelocity (const char *model) {
   edict_t *result = nullptr;

   game.searchEntities ("classname", "grenade", [&] (edict_t *ent) {
      if (ent->v.owner == this->ent () && util.isModel (ent, model)) {
         result = ent;

         // set the correct velocity for the grenade
         if (m_grenade.lengthSq () > 100.0f) {
            ent->v.velocity = m_grenade + (m_grenade * m_frameInterval * 4.0f);
         }
         m_grenadeCheckTime = game.time () + 3.0f;

         selectBestWeapon ();
         completeTask ();

         return EntitySearchResult::Break;
      }
      return EntitySearchResult::Continue;
   });
   return result;
}

void Bot::checkGrenadesThrow () {

   // do not check cancel if we have grenade in out hands
   bool preventibleTasks = getCurrentTaskId () == Task::PlantBomb || getCurrentTaskId () == Task::DefuseBomb;

   auto clearThrowStates = [] (uint32_t &states) {
      states &= ~(Sense::ThrowExplosive | Sense::ThrowFlashbang | Sense::ThrowSmoke);
   };

   // check if throwing a grenade is a good thing to do...
   if (preventibleTasks || isInNarrowPlace () || cv_ignore_enemies.bool_ () || m_isUsingGrenade || m_grenadeRequested || m_isReloading || (isKnifeMode () && !bots.isBombPlanted ()) || (m_grenadeRequested || m_grenadeCheckTime >= game.time ())) {
      clearThrowStates (m_states);
      return;
   }

   // check again in some seconds
   m_grenadeCheckTime = game.time () + kGrenadeCheckTime;

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
   float distance = m_lastEnemyOrigin.distance2d (pev->origin);

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
            float radius = cr::max (192.0f, m_lastEnemy->v.velocity.length2d ());
            const Vector &pos = m_lastEnemy->v.velocity.get2d () + m_lastEnemy->v.origin;

            auto predicted = graph.getNarestInRadius (radius, pos, 12);

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
         int nearest = graph.getNearest (m_lastEnemy->v.velocity.get2d () + m_lastEnemy->v.origin);

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
      const float kMaxThrowTime = game.time () + kGrenadeCheckTime * 6.0f;

      if (m_states & Sense::ThrowExplosive) {
         startTask (Task::ThrowExplosive, TaskPri::Throw, kInvalidNodeIndex, kMaxThrowTime, false);
      }
      else if (m_states & Sense::ThrowFlashbang) {
         startTask (Task::ThrowFlashbang, TaskPri::Throw, kInvalidNodeIndex, kMaxThrowTime, false);
      }
      else if (m_states & Sense::ThrowSmoke) {
         startTask (Task::ThrowSmoke, TaskPri::Throw, kInvalidNodeIndex, kMaxThrowTime, false);
      }
   }
   else {
      clearThrowStates (m_states);
   }
}

bool Bot::isEnemyInSight (Vector &endPos) {
   TraceResult aimHitTr {};
   game.testModel (getEyesPos (), getEyesPos () + pev->v_angle.forward () * kInfiniteDistance, 0, m_enemy, &aimHitTr);

   if (aimHitTr.pHit != m_enemy) {
      return false;
   }
   endPos = aimHitTr.vecEndPos;
   return true;
}
