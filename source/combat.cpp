//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_shoots_thru_walls ("yb_shoots_thru_walls", "2");
ConVar yb_ignore_enemies ("yb_ignore_enemies", "0");
ConVar yb_csdm_mode ("yb_csdm_mode", "0", VT_NOSERVER);
ConVar yb_check_enemy_rendering ("yb_check_enemy_rendering", "0", VT_NOSERVER);

ConVar mp_friendlyfire ("mp_friendlyfire", NULL, VT_NOREGISTER);

int Bot::GetNearbyFriendsNearPosition(const Vector &origin, float radius)
{
   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
         continue;

      if ((g_clients[i].origin - origin).GetLengthSquared () < GET_SQUARE (radius))
         count++;
   }
   return count;
}

int Bot::GetNearbyEnemiesNearPosition(const Vector &origin, float radius)
{
   int count = 0;

   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team == m_team)
         continue;

      if ((g_clients[i].origin - origin).GetLengthSquared () < GET_SQUARE (radius))
         count++;
   }
   return count;
}

bool Bot::IsEnemyHiddenByRendering (edict_t *enemy)
{
   if (IsEntityNull (enemy) || !yb_check_enemy_rendering.GetBool ())
      return false;

   entvars_t &v = enemy->v;

   bool enemyHasGun = (v.weapons & WEAPON_PRIMARY) || (v.weapons & WEAPON_SECONDARY);
   bool enemyGunfire = (v.button & IN_ATTACK) || (v.oldbuttons & IN_ATTACK);

   if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && (!enemyGunfire || !enemyHasGun))
      return true;

   if ((v.renderfx == kRenderFxExplode || (v.effects & EF_NODRAW)) && enemyGunfire && enemyHasGun)
      return false;

   if (v.renderfx != kRenderFxHologram && v.renderfx != kRenderFxExplode && v.rendermode != kRenderNormal)
   {
      if (v.renderfx == kRenderFxGlowShell)
      {
         if (v.renderamt <= 20.0f && v.rendercolor.x <= 20.0f && v.rendercolor.y <= 20.f && v.rendercolor.z <= 20.f)
         {
            if (!enemyGunfire || !enemyHasGun)
               return true;

            return false;
         }
         else if (!enemyGunfire && v.renderamt <= 60.0f && v.rendercolor.x <= 60.f && v.rendercolor.y <= 60.0f && v.rendercolor.z <= 60.0f)
            return true;
      }
      else if (v.renderamt <= 20.0f)
      {
         if (!enemyGunfire || !enemyHasGun)
            return true;

         return false;
      }
      else if (!enemyGunfire && v.renderamt <= 60.0f)
         return true;
   }
   return false;
}

bool Bot::CheckVisibility (edict_t *target, Vector *origin, byte *bodyPart)
{
   // this function checks visibility of a bot target.

   if (IsEnemyHiddenByRendering (target))
   {
      *bodyPart = 0;
      *origin = nullvec;

      return false;
   }

   const Vector &botHead = EyePosition ();
   TraceResult tr;

   *bodyPart = 0;

   // check for the body
   TraceLine (botHead, target->v.origin, true, true, GetEntity (), &tr);

   if (tr.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_BODY;
      *origin = target->v.origin;

      if (m_difficulty == 4)
         origin->z += 3.0f;
   }

   // check for the head
   TraceLine (botHead, target->v.origin + target->v.view_ofs, true, true, GetEntity (), &tr);

   if (tr.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_HEAD;
      *origin = target->v.origin + target->v.view_ofs;

      if (m_difficulty == 4)
         origin->z += 1.0f;
   }

   if (*bodyPart != 0)
      return true;

   // thanks for this code goes to kwo
   MakeVectors (target->v.angles);

   // worst case, choose random position in enemy body
   for (int i = 0; i < 6; i++)
   {
      Vector pos = target->v.origin;

      switch (i)
      {
      case 0: // left arm
         pos.x -= 10.0f * g_pGlobals->v_right.x;
         pos.y -= 10.0f * g_pGlobals->v_right.y;
         pos.z += 8.0f;
         break;

      case 1: // right arm
         pos.x += 10.0f * g_pGlobals->v_right.x;
         pos.y += 10.0f * g_pGlobals->v_right.y;
         pos.z += 8.0f;
         break;

      case 2: // left leg
         pos.x -= 10.0f * g_pGlobals->v_right.x;
         pos.y -= 10.0f * g_pGlobals->v_right.y;
         pos.z -= 12.0f;
         break;

      case 3: // right leg
         pos.x += 10.0f * g_pGlobals->v_right.x;
         pos.y += 10.0f * g_pGlobals->v_right.y;
         pos.z -= 12.0f;
         break;

      case 4: // left foot
         pos.x -= 10.0f * g_pGlobals->v_right.x;
         pos.y -= 10.0f * g_pGlobals->v_right.y;
         pos.z -= 24.0f;
         break;

      case 5: // right foot
         pos.x += 10.0f * g_pGlobals->v_right.x;
         pos.y += 10.0f * g_pGlobals->v_right.y;
         pos.z -= 24.0f;
         break;
      }

      // check direct line to random part of the player body
      TraceLine (botHead, pos, true, true, GetEntity (), &tr);

      // check if we hit something
      if (tr.flFraction >= 1.0f)
      {
         *origin = tr.vecEndPos;
         *bodyPart |= VISIBLE_OTHER;

         return true;
      }
   }
   return false;
}

bool Bot::IsEnemyViewable (edict_t *player)
{
   if (IsEntityNull (player))
      return false;

   bool forceTrueIfVisible = false;

   if (IsValidPlayer (pev->dmg_inflictor) && GetTeam (pev->dmg_inflictor) != m_team)
      forceTrueIfVisible = true;

   if ((IsInViewCone (player->v.origin + pev->view_ofs) || forceTrueIfVisible) && CheckVisibility (player, &m_enemyOrigin, &m_visibility))
   {
      m_seeEnemyTime = GetWorldTime ();
      m_lastEnemy = player;
      m_lastEnemyOrigin = player->v.origin;

      return true;
   }
   return false;
}

bool Bot::LookupEnemy (void)
{
   // this function tries to find the best suitable enemy for the bot

   // do not search for enemies while we're blinded, or shooting disabled by user
   if (m_blindTime > GetWorldTime () || yb_ignore_enemies.GetBool ())
      return false;

   edict_t *player, *newEnemy = NULL;

   float nearestDistance = m_viewDistance;

   // setup potentially visible set for this bot
   Vector potentialVisibility = EyePosition ();

   if (pev->flags & FL_DUCKING)
      potentialVisibility = potentialVisibility + (VEC_HULL_MIN - VEC_DUCK_HULL_MIN);

   byte *pvs = ENGINE_SET_PVS (reinterpret_cast <float *> (&potentialVisibility));

   // clear suspected flag
   if (m_seeEnemyTime + 3.0f < GetWorldTime ())
      m_states &= ~STATE_SUSPECT_ENEMY;

   if (!IsEntityNull (m_enemy) && m_enemyUpdateTime > GetWorldTime ())
   {
      player = m_enemy;

      // is player is alive
      if (IsAlive (player) && IsEnemyViewable (player))
         newEnemy = player;
   }

   // the old enemy is no longer visible or
   if (IsEntityNull (newEnemy))
   {
      m_enemyUpdateTime = GetWorldTime () + 0.5f;

      // ignore shielded enemies, while we have real one
      edict_t *shieldEnemy = NULL;

      // search the world for players...
      for (int i = 0; i < GetMaxClients (); i++)
      {
         if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || (g_clients[i].team == m_team) || (g_clients[i].ent == GetEntity ()))
            continue;

         player = g_clients[i].ent;

         // let the engine check if this player is potentially visible
         if (!ENGINE_CHECK_VISIBILITY (player, pvs))
            continue;

         // skip glowed players, in free for all mode, we can't hit them
         if (player->v.renderfx == kRenderFxGlowShell && yb_csdm_mode.GetInt () >= 1)
            continue;

         // do some blind by smoke grenade
         if (m_blindRecognizeTime < GetWorldTime () && IsBehindSmokeClouds (player))
         {
            m_blindRecognizeTime = GetWorldTime () + Random.Float (1.0, 2.0);

            if (Random.Long (0, 100) < 50)
               ChatterMessage (Chatter_BehindSmoke);
         }

         if (player->v.button & (IN_ATTACK | IN_ATTACK2))
            m_blindRecognizeTime = GetWorldTime () - 0.1;

         // see if bot can see the player...
         if (m_blindRecognizeTime < GetWorldTime () && IsEnemyViewable (player))
         {
            if (IsEnemyProtectedByShield (player))
            {
               shieldEnemy = player;
               continue;
            }
            float distance = (player->v.origin - pev->origin).GetLength ();

            if (distance < nearestDistance)
            {
               nearestDistance = distance;
               newEnemy = player;

               // aim VIP first on AS maps...
               if (IsPlayerVIP (newEnemy))
                  break;
            }
         }
      }

      if (IsEntityNull (newEnemy) && !IsEntityNull (shieldEnemy))
         newEnemy = shieldEnemy;
   }

   if (IsValidPlayer (newEnemy))
   {
      g_botsCanPause = true;

      m_aimFlags |= AIM_ENEMY;
      m_states |= STATE_SEEING_ENEMY;

      if (newEnemy == m_enemy)
      {
         // if enemy is still visible and in field of view, keep it keep track of when we last saw an enemy
         m_seeEnemyTime = GetWorldTime ();

         // zero out reaction time
         m_actualReactionTime = 0.0;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
      else
      {
         if (m_seeEnemyTime + 3.0 < GetWorldTime () && (m_hasC4 || HasHostage () || !IsEntityNull (m_targetEntity)))
            RadioMessage (Radio_EnemySpotted);

         m_targetEntity = NULL; // stop following when we see an enemy...

         if (Random.Long (0, 100) < m_difficulty * 25)
            m_enemySurpriseTime = GetWorldTime () + m_actualReactionTime * 0.5f;
         else
            m_enemySurpriseTime = GetWorldTime () + m_actualReactionTime;

         // zero out reaction time
         m_actualReactionTime = 0.0;
         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;
         m_enemyReachableTimer = 0.0;

         // keep track of when we last saw an enemy
         m_seeEnemyTime = GetWorldTime ();

         // now alarm all teammates who see this bot & don't have an actual enemy of the bots enemy should simulate human players seeing a teammate firing
         for (int j = 0; j < GetMaxClients (); j++)
         {
            if (!(g_clients[j].flags & CF_USED) || !(g_clients[j].flags & CF_ALIVE) || g_clients[j].team != m_team || g_clients[j].ent == GetEntity ())
               continue;

            Bot *friendBot = botMgr->GetBot (g_clients[j].ent);

            if (friendBot != NULL)
            {
               if (friendBot->m_seeEnemyTime + 2.0f < GetWorldTime () || IsEntityNull (friendBot->m_lastEnemy))
               {
                  if (IsVisible (pev->origin, ENT (friendBot->pev)))
                  {
                     friendBot->m_lastEnemy = newEnemy;
                     friendBot->m_lastEnemyOrigin = m_lastEnemyOrigin;
                     friendBot->m_seeEnemyTime = GetWorldTime ();
                  }
               }
            }
         }
         return true;
      }
   }
   else if (!IsEntityNull (m_enemy))
   {
      newEnemy = m_enemy;
      m_lastEnemy = newEnemy;

      if (!IsAlive (newEnemy))
      {
         m_enemy = NULL;
  
         // shoot at dying players if no new enemy to give some more human-like illusion
         if (m_seeEnemyTime + 0.1f > GetWorldTime ())
         {
            if (!UsesSniper ())
            {
               m_shootAtDeadTime = GetWorldTime () + 0.4f;
               m_actualReactionTime = 0.0;
               m_states |= STATE_SUSPECT_ENEMY;

               return true;
            }
            return false;
         }
         else if (m_shootAtDeadTime > GetWorldTime ())
         {
            m_actualReactionTime = 0.0;
            m_states |= STATE_SUSPECT_ENEMY;

            return true;
         }
         return false;
      }

      // if no enemy visible check if last one shoot able through wall
      if (yb_shoots_thru_walls.GetBool () && m_difficulty >= 2 && IsShootableThruObstacle (newEnemy->v.origin))
      {
         m_seeEnemyTime = GetWorldTime ();

         m_states |= STATE_SUSPECT_ENEMY;
         m_aimFlags |= AIM_LAST_ENEMY;

         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
   }

   // check if bots should reload...
   if ((m_aimFlags <= AIM_PREDICT_PATH && m_seeEnemyTime + 3.0 < GetWorldTime () &&  !(m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) && IsEntityNull (m_lastEnemy) && IsEntityNull (m_enemy) && GetTaskId () != TASK_SHOOTBREAKABLE && GetTaskId () != TASK_PLANTBOMB && GetTaskId () != TASK_DEFUSEBOMB) || g_roundEnded)
   {
      if (!m_reloadState)
         m_reloadState = RELOAD_PRIMARY;
   }

   // is the bot using a sniper rifle or a zoomable rifle?
   if ((UsesSniper () || UsesZoomableRifle ()) && m_zoomCheckTime + 1.0 < GetWorldTime ())
   {
      if (pev->fov < 90) // let the bot zoom out
         pev->button |= IN_ATTACK2;
      else
         m_zoomCheckTime = 0.0;
   }
   return false;
}

const Vector &Bot::GetAimPosition (void)
{
   // the purpose of this function, is to make bot aiming not so ideal. it's mutate m_enemyOrigin enemy vector
   // returned from visibility check function.

   float distance = (m_enemy->v.origin - pev->origin).GetLength ();

   // get enemy position initially
   Vector targetOrigin = m_enemy->v.origin;
   Vector randomize = nullvec;

   const Vector &adjust = Vector (Random.Float (m_enemy->v.mins.x * 0.5f, m_enemy->v.maxs.x * 0.5f), Random.Float (m_enemy->v.mins.y * 0.5f, m_enemy->v.maxs.y * 0.5f), Random.Float (m_enemy->v.mins.z * 0.5f, m_enemy->v.maxs.z * 0.5f));

   // do not aim at head, at long distance (only if not using sniper weapon)
   if ((m_visibility & VISIBLE_BODY) && !UsesSniper () && !UsesPistol () && (distance > (m_difficulty == 4 ? 2400.0 : 1200.0)))
      m_visibility &= ~VISIBLE_HEAD;

   // if we only suspect an enemy behind a wall take the worst skill
   if ((m_states & STATE_SUSPECT_ENEMY) && !(m_states & STATE_SEEING_ENEMY))
      targetOrigin = targetOrigin + adjust;
   else
   {
      // now take in account different parts of enemy body
      if (m_visibility & (VISIBLE_HEAD | VISIBLE_BODY)) // visible head & body
      {
         int headshotFreq[5] = { 20, 40, 60, 80, 100 };

         // now check is our skill match to aim at head, else aim at enemy body
         if ((Random.Long (1, 100) < headshotFreq[m_difficulty]) || UsesPistol ())
            targetOrigin = targetOrigin + m_enemy->v.view_ofs + Vector (0.0f, 0.0f, GetZOffset (distance));
         else
            targetOrigin = targetOrigin + Vector (0.0f, 0.0f, GetZOffset (distance));
      }
      else if (m_visibility & VISIBLE_BODY) // visible only body
         targetOrigin = targetOrigin + Vector (0.0f, 0.0f, GetZOffset (distance));
      else if (m_visibility & VISIBLE_OTHER) // random part of body is visible
         targetOrigin = m_enemyOrigin;
      else if (m_visibility & VISIBLE_HEAD) // visible only head
         targetOrigin = targetOrigin + m_enemy->v.view_ofs + Vector (0.0f, 0.0f, GetZOffset (distance));
      else // something goes wrong, use last enemy origin
      {
         targetOrigin = m_lastEnemyOrigin;
         
         if (m_difficulty < 3)
            randomize = adjust;
      }
      m_lastEnemyOrigin = targetOrigin;
   }
   const Vector &velocity = UsesSniper () ? nullvec : ((1.0f * m_frameInterval * m_enemy->v.velocity - 1.0 * m_frameInterval * pev->velocity) * m_frameInterval).Get2D ();

   if (m_difficulty < 3 && randomize != nullvec)
   {
      float divOffs = (m_enemyOrigin - pev->origin).GetLength ();

      if (pev->fov < 40)
         divOffs = divOffs / 2000;
      else if (pev->fov < 90)
         divOffs = divOffs / 1000;
      else
         divOffs = divOffs / 500;

      // randomize the target position
      m_enemyOrigin = divOffs * randomize + velocity;
   }
   else
      m_enemyOrigin = targetOrigin + velocity;

   return m_enemyOrigin;
}

float Bot::GetZOffset (float distance)
{
   // got it from pbmm

   bool sniper = UsesSniper ();
   bool pistol = UsesPistol ();
   bool rifle = UsesRifle ();

   bool zoomableRifle = UsesZoomableRifle ();
   bool submachine = UsesSubmachineGun ();
   bool shotgun = (m_currentWeapon == WEAPON_XM1014 || m_currentWeapon == WEAPON_M3);
   bool m249 = m_currentWeapon == WEAPON_M249;

   const float BurstDistance = 300.0f;
   const float DoubleBurstDistance = BurstDistance * 2;

   float result = 3.5f;

   if (distance < 2800.0f && distance > DoubleBurstDistance)
   {
      if (sniper) result = 3.5f;
      else if (zoomableRifle) result = 4.5f;
      else if (pistol) result = 6.5f;
      else if (submachine) result = 5.5f;
      else if (rifle) result = 5.5f;
      else if (m249) result = 2.5f;
      else if (shotgun) result = 10.5f;
   }
   else if (distance > BurstDistance && distance <= DoubleBurstDistance)
   {
      if (sniper) result = 3.5f;
      else if (zoomableRifle) result = 3.5f;
      else if (pistol) result = 6.5f;
      else if (submachine) result = 3.5f;
      else if (rifle) result = 1.6f;
      else if (m249) result = -1.0f;
      else if (shotgun) result = 10.0f;
   }
   else if (distance < BurstDistance)
   {
      if (sniper) result = 4.5f;
      else if (zoomableRifle) result = -5.0f;
      else if (pistol) result = 4.5f;
      else if (submachine) result = -4.5f;
      else if (rifle) result = -4.5f;
      else if (m249) result = -6.0f;
      else if (shotgun) result = -5.0f;
   }
   return result;
}

bool Bot::IsFriendInLineOfFire (float distance)
{
   // bot can't hurt teammates, if friendly fire is not enabled...
   if (!mp_friendlyfire.GetBool () || yb_csdm_mode.GetInt () > 0)
      return false;

   MakeVectors (pev->v_angle);

   TraceResult tr;
   TraceLine (EyePosition (), EyePosition () + 10000.0f * pev->v_angle, false, false, GetEntity (), &tr);

   // check if we hit something
   if (!IsEntityNull (tr.pHit) && tr.pHit != g_worldEntity)
   {
      int playerIndex = IndexOfEntity (tr.pHit) - 1;

      // check valid range
      if (playerIndex >= 0 && playerIndex < GetMaxClients () && g_clients[playerIndex].team == m_team && (g_clients[playerIndex].flags & CF_ALIVE))
         return true;
   }

   // search the world for players
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
         continue;

      edict_t *ent = g_clients[i].ent;

      float friendDistance = (ent->v.origin - pev->origin).GetLength ();
      float squareDistance = sqrtf (1089.0 + (friendDistance * friendDistance));

      if (GetShootingConeDeviation (GetEntity (), &ent->v.origin) > (friendDistance * friendDistance) / (squareDistance * squareDistance) && friendDistance <= distance)
         return true;
   }
   return false;
}

bool Bot::IsShootableThruObstacle (const Vector &dest)
{
   // this function returns true if enemy can be shoot through some obstacle, false otherwise.
   // credits goes to Immortal_BLG

   if (yb_shoots_thru_walls.GetInt () == 2)
      return IsShootableThruObstacleEx (dest);

   if (m_difficulty < 2)
      return false;

   int penetratePower = GetWeaponPenetrationPower (m_currentWeapon);

   if (penetratePower == 0)
      return false;

   TraceResult tr;

   float obstacleDistance = 0.0f;
   TraceLine (EyePosition (), dest, true, GetEntity (), &tr);

   if (tr.fStartSolid)
   {
      const Vector &source = tr.vecEndPos;
      TraceLine (dest, source, true, GetEntity (), &tr);

      if (tr.flFraction != 1.0f)
      {
         if ((tr.vecEndPos - dest).GetLengthSquared () > GET_SQUARE (800.0f))
            return false;

         if (tr.vecEndPos.z >= dest.z + 200.0f)
            return false;

         obstacleDistance = (tr.vecEndPos - source).GetLength ();
      }
   }

   if (obstacleDistance > 0.0f)
   {
      while (penetratePower > 0)
      {
         if (obstacleDistance > 75.0f)
         {
            obstacleDistance -= 75.0f;
            penetratePower--;

            continue;
         }
         return true;
      }
   }
   return false;
}

bool Bot::IsShootableThruObstacleEx (const Vector &dest)
{
   // this function returns if enemy can be shoot through some obstacle

   if (m_difficulty < 2 || GetWeaponPenetrationPower (m_currentWeapon) == 0)
      return false;

   Vector source = EyePosition ();
   Vector direction = (dest - source).Normalize ();  // 1 unit long
   Vector point = nullvec;

   int thikness = 0;
   int numHits = 0;

   TraceResult tr;
   TraceLine (source, dest, true, true, GetEntity (), &tr);

   while (tr.flFraction != 1.0 && numHits < 3)
   {
      numHits++;
      thikness++;

      point = tr.vecEndPos + direction;

      while (POINT_CONTENTS (point) == CONTENTS_SOLID && thikness < 98)
      {
         point = point + direction;
         thikness++;
      }
      TraceLine (point, dest, true, true, GetEntity (), &tr);
   }

   if (numHits < 3 && thikness < 98)
   {
      if ((dest - point).GetLengthSquared () < 13143)
         return true;
   }
   return false;
}

bool Bot::DoFirePause (float distance)
{
   // returns true if bot needs to pause between firing to compensate for punchangle & weapon spread

   if (UsesSniper ())
   {
      m_shootTime = GetWorldTime ();
      return false;
   }

   if (m_firePause > GetWorldTime ())
      return true;

   if ((m_aimFlags & AIM_ENEMY) && m_enemyOrigin != nullvec)
   {
      if (IsEnemyProtectedByShield (m_enemy) && GetShootingConeDeviation (GetEntity (), &m_enemyOrigin) > 0.92f)
         return true;
   }

   float offset = 0.0f;
   const float BurstDistance = 300.0f;

   if (distance < BurstDistance)
      return false;
   else if (distance < 2 * BurstDistance)
      offset = 10.0;
   else
      offset = 5.0;

   const float xPunch = DegreeToRadian (pev->punchangle.x);
   const float yPunch = DegreeToRadian (pev->punchangle.y);

   // check if we need to compensate recoil
   if (tanf (sqrtf (fabsf (xPunch * xPunch) + fabsf (yPunch * yPunch))) * distance > offset + 30.0f + ((100 - (m_difficulty * 25)) / 100.f))
   {
      if (m_firePause < GetWorldTime () - 0.4f)
         m_firePause = GetWorldTime () + Random.Float (0.4f, 0.4f + 0.3f * ((100 - (m_difficulty * 25)) / 100.f));

      return true;
   }
   return false;
}

void Bot::FireWeapon (void)
{
   // this function will return true if weapon was fired, false otherwise
   float distance = (m_lookAt - EyePosition ()).GetLength (); // how far away is the enemy?

   // if using grenade stop this
   if (m_isUsingGrenade)
   {
      m_shootTime = GetWorldTime () + 0.1;
      return;
   }

   // or if friend in line of fire, stop this too but do not update shoot time
   if (!IsEntityNull (m_enemy))
   {
      if (IsFriendInLineOfFire (distance))
      {
         m_fightStyle = 0;
         m_lastFightStyleCheck = GetWorldTime ();

         return;
      }
   }
   WeaponSelect *selectTab = &g_weaponSelect[0];

   edict_t *enemy = m_enemy;

   int selectId = WEAPON_KNIFE, selectIndex = 0, chosenWeaponIndex = 0;
   int weapons = pev->weapons;

   // if jason mode use knife only
   if (yb_jasonmode.GetBool ())
      goto WeaponSelectEnd;

   // use knife if near and good difficulty (l33t dude!)
   if (m_difficulty >= 3 && pev->health > 80 && !IsEntityNull (enemy) && pev->health >= enemy->v.health && distance < 100.0f && !IsGroupOfEnemies (pev->origin))
      goto WeaponSelectEnd;

   // loop through all the weapons until terminator is found...
   while (selectTab[selectIndex].id)
   {
      // is the bot carrying this weapon?
      if (weapons & (1 << selectTab[selectIndex].id))
      {
         // is enough ammo available to fire AND check is better to use pistol in our current situation...
         if (m_ammoInClip[selectTab[selectIndex].id] > 0 && !IsWeaponBadInDistance (selectIndex, distance))
            chosenWeaponIndex = selectIndex;
      }
      selectIndex++;
   }
   selectId = selectTab[chosenWeaponIndex].id;

   // if no available weapon...
   if (chosenWeaponIndex == 0)
   {
      selectIndex = 0;

      // loop through all the weapons until terminator is found...
      while (selectTab[selectIndex].id)
      {
         int id = selectTab[selectIndex].id;

         // is the bot carrying this weapon?
         if (weapons & (1 << id))
         {
            if (g_weaponDefs[id].ammo1 != -1 && m_ammo[g_weaponDefs[id].ammo1] >= selectTab[selectIndex].minPrimaryAmmo)
            {
               // available ammo found, reload weapon
               if (m_reloadState == RELOAD_NONE || m_reloadCheckTime > GetWorldTime ())
               {
                  m_isReloading = true;
                  m_reloadState = RELOAD_PRIMARY;
                  m_reloadCheckTime = GetWorldTime ();

                  RadioMessage (Radio_NeedBackup);
               }
               return;
            }
         }
         selectIndex++;
      }
      selectId = WEAPON_KNIFE; // no available ammo, use knife!
   }

WeaponSelectEnd:

   // we want to fire weapon, don't reload now
   if (!m_isReloading)
   {
      m_reloadState = RELOAD_NONE;
      m_reloadCheckTime = GetWorldTime () + 3.0;
   }

   // select this weapon if it isn't already selected
   if (m_currentWeapon != selectId)
   {
      SelectWeaponByName (g_weaponDefs[selectId].className);

      // reset burst fire variables
      m_firePause = 0.0;
      m_timeLastFired = 0.0;

      return;
   }

   if (selectTab[chosenWeaponIndex].id != selectId)
   {
      chosenWeaponIndex = 0;

      // loop through all the weapons until terminator is found...
      while (selectTab[chosenWeaponIndex].id)
      {
         if (selectTab[chosenWeaponIndex].id == selectId)
            break;

         chosenWeaponIndex++;
      }
   }

   // if we're have a glock or famas vary burst fire mode
   CheckBurstMode (distance);

   if (HasShield () && m_shieldCheckTime < GetWorldTime () && GetTaskId () != TASK_CAMP) // better shield gun usage
   {
      if (distance >= 750 && !IsShieldDrawn ())
         pev->button |= IN_ATTACK2; // draw the shield
      else if (IsShieldDrawn () || (!IsEntityNull (m_enemy) && (m_enemy->v.button & IN_RELOAD) || !IsEnemyViewable(m_enemy)))
         pev->button |= IN_ATTACK2; // draw out the shield

      m_shieldCheckTime = GetWorldTime () + 1.0;
   }

   if (UsesSniper () && m_zoomCheckTime < GetWorldTime ()) // is the bot holding a sniper rifle?
   {
      if (distance > 1500 && pev->fov >= 40) // should the bot switch to the long-range zoom?
         pev->button |= IN_ATTACK2;

      else if (distance > 150 && pev->fov >= 90) // else should the bot switch to the close-range zoom ?
         pev->button |= IN_ATTACK2;

      else if (distance <= 150 && pev->fov < 90) // else should the bot restore the normal view ?
         pev->button |= IN_ATTACK2;

      m_zoomCheckTime = GetWorldTime ();

      if (!IsEntityNull (m_enemy) && (pev->velocity.x != 0 || pev->velocity.y != 0 || pev->velocity.z != 0) && (pev->basevelocity.x != 0 || pev->basevelocity.y != 0 || pev->basevelocity.z != 0))
      {
         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;
         m_navTimeset = GetWorldTime ();
      }
   }
   else if (m_difficulty <= 3 && UsesZoomableRifle () && m_zoomCheckTime < GetWorldTime ()) // else is the bot holding a zoomable rifle?
   {
      if (distance > 800 && pev->fov >= 90) // should the bot switch to zoomed mode?
         pev->button |= IN_ATTACK2;

      else if (distance <= 800 && pev->fov < 90) // else should the bot restore the normal view?
         pev->button |= IN_ATTACK2;

      m_zoomCheckTime = GetWorldTime ();
   }

   if (selectId != WEAPON_KNIFE && HasPrimaryWeapon () && GetAmmoInClip () <= 0)
   {
      if (GetAmmo () <= 0 && !(m_states &= ~(STATE_THROW_HE | STATE_THROW_FB | STATE_THROW_SG)))
         SelectPistol();

      pev->button |= IN_RELOAD;
      pev->button &= ~IN_ATTACK;

      return;
   }

   // need to care for burst fire?
   if (distance < 256.0 || m_blindTime > GetWorldTime ())
   {
      if (selectId == WEAPON_KNIFE)
      {
         if (distance < 64.0f)
         { 
            if (Random.Long (1, 100) < 15 || HasShield ())
               pev->button |= IN_ATTACK; // use primary attack
            else
               pev->button |= IN_ATTACK2; // use secondary attack
         }
      }    
      else
      {
         LookupEnemy ();

         if (selectTab[chosenWeaponIndex].primaryFireHold && m_ammo[g_weaponDefs[selectTab[selectIndex].id].ammo1] >= selectTab[selectIndex].minPrimaryAmmo) // if automatic weapon, just press attack
            pev->button |= IN_ATTACK;
         else // if not, toggle buttons
         {       
            if ((pev->oldbuttons & IN_ATTACK) == 0)      
               pev->button |= IN_ATTACK;
         }
      }
      m_shootTime = GetWorldTime ();
   }
   else
   {
      if (DoFirePause (distance))
         return;

      // don't attack with knife over long distance
      if (selectId == WEAPON_KNIFE)
      {
         m_shootTime = GetWorldTime ();
         return;
      }

      if (selectTab[chosenWeaponIndex].primaryFireHold)
      {
         m_shootTime = GetWorldTime ();
         m_zoomCheckTime = GetWorldTime ();
    
         pev->button |= IN_ATTACK;  // use primary attack      
      }
      else
      {
         pev->button |= IN_ATTACK;

         // m_shootTime = GetWorldTime () + baseDelay + Random.Float (minDelay, maxDelay);
         m_shootTime = GetWorldTime () + Random.Float (0.15f, 0.35f);
         m_zoomCheckTime = GetWorldTime () - 0.09f;
      }
   }
}

bool Bot::IsWeaponBadInDistance (int weaponIndex, float distance)
{
   // this function checks, is it better to use pistol instead of current primary weapon
   // to attack our enemy, since current weapon is not very good in this situation.

   int weaponID = g_weaponSelect[weaponIndex].id;

   if (weaponID == WEAPON_KNIFE)
	   return false;

   // check is ammo available for secondary weapon
   if (m_ammoInClip[g_weaponSelect[GetBestSecondaryWeaponCarried ()].id] >= 1)
      return false;

   // better use pistol in short range distances, when using sniper weapons
   if ((weaponID == WEAPON_SCOUT || weaponID == WEAPON_AWP || weaponID == WEAPON_G3SG1 || weaponID == WEAPON_SG550) && distance < 500.0f)
      return true;

   // shotguns is too inaccurate at long distances, so weapon is bad
   if ((weaponID == WEAPON_M3 || weaponID == WEAPON_XM1014) && distance > 750.0f)
      return true;

   return false;
}

void Bot::FocusEnemy (void)
{
   // aim for the head and/or body
   Vector enemyOrigin = GetAimPosition ();
   m_lookAt = enemyOrigin;

   if (m_enemySurpriseTime > GetWorldTime ())
      return;

   enemyOrigin = (enemyOrigin - EyePosition ()).Get2D ();

   float distance = enemyOrigin.GetLength ();  // how far away is the enemy scum?

   if (distance < 128.0f)
   {
      if (m_currentWeapon == WEAPON_KNIFE)
      {
         if (distance <= 80.0f)
            m_wantsToFire = true;
      }
      else
         m_wantsToFire = GetShootingConeDeviation (GetEntity (), &m_enemyOrigin) > 0.8f;
   }
   else
   {
      float dot = GetShootingConeDeviation (GetEntity (), &m_enemyOrigin);

      if (dot < 0.90f)
         m_wantsToFire = false;
      else
      {
         float enemyDot = GetShootingConeDeviation (m_enemy, &pev->origin);

         // enemy faces bot?
         if (enemyDot >= 0.90f)
            m_wantsToFire = true;
         else
         {
            if (dot > 0.99f)
               m_wantsToFire = true;
            else
               m_wantsToFire = false;
         }
         
      }
   }
}

void Bot::CombatFight (void)
{
   // no enemy? no need to do strafing
   if (IsEntityNull (m_enemy))
      return;

   Vector enemyOrigin = m_lookAt;

   if (m_currentWeapon == WEAPON_KNIFE)
      m_destOrigin = m_enemy->v.origin;

   enemyOrigin = (enemyOrigin - EyePosition ()).Get2D (); // ignore z component (up & down)

   float distance = enemyOrigin.GetLength ();  // how far away is the enemy scum?

   if (m_timeWaypointMove + m_frameInterval + 0.5f < GetWorldTime ())
   {
      int approach;

      if ((m_states & STATE_SUSPECT_ENEMY) && !(m_states & STATE_SEEING_ENEMY)) // if suspecting enemy stand still
         approach = 49;
      else if (m_isReloading || m_isVIP) // if reloading or vip back off
         approach = 29;
      else if (m_currentWeapon == WEAPON_KNIFE) // knife?
         approach = 100;
      else
      {
         approach = static_cast <int> (pev->health * m_agressionLevel);

         if (UsesSniper () && approach > 49)
            approach = 49;
      }

      if (UsesPistol() && !((m_enemy->v.weapons & WEAPON_SECONDARY) || (m_enemy->v.weapons & (1 << WEAPON_SG550))) && !g_bombPlanted)
      {
         m_fearLevel += 0.5;

         CheckGrenades();
         CheckThrow (EyePosition(), m_throw);

         if ((m_states & STATE_SEEING_ENEMY) && !m_hasC4)
            PushTask (TASK_SEEKCOVER, TASKPRI_SEEKCOVER, -1, Random.Long (10, 20), true);
      }

      // only take cover when bomb is not planted and enemy can see the bot or the bot is VIP
      if (approach < 30 && !g_bombPlanted && (IsInViewCone (m_enemy->v.origin) || m_isVIP))
      {
         m_moveSpeed = -pev->maxspeed;

         TaskItem *task = GetTask ();

         task->id = TASK_SEEKCOVER;
         task->resume = true;
         task->desire = TASKPRI_ATTACK + 1.0;
      }
      else if (approach < 50)
         m_moveSpeed = 0.0;
      else
         m_moveSpeed = pev->maxspeed;

      if (distance < 96 && m_currentWeapon != WEAPON_KNIFE)
         m_moveSpeed = -pev->maxspeed;

      if (UsesSniper ())
      {
         m_fightStyle = 1;
         m_lastFightStyleCheck = GetWorldTime ();
      }
      else if (UsesRifle () || UsesSubmachineGun ())
      {
         if (m_lastFightStyleCheck + 3.0 < GetWorldTime ())
         {
            int rand = Random.Long (1, 100);

            if (distance < 450)
               m_fightStyle = 0;
            else if (distance < 1024)
            {
               if (rand < (UsesSubmachineGun () ? 50 : 30))
                  m_fightStyle = 0;
               else
                  m_fightStyle = 1;
            }
            else
            {
               if (rand < (UsesSubmachineGun () ? 80 : 93))
                  m_fightStyle = 1;
               else
                  m_fightStyle = 0;
            }
            m_lastFightStyleCheck = GetWorldTime ();
         }
      }
      else
      {
         if (m_lastFightStyleCheck + 3.0 < GetWorldTime ())
         {
            if (Random.Long (0, 100) < 50)
               m_fightStyle = 1;
            else
               m_fightStyle = 0;

            m_lastFightStyleCheck = GetWorldTime ();
         }
      }

      if ((m_difficulty >= 1 && m_fightStyle == 0) || ((pev->button & IN_RELOAD) || m_isReloading) || (UsesPistol () && distance < 400.0f))
      {
         if (m_strafeSetTime < GetWorldTime ())
         {
            // to start strafing, we have to first figure out if the target is on the left side or right side
            MakeVectors (m_enemy->v.v_angle);

            const Vector &dirToPoint = (pev->origin - m_enemy->v.origin).Normalize2D ();
            const Vector &rightSide = g_pGlobals->v_right.Normalize2D ();

            if ((dirToPoint | rightSide) < 0)
               m_combatStrafeDir = 1;
            else
               m_combatStrafeDir = 0;

            if (Random.Long (1, 100) < 30)
               m_combatStrafeDir ^= 1;

            m_strafeSetTime = GetWorldTime () + Random.Float (0.5, 3.0);
         }

         if (m_combatStrafeDir == 0)
         {
            if (!CheckWallOnLeft ())
               m_strafeSpeed = -pev->maxspeed;
            else
            {
               m_combatStrafeDir ^= 1;
               m_strafeSetTime = GetWorldTime () + 1.0;
            }
         }
         else
         {
            if (!CheckWallOnRight ())
               m_strafeSpeed = -pev->maxspeed;
            else
            {
               m_combatStrafeDir ^= 1;
               m_strafeSetTime = GetWorldTime () + 1.0;
            }
         }

         if (m_difficulty >= 3 && (m_jumpTime + 5.0 < GetWorldTime () && IsOnFloor () && Random.Long (0, 1000) < (m_isReloading ? 8 : 2) && pev->velocity.GetLength2D () > 150.0) && !UsesSniper ())
            pev->button |= IN_JUMP;

         if (m_moveSpeed > 0.0 && distance > 100.0 && m_currentWeapon != WEAPON_KNIFE)
            m_moveSpeed = 0.0;

         if (m_currentWeapon == WEAPON_KNIFE)
            m_strafeSpeed = 0.0f;
      }
      else if (m_fightStyle == 1)
      {
         bool shouldDuck = true; // should duck

         // check the enemy height
         float enemyHalfHeight = ((m_enemy->v.flags & FL_DUCKING) == FL_DUCKING ? 36.0 : 72.0) / 2;

         // check center/feet
         if (!IsVisible (m_enemy->v.origin, GetEntity ()) && !IsVisible (m_enemy->v.origin + Vector (0, 0, -enemyHalfHeight), GetEntity ()))
            shouldDuck = false;

         if (shouldDuck && GetTaskId () != TASK_SEEKCOVER && GetTaskId () != TASK_HUNTENEMY && (m_visibility & VISIBLE_BODY) && !(m_visibility & VISIBLE_OTHER) && waypoint->IsDuckVisible (m_currentWaypointIndex, waypoint->FindNearest (m_enemy->v.origin)))
            m_duckTime = GetWorldTime () + 0.5f;

         m_moveSpeed = 0.0;
         m_strafeSpeed = 0.0;
         m_navTimeset = GetWorldTime ();
      }
   }

   if (m_duckTime > GetWorldTime ())
   {
      m_moveSpeed = 0.0;
      m_strafeSpeed = 0.0;
   }

   if (m_moveSpeed > 0.0f && m_currentWeapon != WEAPON_KNIFE)
      m_moveSpeed = GetWalkSpeed ();

   if (m_isReloading)
   {
      m_moveSpeed = -pev->maxspeed;
      m_duckTime = GetWorldTime () - 1.0f;
   }

   if (!IsInWater () && !IsOnLadder () && (m_moveSpeed > 0.0f || m_strafeSpeed >= 0.0f))
   {
      MakeVectors (pev->v_angle);

      if (IsDeadlyDrop (pev->origin + (g_pGlobals->v_forward * m_moveSpeed * 0.2) + (g_pGlobals->v_right * m_strafeSpeed * 0.2) + (pev->velocity * m_frameInterval)))
      {
         m_strafeSpeed = -m_strafeSpeed;
         m_moveSpeed = -m_moveSpeed;

         pev->button &= ~IN_JUMP;
      }
   }

   m_isStuck = false;
   m_lastCollTime = GetWorldTime () + 0.5f;
}

bool Bot::HasPrimaryWeapon (void)
{
   // this function returns returns true, if bot has a primary weapon

   return (pev->weapons & WEAPON_PRIMARY) != 0;  
}

bool Bot::HasSecondaryWeapon (void)
{
   // this function returns returns true, if bot has a secondary weapon

   return (pev->weapons & WEAPON_SECONDARY) != 0;
}

bool Bot::HasShield (void)
{
   // this function returns true, if bot has a tactical shield

   return strncmp (STRING (pev->viewmodel), "models/shield/v_shield_", 23) == 0;
}

bool Bot::IsShieldDrawn (void)
{
   // this function returns true, is the tactical shield is drawn

   if (!HasShield ())
      return false;

   return pev->weaponanim == 6 || pev->weaponanim == 7;
}

bool Bot::IsEnemyProtectedByShield (edict_t *enemy)
{
   // this function returns true, if enemy protected by the shield

   if (IsEntityNull (enemy) || IsShieldDrawn ())
      return false;

   // check if enemy has shield and this shield is drawn
   if (strncmp (STRING (enemy->v.viewmodel), "models/shield/v_shield_", 23) == 0 && (enemy->v.weaponanim == 6 || enemy->v.weaponanim == 7))
   {
      if (::IsInViewCone (pev->origin, enemy))
         return true;
   }
   return false;
}

bool Bot::UsesSniper (void)
{
   // this function returns true, if returns if bot is using a sniper rifle

   return m_currentWeapon == WEAPON_AWP || m_currentWeapon == WEAPON_G3SG1 || m_currentWeapon == WEAPON_SCOUT || m_currentWeapon == WEAPON_SG550;
}

bool Bot::UsesRifle (void)
{
   WeaponSelect *selectTab = &g_weaponSelect[0];
   int count = 0;

   while (selectTab->id)
   {
      if (m_currentWeapon == selectTab->id)
         break;

      selectTab++;
      count++;
   }

   if (selectTab->id && count > 13)
      return true;

   return false;
}

bool Bot::UsesPistol (void)
{
   WeaponSelect *selectTab = &g_weaponSelect[0];
   int count = 0;

   // loop through all the weapons until terminator is found
   while (selectTab->id)
   {
      if (m_currentWeapon == selectTab->id)
         break;

      selectTab++;
      count++;
   }

   if (selectTab->id && count < 7)
      return true;

   return false;
}

bool Bot::UsesCampGun (void)
{
   return UsesSubmachineGun () || UsesRifle () || UsesSniper ();
}

bool Bot::UsesSubmachineGun (void)
{
   return m_currentWeapon == WEAPON_MP5 || m_currentWeapon == WEAPON_TMP || m_currentWeapon == WEAPON_P90 || m_currentWeapon == WEAPON_MAC10 || m_currentWeapon == WEAPON_UMP45;
}

bool Bot::UsesZoomableRifle (void)
{
   return m_currentWeapon == WEAPON_AUG || m_currentWeapon == WEAPON_SG552;
}

bool Bot::UsesBadPrimary (void)
{
   return m_currentWeapon == WEAPON_XM1014 || m_currentWeapon == WEAPON_M3 || m_currentWeapon == WEAPON_UMP45 || m_currentWeapon == WEAPON_MAC10 || m_currentWeapon == WEAPON_TMP || m_currentWeapon == WEAPON_P90;
}

int Bot::CheckGrenades (void)
{
   if (pev->weapons & (1 << WEAPON_EXPLOSIVE))
      return WEAPON_EXPLOSIVE;
   else if (pev->weapons & (1 << WEAPON_FLASHBANG))
      return WEAPON_FLASHBANG;
   else if (pev->weapons & (1 << WEAPON_SMOKE))
      return WEAPON_SMOKE;

   return -1;
}

void Bot::SelectBestWeapon (void)
{
   // this function chooses best weapon, from weapons that bot currently own, and change
   // current weapon to best one.

   if (yb_jasonmode.GetBool ())
   {
      // if knife mode activated, force bot to use knife
      SelectWeaponByName ("weapon_knife");
      return;
   }

   if (m_isReloading)
      return;

   WeaponSelect *selectTab = &g_weaponSelect[0];

   int selectIndex = 0;
   int chosenWeaponIndex = 0;

   // loop through all the weapons until terminator is found...
   while (selectTab[selectIndex].id)
   {
      // is the bot NOT carrying this weapon?
      if (!(pev->weapons & (1 << selectTab[selectIndex].id)))
      {
         selectIndex++;  // skip to next weapon
         continue;
      }

      int id = selectTab[selectIndex].id;
      bool ammoLeft = false;

      // is the bot already holding this weapon and there is still ammo in clip?
      if (selectTab[selectIndex].id == m_currentWeapon && (GetAmmoInClip () < 0 || GetAmmoInClip () >= selectTab[selectIndex].minPrimaryAmmo))
         ammoLeft = true;

      // is no ammo required for this weapon OR enough ammo available to fire
      if (g_weaponDefs[id].ammo1 < 0 || m_ammo[g_weaponDefs[id].ammo1] >= selectTab[selectIndex].minPrimaryAmmo)
         ammoLeft = true;

      if (ammoLeft)
         chosenWeaponIndex = selectIndex;

      selectIndex++;
   }

   chosenWeaponIndex %= NUM_WEAPONS + 1;
   selectIndex = chosenWeaponIndex;

   int id = selectTab[selectIndex].id;

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id)
      SelectWeaponByName (selectTab[selectIndex].weaponName);

   m_isReloading = false;
   m_reloadState = RELOAD_NONE;
}


void Bot::SelectPistol (void)
{
   int oldWeapons = pev->weapons;

   pev->weapons &= ~WEAPON_PRIMARY;
   SelectBestWeapon ();

   pev->weapons = oldWeapons;
}

int Bot::GetHighestWeapon (void)
{
   WeaponSelect *selectTab = &g_weaponSelect[0];

   int weapons = pev->weapons;
   int num = 0;
   int i = 0;

   // loop through all the weapons until terminator is found...
   while (selectTab->id)
   {
      // is the bot carrying this weapon?
      if (weapons & (1 << selectTab->id))
         num = i;

      i++;
      selectTab++;
   }
   return num;
}

void Bot::SelectWeaponByName (const char *name)
{
   FakeClientCommand (GetEntity (), name);
}

void Bot::SelectWeaponbyNumber (int num)
{
   FakeClientCommand (GetEntity (), g_weaponSelect[num].weaponName);
}

void Bot::AttachToUser (void)
{
   // this function forces bot to follow user
   Array <edict_t *> foundUsers;

   // search friends near us
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
         continue;

      if (EntityIsVisible (g_clients[i].origin) && !IsValidBot (g_clients[i].ent))
         foundUsers.Push (g_clients[i].ent);
   }

   if (foundUsers.IsEmpty ())
      return;

   m_targetEntity = foundUsers.GetRandomElement ();

   ChatterMessage (Chatter_LeadOnSir);
   PushTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, -1, 0.0, true);
}

void Bot::CommandTeam (void)
{
   // prevent spamming
   if (m_timeTeamOrder > GetWorldTime () + 2 || yb_csdm_mode.GetInt () == 2 || yb_communication_type.GetInt () == 0)
      return;

   bool memberNear = false;
   bool memberExists = false;

   // search teammates seen by this bot
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
         continue;

      memberExists = true;

      if (EntityIsVisible (g_clients[i].origin))
      {
         memberNear = true;
         break;
      }
   }

   if (memberNear) // has teammates ?
   {
      if (m_personality == PERSONALITY_RUSHER && yb_communication_type.GetInt () == 2)
         RadioMessage (Radio_StormTheFront);
      else if (m_personality != PERSONALITY_RUSHER && yb_communication_type.GetInt () == 2)
         RadioMessage (Radio_Fallback);
   }
   else if (memberExists && yb_communication_type.GetInt () == 1)
      RadioMessage (Radio_TakingFire);
   else if (memberExists && yb_communication_type.GetInt () == 2)
      ChatterMessage(Chatter_ScaredEmotion);

   m_timeTeamOrder = GetWorldTime () + Random.Float (5.0, 30.0);
}

bool Bot::IsGroupOfEnemies (const Vector &location, int numEnemies, int radius)
{
   int numPlayers = 0;

   // search the world for enemy players...
   for (int i = 0; i < GetMaxClients (); i++)
   {
      if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].ent == GetEntity ())
         continue;

      if ((g_clients[i].ent->v.origin - location).GetLengthSquared () < GET_SQUARE (radius))
      {
         // don't target our teammates...
         if (g_clients[i].team == m_team)
            return false;

         if (numPlayers++ > numEnemies)
            return true;
      }
   }
   return false;
}

void Bot::CheckReload (void)
{
   // check the reload state
   if (GetTaskId () == TASK_PLANTBOMB || GetTaskId () == TASK_DEFUSEBOMB || GetTaskId () == TASK_PICKUPITEM || GetTaskId () == TASK_THROWFLASHBANG || GetTaskId () == TASK_THROWSMOKE || m_isUsingGrenade)
   {
      m_reloadState = RELOAD_NONE;
      return;
   }

   m_isReloading = false; // update reloading status
   m_reloadCheckTime = GetWorldTime () + 3.0;

   if (m_reloadState != RELOAD_NONE)
   {
      int weaponIndex = 0, maxClip = 0;
      int weapons = pev->weapons;

      if (m_reloadState == RELOAD_PRIMARY)
         weapons &= WEAPON_PRIMARY;
      else if (m_reloadState == RELOAD_SECONDARY)
         weapons &= WEAPON_SECONDARY;

      if (weapons == 0)
      {
         m_reloadState++;

         if (m_reloadState > RELOAD_SECONDARY)
            m_reloadState = RELOAD_NONE;

         return;
      }

      for (int i = 1; i < MAX_WEAPONS; i++)
      {
         if (weapons & (1 << i))
         {
            weaponIndex = i;
            break;
         }
      }
      InternalAssert (weaponIndex);

      switch (weaponIndex)
      {
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

      if (m_ammoInClip[weaponIndex] < (maxClip * 0.8) && m_ammo[g_weaponDefs[weaponIndex].ammo1] > 0)
      {
         if (m_currentWeapon != weaponIndex)
            SelectWeaponByName (g_weaponDefs[weaponIndex].className);
          pev->button &= ~IN_ATTACK;

         if ((pev->oldbuttons & IN_RELOAD) == RELOAD_NONE)
            pev->button |= IN_RELOAD; // press reload button

         m_isReloading = true;
      }
      else
      {
         // if we have enemy don't reload next weapon
         if ((m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) || m_seeEnemyTime + 5.0 > GetWorldTime ())
         {
            m_reloadState = RELOAD_NONE;
            return;
         }
         m_reloadState++;

         if (m_reloadState > RELOAD_SECONDARY)
            m_reloadState = RELOAD_NONE;

         return;
      }
   }
}
