//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_graph_fixcamp ("yb_graph_fixcamp", "0", "Specifies whether bot should not 'fix' camp directions of camp waypoints when loading old PWF format.");
ConVar cv_graph_url ("yb_graph_url", product.download.chars (), "Specifies the URL from bots will be able to download graph in case of missing local one. Set to empty, if no downloads needed.", false, 0.0f, 0.0f);
ConVar cv_graph_auto_save_count ("yb_graph_auto_save_count", "15", "Every N graph nodes placed on map, the graph will be saved automatically (without checks).", true, 0.0f, kMaxNodes);
ConVar cv_graph_draw_distance ("yb_graph_draw_distance", "400", "Maximum distance to draw graph nodes from editor viewport.", true, 64.0f, 3072.0f);

void BotGraph::reset () {
   // this function initialize the graph structures..

   m_loadAttempts = 0;
   m_editFlags = 0;
   m_autoSaveCount = 0;

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
   m_graphAuthor.clear ();
   m_graphModified.clear ();
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
      int index {};
      int number {};
      float distance {};
      float angles {};

   public:
      Connection () {
         reset ();
      }

   public:
      void reset () {
         index = kInvalidNodeIndex;
         number = kInvalidNodeIndex;
         distance = kInfiniteDistance;
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
      cur.distance = static_cast <float> (link.distance);

      if (cur.index == kInvalidNodeIndex) {
         cur.distance = kInfiniteDistance;
      }

      if (cur.distance < top.distance) {
         top.distance = static_cast <float> (link.distance);
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

         if ((cur.distance + prev2.distance) * 1.1f / 2.0f < prev.distance) {
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
      while (inspect_p0 (i)) {}
   }

   // check pass 1
   if (exists (top.index) && exists (sorted[0].index) && exists (sorted[1].index)) {
      if ((sorted[1].angles - top.angles < 80.0f || 360.0f - (sorted[1].angles - top.angles) < 80.0f) && (!(m_paths[sorted[0].index].flags & NodeFlag::Ladder) || !(path.flags & NodeFlag::Ladder)) && !(path.links[sorted[0].number].flags & PathFlag::Jump)) {
         if ((sorted[1].distance + top.distance) * 1.1f / 2.0f < sorted[0].distance) {
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
         if (prev.distance < cur.distance * 1.1f) {

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
         else if (cur.distance < prev.distance * 1.1f) {
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
      while (inspect_p2 (i)) {}
   }

   // check pass 3
   if (exists (top.index) && exists (sorted[0].index)) {
      if ((top.angles - sorted[0].angles < 40.0f || (360.0f - top.angles - sorted[0].angles) < 40.0f) && (!(m_paths[sorted[0].index].flags & NodeFlag::Ladder) || !(path.flags & NodeFlag::Ladder)) && !(path.links[sorted[0].number].flags & PathFlag::Jump)) {
         if (top.distance * 1.1f < sorted[0].distance) {
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
         else if (sorted[0].distance * 1.1f < top.distance && !(path.links[top.number].flags & PathFlag::Jump)) {
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

int BotGraph::getBspSize () {
   MemFile file (strings.format ("maps/%s.bsp", game.getMapName ()));

   if (file) {
      return static_cast <int> (file.length ());
   }
   return 0;
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
         link.index = static_cast <int16_t> (pathIndex);
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

      path.links[slot].index = static_cast <int16_t> (pathIndex);
      path.links[slot].distance = cr::abs (static_cast <int> (distance));
   }
}

int BotGraph::getFarest (const Vector &origin, float maxDistance) {
   // find the farest node to that origin, and return the index to this node

   int index = kInvalidNodeIndex;
   maxDistance = cr::square (maxDistance);

   for (const auto &path : m_paths) {
      const float distance = path.origin.distanceSq (origin);

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
      const float distance = path.origin.distanceSq (origin);

      if (distance < minDistance) {
         index = path.number;
         minDistance = distance;
      }
   }
   return index;
}

int BotGraph::getEditorNearest () {
   if (!hasEditFlag (GraphEdit::On)) {
      return kInvalidNodeIndex;
   }
   return getNearestNoBuckets (m_editor->v.origin, 50.0f);
}

int BotGraph::getNearest (const Vector &origin, float minDistance, int flags) {
   // find the nearest node to that origin and return the index

   if (minDistance > 256.0f && !cr::fequal (minDistance, kInfiniteDistance)) {
      return getNearestNoBuckets (origin, minDistance, flags);
   }
   const auto &bucket = getNodesInBucket (origin);

   if (bucket.length () < kMaxNodeLinks) {
      return getNearestNoBuckets (origin, minDistance, flags);
   }

   int index = kInvalidNodeIndex;
   auto minDistanceSq = cr::square (minDistance);

   for (const auto &at : bucket) {
      if (flags != -1 && !(m_paths[at].flags & flags)) {
         continue; // if flag not -1 and node has no this flag, skip node
      }
      float distance = origin.distanceSq (m_paths[at].origin);

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

IntArray BotGraph::getNarestInRadius (float radius, const Vector &origin, int maxCount) {
   // returns all nodes within radius from position

   radius = cr::square (radius);

   IntArray result;
   const auto &bucket = getNodesInBucket (origin);

   if (bucket.length () < kMaxNodeLinks || radius > cr::square (256.0f)) {
      for (const auto &path : m_paths) {
         if (maxCount != -1 && static_cast <int> (result.length ()) > maxCount) {
            break;
         }

         if (origin.distanceSq (path.origin) < radius) {
            result.push (path.number);
         }
      }
      return result;
   }

   for (const auto &at : bucket) {
      if (maxCount != -1 && static_cast <int> (result.length ()) > maxCount) {
         break;
      }

      if (origin.distanceSq (m_paths[at].origin) < radius) {
         result.push (at);
      }
   }
   return result;
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
   case NodeAddFlag::Camp:
      index = getEditorNearest ();

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

   case NodeAddFlag::CampEnd:
      index = getEditorNearest ();

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

   case NodeAddFlag::JumpStart:
      index = getEditorNearest ();

      if (index != kInvalidNodeIndex && m_paths[index].number >= 0) {
         float distance = m_editor->v.origin.distance (m_paths[index].origin);

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

   case NodeAddFlag::JumpEnd:
      index = getEditorNearest ();

      if (index != kInvalidNodeIndex && m_paths[index].number >= 0) {
         float distance = m_editor->v.origin.distance (m_paths[index].origin);

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
      auto nearest = getEditorNearest ();

      // do not allow to place waypoints "inside" waypoints, make at leat 10 units range
      if (exists (nearest) && newOrigin.distanceSq (m_paths[nearest].origin) < cr::square (10.0f)) {
         ctrl.msg ("Can't add node. It's way to near to %d node. Please move some units anywhere.", nearest);
         return;
      }

      // need to remove limit?
      if (m_paths.length () >= kMaxNodes) {
         return;
      }
      m_paths.emplace ();

      index = length () - 1;
      path = &m_paths[index];

      path->number = index;
      path->flags = 0;

      // store the origin (location) of this node
      path->origin = newOrigin;
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

      // autosave nodes here and there
      if (cv_graph_auto_save_count.bool_ () && ++m_autoSaveCount >= cv_graph_auto_save_count.int_ ()) {
         if (saveGraphData ()) {
            ctrl.msg ("Nodes has been autosaved...");
         }
         else {
            ctrl.msg ("Can't autosave graph data...");
         }
         m_autoSaveCount = 0;
      }

      // store the last used node for the auto node code...
      m_lastNode = m_editor->v.origin;
   }

   if (type == NodeAddFlag::JumpStart) {
      m_lastJumpNode = index;
   }
   else if (type == NodeAddFlag::JumpEnd) {
      float distance = m_paths[m_lastJumpNode].origin.distance (m_editor->v.origin);
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
   case NodeAddFlag::TOnly:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::TerroristOnly;
      break;

   case NodeAddFlag::CTOnly:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::CTOnly;
      break;

   case NodeAddFlag::NoHostage:
      path->flags |= NodeFlag::NoHostage;
      break;

   case NodeAddFlag::Rescue:
      path->flags |= NodeFlag::Rescue;
      break;

   case NodeAddFlag::Camp:
      path->flags |= NodeFlag::Crossing;
      path->flags |= NodeFlag::Camp;

      path->start = m_editor->v.v_angle;
      break;

   case NodeAddFlag::Goal:
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
               float distance = newOrigin.distance (calc.origin);

               addPath (index, calc.number, distance);
               addPath (calc.number, index, distance);
            }
         }
         else {
            // check if the node is reachable from the new one
            if (isNodeReacheable (newOrigin, calc.origin) || isNodeReacheable (calc.origin, newOrigin)) {
               float distance = newOrigin.distance (calc.origin);

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
            addPath (index, destIndex, newOrigin.distance (m_paths[destIndex].origin));
         }

         // check if the new one is reachable from the node (other way)
         if (isNodeReacheable (m_paths[destIndex].origin, newOrigin)) {
            addPath (destIndex, index, newOrigin.distance (m_paths[destIndex].origin));
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
            addPath (index, calc.number, calc.origin.distance (newOrigin));
         }

         // check if the new one is reachable from the node (other way)
         if (isNodeReacheable (calc.origin, newOrigin)) {
            addPath (calc.number, index, calc.origin.distance (newOrigin));
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
   const int index = (target == kInvalidNodeIndex) ? getEditorNearest () : target;

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
   m_paths.remove (path);

   game.playSound (m_editor, "weapons/mine_activate.wav");
}

void BotGraph::toggleFlags (int toggleFlag) {
   // this function allow manually changing flags

   int index = getEditorNearest ();

   if (index != kInvalidNodeIndex) {
      if (m_paths[index].flags & toggleFlag) {
         m_paths[index].flags &= ~toggleFlag;
      }
      else {
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

   int node = exists (index) ? index : getEditorNearest ();

   if (node != kInvalidNodeIndex) {
      m_paths[node].radius = radius;

      // play "done" sound...
      game.playSound (m_editor, "common/wpn_hudon.wav");
      ctrl.msg ("Node %d has been set to radius %.2f.", node, radius);
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

   Twin <int32_t, float> result { kInvalidNodeIndex, 5.32f };
   auto nearestNode = getEditorNearest ();

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

   int nodeFrom = getEditorNearest ();

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

   float distance = m_paths[nodeFrom].origin.distance (m_paths[nodeTo].origin);

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

   int nodeFrom = getEditorNearest ();

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
   int node = exists (index) ? index : getEditorNearest ();

   if (node == kInvalidNodeIndex) {
      m_cacheNodeIndex = kInvalidNodeIndex;
      ctrl.msg ("Cached node cleared (nearby point not found in 50 units range).");

      return;
   }
   m_cacheNodeIndex = node;
   ctrl.msg ("Node %d has been put into memory.", m_cacheNodeIndex);
}

void BotGraph::setAutoPathDistance (const float distance) {
   m_autoPathDistance = distance;

   if (cr::fzero (distance)) {
      ctrl.msg ("Autopathing is now disabled.");
   }
   else {
      ctrl.msg ("Autopath distance is set to %.2f.", distance);
   }
}

void BotGraph::showStats () {
   int terrPoints = 0;
   int ctPoints = 0;
   int goalPoints = 0;
   int rescuePoints = 0;
   int campPoints = 0;
   int sniperPoints = 0;
   int noHostagePoints = 0;

   for (const auto &path : m_paths) {
      if (path.flags & NodeFlag::TerroristOnly) {
         ++terrPoints;
      }

      if (path.flags & NodeFlag::CTOnly) {
         ++ctPoints;
      }

      if (path.flags & NodeFlag::Goal) {
         ++goalPoints;
      }

      if (path.flags & NodeFlag::Rescue) {
         ++rescuePoints;
      }

      if (path.flags & NodeFlag::Camp) {
         ++campPoints;
      }

      if (path.flags & NodeFlag::Sniper) {
         ++sniperPoints;
      }

      if (path.flags & NodeFlag::NoHostage) {
         ++noHostagePoints;
      }
   }

   ctrl.msg ("Nodes: %d - T Points: %d", m_paths.length (), terrPoints);
   ctrl.msg ("CT Points: %d - Goal Points: %d", ctPoints, goalPoints);
   ctrl.msg ("Rescue Points: %d - Camp Points: %d", rescuePoints, campPoints);
   ctrl.msg ("Block Hostage Points: %d - Sniper Points: %d", noHostagePoints, sniperPoints);
}

void BotGraph::showFileInfo () {
   ctrl.msg ("header:");
   ctrl.msg ("  magic: %d", m_graphHeader.magic);
   ctrl.msg ("  version: %d", m_graphHeader.version);
   ctrl.msg ("  node_count: %d", m_graphHeader.length);
   ctrl.msg ("  compressed_size: %dkB", m_graphHeader.compressed / 1024);
   ctrl.msg ("  uncompressed_size: %dkB", m_graphHeader.uncompressed / 1024);
   ctrl.msg ("  options: %d", m_graphHeader.options); // display as string ?

   ctrl.msg ("");

   ctrl.msg ("extensions:");
   ctrl.msg ("  author: %s", m_extenHeader.author);
   ctrl.msg ("  modified_by: %s", m_extenHeader.modified);
   ctrl.msg ("  bsp_size: %d", m_extenHeader.mapSize);
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

   for (int32_t scanDistance = 32; scanDistance < 128; scanDistance += 16) {
      auto scan = static_cast <float> (scanDistance);
      start = path.origin;

      direction = Vector (0.0f, 0.0f, 0.0f).forward () * scan;
      direction = direction.angles ();

      path.radius = scan;

      for (int32_t circleRadius = 0; circleRadius < 360; circleRadius += 20) {
         const auto &forward = direction.forward ();

         auto radiusStart = start + forward * scan;
         auto radiusEnd = start + forward * scan;

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

         auto dropStart = start + forward * scan;
         auto dropEnd = dropStart - Vector (0.0f, 0.0f, scan + 60.0f);

         game.testHull (dropStart, dropEnd, TraceIgnore::Monsters, head_hull, nullptr, &tr);

         if (tr.flFraction >= 1.0f) {
            wayBlocked = true;
            path.radius -= 16.0f;

            break;
         }
         dropStart = start - forward * scan;
         dropEnd = dropStart - Vector (0.0f, 0.0f, scan + 60.0f);

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
         direction.y = cr::wrapAngle (direction.y + static_cast <float> (circleRadius));
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
   int count = length ();

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
   bool dataLoaded = loadStorage <uint8_t> ("vis", "Visibility", StorageOption::Vistable, StorageVersion::Vistable, m_vistable, nullptr, nullptr);

   // if loaded, do not recalculate visibility
   if (dataLoaded) {
      m_needsVisRebuild = false;
   }
}

void BotGraph::saveVisibility () {
   if (m_paths.empty () || m_hasChanged || m_needsVisRebuild) {
      return;
   }
   saveStorage <uint8_t> ("vis", "Visibility", StorageOption::Vistable, StorageVersion::Vistable, m_vistable, nullptr);
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
         (matrix + (i * count) + link.index)->dist = static_cast <int16_t> (link.distance);
         (matrix + (i * count) + link.index)->index = static_cast <int16_t> (link.index);
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
               (matrix + (i * count) + j)->dist = static_cast <int16_t> (distance);
               (matrix + (i * count) + j)->index = (matrix + (i * count) + k)->index;
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
   constexpr int32_t kNarrowPlacesMinGraphVersion = 2;

   // if version 2 or higher, narrow places already initialized and saved into file
   if (m_graphHeader.version >= kNarrowPlacesMinGraphVersion) {
      m_narrowChecked = true;
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
         auto directionCheck = [&] (const Vector &to) -> bool {
            game.testLine (path.origin + offset, to, TraceIgnore::None, nullptr, &tr);

            // check if we're hit worldspawn entity
            if (tr.pHit == worldspawn && tr.flFraction < 1.0f) {
               return true;
            }
            return false;
         };


         if (directionCheck (-forward * distance)) {
            accumWeight += 1;
         }

         if (directionCheck (right * distance)) {
            accumWeight += 1;
         }

         if (directionCheck (-right * distance)) {
            accumWeight += 1;
         }

         if (directionCheck (upward * distance)) {
            accumWeight += 1;
         }
      }
      path.flags &= ~NodeFlag::Narrow;

      if (accumWeight > 1) {
         path.flags |= NodeFlag::Narrow;
      }
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

   PODGraphHeader header {};
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
            reset ();
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
            fp.close ();

            // save new format in case loaded older one
            if (!m_paths.empty ()) {
               ctrl.msg ("Converting old PWF to new format Graph.");

               m_graphAuthor = header.author;

               // clean editor so graph will be saved with header's author
               auto editor = m_editor;
               m_editor = nullptr;

               auto result = saveGraphData ();
               m_editor = editor;

               return result;
            }
         }
      }
      else {
         return false;
      }
   }
   else {
      return false;
   }
   return false;
}

template <typename U> bool BotGraph::saveStorage (StringRef ext, StringRef name, StorageOption options, StorageVersion version, const SmallArray <U> &data, ExtenHeader *exten) {
   bool isGraph = !!(options & StorageOption::Graph);

   // do not allow to save graph with less than 8 nodes
   if (isGraph && length () < kMaxNodeLinks) {
      ctrl.msg ("Can't save graph data with less than %d nodes. Please add some more before saving.", kMaxNodeLinks);
      return false;
   }

   String filename;
   filename.assignf ("%s.%s", game.getMapName (), ext).lowercase ();

   if (data.empty ()) {
      logger.error ("Unable to save %s file. Empty data. (filename: '%s').", name, filename);
      return false;
   }
   else if (isGraph) {
      for (auto &path : m_paths) {
         path.display = 0.0f;
         path.light = illum.getLightLevel (path.origin);
      }
   }

   // open the file
   File file (strings.format ("%s%s/%s", getDataDirectory (false), isGraph ? "graph" : "train", filename), "wb");

   // no open no fun
   if (!file) {
      logger.error ("Unable to open %s file for writing (filename: '%s').", name, filename);
      return false;
   }
   auto rawLength = data.length () * sizeof (U);
   SmallArray <uint8_t> compressed (rawLength + sizeof (uint8_t) * ULZ::Excess);

   // try to compress
   auto compressedLength = static_cast <size_t> (ulz.compress (reinterpret_cast <uint8_t *> (data.data ()), static_cast <int32_t> (rawLength), reinterpret_cast <uint8_t *> (compressed.data ())));

   if (compressedLength > 0) {
      StorageHeader hdr {};

      hdr.magic = kStorageMagic;
      hdr.version = version;
      hdr.options = options;
      hdr.length = length ();
      hdr.compressed = static_cast <int32_t> (compressedLength);
      hdr.uncompressed = static_cast <int32_t> (rawLength);

      file.write (&hdr, sizeof (StorageHeader));
      file.write (compressed.data (), sizeof (uint8_t), compressedLength);

      // add extension
      if ((options & StorageOption::Exten) && exten != nullptr) {
         file.write (exten, sizeof (ExtenHeader));
      }
      ctrl.msg ("Successfully saved Bots %s data.", name);
   }
   else {
      logger.error ("Unable to compress %s data (filename: '%s').", name, filename);
      return false;
   }
   return true;
}

template <typename U> bool BotGraph::loadStorage (StringRef ext, StringRef name, StorageOption options, StorageVersion version, SmallArray <U> &data, ExtenHeader *exten, int32_t *outOptions) {
   String filename;
   filename.assignf ("%s.%s", game.getMapName (), ext).lowercase ();

   // graphs can be downloaded...
   bool isGraph = !!(options & StorageOption::Graph);
   MemFile file (strings.format ("%s%s/%s", getDataDirectory (true), isGraph ? "graph" : "train", filename)); // open the file

   data.clear ();
   data.shrink ();

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

   // if graph & attempted to load multiple times, bail out, we're failed
   if (isGraph && ++m_loadAttempts > 2) {
      m_loadAttempts = 0;
      return raiseLoadingError (isGraph, file, "Unable to load %s (filename: '%s'). Download process has failed as well. No nodes has been found.", name, filename);
   }

   // downloader for graph
   auto download = [&] () -> bool {
      if (!graph.canDownload ()) {
         return false;
      }
      auto downloadAddress = cv_graph_url.str ();

      auto toDownload = strings.format ("%sgraph/%s", getDataDirectory (false), filename);
      auto fromDownload = strings.format ("http://%s/graph/%s", downloadAddress, filename);

      // try to download
      if (http.downloadFile (fromDownload, toDownload)) {
         ctrl.msg ("%s file '%s' successfully downloaded. Processing...", name, filename);
         return true;
      }
      else {
         ctrl.msg ("Can't download '%s' from '%s' to '%s'... (%d).", filename, fromDownload, toDownload, http.getLastStatusCode ());
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
      return raiseLoadingError (isGraph, file, "Unable to open %s file for reading (filename: '%s').", name, filename);
   }

   // read the header
   StorageHeader hdr {};
   file.read (&hdr, sizeof (StorageHeader));

   // check the magic
   if (hdr.magic != kStorageMagic && hdr.magic != kStorageMagicUB) {
      if (tryReload ()) {
         return true;
      }
      return raiseLoadingError (isGraph, file, "Unable to read magic of %s (filename: '%s').", name, filename);
   }

   // check the path-numbers
   if (!isGraph && hdr.length != length ()) {
      return raiseLoadingError (isGraph, file, "Damaged %s (filename: '%s'). Mismatch number of nodes (got: '%d', need: '%d').", name, filename, hdr.length, m_paths.length ());
   }

   // check the count
   if (hdr.length == 0 || hdr.length > kMaxNodes || hdr.length < kMaxNodeLinks) {
      if (tryReload ()) {
         return true;
      }
      return raiseLoadingError (isGraph, file, "Damaged %s (filename: '%s'). Paths length is overflowed (got: '%d').", name, filename, hdr.length);
   }

   // check the version
   if (hdr.version > version && isGraph) {
      ctrl.msg ("Graph version mismatch %s (filename: '%s'). Version number differs (got: '%d', need: '%d') Please, upgrade %s.", name, filename, hdr.version, version, product.name);
   }
   else if (hdr.version > version && !isGraph) {
      return raiseLoadingError (isGraph, file, "Damaged %s (filename: '%s'). Version number differs (got: '%d', need: '%d').", name, filename, hdr.version, version);
   }


   // temporary solution to kill version 1 vistables, which has a bugs
   if ((options & StorageOption::Vistable) && hdr.version == 1) {
      auto vistablePath = strings.format ("%strain/%s.vis", getDataDirectory (), game.getMapName ());

      if (File::exists (vistablePath)) {
         plat.removeFile (vistablePath);
      }
      return raiseLoadingError (isGraph, file, "Bugged vistable %s (filename: '%s'). Version 1 has a bugs, vistable will be recreated.", name, filename);
   }

   // save graph version
   if (isGraph) {
      memcpy (&m_graphHeader, &hdr, sizeof (StorageHeader));
   }

   // check the storage type
   if ((hdr.options & options) != options) {
      return raiseLoadingError (isGraph, file, "Incorrect storage format for %s (filename: '%s').", name, filename);
   }
   auto compressedSize = static_cast <size_t> (hdr.compressed);
   auto numberNodes = static_cast <size_t> (hdr.length);

   SmallArray <uint8_t> compressed (compressedSize + sizeof (uint8_t) * ULZ::Excess);

   // graph is not resized upon load
   if (isGraph) {
      resizeData (numberNodes);
   }

   // read compressed data
   if (file.read (compressed.data (), sizeof (uint8_t), compressedSize) == compressedSize) {

      // try to uncompress
      if (ulz.uncompress (compressed.data (), hdr.compressed, reinterpret_cast <uint8_t *> (data.data ()), hdr.uncompressed) == ULZ::UncompressFailure) {
         return raiseLoadingError (isGraph, file, "Unable to decompress ULZ data for %s (filename: '%s').", name, filename);
      }
      else {

         if (outOptions) {
            outOptions = &hdr.options;
         }

         // author of graph.. save
         if ((hdr.options & StorageOption::Exten) && exten != nullptr) {
            auto extenSize = sizeof (ExtenHeader);
            auto actuallyRead = file.read (exten, extenSize) * extenSize;

            if (isGraph) {
               strings.copy (m_extenHeader.author, exten->author, cr::bufsize (exten->author));

               if (extenSize <= actuallyRead) {
                  // write modified by, only if the name is different
                  if (!strings.isEmpty (m_extenHeader.author) && strncmp (m_extenHeader.author, exten->modified, cr::bufsize (m_extenHeader.author)) != 0) {
                     strings.copy (m_extenHeader.modified, exten->modified, cr::bufsize (exten->modified));
                  }
               }
               else {
                  strings.copy (m_extenHeader.modified, "(none)", cr::bufsize (exten->modified));
               }
               m_extenHeader.mapSize = exten->mapSize;
            }
         }
         ctrl.msg ("Successfully loaded Bots %s data v%d (%d/%.2fMB).", name, hdr.version, m_paths.length (), static_cast <float> (data.capacity () * sizeof (U)) / 1024.0f / 1024.0f);
         file.close ();

         return true;
      }
   }
   else {
      return raiseLoadingError (isGraph, file, "Unable to read ULZ data for %s (filename: '%s').", name, filename);
   }
   return false;
}

bool BotGraph::loadGraphData () {
   ExtenHeader exten {};
   int32_t outOptions = 0;

   m_graphHeader = {};
   m_extenHeader = {};

   // check if loaded
   bool dataLoaded = loadStorage <Path> ("graph", "Graph", StorageOption::Graph, StorageVersion::Graph, m_paths, &exten, &outOptions);

   if (dataLoaded) {
      reset ();
      initBuckets ();

      // add data to buckets
      for (const auto &path : m_paths) {
         addToBucket (path.origin, path.number);
      }

      if ((outOptions & StorageOption::Official) || strncmp (exten.author, "official", 8) == 0 || strlen (exten.author) < 2) {
         m_graphAuthor.assign (product.name);
      }
      else {
         m_graphAuthor.assign (exten.author);
      }
      StringRef modified = exten.modified;

      if (!modified.empty () && !modified.contains ("(none)")) {
         m_graphModified.assign (exten.modified);
      }

      initNodesTypes ();
      loadPathMatrix ();
      loadVisibility ();
      loadPractice ();

      if (exten.mapSize > 0) {
         int mapSize = getBspSize ();

         if (mapSize != exten.mapSize) {
            ctrl.msg ("Warning: Graph data is probably not for this map. Please check bots behaviour.");
         }
      }
      cv_debug_goal.set (kInvalidNodeIndex);

      return true;
   }
   return false;
}

bool BotGraph::canDownload () {
   return !strings.isEmpty (cv_graph_url.str ());
}

bool BotGraph::saveGraphData () {
   auto options = StorageOption::Graph | StorageOption::Exten;
   String editorName;

   if (game.isNullEntity (m_editor) && !m_graphAuthor.empty ()) {
      editorName = m_graphAuthor;

      if (!game.isDedicated ()) {
         options |= StorageOption::Recovered;
      }
   }
   else if (!game.isNullEntity (m_editor)) {
      editorName = m_editor->v.netname.chars ();
   }
   else {
      editorName = product.name;
   }

   // mark as official
   if (editorName.startsWith (product.name)) {
      options |= StorageOption::Official;
   }
   ExtenHeader exten {};

   // only modify the author if no author currently assigned to graph file
   if (m_graphAuthor.empty () || strings.isEmpty (m_extenHeader.author)) {
      strings.copy (exten.author, editorName.chars (), cr::bufsize (exten.author));
   }
   else {
      strings.copy (exten.author, m_extenHeader.author, cr::bufsize (exten.author));
   }

   // only update modified by, if name differs
   if (m_graphAuthor != editorName && !strings.isEmpty (m_extenHeader.author)) {
      strings.copy (exten.modified, editorName.chars (), cr::bufsize (exten.author));
   }
   exten.mapSize = getBspSize ();

   // ensure narrow places saved into file
   m_narrowChecked = false;
   initNarrowPlaces ();

   return saveStorage <Path> ("graph", "Graph", static_cast <StorageOption> (options), StorageVersion::Graph, m_paths, &exten);
}

void BotGraph::saveOldFormat () {
   PODGraphHeader header {};

   strings.copy (header.header, kPodbotMagic, sizeof (kPodbotMagic));
   strings.copy (header.author, m_editor->v.netname.chars (), cr::bufsize (header.author));
   strings.copy (header.mapName, game.getMapName (), cr::bufsize (header.mapName));

   header.mapName[31] = 0;
   header.fileVersion = StorageVersion::Podbot;
   header.pointNumber = length ();

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
   buffer.assignf ("%s/pwf/%s.pwf", getDataDirectory (isMemoryFile), game.getMapName ());

   if (File::exists (buffer)) {
      return buffer.chars ();
   }
   return strings.format ("%s/pwf/%s.pwf", getDataDirectory (isMemoryFile), game.getMapName ());
}

float BotGraph::calculateTravelTime (float maxSpeed, const Vector &src, const Vector &origin) {
   // this function returns 2D traveltime to a position

   return origin.distance2d (src) / maxSpeed;
}

bool BotGraph::isNodeReacheable (const Vector &src, const Vector &destination) {
   TraceResult tr {};

   float distance = destination.distance (src);

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
   if (tr.pHit && (tr.flFraction >= 1.0f || strncmp ("func_door", tr.pHit->v.classname.chars (), 9) == 0)) {
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
         if (tr.flFraction >= 1.0f) {
            return false; // can't reach this one
         }
      }

      // check if distance to ground drops more than step height at points between source and destination...
      Vector direction = (destination - src).normalize (); // 1 unit long
      Vector check = src, down = src;

      down.z = down.z - 1000.0f; // straight down 1000 units

      game.testLine (check, down, TraceIgnore::Monsters, m_editor, &tr);

      float lastHeight = tr.flFraction * 1000.0f; // height from ground
      distance = destination.distance (check); // distance from goal

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
         distance = destination.distance (check); // distance from goal
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
   uint8_t res, shift;

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
      uint16_t standCount = 0, crouchCount = 0;

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
         if (!cr::fequal (tr.flFraction, 1.0f) || tr.fStartSolid) {
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
         shift = static_cast <uint8_t> ((path.number % 4) << 1);

         m_vistable[vis.number * length () + path.number] &= ~static_cast <uint8_t> (3 << shift);
         m_vistable[vis.number * length () + path.number] |= res << shift;

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

   uint8_t res = m_vistable[srcIndex * length () + destIndex];
   res >>= (destIndex % 4) << 1;

   return !((res & 3) == 3);
}

bool BotGraph::isDuckVisible (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return false;
   }

   uint8_t res = m_vistable[srcIndex * length () + destIndex];
   res >>= (destIndex % 4) << 1;

   return !((res & 2) == 2);
}

bool BotGraph::isStandVisible (int srcIndex, int destIndex) {
   if (!exists (srcIndex) || !exists (destIndex)) {
      return false;
   }

   uint8_t res = m_vistable[srcIndex * length () + destIndex];
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
            add (NodeAddFlag::JumpStart);

            m_timeJumpStarted = game.time ();
            m_endJumpPoint = true;
         }
         else {
            m_learnVelocity = m_editor->v.velocity;
            m_learnPosition = m_editor->v.origin;
         }
      }
      else if (((m_editor->v.flags & FL_ONGROUND) || m_editor->v.movetype == MOVETYPE_FLY) && m_timeJumpStarted + 0.1f < game.time () && m_endJumpPoint) {
         add (NodeAddFlag::JumpEnd);

         m_jumpLearnNode = false;
         m_endJumpPoint = false;
      }
   }

   // check if it's a auto-add-node mode enabled
   if (hasEditFlag (GraphEdit::Auto) && (m_editor->v.flags & (FL_ONGROUND | FL_PARTIALGROUND))) {
      // find the distance from the last used node
      float distance = m_lastNode.distanceSq (m_editor->v.origin);

      if (distance > cr::square (128.0f)) {
         // check that no other reachable nodes are nearby...
         for (const auto &path : m_paths) {
            if (isNodeReacheable (m_editor->v.origin, path.origin)) {
               distance = path.origin.distanceSq (m_editor->v.origin);

               if (distance < nearestDistance) {
                  nearestDistance = distance;
               }
            }
         }

         // make sure nearest node is far enough away...
         if (nearestDistance >= cr::square (128.0f)) {
            add (NodeAddFlag::Normal); // place a node here
         }
      }
   }
   m_facingAtIndex = getFacingIndex ();

   // reset the minimal distance changed before
   nearestDistance = kInfiniteDistance;

   // now iterate through all nodes in a map, and draw required ones
   for (auto &path : m_paths) {
      float distance = path.origin.distance (m_editor->v.origin);

      // check if node is whitin a distance, and is visible
      if (distance < cv_graph_draw_distance.float_ () && ((util.isVisible (path.origin, m_editor) && util.isInViewCone (path.origin, m_editor)) || !util.isAlive (m_editor) || distance < 64.0f)) {
         // check the distance
         if (distance < nearestDistance) {
            nearestIndex = path.number;
            nearestDistance = distance;
         }

         if (path.display + 1.0f < game.time ()) {
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
            Color nodeColor { -1, -1, -1 };

            // colorize all other nodes
            if (path.flags & NodeFlag::Goal) {
               nodeColor = { 128, 0, 255 };
            }
            else if (path.flags & NodeFlag::Ladder) {
               nodeColor = { 128, 64, 0 };
            }
            else if (path.flags & NodeFlag::Rescue) {
               nodeColor = { 255, 255, 255 };
            }
            else if (path.flags & NodeFlag::Camp) {
               if (path.flags & NodeFlag::TerroristOnly) {
                  nodeColor = { 255, 160, 160 };
               }
               else if (path.flags & NodeFlag::CTOnly) {
                  nodeColor = { 160, 160, 255 };
               }
               else {
                  nodeColor = { 0, 255, 255 };
               }
            }
            else if (path.flags & NodeFlag::TerroristOnly) {
               nodeColor = { 255, 0, 0 };
            }
            else if (path.flags & NodeFlag::CTOnly) {
               nodeColor = { 0, 0, 255 };
            }
            else {
               nodeColor = { 0, 255, 0 };
            }

            // colorize additional flags
            Color nodeFlagColor { -1, -1, -1 };

            // check the colors
            if (path.flags & NodeFlag::Sniper) {
               nodeFlagColor = { 130, 87, 0 };
            }
            else if (path.flags & NodeFlag::NoHostage) {
               nodeFlagColor = { 255, 255, 255 };
            }
            else if (path.flags & NodeFlag::Lift) {
               nodeFlagColor = { 255, 0, 255 };
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
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_findWPIndex].origin, 10, 0, { 128, 0, 128 }, 200, 0, 5, DrawLine::Arrow);
         }

         // cached node - yellow arrow
         if (m_cacheNodeIndex != kInvalidNodeIndex) {
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_cacheNodeIndex].origin, 10, 0, { 255, 255, 0 }, 200, 0, 5, DrawLine::Arrow);
         }

         // node user facing at - white arrow
         if (m_facingAtIndex != kInvalidNodeIndex) {
            game.drawLine (m_editor, m_editor->v.origin, m_paths[m_facingAtIndex].origin, 10, 0, { 255, 255, 255 }, 200, 0, 5, DrawLine::Arrow);
         }
         m_arrowDisplayTime = game.time ();
      }
   }

   // draw a paths, camplines and danger directions for nearest node
   if (nearestDistance <= 56.0f && m_pathDisplayTime < game.time ()) {
      m_pathDisplayTime = game.time () + 0.96f;

      // create path pointer for faster access
      auto &path = m_paths[nearestIndex];

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
         game.drawLine (m_editor, source, start, 10, 0, { 255, 0, 0 }, 200, 0, 10);
         game.drawLine (m_editor, source, end, 10, 0, { 255, 0, 0 }, 200, 0, 10);
      }

      // draw the connections
      for (const auto &link : path.links) {
         if (link.index == kInvalidNodeIndex) {
            continue;
         }
         // jump connection
         if (link.flags & PathFlag::Jump) {
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, { 255, 0, 128 }, 200, 0, 10);
         }
         else if (isConnected (link.index, nearestIndex)) { // twoway connection
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, { 255, 255, 0 }, 200, 0, 10);
         }
         else { // oneway connection
            game.drawLine (m_editor, path.origin, m_paths[link.index].origin, 5, 0, { 255, 255, 255 }, 200, 0, 10);
         }
      }

      // now look for oneway incoming connections
      for (const auto &connected : m_paths) {
         if (isConnected (connected.number, path.number) && !isConnected (path.number, connected.number)) {
            game.drawLine (m_editor, path.origin, connected.origin, 5, 0, { 0, 192, 96 }, 200, 0, 10);
         }
      }

      // draw the radius circle
      Vector origin = (path.flags & NodeFlag::Crouch) ? path.origin : path.origin - Vector (0.0f, 0.0f, 18.0f);
      Color radiusColor { 0, 0, 255 };

      // if radius is nonzero, draw a full circle
      if (path.radius > 0.0f) {
         float sqr = cr::sqrtf (cr::square (path.radius) * 0.5f);

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
         int dangerIndexT = getDangerIndex (Team::Terrorist, nearestIndex, nearestIndex);
         int dangerIndexCT = getDangerIndex (Team::CT, nearestIndex, nearestIndex);

         if (exists (dangerIndexT)) {
            game.drawLine (m_editor, path.origin, m_paths[dangerIndexT].origin, 15, 0, { 255, 0, 0 }, 200, 0, 10, DrawLine::Arrow); // draw a red arrow to this index's danger point
         }
         if (exists (dangerIndexCT)) {
            game.drawLine (m_editor, path.origin, m_paths[dangerIndexCT].origin, 15, 0, { 0, 0, 255 }, 200, 0, 10, DrawLine::Arrow); // draw a blue arrow to this index's danger point
         }
      }
      static int channel = 0;

      auto sendHudMessage = [] (Color color, float x, float y, edict_t *to, StringRef text) {
         MessageWriter (MSG_ONE_UNRELIABLE, SVC_TEMPENTITY, nullptr, to)
            .writeByte (TE_TEXTMESSAGE)
            .writeByte (channel++ & 0xff) // channel
            .writeShort (MessageWriter::fs16 (x, 13.0f)) // x
            .writeShort (MessageWriter::fs16 (y, 13.0f)) // y
            .writeByte (0) // effect
            .writeByte (color.red) // r1
            .writeByte (color.green) // g1
            .writeByte (color.blue) // b1
            .writeByte (1) // a1
            .writeByte (color.red) // r2
            .writeByte (color.green) // g2
            .writeByte (color.blue) // b2
            .writeByte (1) // a2
            .writeShort (0) // fadeintime
            .writeShort (0) // fadeouttime
            .writeShort (MessageWriter::fu16 (1.0f, 8.0f)) // holdtime
            .writeString (text.chars ());

         if (channel > 3) {
            channel = 0;
         }
      };

      // very helpful stuff..
      auto getNodeData = [&] (StringRef type, int node) -> String {
         String message, flags;

         const auto &p = m_paths[node];
         bool jumpPoint = false;

         // iterate through connections and find, if it's a jump path
         for (const auto &link : p.links) {

            // check if we got a valid connection
            if (link.index != kInvalidNodeIndex && (link.flags & PathFlag::Jump)) {
               jumpPoint = true;
            }
         }
         flags.assignf ("%s%s%s%s%s%s%s%s%s%s%s%s",
                        (p.flags & NodeFlag::Lift) ? " LIFT" : "",
                        (p.flags & NodeFlag::Crouch) ? " CROUCH" : "",
                        (p.flags & NodeFlag::Camp) ? " CAMP" : "",
                        (p.flags & NodeFlag::TerroristOnly) ? " TERRORIST" : "",
                        (p.flags & NodeFlag::CTOnly) ? " CT" : "",
                        (p.flags & NodeFlag::Sniper) ? " SNIPER" : "",
                        (p.flags & NodeFlag::Goal) ? " GOAL" : "",
                        (p.flags & NodeFlag::Ladder) ? " LADDER" : "",
                        (p.flags & NodeFlag::Rescue) ? " RESCUE" : "",
                        (p.flags & NodeFlag::DoubleJump) ? " JUMPHELP" : "",
                        (p.flags & NodeFlag::NoHostage) ? " NOHOSTAGE" : "", jumpPoint ? " JUMP" : "");

         if (flags.empty ()) {
            flags.assign ("(none)");
         }

         // show the information about that point
         message.assignf ("      %s node:\n"
                          "       Node %d of %d, Radius: %.1f, Light: %.1f\n"
                          "       Flags: %s\n"
                          "       Origin: (%.1f, %.1f, %.1f)\n", type, node, m_paths.length () - 1, p.radius, p.light, flags, p.origin.x, p.origin.y, p.origin.z);
         return message;
      };

      // display some information
      sendHudMessage ({ 255, 255, 255 }, 0.0f, 0.025f, m_editor, getNodeData ("Current", nearestIndex));

      // check if we need to show the cached point index
      if (m_cacheNodeIndex != kInvalidNodeIndex) {
         sendHudMessage ({ 255, 255, 255 }, 0.28f, 0.16f, m_editor, getNodeData ("Cached", m_cacheNodeIndex));
      }

      // check if we need to show the facing point index
      if (m_facingAtIndex != kInvalidNodeIndex) {
         sendHudMessage ({ 255, 255, 255 }, 0.28f, 0.025f, m_editor, getNodeData ("Facing", m_facingAtIndex));
      }
      String timeMessage = strings.format ("      Map: %s, Time: %s\n", game.getMapName (), util.getCurrentDateTime ());

      // if node is not changed display experience also
      if (!m_hasChanged) {
         int dangerIndexCT = getDangerIndex (Team::CT, nearestIndex, nearestIndex);
         int dangerIndexT = getDangerIndex (Team::Terrorist, nearestIndex, nearestIndex);

         String practice;
         practice.assignf ("      Node practice data (index / damage):\n"
                           "       CT: %d / %d\n"
                           "       T:  %d / %d\n\n", dangerIndexCT, dangerIndexCT != kInvalidNodeIndex ? getDangerDamage (Team::CT, nearestIndex, dangerIndexCT) : 0, dangerIndexT, dangerIndexT != kInvalidNodeIndex ? getDangerDamage (Team::Terrorist, nearestIndex, dangerIndexT) : 0);

         sendHudMessage ({ 255, 255, 255 }, 0.0f, 0.16f, m_editor, practice + timeMessage);
      }
      else {
         sendHudMessage ({ 255, 255, 255 }, 0.0f, 0.16f, m_editor, timeMessage);
      }
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
   auto length = cr::min (static_cast <size_t>  (kMaxNodes), m_paths.length ());

   // ensure valid capacity
   assert (length > 8 && length < static_cast <size_t>  (kMaxNodes));

   PathWalk walk;
   walk.init (length);

   Array <bool> visited;
   visited.resize (length);

   // first check incoming connectivity, initialize the "visited" table
   for (auto &visit : visited) {
      visit = false;
   }
   walk.add (0); // always check from node number 0

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
            walk.add (index);
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
   outgoingPaths.resize (length);

   for (const auto &path : m_paths) {
      outgoingPaths[path.number].resize (length + 1);

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
   walk.add (0); // always check from node number 0

   while (!walk.empty ()) {
      const int current = walk.first (); // pop a node from the stack
      walk.shift ();

      for (auto &outgoing : outgoingPaths[current]) {
         if (visited[outgoing]) {
            continue; // skip this node as it's already visited
         }
         visited[outgoing] = true;
         walk.add (outgoing);
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
      front = back = game.getEntityOrigin (ent);

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
            add (NodeAddFlag::NoHostage, point);
         }
         point.z += 160;
      } while (point.z < down.z - 40.0f);

      point = down + Vector (0.0f, 0.0f, 38.0f);

      if (getNearestNoBuckets (point, 50.0f) == kInvalidNodeIndex) {
         add (NodeAddFlag::NoHostage, point);
      }
      m_isOnLadder = false;

      return EntitySearchResult::Continue;
   });

   auto autoCreateForEntity = [] (int type, const char *entity) {
      game.searchEntities ("classname", entity, [&] (edict_t *ent) {
         const Vector &pos = game.getEntityOrigin (ent);

         if (graph.getNearestNoBuckets (pos, 50.0f) == kInvalidNodeIndex) {
            graph.add (type, pos);
         }
         return EntitySearchResult::Continue;
      });
   };

   autoCreateForEntity (NodeAddFlag::Normal, "info_player_deathmatch"); // then terrortist spawnpoints
   autoCreateForEntity (NodeAddFlag::Normal, "info_player_start"); // then add ct spawnpoints
   autoCreateForEntity (NodeAddFlag::Normal, "info_vip_start"); // then vip spawnpoint
   autoCreateForEntity (NodeAddFlag::Normal, "armoury_entity"); // weapons on the map ?

   autoCreateForEntity (NodeAddFlag::Rescue, "func_hostage_rescue"); // hostage rescue zone
   autoCreateForEntity (NodeAddFlag::Rescue, "info_hostage_rescue"); // hostage rescue zone (same as above)

   autoCreateForEntity (NodeAddFlag::Goal, "func_bomb_target"); // bombspot zone
   autoCreateForEntity (NodeAddFlag::Goal, "info_bomb_target"); // bombspot zone (same as above)
   autoCreateForEntity (NodeAddFlag::Goal, "hostage_entity"); // hostage entities
   autoCreateForEntity (NodeAddFlag::Goal, "monster_scientist"); // hostage entities (same as above)
   autoCreateForEntity (NodeAddFlag::Goal, "func_vip_safetyzone"); // vip rescue (safety) zone
   autoCreateForEntity (NodeAddFlag::Goal, "func_escapezone"); // terrorist escape zone
}

void BotGraph::eraseFromDisk () {
   // this function removes graph file from the hard disk

   StringArray forErase;
   bots.kickEveryone (true);

   auto map = game.getMapName ();
   auto data = getDataDirectory ();

   // if we're delete graph, delete all corresponding to it files
   forErase.push (strings.format ("%spwf/%s.pwf", data, map)); // graph itself
   forErase.push (strings.format ("%strain/%s.prc", data, map)); // corresponding to practice
   forErase.push (strings.format ("%strain/%s.vis", data, map)); // corresponding to vistable
   forErase.push (strings.format ("%strain/%s.pmx", data, map)); // corresponding to matrix
   forErase.push (strings.format ("%sgraph/%s.graph", data, map)); // new format graph

   for (const auto &item : forErase) {
      if (File::exists (item)) {
         plat.removeFile (item.chars ());
         ctrl.msg ("File %s, has been deleted from the hard disk", item);
      }
      else {
         logger.error ("Unable to open %s", item);
      }
   }
   reset (); // reintialize points
   m_paths.clear ();
}

const char *BotGraph::getDataDirectory (bool isMemoryFile) {
   static String buffer;
   buffer.clear ();

   if (isMemoryFile) {
      buffer.assignf ("addons/%s/data/", product.folder);
   }
   else {
      buffer.assignf ("%s/addons/%s/data/", game.getRunningModName (), product.folder);
   }
   return buffer.chars ();
}

void BotGraph::setBombOrigin (bool reset, const Vector &pos) {
   // this function stores the bomb position as a vector

   if (!game.mapIs (MapFlags::Demolition) || !bots.isBombPlanted ()) {
      return;
   }

   if (reset) {
      m_bombOrigin = nullptr;
      bots.setBombPlanted (false);

      return;
   }

   if (!pos.empty ()) {
      m_bombOrigin = pos;
      return;
   }
   bool wasFound = false;
   auto bombModel = conf.getBombModelName ();

   game.searchEntities ("classname", "grenade", [&] (edict_t *ent) {
      if (util.isModel (ent, bombModel)) {
         m_bombOrigin = game.getEntityOrigin (ent);
         wasFound = true;

         return EntitySearchResult::Break;
      }
      return EntitySearchResult::Continue;
   });

   if (!wasFound) {
      m_bombOrigin = nullptr;
      bots.setBombPlanted (false);
   }
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
   m_endJumpPoint = false;
   m_needsVisRebuild = false;
   m_jumpLearnNode = false;
   m_hasChanged = false;
   m_narrowChecked = false;
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
   m_hashTable.clear ();
}

void BotGraph::addToBucket (const Vector &pos, int index) {
   m_hashTable[locateBucket (pos)].emplace (index);
}

const Array <int32_t> &BotGraph::getNodesInBucket (const Vector &pos) {
   return m_hashTable[locateBucket (pos)];
}

void BotGraph::eraseFromBucket (const Vector &pos, int index) {
   auto &data = m_hashTable[locateBucket (pos)];

   for (size_t i = 0; i < data.length (); ++i) {
      if (data[i] == index) {
         data.erase (i, 1);
         break;
      }
   }
}

int BotGraph::locateBucket (const Vector &pos) {
   constexpr auto width = cr::square (kMaxNodes);

   auto hash = [&] (float axis, int32_t shift) {
      return ((static_cast <int> (axis) + width) & 0x007f80) >> shift;
   };
   return hash (pos.x, 15) + hash (pos.y, 7);
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

      for (int i = 0; i < length (); ++i) {
         int maxDamage = 0;
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
         (practice + (i * length ()) + i)->index[team] = static_cast <int16_t> (bestIndex);
      }
   }
   constexpr auto kHalfDamageVal = static_cast <int> (kMaxPracticeDamageValue * 0.5);

   // adjust values if overflow is about to happen
   if (adjustValues) {
      for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
         for (int i = 0; i < length (); ++i) {
            for (int j = 0; j < length (); ++j) {
               if (i == j) {
                  continue;
               }
               (practice + (i * length ()) + j)->damage[team] = static_cast <int16_t> (cr::clamp (getDangerDamage (team, i, j) - kHalfDamageVal, 0, kMaxPracticeDamageValue));
            }
         }
      }
   }

   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      m_highestDamage[team] = cr::clamp (m_highestDamage[team] - kHalfDamageVal, 1, kMaxPracticeDamageValue);
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

   if (cv_graph_fixcamp.bool_ ()) {
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
   path.vis.stand = 0;
   path.vis.crouch = 0;
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
   (m_practice.data () + (start * length ()) + goal)->value[team] = static_cast <int16_t> (value);
}

void BotGraph::setDangerDamage (int team, int start, int goal, int value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!exists (start) || !exists (goal)) {
      return;
   }
   (m_practice.data () + (start * length ()) + goal)->damage[team] = static_cast <int16_t> (value);
}
