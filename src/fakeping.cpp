//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void PingBitMsg::Clear () {
  bits_ = 0;
  used_ = 0;
  size_ = 0;
}

void PingBitMsg::Write (const int32_t bit, const int32_t size) {
  if (size < 1 || size > 32 - used_) {
    return;
  }
  const auto limit = 1u << size;

  auto value = bit;
  if (value < 0) {
    value = 0;
  }
  if (static_cast<uint32_t> (value) >= limit) {
    value = static_cast<int32_t> (limit - 1);
  }

  bits_ |= static_cast<uint32_t> (value) << used_;
  used_ += size;

  while (used_ >= 8) {
    if (size_ < kMaxBytes) {
      bytes_[size_++] = static_cast<uint8_t> (bits_ & 0xFFu);
    }
    bits_ >>= 8;
    used_ -= 8;
  }
}

void PingBitMsg::Finish () {
  Write (0, 1);

  if (used_ > 0 && size_ < kMaxBytes) {
    bytes_[size_++] = static_cast<uint8_t> (bits_ & 0xFFu);
  }
  bits_ = 0;
  used_ = 0;
}

void PingBitMsg::WritePayload () {
  size_t offset = 0;
  for (; offset + sizeof (uint32_t) <= size_; offset += sizeof (uint32_t)) {
    uint32_t word {};
    memcpy (&word, bytes_ + offset, sizeof (word));
    msg_.WriteLong (static_cast<int32_t> (word));
  }
  for (; offset < size_; ++offset) {
    msg_.WriteByte (bytes_[offset]);
  }
}

void PingBitMsg::Send (edict_t *ent) {
  if (!size_) {
    return;
  }
  msg_.Start (MSG_ONE_UNRELIABLE, SVC_PINGS, nullptr, ent);
  WritePayload ();
  msg_.end ();
}

void PingBitMsg::Broadcast () {
  if (!size_) {
    return;
  }
  msg_.Start (MSG_BROADCAST, SVC_PINGS, nullptr, nullptr);
  WritePayload ();
  msg_.end ();
}

bool FakePingManager::HasFeature () const {
  return game.Is (GameFlags::HasFakePings) && cv_show_latency.As<int> () >= 2;
}

void FakePingManager::Reset (edict_t *to) {
  if (!HasFeature ()) {
    return;
  }

  PingBitMsg reset {};
  reset.Write (1, PingBitMsg::Single);
  reset.Write (game.IndexOfPlayer (to), PingBitMsg::PlayerID);
  reset.Write (0, PingBitMsg::Ping);
  reset.Write (0, PingBitMsg::Loss);
  reset.Finish ();

  for (const auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }
    reset.Send (client.ent);
  }
  payload_dirty_ = true;
}

void FakePingManager::BuildPayload () {
  payload_.Clear ();

  for (const auto &bot : bots) {
    payload_.Write (1, PingBitMsg::Single);
    payload_.Write (bot.Entindex () - 1, PingBitMsg::PlayerID);
    payload_.Write (bot.ping_.load (ystl::MemoryOrder::relaxed), PingBitMsg::Ping);
    payload_.Write (0, PingBitMsg::Loss);
  }
  payload_.Finish ();
  payload_count_ = bots.GetBotCount ();
  payload_dirty_ = false;
}

void FakePingManager::Recalculate () {
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

    bot.ping_.store (static_cast<int> (bot.Entindex () % 2 == 0 ? bot_ping * 0.25f : bot_ping * 0.5f), ystl::MemoryOrder::relaxed);
  }
  payload_dirty_ = true;
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
  Recalculate ();
}

void FakePingManager::Emit (edict_t *ent) {
  if (!HasFeature () || !game.IsPlayerEntity (ent) || bots.GetBotCount () == 0) {
    return;
  }

  if (payload_dirty_ || payload_count_ != bots.GetBotCount ()) {
    BuildPayload ();
  }
  payload_.Send (ent);
}

void FakePingManager::Broadcast () {
  if (!HasFeature () || bots.GetBotCount () == 0) {
    return;
  }

  edict_t *holder = nullptr;
  int holders = 0;

  for (const auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }
    if (((client.ent->v.button | client.ent->v.oldbuttons) & IN_SCORE) == 0) {
      continue;
    }
    holder = client.ent;
    ++holders;
  }

  if (!holders) {
    return;
  }

  if (payload_dirty_ || payload_count_ != bots.GetBotCount ()) {
    BuildPayload ();
  }
  if (holders == 1) {
    payload_.Send (holder);
  }
  else {
    payload_.Broadcast ();
  }
}

void FakePingManager::RestartTimer () {
  recalc_time_.start (cv_ping_updater_interval.As<float> ());
}

int FakePingManager::RandomBase () const {
  return ystl::rg (cv_ping_base_min.As<int> (), cv_ping_base_max.As<int> ());
}

} // namespace bot
