//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void Practice::Initialize () noexcept {
  size_ = graph.Length ();
  const size_t diag_size = static_cast<size_t> (ystl::to_underlying (Team::Num)) * static_cast<size_t> (ystl::max (size_, 0));

  // dense diagonal only, sparse map starts empty
  diag_.resize (diag_size);
  diag_.fill (Cell {});

  sparse_.clear ();
  initialized_ = true;
}

int32_t Practice::GetIndex (Team team, int32_t start, int32_t goal) noexcept {
  if (!Valid (team, start, goal) || !initialized_ || Busy ()) [[unlikely]] {
    return kInvalidNodeIndex;
  }

  // hot diagonal path
  if (start == goal) [[likely]] {
    const auto index = diag_[DiagPos (team, start)].index;
    return index == kInvalidIndex16 ? kInvalidNodeIndex : static_cast<int32_t> (index);
  }
  const auto *found = sparse_.find (SparseKey (team, start, goal));

  if (found == nullptr || found->index == kInvalidIndex16) [[unlikely]] {
    return kInvalidNodeIndex;
  }
  return static_cast<int32_t> (found->index);
}

void Practice::SetIndex (Team team, int32_t start, int32_t goal, int32_t value) noexcept {
  if (Busy () || !Valid (team, start, goal)) [[unlikely]] {
    return;
  }

  // hot diagonal path
  if (start == goal) [[likely]] {
    diag_[DiagPos (team, start)].index = static_cast<uint16_t> (value);
    return;
  }
  const auto key = SparseKey (team, start, goal);
  const auto *found = sparse_.find (key);
  Cell updated = found ? *found : Cell {};
  updated.index = static_cast<uint16_t> (value);

  // default cells are not stored
  if (updated.IsDefault ()) [[unlikely]] {
    sparse_.erase (key);
  }
  else {
    sparse_[key] = updated;
  }
}

int32_t Practice::GetValue (Team team, int32_t start, int32_t goal) const noexcept {
  if (!Valid (team, start, goal) || !initialized_ || Busy ()) [[unlikely]] {
    return 0;
  }

  if (start == goal) [[likely]] {
    return diag_[DiagPos (team, start)].value;
  }
  const auto *found = sparse_.find (SparseKey (team, start, goal));
  return found == nullptr ? 0 : found->value;
}

void Practice::SetValue (Team team, int32_t start, int32_t goal, int32_t value) noexcept {
  if (Busy () || !Valid (team, start, goal)) [[unlikely]] {
    return;
  }

  if (start == goal) [[likely]] {
    diag_[DiagPos (team, start)].value = static_cast<uint16_t> (value);
    return;
  }
  const auto key = SparseKey (team, start, goal);
  const auto *found = sparse_.find (key);
  Cell updated = found ? *found : Cell {};
  updated.value = static_cast<uint16_t> (value);

  // default cells are not stored
  if (updated.IsDefault ()) [[unlikely]] {
    sparse_.erase (key);
  }
  else {
    sparse_[key] = updated;
  }
}

int32_t Practice::GetDamage (Team team, int32_t start, int32_t goal) const noexcept {
  if (!Valid (team, start, goal) || !initialized_ || Busy () || !vistab.IsReady ()) [[unlikely]] {
    return 0;
  }

  // hot diagonal path
  if (start == goal) [[likely]] {
    return diag_[DiagPos (team, start)].damage;
  }
  const auto *found = sparse_.find (SparseKey (team, start, goal));
  return found == nullptr ? 0 : found->damage;
}

void Practice::SetDamage (Team team, int32_t start, int32_t goal, int32_t value) noexcept {
  if (Busy () || !Valid (team, start, goal) || !vistab.IsReady ()) [[unlikely]] {
    return;
  }

  // hot diagonal path
  if (start == goal) [[likely]] {
    diag_[DiagPos (team, start)].damage = static_cast<uint16_t> (value);
    return;
  }
  const auto key = SparseKey (team, start, goal);
  const auto *found = sparse_.find (key);
  Cell updated = found ? *found : Cell {};
  updated.damage = static_cast<uint16_t> (value);

  // default cells are not stored
  if (updated.IsDefault ()) [[unlikely]] {
    sparse_.erase (key);
  }
  else {
    sparse_[key] = updated;
  }
}

float Practice::GetDamageEx (Team team, int32_t start, int32_t goal, bool add_team_highest_damage) noexcept {
  auto damage = static_cast<float> (GetDamage (team, start, goal));

  if (add_team_highest_damage) {
    damage += GetTeamDamage<float> (team);
  }
  return damage;
}

void Practice::UpdateValue (Bot *bot, int damage) {
  // gets called each time a bot gets damaged by some enemy

  // storage is being updated on worker thread, drop the update
  if (Busy ()) [[unlikely]] {
    return;
  }
  if (graph.Length () < 1 || graph.HasChanged () || bot->chosen_goal_index_ < 0 || bot->prev_goal_index_ < 0) [[unlikely]] {
    return;
  }
  const auto health = static_cast<int> (bot->health_value_);

  // max goal value
  constexpr int kMaxGoalValue = PracticeLimit::kGoal;

  // only rate goal node on lethal damage; fixme: improve sniper/deadly-weapon weighting
  if (health - damage <= 0) [[unlikely]] {
    SetValue (bot->team_, bot->chosen_goal_index_, bot->prev_goal_index_,
      ystl::clamp (GetValue (bot->team_, bot->chosen_goal_index_, bot->prev_goal_index_) - health / 20, -kMaxGoalValue, kMaxGoalValue));
  }
}

void Practice::UpdateDamage (Bot *bot, edict_t *attacker, int damage) {
  // this function gets called each time a bot gets damaged by some enemy. stores the damage (team-specific) done by victim

  // storage is being updated on worker thread, drop the update
  if (Busy ()) [[unlikely]] {
    return;
  }
  if (!game.IsPlayerEntity (attacker)) [[unlikely]] {
    return;
  }

  const auto attacker_team = game.GetPlayerTeam (attacker);
  const auto victim_team = bot->team_;

  if (attacker_team == victim_team) [[unlikely]] {
    return;
  }
  if (damage < 20) {
    return; // do not collect damage less than 20, goal values included
  }
  constexpr int kMaxDamageValue = PracticeLimit::kDamage;

  // if these are bots also remember damage to rank destination of the bot
  bot->goal_value_ -= static_cast<float> (damage);
  auto bot_attacker = bots[attacker];

  if (bot_attacker != nullptr) {
    bot_attacker->goal_value_ += static_cast<float> (damage);
  }

  int attacker_index = graph.GetNearest (attacker->v.origin);
  int victim_index = bot->GetCurrentNodeIndex ();

  if (victim_index == kInvalidNodeIndex) [[unlikely]] {
    victim_index = bot->FindNearestNode ();
  }
  const auto update_damage = game.IsFakeClientEntity (attacker) ? 10 : 7;

  // store away the damage done
  const auto damage_value = ystl::clamp (GetDamage (bot->team_, victim_index, attacker_index) + damage / update_damage, 0, kMaxDamageValue);

  if (damage_value > GetTeamDamage (bot->team_)) [[unlikely]] {
    SetTeamDamage (bot->team_, damage_value);
  }
  SetDamage (bot->team_, victim_index, attacker_index, damage_value);
}

void Practice::Update () {
  worker.Enqueue ([this] () {
    SyncUpdate ();
  });
}

void Practice::SyncUpdate () {
  // called after round end to update the most dangerous nodes per team

  // no nodes, no practice used or nodes edited or being edited?
  if (!size_ || graph.HasChanged () || !vistab.IsReady ()) [[unlikely]] {
    return; // no action
  }
  busy_.store (true, ystl::MemoryOrder::relaxed);

  const auto n = size_;
  const auto nn = static_cast<uint32_t> (n) * static_cast<uint32_t> (n);
  const size_t diag_size = static_cast<size_t> (ystl::to_underlying (Team::Num)) * static_cast<size_t> (n);

  // best danger per diagonal slot, single pass over sparse cells
  ystl::Array<int32_t> best_index {};
  ystl::Array<int32_t> best_damage {};

  best_index.resize (diag_size);
  best_damage.resize (diag_size);

  best_index.fill (kInvalidNodeIndex);
  best_damage.fill (0);

  int32_t max_damage_observed = 0;

  for (const auto kv : sparse_) {
    const auto key = kv.first;
    const auto team = static_cast<int32_t> (key / nn);

    const auto rem = key % nn;
    const auto src = static_cast<int32_t> (rem / static_cast<uint32_t> (n));
    const auto dst = static_cast<int32_t> (rem % static_cast<uint32_t> (n));

    // skip stale keys from a resized graph
    if (team < 0 || team >= ystl::to_underlying (Team::Num) || src < 0 || src >= n || dst < 0 || dst >= n) {
      continue;
    }

    const auto damage = static_cast<int32_t> (kv.second.damage);

    if (damage == 0 || !vistab.Visible (src, dst)) {
      continue;
    }
    max_damage_observed = ystl::max (max_damage_observed, damage);

    const auto slot = static_cast<size_t> (team) * static_cast<size_t> (n) + static_cast<size_t> (src);

    if (damage > best_damage[slot]) {
      best_damage[slot] = damage;
      best_index[slot] = dst;
    }
  }

  // publish, always writing clears stale danger nodes
  for (size_t slot = 0; slot < diag_size; ++slot) {
    const auto dst = best_index[slot];

    if (dst == kInvalidNodeIndex || !graph.Exists (dst)) {
      diag_[slot].index = kInvalidIndex16;
    }
    else {
      diag_[slot].index = static_cast<uint16_t> (dst);
    }
  }
  constexpr auto kFullDamageVal = static_cast<int32_t> (PracticeLimit::kDamage);
  constexpr auto kHalfDamageVal = kFullDamageVal / 2;

  if (max_damage_observed > kFullDamageVal) {
    for (size_t slot = 0; slot < diag_size; ++slot) {
      diag_[slot].damage = static_cast<uint16_t> (ystl::clamp (static_cast<int32_t> (diag_[slot].damage) - kHalfDamageVal, 0, kFullDamageVal));
    }
    ystl::Array<uint32_t> drop {};

    for (auto kv : sparse_) {
      kv.second.damage = static_cast<uint16_t> (ystl::clamp (static_cast<int32_t> (kv.second.damage) - kHalfDamageVal, 0, kFullDamageVal));

      // collect defaulted cells for erase
      if (kv.second.IsDefault ()) {
        drop.push (kv.first);
      }
    }

    for (const auto &key : drop) {
      sparse_.erase (key);
    }
  }

  for (auto team = Team::Terrorist; team < Team::Num; ++team) {
    team_damage_[team] = static_cast<uint16_t> (ystl::clamp (team_damage_[team] - kHalfDamageVal, 1, kFullDamageVal));
  }

  // publish updated storage to main thread
  busy_.store (false, ystl::MemoryOrder::release);
}

void Practice::Save () {
  if (!size_ || !initialized_ || Busy ()) [[unlikely]] {
    return; // no action, storage is being updated on worker thread
  }
  const auto n = size_;

  // materialize non-default cells only
  ystl::Array<Entry> entries {};

  for (auto team = Team::Terrorist; team < Team::Num; ++team) {
    const auto team_idx = static_cast<uint16_t> (ystl::to_underlying (team));

    for (int i = 0; i < n; ++i) {
      const auto pos = DiagPos (team, i);
      const auto &cell = diag_[pos];

      if (!cell.IsDefault ()) [[unlikely]] {
        entries.push (Entry { .team = team_idx, .src = static_cast<uint16_t> (i), .dst = static_cast<uint16_t> (i), .cell = cell });
      }
    }
  }
  const auto nn = static_cast<uint32_t> (n) * static_cast<uint32_t> (n);

  for (const auto kv : sparse_) {
    const auto team = static_cast<uint16_t> (kv.first / nn);
    const auto rem = kv.first % nn;

    entries.push (Entry { .team = team,
      .src = static_cast<uint16_t> (rem / static_cast<uint32_t> (n)),
      .dst = static_cast<uint16_t> (rem % static_cast<uint32_t> (n)),
      .cell = kv.second });
  }
  // nothing to store, drop stale file from a resized graph
  if (entries.empty ()) [[unlikely]] {
    const auto path = bstor.BuildPath (StorageFile::Practice);

    if (ystl::plat.file_exists (path.chars ())) {
      ystl::plat.remove_file (path.chars ());
    }
    return;
  }
  bstor.Save<Entry> (entries);
}

void Practice::SyncLoad () {
  if (!graph.Length ()) [[unlikely]] {
    return; // no action
  }
  busy_.store (true, ystl::MemoryOrder::relaxed);

  // initialize sparse storage for current graph size
  Initialize ();

  ystl::Array<Entry> entries {};

  // restore clean storage if load fails as cleared data is not saved back
  if (bstor.Load<Entry> (entries)) {
    const auto n = size_;

    for (const auto &entry : entries) {
      // skip stale entries from a resized graph
      if (entry.team >= static_cast<uint16_t> (ystl::to_underlying (Team::Num)) || entry.src >= n || entry.dst >= n) {
        continue;
      }
      const auto team = static_cast<Team> (entry.team);

      if (entry.cell.IsDefault ()) {
        continue;
      }

      if (entry.src == entry.dst) {
        const auto pos = DiagPos (team, entry.src);
        diag_[pos] = entry.cell;
      }
      else {
        sparse_[SparseKey (team, entry.src, entry.dst)] = entry.cell;
      }
    }
  }
  else {
    Initialize ();
  }

  // publish loaded storage to main thread
  busy_.store (false, ystl::MemoryOrder::release);
}

void Practice::Load () {
  worker.Enqueue ([this] () {
    SyncLoad ();
  });
}

} // namespace bot
