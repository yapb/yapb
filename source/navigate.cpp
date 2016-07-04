//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_whose_your_daddy ("yb_whose_your_daddy", "0");

int Bot::FindGoal (void)
{
   // chooses a destination (goal) waypoint for a bot
   if (!g_bombPlanted && m_team == TERRORIST && (g_mapType & MAP_DE))
   {
      edict_t *pent = NULL;

      while (!engine.IsNullEntity (pent = FIND_ENTITY_BY_STRING (pent, "classname", "weaponbox")))
      {
         if (strcmp (STRING (pent->v.model), "models/w_backpack.mdl") == 0)
         {
            int index = waypoints.FindNearest (engine.GetAbsOrigin (pent));

            if (index >= 0 && index < g_numWaypoints)
               return m_loosedBombWptIndex = index;

            break;
         }
      }

      // forcing terrorist bot to not move to another bomb spot
      if (m_inBombZone && !m_hasProgressBar && m_hasC4)
         return waypoints.FindNearest (pev->origin, 400.0f, FLAG_GOAL);
   }
   int tactic = 0;

   // path finding behavior depending on map type
   float offensive = 0.0f;
   float defensive = 0.0f;

   float goalDesire = 0.0f;
   float forwardDesire = 0.0f;
   float campDesire = 0.0f;
   float backoffDesire = 0.0f;
   float tacticChoice = 0.0f;

   Array <int> *offensiveWpts = NULL;
   Array <int> *defensiveWpts = NULL;

   switch (m_team)
   {
   case TERRORIST:
      offensiveWpts = &waypoints.m_ctPoints;
      defensiveWpts = &waypoints.m_terrorPoints;
      break;

   case CT:
   default:
      offensiveWpts = &waypoints.m_terrorPoints;
      defensiveWpts = &waypoints.m_ctPoints;
      break;
   }

   // terrorist carrying the C4?
   if (m_hasC4 || m_isVIP)
   {
      tactic = 3;
      return FinishFindGoal (tactic, defensiveWpts, offensiveWpts);
   }
   else if (m_team == CT && HasHostage ())
   {
      tactic = 2;
      offensiveWpts = &waypoints.m_rescuePoints;

      return FinishFindGoal (tactic, defensiveWpts, offensiveWpts);
   }

   offensive = m_agressionLevel * 100.0f;
   defensive = m_fearLevel * 100.0f;

   if (g_mapType & (MAP_AS | MAP_CS))
   {
      if (m_team == TERRORIST)
      {
         defensive += 25.0f;
         offensive -= 25.0f;
      }
      else if (m_team == CT)
      {
         // on hostage maps force more bots to save hostages
         if (g_mapType & MAP_CS)
         {
            defensive -= 25.0f - m_difficulty * 0.5f;
            offensive += 25.0f + m_difficulty * 5.0f;
         }
         else // on AS leave as is
         {
            defensive -= 25.0f;
            offensive += 25.0f;
         }
      }
   }
   else if ((g_mapType & MAP_DE) && m_team == CT)
   {
      if (g_bombPlanted && GetTaskId () != TASK_ESCAPEFROMBOMB && !waypoints.GetBombPosition ().IsZero ())
      {
         if (g_bombSayString)
         {
            ChatMessage (CHAT_BOMBPLANT);
            g_bombSayString = false;
         }
         return m_chosenGoalIndex = ChooseBombWaypoint ();
      }
      defensive += 25.0f + m_difficulty * 4.0f;
      offensive -= 25.0f - m_difficulty * 0.5f;

      if (m_personality != PERSONALITY_RUSHER)
         defensive += 10.0f;
   }
   else if ((g_mapType & MAP_DE) && m_team == TERRORIST && g_timeRoundStart + 10.0f < engine.Time ())
   {
      // send some terrorists to guard planted bomb
      if (!m_defendedBomb && g_bombPlanted && GetTaskId () != TASK_ESCAPEFROMBOMB && GetBombTimeleft () >= 15.0)
         return m_chosenGoalIndex = FindDefendWaypoint (waypoints.GetBombPosition ());
   }

   goalDesire = Random.Float (0.0f, 100.0f) + offensive;
   forwardDesire = Random.Float (0.0f, 100.0f) + offensive;
   campDesire = Random.Float (0.0f, 100.0f) + defensive;
   backoffDesire = Random.Float (0.0f, 100.0f) + defensive;

   if (!UsesCampGun ())
      campDesire *= 0.5f;

   tacticChoice = backoffDesire;
   tactic = 0;

   if (campDesire > tacticChoice)
   {
      tacticChoice = campDesire;
      tactic = 1;
   }

   if (forwardDesire > tacticChoice)
   {
      tacticChoice = forwardDesire;
      tactic = 2;
   }

   if (goalDesire > tacticChoice)
      tactic = 3;

   return FinishFindGoal (tactic, defensiveWpts, offensiveWpts);
}

int Bot::FinishFindGoal (int tactic, Array <int> *defensive, Array <int> *offsensive)
{
   int goalChoices[4] = { -1, -1, -1, -1 };

   if (tactic == 0 && !(*defensive).IsEmpty ()) // careful goal
      FilterGoals (*defensive, goalChoices);
   else if (tactic == 1 && !waypoints.m_campPoints.IsEmpty ()) // camp waypoint goal
   {
      // pickup sniper points if possible for sniping bots
      if (!waypoints.m_sniperPoints.IsEmpty () && UsesSniper ())
         FilterGoals (waypoints.m_sniperPoints, goalChoices);
      else
         FilterGoals (waypoints.m_campPoints, goalChoices);
   }
   else if (tactic == 2 && !(*offsensive).IsEmpty ()) // offensive goal
      FilterGoals (*offsensive, goalChoices);
   else if (tactic == 3 && !waypoints.m_goalPoints.IsEmpty ()) // map goal waypoint
   {
      // force bomber to select closest goal, if round-start goal was reset by something
      if (m_hasC4 && g_timeRoundStart + 10.0f < engine.Time ())
      {
         float minDist = 99999.0f;
         int count = 0;

         FOR_EACH_AE (waypoints.m_goalPoints, i)
         {
            Path *path = waypoints.GetPath (waypoints.m_goalPoints[i]);

            float distance = (path->origin - pev->origin).GetLength ();

            if (distance > 1024.0f)
               continue;

            if (distance < minDist)
            {
               goalChoices[count] = i;

               if (++count > 3)
                  count = 0;

               minDist = distance;
            }
         }

         for (int i = 0; i < 4; i++)
         {
            if (goalChoices[i] == -1)
            {
               goalChoices[i] = waypoints.m_goalPoints.GetRandomElement ();
               InternalAssert (goalChoices[i] >= 0 && goalChoices[i] < g_numWaypoints);
            }
         }
      }
      else
         FilterGoals (waypoints.m_goalPoints, goalChoices);
   }

   if (m_currentWaypointIndex == -1 || m_currentWaypointIndex >= g_numWaypoints)
      m_currentWaypointIndex = ChangeWptIndex (waypoints.FindNearest (pev->origin));

   if (goalChoices[0] == -1)
      return m_chosenGoalIndex = Random.Long (0, g_numWaypoints - 1);

   bool isSorting = false;

   do
   {
      isSorting = false;

      for (int i = 0; i < 3; i++)
      {
         int testIndex = goalChoices[i + 1];

         if (testIndex < 0)
            break;

         int goal1 = m_team == TERRORIST ? (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i])->team0Value : (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i])->team1Value;
         int goal2 = m_team == TERRORIST ? (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i + 1])->team0Value : (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i + 1])->team1Value;

         if (goal1 < goal2)
         {
            goalChoices[i + 1] = goalChoices[i];
            goalChoices[i] = testIndex;

            isSorting = true;
         }
      }
   } while (isSorting);

   return m_chosenGoalIndex = goalChoices[0]; // return and store goal
}

void Bot::FilterGoals (const Array <int> &goals, int *result)
{
   // this function filters the goals, so new goal is not bot's old goal, and array of goals doesn't contains duplicate goals

   int searchCount = 0;

   for (int index = 0; index < 4; index++)
   {
      int rand = goals.GetRandomElement ();

      if (searchCount <= 8 && (m_prevGoalIndex == rand || ((result[0] == rand || result[1] == rand || result[2] == rand || result[3] == rand) && goals.GetElementNumber () > 4)) && !IsPointOccupied (rand))
      {
         if (index > 0)
            index--;

         searchCount++;
         continue;
      }
      result[index] = rand;
   }
}

bool Bot::GoalIsValid (void)
{
   int goal = GetTask ()->data;

   if (goal == -1) // not decided about a goal
      return false;
   else if (goal == m_currentWaypointIndex) // no nodes needed
      return true;
   else if (m_navNode == NULL) // no path calculated
      return false;

   // got path - check if still valid
   PathNode *node = m_navNode;

   while (node->next != NULL)
      node = node->next;

   if (node->index == goal)
      return true;

   return false;
}

void Bot::ResetCollideState (void)
{
   m_collideTime = 0.0f;
   m_probeTime = 0.0f;

   m_collisionProbeBits = 0;
   m_collisionState = COLLISION_NOTDECICED;
   m_collStateIndex = 0;

   for (int i = 0; i < MAX_COLLIDE_MOVES; i++)
      m_collideMoves[i] = 0;
}

void Bot::IgnoreCollisionShortly (void)
{
   ResetCollideState ();

   m_lastCollTime = engine.Time () + 0.35f;
   m_isStuck = false;
   m_checkTerrain = false;
}

void Bot::CheckCloseAvoidance (const Vector &dirNormal)
{
   if (m_seeEnemyTime + 1.5f < engine.Time ())
      return;

   edict_t *nearest = NULL;
   float nearestDist = 99999.0f;
   int playerCount = 0;

   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Client *cl = &g_clients[i];

      if (!(cl->flags & (CF_USED | CF_ALIVE)) || cl->ent == GetEntity () || cl->team != m_team)
         continue;

      float distance = (cl->ent->v.origin - pev->origin).GetLength ();

      if (distance < nearestDist && distance < pev->maxspeed)
      {
         nearestDist = distance;
         nearest = cl->ent;

         playerCount++;
      }
   }

   if (playerCount < 4 && IsValidPlayer (nearest))
   {
      MakeVectors (m_moveAngles); // use our movement angles

      // try to predict where we should be next frame
      Vector moved = pev->origin + g_pGlobals->v_forward * m_moveSpeed * m_frameInterval;
      moved += g_pGlobals->v_right * m_strafeSpeed * m_frameInterval;
      moved += pev->velocity * m_frameInterval;

      float nearestDistance = (nearest->v.origin - pev->origin).GetLength2D ();
      float nextFrameDistance = ((nearest->v.origin + nearest->v.velocity * m_frameInterval) - pev->origin).GetLength2D ();

      // is player that near now or in future that we need to steer away?
      if ((nearest->v.origin - moved).GetLength2D () <= 48.0f || (nearestDistance <= 56.0f && nextFrameDistance < nearestDistance))
      {
         // to start strafing, we have to first figure out if the target is on the left side or right side
         const Vector &dirToPoint = (pev->origin - nearest->v.origin).Get2D ();

         if ((dirToPoint | g_pGlobals->v_right.Get2D ()) > 0.0f)
            SetStrafeSpeed (dirNormal, pev->maxspeed);
         else
            SetStrafeSpeed (dirNormal, -pev->maxspeed);

         if (nearestDistance < 56.0f && (dirToPoint | g_pGlobals->v_forward.Get2D ()) < 0.0f)
            m_moveSpeed = -pev->maxspeed;
      }
   }
}

void Bot::CheckTerrain (float movedDistance, const Vector &dirNormal)
{
   m_isStuck = false;
   TraceResult tr;

   CheckCloseAvoidance (dirNormal);

   // Standing still, no need to check?
   // FIXME: doesn't care for ladder movement (handled separately) should be included in some way
   if ((m_moveSpeed >= 10.0f || m_strafeSpeed >= 10.0f) && m_lastCollTime < engine.Time () && m_seeEnemyTime + 0.8f < engine.Time () && GetTaskId () != TASK_ATTACK)
   {
      if (movedDistance < 2.0f && m_prevSpeed >= 20.0f) // didn't we move enough previously?
      {
         // Then consider being stuck
         m_prevTime = engine.Time ();
         m_isStuck = true;

         if (m_firstCollideTime == 0.0)
            m_firstCollideTime = engine.Time () + 0.2f;
      }
      else // not stuck yet
      {
         // test if there's something ahead blocking the way
         if (CantMoveForward (dirNormal, &tr) && !IsOnLadder ())
         {
            if (m_firstCollideTime == 0.0f)
               m_firstCollideTime = engine.Time () + 0.2f;

            else if (m_firstCollideTime <= engine.Time ())
               m_isStuck = true;
         }
         else
            m_firstCollideTime = 0.0f;
      }

      if (!m_isStuck) // not stuck?
      {
         if (m_probeTime + 0.5f < engine.Time ())
            ResetCollideState (); // reset collision memory if not being stuck for 0.5 secs
         else
         {
            // remember to keep pressing duck if it was necessary ago
            if (m_collideMoves[m_collStateIndex] == COLLISION_DUCK && (IsOnFloor () || IsInWater ()))
               pev->button |= IN_DUCK;
         }
         return;
      }
      // bot is stuck!

      Vector src;
      Vector dst;
      
      // not yet decided what to do?
      if (m_collisionState == COLLISION_NOTDECICED)
      {
         int bits = 0;

         if (IsOnLadder ())
            bits |= PROBE_STRAFE;
         else if (IsInWater ())
            bits |= (PROBE_JUMP | PROBE_STRAFE);
         else
            bits |= (PROBE_STRAFE | (m_jumpStateTimer < engine.Time () ? PROBE_JUMP : 0));

         // collision check allowed if not flying through the air
         if (IsOnFloor () || IsOnLadder () || IsInWater ())
         {
            char state[MAX_COLLIDE_MOVES * 2 + 1];
            int i = 0;

            // first 4 entries hold the possible collision states
            state[i++] = COLLISION_STRAFELEFT;
            state[i++] = COLLISION_STRAFERIGHT;
            state[i++] = COLLISION_JUMP;
            state[i++] = COLLISION_DUCK;

            if (bits & PROBE_STRAFE)
            {
               state[i] = 0;
               state[i + 1] = 0;

               // to start strafing, we have to first figure out if the target is on the left side or right side
               MakeVectors (m_moveAngles);

               Vector dirToPoint = (pev->origin - m_destOrigin).Normalize2D ();
               Vector rightSide = g_pGlobals->v_right.Normalize2D ();

               bool dirRight = false;
               bool dirLeft = false;
               bool blockedLeft = false;
               bool blockedRight = false;

               if ((dirToPoint | rightSide) > 0.0f)
                  dirRight = true;
               else
                  dirLeft = true;

               const Vector &testDir = m_moveSpeed > 0.0f ? g_pGlobals->v_forward : -g_pGlobals->v_forward;

               // now check which side is blocked
               src = pev->origin + g_pGlobals->v_right * 32.0f;
               dst = src + testDir * 32.0f;

               engine.TestHull (src, dst, TRACE_IGNORE_MONSTERS, head_hull, GetEntity (), &tr);

               if (tr.flFraction != 1.0f)
                  blockedRight = true;

               src = pev->origin - g_pGlobals->v_right * 32.0f;
               dst = src + testDir * 32.0f;

               engine.TestHull (src, dst, TRACE_IGNORE_MONSTERS, head_hull, GetEntity (), &tr);

               if (tr.flFraction != 1.0f)
                  blockedLeft = true;

               if (dirLeft)
                  state[i] += 5;
               else
                  state[i] -= 5;

               if (blockedLeft)
                  state[i] -= 5;

               i++;

               if (dirRight)
                  state[i] += 5;
               else
                  state[i] -= 5;

               if (blockedRight)
                  state[i] -= 5;
            }

            // now weight all possible states
            if (bits & PROBE_JUMP)
            {
               state[i] = 0;

               if (CanJumpUp (dirNormal))
                  state[i] += 10;

               if (m_destOrigin.z >= pev->origin.z + 18.0f)
                  state[i] += 5;

               if (EntityIsVisible (m_destOrigin))
               {
                  MakeVectors (m_moveAngles);

                  src = EyePosition ();
                  src = src + g_pGlobals->v_right * 15.0f;

                  engine.TestLine (src, m_destOrigin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

                  if (tr.flFraction >= 1.0f)
                  {
                     src = EyePosition ();
                     src = src - g_pGlobals->v_right * 15.0f;

                     engine.TestLine (src, m_destOrigin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

                     if (tr.flFraction >= 1.0f)
                        state[i] += 5;
                  }
               }
               if (pev->flags & FL_DUCKING)
                  src = pev->origin;
               else
                  src = pev->origin + Vector (0.0f, 0.0f, -17.0f);

               dst = src + dirNormal * 30.0f;
               engine.TestLine (src, dst, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

               if (tr.flFraction != 1.0f)
                  state[i] += 10;
            }
            else
               state[i] = 0;
            i++;

#if 0
            if (bits & PROBE_DUCK)
            {
               state[i] = 0;

               if (CanDuckUnder (dirNormal))
                  state[i] += 10;

               if ((m_destOrigin.z + 36.0f <= pev->origin.z) && EntityIsVisible (m_destOrigin))
                  state[i] += 5;
            }
            else
#endif
            state[i] = 0;
            i++;
 
            // weighted all possible moves, now sort them to start with most probable
            bool isSorting = false;

            do
            {
               isSorting = false;
               for (i = 0; i < 3; i++)
               {
                  if (state[i + MAX_COLLIDE_MOVES] < state[i + MAX_COLLIDE_MOVES + 1])
                  {
                     int temp = state[i];

                     state[i] = state[i + 1];
                     state[i + 1] = temp;

                     temp = state[i + MAX_COLLIDE_MOVES];

                     state[i + MAX_COLLIDE_MOVES] = state[i + MAX_COLLIDE_MOVES + 1];
                     state[i + MAX_COLLIDE_MOVES + 1] = temp;

                     isSorting = true;
                  }
               }
            } while (isSorting);

            for (i = 0; i < MAX_COLLIDE_MOVES; i++)
               m_collideMoves[i] = state[i];

            m_collideTime = engine.Time ();
            m_probeTime = engine.Time () + 0.5f;
            m_collisionProbeBits = bits;
            m_collisionState = COLLISION_PROBING;
            m_collStateIndex = 0;
         }
      }

      if (m_collisionState == COLLISION_PROBING)
      {
         if (m_probeTime < engine.Time ())
         {
            m_collStateIndex++;
            m_probeTime = engine.Time () + 0.5f;

            if (m_collStateIndex > MAX_COLLIDE_MOVES)
            {
               m_navTimeset = engine.Time () - 5.0f;
               ResetCollideState ();
            }
         }

         if (m_collStateIndex < MAX_COLLIDE_MOVES)
         {
            switch (m_collideMoves[m_collStateIndex])
            {
            case COLLISION_JUMP:
               if (IsOnFloor () || IsInWater ())
               {
                  pev->button |= IN_JUMP;
                  m_jumpStateTimer = Random.Float (1.0f, 2.0f);
               }
               break;

            case COLLISION_DUCK:
               if (IsOnFloor () || IsInWater ())
                  pev->button |= IN_DUCK;
               break;

            case COLLISION_STRAFELEFT:
               pev->button |= IN_MOVELEFT;
               SetStrafeSpeed (dirNormal, -pev->maxspeed);
               break;

            case COLLISION_STRAFERIGHT:
               pev->button |= IN_MOVERIGHT;
               SetStrafeSpeed (dirNormal, pev->maxspeed);
               break;
            }
         }
      }
   }
}

bool Bot::DoWaypointNav (void)
{
   // this function is a main path navigation

   TraceResult tr, tr2;

   // check if we need to find a waypoint...
   if (m_currentWaypointIndex == -1)
   {
      GetValidWaypoint ();
      m_waypointOrigin = m_currentPath->origin;

      // if wayzone radios non zero vary origin a bit depending on the body angles
      if (m_currentPath->radius > 0)
      {
         MakeVectors (Vector (pev->angles.x, AngleNormalize (pev->angles.y + Random.Float (-90.0f, 90.0f)), 0.0f));
         m_waypointOrigin = m_waypointOrigin + g_pGlobals->v_forward * Random.Float (0, m_currentPath->radius);
      }
      m_navTimeset = engine.Time ();
   }

   if (pev->flags & FL_DUCKING)
      m_destOrigin = m_waypointOrigin;
   else
      m_destOrigin = m_waypointOrigin + pev->view_ofs;

   float waypointDistance = (pev->origin - m_waypointOrigin).GetLength ();

   // this waypoint has additional travel flags - care about them
   if (m_currentTravelFlags & PATHFLAG_JUMP)
   {
      // bot is not jumped yet?
      if (!m_jumpFinished)
      {
         // if bot's on the ground or on the ladder we're free to jump. actually setting the correct velocity is cheating.
         // pressing the jump button gives the illusion of the bot actual jmping.
         if (IsOnFloor () || IsOnLadder ())
         {
            if (m_desiredVelocity.x != 0.0f && m_desiredVelocity.y != 0.0f)
               pev->velocity = m_desiredVelocity + m_desiredVelocity * 0.046f;

            pev->button |= IN_JUMP;

            m_jumpFinished = true;
            m_checkTerrain = false;
            m_desiredVelocity.Zero ();
         }
      }
      else if (!yb_jasonmode.GetBool () && m_currentWeapon == WEAPON_KNIFE && IsOnFloor ())
         SelectBestWeapon ();
   }

   if (m_currentPath->flags & FLAG_LADDER)
   {
      if (m_waypointOrigin.z >= (pev->origin.z + 16.0f))
         m_waypointOrigin = m_currentPath->origin + Vector (0.0f, 0.0f, 16.0f);
      else if (m_waypointOrigin.z < pev->origin.z + 16.0f && !IsOnLadder () && IsOnFloor () && !(pev->flags & FL_DUCKING))
      {
         m_moveSpeed = waypointDistance;

         if (m_moveSpeed < 150.0f)
            m_moveSpeed = 150.0f;
         else if (m_moveSpeed > pev->maxspeed)
            m_moveSpeed = pev->maxspeed;
      }
   }

   // special lift handling (code merged from podbotmm)
   if (m_currentPath->flags & FLAG_LIFT)
   {
      bool liftClosedDoorExists = false;

      // update waypoint time set
      m_navTimeset = engine.Time ();

      // trace line to door
      engine.TestLine (pev->origin, m_currentPath->origin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr2);

      if (tr2.flFraction < 1.0f && strcmp (STRING (tr2.pHit->v.classname), "func_door") == 0 && (m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && pev->groundentity != tr2.pHit)
      {
         if (m_liftState == LIFT_NO_NEARBY)
         {
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = engine.Time () + 7.0;
         }
         liftClosedDoorExists = true;
      }

      // trace line down
      engine.TestLine (m_currentPath->origin, m_currentPath->origin + Vector (0.0f, 0.0f, -50.0f), TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

      // if trace result shows us that it is a lift
      if (!engine.IsNullEntity (tr.pHit) && m_navNode != NULL && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0) && !liftClosedDoorExists)
      {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && tr.pHit->v.velocity.z == 0.0f)
         {
            if (fabsf (pev->origin.z - tr.vecEndPos.z) < 70.0f)
            {
               m_liftEntity = tr.pHit;
               m_liftState = LIFT_ENTERING_IN;
               m_liftTravelPos = m_currentPath->origin;
               m_liftUsageTime = engine.Time () + 5.0f;
            }
         }
         else if (m_liftState == LIFT_TRAVELING_BY)
         {
            m_liftState = LIFT_LEAVING;
            m_liftUsageTime = engine.Time () + 7.0f;
         }
      }
      else if (m_navNode != NULL) // no lift found at waypoint
      {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR) && m_navNode->next != NULL)
         {
            if (m_navNode->next->index >= 0 && m_navNode->next->index < g_numWaypoints && (waypoints.GetPath (m_navNode->next->index)->flags & FLAG_LIFT))
            {
               engine.TestLine (m_currentPath->origin, waypoints.GetPath (m_navNode->next->index)->origin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

               if (!engine.IsNullEntity (tr.pHit) && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0))
                  m_liftEntity = tr.pHit;
            }
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = engine.Time () + 15.0f;
         }
      }

      // bot is going to enter the lift
      if (m_liftState == LIFT_ENTERING_IN)
      {
         m_destOrigin = m_liftTravelPos;

         // check if we enough to destination
         if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = engine.Time ();
            m_aimFlags |= AIM_NAVPOINT;

            ResetCollideState ();

            // need to wait our following teammate ?
            bool needWaitForTeammate = false;

            // if some bot is following a bot going into lift - he should take the same lift to go
            for (int i = 0; i < engine.MaxClients (); i++)
            {
               Bot *bot = bots.GetBot (i);

               if (bot == NULL || bot == this)
                  continue;

               if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != GetEntity () || bot->GetTaskId () != TASK_FOLLOWUSER)
                  continue;

               if (bot->pev->groundentity == m_liftEntity && bot->IsOnFloor ())
                  break;

               bot->m_liftEntity = m_liftEntity;
               bot->m_liftState = LIFT_ENTERING_IN;
               bot->m_liftTravelPos = m_liftTravelPos;

               needWaitForTeammate = true;
            }

            if (needWaitForTeammate)
            {
               m_liftState = LIFT_WAIT_FOR_TEAMMATES;
               m_liftUsageTime = engine.Time () + 8.0f;
            }
            else
            {
               m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
               m_liftUsageTime = engine.Time () + 10.0f;
            }
         }
      }

      // bot is waiting for his teammates
      if (m_liftState == LIFT_WAIT_FOR_TEAMMATES)
      {
         // need to wait our following teammate ?
         bool needWaitForTeammate = false;

         for (int i = 0; i < engine.MaxClients (); i++)
         {
            Bot *bot = bots.GetBot (i);

            if (bot == NULL)
               continue; // skip invalid bots

            if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != GetEntity () || bot->GetTaskId () != TASK_FOLLOWUSER || bot->m_liftEntity != m_liftEntity)
               continue;

            if (bot->pev->groundentity == m_liftEntity || !bot->IsOnFloor ())
            {
               needWaitForTeammate = true;
               break;
            }
         }

         // need to wait for teammate
         if (needWaitForTeammate)
         {
            m_destOrigin = m_liftTravelPos;

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225.0f)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = engine.Time ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }

         // else we need to look for button
         if (!needWaitForTeammate || m_liftUsageTime < engine.Time ())
         {
            m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
            m_liftUsageTime = engine.Time () + 10.0;
         }
      }

      // bot is trying to find button inside a lift
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE)
      {
         edict_t *button = FindNearestButton (STRING (m_liftEntity->v.targetname));

         // got a valid button entity ?
         if (!engine.IsNullEntity (button) && pev->groundentity == m_liftEntity && m_buttonPushTime + 1.0f < engine.Time () && m_liftEntity->v.velocity.z == 0.0f && IsOnFloor ())
         {
            m_pickupItem = button;
            m_pickupType = PICKUP_BUTTON;

            m_navTimeset = engine.Time ();
         }
      }

      // is lift activated and bot is standing on it and lift is moving ?
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE || m_liftState == LIFT_ENTERING_IN || m_liftState == LIFT_WAIT_FOR_TEAMMATES || m_liftState == LIFT_WAITING_FOR)
      {
         if (pev->groundentity == m_liftEntity && m_liftEntity->v.velocity.z != 0.0f && IsOnFloor () && ((waypoints.GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT) || !engine.IsNullEntity (m_targetEntity)))
         {
            m_liftState = LIFT_TRAVELING_BY;
            m_liftUsageTime = engine.Time () + 14.0f;

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225.0f)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = engine.Time ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }
      }

      // bots is currently moving on lift
      if (m_liftState == LIFT_TRAVELING_BY)
      {
         m_destOrigin = Vector (m_liftTravelPos.x, m_liftTravelPos.y, pev->origin.z);

         if ((pev->origin - m_destOrigin).GetLengthSquared () < 225.0f)
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = engine.Time ();
            m_aimFlags |= AIM_NAVPOINT;

            ResetCollideState ();
         }
      }

      // need to find a button outside the lift
      if (m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE)
      {

         // button has been pressed, lift should come
         if (m_buttonPushTime + 8.0f >= engine.Time ())
         {
            if (m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
               m_destOrigin = waypoints.GetPath (m_prevWptIndex[0])->origin;
            else
               m_destOrigin = pev->origin;

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225.0f)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = engine.Time ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }
         else if (!engine.IsNullEntity(m_liftEntity))
         {
            edict_t *button = FindNearestButton (STRING (m_liftEntity->v.targetname));

            // if we got a valid button entity
            if (!engine.IsNullEntity (button))
            {
               // lift is already used ?
               bool liftUsed = false;

               // iterate though clients, and find if lift already used
               for (int i = 0; i < engine.MaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity () || engine.IsNullEntity (g_clients[i].ent->v.groundentity))
                     continue;

                  if (g_clients[i].ent->v.groundentity == m_liftEntity)
                  {
                     liftUsed = true;
                     break;
                  }
               }

               // lift is currently used
               if (liftUsed)
               {
                  if (m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
                     m_destOrigin = waypoints.GetPath (m_prevWptIndex[0])->origin;
                  else
                     m_destOrigin = button->v.origin;

                  if ((pev->origin - m_destOrigin).GetLengthSquared () < 225.0f)
                  {
                     m_moveSpeed = 0.0f;
                     m_strafeSpeed = 0.0f;
                  }
               }
               else
               {
                  m_pickupItem = button;
                  m_pickupType = PICKUP_BUTTON;
                  m_liftState = LIFT_WAITING_FOR;

                  m_navTimeset = engine.Time ();
                  m_liftUsageTime = engine.Time () + 20.0f;
               }
            }
            else
            {
               m_liftState = LIFT_WAITING_FOR;
               m_liftUsageTime = engine.Time () + 15.0f;
            }
         }
      }

      // bot is waiting for lift
      if (m_liftState == LIFT_WAITING_FOR)
      {
         if (m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
         {
            if (!(waypoints.GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT))
               m_destOrigin = waypoints.GetPath (m_prevWptIndex[0])->origin;
            else if (m_prevWptIndex[1] >= 0 && m_prevWptIndex[1] < g_numWaypoints)
               m_destOrigin = waypoints.GetPath (m_prevWptIndex[1])->origin;
         }

         if ((pev->origin - m_destOrigin).GetLengthSquared () < 100.0f)
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = engine.Time ();
            m_aimFlags |= AIM_NAVPOINT;

            ResetCollideState ();
         }
      }

      // if bot is waiting for lift, or going to it
      if (m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_ENTERING_IN)
      {
         // bot fall down somewhere inside the lift's groove :)
         if (pev->groundentity != m_liftEntity && m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
         {
            if ((waypoints.GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT) && (m_currentPath->origin.z - pev->origin.z) > 50.0f && (waypoints.GetPath (m_prevWptIndex[0])->origin.z - pev->origin.z) > 50.0f)
            {
               m_liftState = LIFT_NO_NEARBY;
               m_liftEntity = NULL;
               m_liftUsageTime = 0.0f;

               DeleteSearchNodes ();
               FindWaypoint ();

               if (m_prevWptIndex[2] >= 0 && m_prevWptIndex[2] < g_numWaypoints)
                  FindShortestPath (m_currentWaypointIndex, m_prevWptIndex[2]);

               return false;
            }
         }
      }
   }

   if (!engine.IsNullEntity (m_liftEntity) && !(m_currentPath->flags & FLAG_LIFT))
   {
      if (m_liftState == LIFT_TRAVELING_BY)
      {
         m_liftState = LIFT_LEAVING;
         m_liftUsageTime = engine.Time () + 10.0f;
      }
      if (m_liftState == LIFT_LEAVING && m_liftUsageTime < engine.Time () && pev->groundentity != m_liftEntity)
      {
         m_liftState = LIFT_NO_NEARBY;
         m_liftUsageTime = 0.0f;

         m_liftEntity = NULL;
      }
   }

   if (m_liftUsageTime < engine.Time () && m_liftUsageTime != 0.0f)
   {
      m_liftEntity = NULL;
      m_liftState = LIFT_NO_NEARBY;
      m_liftUsageTime = 0.0f;

      DeleteSearchNodes ();

      if (m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
      {
         if (!(waypoints.GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT))
            ChangeWptIndex (m_prevWptIndex[0]);
         else
            FindWaypoint ();
      }
      else
         FindWaypoint ();

      return false;
   }

   // check if we are going through a door...
   engine.TestLine (pev->origin, m_waypointOrigin, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   if (!engine.IsNullEntity (tr.pHit) && engine.IsNullEntity (m_liftEntity) && strncmp (STRING (tr.pHit->v.classname), "func_door", 9) == 0)
   {
      // if the door is near enough...
      if ((engine.GetAbsOrigin (tr.pHit) - pev->origin).GetLengthSquared () < 2500.0f)
      {
         IgnoreCollisionShortly (); // don't consider being stuck

         if (Random.Long (1, 100) < 50)
            MDLL_Use (tr.pHit, GetEntity ()); // also 'use' the door randomly
      }

      // make sure we are always facing the door when going through it
      m_aimFlags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);

      edict_t *button = FindNearestButton (STRING (tr.pHit->v.targetname));

      // check if we got valid button
      if (!engine.IsNullEntity (button))
      {
         m_pickupItem = button;
         m_pickupType = PICKUP_BUTTON;

         m_navTimeset = engine.Time ();
      }

      // if bot hits the door, then it opens, so wait a bit to let it open safely
      if (pev->velocity.GetLength2D () < 2 && m_timeDoorOpen < engine.Time ())
      {
         PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, engine.Time () + 1, false);

         m_doorOpenAttempt++;
         m_timeDoorOpen = engine.Time () + 1.0f; // retry in 1 sec until door is open

         edict_t *ent = NULL;

         if (m_doorOpenAttempt > 2 && !engine.IsNullEntity (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, 512.0f)))
         {
            if (IsValidPlayer (ent) && IsAlive (ent) && m_team != engine.GetTeam (ent) && GetWeaponPenetrationPower (m_currentWeapon) > 0)
            {
               m_seeEnemyTime = engine.Time () - 0.5f;

               m_states |= STATE_SEEING_ENEMY;
               m_aimFlags |= AIM_ENEMY;

               m_lastEnemy = ent;
               m_enemy = ent;
               m_lastEnemyOrigin = ent->v.origin;

            }
            else if (IsValidPlayer (ent) && IsAlive (ent) && m_team == engine.GetTeam (ent))
            {
               DeleteSearchNodes ();
               ResetTasks ();
            }
            else if (IsValidPlayer (ent) && (!IsAlive (ent) || (ent->v.deadflag & DEAD_DYING)))
               m_doorOpenAttempt = 0; // reset count
         }
      }
   }

   float desiredDistance = 0.0f;

   // initialize the radius for a special waypoint type, where the wpt is considered to be reached
   if (m_currentPath->flags & FLAG_LIFT)
      desiredDistance = 50.0f;
   else if ((pev->flags & FL_DUCKING) || (m_currentPath->flags & FLAG_GOAL))
      desiredDistance = 25.0f;
   else if (m_currentPath->flags & FLAG_LADDER)
      desiredDistance = 15.0f;
   else if (m_currentTravelFlags & PATHFLAG_JUMP)
      desiredDistance = 0.0f;
   else
      desiredDistance = m_currentPath->radius;

   // check if waypoint has a special travelflag, so they need to be reached more precisely
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (m_currentPath->connectionFlags[i] != 0)
      {
         desiredDistance = 0;
         break;
      }
   }

   // needs precise placement - check if we get past the point
   if (desiredDistance < 16.0f && waypointDistance < 30.0f && (pev->origin + (pev->velocity * m_frameInterval) - m_waypointOrigin).GetLength () > waypointDistance)
      desiredDistance = waypointDistance + 1.0f;

   if (waypointDistance < desiredDistance)
   {
      // Did we reach a destination Waypoint?
      if (GetTask ()->data == m_currentWaypointIndex)
      {
         // add goal values
         if (m_chosenGoalIndex != -1)
         {
            int waypointValue;
            int startIndex = m_chosenGoalIndex;
            int goalIndex = m_currentWaypointIndex;

            if (m_team == TERRORIST)
            {
               waypointValue = (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team0Value;
               waypointValue += static_cast <int> (pev->health * 0.5f);
               waypointValue += static_cast <int> (m_goalValue * 0.5f);

               if (waypointValue < -MAX_GOAL_VALUE)
                  waypointValue = -MAX_GOAL_VALUE;
               else if (waypointValue > MAX_GOAL_VALUE)
                  waypointValue = MAX_GOAL_VALUE;

               (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team0Value = waypointValue;
            }
            else
            {
               waypointValue = (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team1Value;
               waypointValue += static_cast <int> (pev->health * 0.5f);
               waypointValue += static_cast <int> (m_goalValue * 0.5f);

               if (waypointValue < -MAX_GOAL_VALUE)
                  waypointValue = -MAX_GOAL_VALUE;
               else if (waypointValue > MAX_GOAL_VALUE)
                  waypointValue = MAX_GOAL_VALUE;

               (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team1Value = waypointValue;
            }
         }
         return true;
      }
      else if (m_navNode == NULL)
         return false;

      int taskTarget = GetTask ()->data;

      if ((g_mapType & MAP_DE) && g_bombPlanted && m_team == CT && GetTaskId () != TASK_ESCAPEFROMBOMB && taskTarget != -1)
      {
         const Vector &bombOrigin = CheckBombAudible ();

         // bot within 'hearable' bomb tick noises?
         if (!bombOrigin.IsZero ())
         {
            float distance = (bombOrigin - waypoints.GetPath (taskTarget)->origin).GetLength ();

            if (distance > 512.0)
            {
               if (Random.Long (0, 100) < 50 && !waypoints.IsGoalVisited (taskTarget))
                  RadioMessage (Radio_SectorClear);

               waypoints.SetGoalVisited (taskTarget); // doesn't hear so not a good goal
            }
         }
         else
         {
            if (Random.Long (0, 100) < 50 && !waypoints.IsGoalVisited (taskTarget))
               RadioMessage (Radio_SectorClear);

            waypoints.SetGoalVisited (taskTarget); // doesn't hear so not a good goal
         }
      }
      HeadTowardWaypoint (); // do the actual movement checking
   }
   return false;
}


void Bot::FindShortestPath (int srcIndex, int destIndex)
{
   // this function finds the shortest path from source index to destination index

   if (srcIndex > g_numWaypoints - 1 || srcIndex < 0)
   {
      AddLogEntry (true, LL_ERROR, "Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }

   if (destIndex > g_numWaypoints - 1 || destIndex < 0)
   {
      AddLogEntry (true, LL_ERROR, "Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }

   DeleteSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   PathNode *node = new PathNode;

   node->index = srcIndex;
   node->next = NULL;

   m_navNodeStart = node;
   m_navNode = m_navNodeStart;

   while (srcIndex != destIndex)
   {
      srcIndex = *(waypoints.m_pathMatrix + (srcIndex * g_numWaypoints) + destIndex);

      if (srcIndex < 0)
      {
         m_prevGoalIndex = -1;
         GetTask ()->data = -1;

         return;
      }

      node->next = new PathNode;
      node = node->next;

      if (node == NULL)
         TerminateOnMalloc ();

      node->index = srcIndex;
      node->next = NULL;
   }
}

// priority queue class (smallest item out first, hlsdk)
class PriorityQueue
{
private:
   struct Node
   {
      int id;
      float pri;
   };

   int m_allocCount;
   int m_size;
   int m_heapSize;
   Node *m_heap;

public:

   inline bool IsEmpty (void)
   {
      return m_size == 0;
   }

   inline PriorityQueue (int initialSize = MAX_WAYPOINTS * 0.5f)
   {
      m_size = 0;
      m_heapSize = initialSize;
      m_allocCount = 0;

      m_heap = static_cast <Node *> (malloc (sizeof (Node) * m_heapSize));
   }

   inline ~PriorityQueue (void)
   {
      free (m_heap);
      m_heap = NULL;
   }

   // inserts a value into the priority queue
   inline void Push (int value, float pri)
   {
      if (m_allocCount > 20)
      {
         AddLogEntry (false, LL_FATAL, "Tried to re-allocate heap too many times in pathfinder. This usually indicates corrupted waypoint file. Please obtain new copy of waypoint.");
         return;
      }

      if (m_heap == NULL)
         return;

      if (m_size >= m_heapSize)
      {
         m_allocCount++;
         m_heapSize += 100;

         Node *newHeap = static_cast <Node *> (realloc (m_heap, sizeof (Node) * m_heapSize));

         if (newHeap != NULL)
            m_heap = newHeap;
      }

      m_heap[m_size].pri = pri;
      m_heap[m_size].id = value;

      int child = ++m_size - 1;

      while (child)
      {
         int parent = (child - 1) * 0.5f;

         if (m_heap[parent].pri <= m_heap[child].pri)
            break;

         Node ref = m_heap[child];

         m_heap[child] = m_heap[parent];
         m_heap[parent] = ref;

         child = parent;
      }
   }

   // removes the smallest item from the priority queue
   inline int Pop (void)
   {
      int result = m_heap[0].id;

      m_size--;
      m_heap[0] = m_heap[m_size];

      int parent = 0;
      int child = (2 * parent) + 1;

      Node ref = m_heap[parent];

      while (child < m_size)
      {
         int right = (2 * parent) + 2;

         if (right < m_size && m_heap[right].pri < m_heap[child].pri)
            child = right;

         if (ref.pri <= m_heap[child].pri)
            break;

         m_heap[parent] = m_heap[child];

         parent = child;
         child = (2 * parent) + 1;
      }
      m_heap[parent] = ref;
      return result;
   }
};

float gfunctionKillsDistT (int currentIndex, int parentIndex)
{
   // least kills and number of nodes to goal for a team

   if (parentIndex == -1)
      return 0.0f;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team0Damage + g_highestDamageT;

   Path *current = waypoints.GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += (g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team0Damage;
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5f;

   return static_cast <float> (waypoints.GetPathDistance (parentIndex, currentIndex)) + cost;
}


float gfunctionKillsDistCT (int currentIndex, int parentIndex)
{
   // least kills and number of nodes to goal for a team

   if (parentIndex == -1)
      return 0.0f;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team1Damage + g_highestDamageCT;

   Path *current = waypoints.GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += static_cast <int> ((g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team1Damage);
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5f;

   return static_cast <float> (waypoints.GetPathDistance (parentIndex, currentIndex)) + cost;
}

float gfunctionKillsDistCTWithHostage (int currentIndex, int parentIndex)
{
   // least kills and number of nodes to goal for a team

   Path *current = waypoints.GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355.0f;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionKillsDistCT (currentIndex, parentIndex) * 500.0f;

   return gfunctionKillsDistCT (currentIndex, parentIndex);
}

float gfunctionKillsT (int currentIndex, int)
{
   // least kills to goal for a team

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team0Damage;

   Path *current = waypoints.GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += (g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team0Damage;
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5f;

   return cost;
}

float gfunctionKillsCT (int currentIndex, int parentIndex)
{
   // least kills to goal for a team

   if (parentIndex == -1)
      return 0.0f;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team1Damage;

   Path *current = waypoints.GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += (g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team1Damage;
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5f;

   return cost + 0.5f;
}

float gfunctionKillsCTWithHostage (int currentIndex, int parentIndex)
{
   // least kills to goal for a team

   if (parentIndex == -1)
      return 0.0f;

   Path *current = waypoints.GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355.0f;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionKillsDistCT (currentIndex, parentIndex) * 500.0f;

   return gfunctionKillsCT (currentIndex, parentIndex);
}

float gfunctionPathDist (int currentIndex, int parentIndex)
{
   if (parentIndex == -1)
      return 0.0f;

   Path *parent = waypoints.GetPath (parentIndex);
   Path *current = waypoints.GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (parent->index[i] == currentIndex)
      {
         // we don't like ladder or crouch point
         if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
            return parent->distances[i] * 1.5f;

         return parent->distances[i];
      }
   }
   return 65355.0f;
}

float gfunctionPathDistWithHostage (int currentIndex, int parentIndex)
{
   Path *current = waypoints.GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355.0f;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionPathDist (currentIndex, parentIndex) * 500.0f;

   return gfunctionPathDist (currentIndex, parentIndex);
}

float hfunctionSquareDist (int index, int, int goalIndex)
{
   // square distance heuristic

   Path *start = waypoints.GetPath (index);
   Path *goal = waypoints.GetPath (goalIndex);

   float xDist = fabsf (start->origin.x - goal->origin.x);
   float yDist = fabsf (start->origin.y - goal->origin.y);
   float zDist = fabsf (start->origin.z - goal->origin.z);

   if (xDist > yDist)
      return 1.4f * yDist + (xDist - yDist) + zDist;

   return 1.4f * xDist + (yDist - xDist) + zDist;
}

float hfunctionSquareDistWithHostage (int index, int startIndex, int goalIndex)
{
   // square distance heuristic with hostages

   if (waypoints.GetPath (startIndex)->flags & FLAG_NOHOSTAGE)
      return 65355.0f;

   return hfunctionSquareDist (index, startIndex, goalIndex);
}

float hfunctionNone (int index, int startIndex, int goalIndex)
{
   return hfunctionSquareDist (index, startIndex, goalIndex) / (128.0f * 10.0f);
}

float hfunctionNumberNodes (int index, int startIndex, int goalIndex)
{
   return hfunctionSquareDist (index, startIndex, goalIndex) / 128.0f * g_highestKills;
}

void Bot::FindPath(int srcIndex, int destIndex, SearchPathType pathType /*= SEARCH_PATH_FASTEST */)
{
   // this function finds a path from srcIndex to destIndex

   if (srcIndex > g_numWaypoints - 1 || srcIndex < 0)
   {
      AddLogEntry (true, LL_ERROR, "Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }

   if (destIndex > g_numWaypoints - 1 || destIndex < 0)
   {
      AddLogEntry (true, LL_ERROR, "Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }
   DeleteSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   // A* Stuff
   enum AStarState {OPEN, CLOSED, NEW};

   struct AStar
   {
      float g;
      float f;
      short parentIndex;
      AStarState state;
   } astar[MAX_WAYPOINTS];

   PriorityQueue openList;

   for (int i = 0; i < MAX_WAYPOINTS; i++)
   {
      astar[i].g = 0.0f;
      astar[i].f = 0.0f;
      astar[i].parentIndex = -1;
      astar[i].state = NEW;
   }

   float (*gcalc) (int, int) = NULL;
   float (*hcalc) (int, int, int) = NULL;

   switch (pathType)
   {
   case SEARCH_PATH_FASTEST:
      if ((g_mapType & MAP_CS) && HasHostage ())
      {
         gcalc = gfunctionPathDistWithHostage;
         hcalc = hfunctionSquareDistWithHostage;
      }
      else
      {
         gcalc = gfunctionPathDist;
         hcalc = hfunctionSquareDist;
      }
      break;

   case SEARCH_PATH_SAFEST_FASTER:
      if (m_team == TERRORIST)
      {
         gcalc = gfunctionKillsDistT;
         hcalc = hfunctionSquareDist;
      }
      else if ((g_mapType & MAP_CS) && HasHostage ())
      {
         gcalc = gfunctionKillsDistCTWithHostage;
         hcalc = hfunctionSquareDistWithHostage;
      }
      else
      {
         gcalc = gfunctionKillsDistCT;
         hcalc = hfunctionSquareDist;
      }
      break;

   case SEARCH_PATH_SAFEST:
   default:
      if (m_team == TERRORIST)
      {
         gcalc = gfunctionKillsT;
         hcalc = hfunctionNone;
      }
      else if ((g_mapType & MAP_CS) && HasHostage ())
      {
         gcalc = gfunctionKillsDistCTWithHostage;
         hcalc = hfunctionNone;
      }
      else
      {
         gcalc = gfunctionKillsCT;
         hcalc = hfunctionNone;
      }
      break;
   }

   // put start node into open list
   astar[srcIndex].g = gcalc (srcIndex, -1);
   astar[srcIndex].f = astar[srcIndex].g + hcalc (srcIndex, srcIndex, destIndex);
   astar[srcIndex].state = OPEN;

   openList.Push (srcIndex, astar[srcIndex].g);

   while (!openList.IsEmpty ())
   {
      // remove the first node from the open list
      int currentIndex = openList.Pop ();

      if (currentIndex < 0 || currentIndex > g_numWaypoints)
      {
         AddLogEntry (false, LL_FATAL, "openList.Pop () = %d. It's not possible to continue execution. Please obtain better waypoint.");
         return;
      }

      // is the current node the goal node?
      if (currentIndex == destIndex)
      {
         // build the complete path
         m_navNode = NULL;

         do
         {
            PathNode *path = new PathNode;

            path->index = currentIndex;
            path->next = m_navNode;

            m_navNode = path;
            currentIndex = astar[currentIndex].parentIndex;

         } while (currentIndex != -1);

         m_navNodeStart = m_navNode;
         return;
      }

      if (astar[currentIndex].state != OPEN)
         continue;

      // put current node into CLOSED list
      astar[currentIndex].state = CLOSED;

      // now expand the current node
      for (int i = 0; i < MAX_PATH_INDEX; i++)
      {
         int currentChild = waypoints.GetPath (currentIndex)->index[i];

         if (currentChild == -1)
            continue;

         // calculate the F value as F = G + H
         float g = astar[currentIndex].g + gcalc (currentChild, currentIndex);
         float h = hcalc (currentChild, srcIndex, destIndex);
         float f = g + h;

         if (astar[currentChild].state == NEW || astar[currentChild].f > f)
         {
            // put the current child into open list
            astar[currentChild].parentIndex = currentIndex;
            astar[currentChild].state = OPEN;

            astar[currentChild].g = g;
            astar[currentChild].f = f;

            openList.Push (currentChild, g);
         }
      }
   }
   FindShortestPath (srcIndex, destIndex); // A* found no path, try floyd pathfinder instead
}

void Bot::DeleteSearchNodes (void)
{
   PathNode *deletingNode = NULL;
   PathNode *node = m_navNodeStart;

   while (node != NULL)
   {
      deletingNode = node->next;
      delete node;

      node = deletingNode;
   }
   m_navNodeStart = NULL;
   m_navNode = NULL;
   m_chosenGoalIndex = -1;
}

int Bot::GetAimingWaypoint (const Vector &to)
{
   // return the most distant waypoint which is seen from the bot to the target and is within count

   if (m_currentWaypointIndex == -1)
      m_currentWaypointIndex = ChangeWptIndex (waypoints.FindNearest (pev->origin));

   int srcIndex = m_currentWaypointIndex;
   int destIndex = waypoints.FindNearest (to);
   int bestIndex = srcIndex;

   while (destIndex != srcIndex)
   {
      destIndex = *(waypoints.m_pathMatrix + (destIndex * g_numWaypoints) + srcIndex);

      if (destIndex < 0)
         break;

      if (waypoints.IsVisible (m_currentWaypointIndex, destIndex))
      {
         bestIndex = destIndex;
         break;
      }
   }
   return bestIndex;
}

bool Bot::FindWaypoint (void)
{
   // this function find a waypoint in the near of the bot if bot had lost his path of pathfinder needs
   // to be restarted over again.

   int waypointIndeces[3], coveredWaypoint = -1, i = 0;
   float reachDistances[3];

   // nullify all waypoint search stuff
   for (i = 0; i < 3; i++)
   {
      waypointIndeces[i] = -1;
      reachDistances[i] = 9999.0f;
   }

   // do main search loop
   for (i = 0; i < g_numWaypoints; i++)
   {
      // ignore current waypoint and previous recent waypoints...
      if (i == m_currentWaypointIndex || i == m_prevWptIndex[0] || i == m_prevWptIndex[1] || i == m_prevWptIndex[2])
         continue;

      if ((g_mapType & MAP_CS) && HasHostage () && (waypoints.GetPath (i)->flags & FLAG_NOHOSTAGE))
         continue;

      // ignore non-reacheable waypoints...
      if (!waypoints.Reachable (this, i))
         continue;

      // check if waypoint is already used by another bot...
      if (IsPointOccupied (i))
      {
         coveredWaypoint = i;
         continue;
      }

      // now pick 1-2 random waypoints that near the bot
      float distance = (waypoints.GetPath (i)->origin - pev->origin).GetLengthSquared ();

      // now fill the waypoint list
      for (int j = 0; j < 3; j++)
      {
         if (distance < reachDistances[j])
         {
            waypointIndeces[j] = i;
            reachDistances[j] = distance;
         }
      }
   }

   // now pick random one from choosen
   if (waypointIndeces[2] != -1)
      i = Random.Long (0, 2);

   else if (waypointIndeces[1] != -1)
      i = Random.Long (0, 1);

   else if (waypointIndeces[0] != -1)
      i = Random.Long (0, 0);

   else if (coveredWaypoint != -1)
   {
      i = 0;
      waypointIndeces[i] = coveredWaypoint;
   }
   else
   {
      i = 0;

      Array <int> found;
      waypoints.FindInRadius (found, 256.0f, pev->origin);

      if (!found.IsEmpty ())
      {
         bool gotId = false;
         int random = found.GetRandomElement ();;

         while (!found.IsEmpty ())
         {
            int index = found.Pop ();

            if (!waypoints.Reachable (this, index))
               continue;

            waypointIndeces[i] = index;
            gotId = true;

            break;
         }

         if (!gotId)
            waypointIndeces[i] = random;
      }
      else
         waypointIndeces[i] = Random.Long (0, g_numWaypoints - 1);
   }

   m_collideTime = engine.Time ();
   ChangeWptIndex (waypointIndeces[i]);

   return true;
}

void Bot::GetValidWaypoint (void)
{
   // checks if the last waypoint the bot was heading for is still valid

   // if bot hasn't got a waypoint we need a new one anyway or if time to get there expired get new one as well
   if (m_currentWaypointIndex == -1)
   {
      DeleteSearchNodes ();
      FindWaypoint ();

      m_waypointOrigin = m_currentPath->origin;

      // FIXME: Do some error checks if we got a waypoint
   }
   else if (m_navTimeset + GetEstimatedReachTime () < engine.Time () && engine.IsNullEntity (m_enemy))
   {
      if (m_team == TERRORIST)
      {
         int value = (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + m_currentWaypointIndex)->team0Damage;
         value += 100;

         if (value > MAX_DAMAGE_VALUE)
            value = MAX_DAMAGE_VALUE;

         (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + m_currentWaypointIndex)->team0Damage = static_cast <unsigned short> (value);

         // affect nearby connected with victim waypoints
         for (int i = 0; i < MAX_PATH_INDEX; i++)
         {
            if ((m_currentPath->index[i] > -1) && (m_currentPath->index[i] < g_numWaypoints))
            {
               value = (g_experienceData + (m_currentPath->index[i] * g_numWaypoints) + m_currentPath->index[i])->team0Damage;
               value += 2;

               if (value > MAX_DAMAGE_VALUE)
                  value = MAX_DAMAGE_VALUE;

               (g_experienceData + (m_currentPath->index[i] * g_numWaypoints) + m_currentPath->index[i])->team0Damage = static_cast <unsigned short> (value);
            }
         }
      }
      else
      {
         int value = (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + m_currentWaypointIndex)->team1Damage;
         value += 100;

         if (value > MAX_DAMAGE_VALUE)
            value = MAX_DAMAGE_VALUE;

         (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + m_currentWaypointIndex)->team1Damage = static_cast <unsigned short> (value);

         // affect nearby connected with victim waypoints
         for (int i = 0; i < MAX_PATH_INDEX; i++)
         {
            if ((m_currentPath->index[i] > -1) && (m_currentPath->index[i] < g_numWaypoints))
            {
               value = (g_experienceData + (m_currentPath->index[i] * g_numWaypoints) + m_currentPath->index[i])->team1Damage;
               value += 2;

               if (value > MAX_DAMAGE_VALUE)
                  value = MAX_DAMAGE_VALUE;

               (g_experienceData + (m_currentPath->index[i] * g_numWaypoints) + m_currentPath->index[i])->team1Damage = static_cast <unsigned short> (value);
            }
         }
      }
      DeleteSearchNodes ();
      
      if (m_goalFailed > 1)
      {
         int newGoal = FindGoal ();

         m_prevGoalIndex = newGoal;
         m_chosenGoalIndex = newGoal;

         // remember index
         GetTask ()->data = newGoal;

         // do path finding if it's not the current waypoint
         if (newGoal != m_currentWaypointIndex)
            FindPath (m_currentWaypointIndex, newGoal, m_pathType);

         m_goalFailed = 0;
      }
      else
      {
         FindWaypoint ();
         m_goalFailed++;
      }

      m_waypointOrigin = m_currentPath->origin;
   }
}

int Bot::ChangeWptIndex(int waypointIndex)
{
   if (waypointIndex == -1)
      return 0;

   m_prevWptIndex[4] = m_prevWptIndex[3];
   m_prevWptIndex[3] = m_prevWptIndex[2];
   m_prevWptIndex[2] = m_prevWptIndex[1];
   m_prevWptIndex[0] = m_currentWaypointIndex;

   m_currentWaypointIndex = waypointIndex;
   m_navTimeset = engine.Time ();

   m_currentPath = waypoints.GetPath (m_currentWaypointIndex);
   m_waypointFlags = m_currentPath->flags;

   return m_currentWaypointIndex; // to satisfy static-code analyzers
}

int Bot::ChooseBombWaypoint (void)
{
   // this function finds the best goal (bomb) waypoint for CTs when searching for a planted bomb.

   Array <int> goals = waypoints.m_goalPoints;

   if (goals.IsEmpty ())
      return Random.Long (0, g_numWaypoints - 1); // reliability check

   Vector bombOrigin = CheckBombAudible ();

   // if bomb returns no valid vector, return the current bot pos
   if (bombOrigin.IsZero ())
      bombOrigin = pev->origin;

   int goal = 0, count = 0;
   float lastDistance = 999999.0f;

   // find nearest goal waypoint either to bomb (if "heard" or player)
   FOR_EACH_AE (goals, i)
   {
      float distance = (waypoints.GetPath (goals[i])->origin - bombOrigin).GetLengthSquared ();

      // check if we got more close distance
      if (distance < lastDistance)
      {
         goal = goals[i];
         lastDistance = distance;
      }
   }

   while (waypoints.IsGoalVisited (goal))
   {
      goal = goals.GetRandomElement ();

      if (count++ >= goals.GetElementNumber ())
         break;
   }
   return goal;
}

int Bot::FindDefendWaypoint (const Vector &origin)
{
   // this function tries to find a good position which has a line of sight to a position,
   // provides enough cover point, and is far away from the defending position

   TraceResult tr;

   int waypointIndex[MAX_PATH_INDEX];
   int minDistance[MAX_PATH_INDEX];

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      waypointIndex[i] = -1;
      minDistance[i] = 128;
   }

   int posIndex = waypoints.FindNearest (origin);
   int srcIndex = waypoints.FindNearest (pev->origin);

   // some of points not found, return random one
   if (srcIndex == -1 || posIndex == -1)
      return Random.Long (0, g_numWaypoints - 1);

   for (int i = 0; i < g_numWaypoints; i++) // find the best waypoint now
   {
      // exclude ladder & current waypoints
      if ((waypoints.GetPath (i)->flags & FLAG_LADDER) || i == srcIndex || !waypoints.IsVisible (i, posIndex) || IsPointOccupied (i))
         continue;

      // use the 'real' pathfinding distances
      int distance = waypoints.GetPathDistance (srcIndex, i);

      // skip wayponts with distance more than 512 units
      if (distance > 512)
         continue;

      engine.TestLine (waypoints.GetPath (i)->origin, waypoints.GetPath (posIndex)->origin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

      // check if line not hit anything
      if (tr.flFraction != 1.0f)
         continue;

      for (int j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (distance > minDistance[j])
         {
            waypointIndex[j] = i;
            minDistance[j] = distance;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (waypointIndex[i] != -1)
      {
         Experience *exp = (g_experienceData + (waypointIndex[i] * g_numWaypoints) + waypointIndex[i]);
         int experience = -1;

         if (m_team == TERRORIST)
            experience = exp->team0Damage;
         else
            experience = exp->team1Damage;

         experience = (experience * 100) / (m_team == TERRORIST ? g_highestDamageT : g_highestDamageCT);
         minDistance[i] = (experience * 100) / 8192;
         minDistance[i] += experience;
      }
   }

   bool isOrderChanged = false;

   // sort results waypoints for farest distance
   do
   {
      isOrderChanged = false;

      // completely sort the data
      for (int i = 0; i < 3 && waypointIndex[i] != -1 && waypointIndex[i + 1] != -1 && minDistance[i] > minDistance[i + 1]; i++)
      {
         int index = waypointIndex[i];

         waypointIndex[i] = waypointIndex[i + 1];
         waypointIndex[i + 1] = index;

         index = minDistance[i];

         minDistance[i] = minDistance[i + 1];
         minDistance[i + 1] = index;

         isOrderChanged = true;
      }
   } while (isOrderChanged);

   

   if (waypointIndex[0] == -1)
   {
      Array <int> found;

      for (int i = 0; i < g_numWaypoints; i++)
      {
         if ((waypoints.GetPath (i)->origin - origin).GetLength () <= 1248.0f && !waypoints.IsVisible (i, posIndex) && !IsPointOccupied (i))
            found.Push (i);
      }

      if (found.IsEmpty ())
         return Random.Long (0, g_numWaypoints - 1); // most worst case, since there a evil error in waypoints

      return found.GetRandomElement ();
   }

   int index = 0;

   for (; index < MAX_PATH_INDEX; index++)
   {
      if (waypointIndex[index] == -1)
         break;
   }
   return waypointIndex[Random.Long (0, (index - 1) * 0.5f)];
}

int Bot::FindCoverWaypoint (float maxDistance)
{
   // this function tries to find a good cover waypoint if bot wants to hide

   // do not move to a position near to the enemy
   if (maxDistance > (m_lastEnemyOrigin - pev->origin).GetLength ())
      maxDistance = (m_lastEnemyOrigin - pev->origin).GetLength ();

   if (maxDistance < 300.0f)
      maxDistance = 300.0f;

   int srcIndex = m_currentWaypointIndex;
   int enemyIndex = waypoints.FindNearest (m_lastEnemyOrigin);
   Array <int> enemyIndices;

   int waypointIndex[MAX_PATH_INDEX];
   int minDistance[MAX_PATH_INDEX];

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      waypointIndex[i] = -1;
      minDistance[i] = static_cast <int> (maxDistance);
   }

   if (enemyIndex == -1)
      return -1;

   // now get enemies neigbouring points
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (waypoints.GetPath (enemyIndex)->index[i] != -1)
         enemyIndices.Push (waypoints.GetPath (enemyIndex)->index[i]);
   }

   // ensure we're on valid point
   ChangeWptIndex (srcIndex);

   // find the best waypoint now
   for (int i = 0; i < g_numWaypoints; i++)
   {
      // exclude ladder, current waypoints and waypoints seen by the enemy
      if ((waypoints.GetPath (i)->flags & FLAG_LADDER) || i == srcIndex || waypoints.IsVisible (enemyIndex, i))
         continue;

      bool neighbourVisible = false;  // now check neighbour waypoints for visibility

      FOR_EACH_AE (enemyIndices, j)
      {
         if (waypoints.IsVisible (enemyIndices[j], i))
         {
            neighbourVisible = true;
            break;
         }
      }

      // skip visible points
      if (neighbourVisible)
         continue;

      // use the 'real' pathfinding distances
      int distances = waypoints.GetPathDistance (srcIndex, i);
      int enemyDistance = waypoints.GetPathDistance (enemyIndex, i);

      if (distances >= enemyDistance)
         continue;

      for (int j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (distances < minDistance[j])
         {
            waypointIndex[j] = i;
            minDistance[j] = distances;

            break;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (waypointIndex[i] != -1)
      {
         Experience *exp = (g_experienceData + (waypointIndex[i] * g_numWaypoints) + waypointIndex[i]);
         int experience = -1;

         if (m_team == TERRORIST)
            experience = exp->team0Damage;
         else
            experience = exp->team1Damage;

         experience = (experience * 100) / MAX_DAMAGE_VALUE;
         minDistance[i] = (experience * 100) / 8192;
         minDistance[i] += experience;
      }
   }

   bool isOrderChanged;

   // sort resulting waypoints for nearest distance
   do
   {
      isOrderChanged = false;

      for (int i = 0; i < 3 && waypointIndex[i] != -1 && waypointIndex[i + 1] != -1 && minDistance[i] > minDistance[i + 1]; i++)
      {
         int index = waypointIndex[i];

         waypointIndex[i] = waypointIndex[i + 1];
         waypointIndex[i + 1] = index;
         index = minDistance[i];
         minDistance[i] = minDistance[i + 1];
         minDistance[i + 1] = index;

         isOrderChanged = true;
      }
   } while (isOrderChanged);

   TraceResult tr;

   // take the first one which isn't spotted by the enemy
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (waypointIndex[i] != -1)
      {
         engine.TestLine (m_lastEnemyOrigin + Vector (0.0f, 0.0f, 36.0f), waypoints.GetPath (waypointIndex[i])->origin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

         if (tr.flFraction < 1.0f)
            return waypointIndex[i];
      }
   }

   // if all are seen by the enemy, take the first one
   if (waypointIndex[0] != -1)
      return waypointIndex[0];

   return -1; // do not use random points
}

bool Bot::GetBestNextWaypoint (void)
{
   // this function does a realtime post processing of waypoints return from the
   // pathfinder, to vary paths and find the best waypoint on our way

   InternalAssert (m_navNode != NULL);
   InternalAssert (m_navNode->next != NULL);

   if (!IsPointOccupied (m_navNode->index))
      return false;

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int id = m_currentPath->index[i];

      if (id != -1 && waypoints.IsConnected (id, m_navNode->next->index) && waypoints.IsConnected (m_currentWaypointIndex, id))
      {
         if (waypoints.GetPath (id)->flags & FLAG_LADDER) // don't use ladder waypoints as alternative
            continue;

         if (!IsPointOccupied (id))
         {
            m_navNode->index = id;
            return true;
         }
      }
   }
   return false;
}

bool Bot::HeadTowardWaypoint (void)
{
   // advances in our pathfinding list and sets the appropiate destination origins for this bot

   GetValidWaypoint (); // check if old waypoints is still reliable

   // no waypoints from pathfinding?
   if (m_navNode == NULL)
      return false;

   TraceResult tr;

   m_navNode = m_navNode->next; // advance in list
   m_currentTravelFlags = 0; // reset travel flags (jumping etc)

   // we're not at the end of the list?
   if (m_navNode != NULL)
   {
      // if in between a route, postprocess the waypoint (find better alternatives)...
      if (m_navNode != m_navNodeStart && m_navNode->next != NULL)
      {
         GetBestNextWaypoint ();
         m_minSpeed = pev->maxspeed;

         // only if we in normal task and bomb is not planted
         if (GetTaskId () == TASK_NORMAL && g_timeRoundMid + 5.0f < engine.Time () && m_timeCamping + 5.0f < engine.Time () && !g_bombPlanted && m_personality != PERSONALITY_RUSHER && !m_hasC4 && !m_isVIP && m_loosedBombWptIndex == -1 && !HasHostage ())
         {
            m_campButtons = 0;

            int nextIndex = m_navNode->next->index;
            float kills = 0;

            if (m_team == TERRORIST)
               kills = (g_experienceData + (nextIndex * g_numWaypoints) + nextIndex)->team0Damage;
            else
               kills = (g_experienceData + (nextIndex * g_numWaypoints) + nextIndex)->team1Damage;

            // if damage done higher than one
            if (kills > 1.0f && g_timeRoundMid > engine.Time ())
            {
               switch (m_personality)
               {
               case PERSONALITY_NORMAL:
                  kills *= 0.33f;
                  break;

               default:
                  kills *= 0.5f;
                  break;
               }

               if (m_baseAgressionLevel < kills && HasPrimaryWeapon ())
               {
                  PushTask (TASK_CAMP, TASKPRI_CAMP, -1, engine.Time () + Random.Float (m_difficulty * 0.5f, m_difficulty) * 5.0f, true);
                  PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, FindDefendWaypoint (waypoints.GetPath (nextIndex)->origin), engine.Time () + Random.Float (3.0f, 10.0f), true);
               }
            }
            else if (g_botsCanPause && !IsOnLadder () && !IsInWater () && !m_currentTravelFlags && IsOnFloor ())
            {
               if (static_cast <float> (kills) == m_baseAgressionLevel)
                  m_campButtons |= IN_DUCK;
               else if (Random.Long (1, 100) > m_difficulty * 25)
                  m_minSpeed = GetWalkSpeed ();
            }
         }
      }

      if (m_navNode != NULL)
      {
         int destIndex = m_navNode->index;

         // find out about connection flags
         if (m_currentWaypointIndex != -1)
         {
            for (int i = 0; i < MAX_PATH_INDEX; i++)
            {
               if (m_currentPath->index[i] == m_navNode->index)
               {
                  m_currentTravelFlags = m_currentPath->connectionFlags[i];
                  m_desiredVelocity = m_currentPath->connectionVelocity[i];
                  m_jumpFinished = false;

                  break;
               }
            }

            // check if bot is going to jump
            bool willJump = false;
            float jumpDistance = 0.0f;

            Vector src;
            Vector dst;
            
            // try to find out about future connection flags
            if (m_navNode->next != NULL)
            {
               for (int i = 0; i < MAX_PATH_INDEX; i++)
               {
                  Path *path = waypoints.GetPath (m_navNode->index);
                  Path *next = waypoints.GetPath (m_navNode->next->index);

                  if (path->index[i] == m_navNode->next->index && (path->connectionFlags[i] & PATHFLAG_JUMP))
                  {
                     src = path->origin;
                     dst = next->origin;

                     jumpDistance = (path->origin - next->origin).GetLength ();
                     willJump = true;

                     break;
                  }
               }
            }

            // is there a jump waypoint right ahead and do we need to draw out the light weapon ?
            if (willJump && m_currentWeapon != WEAPON_KNIFE && m_currentWeapon != WEAPON_SCOUT && !m_isReloading && !UsesPistol () && (jumpDistance > 210.0f || (dst.z + 32.0f > src.z && jumpDistance > 150.0f) || ((dst - src).GetLength2D () < 60.0f && jumpDistance > 60.0f)) && engine.IsNullEntity (m_enemy))
               SelectWeaponByName ("weapon_knife"); // draw out the knife if we needed

            // bot not already on ladder but will be soon?
            if ((waypoints.GetPath (destIndex)->flags & FLAG_LADDER) && !IsOnLadder ())
            {
               // get ladder waypoints used by other (first moving) bots
               for (int c = 0; c < engine.MaxClients (); c++)
               {
                  Bot *otherBot = bots.GetBot (c);

                  // if another bot uses this ladder, wait 3 secs
                  if (otherBot != NULL && otherBot != this && IsAlive (otherBot->GetEntity ()) && otherBot->m_currentWaypointIndex == m_navNode->index)
                  {
                     PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, engine.Time () + 3.0f, false);
                     return true;
                  }
               }
            }
         }
         ChangeWptIndex (destIndex);
      }
   }
   m_waypointOrigin = m_currentPath->origin;

   // if wayzone radius non zero vary origin a bit depending on the body angles
   if (m_currentPath->radius > 0.0f)
   {
      MakeVectors (Vector (pev->angles.x, AngleNormalize (pev->angles.y + Random.Float (-90.0f, 90.0f)), 0.0f));
      m_waypointOrigin = m_waypointOrigin + g_pGlobals->v_forward * Random.Float (0.0f, m_currentPath->radius);
   }

   if (IsOnLadder ())
   {
      engine.TestLine (Vector (pev->origin.x, pev->origin.y, pev->absmin.z), m_waypointOrigin, TRACE_IGNORE_EVERYTHING, GetEntity (), &tr);

      if (tr.flFraction < 1.0f)
         m_waypointOrigin = m_waypointOrigin + (pev->origin - m_waypointOrigin) * 0.5f + Vector (0.0f, 0.0f, 32.0f);
   }
   m_navTimeset = engine.Time ();

   return true;
}

bool Bot::CantMoveForward (const Vector &normal, TraceResult *tr)
{
   // checks if bot is blocked in his movement direction (excluding doors)

   // use some TraceLines to determine if anything is blocking the current path of the bot.

   // first do a trace from the bot's eyes forward...
   Vector src = EyePosition ();
   Vector forward = src + normal * 24.0f;

   MakeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   // trace from the bot's eyes straight forward...
   engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f)
   {
      if (strncmp ("func_door", STRING (tr->pHit->v.classname), 9) == 0)
         return false;

      return true;  // bot's head will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder left diagonal forward to the right shoulder...
   src = EyePosition () + Vector (0.0f, 0.0f, -16.0f) - g_pGlobals->v_right * -16.0f;
   forward = EyePosition () + Vector (0.0f, 0.0f, -16.0f) + g_pGlobals->v_right * 16.0f + normal * 24.0f;

   engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
      return true;  // bot's body will hit something

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder right diagonal forward to the left shoulder...
   src = EyePosition () + Vector (0.0f, 0.0f, -16.0f) + g_pGlobals->v_right * 16.0f;
   forward = EyePosition () + Vector (0.0f, 0.0f, -16.0f) - g_pGlobals->v_right * -16.0f + normal * 24.0f;

   engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
      return true;  // bot's body will hit something

   // now check below waist
   if (pev->flags & FL_DUCKING)
   {
      src = pev->origin + Vector (0.0f, 0.0f, -19.0f + 19.0f);
      forward = src + Vector (0.0f, 0.0f, 10.0f) + normal * 24.0f;

      engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something

      src = pev->origin;
      forward = src + normal * 24.0f;

      engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something
   }
   else
   {
      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0.0f, 0.0f, -17.0f) - g_pGlobals->v_right * -16.0f;
      forward = pev->origin + Vector (0.0f, 0.0f, -17.0f) + g_pGlobals->v_right * 16.0f + normal * 24.0f;

      // trace from the bot's waist straight forward...
      engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something

      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0.0f, 0.0f, -17.0f) + g_pGlobals->v_right * 16.0f;
      forward = pev->origin + Vector (0.0f, 0.0f, -17.0f) - g_pGlobals->v_right * -16.0f + normal * 24.0f;

      engine.TestLine (src, forward, TRACE_IGNORE_MONSTERS, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something
   }
   return false;  // bot can move forward, return false
}

#ifdef DEAD_CODE
bool Bot::CanStrafeLeft (TraceResult *tr)
{
   // this function checks if bot can move sideways

   MakeVectors (pev->v_angle);

   Vector src = pev->origin;
   Vector left = src - g_pGlobals->v_right * -40.0f;

   // trace from the bot's waist straight left...
   TraceLine (src, left, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f)
      return false;  // bot's body will hit something

   src = left;
   left = left + g_pGlobals->v_forward * 40.0f;

   // trace from the strafe pos straight forward...
   TraceLine (src, left, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f)
      return false;  // bot's body will hit something

   return true;
}

bool Bot::CanStrafeRight (TraceResult * tr)
{
   // this function checks if bot can move sideways

   MakeVectors (pev->v_angle);

   Vector src = pev->origin;
   Vector right = src + g_pGlobals->v_right * 40.0f;

   // trace from the bot's waist straight right...
   TraceLine (src, right, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f)
      return false;  // bot's body will hit something

   src = right;
   right = right + g_pGlobals->v_forward * 40.0f;

   // trace from the strafe pos straight forward...
   TraceLine (src, right, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f)
      return false;  // bot's body will hit something

   return true;
}

#endif

bool Bot::CanJumpUp (const Vector &normal)
{
   // this function check if bot can jump over some obstacle

   TraceResult tr;

   // can't jump if not on ground and not on ladder/swimming
   if (!IsOnFloor () && (IsOnLadder () || !IsInWater ()))
      return false;

   // convert current view angle to vectors for traceline math...
   MakeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   // check for normal jump height first...
   Vector src = pev->origin + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   Vector dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   if (tr.flFraction < 1.0f)
      return FinishCanJumpUp (normal);
   else
   {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

      if (tr.flFraction < 1.0f)
         return false;
   }

   // now check same height to one side of the bot...
   src = pev->origin + g_pGlobals->v_right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return FinishCanJumpUp (normal);

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now check same height on the other side of the bot...
   src = pev->origin + (-g_pGlobals->v_right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return FinishCanJumpUp (normal);

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

bool Bot::FinishCanJumpUp (const Vector &normal)
{
   // use center of the body first... maximum duck jump height is 62, so check one unit above that (63)
   Vector src = pev->origin + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   Vector dest = src + normal * 32.0f;

   TraceResult tr;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   if (tr.flFraction < 1.0f)
      return false;
   else
   {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

      // if trace hit something, check duckjump
      if (tr.flFraction < 1.0f)
         return false;
   }

   // now check same height to one side of the bot...
   src = pev->origin + g_pGlobals->v_right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now check same height on the other side of the bot...
   src = pev->origin + (-g_pGlobals->v_right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

bool Bot::CanDuckUnder (const Vector &normal)
{
   // this function check if bot can duck under obstacle

   TraceResult tr;
   Vector baseHeight;

   // convert current view angle to vectors for TraceLine math...
   MakeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   // use center of the body first...
   if (pev->flags & FL_DUCKING)
      baseHeight = pev->origin + Vector (0.0f, 0.0f, -17.0f);
   else
      baseHeight = pev->origin;

   Vector src = baseHeight;
   Vector dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now check same height to one side of the bot...
   src = baseHeight + g_pGlobals->v_right * 16.0f;
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f)
      return false;

   // now check same height on the other side of the bot...
   src = baseHeight + (-g_pGlobals->v_right * 16.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   engine.TestLine (src, dest, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

#ifdef DEAD_CODE

bool Bot::IsBlockedLeft (void)
{
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0.0f)
      direction = -48;

   MakeVectors (pev->angles);

   // do a trace to the left...
   engine.TestLine (pev->origin, g_pGlobals->v_forward * direction - g_pGlobals->v_right * 48.0f, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f && strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0)
      return true;  // bot's body will hit something

   return false;
}

bool Bot::IsBlockedRight (void)
{
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0)
      direction = -48;

   MakeVectors (pev->angles);

   // do a trace to the right...
   engine.TestLine (pev->origin, pev->origin + g_pGlobals->v_forward * direction + g_pGlobals->v_right * 48.0f, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f && (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0))
      return true; // bot's body will hit something

   return false;
}

#endif

bool Bot::CheckWallOnLeft (void)
{
   TraceResult tr;
   MakeVectors (pev->angles);

   engine.TestLine (pev->origin, pev->origin - g_pGlobals->v_right * 40.0f, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
      return true;

   return false;
}

bool Bot::CheckWallOnRight (void)
{
   TraceResult tr;
   MakeVectors (pev->angles);

   // do a trace to the right...
   engine.TestLine (pev->origin, pev->origin + g_pGlobals->v_right * 40.0f, TRACE_IGNORE_MONSTERS, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f)
      return true;

   return false;
}

bool Bot::IsDeadlyDrop (const Vector &to)
{
   // this function eturns if given location would hurt Bot with falling damage

   Vector botPos = pev->origin;
   TraceResult tr;

   Vector move ((to - botPos).ToYaw (), 0.0f, 0.0f);
   MakeVectors (move);

   Vector direction = (to - botPos).Normalize ();  // 1 unit long
   Vector check = botPos;
   Vector down = botPos;

   down.z = down.z - 1000.0f;  // straight down 1000 units

   engine.TestHull (check, down, TRACE_IGNORE_MONSTERS, head_hull, GetEntity (), &tr);

   if (tr.flFraction > 0.036f) // we're not on ground anymore?
      tr.flFraction = 0.036f;

   float lastHeight = tr.flFraction * 1000.0f;  // height from ground

   float distance = (to - check).GetLength ();  // distance from goal

   while (distance > 16.0f)
   {
      check = check + direction * 16.0f; // move 10 units closer to the goal...

      down = check;
      down.z = down.z - 1000.0f;  // straight down 1000 units

      engine.TestHull (check, down, TRACE_IGNORE_MONSTERS, head_hull, GetEntity (), &tr);

      if (tr.fStartSolid) // Wall blocking?
         return false;

      float height = tr.flFraction * 1000.0f;  // height from ground

      if (lastHeight < height - 100.0f) // Drops more than 100 Units?
         return true;

      lastHeight = height;
      distance = (to - check).GetLength ();  // distance from goal
   }
   return false;
}

#ifdef DEAD_CODE

void Bot::ChangePitch (float speed)
{
   // this function turns a bot towards its ideal_pitch

   float idealPitch = AngleNormalize (pev->idealpitch);
   float curent = AngleNormalize (pev->v_angle.x);

   // turn from the current v_angle pitch to the idealpitch by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0.0f)
   {
      if (normalizePitch > speed)
         normalizePitch = speed;
   }
   else
   {
      if (normalizePitch < -speed)
         normalizePitch = -speed;
   }

   pev->v_angle.x = AngleNormalize (curent + normalizePitch);

   if (pev->v_angle.x > 89.9f)
      pev->v_angle.x = 89.9f;

   if (pev->v_angle.x < -89.9f)
      pev->v_angle.x = -89.9f;

   pev->angles.x = -pev->v_angle.x / 3;
}

void Bot::ChangeYaw (float speed)
{
   // this function turns a bot towards its ideal_yaw

   float idealPitch = AngleNormalize (pev->ideal_yaw);
   float curent = AngleNormalize (pev->v_angle.y);

   // turn from the current v_angle yaw to the ideal_yaw by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0.0f)
   {
      if (normalizePitch > speed)
         normalizePitch = speed;
   }
   else
   {
      if (normalizePitch < -speed)
         normalizePitch = -speed;
   }
   pev->v_angle.y = AngleNormalize (curent + normalizePitch);
   pev->angles.y = pev->v_angle.y;
}

#endif

int Bot::GetAimingWaypoint (void)
{
   // find a good waypoint to look at when camping

   int count = 0, indices[3];
   float distTab[3];
   uint16 visibility[3];

   int currentWaypoint = m_currentWaypointIndex;

   if (currentWaypoint == -1)
      return Random.Long (0, g_numWaypoints - 1);

   for (int i = 0; i < g_numWaypoints; i++)
   {
      if (currentWaypoint == i || !waypoints.IsVisible (currentWaypoint, i))
         continue;

      Path *path = waypoints.GetPath (i);

      if (count < 3)
      {
         indices[count] = i;

         distTab[count] = (pev->origin - path->origin).GetLengthSquared ();
         visibility[count] = path->vis.crouch + path->vis.stand;

         count++;
      }
      else
      {
         float distance = (pev->origin - path->origin).GetLengthSquared ();
         uint16 visBits = path->vis.crouch + path->vis.stand;

         for (int j = 0; j < 3; j++)
         {
            if (visBits >= visibility[j] && distance > distTab[j])
            {
               indices[j] = i;

               distTab[j] = distance;
               visibility[j] = visBits;

               break;
            }
         }
      }
   }
   count--;

   if (count >= 0)
      return indices[Random.Long (0, count)];

   return Random.Long (0, g_numWaypoints - 1);
}

void Bot::UpdateBodyAngles (void)
{
   // set the body angles to point the gun correctly
   pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
   pev->angles.y = pev->v_angle.y;

   pev->angles.ClampAngles ();
}

void Bot::UpdateLookAngles (void)
{
   const float delta = Clamp <float> (engine.Time () - m_lookUpdateTime, MATH_EQEPSILON, 0.05f);
   m_lookUpdateTime = engine.Time ();

   // adjust all body and view angles to face an absolute vector
   Vector direction = (m_lookAt - EyePosition ()).ToAngles ();
   direction.x *= -1.0f; // invert for engine

   direction.ClampAngles ();

   // lower skilled bot's have lower aiming
   if (m_difficulty < 3)
   {
      UpdateLookAnglesLowSkill (direction, delta);
      UpdateBodyAngles ();

      return;
   }

   // this is what makes bot almost godlike (got it from sypb)
   if (m_difficulty > 3 && (m_aimFlags & AIM_ENEMY) && (m_wantsToFire || UsesSniper ()) && yb_whose_your_daddy.GetBool ())
   {
      pev->v_angle = direction;
      UpdateBodyAngles ();

      return;
   }
   bool needPreciseAim = (m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE | AIM_LAST_ENEMY | AIM_CAMP) || GetTaskId () == TASK_SHOOTBREAKABLE);

   float accelerate = needPreciseAim ? 3600.0f : 3000.0f;
   float stiffness = needPreciseAim ? 300.0f : 200.0f;
   float damping = needPreciseAim ? 20.0f : 25.0f;

   m_idealAngles = pev->v_angle;

   float angleDiffYaw = AngleDiff (direction.y, m_idealAngles.y);
   float angleDiffPitch = AngleDiff (direction.x, m_idealAngles.x);

   if (angleDiffYaw < 1.0f && angleDiffYaw > -1.0f)
   {
      m_lookYawVel = 0.0f;
      m_idealAngles.y = direction.y;
   }
   else
   {
      float accel = stiffness * angleDiffYaw - damping * m_lookYawVel;

      if (accel > accelerate)
         accel = accelerate;
      else if (accel < -accelerate)
         accel = -accelerate;

      m_lookYawVel += delta * accel;
      m_idealAngles.y += delta * m_lookYawVel;
   }
   float accel = 2.0f * stiffness * angleDiffPitch - damping * m_lookPitchVel;

   if (accel > accelerate)
      accel = accelerate;
   else if (accel < -accelerate)
      accel = -accelerate;

   m_lookPitchVel += delta * accel;
   m_idealAngles.x += delta * m_lookPitchVel;

   if (m_idealAngles.x < -89.0f)
      m_idealAngles.x = -89.0f;
   else if (m_idealAngles.x > 89.0f)
      m_idealAngles.x = 89.0f;

   pev->v_angle = m_idealAngles;
   pev->v_angle.ClampAngles ();

   UpdateBodyAngles ();
}

void Bot::UpdateLookAnglesLowSkill (const Vector &direction, const float delta)
{
   Vector spring (13.0f, 13.0f, 0.0f);
   Vector damperCoefficient (0.22f, 0.22f, 0.0f);

   Vector influence = Vector (0.25f, 0.17f, 0.0f) * ((100 - (m_difficulty * 25)) / 100.f);
   Vector randomization = Vector (2.0f, 0.18f, 0.0f)  * ((100 - (m_difficulty * 25)) / 100.f);

   const float noTargetRatio = 0.3f;
   const float offsetDelay = 1.2f;

   Vector stiffness;
   Vector randomize;

   m_idealAngles = direction.Get2D ();
   m_idealAngles.ClampAngles ();

   if (m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE | AIM_LAST_ENEMY) || GetTaskId () == TASK_SHOOTBREAKABLE)
   {
      m_playerTargetTime = engine.Time ();
      m_randomizedIdealAngles = m_idealAngles;

      stiffness = spring * (0.2f + (m_difficulty * 25) / 125.0f);
   }
   else
   {
      // is it time for bot to randomize the aim direction again (more often where moving) ?
      if (m_randomizeAnglesTime < engine.Time () && ((pev->velocity.GetLength () > 1.0f && m_angularDeviation.GetLength () < 5.0f) || m_angularDeviation.GetLength () < 1.0f))
      {
         // is the bot standing still ?
         if (pev->velocity.GetLength () < 1.0f)
            randomize = randomization * 0.2f; // randomize less
         else
            randomize = randomization;

         // randomize targeted location a bit (slightly towards the ground)
         m_randomizedIdealAngles = m_idealAngles + Vector (Random.Float (-randomize.x * 0.5f, randomize.x * 1.5f), Random.Float (-randomize.y, randomize.y), 0.0f);

         // set next time to do this
         m_randomizeAnglesTime = engine.Time () + Random.Float (0.4f, offsetDelay);
      }
      float stiffnessMultiplier = noTargetRatio;

      // take in account whether the bot was targeting someone in the last N seconds
      if (engine.Time () - (m_playerTargetTime + offsetDelay) < noTargetRatio * 10.0f)
      {
         stiffnessMultiplier = 1.0f - (engine.Time () - m_timeLastFired) * 0.1f;

         // don't allow that stiffness multiplier less than zero
         if (stiffnessMultiplier < 0.0f)
            stiffnessMultiplier = 0.5f;
      }

      // also take in account the remaining deviation (slow down the aiming in the last 10°)
      stiffnessMultiplier *= m_angularDeviation.GetLength () * 0.1f * 0.5f;

      // but don't allow getting below a certain value
      if (stiffnessMultiplier < 0.35f)
         stiffnessMultiplier = 0.35f;

      stiffness = spring * stiffnessMultiplier; // increasingly slow aim
   }
   // compute randomized angle deviation this time
   m_angularDeviation = m_randomizedIdealAngles - pev->v_angle;
   m_angularDeviation.ClampAngles ();

   // spring/damper model aiming
   m_aimSpeed.x = stiffness.x * m_angularDeviation.x - damperCoefficient.x * m_aimSpeed.x;
   m_aimSpeed.y = stiffness.y * m_angularDeviation.y - damperCoefficient.y * m_aimSpeed.y;

   // influence of y movement on x axis and vice versa (less influence than x on y since it's
   // easier and more natural for the bot to "move its mouse" horizontally than vertically)
   m_aimSpeed.x += m_aimSpeed.y * influence.y;
   m_aimSpeed.y += m_aimSpeed.x * influence.x;

   // move the aim cursor
   pev->v_angle = pev->v_angle + delta * Vector (m_aimSpeed.x, m_aimSpeed.y, 0.0f);
   pev->v_angle.ClampAngles ();
}

void Bot::SetStrafeSpeed (const Vector &moveDir, float strafeSpeed)
{
   MakeVectors (pev->angles);

   const Vector &los = (moveDir - pev->origin).Normalize2D ();
   float dot = los | g_pGlobals->v_forward.Get2D ();

   if (dot > 0.0f && !CheckWallOnRight ())
      m_strafeSpeed = strafeSpeed;
   else if (!CheckWallOnLeft ())
      m_strafeSpeed = -strafeSpeed;
}

int Bot::FindPlantedBomb (void)
{
   // this function tries to find planted c4 on the defuse scenario map and returns nearest to it waypoint

   if (m_team != TERRORIST || !(g_mapType & MAP_DE))
      return -1; // don't search for bomb if the player is CT, or it's not defusing bomb

   edict_t *bombEntity = NULL; // temporaly pointer to bomb

   // search the bomb on the map
   while (!engine.IsNullEntity (bombEntity = FIND_ENTITY_BY_CLASSNAME (bombEntity, "grenade")))
   {
      if (strcmp (STRING (bombEntity->v.model) + 9, "c4.mdl") == 0)
      {
         int nearestIndex = waypoints.FindNearest (engine.GetAbsOrigin (bombEntity));

         if (nearestIndex >= 0 && nearestIndex < g_numWaypoints)
            return nearestIndex;

         break;
      }
   }
   return -1;
}

bool Bot::IsPointOccupied (int index)
{
   if (index < 0 || index >= g_numWaypoints)
      return true;

   // first check if current waypoint of one of the bots is index waypoint
   for (int i = 0; i < engine.MaxClients (); i++)
   {
      Bot *bot = bots.GetBot (i);

      if (bot == NULL || bot == this)
         continue;

      // check if this waypoint is already used
      // TODO: take in account real players

      if (bot->m_notKilled && m_currentWaypointIndex != -1 && bot->m_prevWptIndex[0] != -1)
      {
         int occupyId = GetShootingConeDeviation (bot->GetEntity (), &pev->origin) >= 0.7f ? bot->m_prevWptIndex[0] : m_currentWaypointIndex;

         // length check
         float length = (waypoints.GetPath (occupyId)->origin - waypoints.GetPath (index)->origin).GetLengthSquared ();

         if (occupyId == index || bot->GetTask ()->data == index || length < GET_SQUARE (64.0f))
            return true;
      }
   }
   return false;
}

edict_t *Bot::FindNearestButton (const char *targetName)
{
   // this function tries to find nearest to current bot button, and returns pointer to
   // it's entity, also here must be specified the target, that button must open.

   if (IsNullString (targetName))
      return NULL;

   float nearestDistance = 99999.0f;
   edict_t *searchEntity = NULL, *foundEntity = NULL;

   // find the nearest button which can open our target
   while (!engine.IsNullEntity(searchEntity = FIND_ENTITY_BY_TARGET (searchEntity, targetName)))
   {
      Vector entityOrign = engine.GetAbsOrigin (searchEntity);

      // check if this place safe
      if (!IsDeadlyDrop (entityOrign))
      {
         float distance = (pev->origin - entityOrign).GetLengthSquared ();

         // check if we got more close button
         if (distance <= nearestDistance)
         {
            nearestDistance = distance;
            foundEntity = searchEntity;
         }
      }
   }
   return foundEntity;
}