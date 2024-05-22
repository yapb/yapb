//
// YaPB, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright © YaPB Project Developers <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_ping_base_min ("ping_base_min", "5", "Lower bound for base bot ping shown in scoreboard upon creation.", true, 0.0f, 100.0f);
ConVar cv_ping_base_max ("ping_base_max", "20", "Upper bound for base bot ping shown in scoreboard upon creation.", true, 0.0f, 100.0f);
ConVar cv_ping_count_real_players ("ping_count_real_players", "1", "Count player pings when calculating average ping for bots. If no, some random ping chosen for bots.");
ConVar cv_ping_updater_interval ("ping_updater_interval", "1.25", "Interval in which fakeping get updated in scoreboard.", true, 0.1f, 10.0f);

bool BotFakePingManager::hasFeature () const {
   return game.is (GameFlags::HasFakePings) && cv_show_latency.as <int> () >= 2;
}

void BotFakePingManager::reset (edict_t *to) {

   // no reset if game isn't support them
   if (!hasFeature ()) {
      return;
   }
   static PingBitMsg pbm {};

   for (const auto &client : util.getClients ()) {
      if (!(client.flags & ClientFlags::Used) || util.isFakeClient (client.ent)) {
         continue;
      }
      pbm.start (client.ent);

      pbm.write (1, PingBitMsg::Single);
      pbm.write (game.indexOfPlayer (to), PingBitMsg::PlayerID);
      pbm.write (0, PingBitMsg::Ping);
      pbm.write (0, PingBitMsg::Loss);

      pbm.send ();
   }
   pbm.flush ();
}

void BotFakePingManager::syncCalculate () {
   MutexScopedLock lock (m_cs);

   int averagePing {};

   if (cv_ping_count_real_players) {
      int numHumans {};

      for (const auto &client : util.getClients ()) {
         if (!(client.flags & ClientFlags::Used) || util.isFakeClient (client.ent)) {
            continue;
         }
         numHumans++;

         int ping {}, loss {};
         engfuncs.pfnGetPlayerStats (client.ent, &ping, &loss);

         averagePing += ping > 0 && ping < 200 ? ping : randomBase ();
      }

      if (numHumans > 0) {
         averagePing /= numHumans;
      }
      else {
         averagePing = randomBase ();
      }
   }
   else {
      averagePing = randomBase ();
   }

   for (auto &bot : bots) {
      auto botPing = static_cast <int> (bot->m_pingBase + rg (averagePing - averagePing * 0.2f, averagePing + averagePing * 0.2f) + rg (bot->m_difficulty + 3, bot->m_difficulty + 6));

      if (botPing < 5) {
         botPing = rg (10, 15);
      }
      else if (botPing > 100) {
         botPing = rg (30, 40);
      }
      bot->m_ping = static_cast <int> (static_cast <float> (bot->entindex () % 2 == 0 ? botPing * 0.25f : botPing * 0.5f));
   }
}

void BotFakePingManager::calculate () {
   if (!hasFeature ()) {
      return;
   }

   // throttle updating
   if (!m_recalcTime.elapsed ()) {
      return;
   }
   restartTimer ();

   worker.enqueue ([this] () {
      syncCalculate ();
   });
}

void BotFakePingManager::emit (edict_t *ent) {
   if (!util.isPlayer (ent)) {
      return;
   }
   static PingBitMsg pbm {};

   for (const auto &bot : bots) {
      pbm.start (ent);

      pbm.write (1, PingBitMsg::Single);
      pbm.write (bot->entindex () - 1, PingBitMsg::PlayerID);
      pbm.write (bot->m_ping, PingBitMsg::Ping);
      pbm.write (0, PingBitMsg::Loss);

      pbm.send ();
   }
   pbm.flush ();
}

void BotFakePingManager::restartTimer () {
   m_recalcTime.start (cv_ping_updater_interval.as <float> ());
}

int BotFakePingManager::randomBase () const {
   return rg (cv_ping_base_min.as <int> (), cv_ping_base_max.as <int> ());
}

