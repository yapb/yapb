//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_shoots_thru_walls ("yb_shoots_thru_walls", "2");
ConVar yb_ignore_enemies ("yb_ignore_enemies", "0");
ConVar yb_check_enemy_rendering ("yb_check_enemy_rendering", "0");

ConVar mp_friendlyfire ("mp_friendlyfire", nullptr, VT_NOREGISTER);

int Bot::numFriendsNear (const Vector &origin, float radius) {
   int count = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
         continue;
      }

      if ((client.origin - origin).lengthSq () < cr::square (radius)) {
         count++;
      }
   }
   return count;
}

int Bot::numEnemiesNear (const Vector &origin, float radius) {
   int count = 0;

   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team) {
         continue;
      }

      if ((client.origin - origin).lengthSq () < cr::square (radius)) {
         count++;
      }
   }
   return count;
}

bool Bot::isEnemyHidden (edict_t *enemy) {
   if (!yb_check_enemy_rendering.boolean () || engine.isNullEntity (enemy)) {
      return false;
   }
   entvars_t &v = enemy->v;

   bool enemyHasGun = (v.weapons & WEAPON_PRIMARY) || (v.weapons & WEAPON_SECONDARY);
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

bool Bot::checkBodyParts (edict_t *target, Vector *origin, uint8 *bodyPart) {
   // this function checks visibility of a bot target.

   if (isEnemyHidden (target)) {
      *bodyPart = 0;
      origin->nullify ();

      return false;
   }
   TraceResult result;

   Vector eyes = eyePos ();
   Vector spot = target->v.origin;

   *bodyPart = 0;

   engine.testLine (eyes, spot, TRACE_IGNORE_EVERYTHING, ent (), &result);

   if (result.flFraction >= 1.0f) {
      *bodyPart |= VISIBLE_BODY;
      *origin = result.vecEndPos;
   }

   // check top of head
   spot.z += 25.0f;
   engine.testLine (eyes, spot, TRACE_IGNORE_EVERYTHING, ent (), &result);

   if (result.flFraction >= 1.0f) {
      *bodyPart |= VISIBLE_HEAD;
      *origin = result.vecEndPos;
   }

   if (*bodyPart != 0) {
      return true;
   }

   const float standFeet = 34.0f;
   const float crouchFeet = 14.0f;

   if (target->v.flags & FL_DUCKING) {
      spot.z = target->v.origin.z - crouchFeet;
   }
   else {
      spot.z = target->v.origin.z - standFeet;
   }
   engine.testLine (eyes, spot, TRACE_IGNORE_EVERYTHING, ent (), &result);

   if (result.flFraction >= 1.0f) {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }

   const float edgeOffset = 13.0f;
   Vector dir = (target->v.origin - pev->origin).normalize2D ();

   Vector perp (-dir.y, dir.x, 0.0f);
   spot = target->v.origin + Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   engine.testLine (eyes, spot, TRACE_IGNORE_EVERYTHING, ent (), &result);

   if (result.flFraction >= 1.0f) {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }
   spot = target->v.origin - Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   engine.testLine (eyes, spot, TRACE_IGNORE_EVERYTHING, ent (), &result);

   if (result.flFraction >= 1.0f) {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }
   return false;
}

bool Bot::seesEnemy (edict_t *player, bool ignoreFOV) {
   if (engine.isNullEntity (player)) {
      return false;
   }

   if (isPlayer (pev->dmg_inflictor) && engine.getTeam (pev->dmg_inflictor) != m_team) {
      ignoreFOV = true;
   }

   if ((ignoreFOV || isInViewCone (player->v.origin)) && checkBodyParts (player, &m_enemyOrigin, &m_visibility)) {
      m_seeEnemyTime = engine.timebase ();
      m_lastEnemy = player;
      m_lastEnemyOrigin = m_enemyOrigin;

      return true;
   }
   return false;
}

bool Bot::lookupEnemies (void) {
   // this function tries to find the best suitable enemy for the bot

   // do not search for enemies while we're blinded, or shooting disabled by user
   if (m_enemyIgnoreTimer > engine.timebase () || m_blindTime > engine.timebase () || yb_ignore_enemies.boolean ()) {
      return false;
   }
   edict_t *player, *newEnemy = nullptr;
   float nearestDistance = cr::square (m_viewDistance);

   // clear suspected flag
   if (!engine.isNullEntity (m_enemy) && (m_states & STATE_SEEING_ENEMY)) {
      m_states &= ~STATE_SUSPECT_ENEMY;
   }
   else if (engine.isNullEntity (m_enemy) && m_seeEnemyTime + 1.0f > engine.timebase () && isAlive (m_lastEnemy)) {
      m_states |= STATE_SUSPECT_ENEMY;
      m_aimFlags |= AIM_LAST_ENEMY;
   }
   m_visibility = 0;
   m_enemyOrigin.nullify ();

   if (!engine.isNullEntity (m_enemy)) {
      player = m_enemy;

      // is player is alive
      if (m_enemyUpdateTime > engine.timebase () && (m_enemy->v.origin - pev->origin).lengthSq () < nearestDistance && isAlive (player) && seesEnemy (player)) {
         newEnemy = player;
      }
   }

   // the old enemy is no longer visible or
   if (engine.isNullEntity (newEnemy)) {

      // ignore shielded enemies, while we have real one
      edict_t *shieldEnemy = nullptr;

      // search the world for players...
      for (int i = 0; i < engine.maxClients (); i++) {
         const Client &client = g_clients[i];

         if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team || client.ent == ent ()) {
            continue;
         }
         player = client.ent;

         if ((player->v.button & (IN_ATTACK | IN_ATTACK2)) && m_viewDistance < m_maxViewDistance) {
            nearestDistance = cr::square (m_maxViewDistance);
         }

         // see if bot can see the player...
         if (seesEnemy (player)) {
            if (isEnemyBehindShield (player)) {
               shieldEnemy = player;
               continue;
            }
            float distance = (player->v.origin - pev->origin).lengthSq ();

            if (distance * 0.7f < nearestDistance) {
               nearestDistance = distance;
               newEnemy = player;

               // aim VIP first on AS maps...
               if (isPlayerVIP (newEnemy)) {
                  break;
               }
            }
         }
      }
      m_enemyUpdateTime = engine.timebase () + calcThinkInterval () * 30.0f;

      if (engine.isNullEntity (newEnemy) && !engine.isNullEntity (shieldEnemy)) {
         newEnemy = shieldEnemy;
      }
   }

   if (isPlayer (newEnemy)) {
      g_botsCanPause = true;

      m_aimFlags |= AIM_ENEMY;
      m_states |= STATE_SEEING_ENEMY;

      if (newEnemy == m_enemy) {
         // if enemy is still visible and in field of view, keep it keep track of when we last saw an enemy
         m_seeEnemyTime = engine.timebase ();

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
      else {
         if (m_seeEnemyTime + 3.0 < engine.timebase () && (m_hasC4 || hasHostage () || !engine.isNullEntity (m_targetEntity))) {
            pushRadioMessage (RADIO_ENEMY_SPOTTED);
         }
         m_targetEntity = nullptr; // stop following when we see an enemy...

         if (rng.getInt (0, 100) < m_difficulty * 25) {
            m_enemySurpriseTime = m_actualReactionTime * 0.5f;
         }
         else {
            m_enemySurpriseTime = m_actualReactionTime;
         }

         if (usesSniper ()) {
            m_enemySurpriseTime *= 0.5f;
         }
         m_enemySurpriseTime += engine.timebase ();

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;
         m_enemyReachableTimer = 0.0f;

         // keep track of when we last saw an enemy
         m_seeEnemyTime = engine.timebase ();

         if (!(m_oldButtons & IN_ATTACK)) {
            return true;
         }

         // now alarm all teammates who see this bot & don't have an actual enemy of the bots enemy should simulate human players seeing a teammate firing
         for (int j = 0; j < engine.maxClients (); j++) {
            const Client &client = g_clients[j];

            if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
               continue;
            }
            Bot *other = bots.getBot (client.ent);

            if (other != nullptr && other->m_seeEnemyTime + 2.0f < engine.timebase () && engine.isNullEntity (other->m_lastEnemy) && isVisible (pev->origin, other->ent ()) && other->isInViewCone (pev->origin)) {
               other->m_lastEnemy = newEnemy;
               other->m_lastEnemyOrigin = m_lastEnemyOrigin;
               other->m_seeEnemyTime = engine.timebase ();
               other->m_states |= (STATE_SUSPECT_ENEMY | STATE_HEARING_ENEMY);
               other->m_aimFlags |= AIM_LAST_ENEMY;
            }
         }
         return true;
      }
   }
   else if (!engine.isNullEntity (m_enemy)) {
      newEnemy = m_enemy;
      m_lastEnemy = newEnemy;

      if (!isAlive (newEnemy)) {
         m_enemy = nullptr;

         // shoot at dying players if no new enemy to give some more human-like illusion
         if (m_seeEnemyTime + 0.3f > engine.timebase ()) {
            if (!usesSniper ()) {
               m_shootAtDeadTime = engine.timebase () + 0.4f;
               m_actualReactionTime = 0.0f;
               m_states |= STATE_SUSPECT_ENEMY;

               return true;
            }
            return false;
         }
         else if (m_shootAtDeadTime > engine.timebase ()) {
            m_actualReactionTime = 0.0f;
            m_states |= STATE_SUSPECT_ENEMY;

            return true;
         }
         return false;
      }

      // if no enemy visible check if last one shoot able through wall
      if (yb_shoots_thru_walls.boolean () && m_difficulty >= 2 && isPenetrableObstacle (newEnemy->v.origin)) {
         m_seeEnemyTime = engine.timebase ();

         m_states |= STATE_SUSPECT_ENEMY;
         m_aimFlags |= AIM_LAST_ENEMY;

         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
   }

   // check if bots should reload...
   if ((m_aimFlags <= AIM_PREDICT_PATH && m_seeEnemyTime + 3.0f < engine.timebase () && !(m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) && engine.isNullEntity (m_lastEnemy) && engine.isNullEntity (m_enemy) && taskId () != TASK_SHOOTBREAKABLE && taskId () != TASK_PLANTBOMB && taskId () != TASK_DEFUSEBOMB) || g_roundEnded) {
      if (!m_reloadState) {
         m_reloadState = RELOAD_PRIMARY;
      }
   }

   // is the bot using a sniper rifle or a zoomable rifle?
   if ((usesSniper () || usesZoomableRifle ()) && m_zoomCheckTime + 1.0f < engine.timebase ()) {
      if (pev->fov < 90.0f) {
         pev->button |= IN_ATTACK2;
      }
      else {
         m_zoomCheckTime = 0.0f;
      }
   }
   return false;
}

Vector Bot::getBodyOffserError (float distance) {
   if (engine.isNullEntity (m_enemy)) {
      return Vector::null ();
   }

   if (m_aimErrorTime < engine.timebase ()) {
      const float error = distance / (cr::clamp (m_difficulty, 1, 4) * 1000.0f);
      Vector &maxs = m_enemy->v.maxs, &mins = m_enemy->v.mins;

      m_aimLastError = Vector (rng.getFloat (mins.x * error, maxs.x * error), rng.getFloat (mins.y * error, maxs.y * error), rng.getFloat (mins.z * error, maxs.z * error));
      m_aimErrorTime = engine.timebase () + rng.getFloat (0.5f, 1.0f);
   }
   return m_aimLastError;
}

const Vector &Bot::getEnemyBodyOffset (void) {
   // the purpose of this function, is to make bot aiming not so ideal. it's mutate m_enemyOrigin enemy vector
   // returned from visibility check function.

   auto headOffset = [] (edict_t *e) {
      return e->v.absmin.z + e->v.size.z * 0.81f;
   };

   // if no visibility data, use last one
   if (!m_visibility) {
      return m_enemyOrigin;
   }
   float distance = (m_enemy->v.origin - pev->origin).length ();

   // do not aim at head, at long distance (only if not using sniper weapon)
   if ((m_visibility & VISIBLE_BODY) && !usesSniper () && distance > (m_difficulty > 2 ? 2000.0f : 1000.0f)) {
      m_visibility &= ~VISIBLE_HEAD;
   }

   // do not aim at head while close enough to enemy and having sniper
   else if (distance < 800.0f && usesSniper ()) {
      m_visibility &= ~VISIBLE_HEAD;
   }

   // do not aim at head while enemy is soooo close enough to enemy when recoil aims at head automatically
   else if (distance < MAX_SPRAY_DISTANCE) {
      m_visibility &= ~VISIBLE_HEAD;
   }
   Vector aimPos = m_enemy->v.origin;

   if (m_difficulty > 2 && !(m_visibility & VISIBLE_OTHER)) {
      aimPos = (m_enemy->v.velocity - pev->velocity) * calcThinkInterval () + aimPos;
   }

   // if we only suspect an enemy behind a wall take the worst skill
   if (!m_visibility && (m_states & STATE_SUSPECT_ENEMY)) {
      aimPos += getBodyOffserError (distance);
   }
   else {
      // now take in account different parts of enemy body
      if (m_visibility & (VISIBLE_HEAD | VISIBLE_BODY)) {
         int headshotFreq[5] = { 20, 40, 60, 80, 100 };

         // now check is our skill match to aim at head, else aim at enemy body
         if ((rng.getInt (1, 100) < headshotFreq[m_difficulty]) || usesPistol ()) {
            aimPos.z = headOffset (m_enemy) + getEnemyBodyOffsetCorrection (distance);
         }
         else {
            aimPos.z += getEnemyBodyOffsetCorrection (distance);
         }
      }
      else if (m_visibility & VISIBLE_BODY) {
         aimPos.z += getEnemyBodyOffsetCorrection (distance);
      }
      else if (m_visibility & VISIBLE_OTHER) {
         aimPos = m_enemyOrigin;
      }
      else if (m_visibility & VISIBLE_HEAD) {
         aimPos.z = headOffset (m_enemy) + getEnemyBodyOffsetCorrection (distance);
      }
   }

   m_enemyOrigin = aimPos;
   m_lastEnemyOrigin = aimPos;

   // add some error to unskilled bots
   if (m_difficulty < 3) {
      m_enemyOrigin += getBodyOffserError (distance);
   }
   return m_enemyOrigin;
}

float Bot::getEnemyBodyOffsetCorrection (float distance) {
   bool sniper = usesSniper ();
   bool pistol = usesPistol ();
   bool rifle = usesRifle ();

   bool zoomableRifle = usesZoomableRifle ();
   bool submachine = usesSubmachine ();
   bool shotgun = (m_currentWeapon == WEAPON_XM1014 || m_currentWeapon == WEAPON_M3);
   bool m249 = m_currentWeapon == WEAPON_M249;

   float result = -2.0f;

   if (distance < MAX_SPRAY_DISTANCE) {
      return -9.0f;
   }
   else if (distance >= MAX_SPRAY_DISTANCE_X2) {
      if (sniper) {
         result = 0.18f;
      }
      else if (zoomableRifle) {
         result = 1.5f;
      }
      else if (pistol) {
         result = 2.5f;
      }
      else if (submachine) {
         result = 1.5f;
      }
      else if (rifle) {
         result = 1.0f;
      }
      else if (m249) {
         result = -5.5f;
      }
      else if (shotgun) {
         result = -4.5f;
      }
   }
   return result;
}

bool Bot::isFriendInLineOfFire (float distance) {
   // bot can't hurt teammates, if friendly fire is not enabled...
   if (!mp_friendlyfire.boolean () || (g_gameFlags & GAME_CSDM)) {
      return false;
   }
   makeVectors (pev->v_angle);

   TraceResult tr;
   engine.testLine (eyePos (), eyePos () + distance * pev->v_angle, TRACE_IGNORE_NONE, ent (), &tr);

   // check if we hit something
   if (isPlayer (tr.pHit) && tr.pHit != ent ()) {
      auto hit = tr.pHit;

      // check valid range
      if (isAlive (hit) && engine.getTeam (hit) == m_team) {
         return true;
      }
   }

   // search the world for players
   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
         continue;
      }
      edict_t *pent = client.ent;

      float friendDistance = (pent->v.origin - pev->origin).length ();
      float squareDistance = cr::sqrtf (1089.0f + cr::square (friendDistance));

      if (friendDistance <= distance && getShootingConeDeviation (ent (), pent->v.origin) > cr::square (friendDistance) / cr::square (squareDistance)) {
         return true;
      }
   }
   return false;
}

bool Bot::isPenetrableObstacle (const Vector &dest) {
   // this function returns true if enemy can be shoot through some obstacle, false otherwise.
   // credits goes to Immortal_BLG

   if (yb_shoots_thru_walls.integer () == 2) {
      return isPenetrableObstacle2 (dest);
   }

   if (m_difficulty < 2) {
      return false;
   }
   int penetratePower = getWeaponPenetrationPower (m_currentWeapon);

   if (penetratePower == 0) {
      return false;
   }
   TraceResult tr;

   float obstacleDistance = 0.0f;
   engine.testLine (eyePos (), dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   if (tr.fStartSolid) {
      const Vector &source = tr.vecEndPos;
      engine.testLine (dest, source, TRACE_IGNORE_MONSTERS, ent (), &tr);

      if (tr.flFraction != 1.0f) {
         if ((tr.vecEndPos - dest).lengthSq () > cr::square (800.0f)) {
            return false;
         }

         if (tr.vecEndPos.z >= dest.z + 200.0f) {
            return false;
         }
         obstacleDistance = (tr.vecEndPos - source).lengthSq ();
      }
   }
   constexpr float distance = cr::square (75.0f);

   if (obstacleDistance > 0.0f) {
      while (penetratePower > 0) {
         if (obstacleDistance > distance) {
            obstacleDistance -= distance;
            penetratePower--;

            continue;
         }
         return true;
      }
   }
   return false;
}

bool Bot::isPenetrableObstacle2 (const Vector &dest) {
   // this function returns if enemy can be shoot through some obstacle

   if (m_difficulty < 2 || getWeaponPenetrationPower (m_currentWeapon) == 0) {
      return false;
   }

   const Vector &source = eyePos ();
   const Vector &direction = (dest - source).normalize (); // 1 unit long

   int thikness = 0;
   int numHits = 0;

   Vector point;
   TraceResult tr;

   engine.testLine (source, dest, TRACE_IGNORE_EVERYTHING, ent (), &tr);

   while (tr.flFraction != 1.0f && numHits < 3) {
      numHits++;
      thikness++;

      point = tr.vecEndPos + direction;

      while (g_engfuncs.pfnPointContents (point) == CONTENTS_SOLID && thikness < 98) {
         point = point + direction;
         thikness++;
      }
      engine.testLine (point, dest, TRACE_IGNORE_EVERYTHING, ent (), &tr);
   }

   if (numHits < 3 && thikness < 98) {
      if ((dest - point).lengthSq () < 13143.0f) {
         return true;
      }
   }
   return false;
}

bool Bot::throttleFiring (float distance) {
   // returns true if bot needs to pause between firing to compensate for punchangle & weapon spread

   if (usesSniper ()) {
      return false;
   }

   if (m_firePause > engine.timebase ())
      return true;

   if ((m_aimFlags & AIM_ENEMY) && !m_enemyOrigin.empty ()) {
      if (getShootingConeDeviation (ent (), m_enemyOrigin) > 0.92f && isEnemyBehindShield (m_enemy)) {
         return true;
      }
   }
   float offset = 5.0f;

   if (distance < MAX_SPRAY_DISTANCE * 0.5f) {
      return false;
   }
   else if (distance < MAX_SPRAY_DISTANCE) {
      offset = 12.0f;
   }
   else if (distance < MAX_SPRAY_DISTANCE_X2) {
      offset = 10.0f;
   }
   const float xPunch = cr::deg2rad (pev->punchangle.x);
   const float yPunch = cr::deg2rad (pev->punchangle.y);

   float interval = calcThinkInterval ();
   float tolerance = (100.0f - m_difficulty * 25.0f) / 99.0f;

   // check if we need to compensate recoil
   if (cr::tanf (cr::sqrtf (cr::abs (xPunch * xPunch) + cr::abs (yPunch * yPunch))) * distance > offset + 30.0f + tolerance) {
      if (m_firePause < engine.timebase ()) {
         m_firePause = rng.getFloat (0.5f, 0.5f + 0.3f * tolerance);
      }
      m_firePause -= interval;
      m_firePause += engine.timebase ();

      return true;
   }
   return false;
}

void Bot::selectWeapons (float distance, int index, int id, int choosen) {
   WeaponSelect *tab = &g_weaponSelect[0];

   // we want to fire weapon, don't reload now
   if (!m_isReloading) {
      m_reloadState = RELOAD_NONE;
      m_reloadCheckTime = engine.timebase () + 3.0f;
   }

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id) {
      selectWeaponByName (g_weaponDefs[id].className);

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

   if (hasShield () && m_shieldCheckTime < engine.timebase () && taskId () != TASK_CAMP) // better shield gun usage
   {
      if (distance >= 750.0f && !isShieldDrawn ()) {
         pev->button |= IN_ATTACK2; // draw the shield
      }
      else if (isShieldDrawn () || (!engine.isNullEntity (m_enemy) && ((m_enemy->v.button & IN_RELOAD) || !seesEnemy (m_enemy)))) {
         pev->button |= IN_ATTACK2; // draw out the shield
      }
      m_shieldCheckTime = engine.timebase () + 1.0f;
   }

   // is the bot holding a sniper rifle?
   if (usesSniper () && m_zoomCheckTime < engine.timebase ())  {
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
      m_zoomCheckTime = engine.timebase () + 0.25f;
   }

   // else is the bot holding a zoomable rifle?
   else if (m_difficulty < 3 && usesZoomableRifle () && m_zoomCheckTime < engine.timebase ())  {
      // should the bot switch to zoomed mode?
      if (distance > 800.0f && pev->fov >= 90.0f) {
         pev->button |= IN_ATTACK2;
      }

      // else should the bot restore the normal view?
      else if (distance <= 800.0f && pev->fov < 90.0f) {
         pev->button |= IN_ATTACK2;
      }
      m_zoomCheckTime = engine.timebase () + 0.5f;
   }

   // we're should stand still before firing sniper weapons, else sniping is useless..
   if (usesSniper () && (m_states & (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY)) && !m_isReloading && pev->velocity.lengthSq () > 0.0f) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;
      m_navTimeset = engine.timebase ();

      if (cr::abs (pev->velocity.x) > 5.0f || cr::abs (pev->velocity.y) > 5.0f || cr::abs (pev->velocity.z) > 5.0f) {
         m_sniperStopTime = engine.timebase () + 2.5f;
         return;
      }
   }

   // need to care for burst fire?
   if (distance < MAX_SPRAY_DISTANCE || m_blindTime > engine.timebase ()) {
      if (id == WEAPON_KNIFE) {
         if (distance < 64.0f) {
            if (rng.getInt (1, 100) < 30 || hasShield ()) {
               pev->button |= IN_ATTACK; // use primary attack
            }
            else {
               pev->button |= IN_ATTACK2; // use secondary attack
            }
         }
      }
      else {

         // if automatic weapon press attack
         if (tab[choosen].primaryFireHold && m_ammo[g_weaponDefs[tab[index].id].ammo1] > tab[index].minPrimaryAmmo) {
            pev->button |= IN_ATTACK;
         }

         // if not, toggle
         else {
            if ((m_oldButtons & IN_ATTACK) == 0) {
               pev->button |= IN_ATTACK;
            }
         }
      }
      m_shootTime = engine.timebase ();
   }
   else {
      if (throttleFiring (distance)) {
         return;
      }

      // don't attack with knife over long distance
      if (id == WEAPON_KNIFE) {
         m_shootTime = engine.timebase ();
         return;
      }

      if (tab[choosen].primaryFireHold) {
         m_shootTime = engine.timebase ();
         m_zoomCheckTime = engine.timebase ();

         pev->button |= IN_ATTACK; // use primary attack
      }
      else {
         pev->button |= IN_ATTACK;

         const float minDelay[] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f };
         const float maxDelay[] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.7f };

         const int offset = cr::abs <int> (m_difficulty * 25 / 20 - 5);

         m_shootTime = engine.timebase () + 0.1f + rng.getFloat (minDelay[offset], maxDelay[offset]);
         m_zoomCheckTime = engine.timebase ();
      }
   }
}

void Bot::fireWeapons (void) {
   // this function will return true if weapon was fired, false otherwise
   float distance = (m_lookAt - eyePos ()).length (); // how far away is the enemy?

   // or if friend in line of fire, stop this too but do not update shoot time
   if (!engine.isNullEntity (m_enemy)) {
      if (isFriendInLineOfFire (distance)) {
         m_fightStyle = FIGHT_STRAFE;
         m_lastFightStyleCheck = engine.timebase ();

         return;
      }
   }
   WeaponSelect *tab = &g_weaponSelect[0];

   edict_t *enemy = m_enemy;

   int selectId = WEAPON_KNIFE, selectIndex = 0, choosenWeapon = 0;
   int weapons = pev->weapons;

   // if jason mode use knife only
   if (yb_jasonmode.boolean ()) {
      selectWeapons (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // use knife if near and good difficulty (l33t dude!)
   if (m_difficulty >= 3 && pev->health > 80.0f && !engine.isNullEntity (enemy) && pev->health >= enemy->v.health && distance < 100.0f && !isOnLadder () && !isGroupOfEnemies (pev->origin)) {
      selectWeapons (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id) {
      // is the bot carrying this weapon?
      if (weapons & (1 << tab[selectIndex].id)) {

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
         if (weapons & (1 << id)) {
            if (g_weaponDefs[id].ammo1 != -1 && g_weaponDefs[id].ammo1 < 32 && m_ammo[g_weaponDefs[id].ammo1] >= tab[selectIndex].minPrimaryAmmo) {
               // available ammo found, reload weapon
               if (m_reloadState == RELOAD_NONE || m_reloadCheckTime > engine.timebase ()) {
                  m_isReloading = true;
                  m_reloadState = RELOAD_PRIMARY;
                  m_reloadCheckTime = engine.timebase ();

                  if (rng.getInt (0, 100) < cr::abs (m_difficulty * 25 - 100)) {
                     pushRadioMessage (RADIO_NEED_BACKUP);
                  }
               }
               return;
            }
         }
         selectIndex++;
      }
      selectId = WEAPON_KNIFE; // no available ammo, use knife!
   }
   selectWeapons (distance, selectIndex, selectId, choosenWeapon);
}

bool Bot::isWeaponBadAtDistance (int weaponIndex, float distance) {
   // this function checks, is it better to use pistol instead of current primary weapon
   // to attack our enemy, since current weapon is not very good in this situation.

   if (m_difficulty < 2) {
      return false;
   }
   int wid = g_weaponSelect[weaponIndex].id;

   if (wid == WEAPON_KNIFE) {
      return false;
   }

   // check is ammo available for secondary weapon
   if (m_ammoInClip[g_weaponSelect[bestSecondaryCarried ()].id] >= 1) {
      return false;
   }

   // better use pistol in short range distances, when using sniper weapons
   if ((wid == WEAPON_SCOUT || wid == WEAPON_AWP || wid == WEAPON_G3SG1 || wid == WEAPON_SG550) && distance < 450.0f) {
      return true;
   }

   // shotguns is too inaccurate at long distances, so weapon is bad
   if ((wid == WEAPON_M3 || wid == WEAPON_XM1014) && distance > 750.0f) {
      return true;
   }
   return false;
}

void Bot::focusEnemy (void) {
   // aim for the head and/or body
   m_lookAt = getEnemyBodyOffset ();

   if (m_enemySurpriseTime > engine.timebase () || engine.isNullEntity (m_enemy)) {
      return;
   } 
   float distance = (m_lookAt - eyePos ()).length2D (); // how far away is the enemy scum?

   if (distance < 128.0f && !usesSniper ()) {
      if (m_currentWeapon == WEAPON_KNIFE) {
         if (distance < 80.0f) {
            m_wantsToFire = true;
         }
         else if (distance > 120.0f) {
            m_wantsToFire = false;
         }
      }
      else {
         m_wantsToFire = true;
      }
   }
   else {
      float dot = getShootingConeDeviation (ent (), m_enemyOrigin);

      if (dot < 0.90f) {
         m_wantsToFire = false;
      }
      else {
         float enemyDot = getShootingConeDeviation (m_enemy, pev->origin);

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

void Bot::attackMovement (void) {
   // no enemy? no need to do strafing
   if (engine.isNullEntity (m_enemy)) {
      return;
   }
   float distance = (m_lookAt - eyePos ()).length2D (); // how far away is the enemy scum?

   if (m_timeWaypointMove + calcThinkInterval () + 0.5f < engine.timebase ()) {
      int approach;

      if (m_currentWeapon == WEAPON_KNIFE) {
         approach = 100;
      }
      else if ((m_states & STATE_SUSPECT_ENEMY) && !(m_states & STATE_SEEING_ENEMY)) {
         approach = 49;
      }
      else if (m_isReloading || m_isVIP) {
         approach = 29;
      }
      else {
         approach = static_cast <int> (pev->health * m_agressionLevel);

         if (usesSniper () && approach > 49) {
            approach = 49;
         }
      }

      // only take cover when bomb is not planted and enemy can see the bot or the bot is VIP
      if ((m_states & STATE_SEEING_ENEMY) && approach < 30 && !g_bombPlanted && (isInViewCone (m_enemy->v.origin) || m_isVIP)) {
         m_moveSpeed = -pev->maxspeed;
         startTask (TASK_SEEKCOVER, TASKPRI_SEEKCOVER, INVALID_WAYPOINT_INDEX, 0.0f, true);
      }
      else if (approach < 50) {
         m_moveSpeed = 0.0f;
      }
      else {
         m_moveSpeed = pev->maxspeed;
      }

      if (distance < 96.0f && m_currentWeapon != WEAPON_KNIFE) {
         m_moveSpeed = -pev->maxspeed;
      }

      if (usesSniper () || !(m_visibility & (VISIBLE_BODY | VISIBLE_HEAD))) {
         m_fightStyle = FIGHT_STAY;
         m_lastFightStyleCheck = engine.timebase ();
      }
      else if (usesRifle () || usesSubmachine ()) {
         if (m_lastFightStyleCheck + 3.0f < engine.timebase ()) {
            int rand = rng.getInt (1, 100);

            if (distance < 450.0f) {
               m_fightStyle = FIGHT_STRAFE;
            }
            else if (distance < 1024.0f) {
               if (rand < (usesSubmachine () ? 50 : 30)) {
                  m_fightStyle = FIGHT_STRAFE;
               }
               else {
                  m_fightStyle = FIGHT_STAY;
               }
            }
            else {
               if (rand < (usesSubmachine () ? 80 : 93)) {
                  m_fightStyle = FIGHT_STAY;
               }
               else {
                  m_fightStyle = FIGHT_STRAFE;
               }
            }
            m_lastFightStyleCheck = engine.timebase ();
         }
      }
      else {
         if (m_lastFightStyleCheck + 3.0f < engine.timebase ()) {
            if (rng.getInt (0, 100) < 50) {
               m_fightStyle = FIGHT_STRAFE;
            }
            else {
               m_fightStyle = FIGHT_STAY;
            }
            m_lastFightStyleCheck = engine.timebase ();
         }
      }

      if (m_fightStyle == FIGHT_STRAFE || ((pev->button & IN_RELOAD) || m_isReloading) || (usesPistol () && distance < 400.0f) || m_currentWeapon == WEAPON_KNIFE) {
         if (m_strafeSetTime < engine.timebase ()) {
            // to start strafing, we have to first figure out if the target is on the left side or right side
            makeVectors (m_enemy->v.v_angle);

            const Vector &dirToPoint = (pev->origin - m_enemy->v.origin).normalize2D ();
            const Vector &rightSide = g_pGlobals->v_right.normalize2D ();

            if ((dirToPoint | rightSide) < 0) {
               m_combatStrafeDir = STRAFE_DIR_LEFT;
            }
            else {
               m_combatStrafeDir = STRAFE_DIR_RIGHT;
            }

            if (rng.getInt (1, 100) < 30) {
               m_combatStrafeDir = (m_combatStrafeDir == STRAFE_DIR_LEFT ? STRAFE_DIR_RIGHT : STRAFE_DIR_LEFT);
            }
            m_strafeSetTime = engine.timebase () + rng.getFloat (0.5f, 3.0f);
         }

         if (m_combatStrafeDir == STRAFE_DIR_RIGHT) {
            if (!checkWallOnLeft ()) {
               m_strafeSpeed = -pev->maxspeed;
            }
            else {
               m_combatStrafeDir = STRAFE_DIR_LEFT;
               m_strafeSetTime = engine.timebase () + rng.getFloat (0.8f, 1.3f);
            }
         }
         else {
            if (!checkWallOnRight ()) {
               m_strafeSpeed = pev->maxspeed;
            }
            else {
               m_combatStrafeDir = STRAFE_DIR_RIGHT;
               m_strafeSetTime = engine.timebase () + rng.getFloat (0.8f, 1.3f);
            }
         }

         if (m_difficulty >= 3 && (m_jumpTime + 5.0f < engine.timebase () && isOnFloor () && rng.getInt (0, 1000) < (m_isReloading ? 8 : 2) && pev->velocity.length2D () > 120.0f) && !usesSniper ()) {
            pev->button |= IN_JUMP;
         }

         if (m_moveSpeed > 0.0f && distance > 100.0f && m_currentWeapon != WEAPON_KNIFE) {
            m_moveSpeed = 0.0f;
         }

         if (m_currentWeapon == WEAPON_KNIFE) {
            m_strafeSpeed = 0.0f;
         }
      }
      else if (m_fightStyle == FIGHT_STAY) {
         if ((m_visibility & (VISIBLE_HEAD | VISIBLE_BODY)) && !(m_visibility & VISIBLE_OTHER) && taskId () != TASK_SEEKCOVER && taskId () != TASK_HUNTENEMY) {
            int enemyNearestIndex = waypoints.getNearest (m_enemy->v.origin);

            if (waypoints.isDuckVisible (m_currentWaypointIndex, enemyNearestIndex) && waypoints.isDuckVisible (enemyNearestIndex, m_currentWaypointIndex)) {
               m_duckTime = engine.timebase () + 1.0f;
            }
         }
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
         m_navTimeset = engine.timebase ();
      }
   }

   if (m_duckTime > engine.timebase ()) {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;
   }

   if (m_moveSpeed > 0.0f && m_currentWeapon != WEAPON_KNIFE) {
      m_moveSpeed = getShiftSpeed ();
   }

   if (m_isReloading) {
      m_moveSpeed = -pev->maxspeed;
      m_duckTime = engine.timebase () - 1.0f;
   }

   if (!isInWater () && !isOnLadder () && (m_moveSpeed > 0.0f || m_strafeSpeed >= 0.0f)) {
      makeVectors (pev->v_angle);

      if (isDeadlyMove (pev->origin + (g_pGlobals->v_forward * m_moveSpeed * 0.2f) + (g_pGlobals->v_right * m_strafeSpeed * 0.2f) + (pev->velocity * calcThinkInterval ()))) {
         m_strafeSpeed = -m_strafeSpeed;
         m_moveSpeed = -m_moveSpeed;

         pev->button &= ~IN_JUMP;
      }
   }
   ignoreCollision ();
}

bool Bot::hasPrimaryWeapon (void) {
   // this function returns returns true, if bot has a primary weapon

   return (pev->weapons & WEAPON_PRIMARY) != 0;
}

bool Bot::hasSecondaryWeapon (void) {
   // this function returns returns true, if bot has a secondary weapon

   return (pev->weapons & WEAPON_SECONDARY) != 0;
}

bool Bot::hasShield (void) {
   // this function returns true, if bot has a tactical shield

   return strncmp (STRING (pev->viewmodel), "models/shield/v_shield_", 23) == 0;
}

bool Bot::isShieldDrawn (void) {
   // this function returns true, is the tactical shield is drawn

   if (!hasShield ()) {
      return false;
   }
   return pev->weaponanim == 6 || pev->weaponanim == 7;
}

bool Bot::isEnemyBehindShield (edict_t *enemy) {
   // this function returns true, if enemy protected by the shield

   if (engine.isNullEntity (enemy) || isShieldDrawn ()) {
      return false;
   }

   // check if enemy has shield and this shield is drawn
   if (strncmp (STRING (enemy->v.viewmodel), "models/shield/v_shield_", 23) == 0 && (enemy->v.weaponanim == 6 || enemy->v.weaponanim == 7)) {
      if (::isInViewCone (pev->origin, enemy)) {
         return true;
      }
   }
   return false;
}

bool Bot::usesSniper (void) {
   // this function returns true, if returns if bot is using a sniper rifle

   return m_currentWeapon == WEAPON_AWP || m_currentWeapon == WEAPON_G3SG1 || m_currentWeapon == WEAPON_SCOUT || m_currentWeapon == WEAPON_SG550;
}

bool Bot::usesRifle (void) {
   WeaponSelect *tab = &g_weaponSelect[0];
   int count = 0;

   while (tab->id) {
      if (m_currentWeapon == tab->id) {
         break;
      }
      tab++;
      count++;
   }

   if (tab->id && count > 13) {
      return true;
   }
   return false;
}

bool Bot::usesPistol (void) {
   WeaponSelect *tab = &g_weaponSelect[0];
   int count = 0;

   // loop through all the weapons until terminator is found
   while (tab->id) {
      if (m_currentWeapon == tab->id) {
         break;
      }
      tab++;
      count++;
   }

   if (tab->id && count < 7) {
      return true;
   }
   return false;
}

bool Bot::usesCampGun (void) {
   return usesSubmachine () || usesRifle () || usesSniper ();
}

bool Bot::usesSubmachine (void) {
   return m_currentWeapon == WEAPON_MP5 || m_currentWeapon == WEAPON_TMP || m_currentWeapon == WEAPON_P90 || m_currentWeapon == WEAPON_MAC10 || m_currentWeapon == WEAPON_UMP45;
}

bool Bot::usesZoomableRifle (void) {
   return m_currentWeapon == WEAPON_AUG || m_currentWeapon == WEAPON_SG552;
}

bool Bot::usesBadWeapon (void) {
   return m_currentWeapon == WEAPON_XM1014 || m_currentWeapon == WEAPON_M3 || m_currentWeapon == WEAPON_UMP45 || m_currentWeapon == WEAPON_MAC10 || m_currentWeapon == WEAPON_TMP || m_currentWeapon == WEAPON_P90;
}

int Bot::bestGrenadeCarried (void) {
   if (pev->weapons & (1 << WEAPON_EXPLOSIVE)) {
      return WEAPON_EXPLOSIVE;
   }
   else if (pev->weapons & (1 << WEAPON_SMOKE)) {
      return WEAPON_SMOKE;
   }
   else if (pev->weapons & (1 << WEAPON_FLASHBANG)) {
      return WEAPON_FLASHBANG;
   }
   return -1;
}

bool Bot::hasAnyWeapons (void) {
   return (pev->weapons & (WEAPON_PRIMARY | WEAPON_SECONDARY));
}

void Bot::selectBestWeapon (void) {
   // this function chooses best weapon, from weapons that bot currently own, and change
   // current weapon to best one.

   if (yb_jasonmode.boolean ()) {
      // if knife mode activated, force bot to use knife
      selectWeaponByName ("weapon_knife");
      return;
   }

   if (m_isReloading) {
      return;
   }
   WeaponSelect *tab = &g_weaponSelect[0];

   int selectIndex = 0;
   int chosenWeaponIndex = 0;

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id) {
      // is the bot NOT carrying this weapon?
      if (!(pev->weapons & (1 << tab[selectIndex].id))) {
         selectIndex++; // skip to next weapon
         continue;
      }

      int id = tab[selectIndex].id;
      bool ammoLeft = false;

      // is the bot already holding this weapon and there is still ammo in clip?
      if (tab[selectIndex].id == m_currentWeapon && (ammoClip () < 0 || ammoClip () >= tab[selectIndex].minPrimaryAmmo)) {
         ammoLeft = true;
      }

      // is no ammo required for this weapon OR enough ammo available to fire
      if (g_weaponDefs[id].ammo1 < 0 || (g_weaponDefs[id].ammo1 < 32 && m_ammo[g_weaponDefs[id].ammo1] >= tab[selectIndex].minPrimaryAmmo)) {
         ammoLeft = true;
      }

      if (ammoLeft) {
         chosenWeaponIndex = selectIndex;
      }
      selectIndex++;
   }

   chosenWeaponIndex %= NUM_WEAPONS + 1;
   selectIndex = chosenWeaponIndex;

   int id = tab[selectIndex].id;

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id) {
      selectWeaponByName (tab[selectIndex].weaponName);
   }
   m_isReloading = false;
   m_reloadState = RELOAD_NONE;
}

void Bot::selectSecondary (void) {
   int oldWeapons = pev->weapons;

   pev->weapons &= ~WEAPON_PRIMARY;
   selectBestWeapon ();

   pev->weapons = oldWeapons;
}

int Bot::bestWeaponCarried (void) {
   WeaponSelect *tab = &g_weaponSelect[0];

   int weapons = pev->weapons;
   int num = 0;
   int i = 0;

   // loop through all the weapons until terminator is found...
   while (tab->id) {
      // is the bot carrying this weapon?
      if (weapons & (1 << tab->id)) {
         num = i;
      }
      i++;
      tab++;
   }
   return num;
}

void Bot::selectWeaponByName (const char *name) {
   engine.execBotCmd (ent (), name);
}

void Bot::selectWeaponById (int num) {
   engine.execBotCmd (ent (), g_weaponSelect[num].weaponName);
}

void Bot::decideFollowUser (void) {
   // this function forces bot to follow user
   Array<edict_t *> users;

   // search friends near us
   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
         continue;
      }

      if (seesEntity (client.origin) && !isFakeClient (client.ent)) {
         users.push (client.ent);
      }
   }

   if (users.empty ()) {
      return;
   }
   m_targetEntity = users.random ();

   pushChatterMessage (CHATTER_LEAD_ON_SIR);
   startTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, INVALID_WAYPOINT_INDEX, 0.0f, true);
}

void Bot::processTeamCommands (void) {
   // prevent spamming
   if (m_timeTeamOrder > engine.timebase () + 2.0f || (g_gameFlags & GAME_CSDM_FFA) || !yb_communication_type.integer ()) {
      return;
   }

   bool memberNear = false;
   bool memberExists = false;

   // search teammates seen by this bot
   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
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
      if (m_personality == PERSONALITY_RUSHER && yb_communication_type.integer () == 2) {
         pushRadioMessage (RADIO_STORM_THE_FRONT);
      }
      else if (m_personality != PERSONALITY_RUSHER && yb_communication_type.integer () == 2) {
         pushRadioMessage (RADIO_TEAM_FALLBACK);
      }
   }
   else if (memberExists && yb_communication_type.integer () == 1) {
      pushRadioMessage (RADIO_TAKING_FIRE);
   }
   else if (memberExists && yb_communication_type.integer () == 2) {
      pushChatterMessage (CHATTER_SCARED_EMOTE);
   }
   m_timeTeamOrder = engine.timebase () + rng.getFloat (15.0f, 30.0f);
}

bool Bot::isGroupOfEnemies (const Vector &location, int numEnemies, float radius) {
   int numPlayers = 0;

   // search the world for enemy players...
   for (int i = 0; i < engine.maxClients (); i++) {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.ent == ent ()) {
         continue;
      }

      if ((client.ent->v.origin - location).lengthSq () < cr::square (radius)) {
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

void Bot::checkReload (void) {
   // check the reload state
   if (taskId () == TASK_PLANTBOMB || taskId () == TASK_DEFUSEBOMB || taskId () == TASK_PICKUPITEM || taskId () == TASK_THROWFLASHBANG || taskId () == TASK_THROWSMOKE || m_isUsingGrenade) {
      m_reloadState = RELOAD_NONE;
      return;
   }

   m_isReloading = false; // update reloading status
   m_reloadCheckTime = engine.timebase () + 3.0f;

   if (m_reloadState != RELOAD_NONE) {
      int weaponIndex = 0, maxClip = 0;
      int weapons = pev->weapons;

      if (m_reloadState == RELOAD_PRIMARY) {
         weapons &= WEAPON_PRIMARY;
      }
      else if (m_reloadState == RELOAD_SECONDARY) {
         weapons &= WEAPON_SECONDARY;
      }

      if (weapons == 0) {
         m_reloadState++;

         if (m_reloadState > RELOAD_SECONDARY) {
            m_reloadState = RELOAD_NONE;
         }
         return;
      }

      for (int i = 1; i < MAX_WEAPONS; i++) {
         if (weapons & (1 << i)) {
            weaponIndex = i;
            break;
         }
      }
      maxClip = getMaxClip (weaponIndex);

      if (m_ammoInClip[weaponIndex] < maxClip * 0.8f && g_weaponDefs[weaponIndex].ammo1 != -1 && g_weaponDefs[weaponIndex].ammo1 < 32 && m_ammo[g_weaponDefs[weaponIndex].ammo1] > 0) {
         if (m_currentWeapon != weaponIndex) {
            selectWeaponByName (g_weaponDefs[weaponIndex].className);
         }
         pev->button &= ~IN_ATTACK;

         if ((m_oldButtons & IN_RELOAD) == RELOAD_NONE) {
            pev->button |= IN_RELOAD; // press reload button
         }
         m_isReloading = true;
      }
      else {
         // if we have enemy don't reload next weapon
         if ((m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) || m_seeEnemyTime + 5.0f > engine.timebase ()) {
            m_reloadState = RELOAD_NONE;
            return;
         }
         m_reloadState++;

         if (m_reloadState > RELOAD_SECONDARY) {
            m_reloadState = RELOAD_NONE;
         }
         return;
      }
   }
}

int Bot::getMaxClip (int id) {
   int maxClip = 0;

   switch (id) {
   case WEAPON_M249:
      maxClip = 100;
      break;

   case WEAPON_P90:
      maxClip = 50;
      break;

   case WEAPON_GALIL:
      maxClip = 35;
      break;

   case WEAPON_ELITE:
   case WEAPON_MP5:
   case WEAPON_TMP:
   case WEAPON_MAC10:
   case WEAPON_M4A1:
   case WEAPON_AK47:
   case WEAPON_SG552:
   case WEAPON_AUG:
   case WEAPON_SG550:
      maxClip = 30;
      break;

   case WEAPON_UMP45:
   case WEAPON_FAMAS:
      maxClip = 25;
      break;

   case WEAPON_GLOCK:
   case WEAPON_FIVESEVEN:
   case WEAPON_G3SG1:
      maxClip = 20;
      break;

   case WEAPON_P228:
      maxClip = 13;
      break;

   case WEAPON_USP:
      maxClip = 12;
      break;

   case WEAPON_AWP:
   case WEAPON_SCOUT:
      maxClip = 10;
      break;

   case WEAPON_M3:
      maxClip = 8;
      break;

   case WEAPON_DEAGLE:
   case WEAPON_XM1014:
      maxClip = 7;
      break;
   }
   return maxClip;
}
