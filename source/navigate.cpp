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

int Bot::findBestGoal () {

   // chooses a destination (goal) node for a bot
   if (!bots.isBombPlanted () && m_team == Team::Terrorist && game.mapIs (MapFlags::Demolition)) {
      edict_t *pent = nullptr;

      while (!game.isNullEntity (pent = engfuncs.pfnFindEntityByString (pent, "classname", "weaponbox"))) {
         if (strcmp (STRING (pent->v.model), "models/w_backpack.mdl") == 0) {
            int index = graph.getNearest (game.getAbsPos (pent));

            if (graph.exists (index)) {
               return m_loosedBombNodeIndex = index;
            }
            break;
         }
      }

      // forcing terrorist bot to not move to another bomb spot
      if (m_inBombZone && !m_hasProgressBar && m_hasC4) {
         return graph.getNearest (pev->origin, 768.0f, NodeFlag::Goal);
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

   IntArray *offensiveNodes = nullptr;
   IntArray *defensiveNodes = nullptr;

   switch (m_team) {
   case Team::Terrorist:
      offensiveNodes = &graph.m_ctPoints;
      defensiveNodes = &graph.m_terrorPoints;
      break;

   case Team::CT:
   default:
      offensiveNodes = &graph.m_terrorPoints;
      defensiveNodes = &graph.m_ctPoints;
      break;
   }

   // terrorist carrying the C4?
   if (m_hasC4 || m_isVIP) {
      tactic = 3;
      return findGoalPost (tactic, defensiveNodes, offensiveNodes);
   }
   else if (m_team == Team::CT && hasHostage ()) {
      tactic = 2;
      offensiveNodes = &graph.m_rescuePoints;

      return findGoalPost (tactic, defensiveNodes, offensiveNodes);
   }

   offensive = m_agressionLevel * 100.0f;
   defensive = m_fearLevel * 100.0f;

   if (game.mapIs (MapFlags::Assassination | MapFlags::HostageRescue)) {
      if (m_team == Team::Terrorist) {
         defensive += 25.0f;
         offensive -= 25.0f;
      }
      else if (m_team == Team::CT) {
         // on hostage maps force more bots to save hostages
         if (game.mapIs (MapFlags::HostageRescue)) {
            defensive -= 25.0f - m_difficulty * 0.5f;
            offensive += 25.0f + m_difficulty * 5.0f;
         }
         else {
            defensive -= 25.0f;
            offensive += 25.0f;
         }
      }
   }
   else if (game.mapIs (MapFlags::Demolition) && m_team == Team::CT) {
      if (bots.isBombPlanted () && getCurrentTaskId () != Task::EscapeFromBomb && !graph.getBombPos ().empty ()) {

         if (bots.hasBombSay (BombPlantedSay::ChatSay)) {
            pushChatMessage (Chat::Plant);
            bots.clearBombSay (BombPlantedSay::ChatSay);
         }
         return m_chosenGoalIndex = findBombNode ();
      }
      defensive += 25.0f + m_difficulty * 4.0f;
      offensive -= 25.0f - m_difficulty * 0.5f;

      if (m_personality != Personality::Rusher) {
         defensive += 10.0f;
      }
   }
   else if (game.mapIs (MapFlags::Demolition) && m_team == Team::Terrorist && bots.getRoundStartTime () + 10.0f < game.timebase ()) {
      // send some terrorists to guard planted bomb
      if (!m_defendedBomb && bots.isBombPlanted () && getCurrentTaskId () != Task::EscapeFromBomb && getBombTimeleft () >= 15.0) {
         return m_chosenGoalIndex = findDefendNode (graph.getBombPos ());
      }
   }

   goalDesire = rg.float_ (0.0f, 100.0f) + offensive;
   forwardDesire = rg.float_ (0.0f, 100.0f) + offensive;
   campDesire = rg.float_ (0.0f, 100.0f) + defensive;
   backoffDesire = rg.float_ (0.0f, 100.0f) + defensive;

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
   return findGoalPost (tactic, defensiveNodes, offensiveNodes);
}

int Bot::findGoalPost (int tactic, IntArray *defensive, IntArray *offsensive) {
   int goalChoices[4] = { kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex, kInvalidNodeIndex };

   if (tactic == 0 && !(*defensive).empty ()) { // careful goal
      postprocessGoals (*defensive, goalChoices);
   }
   else if (tactic == 1 && !graph.m_campPoints.empty ()) // camp node goal
   {
      // pickup sniper points if possible for sniping bots
      if (!graph.m_sniperPoints.empty () && usesSniper ()) {
         postprocessGoals (graph.m_sniperPoints, goalChoices);
      }
      else {
         postprocessGoals (graph.m_campPoints, goalChoices);
      }
   }
   else if (tactic == 2 && !(*offsensive).empty ()) { // offensive goal
      postprocessGoals (*offsensive, goalChoices);
   }
   else if (tactic == 3 && !graph.m_goalPoints.empty ()) // map goal node
   {
      // force bomber to select closest goal, if round-start goal was reset by something
      if (m_hasC4 && bots.getRoundStartTime () + 20.0f < game.timebase ()) {
         float minDist = 9999999.0f;
         int count = 0;

         for (auto &point : graph.m_goalPoints) {
            float distance = (graph[point].origin - pev->origin).lengthSq ();

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

         for (auto &choice : goalChoices) {
            if (choice == kInvalidNodeIndex) {
               choice = graph.m_goalPoints.random ();
            }
         }
      }
      else {
         postprocessGoals (graph.m_goalPoints, goalChoices);
      }
   }

   if (!graph.exists (m_currentNodeIndex)) {
      m_currentNodeIndex = changePointIndex (findNearestNode ());
   }

   if (goalChoices[0] == kInvalidNodeIndex) {
      return m_chosenGoalIndex = rg.int_ (0, graph.length () - 1);
   }
   bool sorting = false;

   do {
      sorting = false;

      for (int i = 0; i < 3; ++i) {
         if (goalChoices[i + 1] < 0) {
            break;
         }

         if (graph.getDangerValue (m_team, m_currentNodeIndex, goalChoices[i]) < graph.getDangerValue (m_team, m_currentNodeIndex, goalChoices[i + 1])) {
            cr::swap (goalChoices[i + 1], goalChoices[i]);
            sorting = true;
         }
      }
   } while (sorting);
   return m_chosenGoalIndex = goalChoices[0]; // return and store goal
}

void Bot::postprocessGoals (const IntArray &goals, int *result) {
   // this function filters the goals, so new goal is not bot's old goal, and array of goals doesn't contains duplicate goals

   int searchCount = 0;

   for (int index = 0; index < 4; ++index) {
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

bool Bot::hasActiveGoal () {
   int goal = getTask ()->data;

   if (goal == kInvalidNodeIndex) { // not decided about a goal
      return false;
   }
   else if (goal == m_currentNodeIndex) { // no nodes needed
      return true;
   }
   else if (m_pathWalk.empty ()) { // no path calculated
      return false;
   }
   return goal == m_pathWalk.last (); // got path - check if still valid
}

void Bot::resetCollision () {
   m_collideTime = 0.0f;
   m_probeTime = 0.0f;

   m_collisionProbeBits = 0;
   m_collisionState = CollisionState::Undecided;
   m_collStateIndex = 0;

   for (auto &collideMove : m_collideMoves) {
      collideMove = 0;
   }
}

void Bot::ignoreCollision () {
   resetCollision ();

   m_prevTime = game.timebase () + 1.2f;
   m_lastCollTime = game.timebase () + 1.5f;
   m_isStuck = false;
   m_checkTerrain = false;
   m_prevSpeed = m_moveSpeed;
   m_prevOrigin = pev->origin;
}

void Bot::avoidIncomingPlayers (edict_t *touch) {
   auto task = getCurrentTaskId ();

   if (task == Task::PlantBomb || task == Task::DefuseBomb || task == Task::Camp || m_moveSpeed <= 100.0f || m_avoidTime > game.timebase ()) {
      return;
   }

   int ownId = entindex ();
   int otherId = game.indexOfPlayer (touch);

   if (ownId < otherId) {
      return;
   }
   
   if (m_avoid) {
      int currentId = game.indexOfPlayer (m_avoid);

      if (currentId < otherId) {
         return;
      }
   }
   m_avoid = touch;
   m_avoidTime = game.timebase () + 0.33f + getFrameInterval ();
}

bool Bot::doPlayerAvoidance (const Vector &normal) {
   // avoid collision entity, got it form official csbot

   if (m_avoidTime > game.timebase () && util.isAlive (m_avoid)) {
      Vector dir (cr::cosf (pev->v_angle.y), cr::sinf (pev->v_angle.y), 0.0f);
      Vector lat (-dir.y, dir.x, 0.0f);
      Vector to = Vector (m_avoid->v.origin.x - pev->origin.x, m_avoid->v.origin.y - pev->origin.y, 0.0f).normalize ();

      float toProj = to.x * dir.x + to.y * dir.y;
      float latProj = to.x * lat.x + to.y * lat.y;

      constexpr float c = 0.5f;

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
   
   if (doPlayerAvoidance (dirNormal) || m_avoidTime > game.timebase ()) {
      return;
   }
   TraceResult tr;

   // Standing still, no need to check?
   // FIXME: doesn't care for ladder movement (handled separately) should be included in some way
   if ((m_moveSpeed >= 10.0f || m_strafeSpeed >= 10.0f) && m_lastCollTime < game.timebase () && m_seeEnemyTime + 0.8f < game.timebase () && getCurrentTaskId () != Task::Attack) {

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
            if (m_collideMoves[m_collStateIndex] == CollisionState::Duck && (isOnFloor () || isInWater ())) {
               pev->button |= IN_DUCK;
            }
         }
         return;
      }

      // bot is stuck!
      Vector src;
      Vector dst;

      // not yet decided what to do?
      if (m_collisionState == CollisionState::Undecided) {
         int bits = 0;

         if (isOnLadder ()) {
            bits |= CollisionProbe::Strafe;
         }
         else if (isInWater ()) {
            bits |= (CollisionProbe::Jump | CollisionProbe::Strafe);
         }
         else {
            bits |= (CollisionProbe::Strafe | (m_jumpStateTimer < game.timebase () ? CollisionProbe::Jump : 0));
         }

         // collision check allowed if not flying through the air
         if (isOnFloor () || isOnLadder () || isInWater ()) {
            int state[kMaxCollideMoves * 2 + 1];
            int i = 0;

            // first 4 entries hold the possible collision states
            state[i++] = CollisionState::StrafeLeft;
            state[i++] = CollisionState::StrafeRight;
            state[i++] = CollisionState::Jump;
            state[i++] = CollisionState::Duck;

            if (bits & CollisionProbe::Strafe) {
               state[i] = 0;
               state[i + 1] = 0;

               // to start strafing, we have to first figure out if the target is on the left side or right side
               game.makeVectors (m_moveAngles);

               Vector dirToPoint = (pev->origin - m_destOrigin).normalize2d ();
               Vector rightSide = game.vec.right.normalize2d ();

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

               game.testHull (src, dst, TraceIgnore::Monsters, head_hull, ent (), &tr);

               if (tr.flFraction != 1.0f) {
                  blockedRight = true;
               }
               src = pev->origin - game.vec.right * 32.0f;
               dst = src + testDir * 32.0f;

               game.testHull (src, dst, TraceIgnore::Monsters, head_hull, ent (), &tr);

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
               ++i;

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
            if (bits & CollisionProbe::Jump) {
               state[i] = 0;

               if (canJumpUp (dirNormal)) {
                  state[i] += 10;
               }

               if (m_destOrigin.z >= pev->origin.z + 18.0f) {
                  state[i] += 5;
               }

               if (seesEntity (m_destOrigin)) {
                  game.makeVectors (m_moveAngles);

                  src = getEyesPos ();
                  src = src + game.vec.right * 15.0f;

                  game.testLine (src, m_destOrigin, TraceIgnore::Everything, ent (), &tr);

                  if (tr.flFraction >= 1.0f) {
                     src = getEyesPos ();
                     src = src - game.vec.right * 15.0f;

                     game.testLine (src, m_destOrigin, TraceIgnore::Everything, ent (), &tr);

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
               game.testLine (src, dst, TraceIgnore::Everything, ent (), &tr);

               if (tr.flFraction != 1.0f) {
                  state[i] += 10;
               }
            }
            else {
               state[i] = 0;
            }
            ++i;

#if 0
            if (bits & CollisionProbe::Duck)
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
            ++i;

            // weighted all possible moves, now sort them to start with most probable
            bool sorting = false;

            do {
               sorting = false;
               for (i = 0; i < 3; ++i) {
                  if (state[i + kMaxCollideMoves] < state[i + kMaxCollideMoves + 1]) {
                     cr::swap (state[i], state[i + 1]);
                     cr::swap (state[i + kMaxCollideMoves], state[i + kMaxCollideMoves + 1]);

                     sorting = true;
                  }
               }
            } while (sorting);

            for (i = 0; i < kMaxCollideMoves; ++i) {
               m_collideMoves[i] = state[i];
            }

            m_collideTime = game.timebase ();
            m_probeTime = game.timebase () + 0.5f;
            m_collisionProbeBits = bits;
            m_collisionState = CollisionState::Probing;
            m_collStateIndex = 0;
         }
      }

      if (m_collisionState == CollisionState::Probing) {
         if (m_probeTime < game.timebase ()) {
            m_collStateIndex++;
            m_probeTime = game.timebase () + 0.5f;

            if (m_collStateIndex > kMaxCollideMoves) {
               m_navTimeset = game.timebase () - 5.0f;
               resetCollision ();
            }
         }

         if (m_collStateIndex < kMaxCollideMoves) {
            switch (m_collideMoves[m_collStateIndex]) {
            case CollisionState::Jump:
               if (isOnFloor () || isInWater ()) {
                  pev->button |= IN_JUMP;
                  m_jumpStateTimer = game.timebase () + rg.float_ (0.7f, 1.5f);
               }
               break;

            case CollisionState::Duck:
               if (isOnFloor () || isInWater ()) {
                  pev->button |= IN_DUCK;
               }
               break;

            case CollisionState::StrafeLeft:
               pev->button |= IN_MOVELEFT;
               setStrafeSpeed (dirNormal, -pev->maxspeed);
               break;

            case CollisionState::StrafeRight:
               pev->button |= IN_MOVERIGHT;
               setStrafeSpeed (dirNormal, pev->maxspeed);
               break;
            }
         }
      }
   }
}

bool Bot::updateNavigation () {
   // this function is a main path navigation

   TraceResult tr, tr2;
   
   // check if we need to find a node...
   if (m_currentNodeIndex == kInvalidNodeIndex) {
      findValidNode ();
      m_pathOrigin = m_path->origin;
      
      // if wayzone radios non zero vary origin a bit depending on the body angles
      if (m_path->radius > 0) {
         game.makeVectors (Vector (pev->angles.x, cr::normalizeAngles (pev->angles.y + rg.float_ (-90.0f, 90.0f)), 0.0f));
         m_pathOrigin = m_pathOrigin + game.vec.forward * rg.float_ (0, m_path->radius);
      }
      m_navTimeset = game.timebase ();
   }
   m_destOrigin = m_pathOrigin + pev->view_ofs;

   float nodeDistance = (pev->origin - m_pathOrigin).length ();

   // this node has additional travel flags - care about them
   if (m_currentTravelFlags & PathFlag::Jump) {

      // bot is not jumped yet?
      if (!m_jumpFinished) {

         // if bot's on the ground or on the ladder we're free to jump. actually setting the correct velocity is cheating.
         // pressing the jump button gives the illusion of the bot actual jumping.
         if (isOnFloor () || isOnLadder ()) {
            pev->velocity = m_desiredVelocity;
            pev->button |= IN_JUMP;

            m_jumpFinished = true;
            m_checkTerrain = false;
            m_desiredVelocity= nullvec;
         }
      }
      else if (!yb_jasonmode.bool_ () && m_currentWeapon == Weapon::Knife && isOnFloor ()) {
         selectBestWeapon ();
      }
   }

   if (m_path->flags & NodeFlag::Ladder) {
      if (m_pathOrigin.z >= (pev->origin.z + 16.0f)) {
         m_pathOrigin = m_path->origin + Vector (0.0f, 0.0f, 16.0f);
      }
      else if (m_pathOrigin.z < pev->origin.z + 16.0f && !isOnLadder () && isOnFloor () && !(pev->flags & FL_DUCKING)) {
         m_moveSpeed = nodeDistance;

         if (m_moveSpeed < 150.0f) {
            m_moveSpeed = 150.0f;
         }
         else if (m_moveSpeed > pev->maxspeed) {
            m_moveSpeed = pev->maxspeed;
         }
      }
   }

   // special lift handling (code merged from podbotmm)
   if (m_path->flags & NodeFlag::Lift) {
      bool liftClosedDoorExists = false;

      // update node time set
      m_navTimeset = game.timebase ();

      // trace line to door
      game.testLine (pev->origin, m_path->origin, TraceIgnore::Everything, ent (), &tr2);

      if (tr2.flFraction < 1.0f && strcmp (STRING (tr2.pHit->v.classname), "func_door") == 0 && (m_liftState == LiftState::None || m_liftState == LiftState::WaitingFor || m_liftState == LiftState::LookingButtonOutside) && pev->groundentity != tr2.pHit) {
         if (m_liftState == LiftState::None) {
            m_liftState = LiftState::LookingButtonOutside;
            m_liftUsageTime = game.timebase () + 7.0f;
         }
         liftClosedDoorExists = true;
      }

      // trace line down
      game.testLine (m_path->origin, m_path->origin + Vector (0.0f, 0.0f, -50.0f), TraceIgnore::Everything, ent (), &tr);

      // if trace result shows us that it is a lift
      if (!game.isNullEntity (tr.pHit) && !m_pathWalk.empty () && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0) && !liftClosedDoorExists) {
         if ((m_liftState == LiftState::None || m_liftState == LiftState::WaitingFor || m_liftState == LiftState::LookingButtonOutside) && tr.pHit->v.velocity.z == 0.0f) {
            if (cr::abs (pev->origin.z - tr.vecEndPos.z) < 70.0f) {
               m_liftEntity = tr.pHit;
               m_liftState = LiftState::EnteringIn;
               m_liftTravelPos = m_path->origin;
               m_liftUsageTime = game.timebase () + 5.0f;
            }
         }
         else if (m_liftState == LiftState::TravelingBy) {
            m_liftState = LiftState::Leaving;
            m_liftUsageTime = game.timebase () + 7.0f;
         }
      }
      else if (!m_pathWalk.empty ()) // no lift found at node
      {
         if ((m_liftState == LiftState::None || m_liftState == LiftState::WaitingFor) && m_pathWalk.hasNext ()) {
            int nextNode = m_pathWalk.next ();

            if (graph.exists (nextNode) && (graph[nextNode].flags & NodeFlag::Lift)) {
               game.testLine (m_path->origin, graph[nextNode].origin, TraceIgnore::Everything, ent (), &tr);

               if (!game.isNullEntity (tr.pHit) && (strcmp (STRING (tr.pHit->v.classname), "func_door") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_plat") == 0 || strcmp (STRING (tr.pHit->v.classname), "func_train") == 0)) {
                  m_liftEntity = tr.pHit;
               }
            }
            m_liftState = LiftState::LookingButtonOutside;
            m_liftUsageTime = game.timebase () + 15.0f;
         }
      }

      // bot is going to enter the lift
      if (m_liftState == LiftState::EnteringIn) {
         m_destOrigin = m_liftTravelPos;

         // check if we enough to destination
         if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AimFlags::Nav;

            resetCollision ();

            // need to wait our following teammate ?
            bool needWaitForTeammate = false;

            // if some bot is following a bot going into lift - he should take the same lift to go
            for (const auto &bot : bots) {
               if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != ent () || bot->getCurrentTaskId () != Task::FollowUser) {
                  continue;
               }

               if (bot->pev->groundentity == m_liftEntity && bot->isOnFloor ()) {
                  break;
               }

               bot->m_liftEntity = m_liftEntity;
               bot->m_liftState = LiftState::EnteringIn;
               bot->m_liftTravelPos = m_liftTravelPos;

               needWaitForTeammate = true;
            }

            if (needWaitForTeammate) {
               m_liftState = LiftState::WaitingForTeammates;
               m_liftUsageTime = game.timebase () + 8.0f;
            }
            else {
               m_liftState = LiftState::LookingButtonInside;
               m_liftUsageTime = game.timebase () + 10.0f;
            }
         }
      }

      // bot is waiting for his teammates
      if (m_liftState == LiftState::WaitingForTeammates) {
         // need to wait our following teammate ?
         bool needWaitForTeammate = false;

         for (const auto &bot : bots) {
            if (!bot->m_notKilled || bot->m_team != m_team || bot->m_targetEntity != ent () || bot->getCurrentTaskId () != Task::FollowUser || bot->m_liftEntity != m_liftEntity) {
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
               m_aimFlags |= AimFlags::Nav;

               resetCollision ();
            }
         }

         // else we need to look for button
         if (!needWaitForTeammate || m_liftUsageTime < game.timebase ()) {
            m_liftState = LiftState::LookingButtonInside;
            m_liftUsageTime = game.timebase () + 10.0f;
         }
      }

      // bot is trying to find button inside a lift
      if (m_liftState == LiftState::LookingButtonInside) {
         edict_t *button = lookupButton (STRING (m_liftEntity->v.targetname));

         // got a valid button entity ?
         if (!game.isNullEntity (button) && pev->groundentity == m_liftEntity && m_buttonPushTime + 1.0f < game.timebase () && m_liftEntity->v.velocity.z == 0.0f && isOnFloor ()) {
            m_pickupItem = button;
            m_pickupType = Pickup::Button;

            m_navTimeset = game.timebase ();
         }
      }

      // is lift activated and bot is standing on it and lift is moving ?
      if (m_liftState == LiftState::LookingButtonInside || m_liftState == LiftState::EnteringIn || m_liftState == LiftState::WaitingForTeammates || m_liftState == LiftState::WaitingFor) {
         if (pev->groundentity == m_liftEntity && m_liftEntity->v.velocity.z != 0.0f && isOnFloor () && ((graph[m_previousNodes[0]].flags & NodeFlag::Lift) || !game.isNullEntity (m_targetEntity))) {
            m_liftState = LiftState::TravelingBy;
            m_liftUsageTime = game.timebase () + 14.0f;

            if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = game.timebase ();
               m_aimFlags |= AimFlags::Nav;

               resetCollision ();
            }
         }
      }

      // bots is currently moving on lift
      if (m_liftState == LiftState::TravelingBy) {
         m_destOrigin = Vector (m_liftTravelPos.x, m_liftTravelPos.y, pev->origin.z);

         if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AimFlags::Nav;

            resetCollision ();
         }
      }

      // need to find a button outside the lift
      if (m_liftState == LiftState::LookingButtonOutside) {

         // button has been pressed, lift should come
         if (m_buttonPushTime + 8.0f >= game.timebase ()) {
            if (graph.exists (m_previousNodes[0])) {
               m_destOrigin = graph[m_previousNodes[0]].origin;
            }
            else {
               m_destOrigin = pev->origin;
            }

            if ((pev->origin - m_destOrigin).lengthSq () < 225.0f) {
               m_moveSpeed = 0.0f;
               m_strafeSpeed = 0.0f;

               m_navTimeset = game.timebase ();
               m_aimFlags |= AimFlags::Nav;

               resetCollision ();
            }
         }
         else if (!game.isNullEntity (m_liftEntity)) {
            edict_t *button = lookupButton (STRING (m_liftEntity->v.targetname));

            // if we got a valid button entity
            if (!game.isNullEntity (button)) {
               // lift is already used ?
               bool liftUsed = false;

               // iterate though clients, and find if lift already used
               for (const auto &client : util.getClients ()) {
                  if (!(client.flags & ClientFlags::Used) || !(client.flags & ClientFlags::Alive) || client.team != m_team || client.ent == ent () || game.isNullEntity (client.ent->v.groundentity)) {
                     continue;
                  }

                  if (client.ent->v.groundentity == m_liftEntity) {
                     liftUsed = true;
                     break;
                  }
               }

               // lift is currently used
               if (liftUsed) {
                  if (graph.exists (m_previousNodes[0])) {
                     m_destOrigin = graph[m_previousNodes[0]].origin;
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
                  m_pickupType = Pickup::Button;
                  m_liftState = LiftState::WaitingFor;

                  m_navTimeset = game.timebase ();
                  m_liftUsageTime = game.timebase () + 20.0f;
               }
            }
            else {
               m_liftState = LiftState::WaitingFor;
               m_liftUsageTime = game.timebase () + 15.0f;
            }
         }
      }

      // bot is waiting for lift
      if (m_liftState == LiftState::WaitingFor) {
         if (graph.exists (m_previousNodes[0])) {
            if (!(graph[m_previousNodes[0]].flags & NodeFlag::Lift)) {
               m_destOrigin = graph[m_previousNodes[0]].origin;
            }
            else if (graph.exists (m_previousNodes[1])) {
               m_destOrigin = graph[m_previousNodes[1]].origin;
            }
         }

         if ((pev->origin - m_destOrigin).lengthSq () < 100.0f) {
            m_moveSpeed = 0.0f;
            m_strafeSpeed = 0.0f;

            m_navTimeset = game.timebase ();
            m_aimFlags |= AimFlags::Nav;

            resetCollision ();
         }
      }

      // if bot is waiting for lift, or going to it
      if (m_liftState == LiftState::WaitingFor || m_liftState == LiftState::EnteringIn) {
         // bot fall down somewhere inside the lift's groove :)
         if (pev->groundentity != m_liftEntity && graph.exists (m_previousNodes[0])) {
            if ((graph[m_previousNodes[0]].flags & NodeFlag::Lift) && (m_path->origin.z - pev->origin.z) > 50.0f && (graph[m_previousNodes[0]].origin.z - pev->origin.z) > 50.0f) {
               m_liftState = LiftState::None;
               m_liftEntity = nullptr;
               m_liftUsageTime = 0.0f;

               clearSearchNodes ();
               findBestNearestNode ();

               if (graph.exists (m_previousNodes[2])) {
                  findPath (m_currentNodeIndex, m_previousNodes[2], FindPath::Fast);
               }
               return false;
            }
         }
      }
   }

   if (!game.isNullEntity (m_liftEntity) && !(m_path->flags & NodeFlag::Lift)) {
      if (m_liftState == LiftState::TravelingBy) {
         m_liftState = LiftState::Leaving;
         m_liftUsageTime = game.timebase () + 10.0f;
      }
      if (m_liftState == LiftState::Leaving && m_liftUsageTime < game.timebase () && pev->groundentity != m_liftEntity) {
         m_liftState = LiftState::None;
         m_liftUsageTime = 0.0f;

         m_liftEntity = nullptr;
      }
   }

   if (m_liftUsageTime < game.timebase () && m_liftUsageTime != 0.0f) {
      m_liftEntity = nullptr;
      m_liftState = LiftState::None;
      m_liftUsageTime = 0.0f;

      clearSearchNodes ();

      if (graph.exists (m_previousNodes[0])) {
         if (!(graph[m_previousNodes[0]].flags & NodeFlag::Lift)) {
            changePointIndex (m_previousNodes[0]);
         }
         else {
            findBestNearestNode ();
         }
      }
      else {
         findBestNearestNode ();
      }
      return false;
   }

   // check if we are going through a door...
   if (game.mapIs (MapFlags::HasDoors)) {
      game.testLine (pev->origin, m_pathOrigin, TraceIgnore::Monsters, ent (), &tr);

      if (!game.isNullEntity (tr.pHit) && game.isNullEntity (m_liftEntity) && strncmp (STRING (tr.pHit->v.classname), "func_door", 9) == 0) {
         // if the door is near enough...
         if ((game.getAbsPos (tr.pHit) - pev->origin).lengthSq () < 2500.0f) {
            ignoreCollision (); // don't consider being stuck

            if (rg.chance (50)) {
               // do not use door directrly under xash, or we will get failed assert in gamedll code
               if (game.is (GameFlags::Xash3D)) {
                  pev->button |= IN_USE;
               }
               else {
                  MDLL_Use (tr.pHit, ent ()); // also 'use' the door randomly
               }
            }
         }

         // make sure we are always facing the door when going through it
         m_aimFlags &= ~(AimFlags::LastEnemy | AimFlags::PredictPath);
         m_canChooseAimDirection = false;

         edict_t *button = lookupButton (STRING (tr.pHit->v.targetname));

         // check if we got valid button
         if (!game.isNullEntity (button)) {
            m_pickupItem = button;
            m_pickupType = Pickup::Button;

            m_navTimeset = game.timebase ();
         }

         // if bot hits the door, then it opens, so wait a bit to let it open safely
         if (pev->velocity.length2d () < 2 && m_timeDoorOpen < game.timebase ()) {
            startTask (Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.timebase () + 0.5f, false);
            m_timeDoorOpen = game.timebase () + 1.0f; // retry in 1 sec until door is open

            edict_t *pent = nullptr;

            if (m_tryOpenDoor++ > 2 && util.findNearestPlayer (reinterpret_cast <void **> (&pent), ent (), 256.0f, false, false, true, true, false)) {
               m_seeEnemyTime = game.timebase () - 0.5f;

               m_states |= Sense::SeeingEnemy;
               m_aimFlags |= AimFlags::Enemy;

               m_lastEnemy = pent;
               m_enemy = pent;
               m_lastEnemyOrigin = pent->v.origin;

               m_tryOpenDoor = 0;
            }
            else {
               m_tryOpenDoor = 0;
            }
         }
      }
   }
   float desiredDistance = 0.0f;

   // initialize the radius for a special node type, where the node is considered to be reached
   if (m_path->flags & NodeFlag::Lift) {
      desiredDistance = 50.0f;
   }
   else if ((pev->flags & FL_DUCKING) || (m_path->flags & NodeFlag::Goal)) {
      desiredDistance = 25.0f;
   }
   else if (m_path->flags & NodeFlag::Ladder) {
      desiredDistance = 15.0f;
   }
   else if (m_currentTravelFlags & PathFlag::Jump) {
      desiredDistance = 0.0f;
   }
   else if (isOccupiedPoint (m_currentNodeIndex)) {
      desiredDistance = 120.0f;
   }
   else {
      desiredDistance = m_path->radius;
   }

   // check if node has a special travelflag, so they need to be reached more precisely
   for (const auto &link : m_path->links) {
      if (link.flags != 0) {
         desiredDistance = 0.0f;
         break;
      }
   }

   // needs precise placement - check if we get past the point
   if (desiredDistance < 22.0f && nodeDistance < 30.0f && (pev->origin + (pev->velocity * getFrameInterval ()) - m_pathOrigin).lengthSq () > cr::square (nodeDistance)) {
      desiredDistance = nodeDistance + 1.0f;
   }

   if (nodeDistance < desiredDistance) {

      // did we reach a destination node?
      if (getTask ()->data == m_currentNodeIndex) {
         if (m_chosenGoalIndex != kInvalidNodeIndex) {

            // add goal values
            int goalValue = graph.getDangerValue (m_team, m_chosenGoalIndex, m_currentNodeIndex);
            int addedValue = static_cast <int> (pev->health * 0.5f + m_goalValue * 0.5f);

            goalValue = cr::clamp (goalValue + addedValue, -kMaxPracticeGoalValue, kMaxPracticeGoalValue);

            // update the practice for team
            graph.setDangerValue (m_team, m_chosenGoalIndex, m_currentNodeIndex, goalValue);
         }
         return true;
      }
      else if (m_pathWalk.empty ()) {
         return false;
      }
      int taskTarget = getTask ()->data;

      if (game.mapIs (MapFlags::Demolition) && bots.isBombPlanted () && m_team == Team::CT && getCurrentTaskId () != Task::EscapeFromBomb && taskTarget != kInvalidNodeIndex) {
         const Vector &bombOrigin = isBombAudible ();

         // bot within 'hearable' bomb tick noises?
         if (!bombOrigin.empty ()) {
            float distance = (bombOrigin - graph[taskTarget].origin).length ();

            if (distance > 512.0f) {
               if (rg.chance (50) && !graph.isVisited (taskTarget)) {
                  pushRadioMessage (Radio::SectorClear);
               }
               graph.setVisited (taskTarget); // doesn't hear so not a good goal
            }
         }
         else {
            if (rg.chance (50) && !graph.isVisited (taskTarget)) {
               pushRadioMessage (Radio::SectorClear);
            }
            graph.setVisited (taskTarget); // doesn't hear so not a good goal
         }
      }
      advanceMovement (); // do the actual movement checking
   }
   return false;
}

void Bot::findShortestPath (int srcIndex, int destIndex) {
   // this function finds the shortest path from source index to destination index

   if (!graph.exists (srcIndex)){
      logger.error ("Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }
   else if (!graph.exists (destIndex)) {
      logger.error ("Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }
   clearSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   m_pathWalk.push (srcIndex);

   while (srcIndex != destIndex) {
      srcIndex = (graph.m_matrix.data () + (srcIndex * graph.length ()) + destIndex)->index;

      if (srcIndex < 0) {
         m_prevGoalIndex = kInvalidNodeIndex;
         getTask ()->data = kInvalidNodeIndex;

         return;
      }
      m_pathWalk.push (srcIndex);
   }
}

void Bot::findPath (int srcIndex, int destIndex, FindPath pathType /*= FindPath::Fast */) {
   // this function finds a path from srcIndex to destIndex

   // least kills and number of nodes to goal for a team
   auto gfunctionKillsDist = [] (int team, int currentIndex, int parentIndex) -> float {
      if (parentIndex == kInvalidNodeIndex) {
         return 0.0f;
      }
      auto cost = static_cast <float> (graph.getDangerDamage (team, currentIndex, currentIndex) + graph.getHighestDamageForTeam (team));
      Path &current = graph[currentIndex];

      for (auto &neighbour : current.links) {
         if (neighbour.index != kInvalidNodeIndex) {
            cost += static_cast <float> (graph.getDangerDamage (team, neighbour.index, neighbour.index));
         }
      }

      if (current.flags & NodeFlag::Crouch) {
         cost *= 1.5f;
      }
      return cost;
   };

   // least kills and number of nodes to goal for a team
   auto gfunctionKillsDistCTWithHostage = [&gfunctionKillsDist] (int team, int currentIndex, int parentIndex) -> float {
      Path &current = graph[currentIndex];

      if (current.flags & NodeFlag::NoHostage) {
         return 65355.0f;
      }
      else if (current.flags & (NodeFlag::Crouch | NodeFlag::Ladder)) {
         return gfunctionKillsDist (team, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionKillsDist (team, currentIndex, parentIndex);
   };

   // least kills to goal for a team
   auto gfunctionKills = [] (int team, int currentIndex, int) -> float {
      auto cost = static_cast <float> (graph.getDangerDamage (team, currentIndex, currentIndex));
      Path &current = graph[currentIndex];

      for (auto &neighbour : current.links) {
         if (neighbour.index != kInvalidNodeIndex) {
            cost += static_cast <float> (graph.getDangerDamage (team, neighbour.index, neighbour.index));
         }
      }

      if (current.flags & NodeFlag::Crouch) {
         cost *= 1.5f;
      }
      return cost + 0.5f;
   };

   // least kills to goal for a team
   auto gfunctionKillsCTWithHostage = [&gfunctionKills] (int team, int currentIndex, int parentIndex) -> float {
      if (parentIndex == kInvalidNodeIndex) {
         return 0.0f;
      }
      Path &current = graph[currentIndex];

      if (current.flags & NodeFlag::NoHostage) {
         return 65355.0f;
      }
      else if (current.flags & (NodeFlag::Crouch | NodeFlag::Ladder)) {
         return gfunctionKills (team, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionKills (team, currentIndex, parentIndex);
   };

   auto gfunctionPathDist = [] (int, int currentIndex, int parentIndex) -> float {
      if (parentIndex == kInvalidNodeIndex) {
         return 0.0f;
      }
      Path &parent = graph[parentIndex];
      Path &current = graph[currentIndex];

      for (const auto &link : parent.links) {
         if (link.index == currentIndex) {
            // we don't like ladder or crouch point
            if (current.flags & (NodeFlag::Crouch | NodeFlag::Ladder)) {
               return link.distance * 1.5f;
            }
            return static_cast <float> (link.distance);
         }
      }
      return 65355.0f;
   };

   auto gfunctionPathDistWithHostage = [&gfunctionPathDist] (int, int currentIndex, int parentIndex) -> float {
      Path &current = graph[currentIndex];

      if (current.flags & NodeFlag::NoHostage) {
         return 65355.0f;
      }
      else if (current.flags & (NodeFlag::Crouch | NodeFlag::Ladder)) {
         return gfunctionPathDist (Team::Unassigned, currentIndex, parentIndex) * 500.0f;
      }
      return gfunctionPathDist (Team::Unassigned, currentIndex, parentIndex);
   };

   // square distance heuristic
   auto hfunctionPathDist = [] (int index, int, int goalIndex) -> float {
      Path &start = graph[index];
      Path &goal = graph[goalIndex];

      float x = cr::abs (start.origin.x - goal.origin.x);
      float y = cr::abs (start.origin.y - goal.origin.y);
      float z = cr::abs (start.origin.z - goal.origin.z);

      switch (yb_debug_heuristic_type.int_ ()) {
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

         if (yb_debug_heuristic_type.int_ () == 4) {
            return 1000.0f *(cr::ceilf (euclidean) - euclidean);
         }
         return euclidean;
      }
   };

   // square distance heuristic with hostages
   auto hfunctionPathDistWithHostage = [&hfunctionPathDist] (int index, int startIndex, int goalIndex) -> float {
      if (graph[startIndex].flags & NodeFlag::NoHostage) {
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
      if (pathType == FindPath::Optimal) {
         if (game.mapIs (MapFlags::HostageRescue) && hasHostage ()) {
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
      else if (pathType == FindPath::Safe) {
         if (game.mapIs (MapFlags::HostageRescue) && hasHostage ()) {
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
         if (game.mapIs (MapFlags::HostageRescue) && hasHostage ()) {
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

   if (!graph.exists (srcIndex)) {
      logger.error ("Pathfinder source path index not valid (%d)", srcIndex);
      return;
   }
   else if (!graph.exists (destIndex)) {
      logger.error ("Pathfinder destination path index not valid (%d)", destIndex);
      return;
   }
   clearSearchNodes ();

   m_chosenGoalIndex = srcIndex;
   m_goalValue = 0.0f;

   clearRoute ();
   auto srcRoute = &m_routes[srcIndex];

   // put start node into open list
   srcRoute->g = calculate (false, m_team, srcIndex, kInvalidNodeIndex);
   srcRoute->f = srcRoute->g + calculate (true, srcIndex, srcIndex, destIndex);
   srcRoute->state = RouteState::Open;

   m_routeQue.clear ();
   m_routeQue.emplace (srcIndex, srcRoute->g);

   while (!m_routeQue.empty ()) {
      // remove the first node from the open list
      int currentIndex = m_routeQue.pop ().first;

      // safes us from bad graph...
      if (m_routeQue.length () >= kMaxRouteLength - 1) {
         logger.error ("A* Search for bots \"%s\" has tried to build path with at least %d nodes. Seems to be graph is broken.", STRING (pev->netname), m_routeQue.length ());

         // bail out to shortest path
         findShortestPath (srcIndex, destIndex);
         return;
      }

      // is the current node the goal node?
      if (currentIndex == destIndex) {

         // build the complete path
         do {
            m_pathWalk.push (currentIndex);
            currentIndex = m_routes[currentIndex].parent;

         } while (currentIndex != kInvalidNodeIndex);

         m_pathWalk.reverse ();
         return;
      }
      auto curRoute = &m_routes[currentIndex];

      if (curRoute->state != RouteState::Open) {
         continue;
      }

      // put current node into CLOSED list
      curRoute->state = RouteState::Closed;

      // now expand the current node
      for (const auto &child : graph[currentIndex].links) {
         if (child.index == kInvalidNodeIndex) {
            continue;
         }
         auto childRoute = &m_routes[child.index];

         // calculate the F value as F = G + H
         float g = curRoute->g + calculate (false, m_team, child.index, currentIndex);
         float h = calculate (true, child.index, srcIndex, destIndex);
         float f = g + h;

         if (childRoute->state == RouteState::New || childRoute->f > f) {
            // put the current child into open list
            childRoute->parent = currentIndex;
            childRoute->state = RouteState::Open;

            childRoute->g = g;
            childRoute->f = f;

            m_routeQue.emplace (child.index, g);
         }
      }
   }
   findShortestPath (srcIndex, destIndex); // A* found no path, try floyd pathfinder instead
}

void Bot::clearSearchNodes () {
   m_pathWalk.clear ();
   m_chosenGoalIndex = kInvalidNodeIndex;
}

void Bot::clearRoute () {
   m_routes.resize (graph.length ());

   for (int i = 0; i < graph.length (); ++i) {
      auto route = &m_routes[i];

      route->g = route->f = 0.0f;
      route->parent = kInvalidNodeIndex;
      route->state = RouteState::New;
   }
   m_routes.clear ();
}

int Bot::findAimingNode (const Vector &to) {
   // return the most distant node which is seen from the bot to the target and is within count

   if (m_currentNodeIndex == kInvalidNodeIndex) {
      m_currentNodeIndex = changePointIndex (findNearestNode ());
   }

   int destIndex = graph.getNearest (to);
   int bestIndex = m_currentNodeIndex;

   if (destIndex == kInvalidNodeIndex) {
      return kInvalidNodeIndex;
   }
 
   while (destIndex != m_currentNodeIndex) {
      destIndex = (graph.m_matrix.data () + (destIndex * graph.length ()) + m_currentNodeIndex)->index;

      if (destIndex < 0) {
         break;
      }

      if (graph.isVisible (m_currentNodeIndex, destIndex) && graph.isVisible (destIndex, m_currentNodeIndex)) {
         bestIndex = destIndex;
         break;
      }
   }

   if (bestIndex == m_currentNodeIndex) {
      return kInvalidNodeIndex;
   }
   return bestIndex;
}

bool Bot::findBestNearestNode () {
   // this function find a node in the near of the bot if bot had lost his path of pathfinder needs
   // to be restarted over again.

   int busy = kInvalidNodeIndex;

   float lessDist[3] = { 9999.0f, 9999.0f , 9999.0f };
   int lessIndex[3] = { kInvalidNodeIndex, kInvalidNodeIndex , kInvalidNodeIndex };

   auto &bucket = graph.getNodesInBucket (pev->origin);
   int numToSkip = cr::clamp (rg.int_ (0, 3), 0, static_cast <int> (bucket.length () / 2));

   for (const int at : bucket) {
      bool skip = !!(at == m_currentNodeIndex);

      // skip the current node, if any
      if (skip && numToSkip > 0) {
         continue;
      }

      // skip current and recent previous nodes
      for (int j = 0; j < numToSkip; ++j) {
         if (at == m_previousNodes[j]) {
            skip = true;
            break;
         }
      }

      // skip node from recent list
      if (skip) {
         continue;
      }

      // cts with hostages should not pick nodes with no hostage flag
      if (game.mapIs (MapFlags::HostageRescue) && m_team == Team::CT && (graph[at].flags & NodeFlag::NoHostage) && hasHostage ()) {
         continue;
      }

	  // check we're have link to it
      if (m_currentNodeIndex != kInvalidNodeIndex && !graph.isConnected (m_currentNodeIndex, at)) {
         continue;
      }

      // ignore non-reacheable nodes...
      if (!graph.isReachable (this, at)) {
         continue;
      }

      // check if node is already used by another bot...
      if (bots.getRoundStartTime () + 5.0f < game.timebase () && isOccupiedPoint (at)) {
         busy = at;
         continue;
      }

      // if we're still here, find some close nodes
      float distance = (pev->origin - graph[at].origin).lengthSq ();

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
   int selected = kInvalidNodeIndex;

   // now pick random one from choosen
   int index = 0;

   // choice from found
   if (lessIndex[2] != kInvalidNodeIndex) {
      index = rg.int_ (0, 2);
   }
   else if (lessIndex[1] != kInvalidNodeIndex) {
      index = rg.int_ (0, 1);
   }
   else if (lessIndex[0] != kInvalidNodeIndex) {
      index = 0;
   }
   selected = lessIndex[index];

   // if we're still have no node and have busy one (by other bot) pick it up
   if (selected == kInvalidNodeIndex && busy != kInvalidNodeIndex) {
      selected = busy;
   }

   // worst case... find atleast something
   if (selected == kInvalidNodeIndex) {
      selected = findNearestNode ();
   }
   changePointIndex (selected);

   return true;
}

float Bot::getReachTime () {
   auto task = getCurrentTaskId ();
   float estimatedTime = 0.0f; 

   switch (task) {
   case Task::Pause:
   case Task::Camp:
   case Task::Hide:
      return 0.0f;

   default:
      estimatedTime = 2.8f; // time to reach next node
   }

   // calculate 'real' time that we need to get from one node to another
   if (graph.exists (m_currentNodeIndex) && graph.exists (m_previousNodes[0])) {
      float distance = (graph[m_previousNodes[0]].origin - graph[m_currentNodeIndex].origin).length ();

      // caclulate estimated time
      if (pev->maxspeed <= 0.0f) {
         estimatedTime = 3.0f * distance / 240.0f;
      }
      else {
         estimatedTime = 3.0f * distance / pev->maxspeed;
      }
      bool longTermReachability = (m_path->flags & NodeFlag::Crouch) || (m_path->flags & NodeFlag::Ladder) || (pev->button & IN_DUCK) || (m_oldButtons & IN_DUCK);

      // check for special nodes, that can slowdown our movement
      if (longTermReachability) {
         estimatedTime *= 2.0f;
      }
      estimatedTime = cr::clamp (estimatedTime, 1.2f, longTermReachability ? 8.0f : 5.0f);
   }
   return estimatedTime;
}

void Bot::findValidNode () {
   // checks if the last node the bot was heading for is still valid

   auto trySelectNewGoal = [&] () {
      if (m_rechoiceGoalCount > 1) {
         int newGoal = findBestGoal ();

         m_prevGoalIndex = newGoal;
         m_chosenGoalIndex = newGoal;
         getTask ()->data = newGoal;

         // do path finding if it's not the current node
         if (newGoal != m_currentNodeIndex) {
            findPath (m_currentNodeIndex, newGoal, m_pathType);
         }
         m_rechoiceGoalCount = 0;
      }
      else {
         findBestNearestNode ();
         m_rechoiceGoalCount++;
      }
   };

   // if bot hasn't got a node we need a new one anyway or if time to get there expired get new one as well
   if (m_currentNodeIndex == kInvalidNodeIndex) {
      clearSearchNodes ();
      trySelectNewGoal ();

      m_pathOrigin = m_path->origin;
   }
   else if (m_navTimeset + getReachTime () < game.timebase () && game.isNullEntity (m_enemy)) {

      // increase danager for both teams
      for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {

         int damageValue = graph.getDangerDamage (team, m_currentNodeIndex, m_currentNodeIndex);
         damageValue = cr::clamp (damageValue + 100, 0, kMaxPracticeDamageValue);

         // affect nearby connected with victim nodes
         for (auto &neighbour : m_path->links) {
            if (graph.exists (neighbour.index)) {
               int neighbourValue = graph.getDangerDamage (team, neighbour.index, neighbour.index);
               neighbourValue = cr::clamp (neighbourValue + 100, 0, kMaxPracticeDamageValue);

               graph.setDangerDamage (m_team, neighbour.index, neighbour.index, neighbourValue);
            }
         }
         graph.setDangerDamage (m_team, m_currentNodeIndex, m_currentNodeIndex, damageValue);
      }

      clearSearchNodes ();
      trySelectNewGoal ();
 
      m_pathOrigin = m_path->origin;
   }
}

int Bot::changePointIndex (int index) {
   if (index == kInvalidNodeIndex) {
      return 0;
   }
   m_previousNodes[4] = m_previousNodes[3];
   m_previousNodes[3] = m_previousNodes[2];
   m_previousNodes[2] = m_previousNodes[1];
   m_previousNodes[0] = m_currentNodeIndex;

   m_currentNodeIndex = index;
   m_navTimeset = game.timebase ();

   m_path = &graph[index];
   m_pathFlags = m_path->flags;

   return m_currentNodeIndex; // to satisfy static-code analyzers
}

int Bot::findNearestNode () {
   // get the current nearest node to bot with visibility checks

   int index = kInvalidNodeIndex;
   float minimum = cr::square (1024.0f);

   auto &bucket = graph.getNodesInBucket (pev->origin);

   for (const auto at : bucket) {
      if (at == m_currentNodeIndex) {
         continue;
      }
      float distance = (graph[at].origin - pev->origin).lengthSq ();

      if (distance < minimum) {

         // if bot doing navigation, make sure node really visible and not too high
         if ((m_currentNodeIndex != kInvalidNodeIndex && graph.isVisible (m_currentNodeIndex, at)) || graph.isReachable (this, at)) {
            index = at;
            minimum = distance;
         }
      }
   }

   // worst case, take any node...
   if (index == kInvalidNodeIndex) {
      index = graph.getNearestNoBuckets (pev->origin);
   }
   return index;
}

int Bot::findBombNode () {
   // this function finds the best goal (bomb) node for CTs when searching for a planted bomb.

   auto &goals = graph.m_goalPoints;

   auto bomb = graph.getBombPos ();
   auto audible = isBombAudible ();

   if (!audible.empty ()) {
      m_bombSearchOverridden = true;
      return graph.getNearest (audible, 240.0f);
   }
   else if (goals.empty ()) {
      return graph.getNearest (bomb, 240.0f, NodeFlag::Goal); // reliability check
   }

   // take the nearest to bomb nodes instead of goal if close enough
   else if ((pev->origin - bomb).lengthSq () < cr::square (512.0f)) {
      int node = graph.getNearest (bomb, 240.0f);
      m_bombSearchOverridden = true;

      if (node != kInvalidNodeIndex) {
         return node;
      }
   }

   int goal = 0, count = 0;
   float lastDistance = 999999.0f;

   // find nearest goal node either to bomb (if "heard" or player)
   for (auto &point : goals) {
      float distance = (graph[point].origin - bomb).lengthSq ();

      // check if we got more close distance
      if (distance < lastDistance) {
         goal = point;
         lastDistance = distance;
      }
   }

   while (graph.isVisited (goal)) {
      goal = goals.random ();

      if (count++ >= static_cast <int> (goals.length ())) {
         break;
      }
   }
   return goal;
}

int Bot::findDefendNode (const Vector &origin) {
   // this function tries to find a good position which has a line of sight to a position,
   // provides enough cover point, and is far away from the defending position

   if (m_currentNodeIndex == kInvalidNodeIndex) {
      m_currentNodeIndex = changePointIndex (findNearestNode ());
   }
   TraceResult tr;

   int nodeIndex[kMaxNodeLinks];
   int minDistance[kMaxNodeLinks];

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      nodeIndex[i] = kInvalidNodeIndex;
      minDistance[i] = 128;
   }

   int posIndex = graph.getNearest (origin);
   int srcIndex = m_currentNodeIndex;

   // some of points not found, return random one
   if (srcIndex == kInvalidNodeIndex || posIndex == kInvalidNodeIndex) {
      return rg.int_ (0, graph.length () - 1);
   }

   // find the best node now
   for (int i = 0; i < graph.length (); ++i) {
      // exclude ladder & current nodes
      if ((graph[i].flags & NodeFlag::Ladder) || i == srcIndex || !graph.isVisible (i, posIndex) || isOccupiedPoint (i)) {
         continue;
      }

      // use the 'real' pathfinding distances
      int distance = graph.getPathDist (srcIndex, i);

      // skip wayponts with distance more than 512 units
      if (distance > 512) {
         continue;
      }
      game.testLine (graph[i].origin, graph[posIndex].origin, TraceIgnore::Everything, ent (), &tr);

      // check if line not hit anything
      if (tr.flFraction != 1.0f) {
         continue;
      }

      for (int j = 0; j < kMaxNodeLinks; ++j) {
         if (distance > minDistance[j]) {
            nodeIndex[j] = i;
            minDistance[j] = distance;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < kMaxNodeLinks; ++i) {
      if (nodeIndex[i] != kInvalidNodeIndex) {
         int practice = graph.getDangerDamage (m_team, nodeIndex[i], nodeIndex[i]);
         practice = (practice * 100) / graph.getHighestDamageForTeam (m_team);

         minDistance[i] = (practice * 100) / 8192;
         minDistance[i] += practice;
      }
   }
   bool sorting = false;

   // sort results nodes for farest distance
   do {
      sorting = false;

      // completely sort the data
      for (int i = 0; i < 3 && nodeIndex[i] != kInvalidNodeIndex && nodeIndex[i + 1] != kInvalidNodeIndex && minDistance[i] > minDistance[i + 1]; ++i) {
         cr::swap (nodeIndex[i], nodeIndex[i + 1]);
         cr::swap (minDistance[i], minDistance[i + 1]);

         sorting = true;
      }
   } while (sorting);

   if (nodeIndex[0] == kInvalidNodeIndex) {
      IntArray found;

      for (int i = 0; i < graph.length (); ++i) {
         if ((graph[i].origin - origin).lengthSq () <= cr::square (1248.0f) && !graph.isVisible (i, posIndex) && !isOccupiedPoint (i)) {
            found.push (i);
         }
      }

      if (found.empty ()) {
         return rg.int_ (0, graph.length () - 1); // most worst case, since there a evil error in nodes
      }
      return found.random ();
   }
   int index = 0;

   for (; index < kMaxNodeLinks; ++index) {
      if (nodeIndex[index] == kInvalidNodeIndex) {
         break;
      }
   }
   return nodeIndex[rg.int_ (0, static_cast <int> ((index - 1) * 0.5f))];
}

int Bot::findCoverNode (float maxDistance) {
   // this function tries to find a good cover node if bot wants to hide

   const float enemyMax = (m_lastEnemyOrigin - pev->origin).length ();

   // do not move to a position near to the enemy
   if (maxDistance > enemyMax) {
      maxDistance = enemyMax;
   }

   if (maxDistance < 300.0f) {
      maxDistance = 300.0f;
   }

   int srcIndex = m_currentNodeIndex;
   int enemyIndex = graph.getNearest (m_lastEnemyOrigin);

   IntArray enemies;

   int nodeIndex[kMaxNodeLinks];
   int minDistance[kMaxNodeLinks];

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      nodeIndex[i] = kInvalidNodeIndex;
      minDistance[i] = static_cast <int> (maxDistance);
   }

   if (enemyIndex == kInvalidNodeIndex) {
      return kInvalidNodeIndex;
   }

   // now get enemies neigbouring points
   for (auto &link : graph[enemyIndex].links) {
      if (link.index != kInvalidNodeIndex) {
         enemies.push (link.index);
      }
   }

   // ensure we're on valid point
   changePointIndex (srcIndex);

   // find the best node now
   for (int i = 0; i < graph.length (); ++i) {
      // exclude ladder, current node and nodes seen by the enemy
      if ((graph[i].flags & NodeFlag::Ladder) || i == srcIndex || graph.isVisible (enemyIndex, i)) {
         continue;
      }
      bool neighbourVisible = false; // now check neighbour nodes for visibility

      for (auto &enemy : enemies) {
         if (graph.isVisible (enemy, i)) {
            neighbourVisible = true;
            break;
         }
      }

      // skip visible points
      if (neighbourVisible) {
         continue;
      }

      // use the 'real' pathfinding distances
      int distances = graph.getPathDist (srcIndex, i);
      int enemyDistance = graph.getPathDist (enemyIndex, i);

      if (distances >= enemyDistance) {
         continue;
      }

      for (int j = 0; j < kMaxNodeLinks; ++j) {
         if (distances < minDistance[j]) {
            nodeIndex[j] = i;
            minDistance[j] = distances;

            break;
         }
      }
   }

   // use statistic if we have them
   for (int i = 0; i < kMaxNodeLinks; ++i) {
      if (nodeIndex[i] != kInvalidNodeIndex) {
         int practice = graph.getDangerDamage (m_team, nodeIndex[i], nodeIndex[i]);
         practice = (practice * 100) / kMaxPracticeDamageValue;

         minDistance[i] = (practice * 100) / 8192;
         minDistance[i] += practice;
      }
   }
   bool sorting;

   // sort resulting nodes for nearest distance
   do {
      sorting = false;

      for (int i = 0; i < 3 && nodeIndex[i] != kInvalidNodeIndex && nodeIndex[i + 1] != kInvalidNodeIndex && minDistance[i] > minDistance[i + 1]; ++i) {
         cr::swap (nodeIndex[i], nodeIndex[i + 1]);
         cr::swap (minDistance[i], minDistance[i + 1]);

         sorting = true;
      }
   } while (sorting);

   TraceResult tr;

   // take the first one which isn't spotted by the enemy
   for (auto &index : nodeIndex) {
      if (index != kInvalidNodeIndex) {
         game.testLine (m_lastEnemyOrigin + Vector (0.0f, 0.0f, 36.0f), graph[index].origin, TraceIgnore::Everything, ent (), &tr);

         if (tr.flFraction < 1.0f) {
            return index;
         }
      }
   }

   // if all are seen by the enemy, take the first one
   if (nodeIndex[0] != kInvalidNodeIndex) {
      return nodeIndex[0];
   }
   return kInvalidNodeIndex; // do not use random points
}

bool Bot::selectBestNextNode () {
   // this function does a realtime post processing of nodes return from the
   // pathfinder, to vary paths and find the best node on our way

   assert (!m_pathWalk.empty ());
   assert (m_pathWalk.hasNext ());

   if (!isOccupiedPoint (m_pathWalk.first ())) {
      return false;
   }

   for (auto &link : m_path->links) {
      if (link.index != kInvalidNodeIndex && graph.isConnected (link.index, m_pathWalk.next ()) && graph.isConnected (m_currentNodeIndex, link.index)) {

         // don't use ladder nodes as alternative
         if (graph[link.index].flags & NodeFlag::Ladder) {
            continue;
         }

         if (!isOccupiedPoint (link.index) && graph[link.index].origin.z <= graph[m_currentNodeIndex].origin.z + 10.0f) {
            m_pathWalk.first () = link.index;
            return true;
         }
      }
   }
   return false;
}

bool Bot::advanceMovement () {
   // advances in our pathfinding list and sets the appropiate destination origins for this bot

   findValidNode (); // check if old nodes is still reliable
   
   // no nodes from pathfinding?
   if (m_pathWalk.empty ()) {
      return false;
   }
   TraceResult tr;
   
   m_pathWalk.shift (); // advance in list
   m_currentTravelFlags = 0; // reset travel flags (jumping etc)
   
   // we're not at the end of the list?
   if (!m_pathWalk.empty ()) {
      // if in between a route, postprocess the node (find better alternatives)...
      if (m_pathWalk.hasNext () && m_pathWalk.first () != m_pathWalk.last ()) {
         selectBestNextNode ();
         m_minSpeed = pev->maxspeed;

         Task taskID = getCurrentTaskId ();

         // only if we in normal task and bomb is not planted
         if (taskID == Task::Normal && bots.getRoundMidTime () + 5.0f < game.timebase () && m_timeCamping + 5.0f < game.timebase () && !bots.isBombPlanted () && m_personality != Personality::Rusher && !m_hasC4 && !m_isVIP && m_loosedBombNodeIndex == kInvalidNodeIndex && !hasHostage ()) {
            m_campButtons = 0;

            const int nextIndex = m_pathWalk.next ();
            auto kills = static_cast <float> (graph.getDangerDamage (m_team, nextIndex, nextIndex));

            // if damage done higher than one
            if (kills > 1.0f && bots.getRoundMidTime () > game.timebase ()) {
               switch (m_personality) {
               case Personality::Normal:
                  kills *= 0.33f;
                  break;

               default:
                  kills *= 0.5f;
                  break;
               }

               if (m_baseAgressionLevel < kills && hasPrimaryWeapon ()) {
                  startTask (Task::Camp, TaskPri::Camp, kInvalidNodeIndex, game.timebase () + rg.float_ (m_difficulty * 0.5f, static_cast <float> (m_difficulty)) * 5.0f, true);
                  startTask (Task::MoveToPosition, TaskPri::MoveToPosition, findDefendNode (graph[nextIndex].origin), game.timebase () + rg.float_ (3.0f, 10.0f), true);
               }
            }
            else if (bots.canPause () && !isOnLadder () && !isInWater () && !m_currentTravelFlags && isOnFloor ()) {
               if (static_cast <float> (kills) == m_baseAgressionLevel) {
                  m_campButtons |= IN_DUCK;
               }
               else if (rg.chance (m_difficulty * 25)) {
                  m_minSpeed = getShiftSpeed ();
               }
            }

            // force terrorist bot to plant bomb
            if (m_inBombZone && !m_hasProgressBar && m_hasC4) {
               int newGoal = findBestGoal ();

               m_prevGoalIndex = newGoal;
               m_chosenGoalIndex = newGoal;

               // remember index
               getTask ()->data = newGoal;

               // do path finding if it's not the current node
               if (newGoal != m_currentNodeIndex) {
                  findPath (m_currentNodeIndex, newGoal, m_pathType);
               }
               return false;
            }
         }
      }

      if (!m_pathWalk.empty ()) {
         const int destIndex = m_pathWalk.first ();
         
         // find out about connection flags
         if (m_currentNodeIndex != kInvalidNodeIndex) {
            for (const auto &link : m_path->links) {
               if (link.index == destIndex) {
                  m_currentTravelFlags = link.flags;
                  m_desiredVelocity = link.velocity;
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
            if (m_pathWalk.hasNext ()) {
               auto nextIndex = m_pathWalk.next ();

               Path &path = graph[destIndex];
               Path &next = graph[nextIndex];

               for (const auto &link : path.links) {
                  if (link.index == nextIndex && (link.flags & PathFlag::Jump)) {
                     src = path.origin;
                     dst = next.origin;

                     jumpDistance = (src - dst).length ();
                     willJump = true;

                     break;
                  }
               }
            }

            // is there a jump node right ahead and do we need to draw out the light weapon ?
            if (willJump && m_currentWeapon != Weapon::Knife && m_currentWeapon != Weapon::Scout && !m_isReloading && !usesPistol () && (jumpDistance > 200.0f || (dst.z - 32.0f > src.z && jumpDistance > 150.0f)) && !(m_states & (Sense::SeeingEnemy | Sense::SuspectEnemy))) {
               selectWeaponByName ("weapon_knife"); // draw out the knife if we needed
            }

            // bot not already on ladder but will be soon?
            if ((graph[destIndex].flags & NodeFlag::Ladder) && !isOnLadder ()) {
               // get ladder nodes used by other (first moving) bots

               for (const auto &other : bots) {
                  // if another bot uses this ladder, wait 3 secs
                  if (other.get () != this && other->m_notKilled && other->m_currentNodeIndex == destIndex) {
                     startTask (Task::Pause, TaskPri::Pause, kInvalidNodeIndex, game.timebase () + 3.0f, false);
                     return true;
                  }
               }
            }
         }
         changePointIndex (destIndex);
      }
   }
   m_pathOrigin = m_path->origin;

   // if wayzone radius non zero vary origin a bit depending on the body angles
   if (m_path->radius > 0.0f) {
      game.makeVectors (Vector (pev->angles.x, cr::normalizeAngles (pev->angles.y + rg.float_ (-90.0f, 90.0f)), 0.0f));
      m_pathOrigin = m_pathOrigin + game.vec.forward * rg.float_ (0.0f, m_path->radius);
   }

   if (isOnLadder ()) {
      game.testLine (Vector (pev->origin.x, pev->origin.y, pev->absmin.z), m_pathOrigin, TraceIgnore::Everything, ent (), &tr);

      if (tr.flFraction < 1.0f) {
         m_pathOrigin = m_pathOrigin + (pev->origin - m_pathOrigin) * 0.5f + Vector (0.0f, 0.0f, 32.0f);
      }
   }
   m_navTimeset = game.timebase ();

   return true;
}

bool Bot::cantMoveForward (const Vector &normal, TraceResult *tr) {
   // checks if bot is blocked in his movement direction (excluding doors)

   // use some TraceLines to determine if anything is blocking the current path of the bot.

   // first do a trace from the bot's eyes forward...
   Vector src = getEyesPos ();
   Vector forward = src + normal * 24.0f;

   game.makeVectors (Vector (0.0f, pev->angles.y, 0.0f));

   auto checkDoor = [] (TraceResult *tr) {
      if (!game.mapIs (MapFlags::HasDoors)) {
         return false;
      }
      return tr->flFraction < 1.0f && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) != 0;
   };

   // trace from the bot's eyes straight forward...
   game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

   // check if the trace hit something...
   if (tr->flFraction < 1.0f) {
      if (game.mapIs (MapFlags::HasDoors) && strncmp ("func_door", STRING (tr->pHit->v.classname), 9) == 0) {
         return false;
      }
      return true; // bot's head will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder left diagonal forward to the right shoulder...
   src = getEyesPos () + Vector (0.0f, 0.0f, -16.0f) - game.vec.right * -16.0f;
   forward = getEyesPos () + Vector (0.0f, 0.0f, -16.0f) + game.vec.right * 16.0f + normal * 24.0f;

   game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

   // check if the trace hit something...
   if (checkDoor (tr)) {
      return true; // bot's body will hit something
   }

   // bot's head is clear, check at shoulder level...
   // trace from the bot's shoulder right diagonal forward to the left shoulder...
   src = getEyesPos () + Vector (0.0f, 0.0f, -16.0f) + game.vec.right * 16.0f;
   forward = getEyesPos () + Vector (0.0f, 0.0f, -16.0f) - game.vec.right * -16.0f + normal * 24.0f;

   game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

   // check if the trace hit something...
   if (checkDoor (tr)) {
      return true; // bot's body will hit something
   }

   // now check below waist
   if (pev->flags & FL_DUCKING) {
      src = pev->origin + Vector (0.0f, 0.0f, -19.0f + 19.0f);
      forward = src + Vector (0.0f, 0.0f, 10.0f) + normal * 24.0f;

      game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }
      src = pev->origin;
      forward = src + normal * 24.0f;

      game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

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
      game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

      // check if the trace hit something...
      if (checkDoor (tr)) {
         return true; // bot's body will hit something
      }

      // trace from the left waist to the right forward waist pos
      src = pev->origin + Vector (0.0f, 0.0f, -17.0f) + game.vec.right * 16.0f;
      forward = pev->origin + Vector (0.0f, 0.0f, -17.0f) - game.vec.right * -16.0f + normal * 24.0f;

      game.testLine (src, forward, TraceIgnore::Monsters, ent (), tr);

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
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }
   else {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

      if (tr.flFraction < 1.0f) {
         return false;
      }
   }

   // now check same height to one side of the bot...
   src = pev->origin + game.vec.right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = pev->origin + (-game.vec.right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 45.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return doneCanJumpUp (normal);
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

bool Bot::doneCanJumpUp (const Vector &normal) {
   // use center of the body first... maximum duck jump height is 62, so check one unit above that (63)
   Vector src = pev->origin + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   Vector dest = src + normal * 32.0f;

   TraceResult tr;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   if (tr.flFraction < 1.0f) {
      return false;
   }
   else {
      // now trace from jump height upward to check for obstructions...
      src = dest;
      dest.z = dest.z + 37.0f;

      game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

      // if trace hit something, check duckjump
      if (tr.flFraction < 1.0f) {
         return false;
      }
   }

   // now check same height to one side of the bot...
   src = pev->origin + game.vec.right * 16.0f + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = pev->origin + (-game.vec.right * 16.0f) + Vector (0.0f, 0.0f, -36.0f + 63.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at maximum jump height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now trace from jump height upward to check for obstructions...
   src = dest;
   dest.z = dest.z + 37.0f;

   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

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
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height to one side of the bot...
   src = baseHeight + game.vec.right * 16.0f;
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   if (tr.flFraction < 1.0f) {
      return false;
   }

   // now check same height on the other side of the bot...
   src = baseHeight + (-game.vec.right * 16.0f);
   dest = src + normal * 32.0f;

   // trace a line forward at duck height...
   game.testLine (src, dest, TraceIgnore::Monsters, ent (), &tr);

   // if trace hit something, return false
   return tr.flFraction > 1.0f;
}

#ifdef DEAD_CODE

bool Bot::isBlockedLeft () {
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0.0f) {
      direction = -48;
   }
   makeVectors (pev->angles);

   // do a trace to the left...
   game.TestLine (pev->origin, game.vec.forward * direction - game.vec.right * 48.0f, TraceIgnore::Monsters, ent (), &tr);

   // check if the trace hit something...
   if (game.mapIs (MapFlags::HasDoors) && tr.flFraction < 1.0f && strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0) {
      return true; // bot's body will hit something
   }
   return false;
}

bool Bot::isBlockedRight () {
   TraceResult tr;
   int direction = 48;

   if (m_moveSpeed < 0) {
      direction = -48;
   }
   makeVectors (pev->angles);

   // do a trace to the right...
   game.TestLine (pev->origin, pev->origin + game.vec.forward * direction + game.vec.right * 48.0f, TraceIgnore::Monsters, ent (), &tr);

   // check if the trace hit something...
   if (game.mapIs (MapFlags::HasDoors) && tr.flFraction < 1.0f && (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) != 0)) {
      return true; // bot's body will hit something
   }
   return false;
}

#endif

bool Bot::checkWallOnLeft () {
   TraceResult tr;
   game.makeVectors (pev->angles);

   game.testLine (pev->origin, pev->origin - game.vec.right * 40.0f, TraceIgnore::Monsters, ent (), &tr);

   // check if the trace hit something...
   if (tr.flFraction < 1.0f) {
      return true;
   }
   return false;
}

bool Bot::checkWallOnRight () {
   TraceResult tr;
   game.makeVectors (pev->angles);

   // do a trace to the right...
   game.testLine (pev->origin, pev->origin + game.vec.right * 40.0f, TraceIgnore::Monsters, ent (), &tr);

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

   Vector move ((to - botPos).yaw (), 0.0f, 0.0f);
   game.makeVectors (move);

   Vector direction = (to - botPos).normalize (); // 1 unit long
   Vector check = botPos;
   Vector down = botPos;

   down.z = down.z - 1000.0f; // straight down 1000 units

   game.testHull (check, down, TraceIgnore::Monsters, head_hull, ent (), &tr);

   if (tr.flFraction > 0.036f) { // we're not on ground anymore?
      tr.flFraction = 0.036f;
   }

   float lastHeight = tr.flFraction * 1000.0f; // height from ground
   float distance = (to - check).length (); // distance from goal

   while (distance > 16.0f) {
      check = check + direction * 16.0f; // move 10 units closer to the goal...

      down = check;
      down.z = down.z - 1000.0f; // straight down 1000 units

      game.testHull (check, down, TraceIgnore::Monsters, head_hull, ent (), &tr);

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

int Bot::findCampingDirection () {
   // find a good node to look at when camping

   if (m_currentNodeIndex == kInvalidNodeIndex) {
      m_currentNodeIndex = changePointIndex (findNearestNode ());
   }

   int count = 0, indices[3];
   float distTab[3];
   uint16 visibility[3];

   int currentNode = m_currentNodeIndex;

   for (int i = 0; i < graph.length (); ++i) {
      if (currentNode == i || !graph.isVisible (currentNode, i)) {
         continue;
      }
      Path &path = graph[i];

      if (count < 3) {
         indices[count] = i;

         distTab[count] = (pev->origin - path.origin).lengthSq ();
         visibility[count] = path.vis.crouch + path.vis.stand;

         count++;
      }
      else {
         float distance = (pev->origin - path.origin).lengthSq ();
         uint16 visBits = path.vis.crouch + path.vis.stand;

         for (int j = 0; j < 3; ++j) {
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
      return indices[rg.int_ (0, count)];
   }
   return rg.int_ (0, graph.length () - 1);
}

void Bot::updateBodyAngles () {
   // set the body angles to point the gun correctly
   pev->angles.x = -pev->v_angle.x * (1.0f / 3.0f);
   pev->angles.y = pev->v_angle.y;

   pev->angles.clampAngles ();
}

void Bot::updateLookAngles () {
   const float delta = cr::clamp (game.timebase () - m_lookUpdateTime, cr::kFloatEqualEpsilon, 0.05f);
   m_lookUpdateTime = game.timebase ();

   // adjust all body and view angles to face an absolute vector
   Vector direction = (m_lookAt - getEyesPos ()).angles ();
   direction.x *= -1.0f; // invert for engine

   direction.clampAngles ();

   // lower skilled bot's have lower aiming
   if (m_difficulty < 2) {
      updateLookAnglesNewbie (direction, delta);
      updateBodyAngles ();

      return;
   }

   if (m_difficulty > 3 && (m_aimFlags & AimFlags::Enemy) && (m_wantsToFire || usesSniper ()) && yb_whose_your_daddy.bool_ ()) {
      pev->v_angle = direction;
      updateBodyAngles ();

      return;
   }

   float accelerate = 3000.0f;
   float stiffness = 200.0f;
   float damping = 25.0f;

   if ((m_aimFlags & (AimFlags::Enemy | AimFlags::Entity | AimFlags::Grenade)) && m_difficulty > 3) {
      stiffness += 100.0f;
      damping += 5.0f;
   }
   m_idealAngles = pev->v_angle;

   float angleDiffYaw = cr::anglesDifference (direction.y, m_idealAngles.y);
   float angleDiffPitch = cr::anglesDifference (direction.x, m_idealAngles.x);

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

   updateBodyAngles ();
}

void Bot::updateLookAnglesNewbie (const Vector &direction, float delta) {
   Vector spring (13.0f, 13.0f, 0.0f);
   Vector damperCoefficient (0.22f, 0.22f, 0.0f);

   const float offset = cr::clamp (m_difficulty, 1, 4) * 25.0f;

   Vector influence = Vector (0.25f, 0.17f, 0.0f) * (100.0f - offset) / 100.f;
   Vector randomization = Vector (2.0f, 0.18f, 0.0f) * (100.0f - offset) / 100.f;

   const float noTargetRatio = 0.3f;
   const float offsetDelay = 1.2f;

   Vector stiffness;
   Vector randomize;

   m_idealAngles = direction.get2d ();
   m_idealAngles.clampAngles ();

   if (m_aimFlags & (AimFlags::Enemy | AimFlags::Entity)) {
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
         // randomize targeted location bit (slightly towards the ground)
         m_randomizedIdealAngles = m_idealAngles + Vector (rg.float_ (-randomize.x * 0.5f, randomize.x * 1.5f), rg.float_ (-randomize.y, randomize.y), 0.0f);

         // set next time to do this
         m_randomizeAnglesTime = game.timebase () + rg.float_ (0.4f, offsetDelay);
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

   const Vector &los = (moveDir - pev->origin).normalize2d ();
   float dot = los | game.vec.forward.get2d ();

   if (dot > 0.0f && !checkWallOnRight ()) {
      m_strafeSpeed = strafeSpeed;
   }
   else if (!checkWallOnLeft ()) {
      m_strafeSpeed = -strafeSpeed;
   }
}

int Bot::getNearestToPlantedBomb () {
   // this function tries to find planted c4 on the defuse scenario map and returns nearest to it node

   if (m_team != Team::Terrorist || !game.mapIs (MapFlags::Demolition)) {
      return kInvalidNodeIndex; // don't search for bomb if the player is CT, or it's not defusing bomb
   }

   edict_t *bombEntity = nullptr; // temporaly pointer to bomb

   // search the bomb on the map
   while (!game.isNullEntity (bombEntity = engfuncs.pfnFindEntityByString (bombEntity, "classname", "grenade"))) {
      if (strcmp (STRING (bombEntity->v.model) + 9, "c4.mdl") == 0) {
         int nearestIndex = graph.getNearest (game.getAbsPos (bombEntity));

         if (graph.exists (nearestIndex)) {
            return nearestIndex;
         }
         break;
      }
   }
   return kInvalidNodeIndex;
}

bool Bot::isOccupiedPoint (int index) {
   if (!graph.exists (index)) {
      return true;
   }

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & (ClientFlags::Used | ClientFlags::Alive)) || client.team != m_team || client.ent == ent ()) {
         continue;
      }
      auto bot = bots[client.ent];

      if (bot == this) {
         continue;
      }

      if (bot != nullptr) {
         int occupyId = util.getShootingCone (bot->ent (), pev->origin) >= 0.7f ? bot->m_previousNodes[0] : bot->m_currentNodeIndex;

         if (index == occupyId) {
            return true;
         }
      }
      float length = (graph[index].origin - client.origin).lengthSq ();

      if (length < cr::clamp (graph[index].radius, cr::square (64.0f), cr::square (120.0f))) {
         return true;
      }
   }
   return false;
}

edict_t *Bot::lookupButton (const char *targetName) {
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