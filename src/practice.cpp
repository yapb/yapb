//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

int32_t BotPractice::getIndex (int32_t team, int32_t start, int32_t goal) {
   if (!exists (team, start, goal)) {
      return kInvalidNodeIndex;
   }
   return m_data[{start, goal, team}].index;
}

void BotPractice::setIndex (int32_t team, int32_t start, int32_t goal, int32_t value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!graph.exists (start) || !graph.exists (goal) || !graph.exists (value)) {
      return;
   }
   m_data[{start, goal, team}].index = static_cast <int16_t> (value);
}

int32_t BotPractice::getValue (int32_t team, int32_t start, int32_t goal) {
   if (!exists (team, start, goal)) {
      return 0;
   }
   return m_data[{start, goal, team}].value;
}

void BotPractice::setValue (int32_t team, int32_t start, int32_t goal, int32_t value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!graph.exists (start) || !graph.exists (goal)) {
      return;
   }
   m_data[{start, goal, team}].value = static_cast <int16_t> (value);
}

int32_t BotPractice::getDamage (int32_t team, int32_t start, int32_t goal) {
   if (!exists (team, start, goal)) {
      return 0;
   }
   return m_data[{start, goal, team}].damage;
}

void BotPractice::setDamage (int32_t team, int32_t start, int32_t goal, int32_t value) {
   if (team != Team::Terrorist && team != Team::CT) {
      return;
   }

   // reliability check
   if (!graph.exists (start) || !graph.exists (goal)) {
      return;
   }
   m_data[{start, goal, team}].damage = static_cast <int16_t> (value);
}

void BotPractice::update () {
   // this function called after each end of the round to update knowledge about most dangerous nodes for each team.

   auto graphLength = graph.length ();

   // no nodes, no practice used or nodes edited or being edited?
   if (!graphLength || graph.hasChanged () || !vistab.isReady ()) {
      return; // no action
   }
   auto adjustValues = false;

   game.print ("REF");

   // get the most dangerous node for this position for both teams
   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      auto bestIndex = kInvalidNodeIndex; // best index to store

      for (int i = 0; i < graphLength; ++i) {
         auto maxDamage = 0;
         bestIndex = kInvalidNodeIndex;

         for (int j = 0; j < graphLength; ++j) {
            if (i == j || !vistab.visible (i, j) || !exists (team, i, j)) {
               continue;
            }
            auto actDamage = getDamage (team, i, j);

            if (actDamage > maxDamage) {
               maxDamage = actDamage;
               bestIndex = j;
            }
         }

         if (maxDamage > PracticeLimit::Damage) {
            adjustValues = true;
         }
         setIndex (team, i, i, bestIndex);
      }
   }
   constexpr auto kFullDamageVal = static_cast <int32_t> (PracticeLimit::Damage);
   constexpr auto kHalfDamageVal = static_cast <int32_t> (PracticeLimit::Damage / 2);

   // adjust values if overflow is about to happen
   if (adjustValues) {
      for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
         for (int i = 0; i < graphLength; ++i) {
            for (int j = 0; j < graphLength; ++j) {
               if (i == j || !exists (team, i, j)) {
                  continue;
               }
               setDamage (team, i, j, cr::clamp (getDamage (team, i, j) - kHalfDamageVal, 0, kFullDamageVal));
            }
         }
      }
   }

   for (int team = Team::Terrorist; team < kGameTeamNum; ++team) {
      m_teamHighestDamage[team] = cr::clamp (m_teamHighestDamage[team] - kHalfDamageVal, 1, kFullDamageVal);
   }
}

void BotPractice::save () {
   if (!graph.length () || graph.hasChanged ()) {
      return; // no action
   }
   SmallArray <DangerSaveRestore> data;

   // copy hash-map data to our vector
   m_data.foreach ([&data] (const DangerStorage &ds, const PracticeData &pd) {
      data.emplace (ds, pd);
   });
   bstor.save <DangerSaveRestore> (data);
}

void BotPractice::load () {
   if (!graph.length ()) {
      return; // no action
   }
   SmallArray <DangerSaveRestore> data;
   m_data.clear ();

   bool dataLoaded = bstor.load <DangerSaveRestore> (data);

   // copy back to hash table
   if (dataLoaded) {
      for (const auto &dsr : data) {
         if (dsr.data.damage > 0 || dsr.data.index != kInvalidNodeIndex || dsr.data.value > 0) {
            m_data.insert (dsr.danger, dsr.data);
         }
      }
   }
}

