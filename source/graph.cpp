//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) Yet Another POD-Bot Contributors <yapb@entix.io>.
//
// This software is licensed under the MIT license.
// Additional exceptions apply. For full license details, see LICENSE.txt
//

#include <yapb.h>

ConVar yb_graph_fixcamp ("yb_graph_fixcamp", "1", "Specifies whether bot should not 'fix' camp directions of camp waypoints when loading old PWF format.");
ConVar yb_graph_url ("yb_graph_url", "http://graph.yapb.ru", "Specifies the URL from bots will be able to download graph in case of missing local one.", false);

void BotGraph::initGraph () {
   // this function initialize the graph structures..

   m_loadAttempts = 0;
   m_editFlags = 0;

   m_learnVelocity = nullptr;
   m_learnPosition = nullptr;
   m_lastNode = nullptr;

   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;
   m_autoPathDistance = 250.0f;
   m_hasChanged = false;
   m_narrowChecked = false;

   // reset highest recorded damage
   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      m_highestDamage[team] = 1;
   }
}

int BotGraph::clearConnections (int index) {
   // this function removes the useless paths connections from and to node pointed by index. This is based on code from POD-bot MM from KWo

   if (!exists (index)) {
      return 0;
   }
   int numFixedLinks = 0;

   // wrapper form unassiged paths
   auto clearPath = [&] (int from, int to) {
      unassignPath (from, to);
      ++numFixedLinks;
   };

   if (bots.hasBotsOnline ()) {
      bots.kickEveryone (true);
   }

   struct Connection {
      int index;
      int number;
      int distance;
      float angles;

   public:
      Connection () {
         reset ();
      }

   public:
      void reset () {
         index = kInvalidNodeIndex;
         number = kInvalidNodeIndex;
         distance = kInfiniteDistanceLong;
         angles = 0.0f;
      }
   };
   auto &path = m_paths[index];

   Connection sorted[kMaxNodeLinks];
   Connection top;

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      auto &cur = sorted[i];
      const auto &link = path.links[i];

      cur.number = i;
      cur.index = link.index;
      cur.distance = link.distance;

      if (cur.index == kInvalidNodeIndex) {
         cur.distance = kInfiniteDistanceLong;
      }

      if (cur.distance < top.distance) {
         top.distance = link.distance;
         top.number = i;
         top.index = cur.index;
      }
   }

   if (top.number == kInvalidNodeIndex) {
      ctrl.msg ("Cannot find path to the closest connected node to node number %d.", index);
      return numFixedLinks;
   }
   bool sorting = false;

   // sort paths from the closest node to the farest away one...
   do {
      sorting = false;

      for (int i = 0; i < kMaxNodeLinks - 1; ++i) {
         if (sorted[i].distance > sorted[i + 1].distance) {
            cr::swap (sorted[i], sorted[i + 1]);
            sorting = true;
         }
      }
   } while (sorting);

   // calculate angles related to the angle of the closeset connected node
   for (auto &cur : sorted) {
      if (cur.index == kInvalidNodeIndex) {
         cur.distance = kInfiniteDistanceLong;
         cur.angles = 360.0f;
      }
      else if (exists (cur.index)) {
         cur.angles = ((m_paths[cur.index].origin - path.origin).angles () - (m_paths[sorted[0].index].origin - path.origin).angles ()).y;

         if (cur.angles < 0.0f) {
            cur.angles += 360.0f;
         }
      }
   }

   //  sort the paths from the lowest to the highest angle (related to the vector closest node - checked index)...
   do {
      sorting = false;

      for (int i = 0; i < kMaxNodeLinks - 1; ++i) {
         if (sorted[i].index != kInvalidNodeIndex && sorted[i].angles > sorted[i + 1].angles) {
            cr::swap (sorted[i], sorted[i + 1]);
            sorting = true;
         }
      }
   } while (sorting);

   // reset top state
   top.reset ();

   // printing all the stuff causes reliable message overflow
   ctrl.setRapidOutput (true);

   // check pass 0
   auto inspect_p0 = [&] (const int id) -> bool {
      if (id < 2) {
         return false;
      }
      auto &cur = sorted[id], &prev = sorted[id - 1], &prev2 = sorted[id - 2];

      if (cur.index == kInvalidNodeIndex || prev.index == kInvalidNodeIndex || prev2.index == kInvalidNodeIndex) {
         return false;
      }

      // store the highest index which should be tested later...
      top.index = cur.index;
      top.distance = cur.distance;
      top.angles = cur.angles;

      if (cur.angles - prev2.angles < 80.0f) {

         // leave alone ladder connections and don't remove jump connections..
         if (((path.flags & NodeFlag::Ladder) && (m_paths[prev.index].flags & NodeFlag::Ladder)) || (path.links[prev.number].flags & PathFlag::Jump)) {
            return false;
         }

         if ((cur.distance + prev2.distance) * 1.1f / 2.0f < static_cast <float> (prev.distance)) {
            if (path.links[prev.number].index == prev.index) {
               ctrl.msg ("Removing a useless (P.0.1) connection from index = %d to %d.", index, prev.index);

               // unassign this path
               clearPath (index, prev.number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[prev.index].links[j].index == index && !(m_paths[prev.index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.0.2) connection from index = %d to %d.", prev.index, index);

                     // unassign this path
                     clearPath (prev.index, j);
                  }
               }
               prev.index = kInvalidNodeIndex;

               for (int j = id - 1; j < kMaxNodeLinks - 1; ++j) {
                  sorted[j] = cr::move (sorted[j + 1]);
               }
               sorted[kMaxNodeLinks - 1].index = kInvalidNodeIndex;

               // do a second check
               return true;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.0) connection from index = %d to %d.", index, prev.index);
               return false;
            }
         }
      }
      return false;
   };


   for (int i = 2; i < kMaxNodeLinks; ++i) {
      while (inspect_p0 (i)) { }
   }

   // check pass 1
   if (exists (top.index) && exists (sorted[0].index) && exists (sorted[1].index)) {
      if ((sorted[1].angles - top.angles < 80.0f || 360.0f - (sorted[1].angles - top.angles) < 80.0f) && (!(m_paths[sorted[0].index].flags & NodeFlag::Ladder) || !(path.flags & NodeFlag::Ladder)) && !(path.links[sorted[0].number].flags & PathFlag::Jump)) {
         if ((sorted[1].distance + top.distance) * 1.1f / 2.0f < static_cast <float> (sorted[0].distance)) {
            if (path.links[sorted[0].number].index == sorted[0].index) {
               ctrl.msg ("Removing a useless (P.1.1) connection from index = %d to %d.", index, sorted[0].index);

               // unassign this path
               clearPath (index, sorted[0].number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[sorted[0].index].links[j].index == index && !(m_paths[sorted[0].index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.1.2) connection from index = %d to %d.", sorted[0].index, index);

                     // unassign this path
                     clearPath (sorted[0].index, j);
                  }
               }
               sorted[0].index = kInvalidNodeIndex;

               for (int j = 0; j < kMaxNodeLinks - 1; ++j) {
                  sorted[j] = cr::move (sorted[j + 1]);
               }
               sorted[kMaxNodeLinks - 1].index = kInvalidNodeIndex;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.1) connection from index = %d to %d.", sorted[0].index, index);
            }
         }
      }
   }
   top.reset ();

   // check pass 2
   auto inspect_p2 = [&] (const int id) -> bool {
      if (id < 1) {
         return false;
      }
      auto &cur = sorted[id], &prev = sorted[id - 1];

      if (cur.index == kInvalidNodeIndex || prev.index == kInvalidNodeIndex) {
         return false;
      }

      if (cur.angles - prev.angles < 40.0f) {
         if (prev.distance < static_cast <float> (cur.distance * 1.1f)) {

            // leave alone ladder connections and don't remove jump connections..
            if (((path.flags & NodeFlag::Ladder) && (m_paths[cur.index].flags & NodeFlag::Ladder)) || (path.links[cur.number].flags & PathFlag::Jump)) {
               return false;
            }

            if (path.links[cur.number].index == cur.index) {
               ctrl.msg ("Removing a useless (P.2.1) connection from index = %d to %d.", index, cur.index);

               // unassign this path
               clearPath (index, cur.number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[cur.index].links[j].index == index && !(m_paths[cur.index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.2.2) connection from index = %d to %d.", cur.index, index);

                     // unassign this path
                     clearPath (cur.index, j);
                  }
               }
               cur.index = kInvalidNodeIndex;

               for (int j = id - 1; j < kMaxNodeLinks - 1; ++j) {
                  sorted[j] = cr::move (sorted[j + 1]);
               }
               sorted[kMaxNodeLinks - 1].index = kInvalidNodeIndex;
               return true;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.2) connection from index = %d to %d.", index, cur.index);
            }
         }
         else if (cur.distance < static_cast <float> (prev.distance * 1.1f)) {
            // leave alone ladder connections and don't remove jump connections..
            if (((path.flags & NodeFlag::Ladder) && (m_paths[prev.index].flags & NodeFlag::Ladder)) || (path.links[prev.number].flags & PathFlag::Jump)) {
               return false;
            }

            if (path.links[prev.number].index == prev.index) {
               ctrl.msg ("Removing a useless (P.2.3) connection from index = %d to %d.", index, prev.index);

               // unassign this path
               clearPath (index, prev.number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[prev.index].links[j].index == index && !(m_paths[prev.index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.2.4) connection from index = %d to %d.", prev.index, index);

                     // unassign this path
                     clearPath (prev.index, j);
                  }
               }
               prev.index = kInvalidNodeIndex;

               for (int j = id - 1; j < kMaxNodeLinks - 1; ++j) {
                  sorted[j] = cr::move (sorted[j + 1]);
               }
               sorted[kMaxNodeLinks - 1].index = kInvalidNodeIndex;

               // do a second check
               return true;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.2) connection from index = %d to %d.", index, prev.index);
            }
         }
      }
      else {
         top = cur;
      }
      return false;
   };

   for (int i = 1; i < kMaxNodeLinks; ++i) {
      while (inspect_p2 (i)) { }
   }

   // check pass 3
   if (exists (top.index) && exists (sorted[0].index)) {
      if ((top.angles - sorted[0].angles < 40.0f || (360.0f - top.angles - sorted[0].angles) < 40.0f) && (!(m_paths[sorted[0].index].flags & NodeFlag::Ladder) || !(path.flags & NodeFlag::Ladder)) && !(path.links[sorted[0].number].flags & PathFlag::Jump)) {
         if (top.distance * 1.1f < static_cast <float> (sorted[0].distance)) {
            if (path.links[sorted[0].number].index == sorted[0].index) {
               ctrl.msg ("Removing a useless (P.3.1) connection from index = %d to %d.", index, sorted[0].index);

               // unassign this path
               clearPath (index, sorted[0].number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[sorted[0].index].links[j].index == index && !(m_paths[sorted[0].index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.3.2) connection from index = %d to %d.", sorted[0].index, index);

                     // unassign this path
                     clearPath (sorted[0].index, j);
                  }
               }
               sorted[0].index = kInvalidNodeIndex;

               for (int j = 0; j < kMaxNodeLinks - 1; ++j) {
                  sorted[j] = cr::move (sorted[j + 1]);
               }
               sorted[kMaxNodeLinks - 1].index = kInvalidNodeIndex;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.3) connection from index = %d to %d.", sorted[0].index, index);
            }
         }
         else if (sorted[0].distance * 1.1f < static_cast <float> (top.distance) && !(path.links[top.number].flags & PathFlag::Jump)) {
            if (path.links[top.number].index == top.index) {
               ctrl.msg ("Removing a useless (P.3.3) connection from index = %d to %d.", index, sorted[0].index);

               // unassign this path
               clearPath (index, top.number);

               for (int j = 0; j < kMaxNodeLinks; ++j) {
                  if (m_paths[top.index].links[j].index == index && !(m_paths[top.index].links[j].flags & PathFlag::Jump)) {
                     ctrl.msg ("Removing a useless (P.3.4) connection from index = %d to %d.", sorted[0].index, index);

                     // unassign this path
                     clearPath (top.index, j);
                  }
               }
               sorted[0].index = kInvalidNodeIndex;
            }
            else {
               ctrl.msg ("Failed to remove a useless (P.3) connection from index = %d to %d.", sorted[0].index, index);
            }
         }
      }
   }
   ctrl.setRapidOutput (false);

   return numFixedLinks;
}

void BotGraph::addPath (int addIndex, int pathIndex, float distance) {
   if (!exists (addIndex) || !exists (pathIndex)) {
      return;
   }
   auto &path = m_paths[addIndex];

   // don't allow paths get connected twice
   for (const auto &link : path.links) {
      if (link.index == pathIndex) {
         ctrl.msg ("Denied path creation from %d to %d (path already exists).", addIndex, pathIndex);
         return;
      }
   }

   // check for free space in the connection indices
   for (auto &link : path.links) {
      if (link.index == kInvalidNodeIndex) {
         link.index = static_cast <int16> (pathIndex);
         link.distance = cr::abs (static_cast <int> (distance));

         ctrl.msg ("Path added from %d to %d.", addIndex, pathIndex);
         return;
      }
   }

   // there wasn't any free space. try exchanging it with a long-distance path
   int maxDistance = -kInfiniteDistanceLong;
   int slot = kInvalidNodeIndex;

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      if (path.links[i].distance > maxDistance) {
         maxDistance = path.links[i].distance;
         slot = i;
      }
   }

   if (slot != kInvalidNodeIndex) {
      ctrl.msg ("Path added from %d to %d.", addIndex, pathIndex);

      path.links[slot].index = static_cast <int16> (pathIndex);
      path.links[slot].distance = cr::abs (static_cast <int> (distance));
   }
}

int BotGraph::getFarest (const Vector &origin, float maxDistance) {
   // find the farest node to that origin, and return the index to this node

   int index = kInvalidNodeIndex;
   maxDistance = cr::square (maxDistance);

   for (const auto &path : m_paths) {
      float distance = (path.origin - origin).lengthSq ();

      if (distance > maxDistance) {
         index = path.number;
         maxDistance = distance;
      }
   }
   return index;
}

int BotGraph::getNearestNoBuckets (const Vector &origin, float minDistance, int flags) {
   // find the nearest node to that origin and return the index

   // fallback and go thru wall the nodes...
   int index = kInvalidNodeIndex;
   minDistance = cr::square (minDistance);

   for (const auto &path : m_paths) {
      if (flags != -1 && !(path.flags & flags)) {
         continue; // if flag not -1 and node has no this flag, skip node
      }
      float distance = (path.origin - origin).lengthSq ();

      if (distance < minDistance) {
         index = path.number;
         minDistance = distance;
      }
   }
   return index;
}

int BotGraph::getEditorNeareset () {
   if (!hasEditFlag (GraphEdit::On)) {
      return kInvalidNodeIndex;
   }
   return getNearestNoBuckets (m_editor->v.origin, 50.0f);
}

int BotGraph::getNearest (const Vector &origin, float minDistance, int flags) {
   // find the nearest node to that origin and return the index

   auto &bucket = getNodesInBucket (origin);

   if (bucket.empty ()) {
      return getNearestNoBuckets (origin, minDistance, flags);
   }

   int index = kInvalidNodeIndex;
   auto minDistanceSq = cr::square (minDistance);

   for (const auto &at : bucket) {
      if (flags != -1 && !(m_paths[at].flags & flags)) {
         continue; // if flag not -1 and node has no this flag, skip node
      }
      float distance = (m_paths[at].origin - origin).lengthSq ();

      if (distance < minDistanceSq) {
         index = at;
         minDistanceSq = distance;
      }
   }

   // nothing found, try to find without buckets
   if (index == kInvalidNodeIndex) {
      return getNearestNoBuckets (origin, minDistance, flags);
   }
   return index;
}

IntArray BotGraph::searchRadius (float radius, const Vector &origin, int maxCount) {
   // returns all nodes within radius from position

   IntArray result;
   auto &bucket = getNodesInBucket (origin);

   if (bucket.empty ()) {
      result.push (getNearestNoBuckets (origin, radius));
      return cr::move (result);
   }
   radius = cr::square (radius);

   for (const auto &at : bucket) {
      if (maxCount != -1 && static_cast <int> (result.length ()) > maxCount) {
         break;
      }

      if ((m_paths[at].origin - origin).lengthSq () < radius) {
         result.push (at);
      }
   }
   return cr::move (result);
}

void BotGraph::add (int type, const Vector &pos) {
   // @todo: remove type magic numbers

   if (game.isNullEntity (m_editor)) {
      return;
   }
   int index = kInvalidNodeIndex;
   Path *path = nullptr;

   bool addNewNode = true;
   Vector newOrigin = pos.empty () ? m_editor->v.origin : pos;

   if (bots.hasBotsOnline ()) {
      bots.kickEveryone (true);
   }
   m_hasChanged = true;

   switch (type) {
   case 5:
      index = getEditorNeareset ();

      if (index != kInvalidNodeIndex) {
         path = &m_paths[index];

         if (path->flags & NodeFlag::Camp) {
            path->start = m_editor->v.v_angle.get2d ();

            // play "done" sound...
            game.playSound (m_editor, "common/wpn_hudon.wav");
            return;
         }
      }
      break;

   case 6:
      index = getEditorNeareset ();

      if (index != kInvalidNodeIndex) {
         path = &m_paths[index];

         if (!(path->flags & NodeFlag::Camp)) {
            ctrl.msg ("This is not camping node.");
            return;
         }
         path->end = m_editor->v.v_angle.get2d ();

         // play "done" sound...
         game.playSound (m_editor, "common/wpn_hudon.wav");
      }
      return;

   case 9:
      index = getEditorNeareset ();

      if (index != kInvalidNodeIndex && m_paths[index].number >= 0) {
         float distance = (m_paths[index].origin - m_editor->v.origin).length ();

         if (distance < 50.0f) {
            addNewNode = false;

            path = &m_paths[index];
            path->origin = (path->origin + m_learnPosition) * 0.5f;
         }
      }
      else {
         newOrigin = m_learnPosition;
      }
      break;

   case 10:
      index = getEditorNeareset ();

      if (index != kInvalidNodeIndex && m_paths[index].number >= 0) {
         float distance = (m_paths[index].origin - m_editor->v.origin).length ();

         if (distance < 50.0f) {
            addNewNode = false;
            path = &m_paths[index];

            int connectionFlags = 0;

            for (const auto &link : path->links) {
               connectionFlags += link.flags;
            }

            if (connectionFlags == 0) {
               path->origin = (path->origin + m_editor->v.origin) * 0.5f;
            }
         }
      }
      break;
   }

   if (addNewNode) {
      auto nearest = getEditorNeareset ();

      // do not allow to place waypoints "inside" waypoints, make at leat 10 units range
      if (exists (nearest) && (m_paths[nearest].origin - newOrigin).lengthSq () < cr::square (10.0f)) {
         ctrl.msg ("Can't add node. It's way to near to %d node. Please move some units anywhere.", nearest);
         return;
      }

      // need to remove limit?
      if (m_paths.length () >= kMaxNodes) {
         return;
      }
      m_paths.push (Path {});

      index = m_paths.length () - 1;
      path = &m_paths[index];

      path->number = index;
      path->flags = 0;

      // store the origin (location) of this node
      path->origin = newOrigin;
      addToBucket (newOrigin, index);

      path->start = nullptr;
      path->end = nullptr;
      
      path->display = 0.0f;
      path->light = 0.0f;

      for (auto &link : path->links) {
         link.index = kInvalidNodeIndex;
         link.distance = 0;
         link.flags = 0;
         link.velocity = nullptr;
      }
      

      // store the last used node for the auto node code...
      m_lastNode = m_editor->v.origin;
   }

   if (type == 9) {
      m_lastJumpNode = index;
   }
   else if (type == 10) {
      float distance = (m_paths[m_lastJumpNode].origin - m_editor->v.origin).length ();
      addPath (m_lastJumpNode, index, distance);

      for (auto &link : m_paths[m_lastJumpNode].links) {
         if (link.index == index) {
            link.flags |= PathFlag::Jump;
            link.velocity = m_learnVelocity;

            break;
         }
      }
      calculatePathRadius (index);
      return;
   }

   if (!path || path->number == kInvalidNodeIndex) {
      return;
   }

   if (m_editor->v.flags & FL_DUCKING) {
      path->flags |= NodeFlag::Crouch; // set a crouch node
   }

   if (m_editor->v.movetype == MOVETYPE_FLY) {
      path->flags |= NodeFlag::Ladder;
   }
   else if (m_isOnLadder) {
      path->flags |= NodeFlag::Ladder;
   }

   switch (type) {
   case 1:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::TerroristOnly;
      break;

   case 2:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::CTOnly;
      break;

   case 3:
      path->flags |= NodeFlag::NoHostage;
      break;

   case 4:
      path->flags |= NodeFlag::Rescue;
      break;

   case 5:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::Camp;

      path->start = m_editor->v.v_angle;
      break;

   case 100:
      path->flags |= NodeFlag::Goal;
      break;
   }

   // Ladder nodes need careful connections
   if (path->flags & NodeFlag::Ladder) {
      float minDistance = kInfiniteDistance;
      int destIndex = kInvalidNodeIndex;

      TraceResult tr {};

      // calculate all the paths to this new node
      for (const auto &calc : m_paths) {
         if (calc.number == index) {
            continue; // skip the node that was just added
         }

         // other ladder nodes should connect to this
         if (calc.flags & NodeFlag::Ladder) {
            // check if the node is reachable from the new one
            game.testLine (newOrigin, calc.origin, TraceIgnore::Monsters, m_editor, &tr);

            if (cr::fequal (tr.flFraction, 1.0f) && cr::abs (newOrigin.x - calc.origin.x) < 64.0f && cr::abs (newOrigin.y - calc.origin.y) < 64.0f && cr::abs (newOrigin.z - calc.origin.z) < m_autoPathDistance) {
               float distance = (calc.origin - newOrigin).length ();

               addPath (index, calc.number, distance);
               addPath (calc.number, index, distance);
            }
         }
         else {
            // check if the node is reachable from the new one
            if (isNodeReacheable (newOrigin, calc.origin) || isNodeReacheable (calc.origin, newOrigin)) {
               float distance = (calc.origin - newOrigin).length ();

               if (distance < minDistance) {
                  destIndex = calc.number;
                  minDistance = distance;
               }
            }
         }
      }

      if (exists (destIndex)) {
         // check if the node is reachable from the new one (one-way)
         if (isNodeReacheable (newOrigin, m_paths[destIndex].origin)) {
            addPath (index, destIndex, (m_paths[destIndex].origin - newOrigin).length ());
         }

         // check if the new one is reachable from the node (other way)
         if (isNodeReacheable (m_paths[destIndex].origin, newOrigin)) {
            addPath (destIndex, index, (m_paths[destIndex].origin - newOrigin).length ());
         }
      }
   }
   else {
      // calculate all the paths to this new node
      for (const auto &calc : m_paths) {
         if (calc.number == index) {
            continue; // skip the node that was just added
         }

         // check if the node is reachable from the new one (one-way)
         if (isNodeReacheable (newOrigin, calc.origin)) {
            addPath (index, calc.number, (calc.origin - newOrigin).length ());
         }

         // check if the new one is reachable from the node (other way)
         if (isNodeReacheable (calc.origin, newOrigin)) {
            addPath (calc.number, index, (calc.origin - newOrigin).length ());
         }
      }
      clearConnections (index);
   }
   game.playSound (m_editor, "weapons/xbow_hit1.wav");
   calculatePathRadius (index); // calculate the wayzone of this node
}

void BotGraph::erase (int target) {
   m_hasChanged = true;

   if (m_paths.empty ()) {
      return;
   }

   if (bots.hasBotsOnline ()) {
      bots.kickEveryone (true);
   }
   const int index = (target == kInvalidNodeIndex) ? getEditorNeareset () : target;

   if (!exists (index)) {
      return;
   }
   auto &path = m_paths[index];

   // unassign paths that points to this nodes
   for (auto &connected : m_paths) {
      for (auto &link : connected.links) {
         if (link.index == index) {
            link.index = kInvalidNodeIndex;
            link.flags = 0;
            link.distance = 0;
            link.velocity = nullptr;
         }
      }
   }

   // relink nodes so the index will match path number
   for (auto &relink : m_paths) {

      // if pathnumber bigger than deleted node...
      if (relink.number > index) {
         --relink.number;
      }

      for (auto &neighbour : relink.links) {
         if (neighbour.index > index) {
            --neighbour.index;
         }
      }
   }
   eraseFromBucket (path.origin, index);
   m_paths.remove (path);

   game.playSound (m_editor, "weapons/mine_activate.wav");
}

void BotGraph::toggleFlags (int toggleFlag) {
   // this function allow manually changing flags

   int index = getEditorNeareset ();

   if (index != kInvalidNodeIndex) {
      if (m_paths[index].flags & toggleFlag) {
         m_paths[index].flags &= ~toggleFlag;
      }
      else if (!(m_paths[index].flags & toggleFlag)) {
         if (toggleFlag == NodeFlag::Sniper && !(m_paths[index].flags & NodeFlag::Camp)) {
            ctrl.msg ("Cannot assign sniper flag to node %d. This is not camp node.", index);
            return;
         }
         m_paths[index].flags |= toggleFlag;
      }

      // play "done" sound...
      game.playSound (m_editor, "common/wpn_hudon.wav");
   }
}

void BotGraph::setRadius (int index, float radius) {
   // this function allow manually setting the zone radius

   int node = exists (index) ? index : getEditorNeareset ();

   if (node != kInvalidNodeIndex) {
      m_paths[node].radius = static_cast <float> (radius);

      // play "done" sound...
      game.playSound (m_editor, "common/wpn_hudon.wav");
   }
}

bool BotGraph::isConnected (int a, int b) {
   // this function checks if node A has a connection to node B

   if (!exists (a) || !exists (b)) {
      return false;
   }

   for (const auto &link : m_paths[a].links) {
      if (link.index == b) {
         return true;
      }
   }
   return false;
}

int BotGraph::getFacingIndex () {
   // find the waypoint the user is pointing at

   Twin <int32, float> result { kInvalidNodeIndex, 5.32f };
   auto nearestNode = getEditorNeareset ();

   // check bounds from eyes of editor
   const auto &editorEyes = m_editor->v.origin + m_editor->v.view_ofs;

   for (const auto &path : m_paths) {

      // skip nearest waypoint to editor, since this used mostly for adding / removing paths
      if (path.number == nearestNode) {
         continue;
      }

      const auto &to = path.origin - m_editor->v.origin;
      auto angles = (to.angles () - m_editor->v.v_angle).clampAngles ();

      // skip the waypoints that are too far away from us, and we're not looking at them directly
      if (to.lengthSq () > cr::square (500.0f) || cr::abs (angles.y) > result.second) {
         continue;
      }
     
      // check if visible, (we're not using visiblity tables here, as they not valid at time of waypoint editing)
      TraceResult tr {};
      game.testLine (editorEyes, path.origin, TraceIgnore::Everything, m_editor, &tr);

      if (!cr::fequal (tr.flFraction, 1.0f)) {
         continue;
      }
      const float bestAngle = angles.y;

      angles = -m_editor->v.v_angle;
      angles.x = -angles.x;
      angles = (angles + ((path.origin - Vector (0.0f, 0.0f, (path.flags & NodeFlag::Crouch) ? 17.0f : 34.0f)) - editorEyes).angles ()).clampAngles ();

      if (angles.x > 0.0f) {
         continue;
      }
      result = { path.number, bestAngle };
   }
   return result.first;
}

void BotGraph::pathCreate (char dir) {
   // this function allow player to manually create a path from one node to another

   int nodeFrom = getEditorNeareset ();

   if (nodeFrom == kInvalidNodeIndex) {
      ctrl.msg ("Unable to find nearest node in 50 units.");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (!exists (nodeTo)) {
      if (exists (m_cacheNodeIndex)) {
         nodeTo = m_cacheNodeIndex;
      }
      else {
         ctrl.msg ("Unable to find destination node.");
         return;
      }
   }

   if (nodeTo == nodeFrom) {
      ctrl.msg ("Unable to connect node with itself.");
      return;
   }

   float distance = (m_paths[nodeTo].origin - m_paths[nodeFrom].origin).length ();

   if (dir == PathConnection::Outgoing) {
      addPath (nodeFrom, nodeTo, distance);
   }
   else if (dir == PathConnection::Incoming) {
      addPath (nodeTo, nodeFrom, distance);
   }
   else {
      addPath (nodeFrom, nodeTo, distance);
      addPath (nodeTo, nodeFrom, distance);
   }

   game.playSound (m_editor, "common/wpn_hudon.wav");
   m_hasChanged = true;
}

void BotGraph::erasePath () {
   // this function allow player to manually remove a path from one node to another

   int nodeFrom = getEditorNeareset ();

   if (nodeFrom == kInvalidNodeIndex) {
      ctrl.msg ("Unable to find nearest node in 50 units.");
      return;
   }
   int nodeTo = m_facingAtIndex;

   if (!exists (nodeTo)) {
      if (exists (m_cacheNodeIndex)) {
         nodeTo = m_cacheNodeIndex;
      }
      else {
         ctrl.msg ("Unable to find destination node.");
         return;
      }
   }

   // helper
   auto destroy = [] (PathLink &link) -> void {
      link.index = kInvalidNodeIndex;
      link.distance = 0;
      link.flags = 0;
      link.velocity = nullptr;
   };

   for (auto &link : m_paths[nodeFrom].links) {
      if (link.index == nodeTo) {
         destroy (link);
         game.playSound (m_editor, "weapons/mine_activate.wav");

         return;
      }
   }

   // not found this way ? check for incoming connections then
   cr::swap (nodeFrom, nodeTo);

   for (auto &link : m_paths[nodeFrom].links) {
      if (link.index == nodeTo) {
         destroy (link);
         game.playSound (m_editor, "weapons/mine_activate.wav");

         return;
      }
   }
   ctrl.msg ("There is already no path on this node.");
}

void BotGraph::cachePoint (int index) {
   int node = exists (index) ? index : getEditorNeareset ();

   if (node == kInvalidNodeIndex) {
      m_cacheNodeIndex = kInvalidNodeIndex;
      ctrl.msg ("Cached node cleared (nearby point not found in 50 units range).");

      return;
   }
   m_cacheNodeIndex = node;
   ctrl.msg ("Node %d has been put into memory.", m_cacheNodeIndex);
}

void BotGraph::calculatePathRadius (int index) {
   // calculate "wayzones" for the nearest node  (meaning a dynamic distance area to vary node origin)

   auto &path = m_paths[index];
   Vector start, direction;

   if ((path.flags & (NodeFlag::Ladder | NodeFlag::Goal | NodeFlag::Camp | NodeFlag::Rescue | NodeFlag::Crouch)) || m_jumpLearnNode) {
      path.radius = 0.0f;
      return;
   }

   for (const auto &test : path.links) {
      if (test.index != kInvalidNodeIndex && (m_paths[test.index].flags & NodeFlag::Ladder)) {
         path.radius = 0.0f;
         return;
      }
   }
   TraceResult tr {};
   bool wayBlocked = false;

   for (float scanDistance = 32.0f; scanDistance < 128.0f; scanDistance += 16.0f) {
      start = path.origin;

      direction = Vector (0.0f, 0.0f, 0.0f).forward () * scanDistance;
      direction = direction.angles ();

      path.radius = scanDistance;

      for (float circleRadius = 0.0f; circleRadius < 360.0f; circleRadius += 20.0f) {
         const auto &forward = direction.forward ();

         Vector radiusStart = start + forward * scanDistance;
         Vector radiusEnd = start + forward * scanDistance;

         game.testHull (radiusStart, radiusEnd, TraceIgnore::Monsters, head_hull, nullptr, &tr);

         if (tr.flFraction < 1.0f) {
            game.testLine (radiusStart, radiusEnd, TraceIgnore::Monsters, nullptr, &tr);

            if (tr.pHit && strncmp ("func_door", tr.pHit->v.classname.chars (), 9) == 0) {
               path.radius = 0.0f;
               wayBlocked = true;

               break;
            }
            wayBlocked = true;
            path.radius -= 16.0f;

            break;
         }

         Vector dropStart = start + forward * scanDistance;
         Vector dropEnd = dropStart - Vector (0.0f, 0.0f, scanDistance + 60.0f);

         game.testHull (dropStart, dropEnd, TraceIgnore::Monsters, head_hull, nullptr, &tr);

         if (tr.flFraction >= 1.0f) {
            wayBlocked = true;
            path.radius -= 16.0f;

            break;
         }
         dropStart = start - forward * scanDistance;
         dropEnd = dropStart - Vector (0.0f, 0.0f, scanDistance + 60.0f);

         game.testHull (dropStart, dropEnd, TraceIgnore::Monsters, head_hull, nullptr, &tr);

         if (tr.flFraction >= 1.0f) {
            wayBlocked = true;
            path.radius -= 16.0f;
            break;
         }

         radiusEnd.z += 34.0f;
         game.testHull (radiusStart, radiusEnd, TraceIgnore::Monsters, head_hull, nullptr, &tr);

         if (tr.flFraction < 1.0f) {
            wayBlocked = true;
            path.radius -= 16.0f;

            break;
         }
         direction.y = cr::normalizeAngles (direction.y + circleRadius);
      }

      if (wayBlocked) {
         break;
      }
   }
   path.radius -= 16.0f;

   if (path.radius < 0.0f) {
      path.radius = 0.0f;
   }
}

void BotGraph::loadPractice () {
   if (m_paths.empty ()) {
      return;
   }
   
   // reset highest recorded damage
   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      m_highestDamage[team] = 1;
   }

   bool dataLoaded = loadStorage <Practice> ("prc", "Practice", StorageOption::Practice, StorageVersion::Practice, m_practice, nullptr, nullptr);
   int count = m_paths.length ();

   // set's the highest damage if loaded ok
   if (dataLoaded) {
      auto practice = m_practice.data ();

      for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
         for (int i = 0; i < count; ++i) {
            for (int j = 0; j < count; ++j) {
               if (i == j) {
                  if ((practice + (i * count) + j)->damage[team] > m_highestDamage[team]) {
                     m_highestDamage[team] = (practice + (i * count) + j)->damage[team];
                  }
               }
            }
         }
      }
      return;
   }
   auto practice = m_practice.data ();

   // initialize table by hand to correct values, and NOT zero it out
   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      for (int i = 0; i < count; ++i) {
         for (int j = 0; j < count; ++j) {
            (practice + (i * count) + j)->index[team] = kInvalidNodeIndex;
            (practice + (i * count) + j)->damage[team] = 0;
            (practice + (i * count) + j)->value[team] = 0;
         }
      }
   }
}

void BotGraph::savePractice () {
   if (m_paths.empty () || m_hasChanged) {
      return;
   }
   saveStorage <Practice> ("prc", "Practice", StorageOption::Practice, StorageVersion::Practice, m_practice, nullptr);
}

void BotGraph::loadVisibility () {
   m_needsVisRebuild = true;

   if (m_paths.empty ()) {
      return;
   }
   bool dataLoaded = loadStorage <uint8> ("vis", "Visibility", StorageOption::Vistable, StorageVersion::Vistable, m_vistable, nullptr, nullptr);

   // if loaded, do not recalculate visibility
   if (dataLoaded) {
      m_needsVisRebuild = false;
   }
}

void BotGraph::saveVisibility () {
   if (m_paths.empty () || m_hasChanged || m_needsVisRebuild) {
      return;
   }
   saveStorage <uint8> ("vis", "Visibility", StorageOption::Vistable, StorageVersion::Vistable, m_vistable, nullptr);
}

bool BotGraph::loadPathMatrix () {
   if (m_paths.empty ()) {
      return false;
   }
   bool dataLoaded = loadStorage <Matrix> ("pmx", "Pathmatrix", StorageOption::Matrix, StorageVersion::Matrix, m_matrix, nullptr, nullptr);

   // do not rebuild if loaded
   if (dataLoaded) {
      return true;
   }
   auto count = length ();
   auto matrix = m_matrix.data ();

   for (int i = 0; i < count; ++i) {
      for (int j = 0; j < count; ++j) {
         (matrix + i * count + j)->dist = SHRT_MAX;
         (matrix + i * count + j)->index = kInvalidNodeIndex;
      }
   }

   for (int i = 0; i < count; ++i) {
      for (const auto &link : m_paths[i].links) {
         if (!exists (link.index)) {
            continue;
         }
         (matrix + (i * count) + link.index)->dist = static_cast <int16> (link.distance);
         (matrix + (i * count) + link.index)->index = static_cast <int16> (link.index);
      }
   }

   for (int i = 0; i < count; ++i) {
      (matrix + (i * count) + i)->dist = 0;
   }

   for (int k = 0; k < count; ++k) {
      for (int i = 0; i < count; ++i) {
         for (int j = 0; j < count; ++j) {
            int distance = (matrix + (i * count) + k)->dist + (matrix + (k * count) + j)->dist;

            if (distance < (matrix + (i * count) + j)->dist) {
               (matrix + (i * count) + j)->dist = static_cast <int16> (distance);
               (matrix + (i * count) + j)->index = static_cast <int16> ((matrix + (i * count) + k)->index);
            }
         }
      }
   }
   savePathMatrix (); // save path matrix to file for faster access

   return true;
}

void BotGraph::savePathMatrix () {
   if (m_paths.empty ()) {
      return;
   }
   saveStorage <Matrix> ("pmx", "Pathmatrix", StorageOption::Matrix, StorageVersion::Matrix, m_matrix, nullptr);
}

void BotGraph::initLightLevels () {
   // this function get's the light level for each waypoin on the map

   // no nodes ? no light levels, and only one-time init
   if (m_paths.empty () || !cr::fzero (m_paths[0].light)) {
      return;
   }

   // update light levels for all nodes
   for (auto &path : m_paths) {
      path.light = illum.getLightLevel (path.origin);
   }
   // disable lightstyle animations on finish (will be auto-enabled on mapchange)
   illum.enableAnimation (false);

}

void BotGraph::initNarrowPlaces () {
   // this function checks all nodes if they are inside narrow places. this is used to prevent
   // bots to track hidden enemies in narrow places and prevent bots from throwing flashbangs or
   // other grenades inside bad places.

   // no nodes ? 
   if (m_paths.empty () || m_narrowChecked) {
      return;
   }
   TraceResult tr;

   const auto distance = 178.0f;
   const auto worldspawn = game.getStartEntity ();
   const auto offset = Vector (0.0f, 0.0f, 16.0f);

   // check olny paths that have not too much connections
   for (auto &path : m_paths) {

      // skip any goals and camp points
      if (path.flags & (NodeFlag::Camp | NodeFlag::Goal)) {
         continue;
      }
      int linkCount = 0;

      for (const auto &link : path.links) {
         if (link.index == kInvalidNodeIndex || link.index == path.number) {
            continue;
         }

         if (++linkCount > kMaxNodeLinks / 2) {
            break;
         }
      }

      // skip nodes with too much connections, this indicated we're not in narrow place
      if (linkCount > kMaxNodeLinks / 2) {
         continue;
      }
      int accumWeight = 0;

      // we could use this one!
      for (const auto &link : path.links) {
         if (link.index == kInvalidNodeIndex || link.index == path.number) {
            continue;
         }
         const Vector &ang = ((path.origin - m_paths[link.index].origin).normalize () * distance).angles ();

         Vector forward, right, upward;
         ang.angleVectors (&forward, &right, &upward);

         // helper lambda
         auto directionCheck = [&] (const Vector &to, int weight) {
            game.testLine (path.origin + offset, to, TraceIgnore::None, nullptr, &tr);

            // check if we're hit worldspawn entity
            if (tr.pHit == worldspawn && tr.flFraction < 1.0f) {
               accumWeight += weight;
            }
         };
         directionCheck (-forward * distance, 1);

         directionCheck (right * distance, 1);
         directionCheck (-right * distance, 1);

         directionCheck (upward * distance, 1);
      }
      path.flags &= ~NodeFlag::Narrow;

      if (accumWeight > 1) {
         path.flags |= NodeFlag::Narrow;
      }
      accumWeight = 0;
   }
   m_narrowChecked = true;
}

void BotGraph::initNodesTypes () {
   m_terrorPoints.clear ();
   m_ctPoints.clear ();
   m_goalPoints.clear ();
   m_campPoints.clear ();
   m_rescuePoints.clear ();
   m_sniperPoints.clear ();
   m_visitedGoals.clear ();

   for (const auto &path : m_paths) {
      if (path.flags & NodeFlag::TerroristOnly) {
         m_terrorPoints.push (path.number);
      }
      else if (path.flags & NodeFlag::CTOnly) {
         m_ctPoints.push (path.number);
      }
      else if (path.flags & NodeFlag::Goal) {
         m_goalPoints.push (path.number);
      }
      else if (path.flags & NodeFlag::Camp) {
         m_campPoints.push (path.number);
      }
      else if (path.flags & NodeFlag::Sniper) {
         m_sniperPoints.push (path.number);
      }
      else if (path.flags & NodeFlag::Rescue) {
         m_rescuePoints.push (path.number);
      }
   }
}

bool BotGraph::convertOldFormat () {
   MemFile fp (getOldFormatGraphName (true));
 
   PODGraphHeader header;
   plat.bzero (&header, sizeof (header));

   // save for faster access
   auto map = game.getMapName ();

   if (fp) {

      if (fp.read (&header, sizeof (header)) == 0) {
         return false;
      }

      if (strncmp (header.header, kPodbotMagic, cr::bufsize (kPodbotMagic)) == 0) {
         if (header.fileVersion != StorageVersion::Podbot) {
            return false;
         }
         else if (!strings.matches (header.mapName, map)) {
            return false;
         }
         else {
            if (header.pointNumber == 0 || header.pointNumber > kMaxNodes) {
               return false;
            }
            initGraph ();
            m_paths.clear ();

            for (int i = 0; i < header.pointNumber; ++i) {
               Path path {};
               PODPath podpath {};

               if (fp.read (&podpath, sizeof (PODPath)) == 0) {
                  return false;
               }
               convertFromPOD (path, podpath);
   
               // more checks of node quality
               if (path.number < 0 || path.number > header.pointNumber) {
                  return false;
               }
               // add to node array
               m_paths.push (cr::move (path));
            }
         }
      }
      else {
         return false;
      }
      fp.close ();
   }
   else {
      return false;
   }

   // save new format in case loaded older one
   if (!m_paths.empty ()) {
      game.print ("Converting old PWF to new format Graph.");

      m_tempStrings = header.author;
      return saveGraphData ();
   }
   return true;
}

template <typename U> bool BotGraph::saveStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, const SmallArray <U> &data, ExtenHeader *exten) {
   bool isGraph = (ext == "graph");

   String filename;
   filename.assignf ("%s.%s", game.getMapName (), ext.chars ());

   if (data.empty ()) {
      logger.error ("Unable to save %s file. Empty data. (filename: '%s').", name.chars (), filename.chars ());
      return false;
   }
   else if (isGraph) {
      for (auto &path : m_paths) {
         path.display = 0.0f;
         path.light = illum.getLightLevel (path.origin);
      }
   }

   // open the file
   File file (strings.format ("%s%s/%s", getDataDirectory (false), isGraph ? "graph" : "learned", filename.chars ()), "wb");

   // no open no fun
   if (!file) {
      logger.error ("Unable to open %s file for writing (filename: '%s').", name.chars (), filename.chars ());
      file.close ();

      return false;
   }
   ULZ lz;

   size_t rawLength = data.length () * sizeof (U);
   SmallArray <uint8> compressed (rawLength + sizeof (uint8) * ULZ::Excess);

   // try to compress
   auto compressedLength = lz.compress (reinterpret_cast <uint8 *> (data.data ()), rawLength, reinterpret_cast <uint8 *> (compressed.data ()));

   if (compressedLength > 0) {
      StorageHeader hdr;

      hdr.magic = kStorageMagic;
      hdr.version = version;
      hdr.options = options;
      hdr.length = m_paths.length ();
      hdr.compressed = compressedLength;
      hdr.uncompressed = rawLength;
        
      file.write (&hdr, sizeof (StorageHeader));
      file.write (compressed.data (), sizeof (uint8), compressedLength);

      // add extension
      if ((options & StorageOption::Exten) && exten != nullptr) {
         file.write (exten, sizeof (ExtenHeader));
      }
      game.print ("Successfully saved Bots %s data.", name.chars ());
   }
   else {
      logger.error ("Unable to compress %s data (filename: '%s').", name.chars (), filename.chars ());
      file.close ();

      return false;
   }
   file.close ();
   return true;
}

template <typename U> bool BotGraph::loadStorage (const String &ext, const String &name, StorageOption options, StorageVersion version, SmallArray <U> &data, ExtenHeader *exten, int32 *outOptions) {
   String filename;
   filename.assignf ("%s.%s", game.getMapName (), ext.chars ()).lowercase ();

   // graphs can be downloaded...
   bool isGraph = (ext == "graph");
   MemFile file (strings.format ("%s%s/%s", getDataDirectory (true), isGraph ? "graph" : "learned", filename.chars ())); // open the file

   // resize data to fit the stuff
   auto resizeData = [&] (const size_t length) {
      data.resize (length); // for non-graph data the graph should be already loaded
      data.shrink (); // free up memory to minimum

      // ensure we're have enough memory to decompress the data
      data.ensure (length + ULZ::Excess);
   };

   // allocate non-graph data, so we're will be ok in case if loading will fail
   if (!isGraph) {
      resizeData (m_paths.length () * m_paths.length ());
   }

   // error has occured
   auto bailout = [&] (const char *fmt, ...) -> bool {
      va_list args;
      auto result = strings.chars ();

      // concatenate string
      va_start (args, fmt);
      vsnprintf (result, StringBuffer::StaticBufferSize, fmt, args);
      va_end (args);

      logger.error (result);

      // if graph reset paths
      if (isGraph) {
         bots.kickEveryone (true);

         m_tempStrings = result;
         m_paths.clear ();
      }
      file.close ();

      return false;
   };

   // if graph & attempted to load multiple times, bail out, we're failed
   if (isGraph && ++m_loadAttempts > 2) {
      m_loadAttempts = 0;
      return bailout ("Unable to load %s (filename: '%s'). Download process has failed as well. No nodes has been found.", name.chars (), filename.chars ());
   }

   // downloader for graph
   auto download = [&] () -> bool {
      auto toDownload = strings.format ("%sgraph/%s", getDataDirectory (false), filename.chars ());
      auto fromDownload = strings.format ("%s/graph/%s", yb_graph_url.str (), filename.chars ());

      // try to download
      if (http.downloadFile (fromDownload, toDownload))  {
         game.print ("%s file '%s' successfully downloaded. Processing...", name.chars (), filename.chars ());
         return true;
      }
      else {
         game.print ("Can't download '%s'. from '%s' to '%s'... (%d).", filename.chars (), fromDownload, toDownload, http.getLastStatusCode ());
      }
      return false;
   };

   // tries to reload or open pwf file
   auto tryReload = [&] () -> bool {
      file.close ();

      if (!isGraph) {
         return false;
      }

      if (download ()) {
         return loadStorage <U> (ext, name, options, version, data, exten, outOptions);
      }
      
      if (convertOldFormat ()) {
         return loadStorage <U> (ext, name, options, version, data, exten, outOptions);
      }
      return false;
   };

   // no open no fun
   if (!file) {
      if (tryReload ()) {
         return true;
      }
      return bailout ("Unable to open %s file for reading (filename: '%s').", name.chars (), filename.chars ());
   }

   // read the header
   StorageHeader hdr;
   file.read (&hdr, sizeof (StorageHeader));

   // check the magic
   if (hdr.magic != kStorageMagic) {
      if (tryReload ()) {
         return true;
      }
      return bailout ("Unable to read magic of %s (filename: '%s').", name.chars (), filename.chars ());
   }

   // check the path-numbers
   if (!isGraph && hdr.length != length ()) {
      return bailout ("Damaged %s (filename: '%s'). Mismatch number of nodes (got: '%d', need: '%d').", name.chars (), filename.chars (), hdr.length, m_paths.length ());
   }

   // check the count
   if (hdr.length == 0 || hdr.length > kMaxNodes) {
      if (tryReload ()) {
         return true;
      }
      return bailout ("Damaged %s (filename: '%s'). Paths length is overflowed (got: '%d').", name.chars (), filename.chars (), hdr.length);
   }

   // check the version
   if (hdr.version != version) {
      if (tryReload ()) {
         return true;
      }
      return bailout ("Damaged %s (filename: '%s'). Version number differs (got: '%d', need: '%d').", name.chars (), filename.chars (), hdr.version, version);
   }

   // check the storage type
   if ((hdr.options & options) != options) {
      return bailout ("Incorrect storage format for %s (filename: '%s').", name.chars (), filename.chars ());
   }
   SmallArray <uint8> compressed (hdr.compressed + sizeof (uint8) * ULZ::Excess);

   // graph is not resized upon load
   if (isGraph) {
      resizeData (hdr.length);
   }

   // read compressed data
   if (file.read (compressed.data (), sizeof (uint8), hdr.compressed) == static_cast <size_t> (hdr.compressed)) {
      ULZ lz;

      // try to uncompress
      if (lz.uncompress (compressed.data (), hdr.compressed, reinterpret_cast <uint8 *> (data.data ()), hdr.uncompressed) == ULZ::UncompressFailure) {
         return bailout ("Unable to decompress ULZ data for %s (filename: '%s').", name.chars (), filename.chars ());
      }
      else {
         
         if (outOptions) {
            outOptions = &hdr.options;
         }

         // author of graph.. save
         if ((hdr.options & StorageOption::Exten) && exten != nullptr) {
            file.read (exten, sizeof (ExtenHeader));
         }
         game.print ("Successfully loaded Bots %s data (%d/%.2fMB).", name.chars (), data.length (), static_cast <float> (data.capacity () * sizeof (U)) / 1024.0f / 1024.0f);
         file.close ();

         return true;
      }
   }
   else {
      return bailout ("Unable to read ULZ data for %s (filename: '%s').", name.chars (), filename.chars ());
   }
   return false;
}

bool BotGraph::loadGraphData () {
   ExtenHeader exten {};
   int32 outOptions = 0;

   m_paths.clear ();

   // check if loaded
   bool dataLoaded = loadStorage <Path> ("graph", "Graph", StorageOption::Graph, StorageVersion::Graph, m_paths, &exten, &outOptions);

   if (dataLoaded) {
      initGraph ();
      initBuckets ();

      // add data to buckets
      for (const auto &path : m_paths) {
         addToBucket (path.origin, path.number);
      }

      if ((outOptions & StorageOption::Official) || strncmp (exten.author, "official", 8) == 0 || strlen (exten.author) < 2) {
         m_tempStrings.assign ("Using Official Navigation Graph");
      }
      else {
         m_tempStrings.assignf ("Navigation Graph Authord By: %s", exten.author);
      }
      initNodesTypes ();
      loadPathMatrix ();
      loadVisibility ();
      loadPractice ();

      if (exten.mapSize > 0) {
         int mapSize = engfuncs.pfnGetFileSize (strings.format ("maps/%s.bsp", game.getMapName ()));

         if (mapSize != exten.mapSize) {
            game.print ("Warning: Graph data is probably not for this map. Please check bots behaviour.");
         }
      }
      extern ConVar yb_debug_goal;
      yb_debug_goal.set (kInvalidNodeIndex);

      return true;
   }
   return false;
}

bool BotGraph::saveGraphData () {
   auto options = StorageOption::Graph | StorageOption::Exten;
   String author;

   if (game.isNullEntity (m_editor) && !m_tempStrings.empty ()) {
      author = m_tempStrings;

      options |= StorageOption::Recovered;
   }
   else if (!game.isNullEntity (m_editor)) {
      author = m_editor->v.netname.chars ();
   }
   else {
      author = "YAPB";
   }

   // mark as official
   if (author.startsWith ("YAPB")) {
      options |= StorageOption::Official;
   }

   ExtenHeader exten {};
   strings.copy (exten.author, author.chars (), cr::bufsize (exten.author));
   exten.mapSize = engfuncs.pfnGetFileSize (strings.format ("maps/%s.bsp", game.getMapName ()));

   return saveStorage <Path> ("graph", "Graph", static_cast <StorageOption> (options), StorageVersion::Graph, m_paths, &exten);
}

void BotGraph::saveOldFormat () {
   PODGraphHeader header {};

   strings.copy (header.header, kPodbotMagic, sizeof (kPodbotMagic));
   strings.copy (header.author, m_editor->v.netname.chars (), cr::bufsize (header.author));
   strings.copy (header.mapName, game.getMapName (), cr::bufsize (header.mapName));

   header.mapName[31] = 0;
   header.fileVersion = StorageVersion::Podbot;
   header.pointNumber = m_paths.length ();

   File fp;

   // file was opened
   if (fp.open (getOldFormatGraphName (), "wb")) {
      // write the node header to the file...
      fp.write (&header, sizeof (header));

      // save the node paths...
      for (const auto &path : m_paths) {
         PODPath pod {};
         convertToPOD (path, pod);

         fp.write (&pod, sizeof (PODPath));
      }
      fp.close ();
   }
   else {
      logger.error ("Error writing '%s.pwf' node file.", game.getMapName ());
   }
}

const char *BotGraph::getOldFormatGraphName (bool isMemoryFile) {
   static String buffer;
   buffer.assignf ("%s%s.pwf", getDataDirectory (isMemoryFile), game.getMapName ());

   if (File::exists (buffer)) {
      return buffer.chars ();
   }
   return strings.format ("%s%s.pwf", getDataDirectory (isMemoryFile), game.getMapName ());
}

float BotGraph::calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin) {
   // this function returns 2D traveltime to a position

   return (origin - src).length2d () / maxSpeed;
}

bool BotGraph::isNodeReacheable (const Vector &src, const Vector &destination) {
   TraceResult tr {};

   float distance = (destination - src).length ();

   // is the destination not close enough?
   if (distance > m_autoPathDistance) {
      return false;
   }

   // check if we go through a func_illusionary, in which case return false
   game.testHull (src, destination, TraceIgnore::Monsters, head_hull, m_editor, &tr);

   if (tr.pHit && strcmp ("func_illusionary", tr.pHit->v.classname.chars ()) == 0) {
      return false; // don't add pathnodes through func_illusionaries
   }

   // check if this node is "visible"...
   game.testLine (src, destination, TraceIgnore::Monsters, m_editor, &tr);

   // if node is visible from current position (even behind head)...
   if (tr.flFraction >= 1.0f || (tr.pHit && strncmp ("func_door", tr.pHit->v.classname.chars (), 9) == 0)) {
      // if it's a door check if nothing blocks behind
      if (strncmp ("func_door", tr.pHit->v.classname.chars (), 9) == 0) {
         game.testLine (tr.vecEndPos, destination, TraceIgnore::Monsters, tr.pHit, &tr);

         if (tr.flFraction < 1.0f) {
            return false;
         }
      }

      // check for special case of both nodes being in water...
      if (engfuncs.pfnPointContents (src) == CONTENTS_WATER && engfuncs.pfnPointContents (destination) == CONTENTS_WATER) {
         return true; // then they're reachable each other
      }

      // is dest node higher than src? (45 is max jump height)
      if (destination.z > src.z + 45.0f) {
         Vector sourceNew = destination;
         Vector destinationNew = destination;
         destinationNew.z = destinationNew.z - 50.0f; // straight down 50 units

         game.testLine (sourceNew, destinationNew, TraceIgnore::Monsters, m_editor, &tr);

         // check if we didn't hit anything, if not then it's in mid-air
         if (tr.flFraction >= 1.0) {
            return false; // can't reach this one
         }
      }

      // check if distance to ground drops more than step height at points between source and destination...
      Vector direction = (destination - src).normalize (); // 1 unit long
      Vector check = src, down = src;

      down.z = down.z - 1000.0f; // straight down 1000 units

      game.testLine (check, down, TraceIgnore::Monsters, m_editor, &tr);

      float lastHeight = tr.flFraction * 1000.0f; // height from ground
      distance = (destination - check).length (); // distance from goal

      while (distance > 10.0f) {
         // move 10 units closer to the goal...
         check = check + (direction * 10.0f);

         down = check;
         down.z = down.z - 1000.0f; // straight down 1000 units

         game.testLine (check, down, TraceIgnore::Monsters, m_editor, &tr);

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

void BotGraph::rebuildVisibility () {
   if (!m_needsVisRebuild) {
      return;
   }

   TraceResult tr {};
   uint8 res, shift;

   for (const auto &vis : m_paths) {
      Vector sourceDuck = vis.origin;
      Vector sourceStand = vis.origin;

      if (vis.flags & NodeFlag::Crouch) {
         sourceDuck.z += 12.0f;
         sourceStand.z += 18.0f + 28.0f;
      }
      else {
         sourceDuck.z += -18.0f + 12.0f;
         sourceStand.z += 28.0f;
      }
      uint16 standCount = 0, crouchCount = 0;

      for (const auto &path : m_paths) {
         // first check ducked visibility
         Vector dest = path.origin;

         game.testLine (sourceDuck, dest, TraceIgnore::Monsters, nullptr, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (!cr::fequal (tr.flFraction, 1.0f) || tr.fStartSolid) {
            res = 1;
         }
         else {
            res = 0;
         }
         res <<= 1;

         game.testLine (sourceStand, dest, TraceIgnore::Monsters, nullptr, &tr);

         // check if line of sight to object is not blocked (i.e. visible)
         if (cr::fequal (tr.flFraction, 1.0f) || tr.fStartSolid) {
            res |= 1;
         }

         if (res != 0) {
            dest = path.origin;

            // first check ducked visibility
            if (path.flags & NodeFlag::Crouch) {
               dest.z += 18.0f + 28.0f;
            }
            else {
               dest.z += 28.0f;
            }
            game.testLine (sourceDuck, dest, TraceIgnore::Monsters, nullptr, &tr);

            // check if line of sight to object is not blocked (i.e. visible)
            if (!cr::fequal (tr.flFraction, 1.0f) || tr.fStartSolid) {
               res |= 2;
            }
            else {
               res &= 1;
            }
            game.testLine (sourceStand, dest, TraceIgnore::Monsters, nullptr, &tr);

            // check if line of sight to object is not blocked (i.e. visible)
            if (!cr::fequal (tr.flFraction, 1.0f) || tr.fStartSolid) {
               res |= 1;
            }
            else {
               res &= 2;
            }
         }
         shift = (path.number % 4) << 1;

         m_vistable[vis.number * m_paths.length () + path.number] &= ~(3 << shift);
         m_vistable[vis.number * m_paths.length () + path.number] |= res << shift;

         if (!(res & 2)) {
            ++crouchCount;
         }

         if (!(res & 1)) {
            ++standCount;
         }
      }
      m_paths[vis.number].vis.crouch = crouchCount;
      m_paths[vis.number].vis.stand = standCount;
   }
   m_needsVisRebuild = false;
   saveVisibility ();
}

bool BotGraph::isVisible (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return false;
   }

   uint8 res = m_vistable[srcIndex * m_paths.length () + destIndex];
   res >>= (destIndex % 4) << 1;

   return !((res & 3) == 3);
}

bool BotGraph::isDuckVisible (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return false;
   }

   uint8 res = m_vistable[srcIndex * m_paths.length () + destIndex];
   res >>= (destIndex % 4) << 1;

   return !((res & 2) == 2);
}

bool BotGraph::isStandVisible (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return false;
   }

   uint8 res = m_vistable[srcIndex * m_paths.length () + destIndex];
   res >>= (destIndex % 4) << 1;

   return !((res & 1) == 1);
}

void BotGraph::frame () {
   // this function executes frame of graph operation code.

   if (game.isNullEntity (m_editor)) {
      return; // this function is only valid with editor, and in graph enabled mode.
   }

   // keep the clipping mode enabled, or it can be turned off after new round has started
   if (graph.hasEditFlag (GraphEdit::Noclip) && util.isAlive (m_editor)) {
      m_editor->v.movetype = MOVETYPE_NOCLIP;
   }

   float nearestDistance = kInfiniteDistance;
   int nearestIndex = kInvalidNodeIndex;

   // check if it's time to add jump node
   if (m_jumpLearnNode) {
      if (!m_endJumpPoint) {
         if (m_editor->v.button & IN_JUMP) {
            add (9);
            
            m_timeJumpStarted = game.time ();
            m_endJumpPoint = true;
         }
         else {
            m_learnVelocity = m_editor->v.velocity;
            m_learnPosition = m_editor->v.origin;
         }
      }
      else if (((m_editor->v.flags & FL_ONGROUND) || m_editor->v.movetype == MOVETYPE_FLY) && m_timeJumpStarted + 0.1f < game.time () && m_endJumpPoint) {
         add (10);

         m_jumpLearnNode = false;
         m_endJumpPoint = false;
      }
   }

   // check if it's a auto-add-node mode enabled
   if (hasEditFlag (GraphEdit::Auto) && (m_editor->v.flags & (FL_ONGROUND | FL_PARTIALGROUND))) {
      // find the distance from the last used node
      float distance = (m_lastNode - m_editor->v.origin).lengthSq ();

      if (distance > cr::square (128.0f)) {
         // check that no other reachable nodes are nearby...
         for (const auto &path : m_paths) {
            if (isNodeReacheable (m_editor->v.origin, path.origin)) {
               distance = (path.origin - m_editor->v.origin).lengthSq ();

               if (distance < nearestDistance) {
                  nearestDistance = distance;
               }
            }
         }

         // make sure nearest node is far enough away...
         if (nearestDistance >= cr::square (128.0f)) {
            add (GraphAdd::Normal); // place a node here
         }
      }
   }
   m_facingAtIndex = getFacingIndex ();

   // reset the minimal distance changed before
   nearestDistance = kInfiniteDistance;

   // now iterate through all nodes in a map, and draw required ones
   for (auto &path : m_paths) {
      float distance = (path.origin - m_editor->v.origin).length ();

      // check if node is whitin a distance, and is visible
      if (distance < 512.0f && ((util.isVisible (path.origin, m_editor) && util.isInViewCone (path.origin, m_editor)) || !util.isAlive (m_editor) || distance < 128.0f)) {
         // check the distance
         if (distance < nearestDistance) {
            nearestIndex = path.number;
            nearestDistance = distance;
         }

         if (path.display + 0.8f < game.time ()) {
            float nodeHeight = 0.0f;

            // check the node height
            if (path.flags & NodeFlag::Crouch) {
               nodeHeight = 36.0f;
            }
            else {
               nodeHeight = 72.0f;
            }
            float nodeHalfHeight = nodeHeight * 0.5f;

            // all nodes are by default are green
            Color nodeColor = Color (-1, -1, -1);

            // colorize all other nodes
            if (path.flags & NodeFlag::Camp) {
               nodeColor = Color (0, 255, 255);
            }
            else if (path.flags & NodeFlag::Goal) {
               nodeColor = Color (128, 0, 255);
            }
            else if (path.flags & NodeFlag::Ladder) {
               nodeColor = Color (128, 64, 0);
            }
            else if (path.flags & NodeFlag::Rescue) {
               nodeColor = Color (255, 255, 255);
            }
            else {
               nodeColor = Color (0, 255, 0);
            } 

            // colorize additional flags
            Color nodeFlagColor = Color (-1, -1, -1);

            // check the colors
            if (path.flags & NodeFlag::Sniper) {
               nodeFlagColor = Color (130, 87, 0);
            }
            else if (path.flags & NodeFlag::NoHostage) {
               nodeFlagColor = Color (255, 255, 255);
            }
            else if (path.flags & NodeFlag::TerroristOnly) {
               nodeFlagColor = Color (255, 0, 0);
            }
            else if (path.flags & NodeFlag::CTOnly) {
               nodeFlagColor = Color (0, 0, 255);
            }
            int nodeWidth = 14;

            if (exists (m_facingAtIndex) && path.number == m_facingAtIndex) {
               nodeWidth *= 2;
            }

            // draw node without additional flags
            if (nodeFlagColor.red == -1) {
               game.drawLine (m_editor, path.origin - Vector (0, 0, nodeHalfHeight), path.origin + Vector (0, 0, nodeHalfHeight), nodeWidth + 1, 0, nodeColor, 250, 0, 10);
            }
            
            // draw node with flags
            else {
               game.drawLine (m_editor, path.origin - Vector (0, 0, nodeHalfHeight), path.origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75f), nodeWidth, 0, nodeColor, 250, 0, 10); // draw basic path
               game.drawLine (m_editor, path.origin - Vector (0, 0, nodeHalfHeight - nodeHeight * 0.75f), path.origin + Vector (0, 0, nodeHalfHeight), nodeWidth, 0, nodeFlagColor, 250, 0, 10); // draw additional path
            }
            path.display = game.time ();
         }
      }
   }

   if (nearestIndex == kInvalidNodeIndex) {
      return;
   }

   // draw arrow to a some importaint nodes
   if (exists (m_findWPIndex) || exists (m_cacheNodeIndex) || exists (m_facingAtIndex)) {
      // check for drawing code
      if (m_arrowDisplayTime + 0.5f < game.time ()) {

         // finding node - pink arrow
         if (m_findWPIndex != kInvalidNodeIndex) {
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_findWPIndex].origin, 10, 0, Color (128, 0, 128), 200, 0, 5, DrawLine::Arrow);
         }

         // cached node - yellow arrow
         if (m_cacheNodeIndex != kInvalidNodeIndex) {
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_cacheNodeIndex].origin, 10, 0, Color (255, 255, 0), 200, 0, 5, DrawLine::Arrow);
         }

         // node user facing at - white arrow
         if (m_facingAtIndex != kInvalidNodeIndex) {
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_facingAtIndex].origin, 10, 0, Color (255, 255, 255), 200, 0, 5, DrawLine::Arrow);
         }
         m_arrowDisplayTime = game.time ();
      }
   }

   // create path pointer for faster access
   auto &path = m_paths[nearestIndex];

   // draw a paths, camplines and danger directions for nearest node
   if (nearestDistance <= 56.0f && m_pathDisplayTime <= game.time ()) {
      m_pathDisplayTime = game.time () + 1.0f;

      // draw the camplines
      if (path.flags & NodeFlag::Camp) {
         float height = 36.0f;

         // check if it's a source
         if (path.flags & NodeFlag::Crouch) {
            height = 18.0f;
         }
         const auto &source = Vector (path.origin.x, path.origin.y, path.origin.z + height); // source
         const auto &start = path.origin + Vector (path.start.x, path.start.y, 0.0f).forward () * 500.0f; // camp start
         const auto &end = path.origin + Vector (path.end.x, path.end.y, 0.0f).forward () * 500.0f; // camp end
         
         // draw it now
         game.drawLine (m_editor, source, start, 10, 0, Color (255, 0, 0), 200, 0, 10);
         game.drawLine (m_editor, source, end, 10, 0, Color (255, 0, 0), 200, 0, 10);
      }

      // draw the connections
      for (const auto &link : path.links) {
         if (link.index == kInvalidNodeIndex) {
            continue;
         }
         // jump connection
         if (link.flags & PathFlag::Jump) {
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, Color (255, 0, 128), 200, 0, 10);
         }
         else if (isConnected (link.index, nearestIndex)) { // twoway connection
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, Color (255, 255, 0), 200, 0, 10);
         }
         else { // oneway connection
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, Color (250, 250, 250), 200, 0, 10);
         }
      }

      // now look for oneway incoming connections
      for (const auto &connected : m_paths) {
         if (isConnected (connected.number, path.number) && !isConnected (path.number, connected.number)) {
            game.drawLine (m_editor, path.origin, connected.origin, 5, 0, Color (0, 192, 96), 200, 0, 10);
         }
      }

      // draw the radius circle
      Vector origin = (path.flags & NodeFlag::Crouch) ? path.origin : path.origin - Vector (0.0f, 0.0f, 18.0f);
      Color radiusColor (0, 0, 255);

      // if radius is nonzero, draw a full circle
      if (path.radius > 0.0f) {
         float sqr = cr::sqrtf (path.radius * path.radius * 0.5f);

         game.drawLine (m_editor, origin + Vector (path.radius, 0.0f, 0.0f), origin + Vector (sqr, -sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
         game.drawLine (m_editor, origin + Vector (sqr, -sqr, 0.0f), origin + Vector (0.0f, -path.radius, 0.0f), 5, 0, radiusColor, 200, 0, 10);

         game.drawLine (m_editor, origin + Vector (0.0f, -path.radius, 0.0f), origin + Vector (-sqr, -sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
         game.drawLine (m_editor, origin + Vector (-sqr, -sqr, 0.0f), origin + Vector (-path.radius, 0.0f, 0.0f), 5, 0, radiusColor, 200, 0, 10);

         game.drawLine (m_editor, origin + Vector (-path.radius, 0.0f, 0.0f), origin + Vector (-sqr, sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
         game.drawLine (m_editor, origin + Vector (-sqr, sqr, 0.0f), origin + Vector (0.0f, path.radius, 0.0f), 5, 0, radiusColor, 200, 0, 10);

         game.drawLine (m_editor, origin + Vector (0.0f, path.radius, 0.0f), origin + Vector (sqr, sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
         game.drawLine (m_editor, origin + Vector (sqr, sqr, 0.0f), origin + Vector (path.radius, 0.0f, 0.0f), 5, 0, radiusColor, 200, 0, 10);
      }
      else {
         float sqr = cr::sqrtf (32.0f);

         game.drawLine (m_editor, origin + Vector (sqr, -sqr, 0.0f), origin + Vector (-sqr, sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
         game.drawLine (m_editor, origin + Vector (-sqr, -sqr, 0.0f), origin + Vector (sqr, sqr, 0.0f), 5, 0, radiusColor, 200, 0, 10);
      }

      // draw the danger directions
      if (!m_hasChanged) {
         int dangerIndex = getDangerIndex (game.getTeam (m_editor), nearestIndex, nearestIndex);

         if (exists (dangerIndex)) {
            game.drawLine (m_editor, path.origin, m_paths[dangerIndex].origin, 15, 0, Color (255, 0, 0), 200, 0, 10, DrawLine::Arrow); // draw a red arrow to this index's danger point
         }
      }

      auto getFlagsAsStr = [&] (int index) {
         const auto &path = m_paths[index];
         bool jumpPoint = false;

         // iterate through connections and find, if it's a jump path
         for (const auto &link : path.links) {

            // check if we got a valid connection
            if (link.index != kInvalidNodeIndex && (link.flags & PathFlag::Jump)) {
               jumpPoint = true;
            }
         }

         static String buffer;
         buffer.assignf ("%s%s%s%s%s%s%s%s%s%s%s%s%s", (path.flags == 0 && !jumpPoint) ? " (none)" : "", (path.flags & NodeFlag::Lift) ? " LIFT" : "", (path.flags & NodeFlag::Crouch) ? " CROUCH" : "", (path.flags & NodeFlag::Camp) ? " CAMP" : "", (path.flags & NodeFlag::TerroristOnly) ? " TERRORIST" : "", (path.flags & NodeFlag::CTOnly) ? " CT" : "", (path.flags & NodeFlag::Sniper) ? " SNIPER" : "", (path.flags & NodeFlag::Goal) ? " GOAL" : "", (path.flags & NodeFlag::Ladder) ? " LADDER" : "", (path.flags & NodeFlag::Rescue) ? " RESCUE" : "", (path.flags & NodeFlag::DoubleJump) ? " JUMPHELP" : "", (path.flags & NodeFlag::NoHostage) ? " NOHOSTAGE" : "", jumpPoint ? " JUMP" : "");

         // return the message buffer
         return buffer.chars ();
      };

      auto pathOriginStr = [&] (int index) {
         const auto &path = m_paths[index];

         static String buffer;
         buffer.assignf ("(%.1f, %.1f, %.1f)", path.origin.x, path.origin.y, path.origin.z);

         return buffer.chars ();
      };

      // display some information
      String graphMessage;

      // show the information about that point
      graphMessage.assignf ("\n\n\n\n    Graph Information:\n\n"
                              "      Node %d of %d, Radius: %.1f, Light: %.1f\n"
                              "      Flags: %s\n"
                              "      Origin: %s\n\n", nearestIndex, m_paths.length () - 1, path.radius, path.light, getFlagsAsStr (nearestIndex), pathOriginStr (nearestIndex));

      // if node is not changed display experience also
      if (!m_hasChanged) {
         int dangerIndexCT = getDangerIndex (Team::CT, nearestIndex, nearestIndex);
         int dangerIndexT = getDangerIndex (Team::Terrorist, nearestIndex, nearestIndex);

         graphMessage.appendf ("      Experience Info:\n"
                                       "      CT: %d / %d dmg\n"
                                       "      T: %d / %d dmg\n", dangerIndexCT, dangerIndexCT != kInvalidNodeIndex ? getDangerDamage (Team::CT, nearestIndex, dangerIndexCT) : 0, dangerIndexT, dangerIndexT != kInvalidNodeIndex ? getDangerDamage (Team::Terrorist, nearestIndex, dangerIndexT) : 0);
      }

      // check if we need to show the cached point index
      if (m_cacheNodeIndex != kInvalidNodeIndex) {
         graphMessage.appendf ("\n    Cached Node Information:\n\n"
                                       "      Node %d of %d, Radius: %.1f\n"
                                       "      Flags: %s\n"
                                       "      Origin: %s\n", m_cacheNodeIndex, m_paths.length () - 1, m_paths[m_cacheNodeIndex].radius, getFlagsAsStr (m_cacheNodeIndex), pathOriginStr (m_cacheNodeIndex));
      }

      // check if we need to show the facing point index
      if (m_facingAtIndex != kInvalidNodeIndex) {
         graphMessage.appendf ("\n    Facing Node Information:\n\n"
                                       "      Node %d of %d, Radius: %.1f\n"
                                       "      Flags: %s\n"
                                       "      Origin: %s\n", m_facingAtIndex, m_paths.length () - 1, m_paths[m_facingAtIndex].radius, getFlagsAsStr (m_facingAtIndex), pathOriginStr (m_facingAtIndex));
      }

      // draw entire message
      MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, m_editor)
         .writeByte (TE_TEXTMESSAGE)
         .writeByte (4) // channel
         .writeShort (MessageWriter::fs16 (0.0f, 13.0f)) // x
         .writeShort (MessageWriter::fs16 (0.0f, 13.0f)) // y
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
         .writeShort (MessageWriter::fu16 (1.1f, 8.0f)) // holdtime
         .writeString (graphMessage.chars ());
   }
}

bool BotGraph::isConnected (int index) {
   for (const auto &path : m_paths) {
      if (path.number == index) {
         continue;
      }

      for (const auto &test : path.links) {
         if (test.index == index) {
            return true;
         }
      }
   }
   return false;
}

bool BotGraph::checkNodes (bool teleportPlayer) {

   auto teleport = [&] (const Path &path) -> void {
      if (teleportPlayer) {
         engfuncs.pfnSetOrigin (m_editor, path.origin);
         setEditFlag (GraphEdit::On | GraphEdit::Noclip);
      }
   };

   int terrPoints = 0;
   int ctPoints = 0;
   int goalPoints = 0;
   int rescuePoints = 0;

   ctrl.setFromConsole (true);

   for (const auto &path : m_paths) {
      int connections = 0;

      if (path.number != static_cast <int> (m_paths.index (path))) {
         ctrl.msg ("Node %d path differs from index %d.", path.number, m_paths.index (path));
         break;
      }

      for (const auto &test : path.links) {
         if (test.index != kInvalidNodeIndex) {
            if (test.index > length ()) {
               ctrl.msg ("Node %d connected with invalid node %d.", path.number, test.index);
               return false;
            }
            ++connections;
            break;
         }
      }

      if (connections == 0) {
         if (!isConnected (path.number)) {
            ctrl.msg ("Node %d isn't connected with any other node.", path.number);
            return false;
         }
      }

      if (path.flags & NodeFlag::Camp) {
         if (path.end.empty ()) {
            ctrl.msg ("Node %d camp-endposition not set.", path.number);
            return false;
         }
      }
      else if (path.flags & NodeFlag::TerroristOnly) {
         ++terrPoints;
      }
      else if (path.flags & NodeFlag::CTOnly) {
         ++ctPoints;
      }
      else if (path.flags & NodeFlag::Goal) {
         ++goalPoints;
      }
      else if (path.flags & NodeFlag::Rescue) {
         ++rescuePoints;
      }

      for (const auto &test : path.links) {
         if (test.index != kInvalidNodeIndex) {
            if (!exists (test.index)) {
               ctrl.msg ("Node %d path index %d out of range.", path.number, test.index);
               teleport (path);

               return false;
            }
            else if (test.index == path.number) {
               ctrl.msg ("Node %d path index %d points to itself.", path.number, test.index);
               teleport (path);

               return false;
            }
         }
      }
   }

   if (game.mapIs (MapFlags::HostageRescue)) {
      if (rescuePoints == 0) {
         ctrl.msg ("You didn't set a rescue point.");
         return false;
      }
   }
   if (terrPoints == 0) {
      ctrl.msg ("You didn't set any terrorist important point.");
      return false;
   }
   else if (ctPoints == 0) {
      ctrl.msg ("You didn't set any CT important point.");
      return false;
   }
   else if (goalPoints == 0) {
      ctrl.msg ("You didn't set any goal point.");
      return false;
   }

   // perform DFS instead of floyd-warshall, this shit speedup this process in a bit
   PathWalk walk;
   Array <bool> visited;
   visited.resize (m_paths.length ());

   // first check incoming connectivity, initialize the "visited" table
   for (auto &visit : visited) {
      visit = false;
   }
   walk.push (0); // always check from node number 0

   while (!walk.empty ()) {
      // pop a node from the stack
      const int current = walk.first ();
      walk.shift ();

      visited[current] = true;

      for (const auto &link : m_paths[current].links) {
         int index = link.index;

         // skip this node as it's already visited
         if (exists (index) && !visited[index]) {
            visited[index] = true;
            walk.push (index);
         }
      }
   }

   for (const auto &path : m_paths) {
      if (!visited[path.number]) {
         ctrl.msg ("Path broken from node 0 to node %d.", path.number);
         teleport (path);

         return false;
      }
   }

   // then check outgoing connectivity
   Array <IntArray> outgoingPaths; // store incoming paths for speedup
   outgoingPaths.resize (m_paths.length ());

   for (const auto &path: m_paths) {
      outgoingPaths[path.number].resize (m_paths.length () + 1);

      for (const auto &link : path.links) {
         if (exists (link.index)) {
            outgoingPaths[link.index].push (path.number);
         }
      }
   }

   // initialize the "visited" table
   for (auto &visit : visited) {
      visit = false;
   }
   walk.clear ();
   walk.push (0); // always check from node number 0

   while (!walk.empty ()) {
      const int current = walk.first (); // pop a node from the stack
      walk.shift ();

      for (auto &outgoing : outgoingPaths[current]) {
         if (visited[outgoing]) {
            continue; // skip this node as it's already visited
         }
         visited[outgoing] = true;
         walk.push (outgoing);
      }
   }
   
   for (const auto &path : m_paths) {
      if (!visited[path.number]) {
         ctrl.msg ("Path broken from node %d to node 0.", path.number);
         teleport (path);

         return false;
      }
   }
   return true;
}

int BotGraph::getPathDist (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return 1;
   }
   return (m_matrix.data () + (srcIndex * length ()) + destIndex)->dist;
}

void BotGraph::setVisited (int index) {
   if (!exists (index)) {
      return;
   }
   if (!isVisited (index) && (m_paths[index].flags & NodeFlag::Goal)) {
      m_visitedGoals.push (index);
   }
}

void BotGraph::clearVisited () {
   m_visitedGoals.clear ();
}

bool BotGraph::isVisited (int index) {
   for (auto &visited : m_visitedGoals) {
      if (visited == index) {
         return true;
      }
   }
   return false;
}

void BotGraph::addBasic () {
   // this function creates basic node types on map

   // first of all, if map contains ladder points, create it
   game.searchEntities ("classname", "func_ladder", [&] (edict_t *ent) {
      Vector ladderLeft = ent->v.absmin;
      Vector ladderRight = ent->v.absmax;
      ladderLeft.z = ladderRight.z;

      TraceResult tr {};
      Vector up, down, front, back;

      const Vector &diff = ((ladderLeft - ladderRight) ^ Vector (0.0f, 0.0f, 0.0f)).normalize () * 15.0f;
      front = back = game.getEntityWorldOrigin (ent);

      front = front + diff; // front
      back = back - diff; // back

      up = down = front;
      down.z = ent->v.absmax.z;

      game.testHull (down, up, TraceIgnore::Monsters, point_hull, nullptr, &tr);

      if (engfuncs.pfnPointContents (up) == CONTENTS_SOLID || !cr::fequal (tr.flFraction, 1.0f)) {
         up = down = back;
         down.z = ent->v.absmax.z;
      }

      game.testHull (down, up - Vector (0.0f, 0.0f, 1000.0f), TraceIgnore::Monsters, point_hull, nullptr, &tr);
      up = tr.vecEndPos;

      Vector point = up + Vector (0.0f, 0.0f, 39.0f);
      m_isOnLadder = true;

      do {
         if (getNearestNoBuckets (point, 50.0f) == kInvalidNodeIndex) {
            add (3, point);
         }
         point.z += 160;
      } while (point.z < down.z - 40.0f);

      point = down + Vector (0.0f, 0.0f, 38.0f);

      if (getNearestNoBuckets (point, 50.0f) == kInvalidNodeIndex) {
         add (3, point);
      }
      m_isOnLadder = false;

      return EntitySearchResult::Continue;
   });

   auto autoCreateForEntity = [] (int type, const char *entity) {
      game.searchEntities ("classname", entity, [&] (edict_t *ent) {
         const Vector &pos = game.getEntityWorldOrigin (ent);

         if (graph.getNearestNoBuckets (pos, 50.0f) == kInvalidNodeIndex) {
            graph.add (type, pos);
         }
         return EntitySearchResult::Continue;
      });
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

void BotGraph::eraseFromDisk () {
   // this function removes graph file from the hard disk

   StringArray forErase;

   auto map = game.getMapName ();
   auto data = getDataDirectory ();

   bots.kickEveryone (true);

   // if we're delete graph, delete all corresponding to it files
   forErase.push (strings.format ("%s%s.pwf", data, map)); // graph itself
   forErase.push (strings.format ("%slearned/%s.exp", data, map)); // corresponding to practice
   forErase.push (strings.format ("%slearned/%s.vis", data, map)); // corresponding to vistable
   forErase.push (strings.format ("%slearned/%s.pmx", data, map)); // corresponding to matrix
   forErase.push (strings.format ("%sgraph/%s.graph", data, map)); // new format graph

   for (const auto &item : forErase) {
      if (File::exists (item)) {
         plat.removeFile (item.chars ());
         game.print ("File %s, has been deleted from the hard disk", item.chars ());
      }
      else {
         logger.error ("Unable to open %s", item.chars ());
      }
   }
   initGraph (); // reintialize points
   m_paths.clear ();
}

const char *BotGraph::getDataDirectory (bool isMemoryFile) {
   static String buffer;
   buffer.clear ();

   if (isMemoryFile) {
      buffer.assign ("addons/yapb/data/");
   }
   else {
      buffer.assignf ("%s/addons/yapb/data/", game.getModName ());
   }
   return buffer.chars ();
}

void BotGraph::setBombOrigin (bool reset, const Vector &pos) {
   // this function stores the bomb position as a vector

   if (reset) {
      m_bombOrigin = nullptr;
      bots.setBombPlanted (false);

      return;
   }

   if (!pos.empty ()) {
      m_bombOrigin = pos;
      return;
   }
   
   game.searchEntities ("classname", "grenade", [&] (edict_t *ent) {
      if (strcmp (ent->v.model.chars (9), "c4.mdl") == 0) {
         m_bombOrigin = game.getEntityWorldOrigin (ent);
         return EntitySearchResult::Break;
      }
      return EntitySearchResult::Continue;
   });
}

void BotGraph::startLearnJump () {
   m_jumpLearnNode = true;
}

void BotGraph::setSearchIndex (int index) {
   m_findWPIndex = index;

   if (exists (m_findWPIndex)) {
      ctrl.msg ("Showing direction to node %d.", m_findWPIndex);
   }
   else {
      m_findWPIndex = kInvalidNodeIndex;
   }
}

BotGraph::BotGraph () {
   plat.bzero (m_highestDamage, sizeof (m_highestDamage));

   m_endJumpPoint = false;
   m_needsVisRebuild = false;
   m_jumpLearnNode = false;
   m_hasChanged = false;
   m_timeJumpStarted = 0.0f;

   m_lastJumpNode = kInvalidNodeIndex;
   m_cacheNodeIndex = kInvalidNodeIndex;
   m_findWPIndex = kInvalidNodeIndex;
   m_facingAtIndex = kInvalidNodeIndex;
   m_loadAttempts = 0;
   m_isOnLadder = false;

   m_terrorPoints.clear ();
   m_ctPoints.clear ();
   m_goalPoints.clear ();
   m_campPoints.clear ();
   m_rescuePoints.clear ();
   m_sniperPoints.clear ();

   m_loadAttempts = 0;
   m_editFlags = 0;
   m_pathDisplayTime = 0.0f;
   m_arrowDisplayTime = 0.0f;
   m_autoPathDistance = 250.0f;

   m_matrix.clear ();
   m_practice.clear ();

   m_editor = nullptr;
}

void BotGraph::initBuckets () {
   for (int x = 0; x < kMaxBucketsInsidePos; ++x) {
      for (int y = 0; y < kMaxBucketsInsidePos; ++y) {
         for (int z = 0; z < kMaxBucketsInsidePos; ++z) {
            m_buckets[x][y][z].reserve (kMaxNodesInsideBucket);
            m_buckets[x][y][z].clear ();
         }
      }
   }
}

void BotGraph::addToBucket (const Vector &pos, int index) {
   const auto &bucket = locateBucket (pos);
   m_buckets[bucket.x][bucket.y][bucket.z].push (index);
}

void BotGraph::eraseFromBucket (const Vector &pos, int index) {
   const auto &bucket = locateBucket (pos);
   auto &data = m_buckets[bucket.x][bucket.y][bucket.z];

   for (size_t i = 0; i < data.length (); ++i) {
      if (data[i] == index) {
         data.erase (i, 1);
         break;
      }
   }
}

BotGraph::Bucket BotGraph::locateBucket (const Vector &pos) {
   constexpr auto size = static_cast <float> (kMaxNodes * 2);

   return {
       cr::abs (static_cast <int> ((pos.x + size) / kMaxBucketSize)),
       cr::abs (static_cast <int> ((pos.y + size) / kMaxBucketSize)),
       cr::abs (static_cast <int> ((pos.z + size) / kMaxBucketSize))
   };
}

const SmallArray <int32> &BotGraph::getNodesInBucket (const Vector &pos) {
   const auto &bucket = locateBucket (pos);
   return m_buckets[bucket.x][bucket.y][bucket.z];
}

void BotGraph::updateGlobalPractice () {
   // this function called after each end of the round to update knowledge about most dangerous nodes for each team.

   // no nodes, no experience used or nodes edited or being edited?
   if (m_paths.empty () || m_hasChanged) {
      return; // no action
   }
   bool adjustValues = false;
   auto practice = m_practice.data ();

   // get the most dangerous node for this position for both teams
   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      int bestIndex = kInvalidNodeIndex; // best index to store
      int maxDamage = 0;

      for (int i = 0; i < length (); ++i) {
         maxDamage = 0;
         bestIndex = kInvalidNodeIndex;

         for (int j = 0; j < length (); ++j) {
            if (i == j) {
               continue;
            }
            int actDamage = getDangerDamage (team, i, j);

            if (actDamage > maxDamage) {
               maxDamage = actDamage;
               bestIndex = j;
            }
         }

         if (maxDamage > kMaxPracticeDamageValue) {
            adjustValues = true;
         }
         (practice + (i * length ()) + i)->index[team] = static_cast <int16> (bestIndex);
      }
   }
   constexpr int HALF_DAMAGE_VALUE = static_cast <int> (kMaxPracticeDamageValue * 0.5);

   // adjust values if overflow is about to happen
   if (adjustValues) {
      for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
         for (int i = 0; i < length (); ++i) {
            for (int j = 0; j < length (); ++j) {
               if (i == j) {
                  continue;
               }
               (practice + (i * length ()) + j)->damage[team] = static_cast <int16> (cr::clamp (getDangerDamage (team, i, j) - HALF_DAMAGE_VALUE, 0, kMaxPracticeDamageValue));
            }
         }
      }
   }

   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      m_highestDamage[team] = cr::clamp (m_highestDamage [team] - HALF_DAMAGE_VALUE, 1, kMaxPracticeDamageValue);
   }
}

void BotGraph::unassignPath (int from, int to) {
   auto &link = m_paths[from].links[to];

   link.index = kInvalidNodeIndex;
   link.distance = 0;
   link.flags = 0;
   link.velocity = nullptr;

   setEditFlag (GraphEdit::On);
   m_hasChanged = true;
}

void BotGraph::convertFromPOD (Path &path, const PODPath &pod) {
   path = {};

   path.number = pod.number;
   path.flags = pod.flags;
   path.origin = pod.origin;
   path.start = Vector (pod.csx, pod.csy, 0.0f);
   path.end = Vector (pod.cex, pod.cey, 0.0f);

   if (yb_graph_fixcamp.bool_ ()) {
      convertCampDirection (path);
   }
   path.radius = pod.radius;
   path.light = 0.0f;
   path.display = 0.0f;

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      path.links[i].index = pod.index[i];
      path.links[i].distance = pod.distance[i];
      path.links[i].flags = pod.conflags[i];
      path.links[i].velocity = pod.velocity[i];
   }
   path.vis.stand = pod.vis.stand;
   path.vis.crouch = pod.vis.crouch;
}

void BotGraph::convertToPOD (const Path &path, PODPath &pod) {
   pod = {};

   pod.number = path.number;
   pod.flags = path.flags;
   pod.origin = path.origin;
   pod.radius = path.radius;
   pod.csx = path.start.x;
   pod.csy = path.start.y;
   pod.cex = path.end.x;
   pod.cey = path.end.y;

   for (int i = 0; i < kMaxNodeLinks; ++i) {
      pod.index[i] = path.links[i].index;
      pod.distance[i] = path.links[i].distance;
      pod.conflags[i] = path.links[i].flags;
      pod.velocity[i] = path.links[i].velocity;
   }
   pod.vis.stand = path.vis.stand;
   pod.vis.crouch = path.vis.crouch;
}

void BotGraph::convertCampDirection (Path &path) {
   // this function converts old vector based camp directions to angles, note that podbotmm graph
   // are already saved with angles, and converting this stuff may result strange look directions.

   if (m_paths.empty ()) {
      return;
   }
   const Vector &offset = path.origin + Vector (0.0f, 0.0f, (path.flags & NodeFlag::Crouch) ? 15.0f : 17.0f);

   path.start = (Vector (path.start.x, path.start.y, path.origin.z) - offset).angles ();
   path.end = (Vector (path.end.x, path.end.y, path.origin.z) - offset).angles ();

   path.start.x = -path.start.x;
   path.end.x = -path.end.x;

   path.start.clampAngles ();
   path.end.clampAngles ();
}

int BotGraph::getDangerIndex (int team, int start, int goal) {
   if (team != Team::Terrorist && team != Team::CT) {
      return kInvalidNodeIndex;
   }

   // realiablity check
   if (!exists (start) || !exists (goal)) {
      return kInvalidNodeIndex;
   }
   return (m_practice.data () + (start * length ()) + goal)->index[team];
}

int BotGraph::getDangerValue (int team, int start, int goal) {
   if (team != Team::Terrorist && team != Team::CT) {
      return 0;
   }

   // reliability check
   if (!exists (start) || !exists (goal)) {
      return 0;
   }
   return (m_practice.data () + (start * length ()) + goal)->value[team];
}

int BotGraph::getDangerDamage (int team, int start, int goal) {
   if (team != Team::Terrorist && team != Team::CT) {
      return 0;
   }

   // reliability check
   if (!exists (start) || !exists (goal)) {
      return 0;
   }
   return (m_practice.data () + (start * length ()) + goal)->damage[team];
}

void BotGraph::setDangerValue (int team, int start, int goal, int value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!exists (start) || !exists (goal)) {
      return;
   }
   (m_practice.data () + (start * length ()) + goal)->value[team] = static_cast <int16> (value);
}

void BotGraph::setDangerDamage (int team, int start, int goal, int value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!exists (start) || !exists (goal)) {
      return;
   }
   (m_practice.data () + (start * length ()) + goal)->damage[team] = static_cast <int16> (value);
}
