//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// forward declarations
namespace bot {

class Bot;

// limits for storing practice data
namespace PracticeLimit {
constexpr uint16_t kGoal = 2040;
constexpr uint16_t kDamage = 2040;
}

// hybrid storage: dense diagonal for hot path, sparse map for the rest
class Practice final : public ystl::Singleton<Practice> {
public:
  // experience cell
  struct Cell {
    uint16_t damage {};
    uint16_t value {};
    uint16_t index { static_cast<uint16_t> (kInvalidNodeIndex) };

  public:
    Cell () = default;
    ~Cell () = default;

  public:
    Cell (uint16_t d, uint16_t v, uint16_t i) : damage (d), value (v), index (i) {}

  public:
    bool IsDefault () const noexcept {
      return damage == 0 && value == 0 && index == static_cast<uint16_t> (kInvalidNodeIndex);
    }
  };

  // sparse disk entry (v6)
  struct Entry {
    uint16_t team {};
    uint16_t src {};
    uint16_t dst {};
    Cell cell {};
  };

private:
  ystl::Array<Cell> diag_ {}; // hot diagonal cells [team * length + node]
  ystl::HashMap<uint32_t, Cell> sparse_ {}; // off-diagonal cells only

  ystl::FixedArray<uint16_t, ystl::to_underlying (Team::Num)> team_damage_ {};

  int32_t size_ {}; // current graph size
  bool initialized_ {}; // storage initialized flag

  ystl::Atomic<bool> busy_ {}; // storage is being updated/loaded on worker thread

private:
  // is storage being updated on worker thread ?
  bool Busy () const noexcept {
    return busy_.load (ystl::MemoryOrder::acquire);
  }

private:
  // flat diagonal position
  size_t DiagPos (Team team, int src) const noexcept {
    return static_cast<size_t> (ystl::to_underlying (team)) * static_cast<size_t> (size_) + static_cast<size_t> (src);
  }

  // sparse key for off-diagonal cells
  uint32_t SparseKey (Team team, int src, int dst) const noexcept {
    const auto n = static_cast<uint32_t> (size_);
    return (static_cast<uint32_t> (ystl::to_underlying (team)) * n + static_cast<uint32_t> (src)) * n + static_cast<uint32_t> (dst);
  }

  // check bounds
  bool InBounds (int src, int dst) const noexcept {
    return src >= 0 && src < size_ && dst >= 0 && dst < size_;
  }

  // validate team and bounds (combines common checks)
  bool Valid (Team team, int src, int dst) const noexcept {
    return (team == Team::Terrorist || team == Team::CT) && InBounds (src, dst);
  }

public:
  Practice () = default;
  ~Practice () = default;

private:
  // invalid index as uint16_t for comparisons
  static constexpr uint16_t kInvalidIndex16 = static_cast<uint16_t> (kInvalidNodeIndex);

  // initialize storage for given graph size
  void Initialize () noexcept;

  void SyncUpdate ();
  void SyncLoad ();

public:
  int32_t GetIndex (Team team, int32_t start, int32_t goal) noexcept;
  void SetIndex (Team team, int32_t start, int32_t goal, int32_t value) noexcept;

  int32_t GetValue (Team team, int32_t start, int32_t goal) const noexcept;
  void SetValue (Team team, int32_t start, int32_t goal, int32_t value) noexcept;

  int32_t GetDamage (Team team, int32_t start, int32_t goal) const noexcept;
  void SetDamage (Team team, int32_t start, int32_t goal, int32_t value) noexcept;

  // get damage with possibility to get highest team damage
  float GetDamageEx (Team team, int32_t start, int32_t goal, bool add_team_highest_damage) noexcept;

public:
  // update practice damage/value tracking (called when bot takes damage)
  void UpdateValue (Bot *bot, int damage);
  void UpdateDamage (Bot *bot, edict_t *attacker, int damage);

  void Update ();
  void Load ();
  void Save ();

public:
  template <typename U = int32_t> U GetTeamDamage (Team team) const {
    if (Busy ()) [[unlikely]] {
      return static_cast<U> (1);
    }
    return static_cast<U> (ystl::max (1, static_cast<int32_t> (team_damage_[team])));
  }

  void SetTeamDamage (Team team, int32_t value) {
    if (Busy ()) [[unlikely]] {
      return;
    }
    team_damage_[team] = static_cast<uint16_t> (value);
  }
};

YSTL_LE_FIELDS (Practice::Cell, damage, value, index);
YSTL_LE_FIELDS (Practice::Entry, team, src, dst, cell);

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Practice, practice);

} // namespace bot
