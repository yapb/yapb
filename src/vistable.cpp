//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void GraphVistable::Rebuild () {
  if (!rebuild_) {
    return;
  }
  size_ = graph.Length ();

  // empty graph cannot be built - settle as ready-but-empty
  if (!size_) {
    rebuild_ = false;
    return;
  }

  // graph changed mid-build: keep the table marked unusable and retry later
  if (graph.HasChanged ()) {
    return;
  }
  row_bytes_ = static_cast<size_t> ((size_ + 3) / 4); // 4 pairs per byte

  // fixup storage after graph size change
  if (vistable_.size () != static_cast<size_t> (size_) * row_bytes_) {
    vistable_.resize (static_cast<size_t> (size_) * row_bytes_);
    cur_index_ = 0;
    slice_index_ = 0;
  }
  Trace::Result tr {};
  uint8_t shift {};
  VisIndex res = VisIndex::None;

  if (!graph.Exists (slice_index_)) {
    slice_index_ = 0;
  }

  if (!graph.Exists (cur_index_)) {
    cur_index_ = 0;
  }
  const auto &vis = graph[cur_index_];

  auto source_crouch = vis.origin;
  auto source_stand = vis.origin;

  if (has_flag (vis.flags, NodeFlag::Crouch)) {
    source_crouch.z += 12.0f;
    source_stand.z += 18.0f + 28.0f;
  }
  else {
    source_crouch.z += -18.0f + 12.0f;
    source_stand.z += 28.0f;
  }
  auto end = slice_index_ + ystl::rg (250, 400);

  if (end > size_) {
    end = size_;
  }
  // reset vis counts at the start of each new source node
  if (slice_index_ == 0) {
    graph[vis.number].vis.crouch = 0;
    graph[vis.number].vis.stand = 0;
  }

  for (int i = slice_index_; i < end; ++i) {
    const auto &path = graph[i];

    // first check ducked visibility
    ystl::Vector dest = path.origin;

    trace.Line (source_crouch, dest, TraceIgnore::Monsters, nullptr, &tr);

    // check if line of sight to object is not blocked (i.e. visible)
    if (!ystl::fequal (tr.fraction, 1.0f)) {
      res = VisIndex::Stand;
    }
    else {
      res = VisIndex::None;
    }
    res <<= 1;

    trace.Line (source_stand, dest, TraceIgnore::Monsters, nullptr, &tr);

    // check if line of sight to object is not blocked (i.e. visible)
    if (!ystl::fequal (tr.fraction, 1.0f)) {
      res |= VisIndex::Stand;
    }

    if (res != VisIndex::None) {
      dest = path.origin;

      // first check ducked visibility
      if (has_flag (path.flags, NodeFlag::Crouch)) {
        dest.z += 18.0f + 28.0f;
      }
      else {
        dest.z += 28.0f;
      }
      trace.Line (source_crouch, dest, TraceIgnore::Monsters, nullptr, &tr);

      // check if line of sight to object is not blocked (i.e. visible)
      if (!ystl::fequal (tr.fraction, 1.0f)) {
        res |= VisIndex::Crouch;
      }
      else {
        res &= VisIndex::Stand;
      }
      trace.Line (source_stand, dest, TraceIgnore::Monsters, nullptr, &tr);

      // check if line of sight to object is not blocked (i.e. visible)
      if (!ystl::fequal (tr.fraction, 1.0f)) {
        res |= VisIndex::Stand;
      }
      else {
        res &= VisIndex::Crouch;
      }
    }
    shift = static_cast<uint8_t> ((path.number & 3) << 1); // slot inside byte
    const size_t idx = static_cast<size_t> (vis.number) * row_bytes_ + static_cast<size_t> (path.number >> 2);

    vistable_[idx] &= static_cast<uint8_t> (~static_cast<uint8_t> (VisIndex::Any << shift));
    vistable_[idx] |= res << shift;

    if (!has_flag (res, VisIndex::Crouch)) {
      ++graph[vis.number].vis.crouch;
    }

    if (!has_flag (res, VisIndex::Stand)) {
      ++graph[vis.number].vis.stand;
    }
  }

  if (end == size_) {
    slice_index_ = 0;
    cur_index_++;
  }
  else {
    slice_index_ = end;
  }
  auto notify_progress = [] (int value) {
    if (value >= 100 || ctrl.IsDebug ()) {
      game.Print ("Rebuilding vistable... %d%% done.", value);
    }
  };

  // notify host about rebuilding
  if (notify_msg_timer_.started () && notify_msg_timer_.elapsed () && end == size_) {
    notify_progress (cur_index_ * 100 / size_);
    notify_msg_timer_.start (1.0f);
  }

  if (cur_index_ == size_ && end == size_) {
    notify_progress (100);

    vistable_.shrink (); // drop bit_ceil slack
    rebuild_ = false;
    notify_msg_timer_.invalidate ();
    cur_index_ = 0;

    Save ();
  }
}

void GraphVistable::StartRebuild () {
  rebuild_ = true;
  notify_msg_timer_.start (0.0f);
}

bool GraphVistable::Visible (int src_index, int dest_index, VisIndex vis) const {
  if (!graph.Exists (src_index) || !graph.Exists (dest_index)) {
    return false;
  }
  const size_t idx = static_cast<size_t> (src_index) * row_bytes_ + static_cast<size_t> (dest_index >> 2); // packed row

  if (!row_bytes_ || idx >= vistable_.size ()) {
    return false; // not loaded yet
  }
  return !(((vistable_[idx] >> ((dest_index & 3) << 1)) & vis) == vis);
}

void GraphVistable::Load () {
  rebuild_ = true;
  size_ = graph.Length ();

  slice_index_ = 0;
  cur_index_ = 0;
  notify_msg_timer_.invalidate ();

  if (!size_) {
    return;
  }
  row_bytes_ = static_cast<size_t> ((size_ + 3) / 4); // 4 pairs per byte

  const size_t expected = static_cast<size_t> (size_) * row_bytes_;
  bool data_loaded = bstor.Load<VisStorage> (vistable_);

  // reject stale layout from previous versions
  if (data_loaded && vistable_.size () != expected) {
    data_loaded = false;
  }

  // if loaded, do not recalculate visibility
  if (data_loaded) {
    rebuild_ = false; // slack already dropped by storage
  }
  else {
    vistable_.resize (expected);
    notify_msg_timer_.start (0.0f);
  }
}

void GraphVistable::Save () const {
  if (!size_ || rebuild_) {
    return;
  }
  bstor.Save<VisStorage> (vistable_);
}

} // namespace bot
