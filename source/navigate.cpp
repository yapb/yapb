//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_aim_method ("yb_aim_method", "3", VT_NOSERVER);

// any user ever altered this stuff? left this for debug-only builds
#if !defined (NDEBUG)

ConVar yb_aim_damper_coefficient_x ("yb_aim_damper_coefficient_x", "0.22", VT_NOSERVER);
ConVar yb_aim_damper_coefficient_y ("yb_aim_damper_coefficient_y", "0.22", VT_NOSERVER);

ConVar yb_aim_deviation_x ("yb_aim_deviation_x", "2.0", VT_NOSERVER);
ConVar yb_aim_deviation_y ("yb_aim_deviation_y", "1.0", VT_NOSERVER);

ConVar yb_aim_influence_x_on_y ("yb_aim_influence_x_on_y", "0.26", VT_NOSERVER);
ConVar yb_aim_influence_y_on_x ("yb_aim_influence_y_on_x", "0.18", VT_NOSERVER);

ConVar yb_aim_notarget_slowdown_ratio ("yb_aim_notarget_slowdown_ratio", "0.6", VT_NOSERVER);
ConVar yb_aim_offset_delay ("yb_aim_offset_delay", "0.5", VT_NOSERVER);

ConVar yb_aim_spring_stiffness_x ("yb_aim_spring_stiffness_x", "15.0", VT_NOSERVER);
ConVar yb_aim_spring_stiffness_y ("yb_aim_spring_stiffness_y", "14.0", VT_NOSERVER);

ConVar yb_aim_target_anticipation_ratio ("yb_aim_target_anticipation_ratio", "5.0", VT_NOSERVER);

#endif

int Bot::FindGoal (void)
{
   // chooses a destination (goal) waypoint for a bot
   if (!g_bombPlanted && m_team == TEAM_TF && (g_mapType & MAP_DE))
   {
      edict_t *pent = NULL;

      while (!IsEntityNull (pent = FIND_ENTITY_BY_STRING (pent, "classname", "weaponbox")))
      {
         if (strcmp (STRING (pent->v.model), "models/w_backpack.mdl") == 0)
         {
            int index = waypoint->FindNearest (GetEntityOrigin (pent));

            if (index >= 0 && index < g_numWaypoints)
               return m_loosedBombWptIndex = index;

            break;
         }
      }
   }
   int tactic;

   // path finding behaviour depending on map type
   float offensive;
   float defensive;

   float goalDesire;
   float forwardDesire;
   float campDesire;
   float backoffDesire;
   float tacticChoice;

   Array <int> offensiveWpts;
   Array <int> defensiveWpts;

   switch (m_team)
   {
   case TEAM_TF:
      offensiveWpts = waypoint->m_ctPoints;
      defensiveWpts = waypoint->m_terrorPoints;
      break;

   case TEAM_CF:
      offensiveWpts = waypoint->m_terrorPoints;
      defensiveWpts = waypoint->m_ctPoints;
      break;
   }

   // terrorist carrying the C4?
   if (m_hasC4 || m_isVIP)
   {
      tactic = 3;
      goto TacticChoosen;
   }
   else if (m_team == TEAM_CF && HasHostage ())
   {
      tactic = 2;
      offensiveWpts = waypoint->m_rescuePoints;

      goto TacticChoosen;
   }

   offensive = m_agressionLevel * 100;
   defensive = m_fearLevel * 100;

   if (g_mapType & (MAP_AS | MAP_CS))
   {
      if (m_team == TEAM_TF)
      {
         defensive += 25.0f;
         offensive -= 25.0f;
      }
      else if (m_team == TEAM_CF)
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
   else if ((g_mapType & MAP_DE) && m_team == TEAM_CF)
   {
      if (g_bombPlanted && GetTaskId () != TASK_ESCAPEFROMBOMB && waypoint->GetBombPosition () != nullvec)
      {
         if (g_bombSayString)
         {
            ChatMessage (CHAT_BOMBPLANT);
            g_bombSayString = false;
         }
         return m_chosenGoalIndex = ChooseBombWaypoint ();
      }
      defensive += 25.0f;
      offensive -= 25.0f;

      if (m_personality != PERSONALITY_RUSHER)
         defensive += 10.0f;
   }
   else if ((g_mapType & MAP_DE) && m_team == TEAM_TF && g_timeRoundStart + 10.0f < GetWorldTime ())
   {
      // send some terrorists to guard planter bomb
      if (g_bombPlanted && GetTaskId () != TASK_ESCAPEFROMBOMB && GetBombTimeleft () >= 15.0)
         return m_chosenGoalIndex = FindDefendWaypoint (waypoint->GetBombPosition ());
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

TacticChoosen:
   int goalChoices[4] = {-1, -1, -1, -1};

   if (tactic == 0 && !defensiveWpts.IsEmpty ()) // careful goal
      FilterGoals (defensiveWpts, goalChoices);
   else if (tactic == 1 && !waypoint->m_campPoints.IsEmpty ()) // camp waypoint goal
   {
      // pickup sniper points if possible for sniping bots
      if (!waypoint->m_sniperPoints.IsEmpty () && UsesSniper ())
         FilterGoals (waypoint->m_sniperPoints, goalChoices);
      else
         FilterGoals (waypoint->m_campPoints, goalChoices);
   }
   else if (tactic == 2 && !offensiveWpts.IsEmpty ()) // offensive goal
      FilterGoals (offensiveWpts, goalChoices);
   else if (tactic == 3 && !waypoint->m_goalPoints.IsEmpty ()) // map goal waypoint
   {
      // forcee bomber to select closest goal, if round-start goal was reset by something
      if (m_hasC4 && g_timeRoundStart + 20.0f < GetWorldTime ())
      {
         float minDist = 1024.0f;
         int count = 0;

         for (int i = 0; i < g_numWaypoints; i++)
         {
            Path *path = waypoint->GetPath (i);

            if (!(path->flags & FLAG_GOAL))
               continue;

            float distance = (path->origin - pev->origin).GetLength ();

            if (distance < minDist)
            {
               if (count < 4)
               {
                  goalChoices[count] = i;
                  count++;
               }
               minDist = distance;
            }
         }

         for (int i = 0; i < 4; i++)
         {
            if (goalChoices[i] == -1)
            {
               goalChoices[i] = waypoint->m_goalPoints.GetRandomElement ();
               InternalAssert (goalChoices[i] >= 0 && goalChoices[i] < g_numWaypoints);
            }
         }
      }
      else
         FilterGoals (waypoint->m_goalPoints, goalChoices);
   }

   if (m_currentWaypointIndex == -1 || m_currentWaypointIndex >= g_numWaypoints)
      m_currentWaypointIndex = ChangeWptIndex (waypoint->FindNearest (pev->origin));

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

         int goal1 = m_team == TEAM_TF ? (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i])->team0Value : (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i])->team1Value;
         int goal2 = m_team == TEAM_TF ? (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i + 1])->team0Value : (g_experienceData + (m_currentWaypointIndex * g_numWaypoints) + goalChoices[i + 1])->team1Value;

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

   int totalGoals = goals.GetElementNumber ();
   int searchCount = 0;

   for (int index = 0; index < 4; index++)
   {
      int rand = goals.GetRandomElement ();

      if (searchCount <= 8 && (m_prevGoalIndex == rand || ((result[0] == rand || result[1] == rand || result[2] == rand || result[3] == rand) && totalGoals > 4)) && !IsPointOccupied (rand))
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
   m_collideTime = 0.0;
   m_probeTime = 0.0;

   m_collisionProbeBits = 0;
   m_collisionState = COLLISION_NOTDECICED;
   m_collStateIndex = 0;

   m_isStuck = false;

   for (int i = 0; i < MAX_COLLIDE_MOVES; i++)
      m_collideMoves[i] = 0;
}

void Bot::CheckCloseAvoidance (const Vector &dirNormal)
{
   if (m_seeEnemyTime + 1.0f < GetWorldTime () || g_timeRoundStart + 10.0f < GetWorldTime ())
      return;

   edict_t *nearest = NULL;
   float nearestDist = 99999.0f;
   int playerCount = 0;

   for (int i = 0; i < GetMaxClients (); i++)
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

   if (playerCount < 3 && IsValidPlayer (nearest))
   {
      MakeVectors (m_moveAngles); // use our movement angles

      // try to predict where we should be next frame
      Vector moved = pev->origin + g_pGlobals->v_forward * m_moveSpeed * m_frameInterval;
      moved += g_pGlobals->v_right * m_strafeSpeed * m_frameInterval;
      moved += pev->velocity * m_frameInterval;

      float nearestDistance = (nearest->v.origin - pev->origin).GetLength2D ();
      float nextFrameDistance = ((nearest->v.origin + nearest->v.velocity * m_frameInterval) - pev->origin).GetLength2D ();

      // is player that near now or in future that we need to steer away?
      if ((nearest->v.origin - moved).GetLength2D () <= 48.0 || (nearestDistance <= 56.0 && nextFrameDistance < nearestDistance))
      {
         // to start strafing, we have to first figure out if the target is on the left side or right side
         Vector dirToPoint = (pev->origin - nearest->v.origin).Get2D ();

         if ((dirToPoint | g_pGlobals->v_right.Get2D ()) > 0.0)
            SetStrafeSpeed (dirNormal, pev->maxspeed);
         else
            SetStrafeSpeed (dirNormal, -pev->maxspeed);

         ResetCollideState ();

         if (nearestDistance < 56.0 && (dirToPoint | g_pGlobals->v_forward.Get2D ()) < 0.0)
            m_moveSpeed = -pev->maxspeed;
      }
   }
}

void Bot::CheckTerrain (float movedDistance, const Vector &dirNormal)
{
   m_isStuck = false;

   Vector src = nullvec;
   Vector dst = nullvec;

   TraceResult tr;

   CheckCloseAvoidance (dirNormal);

   // Standing still, no need to check?
   // FIXME: doesn't care for ladder movement (handled separately) should be included in some way
   if ((m_moveSpeed >= 10 || m_strafeSpeed >= 10) && m_lastCollTime < GetWorldTime ())
   {
      bool cantMoveForward = false;

      if (movedDistance < 2.0 && m_prevSpeed >= 1.0) // didn't we move enough previously?
      {
         // Then consider being stuck
         m_prevTime = GetWorldTime ();
         m_isStuck = true;

         if (m_firstCollideTime == 0.0)
            m_firstCollideTime = GetWorldTime () + 0.2;
      }
      else // not stuck yet
      {
         // test if there's something ahead blocking the way
         if ((cantMoveForward = CantMoveForward (dirNormal, &tr)) && !IsOnLadder ())
         {
            if (m_firstCollideTime == 0.0)
               m_firstCollideTime = GetWorldTime () + 0.2;

            else if (m_firstCollideTime <= GetWorldTime ())
               m_isStuck = true;
         }
         else
            m_firstCollideTime = 0.0;
      }

      if (!m_isStuck) // not stuck?
      {
         if (m_probeTime + 0.5f < GetWorldTime ())
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
      
      // not yet decided what to do?
      if (m_collisionState == COLLISION_NOTDECICED)
      {
         int bits = 0;

         if (IsOnLadder ())
            bits |= PROBE_STRAFE;
         else if (IsInWater ())
            bits |= (PROBE_JUMP | PROBE_STRAFE);
         else
            bits |= ((Random.Long (0, 20) > (cantMoveForward ? 10 : 7) ? PROBE_JUMP : 0) | PROBE_STRAFE | PROBE_DUCK);

         // collision check allowed if not flying through the air
         if (IsOnFloor () || IsOnLadder () || IsInWater ())
         {
            char state[MAX_COLLIDE_MOVES * 2];
            int i = 0;

            // first 4 entries hold the possible collision states
            state[i++] = COLLISION_JUMP;
            state[i++] = COLLISION_DUCK;
            state[i++] = COLLISION_STRAFELEFT;
            state[i++] = COLLISION_STRAFERIGHT;

            // now weight all possible states
            if (bits & PROBE_JUMP)
            {
               state[i] = 0;

               if (CanJumpUp (dirNormal))
                  state[i] += 10;

               if (m_destOrigin.z >= pev->origin.z + 18.0)
                  state[i] += 5;

               if (EntityIsVisible (m_destOrigin))
               {
                  MakeVectors (m_moveAngles);

                  src = EyePosition ();
                  src = src + g_pGlobals->v_right * 15;

                  TraceLine (src, m_destOrigin, true, true, GetEntity (), &tr);

                  if (tr.flFraction >= 1.0)
                  {
                     src = EyePosition ();
                     src = src - g_pGlobals->v_right * 15;

                     TraceLine (src, m_destOrigin, true, true, GetEntity (), &tr);

                     if (tr.flFraction >= 1.0)
                        state[i] += 5;
                  }
               }
               if (pev->flags & FL_DUCKING)
                  src = pev->origin;
               else
                  src = pev->origin + Vector (0, 0, -17);

               dst = src + dirNormal * 30;
               TraceLine (src, dst, true, true, GetEntity (), &tr);

               if (tr.flFraction != 1.0)
                  state[i] += 10;
            }
            else
               state[i] = 0;
            i++;

            if (bits & PROBE_DUCK)
            {
               state[i] = 0;

               if (CanDuckUnder (dirNormal))
                  state[i] += 10;

               if ((m_destOrigin.z + 36.0 <= pev->origin.z) && EntityIsVisible (m_destOrigin))
                  state[i] += 5;
            }
            else
               state[i] = 0;
            i++;

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

               if ((dirToPoint | rightSide) > 0)
                  dirRight = true;
               else
                  dirLeft = true;

               const Vector &testDir = m_moveSpeed > 0.0f ? g_pGlobals->v_forward : -g_pGlobals->v_forward;

               // now check which side is blocked
               src = pev->origin + (g_pGlobals->v_right * 32);
               dst = src + testDir * 32;

               TraceHull (src, dst, true, head_hull, GetEntity (), &tr);

               if (tr.flFraction != 1.0)
                  blockedRight = true;

               src = pev->origin - (g_pGlobals->v_right * 32);
               dst = src + testDir * 32;

               TraceHull (src, dst, true, head_hull, GetEntity (), &tr);

               if (tr.flFraction != 1.0)
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

            m_collideTime = GetWorldTime ();
            m_probeTime = GetWorldTime () + 0.5;
            m_collisionProbeBits = bits;
            m_collisionState = COLLISION_PROBING;
            m_collStateIndex = 0;
         }
      }

      if (m_collisionState == COLLISION_PROBING)
      {
         if (m_probeTime < GetWorldTime ())
         {
            m_collStateIndex++;
            m_probeTime = GetWorldTime () + 0.5;

            if (m_collStateIndex > MAX_COLLIDE_MOVES)
            {
               m_navTimeset = GetWorldTime () - 5.0;
               ResetCollideState ();
            }
         }

         if (m_collStateIndex < MAX_COLLIDE_MOVES)
         {
            switch (m_collideMoves[m_collStateIndex])
            {
            case COLLISION_JUMP:
               if (IsOnFloor () || IsInWater ())
                  pev->button |= IN_JUMP;
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

   // check if we fallen from something
   if (IsOnFloor () && m_jumpFinished && m_currentWaypointIndex > 0)
   {
      const Vector &wptOrg = m_currentPath->origin;

      if ((pev->origin - wptOrg).GetLength2D () <= 100.0f && (wptOrg.z > pev->origin.z + 20.0f))
         m_currentWaypointIndex = -1;
   }

   // check if we need to find a waypoint...
   if (m_currentWaypointIndex == -1)
   {
      GetValidWaypoint ();
      m_waypointOrigin = m_currentPath->origin;

      // if wayzone radios non zero vary origin a bit depending on the body angles
      if (m_currentPath->radius > 0)
      {
         MakeVectors (Vector (pev->angles.x, AngleNormalize (pev->angles.y + Random.Float (-90, 90)), 0.0));
         m_waypointOrigin = m_waypointOrigin + g_pGlobals->v_forward * Random.Float (0, m_currentPath->radius);
      }
      m_navTimeset = GetWorldTime ();
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
               pev->velocity = m_desiredVelocity + m_desiredVelocity * 0.076f;

            pev->button |= IN_JUMP;

            m_jumpFinished = true;
            m_checkTerrain = false;
            m_desiredVelocity = nullvec;
         }
      }
      else if (!yb_jasonmode.GetBool () && m_currentWeapon == WEAPON_KNIFE && IsOnFloor ())
         SelectBestWeapon ();
   }

   if (m_currentPath->flags & FLAG_LADDER)
   {
      if (m_waypointOrigin.z >= (pev->origin.z + 16.0))
         m_waypointOrigin = m_currentPath->origin + Vector (0, 0, 16);
      else if (m_waypointOrigin.z < pev->origin.z + 16.0 && !IsOnLadder () && IsOnFloor () && !(pev->flags & FL_DUCKING))
      {
         m_moveSpeed = waypointDistance;

         if (m_moveSpeed < 150.0)
            m_moveSpeed = 150.0;
         else if (m_moveSpeed > pev->maxspeed)
            m_moveSpeed = pev->maxspeed;
      }
   }

   // special lift handling (code merged from podbotmm)
   if (m_currentPath->flags & FLAG_LIFT)
   {

      bool liftClosedDoorExists = false;

      // update waypoint time set
      m_navTimeset = GetWorldTime ();

      // trace line to door
      TraceLine (pev->origin, m_currentPath->origin, true, true, GetEntity (), &tr2);

      if (tr2.flFraction < 1.0 && strcmp (STRING (tr2.pHit->v.classname), "func_door") == 0 && (m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && pev->groundentity != tr2.pHit)
      {
         if (m_liftState == LIFT_NO_NEARBY)
         {
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = GetWorldTime () + 7.0;
         }
         liftClosedDoorExists = true;
      }

      // trace line down
      TraceLine (m_currentPath->origin, m_currentPath->origin + Vector (0, 0, -50), true, true, GetEntity (), &tr);

      // if trace result shows us that it is a lift
      if (!IsEntityNull (tr.pHit) && m_navNode != NULL && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0) && !liftClosedDoorExists)
      {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && tr.pHit->v.velocity.z == 0)
         {
            if (fabsf (pev->origin.z - tr.vecEndPos.z) < 70.0)
            {
               m_liftEntity = tr.pHit;
               m_liftState = LIFT_ENTERING_IN;
               m_liftTravelPos = m_currentPath->origin;
               m_liftUsageTime = GetWorldTime () + 5.0;
            }
         }
         else if (m_liftState == LIFT_TRAVELING_BY)
         {
            m_liftState = LIFT_LEAVING;
            m_liftUsageTime = GetWorldTime () + 7.0;
         }
      }
      else if (m_navNode != NULL) // no lift found at waypoint
      {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR) && m_navNode->next != NULL)
         {
            if (m_navNode->next->index >= 0 && m_navNode->next->index < g_numWaypoints && (waypoint->GetPath (m_navNode->next->index)->flags & FLAG_LIFT))
            {
               TraceLine (m_currentPath->origin, waypoint->GetPath (m_navNode->next->index)->origin, true, true, GetEntity (), &tr);

               if (!IsEntityNull (tr.pHit) && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0))
                  m_liftEntity = tr.pHit;
            }
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = GetWorldTime () + 15.0;
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

            m_navTimeset = GetWorldTime ();
            m_aimFlags |= AIM_NAVPOINT;

            ResetCollideState ();

            // need to wait our following teammate ?
            bool needWaitForTeammate = false;

            // if some bot is following a bot going into lift - he should take the same lift to go
            for (int i = 0; i < GetMaxClients (); i++)
            {
               Bot *bot = botMgr->GetBot (i);

               if (bot == NULL || bot == this)
                  continue;

               if (!IsAlive (bot->GetEntity ()) || GetTeam (bot->GetEntity ()) != m_team || bot->m_targetEntity != GetEntity () || bot->GetTaskId () != TASK_FOLLOWUSER)
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
               m_liftUsageTime = GetWorldTime () + 8.0;
            }
            else
            {
               m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
               m_liftUsageTime = GetWorldTime () + 10.0;
            }
         }
      }

      // bot is waiting for his teammates
      if (m_liftState == LIFT_WAIT_FOR_TEAMMATES)
      {
         // need to wait our following teammate ?
         bool needWaitForTeammate = false;

         for (int i = 0; i < GetMaxClients (); i++)
         {
            Bot *bot = botMgr->GetBot (i);

            if (bot == NULL)
               continue; // skip invalid bots

            if (!IsAlive (bot->GetEntity ()) || GetTeam (bot->GetEntity ()) != m_team || bot->m_targetEntity != GetEntity () || bot->GetTaskId () != TASK_FOLLOWUSER || bot->m_liftEntity != m_liftEntity)
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

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = GetWorldTime ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }

         // else we need to look for button
         if (!needWaitForTeammate || (m_liftUsageTime < GetWorldTime ()))
         {
            m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
            m_liftUsageTime = GetWorldTime () + 10.0;
         }
      }

      // bot is trying to find button inside a lift
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE)
      {
         edict_t *button = FindNearestButton (STRING (m_liftEntity->v.targetname));

         // got a valid button entity ?
         if (!IsEntityNull (button) && pev->groundentity == m_liftEntity && m_buttonPushTime + 1.0 < GetWorldTime () && m_liftEntity->v.velocity.z == 0.0 && IsOnFloor ())
         {
            m_pickupItem = button;
            m_pickupType = PICKUP_BUTTON;

            m_navTimeset = GetWorldTime ();
         }
      }

      // is lift activated and bot is standing on it and lift is moving ?
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE || m_liftState == LIFT_ENTERING_IN || m_liftState == LIFT_WAIT_FOR_TEAMMATES || m_liftState == LIFT_WAITING_FOR)
      {
         if (pev->groundentity == m_liftEntity && m_liftEntity->v.velocity.z != 0 && IsOnFloor () && ((waypoint->GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT) || !IsEntityNull (m_targetEntity)))
         {
            m_liftState = LIFT_TRAVELING_BY;
            m_liftUsageTime = GetWorldTime () + 14.0;

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = GetWorldTime ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }
      }

      // bots is currently moving on lift
      if (m_liftState == LIFT_TRAVELING_BY)
      {
         m_destOrigin = Vector (m_liftTravelPos.x, m_liftTravelPos.y, pev->origin.z);

         if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = GetWorldTime ();
            m_aimFlags |= AIM_NAVPOINT;

            ResetCollideState ();
         }
      }

      // need to find a button outside the lift
      if (m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE)
      {

         // button has been pressed, lift should come
         if (m_buttonPushTime + 8.0 >= GetWorldTime ())
         {
            if ((m_prevWptIndex[0] >= 0) && (m_prevWptIndex[0] < g_numWaypoints))
               m_destOrigin = waypoint->GetPath (m_prevWptIndex[0])->origin;
            else
               m_destOrigin = pev->origin;

            if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
            {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = GetWorldTime ();
               m_aimFlags |= AIM_NAVPOINT;

               ResetCollideState ();
            }
         }
         else if (!IsEntityNull(m_liftEntity))
         {
            edict_t *button = FindNearestButton (STRING (m_liftEntity->v.targetname));

            // if we got a valid button entity
            if (!IsEntityNull (button))
            {
               // lift is already used ?
               bool liftUsed = false;

               // iterate though clients, and find if lift already used
               for (int i = 0; i < GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity () || IsEntityNull (g_clients[i].ent->v.groundentity))
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
                     m_destOrigin = waypoint->GetPath (m_prevWptIndex[0])->origin;
                  else
                     m_destOrigin = button->v.origin;

                  if ((pev->origin - m_destOrigin).GetLengthSquared () < 225)
                  {
                     m_moveSpeed = 0.0;
                     m_strafeSpeed = 0.0;
                  }
               }
               else
               {
                  m_pickupItem = button;
                  m_pickupType = PICKUP_BUTTON;
                  m_liftState = LIFT_WAITING_FOR;

                  m_navTimeset = GetWorldTime ();
                  m_liftUsageTime = GetWorldTime () + 20.0;
               }
            }
            else
            {
               m_liftState = LIFT_WAITING_FOR;
               m_liftUsageTime = GetWorldTime () + 15.0;
            }
         }
      }

      // bot is waiting for lift
      if (m_liftState == LIFT_WAITING_FOR)
      {
         if ((m_prevWptIndex[0] >= 0) && (m_prevWptIndex[0] < g_numWaypoints))
         {
            if (!(waypoint->GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT))
               m_destOrigin = waypoint->GetPath (m_prevWptIndex[0])->origin;
            else if ((m_prevWptIndex[1] >= 0) && (m_prevWptIndex[0] < g_numWaypoints))
               m_destOrigin = waypoint->GetPath (m_prevWptIndex[1])->origin;
         }

         if ((pev->origin - m_destOrigin).GetLengthSquared () < 100)
         {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = GetWorldTime ();
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
            if ((waypoint->GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT) && (m_currentPath->origin.z - pev->origin.z) > 50.0 && (waypoint->GetPath (m_prevWptIndex[0])->origin.z - pev->origin.z) > 50.0)
            {
               m_liftState = LIFT_NO_NEARBY;
               m_liftEntity = NULL;
               m_liftUsageTime = 0.0;

               DeleteSearchNodes ();
               FindWaypoint ();

               if (m_prevWptIndex[2] >= 0 && m_prevWptIndex[2] < g_numWaypoints)
                  FindShortestPath (m_currentWaypointIndex, m_prevWptIndex[2]);

               return false;
            }
         }
      }
   }

   if (!IsEntityNull (m_liftEntity) && !(m_currentPath->flags & FLAG_LIFT))
   {
      if (m_liftState == LIFT_TRAVELING_BY)
      {
         m_liftState = LIFT_LEAVING;
         m_liftUsageTime = GetWorldTime () + 10.0;
      }
      if (m_liftState == LIFT_LEAVING && m_liftUsageTime < GetWorldTime () && pev->groundentity != m_liftEntity)
      {
         m_liftState = LIFT_NO_NEARBY;
         m_liftUsageTime = 0.0;

         m_liftEntity = NULL;
      }
   }

   if (m_liftUsageTime < GetWorldTime () && m_liftUsageTime != 0.0)
   {
      m_liftEntity = NULL;
      m_liftState = LIFT_NO_NEARBY;
      m_liftUsageTime = 0.0;

      DeleteSearchNodes ();

      if (m_prevWptIndex[0] >= 0 && m_prevWptIndex[0] < g_numWaypoints)
      {
         if (!(waypoint->GetPath (m_prevWptIndex[0])->flags & FLAG_LIFT))
            ChangeWptIndex (m_prevWptIndex[0]);
         else
            FindWaypoint ();
      }
      else
         FindWaypoint ();

      return false;
   }

   // check if we are going through a door...
   TraceLine (pev->origin, m_waypointOrigin, true, GetEntity (), &tr);

   if (!IsEntityNull (tr.pHit) && IsEntityNull (m_liftEntity) && strncmp (STRING (tr.pHit->v.classname), "func_door", 9) == 0)
   {
      // if the door is near enough...
      if ((GetEntityOrigin (tr.pHit) - pev->origin).GetLengthSquared () < 2500)
      {
         m_lastCollTime = GetWorldTime () + 0.5; // don't consider being stuck

         if (Random.Long (1, 100) < 50)
            MDLL_Use (tr.pHit, GetEntity ()); // also 'use' the door randomly
      }

      // make sure we are always facing the door when going through it
      m_aimFlags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);

      edict_t *button = FindNearestButton (STRING (tr.pHit->v.targetname));

      // check if we got valid button
      if (!IsEntityNull (button))
      {
         m_pickupItem = button;
         m_pickupType = PICKUP_BUTTON;

         m_navTimeset = GetWorldTime ();
      }

      // if bot hits the door, then it opens, so wait a bit to let it open safely
      if (pev->velocity.GetLength2D () < 2 && m_timeDoorOpen < GetWorldTime ())
      {
         PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, GetWorldTime () + 1, false);

         m_doorOpenAttempt++;
         m_timeDoorOpen = GetWorldTime () + 1.0; // retry in 1 sec until door is open

         edict_t *ent = NULL;

         if (m_doorOpenAttempt > 2 && !IsEntityNull (ent = FIND_ENTITY_IN_SPHERE (ent, pev->origin, 256.0f)))
         {
            if (IsValidPlayer (ent) && IsAlive (ent) && m_team != GetTeam (ent) && GetWeaponPenetrationPower (m_currentWeapon) > 0)
            {
               m_seeEnemyTime = GetWorldTime ();

               m_states |= STATE_SUSPECT_ENEMY;
               m_aimFlags |= AIM_LAST_ENEMY;

               m_lastEnemy = ent;
               m_enemy = ent;
               m_lastEnemyOrigin = ent->v.origin;

            }
            else if (IsValidPlayer (ent) && IsAlive (ent) && m_team == GetTeam (ent))
            {
               DeleteSearchNodes ();
               ResetTasks ();
            }
            else if (IsValidPlayer (ent) && (!IsAlive (ent) || (ent->v.deadflag & DEAD_DYING)))
               m_doorOpenAttempt = 0; // reset count
         }
      }
   }

   float desiredDistance = 0.0;

   // initialize the radius for a special waypoint type, where the wpt is considered to be reached
   if (m_currentPath->flags & FLAG_LIFT)
      desiredDistance = 50;
   else if ((pev->flags & FL_DUCKING) || (m_currentPath->flags & FLAG_GOAL))
      desiredDistance = 25;
   else if (m_currentPath->flags & FLAG_LADDER)
      desiredDistance = 15;
   else if (m_currentTravelFlags & PATHFLAG_JUMP)
      desiredDistance = 0.0;
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
   if (desiredDistance < 16.0 && waypointDistance < 30 && (pev->origin + (pev->velocity * m_frameInterval) - m_waypointOrigin).GetLength () > waypointDistance)
      desiredDistance = waypointDistance + 1.0;

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

            if (m_team == TEAM_TF)
            {
               waypointValue = (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team0Value;
               waypointValue += static_cast <int> (pev->health * 0.5);
               waypointValue += static_cast <int> (m_goalValue * 0.5);

               if (waypointValue < -MAX_GOAL_VALUE)
                  waypointValue = -MAX_GOAL_VALUE;
               else if (waypointValue > MAX_GOAL_VALUE)
                  waypointValue = MAX_GOAL_VALUE;

               (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team0Value = waypointValue;
            }
            else
            {
               waypointValue = (g_experienceData + (startIndex * g_numWaypoints) + goalIndex)->team1Value;
               waypointValue += static_cast <int> (pev->health * 0.5);
               waypointValue += static_cast <int> (m_goalValue * 0.5);

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

      if ((g_mapType & MAP_DE) && g_bombPlanted && m_team == TEAM_CF && GetTaskId () != TASK_ESCAPEFROMBOMB && GetTask ()->data != -1)
      {
         Vector bombOrigin = CheckBombAudible ();

         // bot within 'hearable' bomb tick noises?
         if (bombOrigin != nullvec)
         {
            float distance = (bombOrigin - waypoint->GetPath (GetTask ()->data)->origin).GetLength ();

            if (distance > 512.0)
               waypoint->SetGoalVisited (GetTask ()->data); // doesn't hear so not a good goal
         }
         else
            waypoint->SetGoalVisited (GetTask ()->data); // doesn't hear so not a good goal
      }
      HeadTowardWaypoint (); // do the actual movement checking
   }
   return false;
}


void Bot::FindShortestPath (int srcIndex, int destIndex)
{
   // this function finds the shortest path from source index to destination index

   DeleteSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0;

   PathNode *node = new PathNode;

   node->index = srcIndex;
   node->next = NULL;

   m_navNodeStart = node;
   m_navNode = m_navNodeStart;

   while (srcIndex != destIndex)
   {
      srcIndex = *(waypoint->m_pathMatrix + (srcIndex * g_numWaypoints) + destIndex);

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

   inline PriorityQueue (int initialSize = MAX_WAYPOINTS * 0.5)
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
         int parent = (child - 1) * 0.5;

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
      return 0;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team0Damage + g_highestDamageT;

   Path *current = waypoint->GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += (g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team0Damage;
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5;

   return waypoint->GetPathDistance (parentIndex, currentIndex) + cost;
}


float gfunctionKillsDistCT (int currentIndex, int parentIndex)
{
   // least kills and number of nodes to goal for a team

   if (parentIndex == -1)
      return 0;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team1Damage + g_highestDamageCT;

   Path *current = waypoint->GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += static_cast <int> ((g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team1Damage);
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5;

   return waypoint->GetPathDistance (parentIndex, currentIndex) + cost;
}

float gfunctionKillsDistCTWithHostage (int currentIndex, int parentIndex)
{
   // least kills and number of nodes to goal for a team

   Path *current = waypoint->GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionKillsDistCT (currentIndex, parentIndex) * 500;

   return gfunctionKillsDistCT (currentIndex, parentIndex);
}

float gfunctionKillsT (int currentIndex, int)
{
   // least kills to goal for a team

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team0Damage;

   Path *current = waypoint->GetPath (currentIndex);

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
      return 0;

   float cost = (g_experienceData + (currentIndex * g_numWaypoints) + currentIndex)->team1Damage;

   Path *current = waypoint->GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      int neighbour = current->index[i];

      if (neighbour != -1)
         cost += (g_experienceData + (neighbour * g_numWaypoints) + neighbour)->team1Damage;
   }

   if (current->flags & FLAG_CROUCH)
      cost *= 1.5;

   return cost + 0.5f;
}

float gfunctionKillsCTWithHostage (int currentIndex, int parentIndex)
{
   // least kills to goal for a team

   if (parentIndex == -1)
      return 0;

   Path *current = waypoint->GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionKillsDistCT (currentIndex, parentIndex) * 500;

   return gfunctionKillsCT (currentIndex, parentIndex);
}

float gfunctionPathDist (int currentIndex, int parentIndex)
{
   if (parentIndex == -1)
      return 0;

   Path *parent = waypoint->GetPath (parentIndex);
   Path *current = waypoint->GetPath (currentIndex);

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (parent->index[i] == currentIndex)
      {
         // we don't like ladder or crouch point
         if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
            return parent->distances[i] * 1.5;

         return parent->distances[i];
      }
   }
   return 65355;
}

float gfunctionPathDistWithHostage (int currentIndex, int parentIndex)
{
   Path *current = waypoint->GetPath (currentIndex);

   if (current->flags & FLAG_NOHOSTAGE)
      return 65355;

   else if (current->flags & (FLAG_CROUCH | FLAG_LADDER))
      return gfunctionPathDist (currentIndex, parentIndex) * 500;

   return gfunctionPathDist (currentIndex, parentIndex);
}

float hfunctionSquareDist (int index, int, int goalIndex)
{
   // square distance heuristic

   Path *start = waypoint->GetPath (index);
   Path *goal = waypoint->GetPath (goalIndex);

#if 0
   float deltaX = fabsf (start->origin.x - goal->origin.x);
   float deltaY = fabsf (start->origin.y - goal->origin.y);
   float deltaZ = fabsf (start->origin.z - goal->origin.z);

   return static_cast <unsigned int> (deltaX + deltaY + deltaZ);
#else
   float xDist = fabsf (start->origin.x - goal->origin.x);
   float yDist = fabsf (start->origin.y - goal->origin.y);
   float zDist = fabsf (start->origin.z - goal->origin.z);

   if (xDist > yDist)
      return 1.4f * yDist + (xDist - yDist) + zDist;

   return 1.4f * xDist + (yDist - xDist) + zDist;
#endif
}

float hfunctionSquareDistWithHostage (int index, int startIndex, int goalIndex)
{
   // square distance heuristic with hostages

   if (waypoint->GetPath (startIndex)->flags & FLAG_NOHOSTAGE)
      return 65355;

   return hfunctionSquareDist (index, startIndex, goalIndex);
}

float hfunctionNone (int index, int startIndex, int goalIndex)
{
   return hfunctionSquareDist (index, startIndex, goalIndex) / (128 * 10);
}

float hfunctionNumberNodes (int index, int startIndex, int goalIndex)
{
   return hfunctionSquareDist (index, startIndex, goalIndex) / 128 * g_highestKills;
}

void Bot::FindPath (int srcIndex, int destIndex, unsigned char pathType)
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
   m_goalValue = 0.0;

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
      astar[i].g = 0;
      astar[i].f = 0;
      astar[i].parentIndex = -1;
      astar[i].state = NEW;
   }

   float (*gcalc) (int, int) = NULL;
   float (*hcalc) (int, int, int) = NULL;

   switch (pathType)
   {
   case 0:
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

   case 1:
      if (m_team == TEAM_TF)
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

   case 2:
   default:
      if (m_team == TEAM_TF)
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
         int currentChild = waypoint->GetPath (currentIndex)->index[i];

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
      m_currentWaypointIndex = ChangeWptIndex (waypoint->FindNearest (pev->origin));

   int srcIndex = m_currentWaypointIndex;
   int destIndex = waypoint->FindNearest (to);
   int bestIndex = srcIndex;

   while (destIndex != srcIndex)
   {
      destIndex = *(waypoint->m_pathMatrix + (destIndex * g_numWaypoints) + srcIndex);

      if (destIndex < 0)
         break;

      if (waypoint->IsVisible (m_currentWaypointIndex, destIndex))
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
      reachDistances[i] = 9999.0;
   }

   // do main search loop
   for (i = 0; i < g_numWaypoints; i++)
   {
      // ignore current waypoint and previous recent waypoints...
#if 0
      if (i == m_currentWaypointIndex || i == m_prevWptIndex[0] || i == m_prevWptIndex[1] || i == m_prevWptIndex[2] || i == m_prevWptIndex[3] || i == m_prevWptIndex[4])
#else
      if (i == m_currentWaypointIndex || i == m_prevWptIndex[0])
#endif
         continue;

      if ((g_mapType & MAP_CS) && HasHostage () && (waypoint->GetPath (i)->flags & FLAG_NOHOSTAGE))
         continue;

      // ignore non-reacheable waypoints...
      if (!waypoint->Reachable (this, i))
         continue;

      // check if waypoint is already used by another bot...
      if (IsPointOccupied (i))
      {
         coveredWaypoint = i;
         continue;
      }

      // now pick 1-2 random waypoints that near the bot
      float distance = (waypoint->GetPath (i)->origin - pev->origin).GetLengthSquared ();

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
      waypoint->FindInRadius (found, 256.0f, pev->origin);

      if (!found.IsEmpty ())
      {
         bool gotId = false;
         int random = found.GetRandomElement ();;

         while (!found.IsEmpty ())
         {
            int index = found.Pop ();

            if (!waypoint->Reachable (this, index))
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

   m_collideTime = GetWorldTime ();
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
   else if (m_navTimeset + GetEstimatedReachTime () < GetWorldTime () && IsEntityNull (m_enemy))
   {
      if (m_team == TEAM_TF)
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
   m_navTimeset = GetWorldTime ();

   m_currentPath = waypoint->GetPath (m_currentWaypointIndex);
   m_waypointFlags = m_currentPath->flags;

   return m_currentWaypointIndex; // to satisfy static-code analyzers
}

int Bot::ChooseBombWaypoint (void)
{
   // this function finds the best goal (bomb) waypoint for CTs when searching for a planted bomb.

   Array <int> goals = waypoint->m_goalPoints;

   if (goals.IsEmpty ())
      return Random.Long (0, g_numWaypoints - 1); // reliability check

   Vector bombOrigin = CheckBombAudible ();

   // if bomb returns no valid vector, return the current bot pos
   if (bombOrigin == nullvec)
      bombOrigin = pev->origin;

   int goal = 0, count = 0;
   float lastDistance = 99999.0f;

   // find nearest goal waypoint either to bomb (if "heard" or player)
   FOR_EACH_AE (goals, i)
   {
      float distance = (waypoint->GetPath (goals[i])->origin - bombOrigin).GetLengthSquared ();

      // check if we got more close distance
      if (distance < lastDistance)
      {
         goal = goals[i];
         lastDistance = distance;
      }
   }

   while (waypoint->IsGoalVisited (goal))
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

   int posIndex = waypoint->FindNearest (origin);
   int srcIndex = waypoint->FindNearest (pev->origin);

   // some of points not found, return random one
   if (srcIndex == -1 || posIndex == -1)
      return Random.Long (0, g_numWaypoints - 1);

   for (int i = 0; i < g_numWaypoints; i++) // find the best waypoint now
   {
      // exclude ladder & current waypoints
      if ((waypoint->GetPath (i)->flags & FLAG_LADDER) || i == srcIndex || !waypoint->IsVisible (i, posIndex) || IsPointOccupied (i))
         continue;

      // use the 'real' pathfinding distances
      int distances = waypoint->GetPathDistance (srcIndex, i);

      // skip wayponts with distance more than 1024 units
      if (distances > 1248.0f)
         continue;

      TraceLine (waypoint->GetPath (i)->origin, waypoint->GetPath (posIndex)->origin, true, true, GetEntity (), &tr);

      // check if line not hit anything
      if (tr.flFraction != 1.0)
         continue;

      for (int j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (distances > minDistance[j])
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

         if (m_team == TEAM_TF)
            experience = exp->team0Damage;
         else
            experience = exp->team1Damage;

         experience = (experience * 100) / (m_team == TEAM_TF ? g_highestDamageT : g_highestDamageCT);
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
         if ((waypoint->GetPath (i)->origin - origin).GetLength () <= 1248.0f && !IsPointOccupied (i))
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
   return waypointIndex[Random.Long (0, (index - 1) * 0.5)];
}

int Bot::FindCoverWaypoint (float maxDistance)
{
   // this function tries to find a good cover waypoint if bot wants to hide

   // do not move to a position near to the enemy
   if (maxDistance > (m_lastEnemyOrigin - pev->origin).GetLength ())
      maxDistance = (m_lastEnemyOrigin - pev->origin).GetLength ();

   if (maxDistance < 300.0)
      maxDistance = 300.0;

   int srcIndex = m_currentWaypointIndex;
   int enemyIndex = waypoint->FindNearest (m_lastEnemyOrigin);
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
      if (waypoint->GetPath (enemyIndex)->index[i] != -1)
         enemyIndices.Push (waypoint->GetPath (enemyIndex)->index[i]);
   }

   // ensure we're on valid point
   ChangeWptIndex (srcIndex);

   // find the best waypoint now
   for (int i = 0; i < g_numWaypoints; i++)
   {
      // exclude ladder, current waypoints and waypoints seen by the enemy
      if ((waypoint->GetPath (i)->flags & FLAG_LADDER) || i == srcIndex || waypoint->IsVisible (enemyIndex, i))
         continue;

      bool neighbourVisible = false;  // now check neighbour waypoints for visibility

      FOR_EACH_AE (enemyIndices, j)
      {
         if (waypoint->IsVisible (enemyIndices[j], i))
         {
            neighbourVisible = true;
            break;
         }
      }

      // skip visible points
      if (neighbourVisible)
         continue;

      // use the 'real' pathfinding distances
      int distances = waypoint->GetPathDistance (srcIndex, i);
      int enemyDistance = waypoint->GetPathDistance (enemyIndex, i);

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

         if (m_team == TEAM_TF)
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
         TraceLine (m_lastEnemyOrigin + Vector (0, 0, 36), waypoint->GetPath (waypointIndex[i])->origin, true, true, GetEntity (), &tr);

         if (tr.flFraction < 1.0)
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

      if (id != -1 && waypoint->IsConnected (id, m_navNode->next->index) && waypoint->IsConnected (m_currentWaypointIndex, id))
      {
         if (waypoint->GetPath (id)->flags & FLAG_LADDER) // don't use ladder waypoints as alternative
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
         if (GetTaskId () == TASK_NORMAL && g_timeRoundMid + 10.0f < GetWorldTime () && m_timeCamping + 30.0f < GetWorldTime () && !g_bombPlanted && m_personality != PERSONALITY_RUSHER && !m_hasC4 && !m_isVIP && m_loosedBombWptIndex == -1 && !HasHostage ())
         {
            m_campButtons = 0;

            int nextIndex = m_navNode->next->index;
            float kills = 0;

            if (m_team == TEAM_TF)
               kills = (g_experienceData + (nextIndex * g_numWaypoints) + nextIndex)->team0Damage / g_highestDamageT;
            else
               kills = (g_experienceData + (nextIndex * g_numWaypoints) + nextIndex)->team1Damage / g_highestDamageCT;

            // if damage done higher than one
            if (kills > 0.15f && g_timeRoundMid + 15.0f < GetWorldTime ())
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
                  PushTask (TASK_CAMP, TASKPRI_CAMP, -1, GetWorldTime () + Random.Float (m_difficulty * 0.5, m_difficulty) * 5, true);
                  PushTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, FindDefendWaypoint (waypoint->GetPath (nextIndex)->origin), GetWorldTime () + Random.Float (3.0f, 10.0f), true);
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
            float jumpDistance = 0.0;

            Vector src = nullvec;
            Vector destination = nullvec;
            
            // try to find out about future connection flags
            if (m_navNode->next != NULL)
            {
               for (int i = 0; i < MAX_PATH_INDEX; i++)
               {
                  Path *path = waypoint->GetPath (m_navNode->index);
                  Path *next = waypoint->GetPath (m_navNode->next->index);

                  if (path->index[i] == m_navNode->next->index && (path->connectionFlags[i] & PATHFLAG_JUMP))
                  {
                     src = path->origin;
                     destination = next->origin;

                     jumpDistance = (path->origin - next->origin).GetLength ();
                     willJump = true;

                     break;
                  }
               }
            }

            // is there a jump waypoint right ahead and do we need to draw out the light weapon ?
            if (willJump && m_currentWeapon != WEAPON_KNIFE && m_currentWeapon != WEAPON_SCOUT && !m_isReloading && !UsesPistol () && (jumpDistance > 210 || (destination.z + 32.0f > src.z && jumpDistance > 150.0f) || ((destination - src).GetLength2D () < 60 && jumpDistance > 60)) && IsEntityNull (m_enemy))
               SelectWeaponByName ("weapon_knife"); // draw out the knife if we needed

            // bot not already on ladder but will be soon?
            if ((waypoint->GetPath (destIndex)->flags & FLAG_LADDER) && !IsOnLadder ())
            {
               // get ladder waypoints used by other (first moving) bots
               for (int c = 0; c < GetMaxClients (); c++)
               {
                  Bot *otherBot = botMgr->GetBot (c);

                  // if another bot uses this ladder, wait 3 secs
                  if (otherBot != NULL && otherBot != this && IsAlive (otherBot->GetEntity ()) && otherBot->m_currentWaypointIndex == m_navNode->index)
                  {
                     PushTask (TASK_PAUSE, TASKPRI_PAUSE, -1, GetWorldTime () + 3.0, false);
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
      MakeVectors (Vector (pev->angles.x, AngleNormalize (pev->angles.y + Random.Float (-90, 90)), 0.0));
      m_waypointOrigin = m_waypointOrigin + g_pGlobals->v_forward * Random.Float (0, m_currentPath->radius);
   }

   if (IsOnLadder ())
   {
      TraceLine (Vector (pev->origin.x, pev->origin.y, pev->absmin.z), m_waypointOrigin, true, true, GetEntity (), &tr);

      if (tr.flFraction < 1.0)
         m_waypointOrigin = m_waypointOrigin + (pev->origin - m_waypointOrigin) * 0.5 + Vector (0, 0, 32);
   }
   m_navTimeset = GetWorldTime ();

   return true;
}

bool Bot::CantMoveForward (const Vector &normal, TraceResult *tr)
{
   // checks if bot is blocked in his movement direction (excluding doors)

   // use some TraceLines to determine if anything is blocking the current path of the bot.

   // first do a trace from the bot's eyes forward...
   Vector src = EyePosition ();
   Vector forward = src + normal * 24;

   MakeVectors (Vector (0, pev->angles.y, 0));

   // trace from the bot's eyes straight forward...
   TraceLine (src, forward, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
   {
      if (strncmp ("func_door", STRING (tr->pHit->v.classname), 9) == 0)
         return false;

      return true;  // bot's head will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder left diagonal forward to the right shoulder...
   src = EyePosition () + Vector (0, 0, -16) - g_pGlobals->v_right * -16;
   forward = EyePosition () + Vector (0, 0, -16) + g_pGlobals->v_right * 16 + normal * 24;

   TraceLine (src, forward, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
      return true;  // bot's body will hit something

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder right diagonal forward to the left shoulder...
   src = EyePosition () + Vector (0, 0, -16) + g_pGlobals->v_right * 16;
   forward = EyePosition () + Vector (0, 0, -16) - g_pGlobals->v_right * -16 + normal * 24;

   TraceLine (src, forward, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
      return true;  // bot's body will hit something

   // now check below waist
   if (pev->flags & FL_DUCKING)
   {
      src = pev->origin + Vector (0, 0, -19 + 19);
      forward = src + Vector (0, 0, 10) + normal * 24;

      TraceLine (src, forward, true, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something

      src = pev->origin;
      forward = src + normal * 24;

      TraceLine (src, forward, true, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something
   }
   else
   {
      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0, 0, -17) - g_pGlobals->v_right * -16;
      forward = pev->origin + Vector (0, 0, -17) + g_pGlobals->v_right * 16 + normal * 24;

      // trace from the bot's waist straight forward...
      TraceLine (src, forward, true, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
         return true;  // bot's body will hit something

      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0, 0, -17) + g_pGlobals->v_right * 16;
      forward = pev->origin + Vector (0, 0, -17) - g_pGlobals->v_right * -16 + normal * 24;

      TraceLine (src, forward, true, GetEntity (), tr);

      // check if the trace hit something...
      if (tr->flFraction < 1.0 && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0)
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
   Vector left = src - g_pGlobals->v_right * -40;

   // trace from the bot's waist straight left...
   TraceLine (src, left, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
      return false;  // bot's body will hit something

   src = left;
   left = left + g_pGlobals->v_forward * 40;

   // trace from the strafe pos straight forward...
   TraceLine (src, left, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
      return false;  // bot's body will hit something

   return true;
}

bool Bot::CanStrafeRight (TraceResult * tr)
{
   // this function checks if bot can move sideways

   MakeVectors (pev->v_angle);

   Vector src = pev->origin;
   Vector right = src + g_pGlobals->v_right * 40;

   // trace from the bot's waist straight right...
   TraceLine (src, right, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
      return false;  // bot's body will hit something

   src = right;
   right = right + g_pGlobals->v_forward * 40;

   // trace from the strafe pos straight forward...
   TraceLine (src, right, true, GetEntity (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0)
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
   Vector jump = pev->angles;
   jump.x = 0; // reset pitch to 0 (level horizontally)
   jump.z = 0; // reset roll to 0 (straight up and down)

   MakeVectors (jump);

   // check for normal jump height first...
   Vector src = pev->origin + Vector (0, 0, -36 + 45);
   Vector dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   if (tr.flFraction < 1.0)
      goto CheckDuckJump;
   else
   {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37;

      TraceLine (src, dest, true, GetEntity (), &tr);

      if (tr.flFraction < 1.0)
         return false;
   }

   // now check same height to one side of the bot...
   src = pev->origin + g_pGlobals->v_right * 16 + Vector (0, 0, -36 + 45);
   dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      goto CheckDuckJump;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37;

   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now check same height on the other side of the bot...
   src = pev->origin + (-g_pGlobals->v_right * 16) + Vector (0, 0, -36 + 45);
   dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      goto CheckDuckJump;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37;

   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0;

   // here we check if a duck jump would work...
CheckDuckJump:

   // use center of the body first... maximum duck jump height is 62, so check one unit above that (63)
   src = pev->origin + Vector (0, 0, -36 + 63);
   dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   if (tr.flFraction < 1.0)
      return false;
   else
   {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37;

      TraceLine (src, dest, true, GetEntity (), &tr);

      // if trace hit something, check duckjump
      if (tr.flFraction < 1.0)
         return false;
   }

   // now check same height to one side of the bot...
   src = pev->origin + g_pGlobals->v_right * 16 + Vector (0, 0, -36 + 63);
   dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37;

   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now check same height on the other side of the bot...
   src = pev->origin + (-g_pGlobals->v_right * 16) + Vector (0, 0, -36 + 63);
   dest = src + normal * 32;

   // trace a line forward at maximum jump height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37;

   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0;
}

bool Bot::CanDuckUnder (const Vector &normal)
{
   // this function check if bot can duck under obstacle

   TraceResult tr;
   Vector baseHeight;

   // convert current view angle to vectors for TraceLine math...
   Vector duck = pev->angles;
   duck.x = 0; // reset pitch to 0 (level horizontally)
   duck.z = 0; // reset roll to 0 (straight up and down)

   MakeVectors (duck);

   // use center of the body first...
   if (pev->flags & FL_DUCKING)
      baseHeight = pev->origin + Vector (0, 0, -17);
   else
      baseHeight = pev->origin;

   Vector src = baseHeight;
   Vector dest = src + normal * 32;

   // trace a line forward at duck height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now check same height to one side of the bot...
   src = baseHeight + g_pGlobals->v_right * 16;
   dest = src + normal * 32;

   // trace a line forward at duck height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0)
      return false;

   // now check same height on the other side of the bot...
   src = baseHeight + (-g_pGlobals->v_right * 16);
   dest = src + normal * 32;

   // trace a line forward at duck height...
   TraceLine (src, dest, true, GetEntity (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0;
}

#ifdef DEAD_CODE

bool Bot::IsBlockedLeft (void)
{
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0)
      direction = -48;

   MakeVectors (pev->angles);

   // do a trace to the left...
   TraceLine (pev->origin, g_pGlobals->v_forward * direction - g_pGlobals->v_right * 48, true, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0 && strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0)
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
   TraceLine (pev->origin, pev->origin + g_pGlobals->v_forward * direction + g_pGlobals->v_right * 48, true, GetEntity (), &tr);

   // check if the trace hit something...
   if ((tr.flFraction < 1.0) && (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0))
      return true; // bot's body will hit something

   return false;
}

#endif

bool Bot::CheckWallOnLeft (void)
{
   TraceResult tr;
   MakeVectors (pev->angles);

   TraceLine (pev->origin, pev->origin - g_pGlobals->v_right * 40, true, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0)
      return true;

   return false;
}

bool Bot::CheckWallOnRight (void)
{
   TraceResult tr;
   MakeVectors (pev->angles);

   // do a trace to the right...
   TraceLine (pev->origin, pev->origin + g_pGlobals->v_right * 40, true, GetEntity (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0)
      return true;

   return false;
}

bool Bot::IsDeadlyDrop (const Vector &to)
{
   // this function eturns if given location would hurt Bot with falling damage

   Vector botPos = pev->origin;
   TraceResult tr;

   Vector move ((to - botPos).ToYaw (), 0, 0);
   MakeVectors (move);

   Vector direction = (to - botPos).Normalize ();  // 1 unit long
   Vector check = botPos;
   Vector down = botPos;

   down.z = down.z - 1000.0;  // straight down 1000 units

   TraceHull (check, down, true, head_hull, GetEntity (), &tr);

   if (tr.flFraction > 0.036) // We're not on ground anymore?
      tr.flFraction = 0.036;

   float lastHeight = tr.flFraction * 1000.0;  // height from ground

   float distance = (to - check).GetLength ();  // distance from goal

   while (distance > 16.0)
   {
      check = check + direction * 16.0; // move 10 units closer to the goal...

      down = check;
      down.z = down.z - 1000.0;  // straight down 1000 units

      TraceHull (check, down, true, head_hull, GetEntity (), &tr);

      if (tr.fStartSolid) // Wall blocking?
         return false;

      float height = tr.flFraction * 1000.0;  // height from ground

      if (lastHeight < height - 100) // Drops more than 100 Units?
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

   if (yb_aim_method.GetInt () == 1)
   {
      // turn to the ideal angle immediately
      pev->v_angle.x = idealPitch;
      pev->angles.x = -idealPitch / 3;

      return;
   }

   float curent = AngleNormalize (pev->v_angle.x);

   // turn from the current v_angle pitch to the idealpitch by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0)
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

   if (pev->v_angle.x > 89.9)
      pev->v_angle.x = 89.9;

   if (pev->v_angle.x < -89.9)
      pev->v_angle.x = -89.9;

   pev->angles.x = -pev->v_angle.x / 3;
}

void Bot::ChangeYaw (float speed)
{
   // this function turns a bot towards its ideal_yaw

   float idealPitch = AngleNormalize (pev->ideal_yaw);

   if (yb_aim_method.GetInt () == 1)
   {
      // turn to the ideal angle immediately
      pev->angles.y = (pev->v_angle.y = idealPitch);
      return;
   }

   float curent = AngleNormalize (pev->v_angle.y);

   // turn from the current v_angle yaw to the ideal_yaw by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0)
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
      if (currentWaypoint == i || !waypoint->IsVisible (currentWaypoint, i))
         continue;

      Path *path = waypoint->GetPath (i);

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

void Bot::UpdateLookAngles (void)
{
   // adjust all body and view angles to face an absolute vector
   Vector direction = (m_lookAt - EyePosition ()).ToAngles ();
   direction = direction + pev->punchangle * (m_difficulty * 25) / 100.0;
   direction.x *= -1.0f; // invert for engine

   Vector deviation = (direction - pev->v_angle);

   direction.ClampAngles ();
   deviation.ClampAngles ();

   int aimMethod = yb_aim_method.GetInt ();

   if (aimMethod < 1 || aimMethod > 3)
      aimMethod = 3;

   if (aimMethod == 1)
      pev->v_angle = direction;
   else if (aimMethod == 2)
   {
      float turnSkill = static_cast <float> (0.05 * (m_difficulty * 25)) + 0.5;
      float aimSpeed = 0.17 + turnSkill * 0.06;
      float frameCompensation = g_pGlobals->frametime * 1000 * 0.01;

      if ((m_aimFlags & AIM_ENEMY) && !(m_aimFlags & AIM_ENTITY))
         aimSpeed *= 1.75;

      float momentum = (1.0 - aimSpeed) * 0.5;

      pev->pitch_speed = ((pev->pitch_speed * momentum) + aimSpeed * deviation.x * (1.0 - momentum)) * frameCompensation;
      pev->yaw_speed = ((pev->yaw_speed * momentum) + aimSpeed * deviation.y * (1.0 - momentum)) * frameCompensation;

      if (m_difficulty <= 2)
      {
         // influence of y movement on x axis, based on skill (less influence than x on y since it's
         // easier and more natural for the bot to "move its mouse" horizontally than vertically)
         pev->pitch_speed += pev->yaw_speed / (10.0 * turnSkill);
         pev->yaw_speed += pev->pitch_speed / (10.0 * turnSkill);
      }

      pev->v_angle.x += pev->pitch_speed; // change pitch angles
      pev->v_angle.y += pev->yaw_speed; // change yaw angles
   }
   else if (aimMethod == 3)
   {
#if defined (NDEBUG)
      Vector springStiffness (13.0f, 13.0f, 0.0f);
      Vector damperCoefficient (0.22f, 0.22f, 0.0f);

      Vector influence (0.25f, 0.17f, 0.0f);
      Vector randomization (2.0f, 0.18f, 0.0f);

      const float noTargetRatio = 0.3f;
      const float offsetDelay = 1.2f;
      const float targetRatio = 0.8f;
#else
      Vector springStiffness (yb_aim_spring_stiffness_x.GetFloat (), yb_aim_spring_stiffness_y.GetFloat (), 0);
      Vector damperCoefficient (yb_aim_damper_coefficient_x.GetFloat (), yb_aim_damper_coefficient_y.GetFloat (), 0);
      Vector influence (yb_aim_influence_x_on_y.GetFloat (), yb_aim_influence_y_on_x.GetFloat (), 0);
      Vector randomization (yb_aim_deviation_x.GetFloat (), yb_aim_deviation_y.GetFloat (), 0);

      const float noTargetRatio = yb_aim_notarget_slowdown_ratio.GetFloat ();
      const float offsetDelay = yb_aim_offset_delay.GetFloat ();
      const float targetRatio = yb_aim_target_anticipation_ratio.GetFloat ();
#endif

      Vector stiffness = nullvec;
      Vector randomize = nullvec;

      m_idealAngles = direction.Get2D ();
      m_targetOriginAngularSpeed.ClampAngles ();
      m_idealAngles.ClampAngles ();

      if (m_difficulty == 4)
      {
         influence = influence * ((100 - (m_difficulty * 25)) / 100.f);
         randomization = randomization * ((100 - (m_difficulty * 25)) / 100.f);
      }

      if (m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE | AIM_LAST_ENEMY) || GetTaskId () == TASK_SHOOTBREAKABLE)
      {
         m_playerTargetTime = GetWorldTime ();
         m_randomizedIdealAngles = m_idealAngles;

         if (IsValidPlayer (m_enemy))
         {
            m_targetOriginAngularSpeed = ((m_enemyOrigin - pev->origin + 1.5 * m_frameInterval * (1.0 * m_enemy->v.velocity) - 0.0 * g_pGlobals->frametime * pev->velocity).ToAngles () - (m_enemyOrigin - pev->origin).ToAngles ()) * 0.45f * targetRatio * static_cast <float> ((m_difficulty * 25) / 100);

            if (m_angularDeviation.GetLength () < 5.0)
               springStiffness = (5.0 - m_angularDeviation.GetLength ()) * 0.25 * static_cast <float> ((m_difficulty * 25) / 100) * springStiffness + springStiffness;

            m_targetOriginAngularSpeed.x = -m_targetOriginAngularSpeed.x;

            if ((pev->fov < 90) && (m_angularDeviation.GetLength () >= 5.0))
               springStiffness = springStiffness * 2;

            m_targetOriginAngularSpeed.ClampAngles ();
         }
         else
            m_targetOriginAngularSpeed = nullvec;


         if (m_difficulty >= 3)
            stiffness = springStiffness;
         else
            stiffness = springStiffness * (0.2 + (m_difficulty * 25) / 125.0);
      }
      else
      {
         // is it time for bot to randomize the aim direction again (more often where moving) ?
         if (m_randomizeAnglesTime < GetWorldTime () && ((pev->velocity.GetLength () > 1.0 && m_angularDeviation.GetLength () < 5.0) || m_angularDeviation.GetLength () < 1.0))
         {
            // is the bot standing still ?
            if (pev->velocity.GetLength () < 1.0)
               randomize = randomization * 0.2; // randomize less
            else
               randomize = randomization;

            // randomize targeted location a bit (slightly towards the ground)
            m_randomizedIdealAngles = m_idealAngles + Vector (Random.Float (-randomize.x * 0.5, randomize.x * 1.5), Random.Float (-randomize.y, randomize.y), 0);

            // set next time to do this
            m_randomizeAnglesTime = GetWorldTime () + Random.Float (0.4f, offsetDelay);
         }
         float stiffnessMultiplier = noTargetRatio;

         // take in account whether the bot was targeting someone in the last N seconds
         if (GetWorldTime () - (m_playerTargetTime + offsetDelay) < noTargetRatio * 10.0)
         {
            stiffnessMultiplier = 1.0 - (GetWorldTime () - m_timeLastFired) * 0.1;

            // don't allow that stiffness multiplier less than zero
            if (stiffnessMultiplier < 0.0)
               stiffnessMultiplier = 0.5;
         }

         // also take in account the remaining deviation (slow down the aiming in the last 10°)
         if (m_difficulty < 3 && (m_angularDeviation.GetLength () < 10.0))
            stiffnessMultiplier *= m_angularDeviation.GetLength () * 0.1;

         // slow down even more if we are not moving
         if (m_difficulty < 3 && pev->velocity.GetLength () < 1.0 && GetTaskId () != TASK_CAMP && GetTaskId () != TASK_ATTACK)
            stiffnessMultiplier *= 0.5;

         // but don't allow getting below a certain value
         if (stiffnessMultiplier < 0.35)
            stiffnessMultiplier = 0.35;

         stiffness = springStiffness * stiffnessMultiplier; // increasingly slow aim

         // no target means no angular speed to take in account
         m_targetOriginAngularSpeed = nullvec;
      }
      // compute randomized angle deviation this time
      m_angularDeviation = m_randomizedIdealAngles + m_targetOriginAngularSpeed - pev->v_angle;
      m_angularDeviation.ClampAngles ();

      // spring/damper model aiming
      m_aimSpeed.x = (stiffness.x * m_angularDeviation.x) - (damperCoefficient.x * m_aimSpeed.x);
      m_aimSpeed.y = (stiffness.y * m_angularDeviation.y) - (damperCoefficient.y * m_aimSpeed.y);

      // influence of y movement on x axis and vice versa (less influence than x on y since it's
      // easier and more natural for the bot to "move its mouse" horizontally than vertically)
      m_aimSpeed.x += m_aimSpeed.y * influence.y;
      m_aimSpeed.y += m_aimSpeed.x * influence.x;

      // move the aim cursor
      if (m_difficulty == 4 && (m_aimFlags & AIM_ENEMY) && (m_wantsToFire || UsesSniper ()))
         pev->v_angle = direction;
      else
         pev->v_angle = pev->v_angle + m_frameInterval * Vector (m_aimSpeed.x, m_aimSpeed.y, 0.0f);

      pev->v_angle.ClampAngles ();
   }

   // set the body angles to point the gun correctly
   pev->angles.x = -pev->v_angle.x * (1.0 / 3.0);
   pev->angles.y = pev->v_angle.y;

   pev->v_angle.ClampAngles ();
   pev->angles.ClampAngles ();

   pev->angles.z = pev->v_angle.z = 0.0; // ignore Z component
}

void Bot::SetStrafeSpeed (const Vector &moveDir, float strafeSpeed)
{
   MakeVectors (pev->angles);

   const Vector &los = (moveDir - pev->origin).Normalize2D ();
   float dot = los | g_pGlobals->v_forward.Get2D ();

   if (dot > 0 && !CheckWallOnRight ())
      m_strafeSpeed = strafeSpeed;
   else if (!CheckWallOnLeft ())
      m_strafeSpeed = -strafeSpeed;
}

int Bot::FindPlantedBomb (void)
{
   // this function tries to find planted c4 on the defuse scenario map and returns nearest to it waypoint

   if (m_team != TEAM_TF || !(g_mapType & MAP_DE))
      return -1; // don't search for bomb if the player is CT, or it's not defusing bomb

   edict_t *bombEntity = NULL; // temporaly pointer to bomb

   // search the bomb on the map
   while (!IsEntityNull (bombEntity = FIND_ENTITY_BY_CLASSNAME (bombEntity, "grenade")))
   {
      if (strcmp (STRING (bombEntity->v.model) + 9, "c4.mdl") == 0)
      {
         int nearestIndex = waypoint->FindNearest (GetEntityOrigin (bombEntity));

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
   for (int i = 0; i < GetMaxClients (); i++)
   {
      Bot *bot = botMgr->GetBot (i);

      if (bot == NULL || bot == this)
         continue;

      // check if this waypoint is already used
      if (bot->m_notKilled)
      {
         int occupyId = GetShootingConeDeviation (bot->GetEntity (), &pev->origin) >= 0.7f ? bot->m_prevWptIndex[0] : m_currentWaypointIndex;

         if (occupyId == index || bot->GetTask ()->data == index || (waypoint->GetPath (occupyId)->origin - waypoint->GetPath (index)->origin).GetLengthSquared () < 4096.0f)
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
   while (!IsEntityNull(searchEntity = FIND_ENTITY_BY_TARGET (searchEntity, targetName)))
   {
      Vector entityOrign = GetEntityOrigin (searchEntity);

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