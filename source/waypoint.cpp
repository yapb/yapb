//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <core.h>

ConVar yb_wptsubfolder ("yb_wptsubfolder", "");

ConVar yb_waypoint_autodl_host ("yb_waypoint_autodl_host", "yapb.ru");
ConVar yb_waypoint_autodl_enable ("yb_waypoint_autodl_enable", "1");

void Waypoint::init (void) {
   // this function initialize the waypoint structures..
   m_loadTries = 0;

   m_learnVelocity.nullify ();
   m_learnPosition.nullify ();
   m_lastWaypoint.nullify ();

   // have any waypoint path nodes been allocated yet?
   if (m_waypointPaths) {
      cleanupPathMemory ();
   }
   g_numWaypoints = 0;
}

void Waypoint::cleanupPathMemory (void) {
   for (int i = 0; i < g_numWaypoints && m_paths[i] != nullptr; i++) {
      delete m_paths[i];
      m_paths[i] = nullptr;
   }
}

void Waypoint::addPath (int addIndex, int pathIndex, float distance) {
   if (addIndex < 0 || addIndex >= g_numWaypoints || pathIndex < 0 || pathIndex >= g_numWaypoints || addIndex == pathIndex) {
      return;
   }
   Path *path = m_paths[addIndex];

   // don't allow paths get connected twice
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (path->index[i] == pathIndex) {
         logEntry (true, LL_WARNING, "Denied path creation from %d to %d (path already exists)", addIndex, pathIndex);
         return;
      }
   }

   // check for free space in the connection indices
   for (int16 i = 0; i < MAX_PATH_INDEX; i++) {
      if (path->index[i] == INVALID_WAYPOINT_INDEX) {
         path->index[i] = static_cast<int16> (pathIndex);
         path->distances[i] = A_abs (static_cast<int> (distance));

         logEntry (true, LL_DEFAULT, "Path added from %d to %d", addIndex, pathIndex);
         return;
      }
   }

   // there wasn't any free space. try exchanging it with a long-distance path
   int maxDistance = -9999;
   int slotID = INVALID_WAYPOINT_INDEX;

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (path->distances[i] > maxDistance) {
         maxDistance = path->distances[i];
         slotID = i;
      }
   }

   if (slotID != INVALID_WAYPOINT_INDEX) {
      logEntry (true, LL_DEFAULT, "Path added from %d to %d", addIndex, pathIndex);

      path->index[slotID] = static_cast<int16> (pathIndex);
      path->distances[slotID] = A_abs (static_cast<int> (distance));
   }
}

int Waypoint::getFarest (const Vector &origin, float maxDistance) {
   // find the farest waypoint to that Origin, and return the index to this waypoint

   int index = INVALID_WAYPOINT_INDEX;
   maxDistance = A_square (maxDistance);

   for (int i = 0; i < g_numWaypoints; i++) {
      float distance = (m_paths[i]->origin - origin).lengthSq ();

      if (distance > maxDistance) {
         index = i;
         maxDistance = distance;
      }
   }
   return index;
}

int Waypoint::getNearest (const Vector &origin, float minDistance, int flags) {
   // find the nearest waypoint to that origin and return the index

   int index = INVALID_WAYPOINT_INDEX;
   minDistance = A_square (minDistance);

   for (int i = 0; i < g_numWaypoints; i++) {
      if (flags != -1 && !(m_paths[i]->flags & flags)) {
         continue; // if flag not -1 and waypoint has no this flag, skip waypoint
      }
      float distance = (m_paths[i]->origin - origin).lengthSq ();

      if (distance < minDistance) {
         index = i;
         minDistance = distance;
      }
   }
   return index;
}

void Waypoint::searchRadius (IntArray &radiusHolder, float radius, const Vector &origin, int maxCount) {
   // returns all waypoints within radius from position

   radius = A_square (radius);

   for (int i = 0; i < g_numWaypoints; i++) {
      if (maxCount != -1 && static_cast<int> (radiusHolder.length ()) > maxCount) {
         break;
      }

      if ((m_paths[i]->origin - origin).lengthSq () < radius) {
         radiusHolder.push (i);
      }
   }
}

void Waypoint::push (int flags, const Vector &waypointOrigin) {
   if (engine.isNullEntity (g_hostEntity)) {
      return;
   }

   int index = INVALID_WAYPOINT_INDEX, i;
   float distance;

   Vector forward;
   Path *path = nullptr;

   bool placeNew = true;
   Vector newOrigin = waypointOrigin;

   if (waypointOrigin.empty ()) {
      newOrigin = g_hostEntity->v.origin;
   }

   if (bots.getBotCount () > 0) {
      bots.kickEveryone (true);
   }
   m_waypointsChanged = true;

   switch (flags) {
   case 6:
      index = getNearest (g_hostEntity->v.origin, 50.0f);

      if (index != INVALID_WAYPOINT_INDEX) {
         path = m_paths[index];

         if (!(path->flags & FLAG_CAMP)) {
            engine.centerPrint ("This is not Camping Waypoint");
            return;
         }

         makeVectors (g_hostEntity->v.v_angle);
         forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;

         path->campEndX = forward.x;
         path->campEndY = forward.y;

         // play "done" sound...
         engine.playSound (g_hostEntity, "common/wpn_hudon.wav");
      }
      return;

   case 9:
      index = getNearest (g_hostEntity->v.origin, 50.0f);

      if (index != INVALID_WAYPOINT_INDEX && m_paths[index] != nullptr) {
         distance = (m_paths[index]->origin - g_hostEntity->v.origin).length ();

         if (distance < 50.0f) {
            placeNew = false;

            path = m_paths[index];
            path->origin = (path->origin + m_learnPosition) * 0.5f;
         }
      }
      else
         newOrigin = m_learnPosition;
      break;

   case 10:
      index = getNearest (g_hostEntity->v.origin, 50.0f);

      if (index != INVALID_WAYPOINT_INDEX && m_paths[index] != nullptr) {
         distance = (m_paths[index]->origin - g_hostEntity->v.origin).length ();

         if (distance < 50.0f) {
            placeNew = false;
            path = m_paths[index];

            int connectionFlags = 0;

            for (i = 0; i < MAX_PATH_INDEX; i++) {
               connectionFlags += path->connectionFlags[i];
            }
            if (connectionFlags == 0) {
               path->origin = (path->origin + g_hostEntity->v.origin) * 0.5f;
            }
         }
      }
      break;
   }

   if (placeNew) {
      if (g_numWaypoints >= MAX_WAYPOINTS) {
         return;
      }
      index = g_numWaypoints;

      m_paths[index] = new Path;
      path = m_paths[index];

      // increment total number of waypoints
      g_numWaypoints++;
      path->pathNumber = index;
      path->flags = 0;

      // store the origin (location) of this waypoint
      path->origin = newOrigin;

      path->campEndX = 0.0f;
      path->campEndY = 0.0f;
      path->campStartX = 0.0f;
      path->campStartY = 0.0f;

      for (i = 0; i < MAX_PATH_INDEX; i++) {
         path->index[i] = INVALID_WAYPOINT_INDEX;
         path->distances[i] = 0;

         path->connectionFlags[i] = 0;
         path->connectionVelocity[i].nullify ();
      }

      // store the last used waypoint for the auto waypoint code...
      m_lastWaypoint = g_hostEntity->v.origin;
   }

   // set the time that this waypoint was originally displayed...
   m_waypointDisplayTime[index] = 0;

   if (flags == 9) {
      m_lastJumpWaypoint = index;
   }
   else if (flags == 10) {
      distance = (m_paths[m_lastJumpWaypoint]->origin - g_hostEntity->v.origin).length ();
      addPath (m_lastJumpWaypoint, index, distance);

      for (i = 0; i < MAX_PATH_INDEX; i++) {
         if (m_paths[m_lastJumpWaypoint]->index[i] == index) {
            m_paths[m_lastJumpWaypoint]->connectionFlags[i] |= PATHFLAG_JUMP;
            m_paths[m_lastJumpWaypoint]->connectionVelocity[i] = m_learnVelocity;

            break;
         }
      }

      calculatePathRadius (index);
      return;
   }

   if (path == nullptr) {
      return;
   }

   if (g_hostEntity->v.flags & FL_DUCKING) {
      path->flags |= FLAG_CROUCH; // set a crouch waypoint
   }

   if (g_hostEntity->v.movetype == MOVETYPE_FLY) {
      path->flags |= FLAG_LADDER;
      makeVectors (g_hostEntity->v.v_angle);

      forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;
      path->campStartY = forward.y;
   }
   else if (m_isOnLadder) {
      path->flags |= FLAG_LADDER;
   }

   switch (flags) {
   case 1:
      path->flags |= FLAG_CROSSING;
      path->flags |= FLAG_TF_ONLY;
      break;

   case 2:
      path->flags |= FLAG_CROSSING;
      path->flags |= FLAG_CF_ONLY;
      break;

   case 3:
      path->flags |= FLAG_NOHOSTAGE;
      break;

   case 4:
      path->flags |= FLAG_RESCUE;
      break;

   case 5:
      path->flags |= FLAG_CROSSING;
      path->flags |= FLAG_CAMP;

      makeVectors (g_hostEntity->v.v_angle);
      forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;

      path->campStartX = forward.x;
      path->campStartY = forward.y;
      break;

   case 100:
      path->flags |= FLAG_GOAL;
      break;
   }

   // Ladder waypoints need careful connections
   if (path->flags & FLAG_LADDER) {
      float minDistance = 9999.0f;
      int destIndex = INVALID_WAYPOINT_INDEX;

      TraceResult tr;

      // calculate all the paths to this new waypoint
      for (i = 0; i < g_numWaypoints; i++) {
         if (i == index) {
            continue; // skip the waypoint that was just added
         }

         // other ladder waypoints should connect to this
         if (m_paths[i]->flags & FLAG_LADDER) {
            // check if the waypoint is reachable from the new one
            engine.testLine (newOrigin, m_paths[i]->origin, TRACE_IGNORE_MONSTERS, g_hostEntity, &tr);

            if (tr.flFraction == 1.0f && A_abs (newOrigin.x - m_paths[i]->origin.x) < 64.0f && A_abs (newOrigin.y - m_paths[i]->origin.y) < 64.0f && A_abs (newOrigin.z - m_paths[i]->origin.z) < g_autoPathDistance) {
               distance = (m_paths[i]->origin - newOrigin).length ();

               addPath (index, i, distance);
               addPath (i, index, distance);
            }
         }
         else {
            // check if the waypoint is reachable from the new one
            if (isNodeReacheable (newOrigin, m_paths[i]->origin) || isNodeReacheable (m_paths[i]->origin, newOrigin)) {
               distance = (m_paths[i]->origin - newOrigin).length ();

               if (distance < minDistance) {
                  destIndex = i;
                  minDistance = distance;
               }
            }
         }
      }

      if (destIndex > INVALID_WAYPOINT_INDEX && destIndex < g_numWaypoints) {
         // check if the waypoint is reachable from the new one (one-way)
         if (isNodeReacheable (newOrigin, m_paths[destIndex]->origin)) {
            distance = (m_paths[destIndex]->origin - newOrigin).length ();
            addPath (index, destIndex, distance);
         }

         // check if the new one is reachable from the waypoint (other way)
         if (isNodeReacheable (m_paths[destIndex]->origin, newOrigin)) {
            distance = (m_paths[destIndex]->origin - newOrigin).length ();
            addPath (destIndex, index, distance);
         }
      }
   }
   else {
      // calculate all the paths to this new waypoint
      for (i = 0; i < g_numWaypoints; i++) {
         if (i == index) {
            continue; // skip the waypoint that was just added
         }

         // check if the waypoint is reachable from the new one (one-way)
         if (isNodeReacheable (newOrigin, m_paths[i]->origin)) {
            distance = (m_paths[i]->origin - newOrigin).length ();
            addPath (index, i, distance);
         }

         // check if the new one is reachable from the waypoint (other way)
         if (isNodeReacheable (m_paths[i]->origin, newOrigin)) {
            distance = (m_paths[i]->origin - newOrigin).length ();
            addPath (i, index, distance);
         }
      }
   }
   engine.playSound (g_hostEntity, "weapons/xbow_hit1.wav");
   calculatePathRadius (index); // calculate the wayzone of this waypoint
}

void Waypoint::erase (void) {
   m_waypointsChanged = true;

   if (g_numWaypoints < 1) {
      return;
   }

   if (bots.getBotCount () > 0) {
      bots.kickEveryone (true);
   }
   int index = getNearest (g_hostEntity->v.origin, 50.0f);

   if (index < 0) {
      return;
   }

   Path *path = nullptr;
   assert (m_paths[index] != nullptr);

   int i, j;

   for (i = 0; i < g_numWaypoints; i++) // delete all references to Node
   {
      path = m_paths[i];

      for (j = 0; j < MAX_PATH_INDEX; j++) {
         if (path->index[j] == index) {
            path->index[j] = INVALID_WAYPOINT_INDEX; // unassign this path
            path->connectionFlags[j] = 0;
            path->distances[j] = 0;
            path->connectionVelocity[j].nullify ();
         }
      }
   }

   for (i = 0; i < g_numWaypoints; i++) {
      path = m_paths[i];

      if (path->pathNumber > index) { // if pathnumber bigger than deleted node...
         path->pathNumber--;
      }

      for (j = 0; j < MAX_PATH_INDEX; j++) {
         if (path->index[j] > index) {
            path->index[j]--;
         }
      }
   }

   // free deleted node
   delete m_paths[index];
   m_paths[index] = nullptr;

   // rotate path array down
   for (i = index; i < g_numWaypoints - 1; i++) {
      m_paths[i] = m_paths[i + 1];
   }
   g_numWaypoints--;
   m_waypointDisplayTime[index] = 0;

   engine.playSound (g_hostEntity, "weapons/mine_activate.wav");
}

void Waypoint::toggleFlags (int toggleFlag) {
   // this function allow manually changing flags

   int index = getNearest (g_hostEntity->v.origin, 50.0f);

   if (index != INVALID_WAYPOINT_INDEX) {
      if (m_paths[index]->flags & toggleFlag) {
         m_paths[index]->flags &= ~toggleFlag;
      }
      else if (!(m_paths[index]->flags & toggleFlag)) {
         if (toggleFlag == FLAG_SNIPER && !(m_paths[index]->flags & FLAG_CAMP)) {
            logEntry (true, LL_ERROR, "Cannot assign sniper flag to waypoint #%d. This is not camp waypoint", index);
            return;
         }
         m_paths[index]->flags |= toggleFlag;
      }

      // play "done" sound...
      engine.playSound (g_hostEntity, "common/wpn_hudon.wav");
   }
}

void Waypoint::setRadius (int radius) {
   // this function allow manually setting the zone radius

   int index = getNearest (g_hostEntity->v.origin, 50.0f);

   if (index != INVALID_WAYPOINT_INDEX) {
      m_paths[index]->radius = static_cast<float> (radius);

      // play "done" sound...
      engine.playSound (g_hostEntity, "common/wpn_hudon.wav");
   }
}

bool Waypoint::isConnected (int pointA, int pointB) {
   // this function checks if waypoint A has a connection to waypoint B

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (m_paths[pointA]->index[i] == pointB) {
         return true;
      }
   }
   return false;
}

int Waypoint::getFacingIndex (void) {
   // this function finds waypoint the user is pointing at.

   int pointedIndex = INVALID_WAYPOINT_INDEX;
   float viewCone[3] = {0.0f, 0.0f, 0.0f};

   // find the waypoint the user is pointing at
   for (int i = 0; i < g_numWaypoints; i++) {
      if ((m_paths[i]->origin - g_hostEntity->v.origin).lengthSq () > 250000.0f) {
         continue;
      }
      // get the current view cone
      viewCone[0] = getShootingConeDeviation (g_hostEntity, m_paths[i]->origin);
      Vector bound = m_paths[i]->origin - Vector (0.0f, 0.0f, (m_paths[i]->flags & FLAG_CROUCH) ? 8.0f : 15.0f);

      // get the current view cone
      viewCone[1] = getShootingConeDeviation (g_hostEntity, bound);
      bound = m_paths[i]->origin + Vector (0.0f, 0.0f, (m_paths[i]->flags & FLAG_CROUCH) ? 8.0f : 15.0f);

      // get the current view cone
      viewCone[2] = getShootingConeDeviation (g_hostEntity, bound);

      // check if we can see it
      if (viewCone[0] < 0.998f && viewCone[1] < 0.997f && viewCone[2] < 0.997f) {
         continue;
      }
      pointedIndex = i;
   }
   return pointedIndex;
}

void Waypoint::pathCreate (char dir) {
   // this function allow player to manually create a path from one waypoint to another

   int nodeFrom = getNearest (g_hostEntity->v.origin, 50.0f);

   if (nodeFrom == INVALID_WAYPOINT_INDEX) {
      engine.centerPrint ("Unable to find nearest waypoint in 50 units");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (nodeTo < 0 || nodeTo >= g_numWaypoints) {
      if (m_cacheWaypointIndex >= 0 && m_cacheWaypointIndex < g_numWaypoints) {
         nodeTo = m_cacheWaypointIndex;
      }
      else {
         engine.centerPrint ("Unable to find destination waypoint");
         return;
      }
   }

   if (nodeTo == nodeFrom) {
      engine.centerPrint ("Unable to connect waypoint with itself");
      return;
   }

   float distance = (m_paths[nodeTo]->origin - m_paths[nodeFrom]->origin).length ();

   if (dir == CONNECTION_OUTGOING) {
      addPath (nodeFrom, nodeTo, distance);
   }
   else if (dir == CONNECTION_INCOMING) {
      addPath (nodeTo, nodeFrom, distance);
   }
   else {
      addPath (nodeFrom, nodeTo, distance);
      addPath (nodeTo, nodeFrom, distance);
   }

   engine.playSound (g_hostEntity, "common/wpn_hudon.wav");
   m_waypointsChanged = true;
}

void Waypoint::erasePath (void) {
   // this function allow player to manually remove a path from one waypoint to another

   int nodeFrom = getNearest (g_hostEntity->v.origin, 50.0f);
   int index = 0;

   if (nodeFrom == INVALID_WAYPOINT_INDEX) {
      engine.centerPrint ("Unable to find nearest waypoint in 50 units");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (nodeTo < 0 || nodeTo >= g_numWaypoints) {
      if (m_cacheWaypointIndex >= 0 && m_cacheWaypointIndex < g_numWaypoints) {
         nodeTo = m_cacheWaypointIndex;
      }
      else {
         engine.centerPrint ("Unable to find destination waypoint");
         return;
      }
   }

   for (index = 0; index < MAX_PATH_INDEX; index++) {
      if (m_paths[nodeFrom]->index[index] == nodeTo) {
         m_waypointsChanged = true;

         m_paths[nodeFrom]->index[index] = INVALID_WAYPOINT_INDEX; // unassigns this path
         m_paths[nodeFrom]->distances[index] = 0;
         m_paths[nodeFrom]->connectionFlags[index] = 0;
         m_paths[nodeFrom]->connectionVelocity[index].nullify ();

         engine.playSound (g_hostEntity, "weapons/mine_activate.wav");
         return;
      }
   }

   // not found this way ? check for incoming connections then
   index = nodeFrom;
   nodeFrom = nodeTo;
   nodeTo = index;

   for (index = 0; index < MAX_PATH_INDEX; index++) {
      if (m_paths[nodeFrom]->index[index] == nodeTo) {
         m_waypointsChanged = true;

         m_paths[nodeFrom]->index[index] = INVALID_WAYPOINT_INDEX; // unassign this path
         m_paths[nodeFrom]->distances[index] = 0;

         m_paths[nodeFrom]->connectionFlags[index] = 0;
         m_paths[nodeFrom]->connectionVelocity[index].nullify ();

         engine.playSound (g_hostEntity, "weapons/mine_activate.wav");
         return;
      }
   }
   engine.centerPrint ("There is already no path on this waypoint");
}

void Waypoint::cachePoint (void) {
   int node = getNearest (g_hostEntity->v.origin, 50.0f);

   if (node == INVALID_WAYPOINT_INDEX) {
      m_cacheWaypointIndex = INVALID_WAYPOINT_INDEX;
      engine.centerPrint ("Cached waypoint cleared (nearby point not found in 50 units range)");

      return;
   }
   m_cacheWaypointIndex = node;
   engine.centerPrint ("Waypoint #%d has been put into memory", m_cacheWaypointIndex);
}

void Waypoint::calculatePathRadius (int index) {
   // calculate "wayzones" for the nearest waypoint to pentedict (meaning a dynamic distance area to vary waypoint origin)

   Path *path = m_paths[index];
   Vector start, direction;

   if ((path->flags & (FLAG_LADDER | FLAG_GOAL | FLAG_CAMP | FLAG_RESCUE | FLAG_CROUCH)) || m_learnJumpWaypoint) {
      path->radius = 0.0f;
      return;
   }

   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      if (path->index[i] != INVALID_WAYPOINT_INDEX && (m_paths[path->index[i]]->flags & FLAG_LADDER)) {
         path->radius = 0.0f;
         return;
      }
   }
   TraceResult tr;
   bool wayBlocked = false;

   for (float scanDistance = 32.0f; scanDistance < 128.0f; scanDistance += 16.0f) {
      start = path->origin;
      makeVectors (Vector::null ());

      direction = g_pGlobals->v_forward * scanDistance;
      direction = direction.toAngles ();

      path->radius = scanDistance;

      for (float circleRadius = 0.0f; circleRadius < 360.0f; circleRadius += 20.0f) {
         makeVectors (direction);

         Vector radiusStart = start + g_pGlobals->v_forward * scanDistance;
         Vector radiusEnd = start + g_pGlobals->v_forward * scanDistance;

         engine.testHull (radiusStart, radiusEnd, TRACE_IGNORE_MONSTERS, head_hull, nullptr, &tr);

         if (tr.flFraction < 1.0f) {
            engine.testLine (radiusStart, radiusEnd, TRACE_IGNORE_MONSTERS, nullptr, &tr);

            if (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) == 0) {
               path->radius = 0.0f;
               wayBlocked = true;

               break;
            }
            wayBlocked = true;
            path->radius -= 16.0f;

            break;
         }

         Vector dropStart = start + g_pGlobals->v_forward * scanDistance;
         Vector dropEnd = dropStart - Vector (0.0f, 0.0f, scanDistance + 60.0f);

         engine.testHull (dropStart, dropEnd, TRACE_IGNORE_MONSTERS, head_hull, nullptr, &tr);

         if (tr.flFraction >= 1.0f) {
            wayBlocked = true;
            path->radius -= 16.0f;

            break;
         }
         dropStart = start - g_pGlobals->v_forward * scanDistance;
         dropEnd = dropStart - Vector (0.0f, 0.0f, scanDistance + 60.0f);

         engine.testHull (dropStart, dropEnd, TRACE_IGNORE_MONSTERS, head_hull, nullptr, &tr);

         if (tr.flFraction >= 1.0f) {
            wayBlocked = true;
            path->radius -= 16.0f;
            break;
         }

         radiusEnd.z += 34.0f;
         engine.testHull (radiusStart, radiusEnd, TRACE_IGNORE_MONSTERS, head_hull, nullptr, &tr);

         if (tr.flFraction < 1.0f) {
            wayBlocked = true;
            path->radius -= 16.0f;

            break;
         }
         direction.y = angleNorm (direction.y + circleRadius);
      }

      if (wayBlocked) {
         break;
      }
   }
   path->radius -= 16.0f;

   if (path->radius < 0.0f)
      path->radius = 0.0f;
}

void Waypoint::saveExperience (void) {
   ExtensionHeader header;

   if (g_numWaypoints < 1 || m_waypointsChanged) {
      return;
   }

   memset (header.header, 0, sizeof (header.header));
   strcpy (header.header, FH_EXPERIENCE);

   header.fileVersion = FV_EXPERIENCE;
   header.pointNumber = g_numWaypoints;

   ExperienceSave *experienceSave = new ExperienceSave[g_numWaypoints * g_numWaypoints];

   for (int i = 0; i < g_numWaypoints; i++) {
      for (int j = 0; j < g_numWaypoints; j++) {
         (experienceSave + (i * g_numWaypoints) + j)->team0Damage = static_cast<uint8> ((g_experienceData + (i * g_numWaypoints) + j)->team0Damage >> 3);
         (experienceSave + (i * g_numWaypoints) + j)->team1Damage = static_cast<uint8> ((g_experienceData + (i * g_numWaypoints) + j)->team1Damage >> 3);
         (experienceSave + (i * g_numWaypoints) + j)->team0Value = static_cast<int8> ((g_experienceData + (i * g_numWaypoints) + j)->team0Value / 8);
         (experienceSave + (i * g_numWaypoints) + j)->team1Value = static_cast<int8> ((g_experienceData + (i * g_numWaypoints) + j)->team1Value / 8);
      }
   }
   int result = Compress::encode (format ("%slearned/%s.exp", getDataDirectory (), engine.getMapName ()), (uint8 *)&header, sizeof (ExtensionHeader), (uint8 *) experienceSave, g_numWaypoints * g_numWaypoints * sizeof (ExperienceSave));

   delete[] experienceSave;

   if (result == -1) {
      logEntry (true, LL_ERROR, "Couldn't save experience data");
      return;
   }
}

void Waypoint::initExperience (void) {
   int i, j;

   delete[] g_experienceData;
   g_experienceData = nullptr;

   if (g_numWaypoints < 1) {
      return;
   }
   g_experienceData = new Experience[g_numWaypoints * g_numWaypoints];

   g_highestDamageCT = 1;
   g_highestDamageT = 1;

   // initialize table by hand to correct values, and NOT zero it out
   for (i = 0; i < g_numWaypoints; i++) {
      for (j = 0; j < g_numWaypoints; j++) {
         (g_experienceData + (i * g_numWaypoints) + j)->team0DangerIndex = INVALID_WAYPOINT_INDEX;
         (g_experienceData + (i * g_numWaypoints) + j)->team1DangerIndex = INVALID_WAYPOINT_INDEX;

         (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = 0;
         (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = 0;

         (g_experienceData + (i * g_numWaypoints) + j)->team0Value = 0;
         (g_experienceData + (i * g_numWaypoints) + j)->team1Value = 0;
      }
   }
   File fp (format ("%slearned/%s.exp", getDataDirectory (), engine.getMapName ()), "rb");

   // if file exists, read the experience data from it
   if (fp.isValid ()) {
      ExtensionHeader header;
      memset (&header, 0, sizeof (header));

      if (fp.read (&header, sizeof (header)) == 0) {
         logEntry (true, LL_ERROR, "Experience data damaged (unable to read header)");

         fp.close ();
         return;
      }
      fp.close ();

      if (strncmp (header.header, FH_EXPERIENCE, strlen (FH_EXPERIENCE)) == 0) {
         if (header.fileVersion == FV_EXPERIENCE && header.pointNumber == g_numWaypoints) {
            ExperienceSave *experienceLoad = new ExperienceSave[g_numWaypoints * g_numWaypoints * sizeof (ExperienceSave)];

            Compress::decode (format ("%slearned/%s.exp", getDataDirectory (), engine.getMapName ()), sizeof (ExtensionHeader), (uint8 *)experienceLoad, g_numWaypoints * g_numWaypoints * sizeof (ExperienceSave));

            for (i = 0; i < g_numWaypoints; i++) {
               for (j = 0; j < g_numWaypoints; j++) {
                  if (i == j) {
                     (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = (uint16) ((experienceLoad + (i * g_numWaypoints) + j)->team0Damage);
                     (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = (uint16) ((experienceLoad + (i * g_numWaypoints) + j)->team1Damage);

                     if ((g_experienceData + (i * g_numWaypoints) + j)->team0Damage > g_highestDamageT)
                        g_highestDamageT = (g_experienceData + (i * g_numWaypoints) + j)->team0Damage;

                     if ((g_experienceData + (i * g_numWaypoints) + j)->team1Damage > g_highestDamageCT)
                        g_highestDamageCT = (g_experienceData + (i * g_numWaypoints) + j)->team1Damage;
                  }
                  else {
                     (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = (uint16) ((experienceLoad + (i * g_numWaypoints) + j)->team0Damage) << 3;
                     (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = (uint16) ((experienceLoad + (i * g_numWaypoints) + j)->team1Damage) << 3;
                  }

                  (g_experienceData + (i * g_numWaypoints) + j)->team0Value = (int16) ((experienceLoad + i * (g_numWaypoints) + j)->team0Value) * 8;
                  (g_experienceData + (i * g_numWaypoints) + j)->team1Value = (int16) ((experienceLoad + i * (g_numWaypoints) + j)->team1Value) * 8;
               }
            }
            delete[] experienceLoad;
         }
         else
            logEntry (true, LL_WARNING, "Experience data damaged (wrong version, or not for this map)");
      }
   }
}

void Waypoint::saveVisibility (void) {
   if (g_numWaypoints == 0) {
      return;
   }

   ExtensionHeader header;
   memset (&header, 0, sizeof (ExtensionHeader));

   // parse header
   memset (header.header, 0, sizeof (header.header));
   strcpy (header.header, FH_VISTABLE);

   header.fileVersion = FV_VISTABLE;
   header.pointNumber = g_numWaypoints;

   File fp (format ("%slearned/%s.vis", getDataDirectory (), engine.getMapName ()), "wb");

   if (!fp.isValid ()) {
      logEntry (true, LL_ERROR, "Failed to open visibility table for writing");
      return;
   }
   fp.close ();

   Compress::encode (format ("%slearned/%s.vis", getDataDirectory (), engine.getMapName ()), (uint8 *)&header, sizeof (ExtensionHeader), (uint8 *)m_visLUT, MAX_WAYPOINTS * (MAX_WAYPOINTS / 4) * sizeof (uint8));
}

void Waypoint::initVisibility (void) {
   if (g_numWaypoints == 0)
      return;

   ExtensionHeader header;

   File fp (format ("%slearned/%s.vis", getDataDirectory (), engine.getMapName ()), "rb");
   m_redoneVisibility = false;

   if (!fp.isValid ()) {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      logEntry (true, LL_DEFAULT, "Vistable doesn't exists, vistable will be rebuilded");
      return;
   }

   // read the header of the file
   if (fp.read (&header, sizeof (header)) == 0) {
      logEntry (true, LL_ERROR, "Vistable damaged (unable to read header)");

      fp.close ();
      return;
   }

   if (strncmp (header.header, FH_VISTABLE, strlen (FH_VISTABLE)) != 0 || header.fileVersion != FV_VISTABLE || header.pointNumber != g_numWaypoints) {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      logEntry (true, LL_WARNING, "Visibility table damaged (wrong version, or not for this map), vistable will be rebuilded.");
      fp.close ();

      return;
   }
   int result = Compress::decode (format ("%slearned/%s.vis", getDataDirectory (), engine.getMapName ()), sizeof (ExtensionHeader), (uint8 *)m_visLUT, MAX_WAYPOINTS * (MAX_WAYPOINTS / 4) * sizeof (uint8));

   if (result == -1) {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      logEntry (true, LL_ERROR, "Failed to decode vistable, vistable will be rebuilded.");
      fp.close ();

      return;
   }
   fp.close ();
}

void Waypoint::initTypes (void) {
   m_terrorPoints.clear ();
   m_ctPoints.clear ();
   m_goalPoints.clear ();
   m_campPoints.clear ();
   m_rescuePoints.clear ();
   m_sniperPoints.clear ();
   m_visitedGoals.clear ();

   for (int i = 0; i < g_numWaypoints; i++) {
      if (m_paths[i]->flags & FLAG_TF_ONLY) {
         m_terrorPoints.push (i);
      }
      else if (m_paths[i]->flags & FLAG_CF_ONLY) {
         m_ctPoints.push (i);
      }
      else if (m_paths[i]->flags & FLAG_GOAL) {
         m_goalPoints.push (i);
      }
      else if (m_paths[i]->flags & FLAG_CAMP) {
         m_campPoints.push (i);
      }
      else if (m_paths[i]->flags & FLAG_SNIPER) {
         m_sniperPoints.push (i);
      }
      else if (m_paths[i]->flags & FLAG_RESCUE) {
         m_rescuePoints.push (i);
      }
   }
}

bool Waypoint::load (void) {
   if (m_loadTries++ > 3) {
      m_loadTries = 0;

      sprintf (m_infoBuffer, "Giving up loading waypoint file (%s). Something went wrong.", engine.getMapName ());
      logEntry (true, LL_ERROR, m_infoBuffer);

      return false;
   }
   MemFile fp (getWaypointFilename (true));
 
   WaypointHeader header;
   memset (&header, 0, sizeof (header));

   // save for faster access
   const char *map = engine.getMapName ();

   if (fp.isValid ()) {
      if (fp.read (&header, sizeof (header)) == 0) {
         sprintf (m_infoBuffer, "%s.pwf - damaged waypoint file (unable to read header)", map);
         logEntry (true, LL_ERROR, m_infoBuffer);

         fp.close ();
         return false;
      }

      if (strncmp (header.header, FH_WAYPOINT, strlen (FH_WAYPOINT)) == 0) {
         if (header.fileVersion != FV_WAYPOINT) {
            sprintf (m_infoBuffer, "%s.pwf - incorrect waypoint file version (expected '%d' found '%ld')", map, FV_WAYPOINT, header.fileVersion);
            logEntry (true, LL_ERROR, m_infoBuffer);

            fp.close ();
            return false;
         }
         else if (stricmp (header.mapName, map)) {
            sprintf (m_infoBuffer, "%s.pwf - hacked waypoint file, file name doesn't match waypoint header information (mapname: '%s', header: '%s')", map, map, header.mapName);
            logEntry (true, LL_ERROR, m_infoBuffer);

            fp.close ();
            return false;
         }
         else {
            if (header.pointNumber == 0 || header.pointNumber > MAX_WAYPOINTS) {
               sprintf (m_infoBuffer, "%s.pwf - waypoint file contains illegal number of waypoints (mapname: '%s', header: '%s')", map, map, header.mapName);
               logEntry (true, LL_ERROR, m_infoBuffer);

               fp.close ();
               return false;
            }

            init ();
            g_numWaypoints = header.pointNumber;

            for (int i = 0; i < g_numWaypoints; i++) {
               m_paths[i] = new Path;

               if (fp.read (m_paths[i], sizeof (Path)) == 0) {
                  sprintf (m_infoBuffer, "%s.pwf - truncated waypoint file (count: %d, need: %d)", map, i, g_numWaypoints);
                  logEntry (true, LL_ERROR, m_infoBuffer);

                  fp.close ();
                  return false;
               }

               // more checks of waypoint quality
               if (m_paths[i]->pathNumber < 0 || m_paths[i]->pathNumber > g_numWaypoints) {
                  sprintf (m_infoBuffer, "%s.pwf - bad waypoint file (path #%d index is out of bounds)", map, i);
                  logEntry (true, LL_ERROR, m_infoBuffer);

                  fp.close ();
                  return false;
               }
            }
            m_waypointPaths = true;
         }
      }
      else {
         sprintf (m_infoBuffer, "%s.pwf is not a yapb waypoint file (header found '%s' needed '%s'", map, header.header, FH_WAYPOINT);
         logEntry (true, LL_ERROR, m_infoBuffer);

         fp.close ();
         return false;
      }
      fp.close ();
   }
   else {
      if (yb_waypoint_autodl_enable.boolean ()) {
         logEntry (true, LL_DEFAULT, "%s.pwf does not exist, trying to download from waypoint database", map);
         WaypointDownloadError status = downloadWaypoint ();

         if (status == WDE_SOCKET_ERROR) {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Socket error.", map);
            logEntry (true, LL_ERROR, m_infoBuffer);

            yb_waypoint_autodl_enable.set (0);

            return false;
         }
         else if (status == WDE_CONNECT_ERROR) {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Connection problems.", map);
            logEntry (true, LL_ERROR, m_infoBuffer);

            yb_waypoint_autodl_enable.set (0);

            return false;
         }
         else if (status == WDE_NOTFOUND_ERROR) {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Waypoint not available.", map);
            logEntry (true, LL_ERROR, m_infoBuffer);

            return false;
         }
         else {
            logEntry (true, LL_DEFAULT, "%s.pwf was downloaded from waypoint database. Trying to load...", map);
            return load ();
         }
      }
      sprintf (m_infoBuffer, "%s.pwf does not exist", map);
      logEntry (true, LL_ERROR, m_infoBuffer);

      return false;
   }

   if (strncmp (header.author, "official", 7) == 0) {
      sprintf (m_infoBuffer, "Using Official Waypoint File");
   }
   else {
      sprintf (m_infoBuffer, "Using waypoint file by: %s", header.author);
   }
    
   for (int i = 0; i < g_numWaypoints; i++) {
      m_waypointDisplayTime[i] = 0.0;
   }

   initPathMatrix ();
   initTypes ();

   m_waypointsChanged = false;
   g_highestKills = 1;

   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;

   initVisibility ();
   initExperience ();

   extern ConVar yb_debug_goal;
   yb_debug_goal.set (INVALID_WAYPOINT_INDEX);

   return true;
}

void Waypoint::save (void) {
   WaypointHeader header;

   memset (header.mapName, 0, sizeof (header.mapName));
   memset (header.author, 0, sizeof (header.author));
   memset (header.header, 0, sizeof (header.header));

   strcpy (header.header, FH_WAYPOINT);
   strncpy (header.author, STRING (g_hostEntity->v.netname), A_bufsize (header.author));
   strncpy (header.mapName, engine.getMapName (), A_bufsize (header.mapName));

   header.mapName[31] = 0;
   header.fileVersion = FV_WAYPOINT;
   header.pointNumber = g_numWaypoints;

   File fp (getWaypointFilename (), "wb");

   // file was opened
   if (fp.isValid ()) {
      // write the waypoint header to the file...
      fp.write (&header, sizeof (header), 1);

      // save the waypoint paths...
      for (int i = 0; i < g_numWaypoints; i++) {
         fp.write (m_paths[i], sizeof (Path));
      }
      fp.close ();
   }
   else
      logEntry (true, LL_ERROR, "Error writing '%s.pwf' waypoint file", engine.getMapName ());
}

const char *Waypoint::getWaypointFilename (bool isMemoryFile) {
   static char buffer[256];
   snprintf (buffer, A_bufsize (buffer), "%s%s%s.pwf", getDataDirectory (isMemoryFile), isEmptyStr (yb_wptsubfolder.str ()) ? "" : yb_wptsubfolder.str (), engine.getMapName ());

   if (File::exists (buffer)) {
      return &buffer[0];
   }
   return format ("%s%s.pwf", getDataDirectory (isMemoryFile), engine.getMapName ());
}

float Waypoint::calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin) {
   // this function returns 2D traveltime to a position

   return (origin - src).length2D () / maxSpeed;
}

bool Waypoint::isReachable (Bot *bot, int index) {
   // this function return whether bot able to reach index waypoint or not, depending on several factors.

   if (!bot || index < 0 || index >= g_numWaypoints) {
      return false;
   }

   const Vector &src = bot->pev->origin;
   const Vector &dst = m_paths[index]->origin;

   // is the destination close enough?
   if ((dst - src).lengthSq () >= A_square (400.0f)) {
      return false;
   }
   float ladderDist = (dst - src).length2D ();

   TraceResult tr;
   engine.testLine (src, dst, TRACE_IGNORE_MONSTERS, bot->ent (), &tr);

   // if waypoint is visible from current position (even behind head)...
   if (tr.flFraction >= 1.0f) {

      // it's should be not a problem to reach waypoint inside water...
      if (bot->pev->waterlevel == 2 || bot->pev->waterlevel == 3) {
         return true;
      }

      // check for ladder
      bool nonLadder = !(m_paths[index]->flags & FLAG_LADDER) || ladderDist > 16.0f;

      // is dest waypoint higher than src? (62 is max jump height)
      if (nonLadder && dst.z > src.z + 62.0f) {
         return false; // can't reach this one
      }

      // is dest waypoint lower than src?
      if (nonLadder && dst.z < src.z - 100.0f) {
         return false; // can't reach this one
      }
      return true;
   }
   return false;
}

bool Waypoint::isNodeReacheable (const Vector &src, const Vector &destination) {
   TraceResult tr;

   float distance = (destination - src).length ();

   // is the destination not close enough?
   if (distance > g_autoPathDistance) {
      return false;
   }

   // check if we go through a func_illusionary, in which case return false
   engine.testHull (src, destination, TRACE_IGNORE_MONSTERS, head_hull, g_hostEntity, &tr);

   if (!engine.isNullEntity (tr.pHit) && strcmp ("func_illusionary", STRING (tr.pHit->v.classname)) == 0) {
      return false; // don't add pathwaypoints through func_illusionaries
   }

   // check if this waypoint is "visible"...
   engine.testLine (src, destination, TRACE_IGNORE_MONSTERS, g_hostEntity, &tr);

   // if waypoint is visible from current position (even behind head)...
   if (tr.flFraction >= 1.0f || strncmp ("func_door", STRING (tr.pHit->v.classname), 9) == 0) {
      // if it's a door check if nothing blocks behind
      if (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) == 0) {
         engine.testLine (tr.vecEndPos, destination, TRACE_IGNORE_MONSTERS, tr.pHit, &tr);

         if (tr.flFraction < 1.0f)
            return false;
      }

      // check for special case of both waypoints being in water...
      if (g_engfuncs.pfnPointContents (src) == CONTENTS_WATER && g_engfuncs.pfnPointContents (destination) == CONTENTS_WATER) {
         return true; // then they're reachable each other
      }

      // is dest waypoint higher than src? (45 is max jump height)
      if (destination.z > src.z + 45.0f) {
         Vector sourceNew = destination;
         Vector destinationNew = destination;
         destinationNew.z = destinationNew.z - 50.0f; // straight down 50 units

         engine.testLine (sourceNew, destinationNew, TRACE_IGNORE_MONSTERS, g_hostEntity, &tr);

         // check if we didn't hit anything, if not then it's in mid-air
         if (tr.flFraction >= 1.0) {
            return false; // can't reach this one
         }
      }

      // check if distance to ground drops more than step height at points between source and destination...
      Vector direction = (destination - src).normalize (); // 1 unit long
      Vector check = src, down = src;

      down.z = down.z - 1000.0f; // straight down 1000 units

      engine.testLine (check, down, TRACE_IGNORE_MONSTERS, g_hostEntity, &tr);

      float lastHeight = tr.flFraction * 1000.0f; // height from ground
      distance = (destination - check).length (); // distance from goal

      while (distance > 10.0f) {
         // move 10 units closer to the goal...
         check = check + (direction * 10.0f);

         down = check;
         down.z = down.z - 1000.0f; // straight down 1000 units

         engine.testLine (check, down, TRACE_IGNORE_MONSTERS, g_hostEntity, &tr);

         float height = tr.flFraction * 1000.0f; // height from ground

         // is the current height greater than the step height?
         if (height < lastHeight - 18.0f) {
            return false; // can't get there without jumping...
         }
         lastHeight = height;
         distance = (destination - check).length (); // distance from goal
      }
      return true;
   }
   return false;
}

void Waypoint::rebuildVisibility (void) {
   if (!m_redoneVisibility) {
      return;
   }

   TraceResult tr;
   uint8 res, shift;

   for (m_visibilityIndex = 0; m_visibilityIndex < g_numWaypoints; m_visibilityIndex++) {
      Vector sourceDuck = m_paths[m_visibilityIndex]->origin;
      Vector sourceStand = m_paths[m_visibilityIndex]->origin;

      if (m_paths[m_visibilityIndex]->flags & FLAG_CROUCH) {
         sourceDuck.z += 12.0f;
         sourceStand.z += 18.0f + 28.0f;
      }
      else {
         sourceDuck.z += -18.0f + 12.0f;
         sourceStand.z += 28.0f;
      }
      uint16 standCount = 0, crouchCount = 0;

      for (int i = 0; i < g_numWaypoints; i++) {
         // first check ducked visibility
         Vector dest = m_paths[i]->origin;

         engine.testLine (sourceDuck, dest, TRACE_IGNORE_MONSTERS, nullptr, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (tr.flFraction != 1.0f || tr.fStartSolid) {
            res = 1;
         }
         else {
            res = 0;
         }
         res <<= 1;

         engine.testLine (sourceStand, dest, TRACE_IGNORE_MONSTERS, nullptr, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (tr.flFraction != 1.0f || tr.fStartSolid) {
            res |= 1;
         }

         if (res != 0) {
            dest = m_paths[i]->origin;

            // first check ducked visibility
            if (m_paths[i]->flags & FLAG_CROUCH) {
               dest.z += 18.0f + 28.0f;
            }
            else {
               dest.z += 28.0f;
            }
            engine.testLine (sourceDuck, dest, TRACE_IGNORE_MONSTERS, nullptr, &tr);

            // check if line of sight to object is not blocked (i.e. visible)
            if (tr.flFraction != 1.0f || tr.fStartSolid) {
               res |= 2;
            }
            else {
               res &= 1;
            }
            engine.testLine (sourceStand, dest, TRACE_IGNORE_MONSTERS, nullptr, &tr);

            // check if line of sight to object is not blocked (i.e. visible)
            if (tr.flFraction != 1.0f || tr.fStartSolid) {
               res |= 1;
            }
            else {
               res &= 2;
            }
         }
         shift = (i % 4) << 1;

         m_visLUT[m_visibilityIndex][i >> 2] &= ~(3 << shift);
         m_visLUT[m_visibilityIndex][i >> 2] |= res << shift;

         if (!(res & 2)) {
            crouchCount++;
         }

         if (!(res & 1)) {
            standCount++;
         }
      }
      m_paths[m_visibilityIndex]->vis.crouch = crouchCount;
      m_paths[m_visibilityIndex]->vis.stand = standCount;
   }
   m_redoneVisibility = false;
}

bool Waypoint::isVisible (int srcIndex, int destIndex) {
   if (srcIndex < 0 || srcIndex >= g_numWaypoints || destIndex < 0 || destIndex >= g_numWaypoints) {
      return false;
   }

   uint8 res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 3) == 3);
}

bool Waypoint::isDuckVisible (int srcIndex, int destIndex) {
   if (srcIndex < 0 || srcIndex >= g_numWaypoints || destIndex < 0 || destIndex >= g_numWaypoints) {
      return false;
   }

   uint8 res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 2) == 2);
}

bool Waypoint::isStandVisible (int srcIndex, int destIndex) {
   if (srcIndex < 0 || srcIndex >= g_numWaypoints || destIndex < 0 || destIndex >= g_numWaypoints) {
      return false;
   }

   uint8 res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 1) == 1);
}

const char *Waypoint::getInformation (int id) {
   // this function returns path information for waypoint pointed by id.

   Path *path = m_paths[id];

   // if this path is null, return
   if (path == nullptr) {
      return "\0";
   }
   bool jumpPoint = false;

   // iterate through connections and find, if it's a jump path
   for (int i = 0; i < MAX_PATH_INDEX; i++) {
      // check if we got a valid connection
      if (path->index[i] != INVALID_WAYPOINT_INDEX && (path->connectionFlags[i] & PATHFLAG_JUMP)) {
         jumpPoint = true;
      }
   }

   static char messageBuffer[MAX_PRINT_BUFFER];
   sprintf (messageBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s", (path->flags == 0 && !jumpPoint) ? " (none)" : "", (path->flags & FLAG_LIFT) ? " LIFT" : "", (path->flags & FLAG_CROUCH) ? " CROUCH" : "", (path->flags & FLAG_CROSSING) ? " CROSSING" : "", (path->flags & FLAG_CAMP) ? " CAMP" : "", (path->flags & FLAG_TF_ONLY) ? " TERRORIST" : "", (path->flags & FLAG_CF_ONLY) ? " CT" : "", (path->flags & FLAG_SNIPER) ? " SNIPER" : "", (path->flags & FLAG_GOAL) ? " GOAL" : "", (path->flags & FLAG_LADDER) ? " LADDER" : "", (path->flags & FLAG_RESCUE) ? " RESCUE" : "", (path->flags & FLAG_DOUBLEJUMP) ? " JUMPHELP" : "", (path->flags & FLAG_NOHOSTAGE) ? " NOHOSTAGE" : "", jumpPoint ? " JUMP" : "");

   // return the message buffer
   return messageBuffer;
}

void Waypoint::frame (void) {
   // this function executes frame of waypoint operation code.

   if (engine.isNullEntity (g_hostEntity)) {
      return; // this function is only valid on listenserver, and in waypoint enabled mode.
   }

   float nearestDistance = 99999.0f;
   int nearestIndex = INVALID_WAYPOINT_INDEX;

   // check if it's time to add jump waypoint
   if (m_learnJumpWaypoint) {
      if (!m_endJumpPoint) {
         if (g_hostEntity->v.button & IN_JUMP) {
            push (9);

            m_timeJumpStarted = engine.timebase ();
            m_endJumpPoint = true;
         }
         else {
            m_learnVelocity = g_hostEntity->v.velocity;
            m_learnPosition = g_hostEntity->v.origin;
         }
      }
      else if (((g_hostEntity->v.flags & FL_ONGROUND) || g_hostEntity->v.movetype == MOVETYPE_FLY) && m_timeJumpStarted + 0.1f < engine.timebase () && m_endJumpPoint) {
         push (10);

         m_learnJumpWaypoint = false;
         m_endJumpPoint = false;
      }
   }

   // check if it's a autowaypoint mode enabled
   if (g_autoWaypoint && (g_hostEntity->v.flags & (FL_ONGROUND | FL_PARTIALGROUND))) {
      // find the distance from the last used waypoint
      float distance = (m_lastWaypoint - g_hostEntity->v.origin).lengthSq ();

      if (distance > 16384.0f) {
         // check that no other reachable waypoints are nearby...
         for (int i = 0; i < g_numWaypoints; i++) {
            if (isNodeReacheable (g_hostEntity->v.origin, m_paths[i]->origin)) {
               distance = (m_paths[i]->origin - g_hostEntity->v.origin).lengthSq ();

               if (distance < nearestDistance) {
                  nearestDistance = distance;
               }
            }
         }

         // make sure nearest waypoint is far enough away...
         if (nearestDistance >= 16384.0f) {
            push (0); // place a waypoint here
         }
      }
   }
   m_facingAtIndex = getFacingIndex ();

   // reset the minimal distance changed before
   nearestDistance = 999999.0f;

   // now iterate through all waypoints in a map, and draw required ones
   for (int i = 0; i < g_numWaypoints; i++) {
      float distance = (m_paths[i]->origin - g_hostEntity->v.origin).length ();

      // check if waypoint is whitin a distance, and is visible
      if (distance < 512.0f && ((::isVisible (m_paths[i]->origin, g_hostEntity) && isInViewCone (m_paths[i]->origin, g_hostEntity)) || !isAlive (g_hostEntity) || distance < 128.0f)) {
         // check the distance
         if (distance < nearestDistance) {
            nearestIndex = i;
            nearestDistance = distance;
         }

         if (m_waypointDisplayTime[i] + 0.8f < engine.timebase ()) {
            float nodeHeight = 0.0f;

            // check the node height
            if (m_paths[i]->flags & FLAG_CROUCH) {
               nodeHeight = 36.0f;
            }
            else {
               nodeHeight = 72.0f;
            }
            float nodeHalfHeight = nodeHeight * 0.5f;

            // all waypoints are by default are green
            Vector nodeColor;

            // colorize all other waypoints
            if (m_paths[i]->flags & FLAG_CAMP) {
               nodeColor = Vector (0, 255, 255);
            }
            else if (m_paths[i]->flags & FLAG_GOAL) {
               nodeColor = Vector (128, 0, 255);
            }
            else if (m_paths[i]->flags & FLAG_LADDER) {
               nodeColor = Vector (128, 64, 0);
            }
            else if (m_paths[i]->flags & FLAG_RESCUE) {
               nodeColor = Vector (255, 255, 255);
            }
            else {
               nodeColor = Vector (0, 255, 0);
            }

            // colorize additional flags
            Vector nodeFlagColor = Vector (-1, -1, -1);

            // check the colors
            if (m_paths[i]->flags & FLAG_SNIPER) {
               nodeFlagColor = Vector (130, 87, 0);
            }
            else if (m_paths[i]->flags & FLAG_NOHOSTAGE) {
               nodeFlagColor = Vector (255, 255, 255);
            }
            else if (m_paths[i]->flags & FLAG_TF_ONLY) {
               nodeFlagColor = Vector (255, 0, 0);
            }
            else if (m_paths[i]->flags & FLAG_CF_ONLY) {
               nodeFlagColor = Vector (0, 0, 255);
            }

            // draw node without additional flags
            if (nodeFlagColor.x == -1) {
               engine.drawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight), m_paths[i]->origin + Vector (0, 0, nodeHalfHeight), 15, 0, static_cast<int> (nodeColor.x), static_cast<int> (nodeColor.y), static_cast<int> (nodeColor.z), 250, 0, 10);
            }
            
            // draw node with flags
            else {
               engine.drawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight), m_paths[i]->origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75f), 14, 0, static_cast<int> (nodeColor.x), static_cast<int> (nodeColor.y), static_cast<int> (nodeColor.z), 250, 0, 10); // draw basic path
               engine.drawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75f), m_paths[i]->origin + Vector (0, 0, nodeHalfHeight), 14, 0, static_cast<int> (nodeFlagColor.x), static_cast<int> (nodeFlagColor.y), static_cast<int> (nodeFlagColor.z), 250, 0, 10); // draw additional path
            }
            m_waypointDisplayTime[i] = engine.timebase ();
         }
      }
   }

   if (nearestIndex == INVALID_WAYPOINT_INDEX) {
      return;
   }

   // draw arrow to a some importaint waypoints
   if ((m_findWPIndex != INVALID_WAYPOINT_INDEX && m_findWPIndex < g_numWaypoints) || (m_cacheWaypointIndex != INVALID_WAYPOINT_INDEX && m_cacheWaypointIndex < g_numWaypoints) || (m_facingAtIndex != INVALID_WAYPOINT_INDEX && m_facingAtIndex < g_numWaypoints)) {
      // check for drawing code
      if (m_arrowDisplayTime + 0.5f < engine.timebase ()) {

         // finding waypoint - pink arrow
         if (m_findWPIndex != INVALID_WAYPOINT_INDEX) {
            engine.drawLine (g_hostEntity, g_hostEntity->v.origin, m_paths[m_findWPIndex]->origin, 10, 0, 128, 0, 128, 200, 0, 5, DRAW_ARROW);
         }

         // cached waypoint - yellow arrow
         if (m_cacheWaypointIndex != INVALID_WAYPOINT_INDEX) {
            engine.drawLine (g_hostEntity, g_hostEntity->v.origin, m_paths[m_cacheWaypointIndex]->origin, 10, 0, 255, 255, 0, 200, 0, 5, DRAW_ARROW);
         }

         // waypoint user facing at - white arrow
         if (m_facingAtIndex != INVALID_WAYPOINT_INDEX) {
            engine.drawLine (g_hostEntity, g_hostEntity->v.origin, m_paths[m_facingAtIndex]->origin, 10, 0, 255, 255, 255, 200, 0, 5, DRAW_ARROW);
         }
         m_arrowDisplayTime = engine.timebase ();
      }
   }

   // create path pointer for faster access
   Path *path = m_paths[nearestIndex];

   // draw a paths, camplines and danger directions for nearest waypoint
   if (nearestDistance <= 56.0f && m_pathDisplayTime <= engine.timebase ()) {
      m_pathDisplayTime = engine.timebase () + 1.0f;

      // draw the camplines
      if (path->flags & FLAG_CAMP) {
         Vector campSourceOrigin = path->origin + Vector (0.0f, 0.0f, 36.0f);

         // check if it's a source
         if (path->flags & FLAG_CROUCH) {
            campSourceOrigin = path->origin + Vector (0.0f, 0.0f, 18.0f);
         }
         Vector campStartOrigin = Vector (path->campStartX, path->campStartY, campSourceOrigin.z); // camp start
         Vector campEndOrigin = Vector (path->campEndX, path->campEndY, campSourceOrigin.z); // camp end

         // draw it now
         engine.drawLine (g_hostEntity, campSourceOrigin, campStartOrigin, 10, 0, 255, 0, 0, 200, 0, 10);
         engine.drawLine (g_hostEntity, campSourceOrigin, campEndOrigin, 10, 0, 255, 0, 0, 200, 0, 10);
      }

      // draw the connections
      for (int i = 0; i < MAX_PATH_INDEX; i++) {
         if (path->index[i] == INVALID_WAYPOINT_INDEX) {
            continue;
         }
         // jump connection
         if (path->connectionFlags[i] & PATHFLAG_JUMP) {
            engine.drawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 255, 0, 128, 200, 0, 10);
         }
         else if (isConnected (path->index[i], nearestIndex)) { // twoway connection
            engine.drawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 255, 255, 0, 200, 0, 10);
         }
         else { // oneway connection
            engine.drawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 250, 250, 250, 200, 0, 10);
         }
      }

      // now look for oneway incoming connections
      for (int i = 0; i < g_numWaypoints; i++) {
         if (isConnected (m_paths[i]->pathNumber, path->pathNumber) && !isConnected (path->pathNumber, m_paths[i]->pathNumber)) {
            engine.drawLine (g_hostEntity, path->origin, m_paths[i]->origin, 5, 0, 0, 192, 96, 200, 0, 10);
         }
      }

      // draw the radius circle
      Vector origin = (path->flags & FLAG_CROUCH) ? path->origin : path->origin - Vector (0.0f, 0.0f, 18.0f);

      // if radius is nonzero, draw a full circle
      if (path->radius > 0.0f) {
         float squareRoot = A_sqrtf (path->radius * path->radius * 0.5f);

         engine.drawLine (g_hostEntity, origin + Vector (path->radius, 0.0f, 0.0f), origin + Vector (squareRoot, -squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         engine.drawLine (g_hostEntity, origin + Vector (squareRoot, -squareRoot, 0.0f), origin + Vector (0.0f, -path->radius, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         engine.drawLine (g_hostEntity, origin + Vector (0.0f, -path->radius, 0.0f), origin + Vector (-squareRoot, -squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         engine.drawLine (g_hostEntity, origin + Vector (-squareRoot, -squareRoot, 0.0f), origin + Vector (-path->radius, 0.0f, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         engine.drawLine (g_hostEntity, origin + Vector (-path->radius, 0.0f, 0.0f), origin + Vector (-squareRoot, squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         engine.drawLine (g_hostEntity, origin + Vector (-squareRoot, squareRoot, 0.0f), origin + Vector (0.0f, path->radius, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         engine.drawLine (g_hostEntity, origin + Vector (0.0f, path->radius, 0.0f), origin + Vector (squareRoot, squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         engine.drawLine (g_hostEntity, origin + Vector (squareRoot, squareRoot, 0.0f), origin + Vector (path->radius, 0.0f, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
      }
      else {
         float squareRoot = A_sqrtf (32.0f);

         engine.drawLine (g_hostEntity, origin + Vector (squareRoot, -squareRoot, 0.0f), origin + Vector (-squareRoot, squareRoot, 0.0f), 5, 0, 255, 0, 0, 200, 0, 10);
         engine.drawLine (g_hostEntity, origin + Vector (-squareRoot, -squareRoot, 0.0f), origin + Vector (squareRoot, squareRoot, 0.0f), 5, 0, 255, 0, 0, 200, 0, 10);
      }

      // draw the danger directions
      if (!m_waypointsChanged) {
         if ((g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team0DangerIndex != INVALID_WAYPOINT_INDEX && engine.getTeam (g_hostEntity) == TEAM_TERRORIST) {
            engine.drawLine (g_hostEntity, path->origin, m_paths[(g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team0DangerIndex]->origin, 15, 0, 255, 0, 0, 200, 0, 10, DRAW_ARROW); // draw a red arrow to this index's danger point
         }

         if ((g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team1DangerIndex != INVALID_WAYPOINT_INDEX && engine.getTeam (g_hostEntity) == TEAM_COUNTER) {
            engine.drawLine (g_hostEntity, path->origin, m_paths[(g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team1DangerIndex]->origin, 15, 0, 0, 0, 255, 200, 0, 10, DRAW_ARROW); // draw a blue arrow to this index's danger point
         }
      }
      // display some information
      char tempMessage[4096];

      // show the information about that point
      int length = sprintf (tempMessage,
                            "\n\n\n\n    Waypoint Information:\n\n"
                            "      Waypoint %d of %d, Radius: %.1f\n"
                            "      Flags: %s\n\n",
                            nearestIndex, g_numWaypoints, path->radius, getInformation (nearestIndex));

      // if waypoint is not changed display experience also
      if (!m_waypointsChanged) {
         int dangerIndexCT = (g_experienceData + nearestIndex * g_numWaypoints + nearestIndex)->team1DangerIndex;
         int dangerIndexT = (g_experienceData + nearestIndex * g_numWaypoints + nearestIndex)->team0DangerIndex;

         length += sprintf (&tempMessage[length],
                            "      Experience Info:\n"
                            "      CT: %d / %d\n"
                            "      T: %d / %d\n",
                            dangerIndexCT, dangerIndexCT != INVALID_WAYPOINT_INDEX ? (g_experienceData + nearestIndex * g_numWaypoints + dangerIndexCT)->team1Damage : 0, dangerIndexT, dangerIndexT != INVALID_WAYPOINT_INDEX ? (g_experienceData + nearestIndex * g_numWaypoints + dangerIndexT)->team0Damage : 0);
      }

      // check if we need to show the cached point index
      if (m_cacheWaypointIndex != INVALID_WAYPOINT_INDEX) {
         length += sprintf (&tempMessage[length],
                            "\n    Cached Waypoint Information:\n\n"
                            "      Waypoint %d of %d, Radius: %.1f\n"
                            "      Flags: %s\n",
                            m_cacheWaypointIndex, g_numWaypoints, m_paths[m_cacheWaypointIndex]->radius, getInformation (m_cacheWaypointIndex));
      }

      // check if we need to show the facing point index
      if (m_facingAtIndex != INVALID_WAYPOINT_INDEX) {
         length += sprintf (&tempMessage[length],
                            "\n    Facing Waypoint Information:\n\n"
                            "      Waypoint %d of %d, Radius: %.1f\n"
                            "      Flags: %s\n",
                            m_facingAtIndex, g_numWaypoints, m_paths[m_facingAtIndex]->radius, getInformation (m_facingAtIndex));
      }

      // draw entire message
      MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, Vector::null (), g_hostEntity)
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (4) // channel
         .writeShort (MessageWriter::fs16 (0, 1 << 13)) // x
         .writeShort (MessageWriter::fs16 (0, 1 << 13)) // y
         .writeByte (0) // effect
         .writeByte (255) // r1
         .writeByte (255) // g1
         .writeByte (255) // b1
         .writeByte (1) // a1
         .writeByte (255) // r2
         .writeByte (255) // g2
         .writeByte (255) // b2
         .writeByte (255) // a2
         .writeShort (0) // fadeintime
         .writeShort (0) // fadeouttime
         .writeShort (MessageWriter::fu16 (1.1f, 1 << 8)) // holdtime
         .writeString (tempMessage);
   }
}

bool Waypoint::isConnected (int index) {
   for (int i = 0; i < g_numWaypoints; i++) {
      if (i == index) {
         continue;
      }
      for (int j = 0; j < MAX_PATH_INDEX; j++) {
         if (m_paths[i]->index[j] == index) {
            return true;
         }
      }
   }
   return false;
}

bool Waypoint::checkNodes (void) {
   int terrPoints = 0;
   int ctPoints = 0;
   int goalPoints = 0;
   int rescuePoints = 0;
   int i, j;

   for (i = 0; i < g_numWaypoints; i++) {
      int connections = 0;

      for (j = 0; j < MAX_PATH_INDEX; j++) {
         if (m_paths[i]->index[j] != INVALID_WAYPOINT_INDEX) {
            if (m_paths[i]->index[j] > g_numWaypoints) {
               logEntry (true, LL_WARNING, "Waypoint %d connected with invalid Waypoint #%d!", i, m_paths[i]->index[j]);
               return false;
            }
            connections++;
            break;
         }
      }

      if (connections == 0) {
         if (!isConnected (i)) {
            logEntry (true, LL_WARNING, "Waypoint %d isn't connected with any other Waypoint!", i);
            return false;
         }
      }

      if (m_paths[i]->pathNumber != i) {
         logEntry (true, LL_WARNING, "Waypoint %d pathnumber differs from index!", i);
         return false;
      }

      if (m_paths[i]->flags & FLAG_CAMP) {
         if (m_paths[i]->campEndX == 0.0f && m_paths[i]->campEndY == 0.0f) {
            logEntry (true, LL_WARNING, "Waypoint %d Camp-Endposition not set!", i);
            return false;
         }
      }
      else if (m_paths[i]->flags & FLAG_TF_ONLY) {
         terrPoints++;
      }
      else if (m_paths[i]->flags & FLAG_CF_ONLY) {
         ctPoints++;
      }
      else if (m_paths[i]->flags & FLAG_GOAL) {
         goalPoints++;
      }
      else if (m_paths[i]->flags & FLAG_RESCUE) {
         rescuePoints++;
      }

      for (int k = 0; k < MAX_PATH_INDEX; k++) {
         if (m_paths[i]->index[k] != INVALID_WAYPOINT_INDEX) {
            if (m_paths[i]->index[k] >= g_numWaypoints || m_paths[i]->index[k] < INVALID_WAYPOINT_INDEX) {
               logEntry (true, LL_WARNING, "Waypoint %d - Pathindex %d out of Range!", i, k);
               g_engfuncs.pfnSetOrigin (g_hostEntity, m_paths[i]->origin);

               g_waypointOn = true;
               g_editNoclip = true;

               return false;
            }
            else if (m_paths[i]->index[k] == i) {
               logEntry (true, LL_WARNING, "Waypoint %d - Pathindex %d points to itself!", i, k);

               if (g_waypointOn && !engine.isDedicated ()) {
                  g_engfuncs.pfnSetOrigin (g_hostEntity, m_paths[i]->origin);

                  g_waypointOn = true;
                  g_editNoclip = true;
               }
               return false;
            }
         }
      }
   }

   if (g_mapType & MAP_CS) {
      if (rescuePoints == 0) {
         logEntry (true, LL_WARNING, "You didn't set a Rescue Point!");
         return false;
      }
   }
   if (terrPoints == 0) {
      logEntry (true, LL_WARNING, "You didn't set any Terrorist Important Point!");
      return false;
   }
   else if (ctPoints == 0) {
      logEntry (true, LL_WARNING, "You didn't set any CT Important Point!");
      return false;
   }
   else if (goalPoints == 0) {
      logEntry (true, LL_WARNING, "You didn't set any Goal Point!");
      return false;
   }

   // perform DFS instead of floyd-warshall, this shit speedup this process in a bit
   PathWalk walk;
   bool visited[MAX_WAYPOINTS];

   // first check incoming connectivity, initialize the "visited" table
   for (i = 0; i < g_numWaypoints; i++) {
      visited[i] = false;
   }
   walk.push (0); // always check from waypoint number 0

   while (!walk.empty ()) {
      // pop a node from the stack
      const int current = walk.front ();
      walk.shift ();

      visited[current] = true;

      for (j = 0; j < MAX_PATH_INDEX; j++) {
         int index = m_paths[current]->index[j];

         // skip this waypoint as it's already visited
         if (index >= 0 && index < g_numWaypoints && !visited[index]) {
            visited[index] = true;
            walk.push (index);
         }
      }
   }

   for (i = 0; i < g_numWaypoints; i++) {
      if (!visited[i]) {
         logEntry (true, LL_WARNING, "Path broken from Waypoint #0 to Waypoint #%d!", i);

         if (g_waypointOn && !engine.isDedicated ()) {
            g_engfuncs.pfnSetOrigin (g_hostEntity, m_paths[i]->origin);

            g_waypointOn = true;
            g_editNoclip = true;
         }
         return false;
      }
   }

   // then check outgoing connectivity
   IntArray outgoingPaths[MAX_WAYPOINTS]; // store incoming paths for speedup

   for (i = 0; i < g_numWaypoints; i++) {
      outgoingPaths[i].reserve (g_numWaypoints + 1);

      for (j = 0; j < MAX_PATH_INDEX; j++) {
         if (m_paths[i]->index[j] >= 0 && m_paths[i]->index[j] < g_numWaypoints) {
            outgoingPaths[m_paths[i]->index[j]].push (i);
         }
      }
   }

   // initialize the "visited" table
   for (i = 0; i < g_numWaypoints; i++) {
      visited[i] = false;
   }
   walk.clear ();
   walk.push (0); // always check from waypoint number 0

   while (!walk.empty ()) {
      const int current = walk.front (); // pop a node from the stack
      walk.shift ();

      for (auto &outgoing : outgoingPaths[current]) {
         if (visited[outgoing]) {
            continue; // skip this waypoint as it's already visited
         }
         visited[outgoing] = true;
         walk.push (outgoing);
      }
   }
   
   for (i = 0; i < g_numWaypoints; i++) {
      if (!visited[i]) {
         logEntry (true, LL_WARNING, "Path broken from Waypoint #%d to Waypoint #0!", i);

         if (g_waypointOn && !engine.isDedicated ()) {
            g_engfuncs.pfnSetOrigin (g_hostEntity, m_paths[i]->origin);

            g_waypointOn = true;
            g_editNoclip = true;
         }
         return false;
      }
   }
   return true;
}

void Waypoint::initPathMatrix (void) {
   int i, j, k;

   delete[] m_distMatrix;
   delete[] m_pathMatrix;

   m_distMatrix = nullptr;
   m_pathMatrix = nullptr;

   m_distMatrix = new int[g_numWaypoints * g_numWaypoints];
   m_pathMatrix = new int[g_numWaypoints * g_numWaypoints];

   if (loadPathMatrix ()) {
      return; // matrix loaded from file
   }

   for (i = 0; i < g_numWaypoints; i++) {
      for (j = 0; j < g_numWaypoints; j++) {
         *(m_distMatrix + i * g_numWaypoints + j) = 999999;
         *(m_pathMatrix + i * g_numWaypoints + j) = INVALID_WAYPOINT_INDEX;
      }
   }

   for (i = 0; i < g_numWaypoints; i++) {
      for (j = 0; j < MAX_PATH_INDEX; j++) {
         if (m_paths[i]->index[j] >= 0 && m_paths[i]->index[j] < g_numWaypoints) {
            *(m_distMatrix + (i * g_numWaypoints) + m_paths[i]->index[j]) = m_paths[i]->distances[j];
            *(m_pathMatrix + (i * g_numWaypoints) + m_paths[i]->index[j]) = m_paths[i]->index[j];
         }
      }
   }

   for (i = 0; i < g_numWaypoints; i++) {
      *(m_distMatrix + (i * g_numWaypoints) + i) = 0;
   }

   for (k = 0; k < g_numWaypoints; k++) {
      for (i = 0; i < g_numWaypoints; i++) {
         for (j = 0; j < g_numWaypoints; j++) {
            if (*(m_distMatrix + (i * g_numWaypoints) + k) + *(m_distMatrix + (k * g_numWaypoints) + j) < (*(m_distMatrix + (i * g_numWaypoints) + j))) {
               *(m_distMatrix + (i * g_numWaypoints) + j) = *(m_distMatrix + (i * g_numWaypoints) + k) + *(m_distMatrix + (k * g_numWaypoints) + j);
               *(m_pathMatrix + (i * g_numWaypoints) + j) = *(m_pathMatrix + (i * g_numWaypoints) + k);
            }
         }
      }
   }

   // save path matrix to file for faster access
   savePathMatrix ();
}

void Waypoint::savePathMatrix (void) {
   if (g_numWaypoints < 1 || m_waypointsChanged) {
      return;
   }

   File fp (format ("%slearned/%s.pmt", getDataDirectory (), engine.getMapName ()), "wb");

   // unable to open file
   if (!fp.isValid ()) {
      logEntry (false, LL_FATAL, "Failed to open file for writing");
      return;
   }
   ExtensionHeader header;

   memset (header.header, 0, sizeof (header.header));
   strcpy (header.header, FH_MATRIX);

   header.fileVersion = FV_MATRIX;
   header.pointNumber = g_numWaypoints;

   // write header info
   fp.write (&header, sizeof (ExtensionHeader));

   // write path & distance matrix
   fp.write (m_pathMatrix, sizeof (int), g_numWaypoints * g_numWaypoints);
   fp.write (m_distMatrix, sizeof (int), g_numWaypoints * g_numWaypoints);

   // and close the file
   fp.close ();
}

bool Waypoint::loadPathMatrix (void) {
   File fp (format ("%slearned/%s.pmt", getDataDirectory (), engine.getMapName ()), "rb");

   // file doesn't exists return false
   if (!fp.isValid ()) {
      return false;
   }

   ExtensionHeader header;
   memset (&header, 0, sizeof (header));

   // read number of waypoints
   if (fp.read (&header, sizeof (ExtensionHeader)) == 0) {
      fp.close ();
      return false;
   }

   if (header.pointNumber != g_numWaypoints || header.fileVersion != FV_MATRIX) {
      logEntry (true, LL_WARNING, "Pathmatrix damaged (wrong version, or not for this map). Pathmatrix will be rebuilt.");
      fp.close ();

      return false;
   }

   // read path & distance matrixes
   if (fp.read (m_pathMatrix, sizeof (int), g_numWaypoints * g_numWaypoints) == 0) {
      fp.close ();
      return false;
   }

   if (fp.read (m_distMatrix, sizeof (int), g_numWaypoints * g_numWaypoints) == 0) {
      fp.close ();
      return false;
   }
   fp.close (); // and close the file

   return true;
}

int Waypoint::getPathDist (int srcIndex, int destIndex) {
   if (srcIndex < 0 || srcIndex >= g_numWaypoints || destIndex < 0 || destIndex >= g_numWaypoints) {
      return 1;
   }
   return *(m_distMatrix + (srcIndex * g_numWaypoints) + destIndex);
}

void Waypoint::setVisited (int index) {
   if (index < 0 || index >= g_numWaypoints) {
      return;
   }
   if (!isVisited (index) && (m_paths[index]->flags & FLAG_GOAL))
      m_visitedGoals.push (index);
}

void Waypoint::clearVisited (void) {
   m_visitedGoals.clear ();
}

bool Waypoint::isVisited (int index) {
   for (auto &visited : m_visitedGoals) {
      if (visited == index) {
         return true;
      }
   }
   return false;
}

void Waypoint::addBasic (void) {
   // this function creates basic waypoint types on map

   edict_t *ent = nullptr;

   // first of all, if map contains ladder points, create it
   while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityByString (ent, "classname", "func_ladder"))) {
      Vector ladderLeft = ent->v.absmin;
      Vector ladderRight = ent->v.absmax;
      ladderLeft.z = ladderRight.z;

      TraceResult tr;
      Vector up, down, front, back;

      Vector diff = ((ladderLeft - ladderRight) ^ Vector (0.0f, 0.0f, 0.0f)).normalize () * 15.0f;
      front = back = engine.getAbsPos (ent);

      front = front + diff; // front
      back = back - diff; // back

      up = down = front;
      down.z = ent->v.absmax.z;

      engine.testHull (down, up, TRACE_IGNORE_MONSTERS, point_hull, nullptr, &tr);

      if (g_engfuncs.pfnPointContents (up) == CONTENTS_SOLID || tr.flFraction != 1.0f) {
         up = down = back;
         down.z = ent->v.absmax.z;
      }

      engine.testHull (down, up - Vector (0.0f, 0.0f, 1000.0f), TRACE_IGNORE_MONSTERS, point_hull, nullptr, &tr);
      up = tr.vecEndPos;

      Vector point = up + Vector (0.0f, 0.0f, 39.0f);
      m_isOnLadder = true;

      do {
         if (getNearest (point, 50.0f) == INVALID_WAYPOINT_INDEX) {
            push (3, point);
         }
         point.z += 160;
      } while (point.z < down.z - 40.0f);

      point = down + Vector (0.0f, 0.0f, 38.0f);

      if (getNearest (point, 50.0f) == INVALID_WAYPOINT_INDEX) {
         push (3, point);
      }
      m_isOnLadder = false;
   }

   auto autoCreateForEntity = [](int type, const char *entity) {
      edict_t *ent = nullptr;

      while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityByString (ent, "classname", entity))) {
         const Vector &pos = engine.getAbsPos (ent);

         if (waypoints.getNearest (pos, 50.0f) == INVALID_WAYPOINT_INDEX) {
            waypoints.push (type, pos);
         }
      }
   };

   autoCreateForEntity (0, "info_player_deathmatch"); // then terrortist spawnpoints
   autoCreateForEntity (0, "info_player_start"); // then add ct spawnpoints
   autoCreateForEntity (0, "info_vip_start"); // then vip spawnpoint
   autoCreateForEntity (0, "armoury_entity"); // weapons on the map ?

   autoCreateForEntity (4, "func_hostage_rescue"); // hostage rescue zone
   autoCreateForEntity (4, "info_hostage_rescue"); // hostage rescue zone (same as above)

   autoCreateForEntity (100, "func_bomb_target"); // bombspot zone
   autoCreateForEntity (100, "info_bomb_target"); // bombspot zone (same as above)
   autoCreateForEntity (100, "hostage_entity"); // hostage entities
   autoCreateForEntity (100, "func_vip_safetyzone"); // vip rescue (safety) zone
   autoCreateForEntity (100, "func_escapezone"); // terrorist escape zone
}

void Waypoint::eraseFromDisk (void) {
   // this function removes waypoint file from the hard disk

   StringArray forErase;
   const char *map = engine.getMapName ();

   bots.kickEveryone (true);

   // if we're delete waypoint, delete all corresponding to it files
   forErase.push (format ("%s%s.pwf", getDataDirectory (), map)); // waypoint itself
   forErase.push (format ("%slearned/%s.exp", getDataDirectory (), map)); // corresponding to waypoint experience
   forErase.push (format ("%slearned/%s.vis", getDataDirectory (), map)); // corresponding to waypoint vistable
   forErase.push (format ("%slearned/%s.pmt", getDataDirectory (), map)); // corresponding to waypoint path matrix

   for (auto &item : forErase) {
      if (File::exists (const_cast<char *> (item.chars ()))) {
         _unlink (item.chars ());
         logEntry (true, LL_DEFAULT, "File %s, has been deleted from the hard disk", item.chars ());
      }
      else {
         logEntry (true, LL_ERROR, "Unable to open %s", item.chars ());
      }
   }
   init (); // reintialize points
}

const char *Waypoint::getDataDirectory (bool isMemoryFile) {
   static char buffer[256];

   if (isMemoryFile) {
      sprintf (buffer, "addons/yapb/data/");
   }
   else {
      sprintf (buffer, "%s/addons/yapb/data/", engine.getModName ());
   }
   return &buffer[0];
}

void Waypoint::setBombPos (bool reset, const Vector &pos) {
   // this function stores the bomb position as a vector

   if (reset) {
      m_bombPos.nullify ();
      g_bombPlanted = false;

      return;
   }

   if (!pos.empty ()) {
      m_bombPos = pos;
      return;
   }
   edict_t *ent = nullptr;

   while (!engine.isNullEntity (ent = g_engfuncs.pfnFindEntityByString (ent, "classname", "grenade"))) {
      if (strcmp (STRING (ent->v.model) + 9, "c4.mdl") == 0) {
         m_bombPos = engine.getAbsPos (ent);
         break;
      }
   }
}

void Waypoint::startLearnJump (void) {
   m_learnJumpWaypoint = true;
}

void Waypoint::setSearchIndex (int index) {
   m_findWPIndex = index;

   if (m_findWPIndex < g_numWaypoints) {
      engine.print ("Showing Direction to Waypoint #%d", m_findWPIndex);
   }
   else {
      m_findWPIndex = INVALID_WAYPOINT_INDEX;
   }
}

Waypoint::Waypoint (void) {
   cleanupPathMemory ();

   memset (m_visLUT, 0, sizeof (m_visLUT));
   memset (m_waypointDisplayTime, 0, sizeof (m_waypointDisplayTime));
   memset (m_infoBuffer, 0, sizeof (m_infoBuffer));

   m_waypointPaths = false;
   m_endJumpPoint = false;
   m_redoneVisibility = false;
   m_learnJumpWaypoint = false;
   m_waypointsChanged = false;
   m_timeJumpStarted = 0.0f;

   m_lastJumpWaypoint = INVALID_WAYPOINT_INDEX;
   m_cacheWaypointIndex = INVALID_WAYPOINT_INDEX;
   m_findWPIndex = INVALID_WAYPOINT_INDEX;
   m_facingAtIndex = INVALID_WAYPOINT_INDEX;
   m_visibilityIndex = 0;
   m_loadTries = 0;

   m_isOnLadder = false;

   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;

   m_terrorPoints.clear ();
   m_ctPoints.clear ();
   m_goalPoints.clear ();
   m_campPoints.clear ();
   m_rescuePoints.clear ();
   m_sniperPoints.clear ();

   m_distMatrix = nullptr;
   m_pathMatrix = nullptr;

   for (int i = 0; i < MAX_WAYPOINTS; i++) {
      m_paths[i] = nullptr;
   }
}

Waypoint::~Waypoint (void) {
   cleanupPathMemory ();

   delete[] m_distMatrix;
   delete[] m_pathMatrix;

   m_distMatrix = nullptr;
   m_pathMatrix = nullptr;

   for (int i = 0; i < MAX_WAYPOINTS; i++) {
      m_paths[i] = nullptr;
   }
}

void Waypoint::closeSocket (int sock) {
#if defined(PLATFORM_WIN32)
   if (sock != -1) {
      closesocket (sock);
   }
   WSACleanup ();
#else
   if (sock != -1)
      close (sock);
#endif
}

WaypointDownloadError Waypoint::downloadWaypoint (void) {
#if defined(PLATFORM_WIN32)
   WORD requestedVersion = MAKEWORD (1, 1);
   WSADATA wsaData;

   int wsa = WSAStartup (requestedVersion, &wsaData);

   if (wsa != 0) {
      return WDE_SOCKET_ERROR;
   }
#endif

   hostent *host = gethostbyname (yb_waypoint_autodl_host.str ());

   if (host == nullptr) {
      return WDE_SOCKET_ERROR;
   }
   int socketHandle = socket (AF_INET, SOCK_STREAM, 0);

   if (socketHandle < 0) {
      closeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   sockaddr_in dest;

   timeval timeout;
   timeout.tv_sec = 5;
   timeout.tv_usec = 0;

   int result = setsockopt (socketHandle, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof (timeout));

   if (result < 0) {
      closeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   result = setsockopt (socketHandle, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof (timeout));

   if (result < 0) {
      closeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   memset (&dest, 0, sizeof (dest));

   dest.sin_family = AF_INET;
   dest.sin_port = htons (80);
   dest.sin_addr.s_addr = inet_addr (inet_ntoa (*((struct in_addr *)host->h_addr)));

   if (connect (socketHandle, (struct sockaddr *)&dest, (int)sizeof (dest)) == -1) {
      closeSocket (socketHandle);
      return WDE_CONNECT_ERROR;
   }

   String request;
   request.format ("GET /wpdb/%s.pwf HTTP/1.0\r\nAccept: */*\r\nUser-Agent: YaPB/%s\r\nHost: %s\r\n\r\n", engine.getMapName (), PRODUCT_VERSION, yb_waypoint_autodl_host.str ());

   if (send (socketHandle, request.chars (), request.length () + 1, 0) < 1) {
      closeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }

   const int ChunkSize = MAX_PRINT_BUFFER;
   char buffer[ChunkSize] = { 0, };

   bool finished = false;
   int recvPosition = 0;
   int symbolsInLine = 0;

   // scan for the end of the header
   while (!finished && recvPosition < ChunkSize) { 
      if (recv (socketHandle, &buffer[recvPosition], 1, 0) == 0) {
         finished = true;
      }

      // ugly, but whatever
      if (recvPosition > 2 && buffer[recvPosition - 2] == '4' && buffer[recvPosition - 1] == '0' && buffer[recvPosition] == '4') {
         closeSocket (socketHandle);
         return WDE_NOTFOUND_ERROR;
      }

      switch (buffer[recvPosition]) {
      case '\r':
         break;

      case '\n':
         if (symbolsInLine == 0)
            finished = true;

         symbolsInLine = 0;
         break;

      default:
         symbolsInLine++;
         break;
      }
      recvPosition++;
   }

   File fp (waypoints.getWaypointFilename (), "wb");

   if (!fp.isValid ()) {
      closeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   int recvSize = 0;

   do {
      recvSize = recv (socketHandle, buffer, ChunkSize, 0);

      if (recvSize > 0) {
         fp.write (buffer, recvSize);
         fp.flush ();
      }

   } while (recvSize != 0);

   fp.close ();
   closeSocket (socketHandle);

   return WDE_NOERROR;
}
