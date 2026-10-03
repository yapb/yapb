//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// limits for storing practice data
namespace bot {

enum class VisIndex : uint16_t {
  None = 0,
  Stand = 1,
  Crouch = 2,
  Any = Stand | Crouch
};
YSTL_ENABLE_ENUM_FLAGS (VisIndex);

// defines visibility count
struct PathVis {
  uint16_t stand {}, crouch {};
};

YSTL_LE_FIELDS (PathVis, stand, crouch);

class GraphVistable final : public ystl::Singleton<GraphVistable> {
public:
  using VisStorage = uint8_t;

private:
  ystl::Array<VisStorage, ReservePolicy::Proportional> vistable_ {};
  bool rebuild_ {};
  int size_ {};
  size_t row_bytes_ {}; // bytes per row (4 pairs per byte)

  int cur_index_ {};
  int slice_index_ {};

  ystl::CountdownTimer notify_msg_timer_ {};

public:
  explicit GraphVistable () = default;
  ~GraphVistable () = default;

public:
  bool Visible (int src_index, int dest_index, VisIndex vis = VisIndex::Any) const;

  void Load ();
  void Save () const;
  void Rebuild ();

public:
  // triggers re-check for all the nodes
  void StartRebuild ();

  // ready to use ?
  bool IsReady () const {
    return !rebuild_;
  }

  // is visible fromr both points ?
  bool VisibleBothSides (int src_index, int dest_index, VisIndex vis = VisIndex::Any) const {
    return Visible (src_index, dest_index, vis) && Visible (dest_index, src_index, vis);
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (GraphVistable, vistab);

} // namespace bot
