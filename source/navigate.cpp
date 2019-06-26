//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_whose_your_daddy ("yb_whose_your_daddy", "0");
ConVar yb_debug_heuristic_type ("yb_debug_heuristic_type", "4");

int Bot::searchGoal (void) {

   // chooses a destination (goal) waypoint for a bot
   if (!bots.isBombPlanted () && m_team == TEAM_TERRORIST && game.mapIs (MAP_DE)) {
      edict_t *pent = nullptr;

      while (!game.isNullEntity (pent = engfuncs.pfnFindEntityByString (pent, "classname", "weaponbox"))) {
         if (strcmp (STRING (pent->v.model), "models/w_backpack.mdl") == 0) {
            int index = waypoints.getNearest (game.getAbsPos (pent));

            if (waypoints.exists (index)) {
               return m_loosedBombWptIndex = index;
            }
            break;
         }
      }

      // forcing terrorist bot to not move to another bomb spot
      if (m_inBombZone && !m_hasProgressBar && m_hasC4) {
         return waypoints.getNearest (pev->origin, 768.0f, FLAG_GOAL);
      }
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

   IntArray *offensiveWpts = nullptr;
   IntArray *defensiveWpts = nullptr;

   switch (m_team) {
   case TEAM_TERRORIST:
      offensiveWpts = &waypoints.m_ctPoints;
      defensiveWpts = &waypoints.m_terrorPoints;
      break;

   case TEAM_COUNTER:
   default:
      offensiveWpts = &waypoints.m_terrorPoints;
      defensiveWpts = &waypoints.m_ctPoints;
      break;
   }

   // terrorist carrying the C4?
   if (m_hasC4 || m_isVIP) {
      tactic = 3;
      return getGoalProcess (tactic, defensiveWpts, offensiveWpts);
   }
   else if (m_team == TEAM_COUNTER && hasHostage ()) {
      tactic = 2;
      offensiveWpts = &waypoints.m_rescuePoints;

      return getGoalProcess (tactic, defensiveWpts, offensiveWpts);
   }

   offensive = m_agressionLevel * 100.0f;
   defensive = m_fearLevel * 100.0f;

   if (game.mapIs (MAP_AS | MAP_CS)) {
      if (m_team == TEAM_TERRORIST) {
         defensive += 25.0f;
         offensive -= 25.0f;
      }
      else if (m_team == TEAM_COUNTER) {
         // on hostage maps force more bots to save hostages
         if (game.mapIs (MAP_CS)) {
            defensive -= 25.0f - m_difficulty * 0.5f;
            offensive += 25.0f + m_difficulty * 5.0f;
         }
         else {
            defensive -= 25.0f;
            offensive += 25.0f;
         }
      }
   }
   else if (game.mapIs (MAP_DE) && m_team == TEAM_COUNTER) {
      if (bots.isBombPlanted () && taskId () != TASK_ESCAPEFROMBOMB && !waypoints.getBombPos ().empty ()) {

         if (bots.hasBombSay (BSS_NEED_TO_FIND_CHAT)) {
            pushChatMessage (CHAT_BOMBPLANT);
            bots.clearBombSay (BSS_NEED_TO_FIND_CHAT);
         }
         return m_chosenGoalIndex = getBombPoint ();
      }
      defensive += 25.0f + m_difficulty * 4.0f;
      offensive -= 25.0f - m_difficulty * 0.5f;

      if (m_personality != PERSONALITY_RUSHER) {
         defensive += 10.0f;
      }
   }
   else if (game.mapIs (MAP_DE) && m_team == TEAM_TERRORIST && bots.getRoundStartTime () + 10.0f < game.timebase ()) {
      // send some terrorists to guard planted bomb
      if (!m_defendedBomb && bots.isBombPlanted () && taskId () != TASK_ESCAPEFROMBOMB && getBombTimeleft () >= 15.0) {
         return m_chosenGoalIndex = getDefendPoint (waypoints.getBombPos ());
      }
   }

   goalDesire = rng.getFloat (0.0f, 100.0f) + offensive;
   forwardDesire = rng.getFloat (0.0f, 100.0f) + offensive;
   campDesire = rng.getFloat (0.0f, 100.0f) + defensive;
   backoffDesire = rng.getFloat (0.0f, 100.0f) + defensive;

   if (!usesCampGun ()) {
      campDesire *= 0.5f;
   }

   tacticChoice = backoffDesire;
   tactic = 0;

   if (campDesire > tacticChoice) {
      tacticChoice = campDesire;
      tactic = 1;
   }

   if (forwardDesire > tacticChoice) {
      tacticChoice = forwardDesire;
      tactic = 2;
   }

   if (goalDesire > tacticChoice) {
      tactic = 3;
   }
   return getGoalProcess (tactic, defensiveWpts, offensiveWpts);
}

int Bot::getGoalProcess (int tactic, IntArray *defensive, IntArray *offsensive) {
   int goalChoices[4] = { INVALID_WAYPOINT_INDEX, INVALID_WAYPOINT_INDEX, INVALID_WAYPOINT_INDEX, INVALID_WAYPOINT_INDEX };

   if (tactic == 0 && !(*defensive).empty ()) { // careful goal
      filterGoals (*defensive, goalChoices);
   }
   else if (tactic == 1 && !waypoints.m_campPoints.empty ()) // camp waypoint goal
   {
      // pickup sniper points if possible for sniping bots
      if (!waypoints.m_sniperPoints.empty () && usesSniper ()) {
         filterGoals (waypoints.m_sniperPoints, goalChoices);
      }
      else {
         filterGoals (waypoints.m_campPoints, goalChoices);
      }
   }
   else if (tactic == 2 && !(*offsensive).empty ()) { // offensive goal
      filterGoals (*offsensive, goalChoices);
   }
   else if (tactic == 3 && !waypoints.m_goalPoints.empty ()) // map goal waypoint
   {
      // force bomber to select closest goal, if round-start goal was reset by something
      if (m_hasC4 && bots.getRoundStartTime () + 20.0f < game.timebase ()) {
         float minDist = 9999999.0f;
         int count = 0;

         for (auto &point : waypoints.m_goalPoints) {
            float distance = (waypoints[point].origin - pev->origin).lengthSq ();

            if (distance > cr::square (1024.0f)) {
               continue;
            }
            if (distance < minDist) {
               goalChoices[count] = point;

               if (++count > 3) {
                  count = 0;
               }
               minDist = distance;
            }
         }

         for (int i = 0; i < 4; i++) {
            if (goalChoices[i] == INVALID_WAYPOINT_INDEX) {
               goalChoices[i] = waypoints.m_goalPoints.random ();
            }
         }
      }
      else {
         filterGoals (waypoints.m_goalPoints, goalChoices);
      }
   }

   if (!waypoints.exists (m_currentWaypointIndex)) {
      m_currentWaypointIndex = changePointIndex (getNearestPoint ());
   }

   if (goalChoices[0] == INVALID_WAYPOINT_INDEX) {
      return m_chosenGoalIndex = rng.getInt (0, waypoints.length () - 1);
   }
   bool sorting = false;

   do {
      sorting = false;

      for (int i = 0; i < 3; i++) {
         int testIndex = goalChoices[i + 1];

         if (testIndex < 0) {
            break;
         }

         if (waypoints.getDangerValue (m_team, m_currentWaypointIndex, goalChoices[i]) < waypoints.getDangerValue (m_team, m_currentWaypointIndex, goalChoices[i + 1])) {
            goalChoices[i + 1] = goalChoices[i];
            goalChoices[i] = testIndex;

            sorting = true;
         }
      }
   } while (sorting);

   return m_chosenGoalIndex = goalChoices[0]; // return and store goal
}

void Bot::filterGoals (const IntArray &goals, int *result) {
   // this function filters the goals, so new goal is not bot's old goal, and array of goals doesn't contains duplicate goals

   int searchCount = 0;

   for (int index = 0; index < 4; index++) {
      int rand = goals.random ();

      if (searchCount <= 8 && (m_prevGoalIndex == rand || ((result[0] == rand || result[1] == rand || result[2] == rand || result[3] == rand) && goals.length () > 4)) && !isOccupiedPoint (rand)) {
         if (index > 0) {
            index--;
         }
         searchCount++;
         continue;
      }
      result[index] = rand;
   }
}

bool Bot::hasActiveGoal (void) {
   int goal = task ()->data;

   if (goal == INVALID_WAYPOINT_INDEX) { // not decided about a goal
      return false;
   }
   else if (goal == m_currentWaypointIndex) { // no nodes needed
      return true;
   }
   else if (m_path.empty ()) { // no path calculated
      return false;
   }
   return goal == m_path.back (); // got path - check if still valid
}

void Bot::resetCollision (void) {
   m_collideTime = 0.0f;
   m_probeTime = 0.0f;

   m_collisionProbeBits = 0;
   m_collisionState = COLLISION_NOTDECICED;
   m_collStateIndex = 0;

   for (int i = 0; i < MAX_COLLIDE_MOVES; i++) {
      m_collideMoves[i] = 0;
   }
}

void Bot::ignoreCollision (void) {
   resetCollision ();

   m_prevTime = game.timebase () + 1.2f;
   m_lastCollTime = game.timebase () + 1.5f;
   m_isStuck = false;
   m_checkTerrain = false;
   m_prevSpeed = m_moveSpeed;
   m_prevOrigin = pev->origin;
}

void Bot::avoidIncomingPlayers (edict_t *touch) {
   auto task = taskId ();

   if (task == TASK_PLANTBOMB || task == TASK_DEFUSEBOMB || task == TASK_CAMP || m_moveSpeed <= 100.0f) {
      return;
   }

   int ownId = game.indexOfEntity (ent ());
   int otherId = game.indexOfEntity (touch);

   if (ownId < otherId) {
      return;
   }
   
   if (m_avoid) {
      int currentId = game.indexOfEntity (m_avoid);

      if (currentId < otherId) {
         return;
      }
   }
   m_avoid = touch;
   m_avoidTime = game.timebase () + 0.33f + calcThinkInterval ();
}

bool Bot::doPlayerAvoidance (const Vector &normal) {
   // avoid collision entity, got it form official csbot

   if (m_avoidTime > game.timebase () && util.isAlive (m_avoid)) {

      Vector dir (cr::cosf (pev->v_angle.y), cr::sinf (pev->v_angle.y), 0.0f);
      Vector lat (-dir.y, dir.x, 0.0f);
      Vector to = Vector (m_avoid->v.origin.x - pev->origin.x, m_avoid->v.origin.y - pev->origin.y, 0.0f).normalize ();

      float toProj = to.x * dir.x + to.y * dir.y;
      float latProj = to.x * lat.x + to.y * lat.y;

      const float c = 0.5f;

      if (toProj > c) {
         m_moveSpeed = -pev->maxspeed;
         return true;
      }
      else if (toProj < -c) {
         m_moveSpeed = pev->maxspeed;
         return true;
      }

      if (latProj >= c) {
         pev->button |= IN_MOVELEFT;
         setStrafeSpeed (normal, pev->maxspeed);
         return true;
      }
      else if (latProj <= -c) {
         pev->button |= IN_MOVERIGHT;
         setStrafeSpeed (normal, -pev->maxspeed);
         return true;
      }
      return false;
   }
   else {
      m_avoid = nullptr;
   }
   return false;
}

void Bot::checkTerrain (float movedDistance, const Vector &dirNormal) {
   m_isStuck = false;
   TraceResult tr;

   // Standing still, no need to check?
   // FIXME: doesn't care for ladder movement (handled separately) should be included in some way
   if ((m_moveSpeed >= 10.0f || m_strafeSpeed >= 10.0f) && m_lastCollTime < game.timebase () && m_seeEnemyTime + 0.8f < game.timebase () && taskId () != TASK_ATTACK) {

      // didn't we move enough previously?
      if (movedDistance < 2.0f && m_prevSpeed >= 20.0f) {
         m_prevTime = game.timebase (); // then consider being stuck
         m_isStuck = true;

         if (cr::fzero (m_firstCollideTime)) {
            m_firstCollideTime = game.timebase () + 0.2f;
         }
      }
      // not stuck yet
      else {
         // test if there's something ahead blocking the way
         if (cantMoveForward (dirNormal, &tr) && !isOnLadder ()) {
            if (m_firstCollideTime == 0.0f) {
               m_firstCollideTime = game.timebase () + 0.2f;
            }
            else if (m_firstCollideTime <= game.timebase ()) {
               m_isStuck = true;
            }
         }
         else {
            m_firstCollideTime = 0.0f;
         }
      }

      // not stuck?
      if (!m_isStuck) {
         if (m_probeTime + 0.5f < game.timebase ()) {
            resetCollision (); // reset collision memory if not being stuck for 0.5 secs
         }
         else {
            // remember to keep pressing duck if it was necessary ago
            if (m_collideMoves[m_collStateIndex] == COLLISION_DUCK && (isOnFloor () || isInWater ())) {
               pev->button |= IN_DUCK;
            }
         }
         return;
      }
      // bot is stuck!

      Vector src;
      Vector dst;

      // not yet decided what to do?
      if (m_collisionState == COLLISION_NOTDECICED) {
         int bits = 0;

         if (isOnLadder ()) {
            bits |= PROBE_STRAFE;
         }
         else if (isInWater ()) {
            bits |= (PROBE_JUMP | PROBE_STRAFE);
         }
         else {
            bits |= (PROBE_STRAFE | (m_jumpStateTimer < game.timebase () ? PROBE_JUMP : 0));
         }

         // collision check allowed if not flying through the air
         if (isOnFloor () || isOnLadder () || isInWater ()) {
            int state[MAX_COLLIDE_MOVES * 2 + 1];
            int i = 0;

            // first 4 entries hold the possible collision states
            state[i++] = COLLISION_STRAFELEFT;
            state[i++] = COLLISION_STRAFERIGHT;
            state[i++] = COLLISION_JUMP;
            state[i++] = COLLISION_DUCK;

            if (bits & PROBE_STRAFE) {
               state[i] = 0;
               state[i + 1] = 0;

               // to start strafing, we have to first figure out if the target is on the left side or right side
               game.makeVectors (m_moveAngles);

               Vector dirToPoint = (pev->origin - m_destOrigin).normalize2D ();
               Vector rightSide = game.vec.right.normalize2D ();

               bool dirRight = false;
               bool dirLeft = false;
               bool blockedLeft = false;
               bool blockedRight = false;

               if ((dirToPoint | rightSide) > 0.0f) {
                  dirRight = true;
               }
               else {
                  dirLeft = true;
               }
               const Vector &testDir = m_moveSpeed > 0.0f ? game.vec.forward : -game.vec.forward;

               // now check which side is blocked
               src = pev->origin + game.vec.right * 32.0f;
               dst = src + testDir * 32.0f;

               game.testHull (src, dst, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

               if (tr.flFraction != 1.0f)
                  blockedRight = true;

               src = pev->origin - game.vec.right * 32.0f;
               dst = src + testDir * 32.0f;

               game.testHull (src, dst, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

               if (tr.flFraction != 1.0f) {
                  blockedLeft = true;
               }

               if (dirLeft) {
                  state[i] += 5;
               }
               else {
                  state[i] -= 5;
               }

               if (blockedLeft) {
                  state[i] -= 5;
               }
               i++;

               if (dirRight) {
                  state[i] += 5;
               }
               else {
                  state[i] -= 5;
               }

               if (blockedRight) {
                  state[i] -= 5;
               }
            }

            // now weight all possible states
            if (bits & PROBE_JUMP) {
               state[i] = 0;

               if (canJumpUp (dirNormal)) {
                  state[i] += 10;
               }

               if (m_destOrigin.z >= pev->origin.z + 18.0f) {
                  state[i] += 5;
               }

               if (seesEntity (m_destOrigin)) {
                  game.makeVectors (m_moveAngles);

                  src = eyePos ();
                  src = src + game.vec.right * 15.0f;

                  game.testLine (src, m_destOrigin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

                  if (tr.flFraction >= 1.0f) {
                     src = eyePos ();
                     src = src - game.vec.right * 15.0f;

                     game.testLine (src, m_destOrigin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

                     if (tr.flFraction >= 1.0f) {
                        state[i] += 5;
                     }
                  }
               }
               if (pev->flags & FL_DUCKING) {
                  src = pev->origin;
               }
               else {
                  src = pev->origin + Vector (0.0f, 0.0f, -17.0f);
               }
               dst = src + dirNormal * 30.0f;
               game.testLine (src, dst, TRACE_IGNORE_EVERYTHING, ent (), &tr);

               if (tr.flFraction != 1.0f) {
                  state[i] += 10;
               }
            }
            else {
               state[i] = 0;
            }
            i++;

#if 0
            if (bits & PROBE_DUCK)
            {
               state[i] = 0;

               if (canDuckUnder (dirNormal)) {
                  state[i] += 10;
               }

               if ((m_destOrigin.z + 36.0f <= pev->origin.z) && seesEntity (m_destOrigin)) {
                  state[i] += 5;
               }
            }
            else
#endif
            state[i] = 0;
            i++;

            // weighted all possible moves, now sort them to start with most probable
            bool isSorting = false;

            do {
               isSorting = false;
               for (i = 0; i < 3; i++) {
                  if (state[i + MAX_COLLIDE_MOVES] < state[i + MAX_COLLIDE_MOVES + 1]) {
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

            for (i = 0; i < MAX_COLLIDE_MOVES; i++) {
               m_collideMoves[i] = state[i];
            }

            m_collideTime = game.timebase ();
            m_probeTime = game.timebase () + 0.5f;
            m_collisionProbeBits = bits;
            m_collisionState = COLLISION_PROBING;
            m_collStateIndex = 0;
         }
      }

      if (m_collisionState == COLLISION_PROBING) {
         if (m_probeTime < game.timebase ()) {
            m_collStateIndex++;
            m_probeTime = game.timebase () + 0.5f;

            if (m_collStateIndex > MAX_COLLIDE_MOVES) {
               m_navTimeset = game.timebase () - 5.0f;
               resetCollision ();
            }
         }

         if (m_collStateIndex < MAX_COLLIDE_MOVES) {
            switch (m_collideMoves[m_collStateIndex]) {
            case COLLISION_JUMP:
               if (isOnFloor () || isInWater ()) {
                  pev->button |= IN_JUMP;
                  m_jumpStateTimer = game.timebase () + rng.getFloat (0.7f, 1.5f);
               }
               break;

            case COLLISION_DUCK:
               if (isOnFloor () || isInWater ()) {
                  pev->button |= IN_DUCK;
               }
               break;

            case COLLISION_STRAFELEFT:
               pev->button |= IN_MOVELEFT;
               setStrafeSpeed (dirNormal, -pev->maxspeed);
               break;

            case COLLISION_STRAFERIGHT:
               pev->button |= IN_MOVERIGHT;
               setStrafeSpeed (dirNormal, pev->maxspeed);
               break;
            }
         }
      }
   }
   doPlayerAvoidance (dirNormal);
}

bool Bot::processNavigation (void) {
   // this function is a main path navigation

   TraceResult tr, tr2;
   
   // check if we need to find a waypoint...
   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX) {
      getValidPoint ();
      m_waypointOrigin = m_currentPath->origin;
      
      // if wayzone radios non zero vary origin a bit depending on the body angles
      if (m_currentPath->radius > 0) {
         game.makeVectors (Vector (pev->angles.x, cr::angleNorm (pev->angles.y + rng.getFloat (-90.0f, 90.0f)), 0.0f));
         m_waypointOrigin = m_waypointOrigin + game.vec.forward * rng.getFloat (0, m_currentPath->radius);
      }
      m_navTimeset = game.timebase ();
   }
   m_destOrigin = m_waypointOrigin + pev->view_ofs;

   float waypointDistance = (pev->origin - m_waypointOrigin).length ();

   // this waypoint has additional travel flags - care about them
   if (m_currentTravelFlags & PATHFLAG_JUMP) {

      // bot is not jumped yet?
      if (!m_jumpFinished) {

         // if bot's on the ground or on the ladder we're free to jump. actually setting the correct velocity is cheating.
         // pressing the jump button gives the illusion of the bot actual jumping.
         if (isOnFloor () || isOnLadder ()) {
            pev->velocity = m_desiredVelocity;
            pev->button |= IN_JUMP;

            m_jumpFinished = true;
            m_checkTerrain = false;
            m_desiredVelocity.nullify ();
         }
      }
      else if (!yb_jasonmode.boolean () && m_currentWeapon == WEAPON_KNIFE && isOnFloor ()) {
         selectBestWeapon ();
      }
   }

   if (m_currentPath->flags & FLAG_LADDER) {
      if (m_waypointOrigin.z >= (pev->origin.z + 16.0f)) {
         m_waypointOrigin = m_currentPath->origin + Vector (0.0f, 0.0f, 16.0f);
      }
      else if (m_waypointOrigin.z < pev->origin.z + 16.0f && !isOnLadder () && isOnFloor () && !(pev->flags & FL_DUCKING)) {
         m_moveSpeed = waypointDistance;

         if (m_moveSpeed < 150.0f) {
            m_moveSpeed = 150.0f;
         }
         else if (m_moveSpeed > pev->maxspeed) {
            m_moveSpeed = pev->maxspeed;
         }
      }
   }

   // special lift handling (code merged from podbotmm)
   if (m_currentPath->flags & FLAG_LIFT) {
      bool liftClosedDoorExists = false;

      // update waypoint time set
      m_navTimeset = game.timebase ();

      // trace line to door
      game.testLine (pev->origin, m_currentPath->origin, TRACE_IGNORE_EVERYTHING, ent (), &tr2);

      if (tr2.flFraction < 1.0f && strcmp (STRING (tr2.pHit->v.classname), "func_door") == 0 && (m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && pev->groundentity != tr2.pHit) {
         if (m_liftState == LIFT_NO_NEARBY) {
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = game.timebase () + 7.0f;
         }
         liftClosedDoorExists = true;
      }

      // trace line down
      game.testLine (m_currentPath->origin, m_currentPath->origin + Vector (0.0f, 0.0f, -50.0f), TRACE_IGNORE_EVERYTHING, ent (), &tr);

      // if trace result shows us that it is a lift
      if (!game.isNullEntity (tr.pHit) && !m_path.empty () && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0) && !liftClosedDoorExists) {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) && tr.pHit->v.velocity.z == 0.0f) {
            if (cr::abs (pev->origin.z - tr.vecEndPos.z) < 70.0f) {
               m_liftEntity = tr.pHit;
               m_liftState = LIFT_ENTERING_IN;
               m_liftTravelPos = m_currentPath->origin;
               m_liftUsageTime = game.timebase () + 5.0f;
            }
         }
         else if (m_liftState == LIFT_TRAVELING_BY) {
            m_liftState = LIFT_LEAVING;
            m_liftUsageTime = game.timebase () + 7.0f;
         }
      }
      else if (!m_path.empty ()) // no lift found at waypoint
      {
         if ((m_liftState == LIFT_NO_NEARBY || m_liftState == LIFT_WAITING_FOR) && m_path.hasNext ()) {
            int nextNode = m_path.next ();

            if (waypoints.exists (nextNode) && (waypoints[nextNode].flags & FLAG_LIFT)) {
               game.testLine (m_currentPath->origin, waypoints[nextNode].origin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

               if (!game.isNullEntity (tr.pHit) && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0)) {
                  m_liftEntity = tr.pHit;
               }
            }
            m_liftState = LIFT_LOOKING_BUTTON_OUTSIDE;
            m_liftUsageTime = game.timebase () + 15.0f;
         }
      }

      // bot is going to enter the lift
      if (m_liftState == LIFT_ENTERING_IN) {
         m_destOrigin = m_liftTravelPos;

         // check if we enough to destination
         if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AIM_NAVPOINT;

            resetCollision ();

            // need to wait our following teammate ?
            bool needWaitForTeammate = false;

            // if some bot is following a bot going into lift - he should take the same lift to go
            for (int i = 0; i < game.maxClients (); i++) {
               Bot *bot = bots.getBot (i);

               if (bot == nullptr || bot == this) {
                  continue;
               }

               if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != ent () || bot->taskId () != TASK_FOLLOWUSER) {
                  continue;
               }

               if (bot->pev->groundentity == m_liftEntity && bot->isOnFloor ()) {
                  break;
               }

               bot->m_liftEntity = m_liftEntity;
               bot->m_liftState = LIFT_ENTERING_IN;
               bot->m_liftTravelPos = m_liftTravelPos;

               needWaitForTeammate = true;
            }

            if (needWaitForTeammate) {
               m_liftState = LIFT_WAIT_FOR_TEAMMATES;
               m_liftUsageTime = game.timebase () + 8.0f;
            }
            else {
               m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
               m_liftUsageTime = game.timebase () + 10.0f;
            }
         }
      }

      // bot is waiting for his teammates
      if (m_liftState == LIFT_WAIT_FOR_TEAMMATES) {
         // need to wait our following teammate ?
         bool needWaitForTeammate = false;

         for (int i = 0; i < game.maxClients (); i++) {
            Bot *bot = bots.getBot (i);

            if (bot == nullptr) {
               continue; // skip invalid bots
            }

            if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != ent () || bot->taskId () != TASK_FOLLOWUSER || bot->m_liftEntity != m_liftEntity) {
               continue;
            }

            if (bot->pev->groundentity == m_liftEntity || !bot->isOnFloor ()) {
               needWaitForTeammate = true;
               break;
            }
         }

         // need to wait for teammate
         if (needWaitForTeammate) {
            m_destOrigin = m_liftTravelPos;

            if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = game.timebase ();
               m_aimFlags |= AIM_NAVPOINT;

               resetCollision ();
            }
         }

         // else we need to look for button
         if (!needWaitForTeammate || m_liftUsageTime < game.timebase ()) {
            m_liftState = LIFT_LOOKING_BUTTON_INSIDE;
            m_liftUsageTime = game.timebase () + 10.0f;
         }
      }

      // bot is trying to find button inside a lift
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE) {
         edict_t *button = getNearestButton (STRING (m_liftEntity->v.targetname));

         // got a valid button entity ?
         if (!game.isNullEntity (button) && pev->groundentity == m_liftEntity && m_buttonPushTime + 1.0f < game.timebase () && m_liftEntity->v.velocity.z == 0.0f && isOnFloor ()) {
            m_pickupItem = button;
            m_pickupType = PICKUP_BUTTON;

            m_navTimeset = game.timebase ();
         }
      }

      // is lift activated and bot is standing on it and lift is moving ?
      if (m_liftState == LIFT_LOOKING_BUTTON_INSIDE || m_liftState == LIFT_ENTERING_IN || m_liftState == LIFT_WAIT_FOR_TEAMMATES || m_liftState == LIFT_WAITING_FOR) {
         if (pev->groundentity == m_liftEntity && m_liftEntity->v.velocity.z != 0.0f && isOnFloor () && ((waypoints[m_prevWptIndex[0]].flags & FLAG_LIFT) || !game.isNullEntity (m_targetEntity))) {
            m_liftState = LIFT_TRAVELING_BY;
            m_liftUsageTime = game.timebase () + 14.0f;

            if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = game.timebase ();
               m_aimFlags |= AIM_NAVPOINT;

               resetCollision ();
            }
         }
      }

      // bots is currently moving on lift
      if (m_liftState == LIFT_TRAVELING_BY) {
         m_destOrigin = Vector (m_liftTravelPos.x, m_liftTravelPos.y, pev->origin.z);

         if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AIM_NAVPOINT;

            resetCollision ();
         }
      }

      // need to find a button outside the lift
      if (m_liftState == LIFT_LOOKING_BUTTON_OUTSIDE) {

         // button has been pressed, lift should come
         if (m_buttonPushTime + 8.0f >= game.timebase ()) {
            if (waypoints.exists (m_prevWptIndex[0])) {
               m_destOrigin = waypoints[m_prevWptIndex[0]].origin;
            }
            else {
               m_destOrigin = pev->origin;
            }

            if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = game.timebase ();
               m_aimFlags |= AIM_NAVPOINT;

               resetCollision ();
            }
         }
         else if (!game.isNullEntity (m_liftEntity)) {
            edict_t *button = getNearestButton (STRING (m_liftEntity->v.targetname));

            // if we got a valid button entity
            if (!game.isNullEntity (button)) {
               // lift is already used ?
               bool liftUsed = false;

               // iterate though clients, and find if lift already used
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent () || game.isNullEntity (client.ent->v.groundentity)) {
                     continue;
                  }

                  if (client.ent->v.groundentity == m_liftEntity) {
                     liftUsed = true;
                     break;
                  }
               }

               // lift is currently used
               if (liftUsed) {
                  if (waypoints.exists (m_prevWptIndex[0])) {
                     m_destOrigin = waypoints[m_prevWptIndex[0]].origin;
                  }
                  else {
                     m_destOrigin = button->v.origin;
                  }

                  if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
                     m_moveSpeed = 0.0f;
                     m_strafeSpeed = 0.0f;
                  }
               }
               else {
                  m_pickupItem = button;
                  m_pickupType = PICKUP_BUTTON;
                  m_liftState = LIFT_WAITING_FOR;

                  m_navTimeset = game.timebase ();
                  m_liftUsageTime = game.timebase () + 20.0f;
               }
            }
            else {
               m_liftState = LIFT_WAITING_FOR;
               m_liftUsageTime = game.timebase () + 15.0f;
            }
         }
      }

      // bot is waiting for lift
      if (m_liftState == LIFT_WAITING_FOR) {
         if (waypoints.exists (m_prevWptIndex[0])) {
            if (!(waypoints[m_prevWptIndex[0]].flags & FLAG_LIFT)) {
               m_destOrigin = waypoints[m_prevWptIndex[0]].origin;
            }
            else if (waypoints.exists (m_prevWptIndex[1])) {
               m_destOrigin = waypoints[m_prevWptIndex[1]].origin;
            }
         }

         if ((pev->origin - m_destOrigin).lengthSq () < 100.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AIM_NAVPOINT;

            resetCollision ();
         }
      }

      // if bot is waiting for lift, or going to it
      if (m_liftState == LIFT_WAITING_FOR || m_liftState == LIFT_ENTERING_IN) {
         // bot fall down somewhere inside the lift's groove :)
         if (pev->groundentity != m_liftEntity && waypoints.exists (m_prevWptIndex[0])) {
            if ((waypoints[m_prevWptIndex[0]].flags & FLAG_LIFT) && (m_currentPath->origin.z - pev->origin.z) > 50.0f && (waypoints[m_prevWptIndex[0]].origin.z - pev->origin.z) > 50.0f) {
               m_liftState = LIFT_NO_NEARBY;
               m_liftEntity = nullptr;
               m_liftUsageTime = 0.0f;

               clearSearchNodes ();
               searchOptimalPoint ();

               if (waypoints.exists (m_prevWptIndex[2])) {
                  searchPath (m_currentWaypointIndex, m_prevWptIndex[2], SEARCH_PATH_FASTEST);
               }
               return false;
            }
         }
      }
   }

   if (!game.isNullEntity (m_liftEntity) && !(m_currentPath->flags & FLAG_LIFT)) {
      if (m_liftState == LIFT_TRAVELING_BY) {
         m_liftState = LIFT_LEAVING;
         m_liftUsageTime = game.timebase () + 10.0f;
      }
      if (m_liftState == LIFT_LEAVING && m_liftUsageTime < game.timebase () && pev->groundentity != m_liftEntity) {
         m_liftState = LIFT_NO_NEARBY;
         m_liftUsageTime = 0.0f;

         m_liftEntity = nullptr;
      }
   }

   if (m_liftUsageTime < game.timebase () && m_liftUsageTime != 0.0f) {
      m_liftEntity = nullptr;
      m_liftState = LIFT_NO_NEARBY;
      m_liftUsageTime = 0.0f;

      clearSearchNodes ();

      if (waypoints.exists (m_prevWptIndex[0])) {
         if (!(waypoints[m_prevWptIndex[0]].flags & FLAG_LIFT)) {
            changePointIndex (m_prevWptIndex[0]);
         }
         else {
            searchOptimalPoint ();
         }
      }
      else {
         searchOptimalPoint ();
      }
      return false;
   }

   // check if we are going through a door...
   if (game.mapIs (MAP_HAS_DOORS)) {
      game.testLine (pev->origin, m_waypointOrigin, TRACE_IGNORE_MONSTERS, ent (), &tr);

      if (!game.isNullEntity (tr.pHit) && game.isNullEntity (m_liftEntity) && strncmp (STRING (tr.pHit->v.classname), "func_door", 9) == 0) {
         // if the door is near enough...
         if ((game.getAbsPos (tr.pHit) - pev->origin).lengthSq () < 2500.0f) {
            ignoreCollision (); // don't consider being stuck

            if (rng.chance (50)) {
               // do not use door directrly under xash, or we will get failed assert in gamedll code
               if (game.is (GAME_XASH_ENGINE)) {
                  pev->button |= IN_USE;
               }
               else {
                  MDLL_Use (tr.pHit, ent ()); // also 'use' the door randomly
               }
            }
         }

         // make sure we are always facing the door when going through it
         m_aimFlags &= ~(AIM_LAST_ENEMY | AIM_PREDICT_PATH);
         m_canChooseAimDirection = false;

         edict_t *button = getNearestButton (STRING (tr.pHit->v.targetname));

         // check if we got valid button
         if (!game.isNullEntity (button)) {
            m_pickupItem = button;
            m_pickupType = PICKUP_BUTTON;

            m_navTimeset = game.timebase ();
         }

         // if bot hits the door, then it opens, so wait a bit to let it open safely
         if (pev->velocity.length2D () < 2 && m_timeDoorOpen < game.timebase ()) {
            startTask (TASK_PAUSE, TASKPRI_PAUSE, INVALID_WAYPOINT_INDEX, game.timebase () + 0.5f, false);
            m_timeDoorOpen = game.timebase () + 1.0f; // retry in 1 sec until door is open

            edict_t *pent = nullptr;

            if (m_tryOpenDoor++ > 2 && util.findNearestPlayer (reinterpret_cast <void **> (&pent), ent (), 256.0f, false, false, true, true, false)) {
               m_seeEnemyTime = game.timebase () - 0.5f;

               m_states |= STATE_SEEING_ENEMY;
               m_aimFlags |= AIM_ENEMY;

               m_lastEnemy = pent;
               m_enemy = pent;
               m_lastEnemyOrigin = pent->v.origin;

               m_tryOpenDoor = 0;
            }
            else
               m_tryOpenDoor = 0;
         }
      }
   }
   float desiredDistance = 0.0f;

   // initialize the radius for a special waypoint type, where the wpt is considered to be reached
   if (m_currentPath->flags & FLAG_LIFT) {
      desiredDistance = 50.0f;
   }
   else if ((pev->flags & FL_DUCKING) || (m_currentPath->flags & FLAG_GOAL)) {
      desiredDistance = 25.0f;
   }
   else if (m_currentPath->flags & FLAG_LADDER) {
      desiredDistance = 15.0f;
   }
   else if (m_currentTravelFlags & PATHFLAG_JUMP) {
      desiredDistance = 0.0f;
   }
   else if (isOccupiedPoint (m_currentWaypointIndex)) {
      desiredDistance = 120.0f;
   }
   else {
      desiredDistance = m_currentPath->radius;
   }

   // check if waypoint has a special travelflag, so they need to be reached more precisely
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (m_currentPath->connectionFlags[i] != 0) {
         desiredDistance = 0.0f;
         break;
      }
   }

   // needs precise placement - check if we get past the point
   if (desiredDistance < 22.0f && waypointDistance < 30.0f && (pev->origin + (pev->velocity * calcThinkInterval ()) - m_waypointOrigin).lengthSq () > cr::square (waypointDistance)) {
      desiredDistance = waypointDistance + 1.0f;
   }

   if (waypointDistance < desiredDistance) {

      // did we reach a destination waypoint?
      if (task ()->data == m_currentWaypointIndex) {
         if (m_chosenGoalIndex != INVALID_WAYPOINT_INDEX) {

            // add goal values
            int goalValue = waypoints.getDangerValue (m_team, m_chosenGoalIndex, m_currentWaypointIndex);
            int addedValue = static_cast <int> (pev->health * 0.5f + m_goalValue * 0.5f);

            goalValue = cr::clamp (goalValue + addedValue, -MAX_GOAL_VALUE, MAX_GOAL_VALUE);

            // update the experience for team
            auto experience = waypoints.getRawExperience ();

            if (experience) {
               (experience + (m_chosenGoalIndex * waypoints.length ()) + m_currentWaypointIndex)->value[m_team] = goalValue;
            }
         }
         return true;
      }
      else if (m_path.empty ()) {
         return false;
      }
      int taskTarget = task ()->data;

      if (game.mapIs (MAP_DE) && bots.isBombPlanted () && m_team == TEAM_COUNTER && taskId () != TASK_ESCAPEFROMBOMB && taskTarget != INVALID_WAYPOINT_INDEX) {
         const Vector &bombOrigin = isBombAudible ();

         // bot within 'hearable' bomb tick noises?
         if (!bombOrigin.empty ()) {
            float distance = (bombOrigin - waypoints[taskTarget].origin).length ();

            if (distance > 512.0f) {
               if (rng.chance (50) && !waypoints.isVisited (taskTarget)) {
                  pushRadioMessage (RADIO_SECTOR_CLEAR);
               }
               waypoints.setVisited (taskTarget); // doesn't hear so not a good goal
            }
         }
         else {
            if (rng.chance (50) && !waypoints.isVisited (taskTarget)) {
               pushRadioMessage (RADIO_SECTOR_CLEAR);
            }
            waypoints.setVisited (taskTarget); // doesn't hear so not a good goal
         }
      }
      advanceMovement (); // do the actual movement checking
   }
   return false;
}

void Bot::searchShortestPath (int srcIndex, int destIndex) {
   // this function finds the shortest path from source index to destination index

   if (!waypoints.exists (srcIndex)){
      util.logEntry (false, LL_ERROR, "Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }
   else if (!waypoints.exists (destIndex)) {
      util.logEntry (false, LL_ERROR, "Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }
   clearSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   m_path.push (srcIndex);

   while (srcIndex != destIndex) {
      srcIndex = (waypoints.m_matrix + (srcIndex * waypoints.length ()) + destIndex)->index;

      if (srcIndex < 0) {
         m_prevGoalIndex = INVALID_WAYPOINT_INDEX;
         task ()->data = INVALID_WAYPOINT_INDEX;

         return;
      }
      m_path.push (srcIndex);
   }
}

void Bot::searchPath (int srcIndex, int destIndex, SearchPathType pathType /*= SEARCH_PATH_FASTEST */) {
   // this function finds a path from srcIndex to destIndex

   // least kills and number of nodes to goal for a team
   auto gfunctionKillsDist = [] (int team, int currentIndex, int parentIndex) -> float {
      if (parentIndex == INVALID_WAYPOINT_INDEX) {
         return 0.0f;
      }
      float cost = static_cast <float> (waypoints.getDangerDamage (team, currentIndex, currentIndex) + waypoints.getHighestDamageForTeam (team));

      Path &current = waypoints[currentIndex];

      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         int neighbour = current.index[i];

         if (neighbour != INVALID_WAYPOINT_INDEX) {
            cost += static_cast <float> (waypoints.getDangerDamage (team, neighbour, neighbour));
         }
      }

      if (current.flags & FLAG_CROUCH) {
         cost *= 1.5f;
      }
      return cost;
   };

   // least kills and number of nodes to goal for a team
   auto gfunctionKillsDistCTWithHostage = [&gfunctionKillsDist] (int team, int currentIndex, int parentIndex) -> float {
      Path &current = waypoints[currentIndex];

      if (current.flags & FLAG_NOHOSTAGE) {
         return 65355.0f;
      }
      else if (current.flags & (FLAG_CROUCH | FLAG_LADDER)) {
         return gfunctionKillsDist (team, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionKillsDist (team, currentIndex, parentIndex);
   };

   // least kills to goal for a team
   auto gfunctionKills = [] (int team, int currentIndex, int) -> float {
      float cost = static_cast <float> (waypoints.getDangerDamage (team, currentIndex, currentIndex));
      Path &current = waypoints[currentIndex];

      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         int neighbour = current.index[i];

         if (neighbour != INVALID_WAYPOINT_INDEX) {
            cost += static_cast <float> (waypoints.getDangerDamage (team, neighbour, neighbour));
         }
      }

      if (current.flags & FLAG_CROUCH) {
         cost *= 1.5f;
      }
      return cost + 0.5f;
   };

   // least kills to goal for a team
   auto gfunctionKillsCTWithHostage = [&gfunctionKills] (int team, int currentIndex, int parentIndex) -> float {
      if (parentIndex == INVALID_WAYPOINT_INDEX) {
         return 0.0f;
      }
      Path &current = waypoints[currentIndex];

      if (current.flags & FLAG_NOHOSTAGE) {
         return 65355.0f;
      }
      else if (current.flags & (FLAG_CROUCH | FLAG_LADDER)) {
         return gfunctionKills (team, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionKills (team, currentIndex, parentIndex);
   };

   auto gfunctionPathDist = [] (int, int currentIndex, int parentIndex) -> float {
      if (parentIndex == INVALID_WAYPOINT_INDEX) {
         return 0.0f;
      }
      Path &parent = waypoints[parentIndex];
      Path &current = waypoints[currentIndex];

      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         if (parent.index[i] == currentIndex) {
            // we don't like ladder or crouch point
            if (current.flags & (FLAG_CROUCH | FLAG_LADDER)) {
               return parent.distances[i] * 1.5f;
            }
            return static_cast <float> (parent.distances[i]);
         }
      }
      return 65355.0f;
   };

   auto gfunctionPathDistWithHostage = [&gfunctionPathDist] (int, int currentIndex, int parentIndex) -> float {
      Path &current = waypoints[currentIndex];

      if (current.flags & FLAG_NOHOSTAGE) {
         return 65355.0f;
      }
      else if (current.flags & (FLAG_CROUCH | FLAG_LADDER)) {
         return gfunctionPathDist (TEAM_UNASSIGNED, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionPathDist (TEAM_UNASSIGNED, currentIndex, parentIndex);
   };

   // square distance heuristic
   auto hfunctionPathDist = [] (int index, int, int goalIndex) -> float {
      Path &start = waypoints[index];
      Path &goal = waypoints[goalIndex];

      float x = cr::abs (start.origin.x - goal.origin.x);
      float y = cr::abs (start.origin.y - goal.origin.y);
      float z = cr::abs (start.origin.z - goal.origin.z);

      switch (yb_debug_heuristic_type.integer ()) {
      case 0:
      default:
         return cr::max (cr::max (x, y), z); // chebyshev distance

      case 1:
         return x + y + z; // manhattan distance

      case 2:
         return 0.0f; // no heuristic

      case 3:
      case 4:
         // euclidean based distance
         float euclidean = cr::powf (cr::powf (x, 2.0f) + cr::powf (y, 2.0f) + cr::powf (z, 2.0f), 0.5f);

         if (yb_debug_heuristic_type.integer () == 4) {
            return 1000.0f *(cr::ceilf (euclidean) - euclidean);
         }
         return euclidean;
      }
   };

   // square distance heuristic with hostages
   auto hfunctionPathDistWithHostage = [&hfunctionPathDist] (int index, int startIndex, int goalIndex) -> float {
      if (waypoints[startIndex].flags & FLAG_NOHOSTAGE) {
         return 65355.0f;
      }
      return hfunctionPathDist (index, startIndex, goalIndex);
   };

   // none heuristic
   auto hfunctionNone = [&hfunctionPathDist] (int index, int startIndex, int goalIndex) -> float {
      return hfunctionPathDist (index, startIndex, goalIndex) / 128.0f * 10.0f;
   };

   // get correct calculation for heuristic
   auto calculate = [&] (bool hfun, int a, int b, int c) -> float {
      if (pathType == SEARCH_PATH_SAFEST_FASTER) {
         if (game.mapIs (MAP_CS) && hasHostage ()) {
            if (hfun) {
               return hfunctionPathDistWithHostage (a, b, c);
            }
            else {
               return gfunctionKillsDistCTWithHostage (a, b, c);
            }
         }
         else {
            if (hfun) {
               return hfunctionPathDist (a, b, c);
            }
            else {
               return gfunctionKillsDist (a, b, c);
            }
         }
      }
      else if (pathType == SEARCH_PATH_SAFEST) {
         if (game.mapIs (MAP_CS) && hasHostage ()) {
            if (hfun) {
               return hfunctionNone (a, b, c);
            }
            else {
               return gfunctionKillsCTWithHostage (a, b, c);
            }
         }
         else {
            if (hfun) {
               return hfunctionNone (a, b, c);
            }
            else {
               return gfunctionKills (a, b, c);
            }
         }
      }
      else {
         if (game.mapIs (MAP_CS) && hasHostage ()) {
            if (hfun) {
               return hfunctionPathDistWithHostage (a, b, c);
            }
            else {
               return gfunctionPathDistWithHostage (a, b, c);
            }
         }
         else {
            if (hfun) {
               return hfunctionPathDist (a, b, c);
            }
            else {
               return gfunctionPathDist (a, b, c);
            }
         }
      }
   };

   if (!waypoints.exists (srcIndex)) {
      util.logEntry (false, LL_ERROR, "Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }
   else if (!waypoints.exists (destIndex)) {
      util.logEntry (false, LL_ERROR, "Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }
   clearSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   clearRoute ();
   auto srcRoute = &m_routes[srcIndex];

   // put start node into open list
   srcRoute->g = calculate (false, m_team, srcIndex, INVALID_WAYPOINT_INDEX);
   srcRoute->f = srcRoute->g + calculate (true, srcIndex, srcIndex, destIndex);
   srcRoute->state = ROUTE_OPEN;

   m_routeQue.clear ();
   m_routeQue.push (srcIndex, srcRoute->g);

   while (!m_routeQue.empty ()) {
      // remove the first node from the open list
      int currentIndex = m_routeQue.pop ();

      // safes us from bad waypoints...
      if (m_routeQue.length () >= MAX_ROUTE_LENGTH - 1) {
         util.logEntry (true, LL_ERROR, "A* Search for bots \"%s\" has tried to build path with at least %d nodes. Seems to be waypoints are broken.", STRING (pev->netname), m_routeQue.length ());

         // bail out to shortest path
         searchShortestPath (srcIndex, destIndex);
         return;
      }

      // is the current node the goal node?
      if (currentIndex == destIndex) {

         // build the complete path
         do {
            m_path.push (currentIndex);
            currentIndex = m_routes[currentIndex].parent;

         } while (currentIndex != INVALID_WAYPOINT_INDEX);

         m_path.reverse ();
         return;
      }
      auto curRoute = &m_routes[currentIndex];

      if (curRoute->state != ROUTE_OPEN) {
         continue;
      }

      // put current node into CLOSED list
      curRoute->state = ROUTE_CLOSED;

      // now expand the current node
      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         int currentChild = waypoints[currentIndex].index[i];

         if (currentChild == INVALID_WAYPOINT_INDEX) {
            continue;
         }
         auto childRoute = &m_routes[currentChild];

         // calculate the F value as F = G + H
         float g = curRoute->g + calculate (false, m_team, currentChild, currentIndex);
         float h = calculate (true, currentChild, srcIndex, destIndex);
         float f = g + h;

         if (childRoute->state == ROUTE_NEW || childRoute->f > f) {
            // put the current child into open list
            childRoute->parent = currentIndex;
            childRoute->state = ROUTE_OPEN;

            childRoute->g = g;
            childRoute->f = f;

            m_routeQue.push (currentChild, g);
         }
      }
   }
   searchShortestPath (srcIndex, destIndex); // A* found no path, try floyd pathfinder instead
}

void Bot::clearSearchNodes (void) {
   m_path.clear ();
   m_chosenGoalIndex = INVALID_WAYPOINT_INDEX;
}

void Bot::clearRoute (void) {
   int maxWaypoints = waypoints.length ();
   
   if (m_routes.capacity () < static_cast <size_t> (waypoints.length ())) {
      m_routes.reserve (maxWaypoints);
   }

   for (int i = 0; i < maxWaypoints; i++) {
      auto route = &m_routes[i];

      route->g = route->f = 0.0f;
      route->parent = INVALID_WAYPOINT_INDEX;
      route->state = ROUTE_NEW;
   }
   m_routes.clear ();
}

int Bot::searchAimingPoint (const Vector &to) {
   // return the most distant waypoint which is seen from the bot to the target and is within count

   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX) {
      m_currentWaypointIndex = changePointIndex (getNearestPoint ());
   }

   int destIndex = waypoints.getNearest (to);
   int bestIndex = m_currentWaypointIndex;

   if (destIndex == INVALID_WAYPOINT_INDEX) {
      return INVALID_WAYPOINT_INDEX;
   }
 
   while (destIndex != m_currentWaypointIndex) {
      destIndex = (waypoints.m_matrix + (destIndex * waypoints.length ()) + m_currentWaypointIndex)->index;

      if (destIndex < 0) {
         break;
      }

      if (waypoints.isVisible (m_currentWaypointIndex, destIndex) && waypoints.isVisible (destIndex, m_currentWaypointIndex)) {
         bestIndex = destIndex;
         break;
      }
   }

   if (bestIndex == m_currentWaypointIndex) {
      return INVALID_WAYPOINT_INDEX;
   }
   return bestIndex;
}

bool Bot::searchOptimalPoint (void) {
   // this function find a waypoint in the near of the bot if bot had lost his path of pathfinder needs
   // to be restarted over again.

   int busy = INVALID_WAYPOINT_INDEX;

   float lessDist[3] = { 99999.0f, 99999.0f , 99999.0f };
   int lessIndex[3] = { INVALID_WAYPOINT_INDEX, INVALID_WAYPOINT_INDEX , INVALID_WAYPOINT_INDEX };

   auto &bucket = waypoints.getWaypointsInBucket (pev->origin);
   int numToSkip = cr::clamp (rng.getInt (0, static_cast <int> (bucket.length () - 1)), 0, 5);

   for (const int at : bucket) {
      bool skip = !!(at == m_currentWaypointIndex);

      // skip the current waypoint, if any
      if (skip) {
         continue;
      }

      // skip current and recent previous waypoints
      for (int j = 0; j < numToSkip; j++) {
         if (at == m_prevWptIndex[j]) {
            skip = true;
            break;
         }
      }

      // skip waypoint from recent list
      if (skip) {
         continue;
      }

      // cts with hostages should not pick waypoints with no hostage flag
      if (game.mapIs (MAP_CS) && m_team == TEAM_COUNTER && (waypoints[at].flags & FLAG_NOHOSTAGE) && hasHostage ()) {
         continue;
      }

      // ignore non-reacheable waypoints...
      if (!waypoints.isReachable (this, at)) {
         continue;
      }

      // check if waypoint is already used by another bot...
      if (bots.getRoundStartTime () + 5.0f > game.timebase () && isOccupiedPoint (at)) {
         busy = at;
         continue;
      }

      // if we're still here, find some close waypoints
      float distance = (pev->origin - waypoints[at].origin).lengthSq ();

      if (distance < lessDist[0]) {
         lessDist[2] = lessDist[1];
         lessIndex[2] = lessIndex[1];

         lessDist[1] = lessDist[0];
         lessIndex[1] = lessIndex[0];

         lessDist[0] = distance;
         lessIndex[0] = at;
      }
      else if (distance < lessDist[1]) {
         lessDist[2] = lessDist[1];
         lessIndex[2] = lessIndex[1];

         lessDist[1] = distance;
         lessIndex[1] = at;
      }
      else if (distance < lessDist[2]) {
         lessDist[2] = distance;
         lessIndex[2] = at;
      }
   }
   int selected = INVALID_WAYPOINT_INDEX;

   // now pick random one from choosen
   int index = 0;

   // choice from found
   if (lessIndex[2] != INVALID_WAYPOINT_INDEX) {
      index = rng.getInt (0, 2);
   }
   else if (lessIndex[1] != INVALID_WAYPOINT_INDEX) {
      index = rng.getInt (0, 1);
   }
   else if (lessIndex[0] != INVALID_WAYPOINT_INDEX) {
      index = 0;
   }
   selected = lessIndex[index];

   // if we're still have no waypoint and have busy one (by other bot) pick it up
   if (selected == INVALID_WAYPOINT_INDEX && busy != INVALID_WAYPOINT_INDEX) {
      selected = busy;
   }

   // worst case... find atleast something
   if (selected == INVALID_WAYPOINT_INDEX) {
      selected = getNearestPoint ();
   }

   ignoreCollision ();
   changePointIndex (selected);

   return true;
}

float Bot::getReachTime (void) {
   auto task = taskId ();
   float estimatedTime = 0.0f; 

   switch (task) {
   case TASK_PAUSE:
   case TASK_CAMP:
   case TASK_HIDE:
      return 0.0f;

   default:
      estimatedTime = 2.8f; // time to reach next waypoint
   }

   // calculate 'real' time that we need to get from one waypoint to another
   if (waypoints.exists (m_currentWaypointIndex) && waypoints.exists (m_prevWptIndex[0])) {
      float distance = (waypoints[m_prevWptIndex[0]].origin - waypoints[m_currentWaypointIndex].origin).length ();

      // caclulate estimated time
      if (pev->maxspeed <= 0.0f) {
         estimatedTime = 3.0f * distance / 240.0f;
      }
      else {
         estimatedTime = 3.0f * distance / pev->maxspeed;
      }
      bool longTermReachability = (m_currentPath->flags & FLAG_CROUCH) || (m_currentPath->flags & FLAG_LADDER) || (pev->button & IN_DUCK) || (m_oldButtons & IN_DUCK);

      // check for special waypoints, that can slowdown our movement
      if (longTermReachability) {
         estimatedTime *= 2.0f;
      }
      estimatedTime = cr::clamp (estimatedTime, 1.2f, longTermReachability ? 8.0f : 5.0f);
   }
   return estimatedTime;
}

void Bot::getValidPoint (void) {
   // checks if the last waypoint the bot was heading for is still valid

   auto rechoiceGoal = [&] (void) {
      if (m_rechoiceGoalCount > 1) {
         int newGoal = searchGoal ();

         m_prevGoalIndex = newGoal;
         m_chosenGoalIndex = newGoal;
         task ()->data = newGoal;

         // do path finding if it's not the current waypoint
         if (newGoal != m_currentWaypointIndex) {
            searchPath (m_currentWaypointIndex, newGoal, m_pathType);
         }
         m_rechoiceGoalCount = 0;
      }
      else {
         searchOptimalPoint ();
         m_rechoiceGoalCount++;
      }
   };

   // if bot hasn't got a waypoint we need a new one anyway or if time to get there expired get new one as well
   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX) {
      clearSearchNodes ();
      rechoiceGoal ();

      m_waypointOrigin = m_currentPath->origin;
   }
   else if (m_navTimeset + getReachTime () < game.timebase () && game.isNullEntity (m_enemy)) {
      auto experience = waypoints.getRawExperience ();

      // increase danager for both teams
      for (int team = TEAM_TERRORIST; team < MAX_TEAM_COUNT; team++) {

         int damageValue = waypoints.getDangerDamage (team, m_currentWaypointIndex, m_currentWaypointIndex);
         damageValue = cr::clamp (damageValue + 100, 0, MAX_DAMAGE_VALUE);

         // affect nearby connected with victim waypoints
         for (int i = 0; i < MAX_PATH_INDEX; i++) {
            int neighbour = m_currentPath->index[i];

            if (waypoints.exists (neighbour)) {
               int neighbourValue = waypoints.getDangerDamage (team, neighbour, neighbour);
               neighbourValue = cr::clamp (neighbourValue + 100, 0, MAX_DAMAGE_VALUE);

               (experience + (neighbour * waypoints.length ()) + neighbour)->damage[team] = neighbourValue;
            }
         }
         (experience + (m_currentWaypointIndex * waypoints.length ()) + m_currentWaypointIndex)->damage[team] = damageValue;
      }

      clearSearchNodes ();
      rechoiceGoal ();
 
      m_waypointOrigin = m_currentPath->origin;
   }
}

int Bot::changePointIndex (int index) {
   if (index == INVALID_WAYPOINT_INDEX) {
      return 0;
   }
   m_prevWptIndex[4] = m_prevWptIndex[3];
   m_prevWptIndex[3] = m_prevWptIndex[2];
   m_prevWptIndex[2] = m_prevWptIndex[1];
   m_prevWptIndex[0] = m_currentWaypointIndex;

   m_currentWaypointIndex = index;
   m_navTimeset = game.timebase ();

   m_currentPath = &waypoints[index];
   m_waypointFlags = m_currentPath->flags;

   return m_currentWaypointIndex; // to satisfy static-code analyzers
}

int Bot::getNearestPoint (void) {
   // get the current nearest waypoint to bot with visibility checks

   int index = INVALID_WAYPOINT_INDEX;
   float minimum = cr::square (1024.0f);

   auto &bucket = waypoints.getWaypointsInBucket (pev->origin);

   for (const auto at : bucket) {
      if (at == m_currentWaypointIndex) {
         continue;
      }
      float distance = (waypoints[at].origin - pev->origin).lengthSq ();

      if (distance < minimum) {

         // if bot doing navigation, make sure waypoint really visible and not too high
         if ((m_currentWaypointIndex != INVALID_WAYPOINT_INDEX && waypoints.isVisible (m_currentWaypointIndex, at)) || waypoints.isReachable (this, at)) {
            index = at;
            minimum = distance;
         }
      }
   }

   // worst case, take any waypoint...
   if (index == INVALID_WAYPOINT_INDEX) {
      index = waypoints.getNearestNoBuckets (pev->origin);
   }
   return index;
}

int Bot::getBombPoint (void) {
   // this function finds the best goal (bomb) waypoint for CTs when searching for a planted bomb.

   auto &goals = waypoints.m_goalPoints;

   auto bomb = waypoints.getBombPos ();
   auto audible = isBombAudible ();

   if (!audible.empty ()) {
      m_bombSearchOverridden = true;
      return waypoints.getNearest (audible, 240.0f);
   }
   else if (goals.empty ()) {
      return waypoints.getNearest (bomb, 240.0f, FLAG_GOAL); // reliability check
   }

   // take the nearest to bomb waypoints instead of goal if close enough
   else if ((pev->origin - bomb).lengthSq () < cr::square (512.0f)) {
      int waypoint = waypoints.getNearest (bomb, 240.0f);
      m_bombSearchOverridden = true;

      if (waypoint != INVALID_WAYPOINT_INDEX) {
         return waypoint;
      }
   }

   int goal = 0, count = 0;
   float lastDistance = 999999.0f;

   // find nearest goal waypoint either to bomb (if "heard" or player)
   for (auto &point : goals) {
      float distance = (waypoints[point].origin - bomb).lengthSq ();

      // check if we got more close distance
      if (distance < lastDistance) {
         goal = point;
         lastDistance = distance;
      }
   }

   while (waypoints.isVisited (goal)) {
      goal = goals.random ();

      if (count++ >= static_cast <int> (goals.length ())) {
         break;
      }
   }
   return goal;
}

int Bot::getDefendPoint (const Vector &origin) {
   // this function tries to find a good position which has a line of sight to a position,
   // provides enough cover point, and is far away from the defending position

   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX) {
      m_currentWaypointIndex = changePointIndex (getNearestPoint ());
   }
   TraceResult tr;

   int waypointIndex[MAX_PATH_INDEX];
   int minDistance[MAX_PATH_INDEX];

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      waypointIndex[i] = INVALID_WAYPOINT_INDEX;
      minDistance[i] = 128;
   }

   int posIndex = waypoints.getNearest (origin);
   int srcIndex = m_currentWaypointIndex;

   // some of points not found, return random one
   if (srcIndex == INVALID_WAYPOINT_INDEX || posIndex == INVALID_WAYPOINT_INDEX) {
      return rng.getInt (0, waypoints.length () - 1);
   }

   // find the best waypoint now
   for (int i = 0; i < waypoints.length (); i++) {
      // exclude ladder & current waypoints
      if ((waypoints[i].flags & FLAG_LADDER) || i == srcIndex || !waypoints.isVisible (i, posIndex) || isOccupiedPoint (i)) {
         continue;
      }

      // use the 'real' pathfinding distances
      int distance = waypoints.getPathDist (srcIndex, i);

      // skip wayponts with distance more than 512 units
      if (distance > 512) {
         continue;
      }
      game.testLine (waypoints[i].origin, waypoints[posIndex].origin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

      // check if line not hit anything
      if (tr.flFraction != 1.0f) {
         continue;
      }

      for (int j = 0; j < MAX_PATH_INDEX; j++) {
         if (distance > minDistance[j]) {
            waypointIndex[j] = i;
            minDistance[j] = distance;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (waypointIndex[i] != INVALID_WAYPOINT_INDEX) {
         int experience = waypoints.getDangerDamage (m_team, waypointIndex[i], waypointIndex[i]);
         experience = (experience * 100) / waypoints.getHighestDamageForTeam (m_team);

         minDistance[i] = (experience * 100) / 8192;
         minDistance[i] += experience;
      }
   }
   bool sorting = false;

   // sort results waypoints for farest distance
   do {
      sorting = false;

      // completely sort the data
      for (int i = 0; i < 3 && waypointIndex[i] != INVALID_WAYPOINT_INDEX && waypointIndex[i + 1] != INVALID_WAYPOINT_INDEX && minDistance[i] > minDistance[i + 1]; i++) {
         int index = waypointIndex[i];

         waypointIndex[i] = waypointIndex[i + 1];
         waypointIndex[i + 1] = index;

         index = minDistance[i];

         minDistance[i] = minDistance[i + 1];
         minDistance[i + 1] = index;

         sorting = true;
      }
   } while (sorting);

   if (waypointIndex[0] == INVALID_WAYPOINT_INDEX) {
      IntArray found;

      for (int i = 0; i < waypoints.length (); i++) {
         if ((waypoints[i].origin - origin).lengthSq () <= cr::square (1248.0f) && !waypoints.isVisible (i, posIndex) && !isOccupiedPoint (i)) {
            found.push (i);
         }
      }

      if (found.empty ()) {
         return rng.getInt (0, waypoints.length () - 1); // most worst case, since there a evil error in waypoints
      }
      return found.random ();
   }
   int index = 0;

   for (; index < MAX_PATH_INDEX; index++) {
      if (waypointIndex[index] == INVALID_WAYPOINT_INDEX) {
         break;
      }
   }
   return waypointIndex[rng.getInt (0, static_cast <int> ((index - 1) * 0.5f))];
}

int Bot::getCoverPoint (float maxDistance) {
   // this function tries to find a good cover waypoint if bot wants to hide

   const float enemyMax = (m_lastEnemyOrigin - pev->origin).length ();

   // do not move to a position near to the enemy
   if (maxDistance > enemyMax) {
      maxDistance = enemyMax;
   }

   if (maxDistance < 300.0f) {
      maxDistance = 300.0f;
   }

   int srcIndex = m_currentWaypointIndex;
   int enemyIndex = waypoints.getNearest (m_lastEnemyOrigin);

   IntArray enemies;
   enemies.reserve (MAX_ENGINE_PLAYERS);

   int waypointIndex[MAX_PATH_INDEX];
   int minDistance[MAX_PATH_INDEX];

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      waypointIndex[i] = INVALID_WAYPOINT_INDEX;
      minDistance[i] = static_cast <int> (maxDistance);
   }

   if (enemyIndex == INVALID_WAYPOINT_INDEX) {
      return INVALID_WAYPOINT_INDEX;
   }

   // now get enemies neigbouring points
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (waypoints[enemyIndex].index[i] != INVALID_WAYPOINT_INDEX) {
         enemies.push (waypoints[enemyIndex].index[i]);
      }
   }

   // ensure we're on valid point
   changePointIndex (srcIndex);

   // find the best waypoint now
   for (int i = 0; i < waypoints.length (); i++) {
      // exclude ladder, current waypoints and waypoints seen by the enemy
      if ((waypoints[i].flags & FLAG_LADDER) || i == srcIndex || waypoints.isVisible (enemyIndex, i)) {
         continue;
      }
      bool neighbourVisible = false; // now check neighbour waypoints for visibility

      for (auto &enemy : enemies) {
         if (waypoints.isVisible (enemy, i)) {
            neighbourVisible = true;
            break;
         }
      }

      // skip visible points
      if (neighbourVisible) {
         continue;
      }

      // use the 'real' pathfinding distances
      int distances = waypoints.getPathDist (srcIndex, i);
      int enemyDistance = waypoints.getPathDist (enemyIndex, i);

      if (distances >= enemyDistance) {
         continue;
      }

      for (int j = 0; j < MAX_PATH_INDEX; j++) {
         if (distances < minDistance[j]) {
            waypointIndex[j] = i;
            minDistance[j] = distances;

            break;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (waypointIndex[i] != INVALID_WAYPOINT_INDEX) {
         int experience = waypoints.getDangerDamage (m_team, waypointIndex[i], waypointIndex[i]);
         experience = (experience * 100) / MAX_DAMAGE_VALUE;

         minDistance[i] = (experience * 100) / 8192;
         minDistance[i] += experience;
      }
   }
   bool sorting;

   // sort resulting waypoints for nearest distance
   do {
      sorting = false;

      for (int i = 0; i < 3 && waypointIndex[i] != INVALID_WAYPOINT_INDEX && waypointIndex[i + 1] != INVALID_WAYPOINT_INDEX && minDistance[i] > minDistance[i + 1]; i++) {
         int index = waypointIndex[i];

         waypointIndex[i] = waypointIndex[i + 1];
         waypointIndex[i + 1] = index;

         index = minDistance[i];
         minDistance[i] = minDistance[i + 1];
         minDistance[i + 1] = index;

         sorting = true;
      }
   } while (sorting);

   TraceResult tr;

   // take the first one which isn't spotted by the enemy
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (waypointIndex[i] != INVALID_WAYPOINT_INDEX) {
         game.testLine (m_lastEnemyOrigin + Vector (0.0f, 0.0f, 36.0f), waypoints[waypointIndex[i]].origin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

         if (tr.flFraction < 1.0f) {
            return waypointIndex[i];
         }
      }
   }

   // if all are seen by the enemy, take the first one
   if (waypointIndex[0] != INVALID_WAYPOINT_INDEX) {
      return waypointIndex[0];
   }
   return INVALID_WAYPOINT_INDEX; // do not use random points
}

bool Bot::getNextBestPoint (void) {
   // this function does a realtime post processing of waypoints return from the
   // pathfinder, to vary paths and find the best waypoint on our way

   assert (!m_path.empty ());
   assert (m_path.hasNext ());

   if (!isOccupiedPoint (m_path.first ())) {
      return false;
   }

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      int id = m_currentPath->index[i];

      if (id != INVALID_WAYPOINT_INDEX && waypoints.isConnected (id, m_path.next ()) && waypoints.isConnected (m_currentWaypointIndex, id)) {

         // don't use ladder waypoints as alternative
         if (waypoints[id].flags & FLAG_LADDER) {
            continue;
         }

         if (!isOccupiedPoint (id)) {
            m_path.first () = id;
            return true;
         }
      }
   }
   return false;
}

bool Bot::advanceMovement (void) {
   // advances in our pathfinding list and sets the appropiate destination origins for this bot

   getValidPoint (); // check if old waypoints is still reliable
   
   // no waypoints from pathfinding?
   if (m_path.empty ()) {
      return false;
   }
   TraceResult tr;
   
   m_path.shift (); // advance in list
   m_currentTravelFlags = 0; // reset travel flags (jumping etc)
   
   // we're not at the end of the list?
   if (!m_path.empty ()) {
      // if in between a route, postprocess the waypoint (find better alternatives)...
      if (m_path.hasNext () && m_path.first () != m_path.back ()) {
         getNextBestPoint ();
         m_minSpeed = pev->maxspeed;

         TaskID taskID = taskId ();

         // only if we in normal task and bomb is not planted
         if (taskID == TASK_NORMAL && bots.getRoundMidTime () + 5.0f < game.timebase () && m_timeCamping + 5.0f < game.timebase () && !bots.isBombPlanted () && m_personality != PERSONALITY_RUSHER && !m_hasC4 && !m_isVIP && m_loosedBombWptIndex == INVALID_WAYPOINT_INDEX && !hasHostage ()) {
            m_campButtons = 0;

            const int nextIndex = m_path.next ();
            float kills = static_cast <float> (waypoints.getDangerDamage (m_team, nextIndex, nextIndex));

            // if damage done higher than one
            if (kills > 1.0f && bots.getRoundMidTime () > game.timebase ()) {
               switch (m_personality) {
               case PERSONALITY_NORMAL:
                  kills *= 0.33f;
                  break;

               default:
                  kills *= 0.5f;
                  break;
               }

               if (m_baseAgressionLevel < kills && hasPrimaryWeapon ()) {
                  startTask (TASK_CAMP, TASKPRI_CAMP, INVALID_WAYPOINT_INDEX, game.timebase () + rng.getFloat (m_difficulty * 0.5f, static_cast <float> (m_difficulty)) * 5.0f, true);
                  startTask (TASK_MOVETOPOSITION, TASKPRI_MOVETOPOSITION, getDefendPoint (waypoints[nextIndex].origin), game.timebase () + rng.getFloat (3.0f, 10.0f), true);
               }
            }
            else if (bots.canPause () && !isOnLadder () && !isInWater () && !m_currentTravelFlags && isOnFloor ()) {
               if (static_cast <float> (kills) == m_baseAgressionLevel) {
                  m_campButtons |= IN_DUCK;
               }
               else if (rng.chance (m_difficulty * 25)) {
                  m_minSpeed = getShiftSpeed ();
               }
            }

            // force terrorist bot to plant bomb
            if (m_inBombZone && !m_hasProgressBar && m_hasC4) {
               int newGoal = searchGoal ();

               m_prevGoalIndex = newGoal;
               m_chosenGoalIndex = newGoal;

               // remember index
               task ()->data = newGoal;

               // do path finding if it's not the current waypoint
               if (newGoal != m_currentWaypointIndex) {
                  searchPath (m_currentWaypointIndex, newGoal, m_pathType);
               }
               return false;
            }
         }
      }

      if (!m_path.empty ()) {
         const int destIndex = m_path.first ();
         
         // find out about connection flags
         if (m_currentWaypointIndex != INVALID_WAYPOINT_INDEX) {
            for (int i = 0; i < MAX_PATH_INDEX; i++) {
               if (m_currentPath->index[i] == destIndex) {
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
            if (m_path.hasNext ()) {
               for (int i = 0; i < MAX_PATH_INDEX; i++) {
                  const int nextIndex = m_path.next ();

                  Path &path = waypoints[destIndex];
                  Path &next = waypoints[nextIndex];

                  if (path.index[i] == nextIndex && (path.connectionFlags[i] & PATHFLAG_JUMP)) {
                     src = path.origin;
                     dst = next.origin;

                     jumpDistance = (path.origin - next.origin).length ();
                     willJump = true;

                     break;
                  }
               }
            }

            // is there a jump waypoint right ahead and do we need to draw out the light weapon ?
            if (willJump && m_currentWeapon != WEAPON_KNIFE && m_currentWeapon != WEAPON_SCOUT && !m_isReloading && !usesPistol () && (jumpDistance > 200.0f || (dst.z - 32.0f > src.z && jumpDistance > 150.0f)) && !(m_states & (STATE_SEEING_ENEMY | STATE_SUSPECT_ENEMY))) {
               selectWeaponByName ("weapon_knife"); // draw out the knife if we needed
            }

            // bot not already on ladder but will be soon?
            if ((waypoints[destIndex].flags & FLAG_LADDER) && !isOnLadder ()) {
               // get ladder waypoints used by other (first moving) bots
               for (int c = 0; c < game.maxClients (); c++) {
                  Bot *otherBot = bots.getBot (c);

                  // if another bot uses this ladder, wait 3 secs
                  if (otherBot != nullptr && otherBot != this && otherBot->m_notKilled && otherBot->m_currentWaypointIndex == destIndex) {
                     startTask (TASK_PAUSE, TASKPRI_PAUSE, INVALID_WAYPOINT_INDEX, game.timebase () + 3.0f, false);
                     return true;
                  }
               }
            }
         }
         changePointIndex (destIndex);
      }
   }
   m_waypointOrigin = m_currentPath->origin;

   // if wayzone radius non zero vary origin a bit depending on the body angles
   if (m_currentPath->radius > 0.0f) {
      game.makeVectors (Vector (pev->angles.x, cr::angleNorm (pev->angles.y + rng.getFloat (-90.0f, 90.0f)), 0.0f));
      m_waypointOrigin = m_waypointOrigin + game.vec.forward * rng.getFloat (0.0f, m_currentPath->radius);
   }

   if (isOnLadder ()) {
      game.testLine (Vector (pev->origin.x, pev->origin.y, pev->absmin.z), m_waypointOrigin, TRACE_IGNORE_EVERYTHING, ent (), &tr);

      if (tr.flFraction < 1.0f) {
         m_waypointOrigin = m_waypointOrigin + (pev->origin - m_waypointOrigin) * 0.5f + Vector (0.0f, 0.0f, 32.0f);
      }
   }
   m_navTimeset = game.timebase ();

   return true;
}

bool Bot::cantMoveForward (const Vector &normal, TraceResult *tr) {
   // checks if bot is blocked in his movement direction (excluding doors)

   // use some TraceLines to determine if anything is blocking the current path of the bot.

   // first do a trace from the bot's eyes forward...
   Vector src = eyePos ();
   Vector forward = src + normal * 24.0f;

   game.makeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   auto checkDoor = [] (TraceResult *tr) {
      if (!game.mapIs (MAP_HAS_DOORS)) {
         return false;
      }
      return tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0;
   };

   // trace from the bot's eyes straight forward...
   game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      if (game.mapIs (MAP_HAS_DOORS) && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) == 0) {
         return false;
      }
      return true; // bot's head will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder left diagonal forward to the right shoulder...
   src = eyePos () + Vector (0.0f, 0.0f, -16.0f) - game.vec.right * -16.0f;
   forward = eyePos () + Vector (0.0f, 0.0f, -16.0f) + game.vec.right * 16.0f + normal * 24.0f;

   game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

   // check if the trace hit something...
   if (checkDoor (tr)) {
      return true; // bot's body will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder right diagonal forward to the left shoulder...
   src = eyePos () + Vector (0.0f, 0.0f, -16.0f) + game.vec.right * 16.0f;
   forward = eyePos () + Vector (0.0f, 0.0f, -16.0f) - game.vec.right * -16.0f + normal * 24.0f;

   game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

   // check if the trace hit something...
   if (checkDoor (tr)) {
      return true; // bot's body will hit something
   }

   // now check below waist
   if (pev->flags & FL_DUCKING) {
      src = pev->origin + Vector (0.0f, 0.0f, -19.0f + 19.0f);
      forward = src + Vector (0.0f, 0.0f, 10.0f) + normal * 24.0f;

      game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }
      src = pev->origin;
      forward = src + normal * 24.0f;

      game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }
   }
   else {
      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0.0f, 0.0f, -17.0f) - game.vec.right * -16.0f;
      forward = pev->origin + Vector (0.0f, 0.0f, -17.0f) + game.vec.right * 16.0f + normal * 24.0f;

      // trace from the bot's waist straight forward...
      game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }

      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0.0f, 0.0f, -17.0f) + game.vec.right * 16.0f;
      forward = pev->origin + Vector (0.0f, 0.0f, -17.0f) - game.vec.right * -16.0f + normal * 24.0f;

      game.testLine (src, forward, TRACE_IGNORE_MONSTERS, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }
   }
   return false; // bot can move forward, return false
}

#ifdef DEAD_CODE
bool Bot::canStrafeLeft (TraceResult *tr) {
   // this function checks if bot can move sideways

   makeVectors (pev->v_angle);

   Vector src = pev->origin;
   Vector left = src - game.vec.right * -40.0f;

   // trace from the bot's waist straight left...
   TraceLine (src, left, true, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      return false; // bot's body will hit something
   }

   src = left;
   left = left + game.vec.forward * 40.0f;

   // trace from the strafe pos straight forward...
   TraceLine (src, left, true, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      return false; // bot's body will hit something
   }
   return true;
}

bool Bot::canStrafeRight (TraceResult *tr) {
   // this function checks if bot can move sideways

   makeVectors (pev->v_angle);

   Vector src = pev->origin;
   Vector right = src + game.vec.right * 40.0f;

   // trace from the bot's waist straight right...
   TraceLine (src, right, true, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      return false; // bot's body will hit something
   }
   src = right;
   right = right + game.vec.forward * 40.0f;

   // trace from the strafe pos straight forward...
   TraceLine (src, right, true, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      return false; // bot's body will hit something
   }
   return true;
}

#endif

bool Bot::canJumpUp (const Vector &normal) {
   // this function check if bot can jump over some obstacle

   TraceResult tr;

   // can't jump if not on ground and not on ladder/swimming
   if (!isOnFloor () && (isOnLadder () || !isInWater ())) {
      return false;
   }

   // convert current view angle to vectors for traceline math...
   game.makeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   // check for normal jump height first...
   Vector src = pev->origin + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   Vector dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }
   else {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

      if (tr.flFraction < 1.0f) {
         return false;
      }
   }

   // now check same height to one side of the bot...
   src = pev->origin + game.vec.right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = pev->origin + (-game.vec.right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

bool Bot::doneCanJumpUp (const Vector &normal) {
   // use center of the body first... maximum duck jump height is 62, so check one unit above that (63)
   Vector src = pev->origin + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   Vector dest = src + normal * 32.0f;

   TraceResult tr;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   if (tr.flFraction < 1.0f) {
      return false;
   }
   else {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

      // if trace hit something, check duckjump
      if (tr.flFraction < 1.0f) {
         return false;
      }
   }

   // now check same height to one side of the bot...
   src = pev->origin + game.vec.right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = pev->origin + (-game.vec.right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

bool Bot::canDuckUnder (const Vector &normal) {
   // this function check if bot can duck under obstacle

   TraceResult tr;
   Vector baseHeight;

   // convert current view angle to vectors for TraceLine math...
   game.makeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   // use center of the body first...
   if (pev->flags & FL_DUCKING) {
      baseHeight = pev->origin + Vector (0.0f, 0.0f, -17.0f);
   }
   else {
      baseHeight = pev->origin;
   }

   Vector src = baseHeight;
   Vector dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height to one side of the bot...
   src = baseHeight + game.vec.right * 16.0f;
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = baseHeight + (-game.vec.right * 16.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   game.testLine (src, dest, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

#ifdef DEAD_CODE

bool Bot::isBlockedLeft (void) {
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0.0f) {
      direction = -48;
   }
   makeVectors (pev->angles);

   // do a trace to the left...
   game.TestLine (pev->origin, game.vec.forward * direction - game.vec.right * 48.0f, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if the trace hit something...
   if (game.mapIs (MAP_HAS_DOORS) && tr.flFraction < 1.0f && strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0) {
      return true; // bot's body will hit something
   }
   return false;
}

bool Bot::isBlockedRight (void) {
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0) {
      direction = -48;
   }
   makeVectors (pev->angles);

   // do a trace to the right...
   game.TestLine (pev->origin, pev->origin + game.vec.forward * direction + game.vec.right * 48.0f, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if the trace hit something...
   if (game.mapIs (MAP_HAS_DOORS) && tr.flFraction < 1.0f && (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0)) {
      return true; // bot's body will hit something
   }
   return false;
}

#endif

bool Bot::checkWallOnLeft (void) {
   TraceResult tr;
   game.makeVectors (pev->angles);

   game.testLine (pev->origin, pev->origin - game.vec.right * 40.0f, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      return true;
   }
   return false;
}

bool Bot::checkWallOnRight (void) {
   TraceResult tr;
   game.makeVectors (pev->angles);

   // do a trace to the right...
   game.testLine (pev->origin, pev->origin + game.vec.right * 40.0f, TRACE_IGNORE_MONSTERS, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      return true;
   }
   return false;
}

bool Bot::isDeadlyMove (const Vector &to) {
   // this function eturns if given location would hurt Bot with falling damage

   Vector botPos = pev->origin;
   TraceResult tr;

   Vector move ((to - botPos).toYaw (), 0.0f, 0.0f);
   game.makeVectors (move);

   Vector direction = (to - botPos).normalize (); // 1 unit long
   Vector check = botPos;
   Vector down = botPos;

   down.z = down.z - 1000.0f; // straight down 1000 units

   game.testHull (check, down, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

   if (tr.flFraction > 0.036f) { // we're not on ground anymore?
      tr.flFraction = 0.036f;
   }

   float lastHeight = tr.flFraction * 1000.0f; // height from ground
   float distance = (to - check).length (); // distance from goal

   while (distance > 16.0f) {
      check = check + direction * 16.0f; // move 10 units closer to the goal...

      down = check;
      down.z = down.z - 1000.0f; // straight down 1000 units

      game.testHull (check, down, TRACE_IGNORE_MONSTERS, head_hull, ent (), &tr);

      if (tr.fStartSolid) { // Wall blocking?
         return false;
      }
      float height = tr.flFraction * 1000.0f; // height from ground

      if (lastHeight < height - 100.0f) {// Drops more than 100 Units?
         return true;
      }
      lastHeight = height;
      distance = (to - check).length (); // distance from goal
   }
   return false;
}

#ifdef DEAD_CODE

void Bot::changePitch (float speed) {
   // this function turns a bot towards its ideal_pitch

   float idealPitch = AngleNormalize (pev->idealpitch);
   float curent = AngleNormalize (pev->v_angle.x);

   // turn from the current v_angle pitch to the idealpitch by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0.0f) {
      if (normalizePitch > speed) {
         normalizePitch = speed;
      }
   }
   else {
      if (normalizePitch < -speed) {
         normalizePitch = -speed;
      }
   }

   pev->v_angle.x = AngleNormalize (curent + normalizePitch);

   if (pev->v_angle.x > 89.9f) {
      pev->v_angle.x = 89.9f;
   }

   if (pev->v_angle.x < -89.9f) {
      pev->v_angle.x = -89.9f;
   }
   pev->angles.x = -pev->v_angle.x / 3;
}

void Bot::changeYaw (float speed) {
   // this function turns a bot towards its ideal_yaw

   float idealPitch = AngleNormalize (pev->ideal_yaw);
   float curent = AngleNormalize (pev->v_angle.y);

   // turn from the current v_angle yaw to the ideal_yaw by selecting
   // the quickest way to turn to face that direction

   // find the difference in the curent and ideal angle
   float normalizePitch = AngleNormalize (idealPitch - curent);

   if (normalizePitch > 0.0f) {
      if (normalizePitch > speed) {
         normalizePitch = speed;
      }
   }
   else {
      if (normalizePitch < -speed) {
         normalizePitch = -speed;
      }
   }
   pev->v_angle.y = AngleNormalize (curent + normalizePitch);
   pev->angles.y = pev->v_angle.y;
}

#endif

int Bot::searchCampDir (void) {
   // find a good waypoint to look at when camping

   if (m_currentWaypointIndex == INVALID_WAYPOINT_INDEX) {
      m_currentWaypointIndex = changePointIndex (getNearestPoint ());
   }

   int count = 0, indices[3];
   float distTab[3];
   uint16 visibility[3];

   int currentWaypoint = m_currentWaypointIndex;

   for (int i = 0; i < waypoints.length (); i++) {
      if (currentWaypoint == i || !waypoints.isVisible (currentWaypoint, i)) {
         continue;
      }
      Path &path = waypoints[i];

      if (count < 3) {
         indices[count] = i;

         distTab[count] = (pev->origin - path.origin).lengthSq ();
         visibility[count] = path.vis.crouch + path.vis.stand;

         count++;
      }
      else {
         float distance = (pev->origin - path.origin).lengthSq ();
         uint16 visBits = path.vis.crouch + path.vis.stand;

         for (int j = 0; j < 3; j++) {
            if (visBits >= visibility[j] && distance > distTab[j]) {
               indices[j] = i;

               distTab[j] = distance;
               visibility[j] = visBits;

               break;
            }
         }
      }
   }
   count--;

   if (count >= 0) {
      return indices[rng.getInt (0, count)];
   }
   return rng.getInt (0, waypoints.length () - 1);
}

void Bot::processBodyAngles (void) {
   // set the body angles to point the gun correctly
   pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
   pev->angles.y = pev->v_angle.y;

   pev->angles.clampAngles ();
}

void Bot::processLookAngles (void) {
   const float delta = cr::clamp (game.timebase () - m_lookUpdateTime, cr::EQEPSILON, 0.05f);
   m_lookUpdateTime = game.timebase ();

   // adjust all body and view angles to face an absolute vector
   Vector direction = (m_lookAt - eyePos ()).toAngles ();
   direction.x *= -1.0f; // invert for engine

   direction.clampAngles ();

   // lower skilled bot's have lower aiming
   if (m_difficulty < 2) {
      updateLookAnglesNewbie (direction, delta);
      processBodyAngles ();

      return;
   }

   if (m_difficulty > 3 && (m_aimFlags & AIM_ENEMY) && (m_wantsToFire || usesSniper ()) && yb_whose_your_daddy.boolean ()) {
      pev->v_angle = direction;
      processBodyAngles ();

      return;
   }

   float accelerate = 3000.0f;
   float stiffness = 200.0f;
   float damping = 25.0f;

   if ((m_aimFlags & (AIM_ENEMY | AIM_ENTITY | AIM_GRENADE)) && m_difficulty > 3) {
      accelerate += 800.0f;
      stiffness += 320.0f;
      damping -= 8.0f;
   }
   m_idealAngles = pev->v_angle;

   float angleDiffYaw = cr::angleDiff (direction.y, m_idealAngles.y);
   float angleDiffPitch = cr::angleDiff (direction.x, m_idealAngles.x);

   if (angleDiffYaw < 1.0f && angleDiffYaw > -1.0f) {
      m_lookYawVel = 0.0f;
      m_idealAngles.y = direction.y;
   }
   else {
      float accel = cr::clamp (stiffness * angleDiffYaw - damping * m_lookYawVel, -accelerate, accelerate);

      m_lookYawVel += delta * accel;
      m_idealAngles.y += delta * m_lookYawVel;
   }
   float accel = cr::clamp (2.0f * stiffness * angleDiffPitch - damping * m_lookPitchVel, -accelerate, accelerate);

   m_lookPitchVel += delta * accel;
   m_idealAngles.x += cr::clamp (delta * m_lookPitchVel, -89.0f, 89.0f);

   pev->v_angle = m_idealAngles;
   pev->v_angle.clampAngles ();

   processBodyAngles ();
}

void Bot::updateLookAnglesNewbie (const Vector &direction, const float delta) {
   Vector spring (13.0f, 13.0f, 0.0f);
   Vector damperCoefficient (0.22f, 0.22f, 0.0f);

   const float offset = cr::clamp (m_difficulty, 1, 4) * 25.0f;

   Vector influence = Vector (0.25f, 0.17f, 0.0f) * (100.0f - offset) / 100.f;
   Vector randomization = Vector (2.0f, 0.18f, 0.0f) * (100.0f - offset) / 100.f;

   const float noTargetRatio = 0.3f;
   const float offsetDelay = 1.2f;

   Vector stiffness;
   Vector randomize;

   m_idealAngles = direction.make2D ();
   m_idealAngles.clampAngles ();

   if (m_aimFlags & (AIM_ENEMY | AIM_ENTITY)) {
      m_playerTargetTime = game.timebase ();
      m_randomizedIdealAngles = m_idealAngles;

      stiffness = spring * (0.2f + offset / 125.0f);
   }
   else {
      // is it time for bot to randomize the aim direction again (more often where moving) ?
      if (m_randomizeAnglesTime < game.timebase () && ((pev->velocity.length () > 1.0f && m_angularDeviation.length () < 5.0f) || m_angularDeviation.length () < 1.0f)) {
         // is the bot standing still ?
         if (pev->velocity.length () < 1.0f) {
            randomize = randomization * 0.2f; // randomize less
         }
         else {
            randomize = randomization;
         }
         // randomize targeted location a bit (slightly towards the ground)
         m_randomizedIdealAngles = m_idealAngles + Vector (rng.getFloat (-randomize.x * 0.5f, randomize.x * 1.5f), rng.getFloat (-randomize.y, randomize.y), 0.0f);

         // set next time to do this
         m_randomizeAnglesTime = game.timebase () + rng.getFloat (0.4f, offsetDelay);
      }
      float stiffnessMultiplier = noTargetRatio;

      // take in account whether the bot was targeting someone in the last N seconds
      if (game.timebase () - (m_playerTargetTime + offsetDelay) < noTargetRatio * 10.0f) {
         stiffnessMultiplier = 1.0f - (game.timebase () - m_timeLastFired) * 0.1f;

         // don't allow that stiffness multiplier less than zero
         if (stiffnessMultiplier < 0.0f) {
            stiffnessMultiplier = 0.5f;
         }
      }

      // also take in account the remaining deviation (slow down the aiming in the last 10°)
      stiffnessMultiplier *= m_angularDeviation.length () * 0.1f * 0.5f;

      // but don't allow getting below a certain value
      if (stiffnessMultiplier < 0.35f) {
         stiffnessMultiplier = 0.35f;
      }
      stiffness = spring * stiffnessMultiplier; // increasingly slow aim
   }
   // compute randomized angle deviation this time
   m_angularDeviation = m_randomizedIdealAngles - pev->v_angle;
   m_angularDeviation.clampAngles ();

   // spring/damper model aiming
   m_aimSpeed.x = stiffness.x * m_angularDeviation.x - damperCoefficient.x * m_aimSpeed.x;
   m_aimSpeed.y = stiffness.y * m_angularDeviation.y - damperCoefficient.y * m_aimSpeed.y;

   // influence of y movement on x axis and vice versa (less influence than x on y since it's
   // easier and more natural for the bot to "move its mouse" horizontally than vertically)
   m_aimSpeed.x += cr::clamp (m_aimSpeed.y * influence.y, -50.0f, 50.0f);
   m_aimSpeed.y += cr::clamp (m_aimSpeed.x * influence.x, -200.0f, 200.0f);

   // move the aim cursor
   pev->v_angle = pev->v_angle + delta * Vector (m_aimSpeed.x, m_aimSpeed.y, 0.0f);
   pev->v_angle.clampAngles ();
}

void Bot::setStrafeSpeed (const Vector &moveDir, float strafeSpeed) {
   game.makeVectors (pev->angles);

   const Vector &los = (moveDir - pev->origin).normalize2D ();
   float dot = los | game.vec.forward.make2D ();

   if (dot > 0.0f && !checkWallOnRight ()) {
      m_strafeSpeed = strafeSpeed;
   }
   else if (!checkWallOnLeft ()) {
      m_strafeSpeed = -strafeSpeed;
   }
}

int Bot::locatePlantedC4 (void) {
   // this function tries to find planted c4 on the defuse scenario map and returns nearest to it waypoint

   if (m_team != TEAM_TERRORIST || !game.mapIs (MAP_DE)) {
      return INVALID_WAYPOINT_INDEX; // don't search for bomb if the player is CT, or it's not defusing bomb
   }

   edict_t *bombEntity = nullptr; // temporaly pointer to bomb

   // search the bomb on the map
   while (!game.isNullEntity (bombEntity = engfuncs.pfnFindEntityByString (bombEntity, "classname", "grenade"))) {
      if (strcmp (STRING (bombEntity->v.model) + 9, "c4.mdl") == 0) {
         int nearestIndex = waypoints.getNearest (game.getAbsPos (bombEntity));

         if (waypoints.exists (nearestIndex)) {
            return nearestIndex;
         }
         break;
      }
   }
   return INVALID_WAYPOINT_INDEX;
}

bool Bot::isOccupiedPoint (int index) {
   if (!waypoints.exists (index)) {
      return true;
   }

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & (CF_USED | CF_ALIVE)) || client.team != m_team || client.ent == ent ()) {
         continue;
      }
      auto bot = bots.getBot (client.ent);

      if (bot == this) {
         continue;
      }

      if (bot != nullptr) {
         int occupyId = util.getShootingCone (bot->ent (), pev->origin) >= 0.7f ? bot->m_prevWptIndex[0] : bot->m_currentWaypointIndex;

         if (bot != nullptr) {
            if (index == occupyId) {
               return true;
            }
         }
      }
      float length = (waypoints[index].origin - client.origin).lengthSq ();

      if (length < cr::clamp (waypoints[index].radius, cr::square (32.0f), cr::square (90.0f))) {
         return true;
      }
   }
   return false;
}

edict_t *Bot::getNearestButton (const char *targetName) {
   // this function tries to find nearest to current bot button, and returns pointer to
   // it's entity, also here must be specified the target, that button must open.

   if (util.isEmptyStr (targetName)) {
      return nullptr;
   }
   float nearestDistance = 99999.0f;
   edict_t *searchEntity = nullptr, *foundEntity = nullptr;

   // find the nearest button which can open our target
   while (!game.isNullEntity (searchEntity = engfuncs.pfnFindEntityByString (searchEntity, "target", targetName))) {
      const Vector &pos = game.getAbsPos (searchEntity);

      // check if this place safe
      if (!isDeadlyMove (pos)) {
         float distance = (pev->origin - pos).lengthSq ();

         // check if we got more close button
         if (distance <= nearestDistance) {
            nearestDistance = distance;
            foundEntity = searchEntity;
         }
      }
   }
   return foundEntity;
}