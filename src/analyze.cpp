//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_graph_analyze_auto_start ("yb_graph_analyze_auto_start", "0", "Autostart analyzer if all other cases are failed.");
ConVar cv_graph_analyze_distance ("yb_graph_analyze_distance", "64", "The minimum distance to keep nodes from each other.", true, 48.0f, 128.0f);
ConVar cv_graph_analyze_max_jump_height ("yb_graph_analyze_max_jump_height", "44", "Max jump height to test if next node will be unreachable.", true, 44.0f, 64.0f);
ConVar cv_graph_analyze_fps ("yb_graph_analyze_fps", "30.0", "The FPS at which analyzer process is running. This keeps game from freezing during analyzing.", false, 25.0f, 99.0f);
ConVar cv_graph_analyze_clean_paths_on_finish ("yb_graph_analyze_clean_paths_on_finish", "1", "Specifies if analyzer should clean the unnecessary paths upon finishing.");
ConVar cv_graph_analyze_optimize_nodes_on_finish ("yb_graph_analyze_optimize_nodes_on_finish", "1", "Specifies if analyzer should merge some near-placed nodes with much of connections together.");
ConVar cv_graph_analyze_mark_goals_on_finish ("yb_graph_analyze_mark_goals_on_finish", "1", "Specifies if analyzer should mark nodes as map goals automatically upon finish.");

void GraphAnalyze::start () {
   // start analyzer in few seconds after level initialized
   if (cv_graph_analyze_auto_start.bool_ ()) {
      m_updateInterval = game.time () + 3.0f;
      m_basicsCreated = false;

      // set as we're analyzing
      m_isAnalyzing = true;

      // silence all graph messages
      graph.setMessageSilence (true);

      // set all nodes as not expanded
      for (auto &expanded : m_expandedNodes) {
         expanded = false;
      }
   }
   else {
      m_updateInterval = 0.0f;
   }
}

void GraphAnalyze::update () {
   if (cr::fzero (m_updateInterval) || !m_isAnalyzing) {
      return;
   }

   if (m_updateInterval >= game.time ()) {
      return;
   }

   // add basic nodes
   if (!m_basicsCreated) {
      graph.addBasic ();
      m_basicsCreated = true;
   }

   for (int i = 0; i < graph.length (); ++i) {
      if (m_updateInterval >= game.time ()) {
         return;
      }

      if (!graph.exists (i)) {
         return;
      }

      if (m_expandedNodes[i]) {
         continue;
      }

      m_expandedNodes[i] = true;
      setUpdateInterval ();

      auto pos = graph[i].origin;
      auto range = cv_graph_analyze_distance.float_ ();

      for (int dir = 1; dir < kMaxNodeLinks; ++dir) {
         switch (dir) {
         case 1:
            flood (pos, { pos.x + range, pos.y, pos.z }, range);
            break;

         case 2:
            flood (pos, { pos.x - range, pos.y, pos.z }, range);
            break;

         case 3:
            flood (pos, { pos.x, pos.y + range, pos.z }, range);
            break;

         case 4:
            flood (pos, { pos.x, pos.y - range, pos.z }, range);
            break;

         case 5:
            flood (pos, { pos.x + range, pos.y, pos.z + 128.0f }, range);
            break;

         case 6:
            flood (pos, { pos.x - range, pos.y, pos.z + 128.0f }, range);
            break;

         case 7:
            flood (pos, { pos.x, pos.y + range, pos.z + 128.0f }, range);
            break;

         case 8:
            flood (pos, { pos.x, pos.y - range, pos.z + 128.0f }, range);
            break;
         }
      }
   }

   // finish generation if no updates occurred recently
   if (m_updateInterval + 2.0f < game.time ()) {
      finish ();
      return;
   }
}

void GraphAnalyze::finish () {
   // run optimization on finish
   optimize ();

   // mark goal nodes
   markGoals ();

   m_isAnalyzing = false;
   m_updateInterval = 0.0f;

   // un-silence all graph messages
   graph.setMessageSilence (false);
}

void GraphAnalyze::optimize () {
   if (graph.length () == 0) {
      return;
   }

   if (!cv_graph_analyze_optimize_nodes_on_finish.bool_ ()) {
      return;
   }

   // has any connections?
   auto isConnected = [] (const int index) -> bool {
      for (const auto &path : graph) {
         if (path.number != index) {
            for (const auto &link : path.links) {
               if (link.index == index) {
                  return true;
               }
            }
         }
      }
      return false;
   };

   // clean bad paths
   int connections = 0;

   for (auto i = 0; i < graph.length (); ++i) {
      connections = 0;

      for (const auto &link : graph[i].links) {
         if (link.index != kInvalidNodeIndex) {
            if (link.index > graph.length ()) {
               graph.erase (i);
            }
            ++connections;
            break;
         }
      }

      // no connections
      if (!connections && !isConnected (i)) {
         graph.erase (i);
      }

      // path number differs
      if (graph[i].number != i) {
         graph.erase (i);
      }

      for (const auto &link : graph[i].links) {
         if (link.index != kInvalidNodeIndex) {
            if (link.index >= graph.length () || link.index < -kInvalidNodeIndex) {
               graph.erase (i);
            }
            else if (link.index == i) {
               graph.erase (i);
            }
         }
      }
   }


   // clear the uselss connections
   if (cv_graph_analyze_clean_paths_on_finish.bool_ ()) {
      for (auto i = 0; i < graph.length (); ++i) {
         graph.clearConnections (i);
      }
   }

   auto smooth = []  (const Array <int> &nodes) {
      Vector result;

      for (const auto &node : nodes) {
         result += graph[node].origin;
      }
      result /= kMaxNodeLinks;
      result.z = graph[nodes.first ()].origin.z;

      return result;
   };

   // set all nodes as not optimized
   for (auto &optimized : m_optimizedNodes) {
      optimized = false;
   }

   for (int i = 0; i < graph.length (); ++i) {
      if (m_optimizedNodes[i]) {
         continue;
      }
      const auto &path = graph[i];
      Array <int> indexes;

      for (const auto &link : path.links) {
         if (graph.exists (link.index) && !m_optimizedNodes[link.index] && cr::fequal (path.origin.z, graph[link.index].origin.z)) {
            indexes.emplace (link.index);
         }
      }

      // we're have max out node links
      if (indexes.length () >= kMaxNodeLinks) {
         const Vector &pos = smooth (indexes);

         for (const auto &index : indexes) {
            graph.erase (index);
         }
         graph.add (NodeAddFlag::Normal, pos);
      }
   }
}

void GraphAnalyze::flood (const Vector &pos, const Vector &next, float range) {
   range *= 0.75f;

   TraceResult tr;
   game.testHull (pos, { next.x, next.y, next.z + 19.0f }, TraceIgnore::Monsters, head_hull, nullptr, &tr);

   // we're can't reach next point
   if (!cr::fequal (tr.flFraction, 1.0f) && !game.isShootableBreakable (tr.pHit)) {
      return;
   }

   // we're have something in around, skip
   if (graph.exists (graph.getForAnalyzer (tr.vecEndPos, range))) {
      return;
   }
   game.testHull (tr.vecEndPos, { tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z - 999.0f }, TraceIgnore::Monsters, head_hull, nullptr, &tr);

   // ground is away for a break
   if (cr::fequal (tr.flFraction, 1.0f)) {
      return;
   }
   Vector nextPos = { tr.vecEndPos.x, tr.vecEndPos.y, tr.vecEndPos.z + 19.0f };

   const int endIndex = graph.getForAnalyzer (nextPos, range);
   const int targetIndex = graph.getNearestNoBuckets (nextPos, 250.0f);

   if (graph.exists (endIndex) && !graph.exists (targetIndex)) {
      return;
   }
   auto targetPos = graph[targetIndex].origin;

   // re-check there's nothing nearby, and add something we're want
   if (!graph.exists (graph.getNearestNoBuckets (nextPos, range))) {
      m_isCrouch = false;
      game.testLine (nextPos, { nextPos.x, nextPos.y, nextPos.z + 36.0f }, TraceIgnore::Monsters, nullptr, &tr);

      if (!cr::fequal (tr.flFraction, 1.0f)) {
         m_isCrouch = true;
      }
      auto testPos = m_isCrouch ? Vector { nextPos.x, nextPos.y, nextPos.z - 18.0f } : nextPos;

      if ((graph.isNodeReacheable (targetPos, testPos) && graph.isNodeReacheable (testPos, targetPos)) || (graph.isNodeReacheableWithJump (targetPos, testPos) && graph.isNodeReacheableWithJump (targetPos, testPos))) {
         graph.add (NodeAddFlag::Normal, m_isCrouch ? Vector { nextPos.x, nextPos.y, nextPos.z - 9.0f } : nextPos);
      }
   }
}

void GraphAnalyze::setUpdateInterval () {
   const auto frametime (globals->frametime);

   if ((cv_graph_analyze_fps.float_ () + frametime) <= 1.0f / frametime) {
      m_updateInterval = game.time () + frametime * 0.06f;
   }
}

void GraphAnalyze::markGoals () {
   if (!cv_graph_analyze_mark_goals_on_finish.bool_ ()) {
      return;
   }

   auto updateNodeFlags = [] (int type, const char *entity) {
      game.searchEntities ("classname", entity, [&] (edict_t *ent) {
         for  (auto &path : graph) {
            const auto &absOrigin = path.origin + Vector (1.0f, 1.0f, 1.0f);

            if (ent->v.absmin.x > absOrigin.x || ent->v.absmin.y > absOrigin.y) {
               continue;
            }

            if (ent->v.absmax.x < absOrigin.x || ent->v.absmax.y < absOrigin.y) {
               continue;
            }
            path.flags |= type;
         }
         return EntitySearchResult::Continue;
      });
   };


   if (game.mapIs (MapFlags::Demolition)) {
      updateNodeFlags (NodeFlag::Goal, "func_bomb_target"); // bombspot zone
      updateNodeFlags (NodeFlag::Goal, "info_bomb_target"); // bombspot zone (same as above)
   }
   else if (game.mapIs (MapFlags::HostageRescue)) {
      updateNodeFlags (NodeFlag::Rescue, "func_hostage_rescue"); // hostage rescue zone
      updateNodeFlags (NodeFlag::Rescue, "info_hostage_rescue"); // hostage rescue zone (same as above)
      updateNodeFlags (NodeFlag::Rescue, "info_player_start"); // then add ct spawnpoints

      updateNodeFlags (NodeFlag::Goal, "hostage_entity"); // hostage entities
      updateNodeFlags (NodeFlag::Goal, "monster_scientist"); // hostage entities (same as above)
   }
   else if (game.mapIs (MapFlags::Assassination)) {
      updateNodeFlags (NodeFlag::Goal, "func_vip_safetyzone"); // vip rescue (safety) zone
      updateNodeFlags (NodeFlag::Goal, "func_escapezone"); // terrorist escape zone
   }
}
