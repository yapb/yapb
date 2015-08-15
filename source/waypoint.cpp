//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_wptsubfolder ("yb_wptsubfolder", "", VT_NOSERVER);

ConVar yb_waypoint_autodl_host ("yb_waypoint_autodl_host", "yapb.jeefo.net", VT_NOSERVER);
ConVar yb_waypoint_autodl_enable ("yb_waypoint_autodl_enable", "1", VT_NOSERVER);

void Waypoint::Init (void)
{
   // this function initialize the waypoint structures..

   m_learnVelocity.Zero ();
   m_learnPosition.Zero ();
   m_lastWaypoint.Zero ();

   // have any waypoint path nodes been allocated yet?
   if (m_waypointPaths)
   {
      for (int i = 0; i < g_numWaypoints && m_paths[i] != NULL; i++)
      {
         delete m_paths[i];
         m_paths[i] = NULL;
      }
   }
   g_numWaypoints = 0;
}

void Waypoint::AddPath (short int addIndex, short int pathIndex, float distance)
{
   if (addIndex < 0 || addIndex >= g_numWaypoints || pathIndex < 0 || pathIndex >= g_numWaypoints || addIndex == pathIndex)
      return;

   Path *path = m_paths[addIndex];

   // don't allow paths get connected twice
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (path->index[i] == pathIndex)
      {
         AddLogEntry (true, LL_WARNING, "Denied path creation from %d to %d (path already exists)", addIndex, pathIndex);
         return;
      }
   }

   // check for free space in the connection indices
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (path->index[i] == -1)
      {
         path->index[i] = pathIndex;
         path->distances[i] = abs (static_cast <int> (distance));

         AddLogEntry (true, LL_DEFAULT, "Path added from %d to %d", addIndex, pathIndex);
         return;
      }
   }

   // there wasn't any free space. try exchanging it with a long-distance path
   int maxDistance = -9999;
   int slotID = -1;

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (path->distances[i] > maxDistance)
      {
         maxDistance = path->distances[i];
         slotID = i;
      }
   }

   if (slotID != -1)
   {
      AddLogEntry (true, LL_DEFAULT, "Path added from %d to %d", addIndex, pathIndex);

      path->index[slotID] = pathIndex;
      path->distances[slotID] = abs (static_cast <int> (distance));
   }
}

int Waypoint::FindFarest (const Vector &origin, float maxDistance)
{
   // find the farest waypoint to that Origin, and return the index to this waypoint

   int index = -1;

   for (int i = 0; i < g_numWaypoints; i++)
   {
      float distance = (m_paths[i]->origin - origin).GetLength ();

      if (distance > maxDistance)
      {
         index = i;
         maxDistance = distance;
      }
   }
   return index;
}

int Waypoint::FindNearest (const Vector &origin, float minDistance, int flags)
{
   // find the nearest waypoint to that origin and return the index

   int index = -1;

   for (int i = 0; i < g_numWaypoints; i++)
   {
      if (flags != -1 && !(m_paths[i]->flags & flags))
         continue; // if flag not -1 and waypoint has no this flag, skip waypoint

      float distance = (m_paths[i]->origin - origin).GetLengthSquared ();

      if (distance < GET_SQUARE (minDistance))
      {
         index = i;
         minDistance = distance;
      }
   }
   return index;
}

void Waypoint::FindInRadius (Array <int> &radiusHolder, float radius, const Vector &origin, int maxCount)
{
   // returns all waypoints within radius from position

   for (int i = 0; i < g_numWaypoints; i++)
   {
      if (maxCount != -1 && radiusHolder.GetElementNumber () > maxCount)
         break;

      if ((m_paths[i]->origin - origin).GetLengthSquared () < GET_SQUARE (radius))
         radiusHolder.Push (i);
   }
}
void Waypoint::Add (int flags, const Vector &waypointOrigin)
{
   if (IsEntityNull (g_hostEntity))
      return;

   int index = -1, i;
   float distance;

   Vector forward;
   Path *path = NULL;

   bool placeNew = true;
   Vector newOrigin = waypointOrigin;

   if (waypointOrigin.IsZero ())
      newOrigin = g_hostEntity->v.origin;

   if (bots.GetBotsNum () > 0)
      bots.RemoveAll ();

   g_waypointsChanged = true;

   switch (flags)
   {
   case 6:
      index = FindNearest (g_hostEntity->v.origin, 50.0f);

      if (index != -1)
      {
         path = m_paths[index];

         if (!(path->flags & FLAG_CAMP))
         {
            CenterPrint ("This is not Camping Waypoint");
            return;
         }

         MakeVectors (g_hostEntity->v.v_angle);
         forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;

         path->campEndX = forward.x;
         path->campEndY = forward.y;

         // play "done" sound...
         PlaySound (g_hostEntity, "common/wpn_hudon.wav");
      }
      return;

   case 9:
      index = FindNearest (g_hostEntity->v.origin, 50.0f);

      if (index != -1)
      {
         distance = (m_paths[index]->origin - g_hostEntity->v.origin).GetLength ();

         if (distance < 50.0f)
         {
            placeNew = false;
            path = m_paths[index];

            if (flags == 9)
               path->origin = (path->origin + m_learnPosition) * 0.5f;
         }
      }
      else
         newOrigin = m_learnPosition;
      break;

   case 10:
      index = FindNearest (g_hostEntity->v.origin, 50.0f);

      if (index != -1 && m_paths[index] != NULL)
      {
         distance = (m_paths[index]->origin - g_hostEntity->v.origin).GetLength ();

         if (distance < 50.0f)
         {
            placeNew = false;
            path = m_paths[index];

            flags = 0;

            for (i = 0; i < MAX_PATH_INDEX; i++)
               flags += path->connectionFlags[i];

            if (flags == 0)
               path->origin = (path->origin + g_hostEntity->v.origin) * 0.5f;
         }
      }
      break;
   }

   if (placeNew)
   {
      if (g_numWaypoints >= MAX_WAYPOINTS)
         return;

      index = g_numWaypoints;

      m_paths[index] = new Path;

      if (m_paths[index] == NULL)
         TerminateOnMalloc ();

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

      for (i = 0; i < MAX_PATH_INDEX; i++)
      {
         path->index[i] = -1;
         path->distances[i] = 0;

         path->connectionFlags[i] = 0;
         path->connectionVelocity[i].Zero ();
      }

      // store the last used waypoint for the auto waypoint code...
      m_lastWaypoint = g_hostEntity->v.origin;
   }

   // set the time that this waypoint was originally displayed...
   m_waypointDisplayTime[index] = 0;

   if (flags == 9)
      m_lastJumpWaypoint = index;
   else if (flags == 10)
   {
      distance = (m_paths[m_lastJumpWaypoint]->origin - g_hostEntity->v.origin).GetLength ();
      AddPath (m_lastJumpWaypoint, index, distance);

      for (i = 0; i < MAX_PATH_INDEX; i++)
      {
         if (m_paths[m_lastJumpWaypoint]->index[i] == index)
         {
            m_paths[m_lastJumpWaypoint]->connectionFlags[i] |= PATHFLAG_JUMP;
            m_paths[m_lastJumpWaypoint]->connectionVelocity[i] = m_learnVelocity;

            break;
         }
      }

      CalculateWayzone (index);
      return;
   }

   if (g_hostEntity->v.flags & FL_DUCKING)
      path->flags |= FLAG_CROUCH;  // set a crouch waypoint

   if (g_hostEntity->v.movetype == MOVETYPE_FLY)
   {
      path->flags |= FLAG_LADDER;
      MakeVectors (g_hostEntity->v.v_angle);

      forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;
      path->campStartY = forward.y;
   }
   else if (m_isOnLadder)
      path->flags |= FLAG_LADDER;

   switch (flags)
   {
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

      MakeVectors (g_hostEntity->v.v_angle);
      forward = g_hostEntity->v.origin + g_hostEntity->v.view_ofs + g_pGlobals->v_forward * 640.0f;

      path->campStartX = forward.x;
      path->campStartY = forward.y;
      break;

   case 100:
      path->flags |= FLAG_GOAL;
      break;
   }

   // Ladder waypoints need careful connections
   if (path->flags & FLAG_LADDER)
   {
      float minDistance = 9999.0f;
      int destIndex = -1;

      TraceResult tr;

      // calculate all the paths to this new waypoint
      for (i = 0; i < g_numWaypoints; i++)
      {
         if (i == index)
            continue; // skip the waypoint that was just added

         // Other ladder waypoints should connect to this
         if (m_paths[i]->flags & FLAG_LADDER)
         {
            // check if the waypoint is reachable from the new one
            TraceLine (newOrigin, m_paths[i]->origin, true, g_hostEntity, &tr);

            if (tr.flFraction > TRACE_FRACTION_EQ && fabs (newOrigin.x - m_paths[i]->origin.x) < 64.0f && fabs (newOrigin.y - m_paths[i]->origin.y) < 64.0f && fabs (newOrigin.z - m_paths[i]->origin.z) < g_autoPathDistance)
            {
               distance = (m_paths[i]->origin - newOrigin).GetLength ();

               AddPath (index, i, distance);
               AddPath (i, index, distance);
            }
         }
         else
         {
            // check if the waypoint is reachable from the new one
            if (IsNodeReachable (newOrigin, m_paths[i]->origin) || IsNodeReachable (m_paths[i]->origin, newOrigin))
            {
               distance = (m_paths[i]->origin - newOrigin).GetLength ();

               if (distance < minDistance)
               {
                  destIndex = i;
                  minDistance = distance;
               }
            }
         }
      }

      if (destIndex > -1 && destIndex < g_numWaypoints)
      {
         // check if the waypoint is reachable from the new one (one-way)
         if (IsNodeReachable (newOrigin, m_paths[destIndex]->origin))
         {
            distance = (m_paths[destIndex]->origin - newOrigin).GetLength ();
            AddPath (index, destIndex, distance);
         }

         // check if the new one is reachable from the waypoint (other way)
         if (IsNodeReachable (m_paths[destIndex]->origin, newOrigin))
         {
            distance = (m_paths[destIndex]->origin - newOrigin).GetLength ();
            AddPath (destIndex, index, distance);
         }
      }
   }
   else
   {
      // calculate all the paths to this new waypoint
      for (i = 0; i < g_numWaypoints; i++)
      {
         if (i == index)
            continue; // skip the waypoint that was just added

         // check if the waypoint is reachable from the new one (one-way)
         if (IsNodeReachable (newOrigin, m_paths[i]->origin))
         {
            distance = (m_paths[i]->origin - newOrigin).GetLength ();
            AddPath (index, i, distance);
         }

         // check if the new one is reachable from the waypoint (other way)
         if (IsNodeReachable (m_paths[i]->origin, newOrigin))
         {
            distance = (m_paths[i]->origin - newOrigin).GetLength ();
            AddPath (i, index, distance);
         }
      }
   }
   PlaySound (g_hostEntity, "weapons/xbow_hit1.wav");
   CalculateWayzone (index); // calculate the wayzone of this waypoint
}

void Waypoint::Delete (void)
{
   g_waypointsChanged = true;

   if (g_numWaypoints < 1)
      return;

   if (bots.GetBotsNum () > 0)
      bots.RemoveAll ();

   int index = FindNearest (g_hostEntity->v.origin, 50.0f);

   if (index == -1)
      return;

   Path *path = NULL;
   InternalAssert (m_paths[index] != NULL);

   int i, j;

   for (i = 0; i < g_numWaypoints; i++) // delete all references to Node
   {
      path = m_paths[i];

      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (path->index[j] == index)
         {
            path->index[j] = -1;  // unassign this path
            path->connectionFlags[j] = 0;
            path->distances[j] = 0;
            path->connectionVelocity[j].Zero ();
         }
      }
   }

   for (i = 0; i < g_numWaypoints; i++)
   {
      path = m_paths[i];

      if (path->pathNumber > index) // if pathnumber bigger than deleted node...
         path->pathNumber--;

      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (path->index[j] > index)
            path->index[j]--;
      }
   }

   // free deleted node
   delete m_paths[index];
   m_paths[index] = NULL;

   // rotate path array down
   for (i = index; i < g_numWaypoints - 1; i++)
      m_paths[i] = m_paths[i + 1];

   g_numWaypoints--;
   m_waypointDisplayTime[index] = 0;

   PlaySound (g_hostEntity, "weapons/mine_activate.wav");
}

void Waypoint::ToggleFlags (int toggleFlag)
{
   // this function allow manually changing flags

   int index = FindNearest (g_hostEntity->v.origin, 50.0f);

   if (index != -1)
   {
      if (m_paths[index]->flags & toggleFlag)
         m_paths[index]->flags &= ~toggleFlag;

      else if (!(m_paths[index]->flags & toggleFlag))
      {
         if (toggleFlag == FLAG_SNIPER && !(m_paths[index]->flags & FLAG_CAMP))
         {
            AddLogEntry (true, LL_ERROR, "Cannot assign sniper flag to waypoint #%d. This is not camp waypoint", index);
            return;
         }
         m_paths[index]->flags |= toggleFlag;
      }

      // play "done" sound...
      PlaySound (g_hostEntity, "common/wpn_hudon.wav");
   }
}

void Waypoint::SetRadius (int radius)
{
   // this function allow manually setting the zone radius

   int index = FindNearest (g_hostEntity->v.origin, 50.0f);

   if (index != -1)
   {
      m_paths[index]->radius = static_cast <float> (radius);

      // play "done" sound...
      PlaySound (g_hostEntity, "common/wpn_hudon.wav");
   }
}

bool Waypoint::IsConnected (int pointA, int pointB)
{
   // this function checks if waypoint A has a connection to waypoint B

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (m_paths[pointA]->index[i] == pointB)
         return true;
   }
   return false;
}

int Waypoint::GetFacingIndex (void)
{
   // this function finds waypoint the user is pointing at.

   int pointedIndex = -1;
   float viewCone[3] = {0.0f, 0.0f, 0.0f};

   // find the waypoint the user is pointing at
   for (int i = 0; i < g_numWaypoints; i++)
   {
      if ((m_paths[i]->origin - g_hostEntity->v.origin).GetLengthSquared () > 250000.0f)
         continue;

      // get the current view cone
      viewCone[0] = GetShootingConeDeviation (g_hostEntity, &m_paths[i]->origin);
      Vector bound = m_paths[i]->origin - Vector (0.0f, 0.0f, (m_paths[i]->flags & FLAG_CROUCH) ? 8.0f : 15.0f);

      // get the current view cone
      viewCone[1] = GetShootingConeDeviation (g_hostEntity, &bound);
      bound = m_paths[i]->origin + Vector (0.0f, 0.0f, (m_paths[i]->flags & FLAG_CROUCH) ? 8.0f : 15.0f);

      // get the current view cone
      viewCone[2] = GetShootingConeDeviation (g_hostEntity, &bound);

      // check if we can see it
      if (viewCone[0] < 0.998f && viewCone[1] < 0.997f && viewCone[2] < 0.997f)
         continue;

      pointedIndex = i;
   }
   return pointedIndex;
}

void Waypoint::CreatePath (char dir)
{
   // this function allow player to manually create a path from one waypoint to another

   int nodeFrom = FindNearest (g_hostEntity->v.origin, 50.0f);

   if (nodeFrom == -1)
   {
      CenterPrint ("Unable to find nearest waypoint in 50 units");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (nodeTo < 0 || nodeTo >= g_numWaypoints)
   {
      if (m_cacheWaypointIndex >= 0 && m_cacheWaypointIndex < g_numWaypoints)
         nodeTo = m_cacheWaypointIndex;
      else
      {
         CenterPrint ("Unable to find destination waypoint");
         return;
      }
   }

   if (nodeTo == nodeFrom)
   {
      CenterPrint ("Unable to connect waypoint with itself");
      return;
   }

   float distance = (m_paths[nodeTo]->origin - m_paths[nodeFrom]->origin).GetLength ();

   if (dir == CONNECTION_OUTGOING)
      AddPath (nodeFrom, nodeTo, distance);
   else if (dir == CONNECTION_INCOMING)
      AddPath (nodeTo, nodeFrom, distance);
   else
   {
      AddPath (nodeFrom, nodeTo, distance);
      AddPath (nodeTo, nodeFrom, distance);
   }

   PlaySound (g_hostEntity, "common/wpn_hudon.wav");
   g_waypointsChanged = true;
}


void Waypoint::DeletePath (void)
{
   // this function allow player to manually remove a path from one waypoint to another

   int nodeFrom = FindNearest (g_hostEntity->v.origin, 50.0f);
   int index = 0;

   if (nodeFrom == -1)
   {
      CenterPrint ("Unable to find nearest waypoint in 50 units");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (nodeTo < 0 || nodeTo >= g_numWaypoints)
   {
      if (m_cacheWaypointIndex >= 0 && m_cacheWaypointIndex < g_numWaypoints)
         nodeTo = m_cacheWaypointIndex;
      else
      {
         CenterPrint ("Unable to find destination waypoint");
         return;
      }
   }

   for (index = 0; index < MAX_PATH_INDEX; index++)
   {
      if (m_paths[nodeFrom]->index[index] == nodeTo)
      {
         g_waypointsChanged = true;

         m_paths[nodeFrom]->index[index] = -1; // unassigns this path
         m_paths[nodeFrom]->distances[index] = 0;
         m_paths[nodeFrom]->connectionFlags[index] = 0;
         m_paths[nodeFrom]->connectionVelocity[index].Zero ();

         PlaySound (g_hostEntity, "weapons/mine_activate.wav");
         return;
      }
   }

   // not found this way ? check for incoming connections then
   index = nodeFrom;
   nodeFrom = nodeTo;
   nodeTo = index;

   for (index = 0; index < MAX_PATH_INDEX; index++)
   {
      if (m_paths[nodeFrom]->index[index] == nodeTo)
      {
         g_waypointsChanged = true;

         m_paths[nodeFrom]->index[index] = -1; // unassign this path
         m_paths[nodeFrom]->distances[index] = 0;

         m_paths[nodeFrom]->connectionFlags[index] = 0;
         m_paths[nodeFrom]->connectionVelocity[index].Zero ();

         PlaySound (g_hostEntity, "weapons/mine_activate.wav");
         return;
      }
   }
   CenterPrint ("There is already no path on this waypoint");
}

void Waypoint::CacheWaypoint (void)
{
   int node = FindNearest (g_hostEntity->v.origin, 50.0f);

   if (node == -1)
   {
      m_cacheWaypointIndex = -1;
      CenterPrint ("Cached waypoint cleared (nearby point not found in 50 units range)");

      return;
   }
   m_cacheWaypointIndex = node;
   CenterPrint ("Waypoint #%d has been put into memory", m_cacheWaypointIndex);
}

void Waypoint::CalculateWayzone (int index)
{
   // calculate "wayzones" for the nearest waypoint to pentedict (meaning a dynamic distance area to vary waypoint origin)

   Path *path = m_paths[index];
   Vector start, direction;

   TraceResult tr;
   bool wayBlocked = false;

   if ((path->flags & (FLAG_LADDER | FLAG_GOAL | FLAG_CAMP | FLAG_RESCUE | FLAG_CROUCH)) || m_learnJumpWaypoint)
   {
      path->radius = 0.0f;
      return;
   }

   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      if (path->index[i] != -1 && (m_paths[path->index[i]]->flags & FLAG_LADDER))
      {
         path->radius = 0.0f;
         return;
      }
   }

   for (int scanDistance = 16; scanDistance < 128; scanDistance += 16)
   {
      start = path->origin;
      MakeVectors (Vector::GetZero ());

      direction = g_pGlobals->v_forward * scanDistance;
      direction = direction.ToAngles ();

      path->radius = scanDistance;

      for (float circleRadius = 0.0f; circleRadius < 180.0f; circleRadius += 5.0f)
      {
         MakeVectors (direction);

         Vector radiusStart = start - g_pGlobals->v_forward * scanDistance;
         Vector radiusEnd = start + g_pGlobals->v_forward * scanDistance;

         TraceHull (radiusStart, radiusEnd, true, head_hull, NULL, &tr);

         if (tr.flFraction <= TRACE_FRACTION_EQ)
         {
            TraceLine (radiusStart, radiusEnd, true, NULL, &tr);

            if (FClassnameIs (tr.pHit, "func_door") || FClassnameIs (tr.pHit, "func_door_rotating"))
            {
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

         TraceHull (dropStart, dropEnd, true, head_hull, NULL, &tr);

         if (tr.flFraction > TRACE_FRACTION_EQ)
         {
            wayBlocked = true;
            path->radius -= 16.0f;

            break;
         }
         dropStart = start - g_pGlobals->v_forward * scanDistance;
         dropEnd = dropStart - Vector (0.0f, 0.0f, scanDistance + 60.0f);

         TraceHull (dropStart, dropEnd, true, head_hull, NULL, &tr);

         if (tr.flFraction > TRACE_FRACTION_EQ)
         {
            wayBlocked = true;
            path->radius -= 16.0f;
            break;
         }

         radiusEnd.z += 34.0f;
         TraceHull (radiusStart, radiusEnd, true, head_hull, NULL, &tr);

         if (tr.flFraction <= TRACE_FRACTION_EQ)
         {
            wayBlocked = true;
            path->radius -= 16.0f;
            break;
         }

         direction.y = AngleNormalize (direction.y + circleRadius);
      }
      if (wayBlocked)
         break;
   }
   path->radius -= 16.0f;

   if (path->radius < 0.0f)
      path->radius = 0.0f;
}

void Waypoint::SaveExperienceTab (void)
{
   ExtensionHeader header;

   if ((g_numWaypoints <= 0) || g_waypointsChanged)
      return;

   memset (header.header, 0, sizeof (header.header));
   strcpy (header.header, FH_EXPERIENCE);

   header.fileVersion = FV_EXPERIENCE;
   header.pointNumber = g_numWaypoints;

   ExperienceSave *experienceSave = new ExperienceSave[g_numWaypoints * g_numWaypoints];

   for (int i = 0; i < g_numWaypoints; i++)
   {
      for (int j = 0; j < g_numWaypoints; j++)
      {
         (experienceSave + (i * g_numWaypoints) + j)->team0Damage = (g_experienceData + (i * g_numWaypoints) + j)->team0Damage >> 3;
         (experienceSave + (i * g_numWaypoints) + j)->team1Damage = (g_experienceData + (i * g_numWaypoints) + j)->team1Damage >> 3;
         (experienceSave + (i * g_numWaypoints) + j)->team0Value = (g_experienceData + (i * g_numWaypoints) + j)->team0Value / 8;
         (experienceSave + (i * g_numWaypoints) + j)->team1Value = (g_experienceData + (i * g_numWaypoints) + j)->team1Value / 8;
      }
   }

   int result = Compressor::Compress (FormatBuffer ("%slearned/%s.exp", GetWaypointDir (), GetMapName ()), (unsigned char *)&header, sizeof (ExtensionHeader), (unsigned char *)experienceSave, g_numWaypoints * g_numWaypoints * sizeof (ExperienceSave));

   delete [] experienceSave;

   if (result == -1)
   {
      AddLogEntry (true, LL_ERROR, "Couldn't save experience data");
      return;
   }
}

void Waypoint::InitExperienceTab (void)
{
   int i, j;

   delete [] g_experienceData;
   g_experienceData = NULL;

   if (g_numWaypoints < 1)
      return;

   g_experienceData = new Experience[g_numWaypoints * g_numWaypoints];

   g_highestDamageCT = 1;
   g_highestDamageT = 1;

   // initialize table by hand to correct values, and NOT zero it out
   for (i = 0; i < g_numWaypoints; i++)
   {
      for (j = 0; j < g_numWaypoints; j++)
      {
         (g_experienceData + (i * g_numWaypoints) + j)->team0DangerIndex = -1;
         (g_experienceData + (i * g_numWaypoints) + j)->team1DangerIndex = -1;
         (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = 0;
         (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = 0;
         (g_experienceData + (i * g_numWaypoints) + j)->team0Value = 0;
         (g_experienceData + (i * g_numWaypoints) + j)->team1Value = 0;
      }
   }
   File fp (FormatBuffer ("%slearned/%s.exp", GetWaypointDir (), GetMapName ()), "rb");

   // if file exists, read the experience data from it
   if (fp.IsValid ())
   {
      ExtensionHeader header;
      memset (&header, 0, sizeof (header));

      if (fp.Read (&header, sizeof (header)) == 0)
      {
         AddLogEntry (true, LL_ERROR, "Experience data damaged (unable to read header)");

         fp.Close ();
         return;
      }
      fp.Close ();

      if (strncmp (header.header, FH_EXPERIENCE, strlen (FH_EXPERIENCE)) == 0)
      {
         if (header.fileVersion == FV_EXPERIENCE && header.pointNumber == g_numWaypoints)
         {
            ExperienceSave *experienceLoad = new ExperienceSave[g_numWaypoints * g_numWaypoints];

            Compressor::Uncompress (FormatBuffer ("%slearned/%s.exp", GetWaypointDir (), GetMapName ()), sizeof (ExtensionHeader), (unsigned char *)experienceLoad, g_numWaypoints * g_numWaypoints * sizeof (ExperienceSave));

            for (i = 0; i < g_numWaypoints; i++)
            {
               for (j = 0; j < g_numWaypoints; j++)
               {
                  if (i == j)
                  {
                     (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = (unsigned short) ((experienceLoad + (i * g_numWaypoints) + j)->team0Damage);
                     (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = (unsigned short) ((experienceLoad + (i * g_numWaypoints) + j)->team1Damage);

                     if ((g_experienceData + (i * g_numWaypoints) + j)->team0Damage > g_highestDamageT)
                        g_highestDamageT = (g_experienceData + (i * g_numWaypoints) + j)->team0Damage;
       
                     if ((g_experienceData + (i * g_numWaypoints) + j)->team1Damage > g_highestDamageCT)
                        g_highestDamageCT = (g_experienceData + (i * g_numWaypoints) + j)->team1Damage;
                  }
                  else
                  {
                     (g_experienceData + (i * g_numWaypoints) + j)->team0Damage = (unsigned short) ((experienceLoad + (i * g_numWaypoints) + j)->team0Damage) << 3;
                     (g_experienceData + (i * g_numWaypoints) + j)->team1Damage = (unsigned short) ((experienceLoad + (i * g_numWaypoints) + j)->team1Damage) << 3;
                  }

                  (g_experienceData + (i * g_numWaypoints) + j)->team0Value = (signed short) ((experienceLoad + i * (g_numWaypoints) + j)->team0Value) * 8;
                  (g_experienceData + (i * g_numWaypoints) + j)->team1Value = (signed short) ((experienceLoad + i * (g_numWaypoints) + j)->team1Value) * 8;
               }
            }
            delete [] experienceLoad;
         }
         else
            AddLogEntry (true, LL_WARNING, "Experience data damaged (wrong version, or not for this map)");
      }
   }
}

void Waypoint::SaveVisibilityTab (void)
{
   if (g_numWaypoints == 0)
      return;

   ExtensionHeader header;
   memset (&header, 0, sizeof (ExtensionHeader));

   // parse header
   memset (header.header, 0, sizeof (header.header));
   strcpy (header.header, FH_VISTABLE);

   header.fileVersion = FV_VISTABLE;
   header.pointNumber = g_numWaypoints;

   File fp (FormatBuffer ("%slearned/%s.vis", GetWaypointDir (), GetMapName ()), "wb");

   if (!fp.IsValid ())
   {
      AddLogEntry (true, LL_ERROR, "Failed to open visibility table for writing");
      return;
   }
   fp.Close ();

   Compressor::Compress (FormatBuffer ("%slearned/%s.vis", GetWaypointDir (), GetMapName ()), (unsigned char *) &header, sizeof (ExtensionHeader), (unsigned char *) m_visLUT, MAX_WAYPOINTS * (MAX_WAYPOINTS / 4) * sizeof (byte));
}

void Waypoint::InitVisibilityTab (void)
{
   if (g_numWaypoints == 0)
      return;

   ExtensionHeader header;

   File fp (FormatBuffer ("%slearned/%s.vis", GetWaypointDir (), GetMapName ()), "rb");
   m_redoneVisibility = false;

   if (!fp.IsValid ())
   {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      AddLogEntry (true, LL_DEFAULT, "Vistable, not exists, vistable will be rebuilded");
      return;
   }

   // read the header of the file
   if (fp.Read (&header, sizeof (header)) == 0)
   {
      AddLogEntry (true, LL_ERROR, "Vistable damaged (unable to read header)");

      fp.Close ();
      return;
   }

   if (strncmp (header.header, FH_VISTABLE, strlen (FH_VISTABLE)) != 0 || header.fileVersion != FV_VISTABLE || header.pointNumber != g_numWaypoints)
   {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      AddLogEntry (true, LL_WARNING, "Visibility table damaged (wrong version, or not for this map), vistable will be rebuilded.");
      fp.Close ();

      return;
   }
   int result = Compressor::Uncompress (FormatBuffer ("%slearned/%s.vis", GetWaypointDir (), GetMapName ()), sizeof (ExtensionHeader), (unsigned char *) m_visLUT, MAX_WAYPOINTS * (MAX_WAYPOINTS / 4) * sizeof (byte));

   if (result == -1)
   {
      m_visibilityIndex = 0;
      m_redoneVisibility = true;

      AddLogEntry (true, LL_ERROR, "Failed to decode vistable, vistable will be rebuilded.");
      fp.Close ();

      return;
   }
   fp.Close ();
}

void Waypoint::InitTypes (void)
{
   m_terrorPoints.RemoveAll ();
   m_ctPoints.RemoveAll ();
   m_goalPoints.RemoveAll ();
   m_campPoints.RemoveAll ();
   m_rescuePoints.RemoveAll ();
   m_sniperPoints.RemoveAll ();
   m_visitedGoals.RemoveAll ();

   for (int i = 0; i < g_numWaypoints; i++)
   {
      if (m_paths[i]->flags & FLAG_TF_ONLY)
         m_terrorPoints.Push (i);
      else if (m_paths[i]->flags & FLAG_CF_ONLY)
         m_ctPoints.Push (i);
      else if (m_paths[i]->flags & FLAG_GOAL)
         m_goalPoints.Push (i);
      else if (m_paths[i]->flags & FLAG_CAMP)
         m_campPoints.Push (i);
      else if (m_paths[i]->flags & FLAG_SNIPER)
         m_sniperPoints.Push (i);
      else if (m_paths[i]->flags & FLAG_RESCUE)
         m_rescuePoints.Push (i);
   }
}

bool Waypoint::Load (void)
{
   File fp (CheckSubfolderFile (), "rb");

   WaypointHeader header;
   memset (&header, 0, sizeof (header));

   if (fp.IsValid ())
   {
      if (fp.Read (&header, sizeof (header)) == 0)
      {
         sprintf (m_infoBuffer, "%s.pwf - damaged waypoint file (unable to read header)", GetMapName ());
         AddLogEntry (true, LL_ERROR, m_infoBuffer);

         fp.Close ();
         return false;
      }

      if (strncmp (header.header, FH_WAYPOINT, strlen (FH_WAYPOINT)) == 0)
      {
         if (header.fileVersion != FV_WAYPOINT)
         {
            sprintf (m_infoBuffer, "%s.pwf - incorrect waypoint file version (expected '%d' found '%ld')", GetMapName (), FV_WAYPOINT, header.fileVersion);
            AddLogEntry (true, LL_ERROR, m_infoBuffer);

            fp.Close ();
            return false;
         }
         else if (stricmp (header.mapName, GetMapName ()))
         {
            sprintf (m_infoBuffer, "%s.pwf - hacked waypoint file, file name doesn't match waypoint header information (mapname: '%s', header: '%s')", GetMapName (), GetMapName (), header.mapName);
            AddLogEntry (true, LL_ERROR, m_infoBuffer);

            fp.Close ();
            return false;
         }
         else
         {
            if (header.pointNumber == 0 || header.pointNumber > MAX_WAYPOINTS)
            {
               sprintf (m_infoBuffer, "%s.pwf - waypoint file contains illegal number of waypoints (mapname: '%s', header: '%s')", GetMapName (), GetMapName (), header.mapName);
               AddLogEntry (true, LL_ERROR, m_infoBuffer);

               fp.Close ();
               return false;
            }

            Init ();
            g_numWaypoints = header.pointNumber;

            for (int i = 0; i < g_numWaypoints; i++)
            {
               m_paths[i] = new Path;

               if (m_paths[i] == NULL)
                  TerminateOnMalloc ();

               if (fp.Read (m_paths[i], sizeof (Path)) == 0)
               {
                  sprintf (m_infoBuffer, "%s.pwf - truncated waypoint file (count: %d, need: %d)", GetMapName (), i, g_numWaypoints);
                  AddLogEntry (true, LL_ERROR, m_infoBuffer);

                  fp.Close ();
                  return false;
               }
            }
            m_waypointPaths = true;
         }
      }
      else
      {
         sprintf (m_infoBuffer, "%s.pwf is not a yapb waypoint file (header found '%s' needed '%s'", GetMapName (), header.header, FH_WAYPOINT);
         AddLogEntry (true, LL_ERROR, m_infoBuffer);

         fp.Close ();
         return false;
      }
      fp.Close ();
   }
   else
   {
      if (yb_waypoint_autodl_enable.GetBool ())
      {
         AddLogEntry (true, LL_DEFAULT, "%s.pwf does not exist, trying to download from waypoint database", GetMapName ());

         WaypointDownloader dl;
         WaypointDownloadError status = dl.DoDownload ();
 
         if (status == WDE_SOCKET_ERROR)
         {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Socket error.", GetMapName ());
            AddLogEntry (true, LL_ERROR, m_infoBuffer);

            yb_waypoint_autodl_enable.SetInt (0);

            return false;
         }
         else if (status == WDE_CONNECT_ERROR)
         {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Connection problems.", GetMapName ());
            AddLogEntry (true, LL_ERROR, m_infoBuffer);

            yb_waypoint_autodl_enable.SetInt (0);

            return false;
         }
         else if (status == WDE_NOTFOUND_ERROR)
         {
            sprintf (m_infoBuffer, "%s.pwf does not exist. Can't autodownload. Waypoint not available.", GetMapName ());
            AddLogEntry (true, LL_ERROR, m_infoBuffer);

            return false;
         }
         else
         {
            AddLogEntry (true, LL_DEFAULT, "%s.pwf was downloaded from waypoint database. Trying to load...", GetMapName ());
            return Load ();
         }
      }
      sprintf (m_infoBuffer, "%s.pwf does not exist", GetMapName ());
      AddLogEntry (true, LL_ERROR, m_infoBuffer);

      return false;
   }

   if (strncmp (header.author, "official", 7) == 0)
      sprintf (m_infoBuffer, "Using Official Waypoint File");
   else
      sprintf (m_infoBuffer, "Using waypoint file by: %s", header.author);

   for (int i = 0; i < g_numWaypoints; i++)
      m_waypointDisplayTime[i] = 0.0;

   InitPathMatrix ();
   InitTypes ();

   g_waypointsChanged = false;
   g_highestKills = 1;

   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;

   InitVisibilityTab ();
   InitExperienceTab ();

   bots.InitQuota ();

   extern ConVar yb_debug_goal;
   yb_debug_goal.SetInt (-1);

   return true;
}

void Waypoint::Save (void)
{
   WaypointHeader header;

   memset (header.mapName, 0, sizeof (header.mapName));
   memset (header.author , 0, sizeof (header.author));
   memset (header.header, 0, sizeof (header.header));

   strcpy (header.header, FH_WAYPOINT);
   strncpy (header.author, STRING (g_hostEntity->v.netname), SIZEOF_CHAR (header.author));
   strncpy (header.mapName, GetMapName (), SIZEOF_CHAR (header.mapName));

   header.mapName[31] = 0;
   header.fileVersion = FV_WAYPOINT;
   header.pointNumber = g_numWaypoints;

   File fp (CheckSubfolderFile (), "wb");

   // file was opened
   if (fp.IsValid ())
   {
      // write the waypoint header to the file...
      fp.Write (&header, sizeof (header), 1);

      // save the waypoint paths...
      for (int i = 0; i < g_numWaypoints; i++)
         fp.Write (m_paths[i], sizeof (Path));

      fp.Close ();
   }
   else
      AddLogEntry (true, LL_ERROR, "Error writing '%s.pwf' waypoint file", GetMapName ());
}

String Waypoint::CheckSubfolderFile (void)
{
   String returnFile = "";

   if (!IsNullString (yb_wptsubfolder.GetString ()))
      returnFile += (String (yb_wptsubfolder.GetString ()) + "/");

   returnFile = FormatBuffer ("%s%s%s.pwf", GetWaypointDir (), returnFile.GetBuffer (), GetMapName ());

   if (File::Accessible (returnFile))
      return returnFile;

   return FormatBuffer ("%s%s.pwf", GetWaypointDir (), GetMapName ());
}

float Waypoint::GetTravelTime (float maxSpeed, const Vector &src, const Vector &origin)
{
   // this function returns 2D traveltime to a position

   return (origin - src).GetLength2D () / maxSpeed;
}

bool Waypoint::Reachable (Bot *bot, int index)
{
   // this function return wether bot able to reach index waypoint or not, depending on several factors.

   if (bot == NULL || index < 0 || index >= g_numWaypoints)
      return false;

   Vector src = bot->pev->origin;
   Vector dest = m_paths[index]->origin;

   float distance = (dest - src).GetLength ();

   // check is destination is close to us enoguh
   if (distance >= 150.0f)
      return false;

   TraceResult tr;
   TraceLine (src, dest, true, bot->GetEntity (), &tr);

   // if waypoint is visible from current position (even behind head)...
   if (tr.flFraction > TRACE_FRACTION_EQ)
   {
      if (bot->pev->waterlevel == 2 || bot->pev->waterlevel == 3)
         return true;

      float distance2D = (dest - src).GetLength2D ();

      // is destination waypoint higher that source (62 is max jump height), or destination waypoint higher that source
      if (((dest.z > src.z + 62.0f || dest.z < src.z - 100.0f) && (!(m_paths[index]->flags & FLAG_LADDER))) || distance2D >= 120.0f)
         return false; // unable to reach this one

      return true;
   }
   return false;
}

bool Waypoint::IsNodeReachable (const Vector &src, const Vector &destination)
{
   TraceResult tr;

   float distance = (destination - src).GetLength ();

   // is the destination not close enough?
   if (distance > g_autoPathDistance)
      return false;

   // check if we go through a func_illusionary, in which case return false
   TraceHull (src, destination, ignore_monsters, head_hull, g_hostEntity, &tr);

   if (!IsEntityNull (tr.pHit) && strcmp ("func_illusionary", STRING (tr.pHit->v.classname)) == 0)
      return false; // don't add pathwaypoints through func_illusionaries

   // check if this waypoint is "visible"...
   TraceLine (src, destination, ignore_monsters, g_hostEntity, &tr);

   // if waypoint is visible from current position (even behind head)...
   if (tr.flFraction > TRACE_FRACTION_EQ || strncmp ("func_door", STRING (tr.pHit->v.classname), 9) == 0)
   {
      // if it's a door check if nothing blocks behind
      if (strncmp ("func_door", STRING (tr.pHit->v.classname), 9) == 0)
      {
         TraceLine (tr.vecEndPos, destination, ignore_monsters, tr.pHit, &tr);

         if (tr.flFraction <= TRACE_FRACTION_EQ)
            return false;
      }

      // check for special case of both waypoints being in water...
      if (POINT_CONTENTS (src) == CONTENTS_WATER && POINT_CONTENTS (destination) == CONTENTS_WATER)
          return true; // then they're reachable each other

      // is dest waypoint higher than src? (45 is max jump height)
      if (destination.z > src.z + 45.0f)
      {
         Vector sourceNew = destination;
         Vector destinationNew = destination;
         destinationNew.z = destinationNew.z - 50.0f; // straight down 50 units

         TraceLine (sourceNew, destinationNew, ignore_monsters, g_hostEntity, &tr);

         // check if we didn't hit anything, if not then it's in mid-air
         if (tr.flFraction > TRACE_FRACTION_EQ)
            return false; // can't reach this one
      }

      // check if distance to ground drops more than step height at points between source and destination...
      Vector direction = (destination - src).Normalize(); // 1 unit long
      Vector check = src, down = src;

      down.z = down.z - 1000.0f; // straight down 1000 units

      TraceLine (check, down, ignore_monsters, g_hostEntity, &tr);

      float lastHeight = tr.flFraction * 1000.0f; // height from ground
      distance = (destination - check).GetLength (); // distance from goal

      while (distance > 10.0f)
      {
         // move 10 units closer to the goal...
         check = check + (direction * 10.0f);

         down = check;
         down.z = down.z - 1000.0f; // straight down 1000 units

         TraceLine (check, down, ignore_monsters, g_hostEntity, &tr);

         float height = tr.flFraction * 1000.0f; // height from ground

         // is the current height greater than the step height?
         if (height < lastHeight - 18.0f)
            return false; // can't get there without jumping...

         lastHeight = height;
         distance = (destination - check).GetLength (); // distance from goal
      }
      return true;
   }
   return false;
}

void Waypoint::InitializeVisibility (void)
{
   if (m_redoneVisibility == false)
      return;

   TraceResult tr;
   byte res, shift;

   for (m_visibilityIndex = 0; m_visibilityIndex < g_numWaypoints; m_visibilityIndex++)
   {
      Vector sourceDuck = m_paths[m_visibilityIndex]->origin;
      Vector sourceStand = m_paths[m_visibilityIndex]->origin;

      if (m_paths[m_visibilityIndex]->flags & FLAG_CROUCH)
      {
         sourceDuck.z += 12.0f;
         sourceStand.z += 18.0f + 28.0f;
      }
      else
      {
         sourceDuck.z += -18.0f + 12.0f;
         sourceStand.z += 28.0f;
      }
      uint16 standCount = 0, crouchCount = 0;

      for (int i = 0; i < g_numWaypoints; i++)
      {
         // first check ducked visibility
         Vector dest = m_paths[i]->origin;

         TraceLine (sourceDuck, dest, true, NULL, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (tr.flFraction <= TRACE_FRACTION_EQ || tr.fStartSolid)
            res = 1;
         else
            res = 0;

         res <<= 1;

         TraceLine (sourceStand, dest, true, NULL, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (tr.flFraction <= TRACE_FRACTION_EQ || tr.fStartSolid)
            res |= 1;

         shift = (i % 4) << 1;
         m_visLUT[m_visibilityIndex][i >> 2] &= ~(3 << shift);
         m_visLUT[m_visibilityIndex][i >> 2] |= res << shift;

         if (!(res & 2))
            crouchCount++;

         if (!(res & 1))
            standCount++;
      }
      m_paths[m_visibilityIndex]->vis.crouch = crouchCount;
      m_paths[m_visibilityIndex]->vis.stand = standCount;
   }
   m_redoneVisibility = false;
}

bool Waypoint::IsVisible (int srcIndex, int destIndex)
{
   unsigned char res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 3) == 3);
}

bool Waypoint::IsDuckVisible (int srcIndex, int destIndex)
{
   unsigned char res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 2) == 2);
}

bool Waypoint::IsStandVisible (int srcIndex, int destIndex)
{
   unsigned char res = m_visLUT[srcIndex][destIndex >> 2];
   res >>= (destIndex % 4) << 1;

   return !((res & 1) == 1);
}

const char *Waypoint::GetWaypointInfo(int id)
{
   // this function returns path information for waypoint pointed by id.

   Path *path = m_paths[id];

   // if this path is null, return
   if (path == NULL)
      return "\0";

   bool jumpPoint = false;

   // iterate through connections and find, if it's a jump path
   for (int i = 0; i < MAX_PATH_INDEX; i++)
   {
      // check if we got a valid connection
      if (path->index[i] != -1 && (path->connectionFlags[i] & PATHFLAG_JUMP))
         jumpPoint = true;
   }

   static char messageBuffer[1024];
   sprintf (messageBuffer, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s", (path->flags == 0 && !jumpPoint) ? " (none)" : "", (path->flags & FLAG_LIFT) ? " LIFT" : "", (path->flags & FLAG_CROUCH) ? " CROUCH" : "", (path->flags & FLAG_CROSSING) ? " CROSSING" : "", (path->flags & FLAG_CAMP) ? " CAMP" : "", (path->flags & FLAG_TF_ONLY) ? " TERRORIST" : "", (path->flags & FLAG_CF_ONLY) ? " CT" : "", (path->flags & FLAG_SNIPER) ? " SNIPER" : "", (path->flags & FLAG_GOAL) ? " GOAL" : "", (path->flags & FLAG_LADDER) ? " LADDER" : "", (path->flags & FLAG_RESCUE) ? " RESCUE" : "", (path->flags & FLAG_DOUBLEJUMP) ? " JUMPHELP" : "", (path->flags & FLAG_NOHOSTAGE) ? " NOHOSTAGE" : "", jumpPoint ? " JUMP" : "");

   // return the message buffer
   return messageBuffer;
}

void Waypoint::Think (void)
{
   // this function executes frame of waypoint operation code.

   if (IsEntityNull (g_hostEntity))
      return; // this function is only valid on listenserver, and in waypoint enabled mode.

   float nearestDistance = 99999.0f;
   int nearestIndex = -1;

   // check if it's time to add jump waypoint
   if (m_learnJumpWaypoint)
   {
      if (!m_endJumpPoint)
      {
         if (g_hostEntity->v.button & IN_JUMP)
         {
            Add (9);

            m_timeJumpStarted = GetWorldTime ();
            m_endJumpPoint = true;
         }
         else
         {
            m_learnVelocity = g_hostEntity->v.velocity;
            m_learnPosition = g_hostEntity->v.origin;
         }
      }
      else if (((g_hostEntity->v.flags & FL_ONGROUND) || g_hostEntity->v.movetype == MOVETYPE_FLY) && m_timeJumpStarted + 0.1 < GetWorldTime () && m_endJumpPoint)
      {
         Add (10);

         m_learnJumpWaypoint = false;
         m_endJumpPoint = false;
      }
   }

   // check if it's a autowaypoint mode enabled
   if (g_autoWaypoint && (g_hostEntity->v.flags & (FL_ONGROUND | FL_PARTIALGROUND)))
   {
      // find the distance from the last used waypoint
      float distance = (m_lastWaypoint - g_hostEntity->v.origin).GetLengthSquared ();

      if (distance > 16384.0f)
      {
         // check that no other reachable waypoints are nearby...
         for (int i = 0; i < g_numWaypoints; i++)
         {
            if (IsNodeReachable (g_hostEntity->v.origin, m_paths[i]->origin))
            {
               distance = (m_paths[i]->origin - g_hostEntity->v.origin).GetLengthSquared ();

               if (distance < nearestDistance)
                  nearestDistance = distance;
            }
         }

         // make sure nearest waypoint is far enough away...
         if (nearestDistance >= 16384.0f)
            Add (0);  // place a waypoint here
      }
   }
   m_facingAtIndex = GetFacingIndex ();

   // reset the minimal distance changed before
   nearestDistance = 99999.0f;

   // now iterate through all waypoints in a map, and draw required ones
   for (int i = 0; i < g_numWaypoints; i++)
   {
      float distance = (m_paths[i]->origin - g_hostEntity->v.origin).GetLengthSquared ();

      // check if waypoint is whitin a distance, and is visible
      if (distance < GET_SQUARE (500.0f) && ((::IsVisible (m_paths[i]->origin, g_hostEntity) && IsInViewCone (m_paths[i]->origin, g_hostEntity)) || !IsAlive (g_hostEntity) || distance < 2500.0f))
      {
         // check the distance
         if (distance < nearestDistance)
         {
            nearestIndex = i;
            nearestDistance = distance;
         }

         if (m_waypointDisplayTime[i] + 1.0f < GetWorldTime ())
         {
            float nodeHeight = 0.0f;

            // check the node height
            if (m_paths[i]->flags & FLAG_CROUCH)
               nodeHeight = 36.0f;
            else
               nodeHeight = 72.0f;

            float nodeHalfHeight = nodeHeight * 0.5f;

            // all waypoints are by default are green
            Vector nodeColor;

            // colorize all other waypoints
            if (m_paths[i]->flags & FLAG_CAMP)
               nodeColor = Vector (0, 255, 255);
            else if (m_paths[i]->flags & FLAG_GOAL)
               nodeColor = Vector (128, 0, 255);
            else if (m_paths[i]->flags & FLAG_LADDER)
               nodeColor = Vector (128, 64, 0);
            else if (m_paths[i]->flags & FLAG_RESCUE)
               nodeColor = Vector (255, 255, 255);
            else
               nodeColor = Vector (0, 255, 0);

            // colorize additional flags
            Vector nodeFlagColor = Vector (-1, -1, -1);

            // check the colors
            if (m_paths[i]->flags & FLAG_SNIPER)
               nodeFlagColor = Vector (130, 87, 0);
            else if (m_paths[i]->flags & FLAG_NOHOSTAGE)
               nodeFlagColor = Vector (255, 255, 255);
            else if (m_paths[i]->flags & FLAG_TF_ONLY)
               nodeFlagColor = Vector (255, 0, 0);
            else if (m_paths[i]->flags & FLAG_CF_ONLY)
               nodeFlagColor = Vector (0, 0, 255);

            // draw node without additional flags
            if (nodeFlagColor.x == -1)
               DrawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight), m_paths[i]->origin + Vector (0, 0, nodeHalfHeight), 15, 0, static_cast <int> (nodeColor.x), static_cast <int> (nodeColor.y), static_cast <int> (nodeColor.z), 250, 0, 10);
            else // draw node with flags
            {
               DrawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight), m_paths[i]->origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75), 14, 0, static_cast <int> (nodeColor.x), static_cast <int> (nodeColor.y), static_cast <int> (nodeColor.z), 250, 0, 10); // draw basic path
               DrawLine (g_hostEntity, m_paths[i]->origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75), m_paths[i]->origin + Vector (0, 0, nodeHalfHeight), 14, 0, static_cast <int> (nodeFlagColor.x), static_cast <int> (nodeFlagColor.y), static_cast <int> (nodeFlagColor.z), 250, 0, 10); // draw additional path
            }
            m_waypointDisplayTime[i] = GetWorldTime ();
         }
      }
   }

   if (nearestIndex == -1)
      return;

   // draw arrow to a some importaint waypoints
   if ((m_findWPIndex != -1 && m_findWPIndex < g_numWaypoints) || (m_cacheWaypointIndex != -1 && m_cacheWaypointIndex < g_numWaypoints) || (m_facingAtIndex != -1 && m_facingAtIndex < g_numWaypoints))
   {
      // check for drawing code
      if (m_arrowDisplayTime + 0.5f < GetWorldTime ())
      {
         // finding waypoint - pink arrow
         if (m_findWPIndex != -1)
            DrawArrow (g_hostEntity, g_hostEntity->v.origin, m_paths[m_findWPIndex]->origin, 10, 0, 128, 0, 128, 200, 0, 5);

         // cached waypoint - yellow arrow
         if (m_cacheWaypointIndex != -1)
            DrawArrow (g_hostEntity, g_hostEntity->v.origin, m_paths[m_cacheWaypointIndex]->origin, 10, 0, 255, 255, 0, 200, 0, 5);

         // waypoint user facing at - white arrow
         if (m_facingAtIndex != -1)
            DrawArrow (g_hostEntity, g_hostEntity->v.origin, m_paths[m_facingAtIndex]->origin, 10, 0, 255, 255, 255, 200, 0, 5);

         m_arrowDisplayTime = GetWorldTime ();
      }
   }

   // create path pointer for faster access
   Path *path = m_paths[nearestIndex];

   // draw a paths, camplines and danger directions for nearest waypoint
   if (nearestDistance < 4096.0f && m_pathDisplayTime <= GetWorldTime ())
   {
      m_pathDisplayTime = GetWorldTime () + 1.0f;

      // draw the camplines
      if (path->flags & FLAG_CAMP)
      {
         Vector campSourceOrigin = path->origin + Vector (0.0f, 0.0f, 36.0f);

         // check if it's a source
         if (path->flags & FLAG_CROUCH)
            campSourceOrigin = path->origin + Vector (0.0f, 0.0f, 18.0f);

         Vector campStartOrigin = Vector (path->campStartX, path->campStartY, campSourceOrigin.z); // camp start
         Vector campEndOrigin =  Vector (path->campEndX, path->campEndY, campSourceOrigin.z); // camp end

         // draw it now
         DrawLine (g_hostEntity, campSourceOrigin, campStartOrigin, 10, 0, 255, 0, 0, 200, 0, 10);
         DrawLine (g_hostEntity, campSourceOrigin, campEndOrigin, 10, 0, 255, 0, 0, 200, 0, 10);
      }

      // draw the connections
      for (int i = 0; i < MAX_PATH_INDEX; i++)
      {
         if (path->index[i] == -1)
            continue;

         // jump connection
         if (path->connectionFlags[i] & PATHFLAG_JUMP)
            DrawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 255, 0, 128, 200, 0, 10);
         else if (IsConnected (path->index[i], nearestIndex)) // twoway connection
            DrawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 255, 255, 0, 200, 0, 10);
         else // oneway connection
            DrawLine (g_hostEntity, path->origin, m_paths[path->index[i]]->origin, 5, 0, 250, 250, 250, 200, 0, 10);
      }

      // now look for oneway incoming connections
      for (int i = 0; i < g_numWaypoints; i++)
      {
         if (IsConnected (m_paths[i]->pathNumber, path->pathNumber) && !IsConnected (path->pathNumber, m_paths[i]->pathNumber))
            DrawLine (g_hostEntity, path->origin, m_paths[i]->origin, 5, 0, 0, 192, 96, 200, 0, 10);
      }

      // draw the radius circle
      Vector origin = (path->flags & FLAG_CROUCH) ? path->origin : path->origin - Vector (0.0f, 0.0f, 18.0f);

      // if radius is nonzero, draw a full circle
      if (path->radius > 0.0f)
      {
         float squareRoot = sqrtf (path->radius * path->radius * 0.5f);

         DrawLine (g_hostEntity, origin + Vector (path->radius, 0.0f, 0.0f), origin + Vector (squareRoot, -squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         DrawLine (g_hostEntity, origin + Vector (squareRoot, -squareRoot, 0.0f), origin + Vector (0.0f, -path->radius, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         DrawLine (g_hostEntity, origin + Vector (0.0f, -path->radius, 0.0f), origin + Vector (-squareRoot, -squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         DrawLine (g_hostEntity, origin + Vector (-squareRoot, -squareRoot, 0.0f), origin + Vector (-path->radius, 0.0f, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         DrawLine (g_hostEntity, origin + Vector (-path->radius, 0.0f, 0.0f), origin + Vector (-squareRoot, squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         DrawLine (g_hostEntity, origin + Vector (-squareRoot, squareRoot, 0.0f), origin + Vector (0.0f, path->radius, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);

         DrawLine (g_hostEntity, origin + Vector (0.0f, path->radius, 0.0f), origin + Vector (squareRoot, squareRoot, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
         DrawLine (g_hostEntity, origin + Vector (squareRoot, squareRoot, 0.0f), origin + Vector (path->radius, 0.0f, 0.0f), 5, 0, 0, 0, 255, 200, 0, 10);
      }
      else
      {
         float squareRoot = sqrtf (32.0f);

         DrawLine (g_hostEntity, origin + Vector (squareRoot, -squareRoot, 0.0f), origin + Vector (-squareRoot, squareRoot, 0.0f), 5, 0, 255, 0, 0, 200, 0, 10);
         DrawLine (g_hostEntity, origin + Vector (-squareRoot, -squareRoot, 0.0f), origin + Vector (squareRoot, squareRoot, 0.0f), 5, 0, 255, 0, 0, 200, 0, 10);
      }

      // draw the danger directions
      if (!g_waypointsChanged)
      {
         if ((g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team0DangerIndex != -1 && GetTeam (g_hostEntity) == TEAM_TF)
            DrawArrow (g_hostEntity, path->origin, m_paths[(g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team0DangerIndex]->origin, 15, 0, 255, 0, 0, 200, 0, 10); // draw a red arrow to this index's danger point

         if ((g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team1DangerIndex != -1 && GetTeam (g_hostEntity) == TEAM_CF)
            DrawArrow (g_hostEntity, path->origin, m_paths[(g_experienceData + (nearestIndex * g_numWaypoints) + nearestIndex)->team1DangerIndex]->origin, 15, 0, 0, 0, 255, 200, 0, 10); // draw a blue arrow to this index's danger point
      }

      // display some information
      char tempMessage[4096];

      // show the information about that point
      int length = sprintf (tempMessage, "\n\n\n\n    Waypoint Information:\n\n"
         "      Waypoint %d of %d, Radius: %.1f\n"
         "      Flags: %s\n\n", nearestIndex, g_numWaypoints, path->radius, GetWaypointInfo (nearestIndex));


      // if waypoint is not changed display experience also
      if (!g_waypointsChanged)
      {
         int dangerIndexCT = (g_experienceData + nearestIndex * g_numWaypoints + nearestIndex)->team1DangerIndex;
         int dangerIndexT = (g_experienceData + nearestIndex * g_numWaypoints + nearestIndex)->team0DangerIndex;

         length += sprintf (&tempMessage[length],
            "      Experience Info:\n"
            "      CT: %d / %d\n"
            "      T: %d / %d\n", dangerIndexCT, dangerIndexCT != -1 ? (g_experienceData + nearestIndex * g_numWaypoints + dangerIndexCT)->team1Damage : 0, dangerIndexT, dangerIndexT != -1 ? (g_experienceData + nearestIndex * g_numWaypoints + dangerIndexT)->team0Damage : 0);
      }

      // check if we need to show the cached point index
      if (m_cacheWaypointIndex != -1)
      {
         length += sprintf (&tempMessage[length], "\n    Cached Waypoint Information:\n\n"
            "      Waypoint %d of %d, Radius: %.1f\n"
            "      Flags: %s\n", m_cacheWaypointIndex, g_numWaypoints, m_paths[m_cacheWaypointIndex]->radius, GetWaypointInfo (m_cacheWaypointIndex));
      }

      // check if we need to show the facing point index
      if (m_facingAtIndex != -1)
      {
         length += sprintf (&tempMessage[length], "\n    Facing Waypoint Information:\n\n"
            "      Waypoint %d of %d, Radius: %.1f\n"
            "      Flags: %s\n", m_facingAtIndex, g_numWaypoints, m_paths[m_facingAtIndex]->radius, GetWaypointInfo (m_facingAtIndex));
      }

      // draw entire message
      MESSAGE_BEGIN (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, NULL, g_hostEntity);
         WRITE_BYTE (TE_TEXTMESSAGE);
         WRITE_BYTE (4); // channel
         WRITE_SHORT (FixedSigned16 (0, 1 << 13)); // x
         WRITE_SHORT (FixedSigned16 (0, 1 << 13)); // y
         WRITE_BYTE (0); // effect
         WRITE_BYTE (255); // r1
         WRITE_BYTE (255); // g1
         WRITE_BYTE (255); // b1
         WRITE_BYTE (1); // a1
         WRITE_BYTE (255); // r2
         WRITE_BYTE (255); // g2
         WRITE_BYTE (255); // b2
         WRITE_BYTE (255); // a2
         WRITE_SHORT (0); // fadeintime
         WRITE_SHORT (0); // fadeouttime
         WRITE_SHORT (FixedUnsigned16 (1.1, 1 << 8)); // holdtime
         WRITE_STRING (tempMessage);
      MESSAGE_END ();
   }
}

bool Waypoint::IsConnected (int index)
{
   for (int i = 0; i < g_numWaypoints; i++)
   {
      if (i != index)
      {
         for (int j = 0; j < MAX_PATH_INDEX; j++)
         {
            if (m_paths[i]->index[j] == index)
               return true;
         }
      }
   }
   return false;
}

bool Waypoint::NodesValid (void)
{
   int terrPoints = 0;
   int ctPoints = 0;
   int goalPoints = 0;
   int rescuePoints = 0;
   int i, j;

   for (i = 0; i < g_numWaypoints; i++)
   {
      int connections = 0;

      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (m_paths[i]->index[j] != -1)
         {
            if (m_paths[i]->index[j] > g_numWaypoints)
            {
               AddLogEntry (true, LL_WARNING, "Waypoint %d connected with invalid Waypoint #%d!", i, m_paths[i]->index[j]);
               return false;
            }
            connections++;
            break;
         }
      }

      if (connections == 0)
      {
         if (!IsConnected (i))
         {
            AddLogEntry (true, LL_WARNING, "Waypoint %d isn't connected with any other Waypoint!", i);
            return false;
         }
      }

      if (m_paths[i]->pathNumber != i)
      {
         AddLogEntry (true, LL_WARNING, "Waypoint %d pathnumber differs from index!", i);
         return false;
      }

      if (m_paths[i]->flags & FLAG_CAMP)
      {
         if (m_paths[i]->campEndX == 0 && m_paths[i]->campEndY == 0)
         {
            AddLogEntry (true, LL_WARNING, "Waypoint %d Camp-Endposition not set!", i);
            return false;
         }
      }
      else if (m_paths[i]->flags & FLAG_TF_ONLY)
         terrPoints++;
      else if (m_paths[i]->flags & FLAG_CF_ONLY)
         ctPoints++;
      else if (m_paths[i]->flags & FLAG_GOAL)
         goalPoints++;
      else if (m_paths[i]->flags & FLAG_RESCUE)
         rescuePoints++;

      for (int k = 0; k < MAX_PATH_INDEX; k++)
      {
         if (m_paths[i]->index[k] != -1)
         {
            if (m_paths[i]->index[k] >= g_numWaypoints || m_paths[i]->index[k] < -1)
            {
               AddLogEntry (true, LL_WARNING, "Waypoint %d - Pathindex %d out of Range!", i, k);
               (*g_engfuncs.pfnSetOrigin) (g_hostEntity, m_paths[i]->origin);

               g_waypointOn = true;
               g_editNoclip = true;

               return false;
            }
            else if (m_paths[i]->index[k] == i)
            {
               AddLogEntry (true, LL_WARNING, "Waypoint %d - Pathindex %d points to itself!", i, k);

               if (g_waypointOn && !IsDedicatedServer ())
               {
                  (*g_engfuncs.pfnSetOrigin) (g_hostEntity, m_paths[i]->origin);

                  g_waypointOn = true;
                  g_editNoclip = true;
               }
               return false;
            }
         }
      }
   }

   if (g_mapType & MAP_CS)
   {
      if (rescuePoints == 0)
      {
         AddLogEntry (true, LL_WARNING, "You didn't set a Rescue Point!");
         return false;
      }
   }
   if (terrPoints == 0)
   {
      AddLogEntry (true, LL_WARNING, "You didn't set any Terrorist Important Point!");
      return false;
   }
   else if (ctPoints == 0)
   {
      AddLogEntry (true, LL_WARNING, "You didn't set any CT Important Point!");
      return false;
   }
   else if (goalPoints == 0)
   {
      AddLogEntry (true, LL_WARNING, "You didn't set any Goal Point!");
      return false;
   }

   // perform DFS instead of floyd-warshall, this shit speedup this process in a bit
   PathNode *stack = NULL;
   bool visited[MAX_WAYPOINTS];

   // first check incoming connectivity, initialize the "visited" table
   for (i = 0; i < g_numWaypoints; i++)
      visited[i] = false;

   // check from waypoint nr. 0
   stack = new PathNode;
   stack->next = NULL;
   stack->index = 0;

   while (stack != NULL)
   {
      // pop a node from the stack
      PathNode *current = stack;
      stack = stack->next;

      visited[current->index] = true;

      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         int index = m_paths[current->index]->index[j];

         if (visited[index])
            continue; // skip this waypoint as it's already visited

         if (index >= 0 && index < g_numWaypoints)
         {
            PathNode *pNewNode = new PathNode;

            pNewNode->next = stack;
            pNewNode->index = index;
            stack = pNewNode;
         }
      }
      delete current;
   }

   for (i = 0; i < g_numWaypoints; i++)
   {
      if (!visited[i])
      {
         AddLogEntry (true, LL_WARNING, "Path broken from Waypoint #0 to Waypoint #%d!", i);

         if (g_waypointOn && !IsDedicatedServer ())
         {
            (*g_engfuncs.pfnSetOrigin) (g_hostEntity, m_paths[i]->origin);

            g_waypointOn = true;
            g_editNoclip = true;
         }
         return false;
      }
   }

   // then check outgoing connectivity
   Array <int> outgoingPaths[MAX_WAYPOINTS]; // store incoming paths for speedup

   for (i = 0; i < g_numWaypoints; i++)
     {
      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (m_paths[i]->index[j] >= 0 && m_paths[i]->index[j] < g_numWaypoints)
            outgoingPaths[m_paths[i]->index[j]].Push (i);
      }
   }

   // initialize the "visited" table
   for (i = 0; i < g_numWaypoints; i++)
      visited[i] = false;

   // check from Waypoint nr. 0
   stack = new PathNode;
   stack->next = NULL;
   stack->index = 0;

   while (stack != NULL)
   {
      // pop a node from the stack
      PathNode *current = stack;
      stack = stack->next;

      visited[current->index] = true;

      FOR_EACH_AE (outgoingPaths[current->index], p)
      {
         if (visited[outgoingPaths[current->index][p]])
            continue; // skip this waypoint as it's already visited

         PathNode *newNode = new PathNode;
     
         newNode->next = stack;
         newNode->index = outgoingPaths[current->index][p];

         stack = newNode;
      }
      delete current;
   }

   for (i = 0; i < g_numWaypoints; i++)
   {
      if (!visited[i])
      {
         AddLogEntry (true, LL_WARNING, "Path broken from Waypoint #%d to Waypoint #0!", i);

         if (g_waypointOn && !IsDedicatedServer ())
         {
            (*g_engfuncs.pfnSetOrigin) (g_hostEntity, m_paths[i]->origin);

            g_waypointOn = true;
            g_editNoclip = true;
         }
         return false;
      }
   }
   return true;
}

void Waypoint::InitPathMatrix (void)
{
   int i, j, k;

   delete [] m_distMatrix;
   delete [] m_pathMatrix;

   m_distMatrix = NULL;
   m_pathMatrix = NULL;

   m_distMatrix = new int [g_numWaypoints * g_numWaypoints];
   m_pathMatrix = new int [g_numWaypoints * g_numWaypoints];

   if (LoadPathMatrix ())
      return; // matrix loaded from file

   for (i = 0; i < g_numWaypoints; i++)
   {
      for (j = 0; j < g_numWaypoints; j++)
      {
         *(m_distMatrix + i * g_numWaypoints + j) = 999999;
         *(m_pathMatrix + i * g_numWaypoints + j) = -1;
      }
   }

   for (i = 0; i < g_numWaypoints; i++)
   {
      for (j = 0; j < MAX_PATH_INDEX; j++)
      {
         if (m_paths[i]->index[j] >= 0 && m_paths[i]->index[j] < g_numWaypoints)
         {
            *(m_distMatrix + (i * g_numWaypoints) + m_paths[i]->index[j]) = m_paths[i]->distances[j];
            *(m_pathMatrix + (i * g_numWaypoints) + m_paths[i]->index[j]) = m_paths[i]->index[j];
         }
      }
   }

   for (i = 0; i < g_numWaypoints; i++)
      *(m_distMatrix + (i * g_numWaypoints) + i) = 0;

   for (k = 0; k < g_numWaypoints; k++)
   {
      for (i = 0; i < g_numWaypoints; i++)
      {
         for (j = 0; j < g_numWaypoints; j++)
         {
            if (*(m_distMatrix + (i * g_numWaypoints) + k) + *(m_distMatrix + (k * g_numWaypoints) + j) < (*(m_distMatrix + (i * g_numWaypoints) + j)))
            {
               *(m_distMatrix + (i * g_numWaypoints) + j) = *(m_distMatrix + (i * g_numWaypoints) + k) + *(m_distMatrix + (k * g_numWaypoints) + j);
               *(m_pathMatrix + (i * g_numWaypoints) + j) = *(m_pathMatrix + (i * g_numWaypoints) + k);
            }
         }
      }
   }

   // save path matrix to file for faster access
   SavePathMatrix ();
}

void Waypoint::SavePathMatrix (void)
{
   File fp (FormatBuffer ("%slearned/%s.pmt", GetWaypointDir (), GetMapName ()), "wb");

   // unable to open file
   if (!fp.IsValid ())
   {
      AddLogEntry (false, LL_FATAL, "Failed to open file for writing");
      return;
   }

   // write number of waypoints
   fp.Write (&g_numWaypoints, sizeof (int));

   // write path & distance matrix
   fp.Write (m_pathMatrix, sizeof (int), g_numWaypoints * g_numWaypoints);
   fp.Write (m_distMatrix, sizeof (int), g_numWaypoints * g_numWaypoints);

   // and close the file
   fp.Close ();
}

bool Waypoint::LoadPathMatrix (void)
{
   File fp (FormatBuffer ("%slearned/%s.pmt", GetWaypointDir (), GetMapName ()), "rb");

   // file doesn't exists return false
   if (!fp.IsValid ())
      return false;

   int num = 0;

   // read number of waypoints
   if (fp.Read (&num, sizeof (int)) == 0)
   {
      fp.Close ();
      return false;
   }

   if (num != g_numWaypoints)
   {
      AddLogEntry (true, LL_WARNING, "Pathmatrix damaged (wrong version, or not for this map). Pathmatrix will be rebuilt.");
      fp.Close ();

      return false;
   }

   // read path & distance matrixes
   if (fp.Read (m_pathMatrix, sizeof (int), g_numWaypoints * g_numWaypoints) == 0)
   {
      fp.Close ();
      return false;
   }

   if (fp.Read (m_distMatrix, sizeof (int), g_numWaypoints * g_numWaypoints) == 0)
   {
      fp.Close ();
      return false;
   }
   fp.Close (); // and close the file

   return true;
}

int Waypoint::GetPathDistance (int srcIndex, int destIndex)
{
   if (srcIndex < 0 || srcIndex >= g_numWaypoints || destIndex < 0 || destIndex >= g_numWaypoints)
      return 1;

   return *(m_distMatrix + (srcIndex * g_numWaypoints) + destIndex);
}

void Waypoint::SetGoalVisited (int index)
{
   if (index < 0 || index >= g_numWaypoints)
      return;

   if (!IsGoalVisited (index) || !(m_paths[index]->flags & FLAG_GOAL))
   {
#if 0
      int bombPoint = FindNearest (GetBombPosition ());

      Array <int> markAsVisited;
      FindInRadius (markAsVisited, 356.0f, GetPath (index)->origin);

      IterateArray (markAsVisited, i)
      {
         // skip still valid bomb point
         if (markAsVisited[i] == bombPoint)
            continue;

         m_visitedGoals.Push (markAsVisited[i]);
      }
#endif
      m_visitedGoals.Push (index);
   }
}

bool Waypoint::IsGoalVisited (int index)
{
   FOR_EACH_AE (m_visitedGoals, i)
   {
      if (m_visitedGoals[i] == index)
         return true;
   }
   return false;
}

void Waypoint::CreateBasic (void)
{
   // this function creates basic waypoint types on map

   edict_t *ent = NULL;

   // first of all, if map contains ladder points, create it
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "func_ladder")))
   {
      Vector ladderLeft = ent->v.absmin;
      Vector ladderRight = ent->v.absmax;
      ladderLeft.z = ladderRight.z;

      TraceResult tr;
      Vector up, down, front, back;

      Vector diff = ((ladderLeft - ladderRight) ^ Vector (0.0f, 0.0f, 0.0f)).Normalize () * 15.0f;
      front = back = GetEntityOrigin (ent);

      front = front + diff; // front
      back = back - diff; // back

      up = down = front;
      down.z = ent->v.absmax.z;

      TraceHull (down, up, true, point_hull, NULL, &tr);

      if (POINT_CONTENTS (up) == CONTENTS_SOLID || tr.flFraction <= TRACE_FRACTION_EQ)
      {
         up = down = back;
         down.z = ent->v.absmax.z;
      }

      TraceHull (down, up - Vector (0.0f, 0.0f, 1000.0f), true, point_hull, NULL, &tr);
      up = tr.vecEndPos;

      Vector pointOrigin = up + Vector (0.0f, 0.0f, 39.0f);
      m_isOnLadder = true;

      do
      {
         if (FindNearest (pointOrigin, 50.0f) == -1)
            Add (3, pointOrigin);

         pointOrigin.z += 160;
      } while (pointOrigin.z < down.z - 40.0f);

      pointOrigin = down + Vector (0.0f, 0.0f, 38.0f);

      if (FindNearest (pointOrigin, 50.0f) == -1)
         Add (3, pointOrigin);

      m_isOnLadder = false;
   }

   // then terrortist spawnpoints
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "info_player_deathmatch")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (0, origin);
   }

   // then add ct spawnpoints
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "info_player_start")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (0, origin);
   }

   // then vip spawnpoint
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "info_vip_start")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (0, origin);
   }

   // hostage rescue zone
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "func_hostage_rescue")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (4, origin);
   }

   // hostage rescue zone (same as above)
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "info_hostage_rescue")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (4, origin);
   }

   // bombspot zone
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "func_bomb_target")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (100, origin);
   }

   // bombspot zone (same as above)
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "info_bomb_target")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (100, origin);
   }

   // hostage entities
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "hostage_entity")))
   {
      // if already saved || moving skip it
      if ((ent->v.effects & EF_NODRAW) && ent->v.speed > 0.0f)
         continue;

      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50) == -1)
         Add (100, origin);
   }

   // vip rescue (safety) zone
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "func_vip_safetyzone")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (100, origin);
   }

   // terrorist escape zone
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "func_escapezone")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (100, origin);
   }

   // weapons on the map ?
   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "armoury_entity")))
   {
      Vector origin = GetEntityOrigin (ent);

      if (FindNearest (origin, 50.0f) == -1)
         Add (0, origin);
   }
}

Path *Waypoint::GetPath (int id)
{
   Path *path = m_paths[id];

   if (path == NULL)
      return NULL;

   return path;
}

void Waypoint::EraseFromHardDisk (void)
{
   // this function removes waypoint file from the hard disk

   String deleteList[5];

   // if we're delete waypoint, delete all corresponding to it files
   deleteList[0] = FormatBuffer ("%s%s.pwf", GetWaypointDir (), GetMapName ()); // waypoint itself
   deleteList[1] = FormatBuffer ("%slearned/%s.exp", GetWaypointDir (), GetMapName ()); // corresponding to waypoint experience
   deleteList[3] = FormatBuffer ("%slearned/%s.vis", GetWaypointDir (), GetMapName ()); // corresponding to waypoint vistable
   deleteList[3] = FormatBuffer ("%slearned/%s.pmt", GetWaypointDir (), GetMapName ()); // corresponding to waypoint path matrix
   deleteList[4] = FormatBuffer ("%slearned/%s.xml", GetWaypointDir (), GetMapName ()); // corresponding to waypoint xml database

   for (int i = 0; i < 4; i++)
   {
      if (File::Accessible (const_cast <char *> (deleteList[i].GetBuffer ())))
      {
         unlink (deleteList[i].GetBuffer ());
         AddLogEntry (true, LL_DEFAULT, "File %s, has been deleted from the hard disk", deleteList[i].GetBuffer ());
      }
      else
         AddLogEntry (true, LL_ERROR, "Unable to open %s", deleteList[i].GetBuffer ());
   }
   Init (); // reintialize points
}

void Waypoint::SetBombPosition (bool shouldReset)
{
   // this function stores the bomb position as a vector

   if (shouldReset)
   {
      m_foundBombOrigin.Zero ();
      g_bombPlanted = false;

      return;
   }

   edict_t *ent = NULL;

   while (!IsEntityNull (ent = FIND_ENTITY_BY_CLASSNAME (ent, "grenade")))
   {
      if (strcmp (STRING (ent->v.model) + 9, "c4.mdl") == 0)
      {
         m_foundBombOrigin = GetEntityOrigin (ent);
         break;
      }
   }
}

void Waypoint::SetLearnJumpWaypoint (void)
{
   m_learnJumpWaypoint = true;
}

void Waypoint::SetFindIndex (int index)
{
   m_findWPIndex = index;

   if (m_findWPIndex < g_numWaypoints)
      ServerPrint ("Showing Direction to Waypoint #%d", m_findWPIndex);
   else
      m_findWPIndex = -1;
}

int Waypoint::AddGoalScore (int index, int other[4])
{
   Array <int> left;

   if (m_goalsScore[index] < 1024.0f)
      left.Push (index);

   for (int i = 0; i < 3; i++)
   {
      if (m_goalsScore[other[i]] < 1024.0f)
         left.Push (other[i]);
   }

   if (left.IsEmpty ())
      index = other[Random.Long (0, 3)];
   else
      index = left.GetRandomElement ();

   if (m_paths[index]->flags & FLAG_GOAL)
      m_goalsScore[index] += 384.0f;
   else if (m_paths[index]->flags & (FLAG_CF_ONLY | FLAG_TF_ONLY))
      m_goalsScore[index] += 768.0f;
   else if (m_paths[index]->flags & FLAG_CAMP)
      m_goalsScore[index] += 1024.0f;

   return index;
}

void Waypoint::ClearGoalScore (void)
{
   // iterate though all waypoints
   for (int i = 0; i < MAX_WAYPOINTS; i++)
      m_goalsScore[i] = 0.0f;
}

Waypoint::Waypoint (void)
{
   m_waypointPaths = false;
   m_endJumpPoint = false;
   m_redoneVisibility = false;
   m_learnJumpWaypoint = false;
   m_timeJumpStarted = 0.0f;

   m_lastJumpWaypoint = -1;
   m_cacheWaypointIndex = -1;
   m_findWPIndex = -1;
   m_facingAtIndex = -1;
   m_visibilityIndex = 0;

   m_isOnLadder = false;

   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;

   m_terrorPoints.RemoveAll ();
   m_ctPoints.RemoveAll ();
   m_goalPoints.RemoveAll ();
   m_campPoints.RemoveAll ();
   m_rescuePoints.RemoveAll ();
   m_sniperPoints.RemoveAll ();

   m_distMatrix = NULL;
   m_pathMatrix = NULL;

   for (int i = 0; i < MAX_WAYPOINTS; i++)
      m_paths[i] = NULL;
}

Waypoint::~Waypoint (void)
{
   delete [] m_distMatrix;
   delete [] m_pathMatrix;

   m_distMatrix = NULL;
   m_pathMatrix = NULL;
}

void WaypointDownloader::FreeSocket (int sock)
{
#if defined (PLATFORM_WIN32)
   if (sock != -1)
      closesocket (sock);

   WSACleanup ();
#else
   if (sock != -1)
      close (sock);
#endif
}


WaypointDownloadError WaypointDownloader::DoDownload (void)
{
#if defined (PLATFORM_WIN32)
   WORD requestedVersion = MAKEWORD (1, 1);
   WSADATA wsaData;

   int wsa = WSAStartup (requestedVersion, &wsaData);

   if (wsa != 0)
      return WDE_SOCKET_ERROR;
#endif

   hostent *host = gethostbyname (yb_waypoint_autodl_host.GetString ());

   if (host == NULL)
      return WDE_SOCKET_ERROR;

   int socketHandle = socket (AF_INET, SOCK_STREAM, 0);

   if (socketHandle < 0)
   {
      FreeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   sockaddr_in dest;

   timeval timeout;
   timeout.tv_sec = 5;
   timeout.tv_usec = 0;

   int result = setsockopt (socketHandle, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof (timeout));

   if (result < 0)
   {
      FreeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   result = setsockopt (socketHandle, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof (timeout));

   if (result < 0)
   {
      FreeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }
   memset (&dest, 0, sizeof (dest));

   dest.sin_family = AF_INET;
   dest.sin_port = htons (80);
   dest.sin_addr.s_addr = inet_addr (inet_ntoa (*((struct in_addr *) host->h_addr)));

   if (connect (socketHandle, (struct sockaddr*) &dest, (int) sizeof (dest)) == -1)
   {
      FreeSocket (socketHandle);
      return WDE_CONNECT_ERROR;
   }

   String request;
   request.AssignFormat ("GET /wpdb/%s.pwf HTTP/1.0\r\nAccept: */*\r\nUser-Agent: YaPB/%s\r\nHost: %s\r\n\r\n", GetMapName (), PRODUCT_VERSION, yb_waypoint_autodl_host.GetString ());

   if (send (socketHandle, request.GetBuffer (), request.GetLength () + 1, 0) < 1)
   {
      FreeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }

   const int ChunkSize = 1024;
   char buffer[ChunkSize] = { 0, };

   bool finished = false;
   int recvPosition = 0;
   int symbolsInLine = 0;

   // scan for the end of the header
   while (!finished && recvPosition < ChunkSize)
   {
      if (recv (socketHandle, &buffer[recvPosition], 1, 0) == 0)
         finished = true;

      // ugly, but whatever
      if (buffer[recvPosition - 2] == '4' && buffer[recvPosition - 1] == '0' && buffer[recvPosition] == '4')
      {
         FreeSocket (socketHandle);
         return WDE_NOTFOUND_ERROR;
      }

      switch (buffer[recvPosition])
      {
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

   File fp (waypoints.CheckSubfolderFile (), "wb");

   if (!fp.IsValid ())
   {
      FreeSocket (socketHandle);
      return WDE_SOCKET_ERROR;
   }

   int recvSize = 0;

   do
   {
      recvSize = recv (socketHandle, buffer, ChunkSize, 0);

      if (recvSize > 0)
      {
         fp.Write (buffer, recvSize);
         fp.Flush ();
      }

   } while (recvSize != 0);

   fp.Close ();
   FreeSocket (socketHandle);

   return WDE_NOERROR;
}