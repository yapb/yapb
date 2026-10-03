//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

bool FakePingManager::HasFeature () const {
  return game.Is (GameFlags::HasFakePings) && cv_show_latency.As<int> () >= 2;
}

void FakePingManager::Reset (edict_t *to) {

  // no reset if game isn't support them
  if (!HasFeature ()) {
    return;
  }

  for (const auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }
    pbm_.Start (client.ent);

    pbm_.Write (1, PingBitMsg::Single);
    pbm_.Write (game.IndexOfPlayer (to), PingBitMsg::PlayerID);
    pbm_.Write (0, PingBitMsg::Ping);
    pbm_.Write (0, PingBitMsg::Loss);

    pbm_.Send ();
    pbm_.Flush ();
  }
}

void FakePingManager::SyncCalculate () {
  int average_ping {};

  if (cv_ping_count_real_players) {
    int num_humans {};

    for (const auto &client : clients) {
      if (!client.IsUsed () || client.IsBot ()) {
        continue;
      }
      ++num_humans;

      int ping {}, loss {};
      engfuncs.pfnGetPlayerStats (client.ent, &ping, &loss);

      average_ping += ping > 0 && ping < 200 ? ping : RandomBase ();
    }

    if (num_humans > 0) {
      average_ping /= num_humans;
    }
    else {
      average_ping = RandomBase ();
    }
  }
  else {
    average_ping = RandomBase ();
  }

  for (auto &bot : bots) {
    const auto diff = static_cast<int> (static_cast<float> (average_ping) * 0.2f);
    const auto int_diff = static_cast<int> (bot.difficulty_);

    // randomize bot ping
    auto bot_ping =
      static_cast<float> (bot.ping_base_ + ystl::rg (average_ping - diff, average_ping + diff) + ystl::rg (int_diff + 3, int_diff + 6));

    if (bot_ping < 5.0f) {
      bot_ping = ystl::rg (10.0f, 15.0f);
    }
    else if (bot_ping > 75.0f) {
      bot_ping = ystl::rg (30.0f, 40.0f);
    }
    bot.ping_ = static_cast<int> (bot.Entindex () % 2 == 0 ? bot_ping * 0.25f : bot_ping * 0.5f);
  }
}

void FakePingManager::Calculate () {
  if (!HasFeature ()) {
    return;
  }

  // throttle updating
  if (!recalc_time_.elapsed ()) {
    return;
  }
  RestartTimer ();

  worker.Enqueue ([this] () {
    SyncCalculate ();
  });
}

void FakePingManager::Emit (edict_t *ent) {
  if (!HasFeature () || !game.IsPlayerEntity (ent)) {
    return;
  }

  for (const auto &bot : bots) {
    pbm_.Start (ent);

    pbm_.Write (1, PingBitMsg::Single);
    pbm_.Write (bot.Entindex () - 1, PingBitMsg::PlayerID);
    pbm_.Write (bot.ping_, PingBitMsg::Ping);
    pbm_.Write (0, PingBitMsg::Loss);

    pbm_.Send ();
  }
  pbm_.Flush ();
}

void FakePingManager::RestartTimer () {
  recalc_time_.start (cv_ping_updater_interval.As<float> ());
}

int FakePingManager::RandomBase () const {
  return ystl::rg (cv_ping_base_min.As<int> (), cv_ping_base_max.As<int> ());
}

} // namespace bot
