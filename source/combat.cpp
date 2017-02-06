//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_shoots_thru_walls ("yb_shoots_thru_walls", "2");
ConVar yb_ignore_enemies ("yb_ignore_enemies", "0");
ConVar yb_check_enemy_rendering ("yb_check_enemy_rendering", "0");

ConVar mp_friendlyfire ("mp_friendlyfire", nullptr, VT_NOREGISTER);

int Bot::GetNearbyFriendsNearPosition(const Vector &origin, float radius)
{
   int count = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == GetEntity ())
         continue;

      if ((client.origin - origin).GetLengthSquared () < GET_SQUARE (radius))
         count++;
   }
   return count;
}

int Bot::GetNearbyEnemiesNearPosition(const Vector &origin, float radius)
{
   int count = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team)
         continue;

      if ((client.origin - origin).GetLengthSquared () < GET_SQUARE (radius))
         count++;
   }
   return count;
}

bool Bot::IsEnemyHiddenByRendering (edict_t *enemy)
{
   if (!yb_check_enemy_rendering.GetBool () || engine.IsNullEntity (enemy))
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
         if (v.renderamt <= 20.0f && v.rendercolor.x <= 20.0f && v.rendercolor.y <= 20.0f && v.rendercolor.z <= 20.0f)
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

bool Bot::CheckVisibility (edict_t *target, Vector *origin, uint8 *bodyPart)
{
   // this function checks visibility of a bot target.

   if (IsEnemyHiddenByRendering (target))
   {
      *bodyPart = 0;
      origin->Zero ();

      return false;
   }
   TraceResult result;

   Vector eyes = EyePosition ();
   Vector spot = target->v.origin;
   
   *bodyPart = 0;

   engine.TestLine (eyes, spot, TRACE_IGNORE_EVERYTHING, pev->pContainingEntity, &result);

   if (result.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_BODY;
      *origin = result.vecEndPos;

      if (m_difficulty > 3)
         origin->z += 3.0f;
   }

   // check top of head
   spot = spot + Vector (0, 0, 25.0f);
   engine.TestLine (eyes, spot, TRACE_IGNORE_EVERYTHING, pev->pContainingEntity, &result);

   if (result.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_HEAD;
      *origin = result.vecEndPos;

      if (m_difficulty > 3)
         origin->z += 1.0f;
   }

   if (*bodyPart != 0)
      return true;

   const float standFeet = 34.0f;
   const float crouchFeet = 14.0f;

   if (target->v.flags & FL_DUCKING)
      spot.z = target->v.origin.z - crouchFeet;
   else
      spot.z = target->v.origin.z - standFeet;

   engine.TestLine (eyes, spot, TRACE_IGNORE_EVERYTHING, pev->pContainingEntity, &result);

   if (result.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }

   const float edgeOffset = 13.0f;
   Vector dir = (target->v.origin - pev->origin).Normalize2D ();

   Vector perp (-dir.y, dir.x, 0.0f);
   spot = target->v.origin + Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   engine.TestLine (eyes, spot, TRACE_IGNORE_EVERYTHING, pev->pContainingEntity, &result);

   if (result.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }
   spot = target->v.origin - Vector (perp.x * edgeOffset, perp.y * edgeOffset, 0);

   engine.TestLine (eyes, spot, TRACE_IGNORE_EVERYTHING, pev->pContainingEntity, &result);

   if (result.flFraction >= 1.0f)
   {
      *bodyPart |= VISIBLE_OTHER;
      *origin = result.vecEndPos;

      return true;
   }
   return false;
}

bool Bot::IsEnemyViewable (edict_t *player)
{
   if (engine.IsNullEntity (player))
      return false;

   bool forceTrueIfVisible = false;

   if (IsValidPlayer (pev->dmg_inflictor) && engine.GetTeam (pev->dmg_inflictor) != m_team)
      forceTrueIfVisible = true;

   if ((IsInViewCone (player->v.origin + pev->view_ofs) || forceTrueIfVisible) && CheckVisibility (player, &m_enemyOrigin, &m_visibility))
   {
      m_seeEnemyTime = engine.Time ();
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
   if (m_enemyIgnoreTimer > engine.Time () || m_blindTime > engine.Time () || yb_ignore_enemies.GetBool ())
      return false;

   edict_t *player, *newEnemy = nullptr;

   float nearestDistance = m_viewDistance;

   // clear suspected flag
   if (m_seeEnemyTime + 3.0f < engine.Time ())
      m_states &= ~STATE_SUSPECT_ENEMY;

   if (!engine.IsNullEntity (m_enemy) && m_enemyUpdateTime > engine.Time ())
   {
      player = m_enemy;

      // is player is alive
      if (IsAlive (player) && IsEnemyViewable (player))
         newEnemy = player;
   }

   // the old enemy is no longer visible or
   if (engine.IsNullEntity (newEnemy))
   {
      m_enemyUpdateTime = engine.Time () + 0.5f;

      // ignore shielded enemies, while we have real one
      edict_t *shieldEnemy = nullptr;

      // search the world for players...
      for (int i = 0; i < engine.MaxClients (); i++)
      {
         const Client &client = g_clients[i];

         if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team || client.ent == GetEntity ())
            continue;

         player = client.ent;

         // do some blind by smoke grenade
         if (m_blindRecognizeTime < engine.Time () && IsBehindSmokeClouds (player))
         {
            m_blindRecognizeTime = engine.Time () + Random.Float (1.0f, 2.0f);

            if (Random.Int (0, 100) < 50)
               ChatterMessage (Chatter_BehindSmoke);
         }

         if (player->v.button & (IN_ATTACK | IN_ATTACK2))
            m_blindRecognizeTime = engine.Time () - 0.1f;

         // see if bot can see the player...
         if (m_blindRecognizeTime < engine.Time () && IsEnemyViewable (player))
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

      if (engine.IsNullEntity (newEnemy) && !engine.IsNullEntity (shieldEnemy))
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
         m_seeEnemyTime = engine.Time ();

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
      else
      {
         if (m_seeEnemyTime + 3.0 < engine.Time () && (m_hasC4 || HasHostage () || !engine.IsNullEntity (m_targetEntity)))
            RadioMessage (Radio_EnemySpotted);

         m_targetEntity = nullptr; // stop following when we see an enemy...

         if (Random.Int (0, 100) < m_difficulty * 25)
            m_enemySurpriseTime = engine.Time () + m_actualReactionTime * 0.5f;
         else
            m_enemySurpriseTime = engine.Time () + m_actualReactionTime;

         // zero out reaction time
         m_actualReactionTime = 0.0f;
         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;
         m_enemyReachableTimer = 0.0f;

         // keep track of when we last saw an enemy
         m_seeEnemyTime = engine.Time ();

         if (!(pev->oldbuttons & IN_ATTACK))
            return true;

         // now alarm all teammates who see this bot & don't have an actual enemy of the bots enemy should simulate human players seeing a teammate firing
         for (int j = 0; j < engine.MaxClients (); j++)
         {
            const Client &client = g_clients[j];

            if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == GetEntity ())
               continue;

            Bot *other = bots.GetBot (client.ent);

            if (other != nullptr && other->m_seeEnemyTime + 2.0f < engine.Time () && engine.IsNullEntity (other->m_lastEnemy) && IsVisible (pev->origin, other->GetEntity ()) && other->IsInViewCone (pev->origin))
            {
               other->m_lastEnemy = newEnemy;
               other->m_lastEnemyOrigin = m_lastEnemyOrigin;
               other->m_seeEnemyTime = engine.Time ();
               other->m_states |= (STATE_SUSPECT_ENEMY | STATE_HEARING_ENEMY);
               other->m_aimFlags |= AIM_LAST_ENEMY;
            }
         }
         return true;
      }
   }
   else if (!engine.IsNullEntity (m_enemy))
   {
      newEnemy = m_enemy;
      m_lastEnemy = newEnemy;

      if (!IsAlive (newEnemy))
      {
         m_enemy = nullptr;
  
         // shoot at dying players if no new enemy to give some more human-like illusion
         if (m_seeEnemyTime + 0.1f > engine.Time ())
         {
            if (!UsesSniper ())
            {
               m_shootAtDeadTime = engine.Time () + 0.4f;
               m_actualReactionTime = 0.0f;
               m_states |= STATE_SUSPECT_ENEMY;

               return true;
            }
            return false;
         }
         else if (m_shootAtDeadTime > engine.Time ())
         {
            m_actualReactionTime = 0.0f;
            m_states |= STATE_SUSPECT_ENEMY;

            return true;
         }
         return false;
      }

      // if no enemy visible check if last one shoot able through wall
      if (yb_shoots_thru_walls.GetBool () && m_difficulty >= 2 && IsShootableThruObstacle (newEnemy->v.origin))
      {
         m_seeEnemyTime = engine.Time ();

         m_states |= STATE_SUSPECT_ENEMY;
         m_aimFlags |= AIM_LAST_ENEMY;

         m_enemy = newEnemy;
         m_lastEnemy = newEnemy;
         m_lastEnemyOrigin = newEnemy->v.origin;

         return true;
      }
   }

   // check if bots should reload...
   if ((m_aimFlags <= AIM_PREDICT_PATH && m_seeEnemyTime + 3.0f < engine.Time () &&  !(m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) && engine.IsNullEntity (m_lastEnemy) && engine.IsNullEntity (m_enemy) && GetTaskId () != TASK_SHOOTBREAKABLE && GetTaskId () != TASK_PLANTBOMB && GetTaskId () != TASK_DEFUSEBOMB) || g_roundEnded)
   {
      if (!m_reloadState)
         m_reloadState = RELOAD_PRIMARY;
   }

   // is the bot using a sniper rifle or a zoomable rifle?
   if ((UsesSniper () || UsesZoomableRifle ()) && m_zoomCheckTime + 1.0f < engine.Time ())
   {
      if (pev->fov < 90.0f) // let the bot zoom out
         pev->button |= IN_ATTACK2;
      else
         m_zoomCheckTime = 0.0f;
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
   Vector randomize;

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
         if ((Random.Int (1, 100) < headshotFreq[m_difficulty]) || UsesPistol ())
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
   const Vector &velocity = UsesSniper () ? Vector::GetZero () : 1.0f * GetThinkInterval () * (m_enemy->v.velocity - pev->velocity);

   if (m_difficulty < 3 && !randomize.IsZero ())
   {
      float divOffs = (m_enemyOrigin - pev->origin).GetLength ();

      if (pev->fov < 40)
         divOffs = divOffs / 2000;
      else if (pev->fov < 90)
         divOffs = divOffs / 1000;
      else
         divOffs = divOffs / 500;

      // randomize the target position
      m_enemyOrigin = divOffs * randomize;
   }
   else
      m_enemyOrigin = targetOrigin;

   if (distance >= 256.0f && m_difficulty < 4)
      m_enemyOrigin += velocity;

   return m_enemyOrigin;
}

float Bot::GetZOffset (float distance)
{
   if (m_difficulty < 3)
      return 0.0f;

   bool sniper = UsesSniper ();
   bool pistol = UsesPistol ();
   bool rifle = UsesRifle ();

   bool zoomableRifle = UsesZoomableRifle ();
   bool submachine = UsesSubmachineGun ();
   bool shotgun = (m_currentWeapon == WEAPON_XM1014 || m_currentWeapon == WEAPON_M3);
   bool m249 = m_currentWeapon == WEAPON_M249;

   float result = 3.5f;

   if (distance < 2800.0f && distance > MAX_SPRAY_DISTANCE_X2)
   {
      if (sniper) result = 1.5f;
      else if (zoomableRifle) result = 4.5f;
      else if (pistol) result = 6.5f;
      else if (submachine) result = 5.5f;
      else if (rifle) result = 5.5f;
      else if (m249) result = 2.5f;
      else if (shotgun) result = 10.5f;
   }
   else if (distance > MAX_SPRAY_DISTANCE && distance <= MAX_SPRAY_DISTANCE_X2)
   {
      if (sniper) result = 2.5f;
      else if (zoomableRifle) result = 3.5f;
      else if (pistol) result = 6.5f;
      else if (submachine) result = 3.5f;
      else if (rifle) result = 1.6f;
      else if (m249) result = -1.0f;
      else if (shotgun) result = 10.0f;
   }
   else if (distance < MAX_SPRAY_DISTANCE)
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
   if (!mp_friendlyfire.GetBool () || (g_gameFlags & GAME_CSDM))
      return false;

   MakeVectors (pev->v_angle);

   TraceResult tr;
   engine.TestLine (EyePosition (), EyePosition () + 10000.0f * pev->v_angle, TRACE_IGNORE_NONE, GetEntity (), &tr);

   // check if we hit something
   if (!engine.IsNullEntity (tr.pHit))
   {
      int playerIndex = engine.IndexOfEntity (tr.pHit) - 1;

      // check valid range
      if (playerIndex >= 0 && playerIndex < engine.MaxClients () && g_clients[playerIndex].team == m_team && (g_clients[playerIndex].flags & CF_ALIVE))
         return true;
   }

   // search the world for players
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == GetEntity ())
         continue;

      edict_t *ent = client.ent;

      float friendDistance = (ent->v.origin - pev->origin).GetLength ();
      float squareDistance = A_sqrtf (1089.0f + (friendDistance * friendDistance));

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
   engine.TestLine (EyePosition (), dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   if (tr.fStartSolid)
   {
      const Vector &source = tr.vecEndPos;
      engine.TestLine (dest, source, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

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
   Vector point;

   int thikness = 0;
   int numHits = 0;

   TraceResult tr;
   engine.TestLine (source, dest, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

   while (tr.flFraction != 1.0f && numHits < 3)
   {
      numHits++;
      thikness++;

      point = tr.vecEndPos + direction;

      while (POINT_CONTENTS (point) == CONTENTS_SOLID && thikness < 98)
      {
         point = point + direction;
         thikness++;
      }
      engine.TestLine (point, dest, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);
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

   if (m_firePause > engine.Time ())
      return true;

   if ((m_aimFlags & AIM_ENEMY) && !m_enemyOrigin.IsZero ())
   {
      if (GetShootingConeDeviation (GetEntity (), &m_enemyOrigin) > 0.92f && IsEnemyProtectedByShield (m_enemy))
         return true;
   }
   float offset = 0.0f;

   if (distance < MAX_SPRAY_DISTANCE)
      return false;
   else if (distance < MAX_SPRAY_DISTANCE_X2)
      offset = 10.0f;
   else
      offset = 5.0f;

   const float xPunch = DegreeToRadian (pev->punchangle.x);
   const float yPunch = DegreeToRadian (pev->punchangle.y);

   float interval = GetThinkInterval ();

   // check if we need to compensate recoil
   if (tanf (A_sqrtf (fabsf (xPunch * xPunch) + fabsf (yPunch * yPunch))) * distance > offset + 30.0f + ((100 - (m_difficulty * 25)) / 100.f))
   {
      if (m_firePause < engine.Time ())
         m_firePause = Random.Float (0.5f, 0.5f + 0.3f * ((100.0f - (m_difficulty * 25)) / 100.f));

      m_firePause -= interval;
      m_firePause += engine.Time ();

      return true;
   }
   return false;
}

void Bot::FinishWeaponSelection (float distance, int index, int id, int choosen)
{
   WeaponSelect *tab = &g_weaponSelect[0];

   // we want to fire weapon, don't reload now
   if (!m_isReloading)
   {
      m_reloadState = RELOAD_NONE;
      m_reloadCheckTime = engine.Time () + 3.0f;
   }

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id)
   {
      SelectWeaponByName (g_weaponDefs[id].className);

      // reset burst fire variables
      m_firePause = 0.0f;
      m_timeLastFired = 0.0f;

      return;
   }

   if (tab[choosen].id != id)
   {
      choosen = 0;

      // loop through all the weapons until terminator is found...
      while (tab[choosen].id)
      {
         if (tab[choosen].id == id)
            break;

         choosen++;
      }
   }

   // if we're have a glock or famas vary burst fire mode
   CheckBurstMode (distance);

   if (HasShield () && m_shieldCheckTime < engine.Time () && GetTaskId () != TASK_CAMP) // better shield gun usage
   {
      if (distance >= 750.0f && !IsShieldDrawn ())
         pev->button |= IN_ATTACK2; // draw the shield
      else if (IsShieldDrawn () || (!engine.IsNullEntity (m_enemy) && ((m_enemy->v.button & IN_RELOAD) || !IsEnemyViewable (m_enemy))))
         pev->button |= IN_ATTACK2; // draw out the shield

      m_shieldCheckTime = engine.Time () + 1.0f;
   }

   if (UsesSniper () && m_zoomCheckTime + 1.0f < engine.Time ()) // is the bot holding a sniper rifle?
   {
      if (distance > 1500.0f && pev->fov >= 40.0f) // should the bot switch to the long-range zoom?
         pev->button |= IN_ATTACK2;

      else if (distance > 150.0f && pev->fov >= 90.0f) // else should the bot switch to the close-range zoom ?
         pev->button |= IN_ATTACK2;

      else if (distance <= 150.0f && pev->fov < 90.0f) // else should the bot restore the normal view ?
         pev->button |= IN_ATTACK2;

      m_zoomCheckTime = engine.Time ();

      if (!engine.IsNullEntity (m_enemy) && (m_states & STATE_SEEING_ENEMY))
      {
         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
         m_navTimeset = engine.Time ();
      }
   }
   else if (m_difficulty < 4 && UsesZoomableRifle ()) // else is the bot holding a zoomable rifle?
   {
      if (distance > 800.0f && pev->fov >= 90.0f) // should the bot switch to zoomed mode?
         pev->button |= IN_ATTACK2;

      else if (distance <= 800.0f && pev->fov < 90.0f) // else should the bot restore the normal view?
         pev->button |= IN_ATTACK2;

      m_zoomCheckTime = engine.Time ();
   }

   // need to care for burst fire?
   if (distance < MAX_SPRAY_DISTANCE || m_blindTime > engine.Time ())
   {
      if (id == WEAPON_KNIFE)
      {
         if (distance < 64.0f)
         {
            if (Random.Int (1, 100) < 30 || HasShield ())
               pev->button |= IN_ATTACK; // use primary attack
            else
               pev->button |= IN_ATTACK2; // use secondary attack
         }
      }
      else
      {
         if (tab[choosen].primaryFireHold && m_ammo[g_weaponDefs[tab[index].id].ammo1] > tab[index].minPrimaryAmmo) // if automatic weapon, just press attack
            pev->button |= IN_ATTACK;
         else // if not, toggle buttons
         {
            if ((pev->oldbuttons & IN_ATTACK) == 0)
               pev->button |= IN_ATTACK;
         }
      }
      m_shootTime = engine.Time ();
   }
   else
   {
      if (DoFirePause (distance))
         return;

      // don't attack with knife over long distance
      if (id == WEAPON_KNIFE)
      {
         m_shootTime = engine.Time ();
         return;
      }

      if (tab[choosen].primaryFireHold)
      {
         m_shootTime = engine.Time ();
         m_zoomCheckTime = engine.Time ();

         pev->button |= IN_ATTACK;  // use primary attack      
      }
      else
      {
         pev->button |= IN_ATTACK;

         m_shootTime = engine.Time () + Random.Float (0.15f, 0.35f);
         m_zoomCheckTime = engine.Time () - 0.09f;
      }
   }
}

void Bot::FireWeapon (void)
{
   // this function will return true if weapon was fired, false otherwise
   float distance = (m_lookAt - EyePosition ()).GetLength (); // how far away is the enemy?

   // if using grenade stop this
   if (m_isUsingGrenade)
   {
      m_shootTime = engine.Time () + 0.1f;
      return;
   }

   // or if friend in line of fire, stop this too but do not update shoot time
   if (!engine.IsNullEntity (m_enemy))
   {
      if (IsFriendInLineOfFire (distance))
      {
         m_fightStyle = FIGHT_STRAFE;
         m_lastFightStyleCheck = engine.Time ();

         return;
      }
   }
   WeaponSelect *tab = &g_weaponSelect[0];

   edict_t *enemy = m_enemy;

   int selectId = WEAPON_KNIFE, selectIndex = 0, choosenWeapon = 0;
   int weapons = pev->weapons;

   // if jason mode use knife only
   if (yb_jasonmode.GetBool ())
   {
      FinishWeaponSelection (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // use knife if near and good difficulty (l33t dude!)
   if (m_difficulty >= 3 && pev->health > 80.0f && !engine.IsNullEntity (enemy) && pev->health >= enemy->v.health && distance < 100.0f && !IsOnLadder () && !IsGroupOfEnemies (pev->origin))
   {
      FinishWeaponSelection (distance, selectIndex, selectId, choosenWeapon);
      return;
   }

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id)
   {
      // is the bot carrying this weapon?
      if (weapons & (1 << tab[selectIndex].id))
      {
         // is enough ammo available to fire AND check is better to use pistol in our current situation...
         if (m_ammoInClip[tab[selectIndex].id] > 0 && !IsWeaponBadInDistance (selectIndex, distance))
            choosenWeapon = selectIndex;
      }
      selectIndex++;
   }
   selectId = tab[choosenWeapon].id;

   // if no available weapon...
   if (choosenWeapon == 0)
   {
      selectIndex = 0;

      // loop through all the weapons until terminator is found...
      while (tab[selectIndex].id)
      {
         int id = tab[selectIndex].id;

         // is the bot carrying this weapon?
         if (weapons & (1 << id))
         {
            if ( g_weaponDefs[id].ammo1 != -1 && g_weaponDefs[id].ammo1 < 32 && m_ammo[g_weaponDefs[id].ammo1] >= tab[selectIndex].minPrimaryAmmo)
            {
               // available ammo found, reload weapon
               if (m_reloadState == RELOAD_NONE || m_reloadCheckTime > engine.Time ())
               {
                  m_isReloading = true;
                  m_reloadState = RELOAD_PRIMARY;
                  m_reloadCheckTime = engine.Time ();

                  RadioMessage (Radio_NeedBackup);
               }
               return;
            }
         }
         selectIndex++;
      }
      selectId = WEAPON_KNIFE; // no available ammo, use knife!
   }
   FinishWeaponSelection (distance, selectIndex, selectId, choosenWeapon);
}

bool Bot::IsWeaponBadInDistance (int weaponIndex, float distance)
{
   // this function checks, is it better to use pistol instead of current primary weapon
   // to attack our enemy, since current weapon is not very good in this situation.

   if (m_difficulty < 2)
      return false;

   int wid = g_weaponSelect[weaponIndex].id;

   if (wid == WEAPON_KNIFE)
      return false;

   // check is ammo available for secondary weapon
   if (m_ammoInClip[g_weaponSelect[GetBestSecondaryWeaponCarried ()].id] >= 1)
      return false;

   // better use pistol in short range distances, when using sniper weapons
   if ((wid == WEAPON_SCOUT || wid == WEAPON_AWP || wid == WEAPON_G3SG1 || wid == WEAPON_SG550) && distance < 500.0f)
      return true;

   // shotguns is too inaccurate at long distances, so weapon is bad
   if ((wid == WEAPON_M3 || wid == WEAPON_XM1014) && distance > 750.0f)
      return true;

   return false;
}

void Bot::FocusEnemy (void)
{
   // aim for the head and/or body
   m_lookAt = GetAimPosition ();

   if (m_enemySurpriseTime > engine.Time ())
      return;

   float distance = (m_lookAt - EyePosition ()).GetLength2D ();  // how far away is the enemy scum?

   if (distance < 128.0f)
   {
      if (m_currentWeapon == WEAPON_KNIFE)
      {
         if (distance < 80.0f)
            m_wantsToFire = true;
      }
      else
         m_wantsToFire = true;
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
   if (engine.IsNullEntity (m_enemy))
      return;

   float distance = (m_lookAt - EyePosition ()).GetLength2D ();  // how far away is the enemy scum?

   if (m_timeWaypointMove < engine.Time ())
   {
      int approach;

      if (m_currentWeapon == WEAPON_KNIFE) // knife?
         approach = 100;
      else if ((m_states & STATE_SUSPECT_ENEMY) && !(m_states & STATE_SEEING_ENEMY)) // if suspecting enemy stand still
         approach = 49;
      else if (m_isReloading || m_isVIP) // if reloading or vip back off
         approach = 29;
      else
      {
         approach = static_cast <int> (pev->health * m_agressionLevel);

         if (UsesSniper () && approach > 49)
            approach = 49;
      }

      // only take cover when bomb is not planted and enemy can see the bot or the bot is VIP
      if (approach < 30 && !g_bombPlanted && (IsInViewCone (m_enemy->v.origin) || m_isVIP))
      {
         m_moveSpeed = -pev->maxspeed;

         TaskItem *task = GetTask ();

         task->id = TASK_SEEKCOVER;
         task->resume = true;
         task->desire = TASKPRI_ATTACK + 1.0f;
      }
      else if (approach < 50)
         m_moveSpeed = 0.0f;
      else
         m_moveSpeed = pev->maxspeed;

      if (distance < 96.0f && m_currentWeapon != WEAPON_KNIFE)
         m_moveSpeed = -pev->maxspeed;

      if (UsesSniper ())
      {
         m_fightStyle = FIGHT_STAY;
         m_lastFightStyleCheck = engine.Time ();
      }
      else if (UsesRifle () || UsesSubmachineGun ())
      {
         if (m_lastFightStyleCheck + 3.0f < engine.Time ())
         {
            int rand = Random.Int (1, 100);

            if (distance < 450.0f)
               m_fightStyle = FIGHT_STRAFE;
            else if (distance < 1024.0f)
            {
               if (rand < (UsesSubmachineGun () ? 50 : 30))
                  m_fightStyle = FIGHT_STRAFE;
               else
                  m_fightStyle = FIGHT_STAY;
            }
            else
            {
               if (rand < (UsesSubmachineGun () ? 80 : 93))
                  m_fightStyle = FIGHT_STAY;
               else
                  m_fightStyle = FIGHT_STRAFE;
            }
            m_lastFightStyleCheck = engine.Time ();
         }
      }
      else
      {
         if (m_lastFightStyleCheck + 3.0f < engine.Time ())
         {
            if (Random.Int (0, 100) < 50)
               m_fightStyle = FIGHT_STRAFE;
            else
               m_fightStyle = FIGHT_STAY;

            m_lastFightStyleCheck = engine.Time ();
         }
      }

      if (m_fightStyle == FIGHT_STRAFE || ((pev->button & IN_RELOAD) || m_isReloading) || (UsesPistol () && distance < 400.0f) || m_currentWeapon == WEAPON_KNIFE)
      {
         if (m_strafeSetTime < engine.Time ())
         {
            // to start strafing, we have to first figure out if the target is on the left side or right side
            MakeVectors (m_enemy->v.v_angle);

            const Vector &dirToPoint = (pev->origin - m_enemy->v.origin).Normalize2D ();
            const Vector &rightSide = g_pGlobals->v_right.Normalize2D ();

            if ((dirToPoint | rightSide) < 0)
               m_combatStrafeDir = STRAFE_DIR_LEFT;
            else
               m_combatStrafeDir = STRAFE_DIR_RIGHT;

            if (Random.Int (1, 100) < 30)
               m_combatStrafeDir = (m_combatStrafeDir == STRAFE_DIR_LEFT ? STRAFE_DIR_RIGHT : STRAFE_DIR_LEFT);

            m_strafeSetTime = engine.Time () + Random.Float (0.5f, 3.0f);
         }

         if (m_combatStrafeDir == STRAFE_DIR_RIGHT)
         {
            if (!CheckWallOnLeft ())
               m_strafeSpeed = -pev->maxspeed;
            else
            {
               m_combatStrafeDir = STRAFE_DIR_LEFT;
               m_strafeSetTime = engine.Time () + 1.5f;
            }
         }
         else
         {
            if (!CheckWallOnRight ())
               m_strafeSpeed = pev->maxspeed;
            else
            {
               m_combatStrafeDir = STRAFE_DIR_RIGHT;
               m_strafeSetTime = engine.Time () + 1.5f;
            }
         }

         if (m_difficulty >= 3 && (m_jumpTime + 5.0f < engine.Time () && IsOnFloor () && Random.Int (0, 1000) < (m_isReloading ? 8 : 2) && pev->velocity.GetLength2D () > 150.0f) && !UsesSniper ())
            pev->button |= IN_JUMP;

         if (m_moveSpeed > 0.0f && distance > 100.0f && m_currentWeapon != WEAPON_KNIFE)
            m_moveSpeed = 0.0f;

         if (m_currentWeapon == WEAPON_KNIFE)
            m_strafeSpeed = 0.0f;
      }
      else if (m_fightStyle == FIGHT_STAY)
      {
         if ((m_visibility & (VISIBLE_HEAD | VISIBLE_BODY)) && GetTaskId () != TASK_SEEKCOVER && GetTaskId () != TASK_HUNTENEMY && waypoints.IsDuckVisible (m_currentWaypointIndex, waypoints.FindNearest (m_enemy->v.origin)))
            m_duckTime = engine.Time () + 0.5f;

         m_moveSpeed = 0.0f;
         m_strafeSpeed = 0.0f;
         m_navTimeset = engine.Time ();
      }
   }

   if (m_duckTime > engine.Time ())
   {
      m_moveSpeed = 0.0f;
      m_strafeSpeed = 0.0f;
   }

   if (m_moveSpeed > 0.0f && m_currentWeapon != WEAPON_KNIFE)
      m_moveSpeed = GetWalkSpeed ();

   if (m_isReloading)
   {
      m_moveSpeed = -pev->maxspeed;
      m_duckTime = engine.Time () - 1.0f;
   }

   if (!IsInWater () && !IsOnLadder () && (m_moveSpeed > 0.0f || m_strafeSpeed >= 0.0f))
   {
      MakeVectors (pev->v_angle);

      if (IsDeadlyDrop (pev->origin + (g_pGlobals->v_forward * m_moveSpeed * 0.2f) + (g_pGlobals->v_right * m_strafeSpeed * 0.2f) + (pev->velocity * GetThinkInterval ())))
      {
         m_strafeSpeed = -m_strafeSpeed;
         m_moveSpeed = -m_moveSpeed;

         pev->button &= ~IN_JUMP;
      }
   }
   IgnoreCollisionShortly ();
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

   if (engine.IsNullEntity (enemy) || IsShieldDrawn ())
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
   WeaponSelect *tab = &g_weaponSelect[0];
   int count = 0;

   while (tab->id)
   {
      if (m_currentWeapon == tab->id)
         break;

      tab++;
      count++;
   }

   if (tab->id && count > 13)
      return true;

   return false;
}

bool Bot::UsesPistol (void)
{
   WeaponSelect *tab = &g_weaponSelect[0];
   int count = 0;

   // loop through all the weapons until terminator is found
   while (tab->id)
   {
      if (m_currentWeapon == tab->id)
         break;

      tab++;
      count++;
   }

   if (tab->id && count < 7)
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

   WeaponSelect *tab = &g_weaponSelect[0];

   int selectIndex = 0;
   int chosenWeaponIndex = 0;

   // loop through all the weapons until terminator is found...
   while (tab[selectIndex].id)
   {
      // is the bot NOT carrying this weapon?
      if (!(pev->weapons & (1 << tab[selectIndex].id)))
      {
         selectIndex++;  // skip to next weapon
         continue;
      }

      int id = tab[selectIndex].id;
      bool ammoLeft = false;

      // is the bot already holding this weapon and there is still ammo in clip?
      if (tab[selectIndex].id == m_currentWeapon && (GetAmmoInClip () < 0 || GetAmmoInClip () >= tab[selectIndex].minPrimaryAmmo))
         ammoLeft = true;

      // is no ammo required for this weapon OR enough ammo available to fire
      if (g_weaponDefs[id].ammo1 < 0 || (g_weaponDefs[id].ammo1 < 32 && m_ammo[g_weaponDefs[id].ammo1] >= tab[selectIndex].minPrimaryAmmo))
         ammoLeft = true;

      if (ammoLeft)
         chosenWeaponIndex = selectIndex;

      selectIndex++;
   }

   chosenWeaponIndex %= NUM_WEAPONS + 1;
   selectIndex = chosenWeaponIndex;

   int id = tab[selectIndex].id;

   // select this weapon if it isn't already selected
   if (m_currentWeapon != id)
      SelectWeaponByName (tab[selectIndex].weaponName);

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
   WeaponSelect *tab = &g_weaponSelect[0];

   int weapons = pev->weapons;
   int num = 0;
   int i = 0;

   // loop through all the weapons until terminator is found...
   while (tab->id)
   {
      // is the bot carrying this weapon?
      if (weapons & (1 << tab->id))
         num = i;

      i++;
      tab++;
   }
   return num;
}

void Bot::SelectWeaponByName (const char *name)
{
   engine.IssueBotCommand (GetEntity (), name);
}

void Bot::SelectWeaponbyNumber (int num)
{
   engine.IssueBotCommand (GetEntity (), g_weaponSelect[num].weaponName);
}

void Bot::AttachToUser (void)
{
   // this function forces bot to follow user
   Array <edict_t *> users;

   // search friends near us
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == GetEntity ())
         continue;

      if (EntityIsVisible (client.origin) && !IsValidBot (client.ent))
         users.Push (client.ent);
   }

   if (users.IsEmpty ())
      return;

   m_targetEntity = users.GetRandomElement ();

   ChatterMessage (Chatter_LeadOnSir);
   PushTask (TASK_FOLLOWUSER, TASKPRI_FOLLOWUSER, -1, 0.0f, true);
}

void Bot::CommandTeam (void)
{
   // prevent spamming
   if (m_timeTeamOrder > engine.Time () + 2.0f || (g_gameFlags & GAME_CSDM_FFA) || !yb_communication_type.GetInt ())
      return;

   bool memberNear = false;
   bool memberExists = false;

   // search teammates seen by this bot
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == GetEntity ())
         continue;

      memberExists = true;

      if (EntityIsVisible (client.origin))
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

   m_timeTeamOrder = engine.Time () + Random.Float (5.0f, 30.0f);
}

bool Bot::IsGroupOfEnemies (const Vector &location, int numEnemies, int radius)
{
   int numPlayers = 0;

   // search the world for enemy players...
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      const Client &client = g_clients[i];

      if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.ent == GetEntity ())
         continue;

      if ((client.ent->v.origin - location).GetLengthSquared () < GET_SQUARE (radius))
      {
         // don't target our teammates...
         if (client.team == m_team)
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
   m_reloadCheckTime = engine.Time () + 3.0f;

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

      if (m_ammoInClip[weaponIndex] < maxClip * 0.8f && g_weaponDefs[weaponIndex].ammo1 != -1 && g_weaponDefs[weaponIndex].ammo1 < 32 && m_ammo[g_weaponDefs[weaponIndex].ammo1] > 0)
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
         if ((m_states & (STATE_SEEING_ENEMY | STATE_HEARING_ENEMY)) || m_seeEnemyTime + 5.0f > engine.Time ())
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
