//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// adopted from pingfaker amxx plugin
namespace bot {

class PingBitMsg final {
private:
  int32_t bits_ {};
  int32_t used_ {};

  MessageWriter msg_ {};
  bool started_ {};

public:
  enum : int32_t {
    Single = 1,
    PlayerID = 5,
    Loss = 7,
    Ping = 12
  };

public:
  PingBitMsg () = default;
  ~PingBitMsg () = default;

public:
  void Write (int32_t bit, int32_t size) {
    if (size > 32 - used_ || size < 1) {
      return;
    }
    const auto max_size = ystl::bit (size);

    if (bit >= max_size) {
      bit = max_size - 1;
    }
    bits_ = bits_ + (bit << used_);
    used_ += size;
  }

  void Send (bool remaining = false) {
    while (used_ >= 8) {
      msg_.WriteByte (bits_ & (ystl::bit (8) - 1));
      bits_ = (bits_ >> 8);
      used_ -= 8;
    }

    if (remaining && used_ > 0) {
      msg_.WriteByte (bits_);
      bits_ = used_ = 0;
    }
  }

  void Start (edict_t *ent) {
    if (started_) {
      return;
    }
    msg_.Start (MSG_ONE_UNRELIABLE, SVC_PINGS, nullptr, ent);
    started_ = true;
  }

  void Flush () {
    if (!started_) {
      return;
    }
    Write (0, Single);
    Send (true);

    started_ = false;
    msg_.end ();
  }
};

// bot fakeping manager
class FakePingManager final : public ystl::Singleton<FakePingManager> {
private:
  ystl::CountdownTimer recalc_time_ {};
  PingBitMsg pbm_ {};

public:
  explicit FakePingManager () = default;
  ~FakePingManager () = default;

public:
  // verify game supports fakeping and it's enabled
  bool HasFeature () const;

  // reset the ping on disconnecting player
  void Reset (edict_t *ent);

  // calculate our own pings for all the bots
  void SyncCalculate ();

  // calculate our own pings for all the bots
  void Calculate ();

  // emit pings in update client data hook
  void Emit (edict_t *ent);

  // restarts update timers
  void RestartTimer ();

  // get random base ping
  int RandomBase () const;
};

// expose fakeping manager
YSTL_EXPOSE_GLOBAL_SINGLETON (FakePingManager, fakeping);

} // namespace bot
