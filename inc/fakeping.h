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
  static constexpr size_t kMaxBytes = 128;

  uint32_t bits_ {};
  int32_t used_ {};
  size_t size_ {};
  uint8_t bytes_[kMaxBytes] {};

  MessageWriter msg_ {};

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
  // clears the packed payload
  void Clear ();

  // appends size bits of bit to the payload
  void Write (int32_t bit, int32_t size);

  // writes the terminator and pads the last byte
  void Finish ();

  // sends the packed payload to a single client
  void Send (edict_t *ent);

  // sends the packed payload to every client
  void Broadcast ();

private:
  void WritePayload ();
};

// bot fakeping manager
class FakePingManager final : public ystl::Singleton<FakePingManager> {
private:
  ystl::CountdownTimer recalc_time_ {};
  PingBitMsg payload_ {};
  bool payload_dirty_ { true };
  int32_t payload_count_ { -1 };

private:
  // rebuilds the shared bot payload
  void BuildPayload ();

public:
  explicit FakePingManager () = default;
  ~FakePingManager () = default;

public:
  // verify game supports fakeping and it's enabled
  bool HasFeature () const;

  // reset the ping on disconnecting player
  void Reset (edict_t *ent);

  // calculate our own pings for all the bots
  void Recalculate ();

  // calculate our own pings for all the bots
  void Calculate ();

  // send pings to a connecting client
  void Emit (edict_t *ent);

  // send pings to scoreboard holders
  void Broadcast ();

  // restarts update timers
  void RestartTimer ();

  // get random base ping
  int RandomBase () const;
};

// expose fakeping manager
YSTL_EXPOSE_GLOBAL_SINGLETON (FakePingManager, fakeping);

} // namespace bot
