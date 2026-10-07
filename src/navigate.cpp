//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

bool Bot::ShouldRushEndgameTime () const {
  // only relevant at the very end of the round
  if (!game_state.IsRoundTimeLow ()) {
    return false;
  }
  // low-health bots never rush into danger
  if (health_value_ < static_cast<float> (pev->max_health) * 0.35f) {
    return false;
  }
  // low-aggression (fear-suppressed) bots don't push the objective either
  if (agression_level_ < 0.5f) {
    return false;
  }
  switch (personality_) {
  case Personality::Rusher:
    return true; // aggressive - always rush

  case Personality::Normal:
    return rg.chance (50); // normal - random

  case Personality::Careful:
  default:
    return false; // defensive - never (any case)
  }
}

int Bot::FindBestGoal () {
  if (game.Is (GameFlags::ZombieMod) && is_creature_) {
    const auto &[ts, cts] = bots.CountTeamPlayers ();

    if (ts < graph.GetPoints (PointType::Terrorist).size<int> ()) {
      return FindRandomNode (PointType::Terrorist);
    }
    else if (ts < graph.GetPoints (PointType::Goal).size<int> ()) {
      return FindRandomNode (PointType::Goal);
    }
    return FindRandomNode ();
  }

  // chooses a destination (goal) node for a bot
  if (team_ == Team::Terrorist && game.MapIs (MapFlags::Demolition)) {
    auto result = FindBestGoalWhenBombAction ();

    if (graph.Exists (result)) {
      return result;
    }
  }

  // path finding behavior depending on map type
  float offensive = 0.0f;
  float defensive = 0.0f;

  ystl::SmallArray<int32_t> *offensive_nodes = nullptr;
  ystl::SmallArray<int32_t> *defensive_nodes = nullptr;

  switch (team_) {
  case Team::Terrorist:
    offensive_nodes = &graph.GetPoints (PointType::CT);
    defensive_nodes = &graph.GetPoints (PointType::Terrorist);
    break;

  case Team::CT:
  default:
    offensive_nodes = &graph.GetPoints (PointType::Terrorist);
    defensive_nodes = &graph.GetPoints (PointType::CT);
    break;
  }

  // terrorist carrying the c4?
  if (has_c4_ || is_vip_) {
    return FindGoalPost (GoalTactic::Goal, defensive_nodes, offensive_nodes);
  }
  else if (team_ == Team::CT && has_hostage_) {
    return FindBestGoalWhenHostageAction (defensive_nodes, offensive_nodes);
  }
  constexpr float kBehaviorBase = 30.0f;
  const auto difficulty = static_cast<float> (difficulty_);

  // personality-driven behavior modifiers - gives distinct character to each bot type
  float personality_offensive_mod = 0.0f;
  float personality_defensive_mod = 0.0f;

  switch (personality_) {
  case Personality::Rusher:
    // rushers are aggressive and fearless - high offensive, low defensive
    personality_offensive_mod = 40.0f;
    personality_defensive_mod = -30.0f;
    break;

  case Personality::Careful:
    // careful bots are cautious - low offensive, high defensive/camp tendency
    personality_offensive_mod = -20.0f;
    personality_defensive_mod = 35.0f;
    break;

  case Personality::Normal:
  default:
    // normal bots have balanced behavior with slight personality variance
    personality_offensive_mod = rg (-10.0f, 10.0f);
    personality_defensive_mod = rg (-10.0f, 10.0f);
    break;
  }

  offensive = agression_level_ * 100.0f + personality_offensive_mod;
  defensive = fear_level_ * 100.0f + personality_defensive_mod;

  if (game.MapIs (MapFlags::Assassination | MapFlags::HostageRescue)) {
    if (team_ == Team::Terrorist) {
      if (personality_ == Personality::Rusher) {
        defensive -= kBehaviorBase - difficulty * 5.0f;
        offensive += kBehaviorBase + difficulty * 5.0f;
      }
      else if (personality_ == Personality::Normal && rg.chance (20 + Skill () * 3 / 5)) {
        defensive -= kBehaviorBase;
        offensive += kBehaviorBase;
      }
      else {
        defensive += kBehaviorBase;
        offensive -= kBehaviorBase;
      }
    }
    else if (team_ == Team::CT) {
      // on hostage maps force more bots to save hostages, on assassination
      // maps cts (except the vip, handled elsewhere) rush to rescue the vip
      defensive -= kBehaviorBase - difficulty * 5.0f;
      offensive += kBehaviorBase + difficulty * 5.0f;
    }
  }
  else if (game.MapIs (MapFlags::Demolition) && team_ == Team::CT) {
    if (game_state.IsBombPlanted () && GetTaskId () != TaskId::EscapeFromBomb && !game_state.GetBombOrigin ().empty ()) {

      if (bots.HasBombSay (BombPlantedSay::ChatSay)) {
        PushChatMessage (Chat::Plant);
        bots.ClearBombSay (BombPlantedSay::ChatSay);
      }
      return chosen_goal_index_ = FindBombNode ();
    }
    defensive += kBehaviorBase + difficulty * 5.0f;
    offensive -= kBehaviorBase - difficulty * 5.0f;

    if (personality_ != Personality::Rusher) {
      defensive += 10.0f;
    }
  }
  else if (game.MapIs (MapFlags::Demolition) && team_ == Team::Terrorist) {
    // send some terrorists to guard planted bomb
    if (!defended_bomb_ && game_state.IsBombPlanted () && GetTaskId () != TaskId::EscapeFromBomb && game_state.GetBombTimeLeft () >= 15.0f) {

      return chosen_goal_index_ = FindDefendNode (game_state.GetBombOrigin ());
    }
    defensive -= kBehaviorBase - difficulty * 5.0f;
    offensive += kBehaviorBase + difficulty * 5.0f;
  }
  else if (game.MapIs (MapFlags::Escape)) {
    if (team_ == Team::Terrorist) {

      // rushers and higher-difficulty bots go straight for escape zone
      if (personality_ == Personality::Rusher || rg.chance (20 + Skill () * 3 / 5)) {
        return FindGoalPost (GoalTactic::Goal, defensive_nodes, offensive_nodes);
      }
      offensive += kBehaviorBase + difficulty * 5.0f;
      defensive -= kBehaviorBase - difficulty * 5.0f;
    }
    else if (team_ == Team::CT) {
      offensive -= kBehaviorBase - difficulty * 5.0f;
      defensive += kBehaviorBase + difficulty * 5.0f;
    }
  }

  // use varied random ranges for nuanced tactic selection
  float goal_desire = rg (0.0f, 70.0f) + offensive;
  float forward_desire = rg (0.0f, 50.0f) + offensive;
  float backoff_desire = rg (0.0f, 50.0f) + defensive;
  float camp_desire = rg (0.0f, 70.0f) + defensive;

  // personality strongly influences tactic preferences
  switch (personality_) {
  case Personality::Rusher:
    // rushers prefer direct offensive action, dislike camping
    forward_desire *= 1.4f;
    camp_desire *= 0.4f;
    goal_desire *= 1.2f;
    break;

  case Personality::Careful:
    // careful bots prefer camping and defensive positions
    camp_desire *= 1.5f;
    backoff_desire *= 1.3f;
    forward_desire *= 0.6f;
    break;

  case Personality::Normal:
  default:
    // normal bots have slight random variation in tactic preference this creates more natural behavior diversity
    if (rg.chance (50)) {
      forward_desire *= rg (1.0f, 1.2f);
    }
    else {
      camp_desire *= rg (1.0f, 1.2f);
    }
    break;
  }

  // sniper weapons dramatically increase camp desire for all personalities
  if (!UsesCampGun ()) {
    camp_desire = 0.0f;
  }
  else if (UsesSniper ()) {
    camp_desire *= rg (1.5f, 2.5f);
  }

  // round time is running out - rush the objective instead of playing passively
  if (ShouldRushEndgameTime ()) {
    goal_desire *= 1.5f;
    forward_desire *= 1.5f;
    backoff_desire *= 0.2f;
    camp_desire *= 0.1f;
  }

  auto tactic = GoalTactic::Defensive;
  auto tactic_choice = backoff_desire;

  if (camp_desire > tactic_choice) {
    tactic_choice = camp_desire;
    tactic = GoalTactic::Camp;
  }

  if (forward_desire > tactic_choice) {
    tactic_choice = forward_desire;
    tactic = GoalTactic::Offensive;
  }

  if (goal_desire > tactic_choice) {
    tactic_choice = goal_desire;
    tactic = GoalTactic::Goal;
  }
  return FindGoalPost (tactic, defensive_nodes, offensive_nodes);
}

int Bot::FindBestGoalWhenBombAction () {
  int result = kInvalidNodeIndex;

  if (!game_state.IsBombPlanted () && !cv_ignore_objectives) {
    game.SearchEntities ("classname", "weaponbox", [&] (edict_t *ent) {
      if (game.IsEntityModelMatches (ent, "backpack.mdl")) {
        result = graph.GetNearest (game.GetEntityOrigin (ent));

        if (graph.Exists (result)) {

          // if bomb entity is bot's ignore list, clear ignore list
          if (IsIgnoredItem (ent)) {
            ignored_items_.clear ();
          }
          return EntitySearchResult::Break;
        }
      }
      return EntitySearchResult::Continue;
    });

    // found one ?
    if (graph.Exists (result)) {
      return loosed_bomb_node_index_ = result;
    }

    // forcing terrorist bot to not move to another bomb spot
    if (in_bomb_zone_ && !has_progress_bar_ && has_c4_) {
      return graph.GetNearest (pev->origin, 1024.0f, NodeFlag::Goal);
    }
  }
  else if (!defended_bomb_) {
    const auto &bomb_origin = game_state.GetBombOrigin ();

    if (!bomb_origin.empty ()) {
      defended_bomb_ = true;

      result = FindDefendNode (bomb_origin);

      if (!graph.Exists (result)) {
        return chosen_goal_index_ = FindRandomNode ();
      }
      const auto &path = graph[result];

      const float bomb_timer = mp_c4timer.As<float> ();
      const float time_mid_blowup = game_state.GetTimeBombPlanted () + (bomb_timer * 0.5f + bomb_timer * 0.25f) -
                                    graph.CalculateTravelTime (pev->maxspeed, pev->origin, path.origin);

      if (time_mid_blowup > game.Time ()) {
        ClearTask (TaskId::MoveTo); // remove any move tasks

        StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, time_mid_blowup, true); // add/update camp task
        StartTask (TaskId::MoveTo, TaskPri::kMoveTo, result, 0.0f, true); // add/update move task

        // decide to duck or not to duck
        SelectCampButtons (result);

        if (rg.chance (90)) {
          PushRadioChat (RadioChat::DefendingBombsite);
        }
      }
      else {
        PushRadioChat (RadioChat::ShesGonnaBlow); // issue an additional radio message
      }
      return result;
    }
  }
  return result;
}

int Bot::FindBestGoalWhenHostageAction (ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive) {
  bool has_more_hostages_around = false;

  // round time is running out - beeline for the closest rescue zone with the hostage
  if (ShouldRushEndgameTime ()) {
    int nearest_rescue = kInvalidNodeIndex;
    float nearest_dist_sq = kInfiniteDistance;

    for (const auto &point : graph.GetPoints (PointType::Rescue)) {
      if (!graph.Exists (point) || !IsReachableNode (point)) {
        continue;
      }
      const float distance_sq = graph[point].origin.distance_sq (pev->origin);

      if (distance_sq < nearest_dist_sq) {
        nearest_dist_sq = distance_sq;
        nearest_rescue = point;
      }
    }

    if (graph.Exists (nearest_rescue)) {
      return chosen_goal_index_ = nearest_rescue;
    }
  }

  // try to search nearby-unused hostage, and if so, go to next goal
  if (game_state.HasInterestingEntities ()) {
    const auto &interesting = game_state.GetInterestingEntities ();

    // search world for hostages
    for (const auto &item : interesting) {
      if (item.kind != EntityKind::Hostage) {
        continue;
      }
      const auto ent = item.ent;
      bool hostage_in_use = false;

      // do not steal hostages from other bots
      for (const auto &other : bots) {
        if (!other.is_alive_) {
          continue;
        }

        for (const auto &hostage : other.hostages_) {
          if (hostage == ent) {
            hostage_in_use = true;
            break;
          }
        }
      }

      // in-use, skip
      if (hostage_in_use) {
        continue;
      }
      const ystl::Vector origin = game.GetEntityOrigin (ent);

      // too far, go to rescue point
      if (origin.distance_sq (pev->origin) > ystl::sqrf (1024.0f)) {
        continue;
      }
      has_more_hostages_around = true;
      break;
    }
  }
  return FindGoalPost (has_more_hostages_around ? GoalTactic::Goal : GoalTactic::RescueHostage, defensive, offensive);
}

int Bot::FindGoalPost (GoalTactic tactic, ystl::SmallArray<int32_t> *defensive, ystl::SmallArray<int32_t> *offensive) {
  int goal_choices[4] {};
  float goal_distances[4] {};

  for (int i = 0; i < 4; ++i) {
    goal_choices[i] = kInvalidNodeIndex;
    goal_distances[i] = kInfiniteDistance;
  }

  if (tactic == GoalTactic::Defensive && !(*defensive).empty ()) { // careful goal
    PostProcessGoals (*defensive, goal_choices);
  }
  else if (tactic == GoalTactic::Camp && !graph.GetPoints (PointType::Camp).empty ()) { // camp node goal
    // pickup sniper points if possible for sniping bots
    if (!graph.GetPoints (PointType::Sniper).empty () && UsesSniper ()) {
      PostProcessGoals (graph.GetPoints (PointType::Sniper), goal_choices);
    }
    else {
      PostProcessGoals (graph.GetPoints (PointType::Camp), goal_choices);
    }
  }
  else if (tactic == GoalTactic::Offensive && !(*offensive).empty ()) { // offensive goal
    PostProcessGoals (*offensive, goal_choices);
  }
  else if (tactic == GoalTactic::Goal && !graph.GetPoints (PointType::Goal).empty ()) { // map goal node
    // force bomber to select closest goal
    if (is_vip_ || has_c4_) {
      for (const auto &point : graph.GetPoints (PointType::Goal)) {
        if (!graph.Exists (point)) {
          continue;
        }

        if (mode_walls.IsNodeBlocked (graph[point].origin)) {
          continue;
        }
        const float distance_sq = graph[point].origin.distance_sq (pev->origin);

        if (distance_sq > ystl::sqrf (2048.0f)) {
          continue;
        }

        if (IsGroupOfEnemies (graph[point].origin)) {
          continue;
        }

        // replace the worst (farthest) of the top-4
        int worst = 0;

        for (int count = 1; count < 4; ++count) {
          if (goal_distances[count] > goal_distances[worst]) {
            worst = count;
          }
        }

        if (distance_sq < goal_distances[worst]) {
          goal_choices[worst] = point;
          goal_distances[worst] = distance_sq;
        }
      }

      for (auto &choice : goal_choices) {
        if (choice == kInvalidNodeIndex) {
          choice = FindRandomNode (PointType::Goal);
        }
      }
    }
    else {
      PostProcessGoals (graph.GetPoints (PointType::Goal), goal_choices);
    }
  }
  else if (tactic == GoalTactic::RescueHostage && !graph.GetPoints (PointType::Rescue).empty ()) {
    // when low on time, force the absolute closest rescue zone, no randomization
    if (ShouldRushEndgameTime ()) {
      int nearest_rescue = kInvalidNodeIndex;
      float nearest_dist_sq = kInfiniteDistance;

      for (const auto &point : graph.GetPoints (PointType::Rescue)) {
        if (!graph.Exists (point)) {
          continue;
        }

        if (mode_walls.IsNodeBlocked (graph[point].origin)) {
          continue;
        }
        const float distance_sq = graph[point].origin.distance_sq (pev->origin);

        if (distance_sq < nearest_dist_sq) {
          nearest_dist_sq = distance_sq;
          nearest_rescue = point;
        }
      }

      if (graph.Exists (nearest_rescue)) {
        return chosen_goal_index_ = nearest_rescue;
      }
    }

    // force ct with hostage(s) to select closest rescue goal
    for (const auto &point : graph.GetPoints (PointType::Rescue)) {
      if (!graph.Exists (point)) {
        continue;
      }

      if (mode_walls.IsNodeBlocked (graph[point].origin)) {
        continue;
      }
      const float distance_sq = graph[point].origin.distance_sq (pev->origin);

      // replace the worst (farthest) of the top-4
      int worst = 0;

      for (int count = 1; count < 4; ++count) {
        if (goal_distances[count] > goal_distances[worst]) {
          worst = count;
        }
      }

      if (distance_sq < goal_distances[worst]) {
        goal_choices[worst] = point;
        goal_distances[worst] = distance_sq;
      }
    }

    for (auto &choice : goal_choices) {
      if (choice == kInvalidNodeIndex) {
        choice = FindRandomNode (PointType::Rescue);
      }
    }
  }
  EnsureCurrentNodeIndex ();

  // rusher bots does not care any danger
  if (personality_ == Personality::Rusher) {
    const auto random_goal = goal_choices[rg (0, 3)];

    if (graph.Exists (random_goal)) {
      return chosen_goal_index_ = random_goal;
    }
  }

  // count valid goals (exclude negative/invalid values)
  int valid_count = 0;

  for (int i = 0; i < 4; ++i) {
    if (goal_choices[i] != kInvalidNodeIndex) {
      ++valid_count;
    }
  }

  // sort only valid goals by practice value (ascending = closer/better goals first)
  ystl::bubble_sort (goal_choices, static_cast<size_t> (valid_count), [this] (int a, int b) {
    return practice.GetValue (team_, current_node_index_, a) < practice.GetValue (team_, current_node_index_, b);
  });

  // the most worst case
  if (goal_choices[0] == kInvalidNodeIndex) {
    return chosen_goal_index_ = FindRandomNode ();
  }
  return chosen_goal_index_ = goal_choices[0]; // return and store goal
}

void Bot::PostProcessGoals (const ystl::SmallArray<int32_t> &goals, int result[]) {
  // this function filters the goals, so new goal is not bot's old goal, and array of goals doesn't contain duplicate goals

  auto is_recent_or_historical = [&] (int index) -> bool {
    if (prev_goal_index_ == index || previous_nodes_[0] == index) {
      return true;
    }

    // check if historical goal (exact match)
    if (goal_history_.contains (index)) {
      return true;
    }

    // area-based filtering: avoid goals too close to recent goals
    if (graph.Exists (index)) {
      for (const auto &hg : goal_history_) {
        if (graph.Exists (hg)) {
          const float distance_sq = graph[hg].origin.distance_sq (graph[index].origin);

          // avoid goals within 512 units of recent goals for better diversity
          if (distance_sq < ystl::sqrf (512.0f)) {
            return true;
          }
        }
      }
    }

    if (ystl::find (result, 4, index) != nullptr) {
      return true;
    }
    return IsOccupiedNode (index, true);
  };

  // copy and shuffle goals for random but non-repeating iteration
  ystl::SmallArray<int32_t> candidates {};

  candidates.insert (0, goals);
  candidates.shuffle ();

  int filled = 0;

  // first pass: pick non-historical, non-duplicate goals
  for (int i = 0; i < candidates.size<int> () && filled < 4; ++i) {
    // skip goals sealed inside a closed mode wall
    if (graph.Exists (candidates[i]) && mode_walls.IsNodeBlocked (graph[candidates[i]].origin)) {
      continue;
    }

    if (!is_recent_or_historical (candidates[i])) {
      result[filled++] = candidates[i];
    }
  }

  // second pass: if not enough, accept historical goals to fill remaining slots
  for (int i = 0; i < candidates.size<int> () && filled < 4; ++i) {
    // keep the wall filter: never re-add goals sealed inside a closed mode wall
    if (graph.Exists (candidates[i]) && mode_walls.IsNodeBlocked (graph[candidates[i]].origin)) {
      continue;
    }
    if (ystl::find (result, static_cast<size_t> (filled), candidates[i]) == nullptr) {
      result[filled++] = candidates[i];
    }
  }
}

bool Bot::HasActiveGoal () {
  const auto goal = Task ()->data;

  if (goal == kInvalidNodeIndex) { // not decided about a goal
    return false;
  }
  else if (goal == current_node_index_) { // no nodes needed
    if (goal_history_.size () > kMaxNodeLinks / 2) {
      goal_history_.pop_front ();
    }
    goal_history_.emplace_last (goal);

    return true;
  }
  else if (path_walk_.Empty ()) { // no path calculated
    return false;
  }
  const int32_t last_node = path_walk_.Last ();

  // got path - check if still valid
  return goal == last_node;
}

void Bot::ResetCollision () {
  probe_timer_.invalidate ();

  collision_state_ = CollisionState::Undecided;
  coll_state_index_ = 0;

  ystl::fill (collide_moves_, CollisionState::Undecided);
}

void Bot::IgnoreCollision () {
  ResetCollision ();

  last_coll_timer_.start (0.65f);
  first_collide_timer_.invalidate ();
  stuck_accumulated_time_ = 0.0f;
  check_terrain_ = false;
}

void Bot::DoPlayerAvoidance (const ystl::Vector &normal) {
  if (IsOnLadder () || pev->solid == SOLID_NOT || cv_has_team_semiclip || game.Is (GameFlags::FreeForAll) || HasUninterruptibleTask ()) {
    return; // no player avoiding when with semiclip plugin
  }

  // validate existing hindrance is still alive
  if (!game.IsNullEntity (hindrance_) && !game.IsAliveEntity (hindrance_)) {
    hindrance_ = nullptr;
    avoid_strafe_dir_ = 0.0f;
    avoid_commit_timer_.invalidate ();
  }

  edict_t *new_hindrance = nullptr;

  // adaptive threshold based on bot speed and difficulty
  const float base_threshold = 64.0f + static_cast<float> (5 - ystl::to_underlying (difficulty_)) * 8.0f; // 64-96 range
  float distance_sq = ystl::sqrf (base_threshold);

  auto clear_camp = [&] (edict_t *ent) {
    auto bot = bots[ent];

    if (bot) {
      const auto tid = bot->GetTaskId ();
      const auto tid2 = GetTaskId ();

      if ((current_node_index_ == bot->current_node_index_) && (tid == TaskId::Camp || tid == TaskId::Hide || tid == TaskId::Pause)) {
        bot->CompleteTask ();
        bot->FindValidNode ();
      }

      if ((current_node_index_ == bot->current_node_index_) && (tid2 == TaskId::Camp || tid2 == TaskId::Hide || tid2 == TaskId::Pause)) {
        CompleteTask ();
        FindValidNode ();
      }
    }
  };
  const auto own_prio = bots.GetPlayerPriority (Ent ());

  // accumulate repulsion from all nearby higher-priority teammates
  ystl::Vector repulsion_force {};
  float nearest_dist_sq = ystl::sqrf (base_threshold);
  int near_count = 0;

  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }
    const auto nearest_distance_sq = client.ent->v.origin.distance_sq (pev->origin);

    if (nearest_distance_sq >= ystl::sqrf (base_threshold)) {
      continue;
    }
    const auto other_prio = bots.GetPlayerPriority (client.ent);

    // skip lower priority players - they should avoid us
    if (own_prio > other_prio) {
      clear_camp (Ent ());
      continue;
    }

    // accumulate repulsion from all higher-priority bots within threshold
    constexpr float kMinRepulsionDistSq = ystl::sqrf (1.0f);
    const float weight = 1.0f / ystl::max (nearest_distance_sq, kMinRepulsionDistSq);
    const ystl::Vector away = (pev->origin - client.ent->v.origin).normalize2d ();

    repulsion_force += away * weight;
    ++near_count;

    // track nearest for camp-clearing and stuck-copy
    if (nearest_distance_sq < nearest_dist_sq) {
      new_hindrance = client.ent;
      nearest_dist_sq = nearest_distance_sq;
    }
  }

  // if hindrance changed, reset avoidance commitment
  if (hindrance_ != new_hindrance) {
    avoid_strafe_dir_ = 0.0f;
    avoid_commit_timer_.invalidate ();
  }
  hindrance_ = new_hindrance;

  // found somebody?
  if (game.IsNullEntity (hindrance_)) {
    return;
  }

  // compute prediction values for position and distance checks
  const float interval = frame_interval_ * (!IsDucking () && pev->velocity.length_sq2d () > 0.0f ? 6.0f : 2.0f);

  ystl::Vector right {}, forward {};
  move_angles_.angle_vectors (&forward, &right, nullptr);

  ystl::Vector predict = pev->origin + forward * pev->maxspeed * interval;
  predict += right * strafe_speed_ * interval;
  predict += pev->velocity * interval;

  const auto moved_distance_sq = hindrance_->v.origin.distance_sq (predict);
  const auto next_frame_distance_sq = pev->origin.distance_sq (hindrance_->v.origin + hindrance_->v.velocity * interval);

  // if we're stuck with a hindrance, probably we're in bad place, find path to single goal for both bots
  if (IsStuckState ()) {
    auto other = bots[hindrance_];

    if (other != nullptr && !other->IsStuckState ()) {
      Task ()->data = other->Task ()->data;
      prev_goal_index_ = other->prev_goal_index_;
      chosen_goal_index_ = other->chosen_goal_index_;

      auto dest_index = Task ()->data;

      if (!graph.Exists (dest_index)) {
        dest_index = chosen_goal_index_;
      }

      if (!graph.Exists (dest_index)) {
        dest_index = prev_goal_index_;
      }

      if (graph.Exists (dest_index)) {
        FindPath (current_node_index_, dest_index, path_type_);
      }
    }
  }

  // compute net avoidance direction from summed repulsion or fall back to per-hindrance prediction
  ystl::Vector dir {};
  distance_sq = nearest_dist_sq;

  if (near_count > 0 && !repulsion_force.empty ()) {
    dir = repulsion_force.normalize2d ();
  }
  else if (distance_sq > ystl::sqrf (8.0f)) {
    const ystl::Vector hind_predict = hindrance_->v.origin + hindrance_->v.velocity * interval;
    dir = (predict - hind_predict).normalize2d ();
  }

  // vertical awareness - check height difference
  const float height_diff = hindrance_->v.origin.z - pev->origin.z;

  // adjust avoidance distance based on vertical separation
  float avoid_distance_near = 64.0f;
  float avoid_distance_far = 96.0f;

  if (ystl::abs (height_diff) > 72.0f) {
    avoid_distance_near *= 0.7f;
    avoid_distance_far *= 0.7f;
  }

  // is player that near now or in future that we need to steer away?
  if (moved_distance_sq <= ystl::sqrf (avoid_distance_near) ||
      (distance_sq <= ystl::sqrf (avoid_distance_far) && next_frame_distance_sq < distance_sq)) {
    // if we already committed to an avoidance direction, keep it until commitment expires
    if (!avoid_commit_timer_.elapsed ()) {
      SetStrafeSpeed (normal, avoid_strafe_dir_);
    }
    else {
      if (!dir.empty ()) {
        const float dot_right = dir | right.normalize2d ();

        // dead-zone to prevent direction flipping when hindrance is nearly straight ahead
        if (ystl::abs (dot_right) > 0.1f) {
          avoid_strafe_dir_ = dot_right > 0.0f ? pev->maxspeed : -pev->maxspeed;
        }
        else if (ystl::fzero (avoid_strafe_dir_)) {
          avoid_strafe_dir_ = pev->maxspeed;
        }
      }
      else {
        if (ystl::fzero (avoid_strafe_dir_)) {
          avoid_strafe_dir_ = pev->maxspeed;
        }
      }

      avoid_commit_timer_.start (0.4f);
      SetStrafeSpeed (normal, avoid_strafe_dir_);
    }

    // only back off if hindrance is approaching us (closing distance), not just moving
    if (!dir.empty () && distance_sq < ystl::sqrf (96.0f) && next_frame_distance_sq < distance_sq) {
      if ((dir | forward.normalize2d ()) < 0.0f) {
        move_speed_ = -pev->maxspeed;
      }
    }

    // reset collision state so checkterrain doesn't consider us stuck from avoidance movement
    first_collide_timer_.invalidate ();
    ResetCollision ();
  }
  else {
    // clear commitment when no longer avoiding
    avoid_strafe_dir_ = 0.0f;
    avoid_commit_timer_.invalidate ();
  }
}

bool Bot::DetectStuckStatus (const ystl::Vector &dir_normal) {
  // determine if bot is stuck

  // special case for creatures taking damage, but only when actually moving under fire
  if (last_damage_timestamp_ >= game.Time () && is_creature_ && pev->velocity.length_sq2d () > ystl::sqrf (20.0f)) {
    last_coll_timer_.start (ystl::max (0.0f, last_damage_timestamp_ + 0.2f - game.Time ()));
    first_collide_timer_.invalidate ();
    return false;
  }

  // check if moved enough previously
  if (moved_distance_ < kMinMovedDistance && prev_speed_ > 20.0f) {
    prev_timer_.start (0.0f);
    stuck_timer_.start (kStuckLatchDuration);

    if (!first_collide_timer_.started ()) {
      first_collide_timer_.start (0.2f);
    }
    stuck_accumulated_time_ = 0.0f;
    return true;
  }

  // cumulative stuck timeout: if bot consistently moves very slowly while trying to go
  if (move_speed_ > 20.0f && moved_distance_ < kStuckMoveThreshold) {
    stuck_accumulated_time_ += frame_interval_;

    if (stuck_accumulated_time_ >= kStuckMaxDuration) {
      stuck_timer_.start (kStuckLatchDuration);
      stuck_accumulated_time_ = 0.0f;

      if (!first_collide_timer_.started ()) {
        first_collide_timer_.start (0.2f);
      }
      return true;
    }
  }
  else {
    stuck_accumulated_time_ = 0.0f;
  }

  // check for ladder path
  bool is_on_ladder_path = has_flag (path_flags_, NodeFlag::Ladder) && IsPreviousLadder ();

  // test if there's something ahead blocking the way
  if (!is_on_ladder_path && !IsOnLadder ()) {
    auto blocker = IsBlockedForward (dir_normal);

    // if blocked by a breakable, schedule shooting instead of stuck
    if (blocker != nullptr && game.IsBreakableEntity (blocker) && !IsIgnoredBreakable (blocker) && IsBreakableAllowed ()) {
      breakable_entity_ = blocker;
      breakable_origin_ = game.GetEntityOrigin (blocker);
      StartTask (TaskId::ShootBreakable, TaskPri::kShootBreakable, kInvalidNodeIndex, 0.0f, false);
      first_collide_timer_.invalidate ();
      return false;
    }

    if (blocker != nullptr) {
      if (!first_collide_timer_.started ()) {
        first_collide_timer_.start (0.2f);
      }
      else if (first_collide_timer_.elapsed ()) {
        stuck_timer_.start (kStuckLatchDuration);
        return true;
      }
    }
    else {
      first_collide_timer_.invalidate ();
    }
  }
  else {
    first_collide_timer_.invalidate ();
  }
  return false;
}

void Bot::ComputeCollisionWeights (const ystl::Vector &dir_normal, CollisionWeights &weights) {
  // compute weights for each collision response option

  Trace::Result tr {};
  CollisionProbe bits = CollisionProbe::None;

  // determine available probe options based on environment
  if (IsOnLadder ()) {
    bits |= CollisionProbe::Strafe;
  }
  else if (IsInWater ()) {
    bits |= (CollisionProbe::Strafe | CollisionProbe::Jump);
  }
  else {
    bits |= (CollisionProbe::Strafe | CollisionProbe::Jump | CollisionProbe::Duck);
  }

  // initialize all weights
  for (int i = 0; i < kNumCollisionResponses; ++i) {
    weights[i].weight = 0;
  }

  // helper lambda for height difference check
  auto is_similar_height = [&] () -> bool {
    return ystl::abs (dest_origin_.z - pev->origin.z) < 24.0f;
  };

  // refresh strafe direction periodically for human-like movement
  if (avoid_strafe_change_timer_.elapsed ()) {

    // randomly choose new strafe direction and time until next change
    terrain_strafe_dir_ = rg.chance (50) ? 1.0f : -1.0f;
    avoid_strafe_change_timer_.start (rg (0.55f, 0.85f));
  }

  bool dest_visible = false;

  // jump weight calculation
  if (has_flag (bits, CollisionProbe::Jump)) {
    auto &jump_weight = weights[CollisionState::Jump].weight;

    if (CanJumpUp (dir_normal)) {
      jump_weight += 8;
    }

    if (dest_origin_.z >= pev->origin.z + 18.0f) {
      jump_weight += 5;
    }
    dest_visible = SeesEntity (dest_origin_);

    if (dest_visible) {
      const ystl::Vector right = move_angles_.right ();

      ystl::Vector src = GetEyesPos () + right * 15.0f;
      ystl::Vector dst = dest_origin_;

      trace.Line (src, dst, TraceIgnore::Everything, Ent (), &tr);

      if (tr.fraction >= 1.0f) {
        src = GetEyesPos () - right * 15.0f;
        trace.Line (src, dst, TraceIgnore::Everything, Ent (), &tr);

        if (tr.fraction >= 1.0f) {
          jump_weight += 5;
        }
      }
    }
    // check for wall in movement direction - only add weight if there's a real obstacle
    ystl::Vector src = IsDucking () ? pev->origin : pev->origin + ystl::Vector (0.0f, 0.0f, -17.0f);
    ystl::Vector dst = src + dir_normal * 30.0f;

    trace.Line (src, dst, TraceIgnore::Everything, Ent (), &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      // only add jump weight if the obstacle is tall enough to require jumping check if there's clearance above the obstacle
      ystl::Vector check_src = dst;
      ystl::Vector check_dst = dst + ystl::Vector (0.0f, 0.0f, 45.0f);

      trace.Line (check_src, check_dst, TraceIgnore::Everything, Ent (), &tr);

      if (tr.fraction >= 1.0f) {
        jump_weight += 6;
      }
    }

    // penalty: jumping is often unnecessary when strafing would work
    if (is_similar_height ()) {
      jump_weight -= 12;
    }
  }

  // strafe weight calculation
  if (has_flag (bits, CollisionProbe::Strafe)) {
    ystl::Vector right {}, forward {};
    move_angles_.angle_vectors (&forward, &right, nullptr);

    const ystl::Vector dir_to_point = (pev->origin - dest_origin_).normalize2d ();
    const ystl::Vector right_side = right.normalize2d ();

    bool dir_left = (dir_to_point | right_side) > 0.0f;
    bool dir_right = !dir_left;

    const ystl::Vector test_dir = move_speed_ > 0.0f ? forward : -forward;
    constexpr float kBlockDistance = 96.0f;

    // check which sides are blocked
    bool blocked_left = false, blocked_right = false;

    ystl::Vector src = pev->origin + right * kBlockDistance;
    ystl::Vector dst = src + test_dir * kBlockDistance;

    trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);
    blocked_right = !ystl::fequal (tr.fraction, 1.0f);

    src = pev->origin - right * kBlockDistance;
    dst = src + test_dir * kBlockDistance;

    trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);
    blocked_left = !ystl::fequal (tr.fraction, 1.0f);

    // strafe left weight
    auto &left_weight = weights[CollisionState::StrafeLeft].weight;

    if (dir_left) {
      left_weight += 12;
    }
    else {
      left_weight -= 3;
    }
    if (blocked_left) {
      left_weight -= 10;
    }
    else {
      left_weight += 5;
    }

    // strafe right weight
    auto &right_weight = weights[CollisionState::StrafeRight].weight;

    if (dir_right) {
      right_weight += 12;
    }
    else {
      right_weight -= 3;
    }
    if (blocked_right) {
      right_weight -= 10;
    }
    else {
      right_weight += 5;
    }

    // additional bonus when destination is at similar height (strafing preferred)
    if (is_similar_height ()) {
      if (dir_left && !blocked_left) {
        weights[CollisionState::StrafeLeft].weight += 8;
      }
      if (dir_right && !blocked_right) {
        weights[CollisionState::StrafeRight].weight += 8;
      }
    }

    // bias strafe direction so bots alternate left and right
    if (terrain_strafe_dir_ > 0.0f) {
      // prefer strafing right
      weights[CollisionState::StrafeRight].weight += 15;
      weights[CollisionState::StrafeLeft].weight -= 5;
    }
    else {
      // prefer strafing left
      weights[CollisionState::StrafeLeft].weight += 15;
      weights[CollisionState::StrafeRight].weight -= 5;
    }
  }

  // duck weight calculation
  if (has_flag (bits, CollisionProbe::Duck)) {
    auto &duck_weight = weights[CollisionState::Duck].weight;

    if (CanDuckUnder (dir_normal)) {
      duck_weight += 10;
    }
    if ((dest_origin_.z + 36.0f <= pev->origin.z) && (dest_visible || SeesEntity (dest_origin_))) {
      duck_weight += 5;
    }
    // penalty: ducking is often unnecessary when strafing would work
    if (is_similar_height ()) {
      duck_weight -= 8;
    }
  }
}

void Bot::ExecuteCollisionResponse () {
  // execute the selected collision response

  if (coll_state_index_ >= kNumCollisionResponses) {
    return;
  }

  switch (collide_moves_[coll_state_index_]) {
  case CollisionState::Jump:
    if ((IsOnFloor () || IsInWater ()) && !IsOnLadder ()) {
      if (IsInWater () || !is_creature_ || last_damage_timestamp_ < game.Time () || has_flag (current_travel_flags_, PathFlag::Jump) ||
          IsStuckState ()) {
        pev->button |= IN_JUMP;
      }
    }
    break;

  case CollisionState::Duck:
    if (IsOnFloor () || IsInWater ()) {
      pev->button |= IN_DUCK;
    }
    break;

  case CollisionState::StrafeLeft:
    SetStrafeSpeedRaw (-pev->maxspeed);
    break;

  case CollisionState::StrafeRight:
    SetStrafeSpeedRaw (pev->maxspeed);
    break;

  default:
    break;
  }
}

void Bot::CheckTerrain (const ystl::Vector &dir_normal) {
  const float minimal_speed = IsDucking () ? 7.0f : 10.0f;

  constexpr float kProbeTestTime = 0.625f;

  // skip terrain response while player avoidance handles movement
  if (!game.IsNullEntity (hindrance_) && !avoid_commit_timer_.elapsed ()) {

    // still detect stuck status, but allow jump/duck responses during player avoidance
    if (DetectStuckStatus (dir_normal) && collision_state_ == CollisionState::Probing) {
      const auto state = collide_moves_[coll_state_index_];

      if (state == CollisionState::Jump || state == CollisionState::Duck) {
        ExecuteCollisionResponse ();
      }
    }
    return;
  }

  // check if movement is significant enough to evaluate
  if ((ystl::abs (move_speed_) < minimal_speed && ystl::abs (strafe_speed_) < minimal_speed) || !last_coll_timer_.elapsed () ||
      GetTaskId () == TaskId::Attack || GetTaskId () == TaskId::Camp) {
    return;
  }

  // detect if bot is stuck
  if (!DetectStuckStatus (dir_normal)) {
    // not stuck - maintain collision memory or reset
    if (probe_timer_.remaining_time () < -kProbeTestTime) {
      ResetCollision ();
    }
    else {
      const auto state = collide_moves_[coll_state_index_];

      if (state == CollisionState::Duck && (IsOnFloor () || IsInWater ())) {
        pev->button |= IN_DUCK;
      }
      else if (state == CollisionState::StrafeLeft) {
        SetStrafeSpeedRaw (-pev->maxspeed);
      }
      else if (state == CollisionState::StrafeRight) {
        SetStrafeSpeedRaw (pev->maxspeed);
      }
    }
    return;
  }

  // bot is stuck - decide what to do
  if (collision_state_ == CollisionState::Undecided) {

    // only compute if on valid surface
    if (IsOnFloor () || IsOnLadder () || IsInWater ()) {
      CollisionWeights weights {};

      // initialize state mapping
      weights[CollisionState::Jump].state = CollisionState::Jump;
      weights[CollisionState::StrafeLeft].state = CollisionState::StrafeLeft;
      weights[CollisionState::StrafeRight].state = CollisionState::StrafeRight;
      weights[CollisionState::Duck].state = CollisionState::Duck;

      ComputeCollisionWeights (dir_normal, weights);

      // sort by weight (highest first) - only sort the 4 collision response states
      ystl::bubble_sort (&weights[0], kNumCollisionResponses, [] (const CollisionWeight &a, const CollisionWeight &b) {
        return a.weight > b.weight;
      });

      // extract sorted states (only the 4 collision response states, skipping state machine states)
      for (int j = 0; j < kNumCollisionResponses; ++j) {
        collide_moves_[j] = weights[j].state;
      }

      // adaptive probe time based on weight confidence
      const auto best_weight = weights[0].weight;
      const auto worst_weight = weights[kNumCollisionResponses - 1].weight;
      const float confidence = static_cast<float> (best_weight) / static_cast<float> (ystl::max (1, best_weight + worst_weight + 1));

      float probe_time = kProbeTestTime;

      if (confidence > 0.8f) {
        probe_time = 0.3f;
      }
      else if (confidence < 0.4f) {
        probe_time = 0.8f;
      }

      probe_timer_.start (probe_time);
      collision_state_ = CollisionState::Probing;
      coll_state_index_ = 0;
    }
  }

  // execute probing states
  if (collision_state_ == CollisionState::Probing) {
    if (probe_timer_.elapsed ()) {
      coll_state_index_++;
      if (coll_state_index_ < kNumCollisionResponses) {
        probe_timer_.start (kProbeTestTime);
      }
      else {
        ResetCollision ();
        return;
      }
    }
    ExecuteCollisionResponse ();
  }
}

void Bot::CheckFall () {
  if (IsPreviousLadder () || has_flag (path_flags_, NodeFlag::Ladder) || IsOnLadder ()) {
    return;
  }

  const bool on_floor = IsOnFloor ();
  const float fall_vel = pev->flFallVelocity;

  if (!is_fall_down_) {
    if (on_floor) {
      // while on ground, continuously track pre-fall position and destination
      fall_down_point_[0] = pev->origin;

      if (current_node_index_ != kInvalidNodeIndex) {
        fall_down_point_[1] = path_origin_;
      }
      else if (!game.IsNullEntity (enemy_)) {
        fall_down_point_[1] = enemy_->v.origin;
      }
      else {
        fall_down_point_[1].clear ();
      }
    }
    else if (!IsInWater () && fall_vel > 100.0f) {
      is_fall_down_ = true;
    }
  }
  if (!is_fall_down_ || !on_floor || !fix_fall_timer_.elapsed ()) {
    return;
  }
  is_fall_down_ = false;

  // skip evaluation if we had no valid reference points
  if (fall_down_point_[0].empty () || fall_down_point_[1].empty ()) {
    return;
  }

  const float base_distance_sq = fall_down_point_[0].distance_sq (fall_down_point_[1]);
  const float now_distance_sq = pev->origin.distance_sq (fall_down_point_[1]);

  const float base_dist = ystl::sqrtf (base_distance_sq);
  const float now_dist = ystl::sqrtf (now_distance_sq);

  const float vert_drop_dest = fall_down_point_[1].z - pev->origin.z;
  const float vert_drop_start = fall_down_point_[0].z - pev->origin.z;

  const bool fell_far_horizontally = (base_dist >= 124.0f && now_dist > base_dist * 1.8f && now_dist > 146.0f);
  const bool fell_short_vertically = (vert_drop_dest > 138.0f || vert_drop_start > 138.0f);

  const bool missed_narrow_path = (current_node_index_ != kInvalidNodeIndex && now_dist > 32.0f && vert_drop_dest > 138.0f);
  const bool is_significant_fall = fell_far_horizontally || fell_short_vertically || missed_narrow_path;

  if (is_significant_fall) {
    if (graph.Exists (current_node_index_) && !IsReachableNode (current_node_index_)) {
      current_node_index_ = kInvalidNodeIndex;
      FindValidNode ();
    }
    fix_fall_timer_.start (1.0f);
  }
}

float Bot::ComputeLadderDesiredDistance () {
  // be lenient on ladder to ladder transitions to avoid slips
  if (path_walk_.HasNext ()) {
    const auto next_node_index = path_walk_.Next ();

    if (graph.Exists (next_node_index) && has_flag (graph[next_node_index].flags, NodeFlag::Ladder)) {

      // next node is also a ladder - check if we're roughly between prev and next nodes
      const float prev_z = graph.Exists (previous_nodes_[0]) ? graph[previous_nodes_[0]].origin.z : pev->origin.z;
      const float next_z = graph[next_node_index].origin.z;
      const float cur_z = pev->origin.z;

      // determine direction from previous node (more stable than velocity)
      const bool came_from_below = prev_z < cur_z;
      const bool came_from_above = prev_z > cur_z;

      // stay lenient when moving correctly or between nodes
      const bool between_nodes = (cur_z > ystl::min (prev_z, next_z) && cur_z < ystl::max (prev_z, next_z));
      const bool progressing_correctly = (came_from_below && next_z > cur_z) || (came_from_above && next_z < cur_z);

      if (progressing_correctly || between_nodes) {
        return ystl::sqrf (72.0f); // moving in correct direction or between nodes, be lenient
      }
    }
    else {
      // next node is not a ladder, allow jumping off
      return ystl::sqrf (48.0f);
    }
  }
  return ystl::sqrf (15.0f);
}

void Bot::MoveToGoal () {
  FindValidNode ();

  bool press_duck = false;

  // current node requires crouching - but only if headroom is actually limited
  if (has_flag (path_flags_, NodeFlag::Crouch)) {
    Trace::Result tr {};

    auto src = path_origin_;
    auto dst = path_origin_;

    src.z += 5.0f;
    dst.z += 72.0f; // player height

    trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

    // only duck if there's a real obstruction
    if (tr.fraction < 0.9f) {
      press_duck = true;
    }
  }

  // keep ducking across consecutive crouch nodes for smooth passage
  if (!press_duck && has_flag (path_flags_, NodeFlag::Crouch) && path_walk_.HasNext ()) {
    if (has_flag (graph[path_walk_.Next ()].flags, NodeFlag::Crouch)) {
      // only continue ducking if we're actually in a tight space
      Trace::Result tr {};

      auto src = pev->origin + ystl::Vector (0, 0, 5);
      auto dst = pev->origin + ystl::Vector (0, 0, 72);

      trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

      if (tr.fraction < 0.85f) {
        press_duck = true;
      }
    }
  }

  // lookahead: next node requires crouching (duck early to give engine time to shrink hull)
  if (!press_duck && path_walk_.HasNext ()) {
    if (has_flag (graph[path_walk_.Next ()].flags, NodeFlag::Crouch)) {
      const float dist_to_next = pev->origin.distance_sq2d (graph[path_walk_.Next ()].origin);

      // only duck when very close to the next crouch node (reduce from 32 to 24 units)
      if (dist_to_next < ystl::sqrf (24.0f)) {
        // verify there's actually low ceiling ahead before ducking early
        Trace::Result tr {};

        auto src = pev->origin + ystl::Vector (0, 0, 5);
        auto dst = pev->origin + ystl::Vector (0, 0, 72);

        trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

        if (tr.fraction < 0.9f) {
          press_duck = true;
        }
      }
    }
  }

  // apply sticky ducking to ensure hull shrinks properly
  if (press_duck) {
    pev->button |= IN_DUCK;
    duck_timer_.start (0.15f);
  }
  last_used_nodes_timer_.start ();

  // special movement for swimming here
  if (IsInWater ()) {
    // check if we need to go forward or back press the correct buttons
    if (IsInFov (dest_origin_ - GetEyesPos ()) > 90.0f) {
      pev->button |= IN_BACK;
    }
    else {
      pev->button |= IN_FORWARD;
    }

    if (move_angles_.x > 60.0f) {
      pev->button |= IN_DUCK;
    }
    else if (move_angles_.x < -60.0f) {
      pev->button |= IN_JUMP;
    }
  }
  else if (IsOnLadder ()) {
    pev->button |= IN_FORWARD;
  }
}

void Bot::ResetMovement () {
  pev->button = 0;

  move_speed_ = 0.0f;
  strafe_speed_ = 0.0f;
  move_angles_.clear ();
}

void Bot::TranslateInput () {
  // no shooting allowed, if yb_dont_shoot is enabled
  if (cv_dont_shoot) {
    pev->button &= ~IN_ATTACK;
  }

  if (!duck_timer_.elapsed ()) {
    pev->button |= IN_DUCK;
  }

  if (pev->button & IN_JUMP) {
    jump_time_ = game.Time ();
  }

  if (jump_time_ + 0.85f > game.Time ()) {
    if (!IsOnFloor () && !IsInWater () && !IsOnLadder ()) {
      pev->button |= IN_DUCK;
    }
  }

  if (!(pev->button & (IN_FORWARD | IN_BACK))) {
    if (move_speed_ > 0.0f) {
      pev->button |= IN_FORWARD;
    }
    else if (move_speed_ < 0.0f) {
      pev->button |= IN_BACK;
    }
  }

  // strafing on a ladder slides the bot off sideways, drop it at the output stage
  if (IsOnLadder ()) {
    strafe_speed_ = 0.0f;
    pev->button &= ~(IN_MOVELEFT | IN_MOVERIGHT);
  }

  if (!(pev->button & (IN_MOVELEFT | IN_MOVERIGHT))) {
    if (strafe_speed_ > 0.0f) {
      pev->button |= IN_MOVERIGHT;
    }
    else if (strafe_speed_ < 0.0f) {
      pev->button |= IN_MOVELEFT;
    }
  }
}

bool Bot::IsWalkableAscent (const ystl::Vector &src, const ystl::Vector &dst) {
  // engine step height, anything above needs a jump
  const float step = sv_stepsize.As<float> ();

  if (step <= 0.0f) {
    return false;
  }
  const ystl::Vector delta = dst - src;

  // pure step-up without horizontal travel
  if (delta.length2d () < 1.0f) {
    return delta.z <= step;
  }
  Trace::Result tr {};

  // stepped ground follow, reject sudden rises above step height
  ystl::Vector check = src, down = src;
  down.z -= 1000.0f;

  trace.Line (check, down, TraceIgnore::Monsters, Ent (), &tr);
  float last_height = tr.fraction * 1000.0f;

  float distance_sq = dst.distance_sq (check);
  const ystl::Vector direction = delta.normalize ();

  // dense sampling, thin lips fall between sparse samples
  while (distance_sq > ystl::sqrf (5.0f)) {
    check = check + direction * 5.0f;
    down = check;
    down.z -= 1000.0f;

    trace.Line (check, down, TraceIgnore::Monsters, Ent (), &tr);
    const float height = tr.fraction * 1000.0f;

    if (height - last_height > step) {
      return false;
    }
    last_height = height;
    distance_sq = dst.distance_sq (check);
  }
  return true;
}

bool Bot::UpdateNavigation () {
  // this function is a main path navigation

  // check if we need to find a node
  if (current_node_index_ == kInvalidNodeIndex) {
    FindValidNode ();
    SetPathOrigin ();

    nav_timer_.start ();
  }
  dest_origin_ = path_origin_;

  // this node has additional travel flags - care about them
  if (HasJumpTravelFlag ()) {

    // bot is not jumped yet?
    if (!jump_finished_) {

      // if bot's on the ground or on the ladder we're free to jump
      if (IsOnFloor () || IsOnLadder ()) {
        if (desired_velocity_.length2d () > 0.0f) {
          pev->velocity = desired_velocity_;
        }
        else if (graph.IsAnalyzed () || analyzer.IsAnalyzed ()) {
          auto feet = pev->origin + pev->mins;
          auto node =
            ystl::Vector { path_origin_.x, path_origin_.y, path_origin_.z - (has_flag (path_flags_, NodeFlag::Crouch) ? 18.0f : 36.0f) };

          if (feet.z > pev->origin.z) {
            feet = pev->origin + pev->maxs;
          }
          feet = { pev->origin.x, pev->origin.y, feet.z };

          // player ballistics, grenade solvers overshoot short hops
          auto velocity = CalcJumpVelocity (feet, node);

          if (velocity.length2d () > 0.0f) {
            pev->velocity = velocity;
            pev->velocity.z = 0.0f;
          }
          else {
            pev->velocity = pev->velocity + pev->velocity * frame_interval_ * 2.0f;
            pev->velocity.z = 0.0f;
          }
        }
        pev->button |= IN_JUMP;

        jump_finished_ = true;
        check_terrain_ = false;
        desired_velocity_.clear ();

        // keep the knife through the flight, return it a moment after landing
        jump_knife_restore_timer_.start (rg (0.5f, 1.0f));

        if (graph.IsAnalyzed () || analyzer.IsAnalyzed ()) {
          current_travel_flags_ &= ~PathFlag::Jump;
        }

        // cool down a little if next path after current will be jump
        if (jump_sequence_) {
          StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + rg (0.75f, 1.25f) + frame_interval_, false);
          jump_sequence_ = false;
        }
      }
    }
  }

  // if knife was drawn by navigation for the jump link, get the best weapon back once the jump is done
  RestoreAfterJump ();

  // special ladder handling
  if (has_flag (path_flags_, NodeFlag::Ladder)) {
    const auto prev_node_index = previous_nodes_[0];
    const auto ladder_distance_sq = pev->origin.distance_sq (path_origin_);

    // do a precise movement when very near
    if (graph.Exists (prev_node_index) && !has_flag (graph[prev_node_index].flags, NodeFlag::Ladder) &&
        ladder_distance_sq < ystl::sqrf (64.0f)) {

      if (!IsDucking ()) {
        move_speed_ = pev->maxspeed * 0.4f;
      }

      // do not duck while not on ladder
      if (!IsOnLadder ()) {
        pev->button &= ~IN_DUCK;
      }
      approaching_ladder_timer_.start (0.5f);
    }

    if (!IsOnLadder () && IsOnFloor () && !IsDucking ()) {
      if (!IsPreviousLadder ()) {
        move_speed_ = ystl::sqrtf (ladder_distance_sq);
      }
      move_speed_ = ystl::clamp (move_speed_, 160.0f, pev->maxspeed);
    }

    // press jump button if we need to leave the ladder
    if (!IsPreviousLadder () && IsOnLadder () && path_origin_.z < pev->origin.z && ladder_dir_ == LadderDir::Down) {
      pev->button |= IN_JUMP;
    }

    // prevent bots-towers on ladders
    for (const auto &client : clients) {
      if (!client.IsTeammate (team_, Ent ())) {
        continue;
      }

      const ystl::Vector client2_d = client.origin.get2d ();
      const ystl::Vector ladder2_d = path_origin_.get2d ();

      if ((client2_d - ladder2_d).length_sq () >= ystl::sqrf (40.0f)) {
        continue;
      }

      Trace::Result tr {};
      trace.Hull (GetEyesPos (), path_origin_, TraceIgnore::Monsters, IsDucking () ? head_hull : human_hull, Ent (), &tr);

      if (tr.hit == client.ent && ystl::abs (pev->origin.z - client.ent->v.origin.z) > 15.0f) {
        for (const auto &prev : previous_nodes_) {
          if (graph.Exists (prev) && !has_flag (graph[prev].flags, NodeFlag::Ladder)) {
            const auto current_task = Task ();

            if (current_task->id != TaskId::MoveTo || !ystl::fequal (current_task->desire, static_cast<float> (TaskPri::kPlantBomb))) {
              if (graph.Exists (prev_node_index)) {
                ChangeNodeIndex (prev_node_index);
              }
              StartTask (TaskId::MoveTo, TaskPri::kPlantBomb, prev, 0.0f, true);
            }
            break;
          }
        }
        break;
      }
    }
  }

  // special lift handling (code merged from podbotmm)
  if (has_flag (path_flags_, NodeFlag::Lift)) {
    if (UpdateLiftHandling ()) {
      if (!UpdateLiftStates ()) {
        return false;
      }
    }
    else {
      return false;
    }
  }
  Trace::Result tr {};

  // check if we are going through a door
  if (game.MapIs (MapFlags::HasDoors) || has_flag (path_flags_, NodeFlag::Button)) {
    trace.Line (pev->origin, path_origin_, TraceIgnore::Monsters, Ent (), &tr);

    if (!game.IsNullEntity (tr.hit) && lift_state_ == LiftState::None && game.IsDoorEntity (tr.hit)) {
      const ystl::Vector origin = game.GetEntityOrigin (tr.hit);
      const float distance_sq = pev->origin.distance_sq (origin);

      // if the door is near enough
      if (distance_sq < ystl::sqrf (56.0f)) {
        IgnoreCollision (); // don't consider being stuck

        // check if door has a linked button nearby that should be used instead of direct open
        bool should_use_button = false;

        if (!ystl::strings.is_empty (tr.hit->v.targetname.chars ()) && button_push_timer_.elapsed ()) {
          auto button = LookupButton (tr.hit->v.targetname.chars (), true);

          // if button exists and is close enough, use it instead of opening the door directly
          if (!game.IsNullEntity (button)) {
            const float button_distance_sq = pev->origin.distance_sq (game.GetEntityOrigin (button));

            if (button_distance_sq < ystl::sqrf (500.0f)) {
              pickup_item_ = button;
              pickup_type_ = Pickup::Button;
              nav_timer_.start ();

              should_use_button = true;
            }
          }
        }

        // only 'use' the door directly if there's no nearby button to press
        if (!should_use_button && button_push_timer_.elapsed () && rg.chance (50)) {
          // do not use door directly under xash, or we will get failed assert in gamedll code
          if (game.Is (GameFlags::Xash3D)) {
            pev->button |= IN_USE;
          }
          else {
            MDLL_Use (tr.hit, Ent ());
          }
          button_push_timer_.start (1.5f);
          door_hit_timer_.start (1.5f); // let the door open before moving
        }
      }

      // make sure we are always facing the door when going through it
      aim_flags_ &= ~(AimFlags::LastEnemy | AimFlags::PredictPath);
      can_set_aim_direction_ = false;

      // bot is blocked by the door, wait for it to open and retry
      if (pev->velocity.length_sq2d () < ystl::sqrf (10.0f) && door_open_timer_.elapsed ()) {
        if (!door_hit_timer_.elapsed ()) {
          StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + 0.5f, false);
        }
        door_open_timer_.start (1.0f); // retry in 1 sec until door is open
        ++try_open_door_;

        // detect what is blocking the door
        DoorBlockType block_type = DoorBlockType::Obstacle; // default: door stuck or unknown
        edict_t *blocker = nullptr;

        // check for players on the other side of the door
        for (const auto &client : clients) {
          if (!game.IsAliveEntity (client.ent)) {
            continue;
          }
          const float player_dist = client.ent->v.origin.distance_sq (pev->origin);

          // check if player is within door range
          if (player_dist < ystl::sqrf (96.0f)) {

            // trace to see if player is on the other side of the door
            Trace::Result tr_player;
            trace.Line (pev->origin, client.ent->v.origin, TraceIgnore::Monsters, Ent (), &tr_player);

            // if we hit the door before reaching player, they're on the other side
            if (!game.IsNullEntity (tr_player.hit) && game.IsDoorEntity (tr_player.hit)) {
              if (client.IsTeammate (team_, Ent ())) {
                block_type = DoorBlockType::Teammate;
              }
              else {
                block_type = DoorBlockType::Enemy;
              }
              blocker = client.ent;
              break; // found a blocker, no need to check further
            }
          }
        }

        // track door blocking state
        if (block_type != DoorBlockType::Obstacle) {

          // reset counter if blocker type changed
          if (door_block_type_ != block_type) {
            door_block_count_ = 1;
            door_block_type_ = block_type;
            door_blocked_timer_.start ();
          }
          else {
            ++door_block_count_;
          }
        }
        else {
          // no player blocker, just a stuck door
          door_block_type_ = DoorBlockType::Obstacle;
        }

        // handle enemy blocker - try to shoot through door
        if (block_type == DoorBlockType::Enemy && try_open_door_ > 1 && try_open_door_ < 4) {

          // check if enemy is penetrable (shootable through door)
          if (blocker != nullptr && cv_shoots_thru_walls && IsPenetrableObstacle (blocker->v.origin) && !cv_ignore_enemies) {
            see_enemy_timer_.start ();

            states_ |= Sense::SeeingEnemy | Sense::SuspectEnemy;
            aim_flags_ |= AimFlags::Enemy;

            enemy_ = blocker;
            last_enemy_origin_ = blocker->v.origin;

            try_open_door_ = 0;
          }
        }

        // handle persistent blocking - re-route when stuck too long
        const bool should_reroute = (try_open_door_ >= 4) || (block_type == DoorBlockType::Teammate && door_block_count_ >= 3);

        if (should_reroute) {
          const bool is_teammate_block = (block_type == DoorBlockType::Teammate);

          // reset counters
          if (is_teammate_block) {
            door_block_count_ = 0;
          }
          try_open_door_ = 0;

          const auto prev_node_index = previous_nodes_[0];

          // go back to prev node
          if (graph.Exists (prev_node_index)) {
            ChangeNodeIndex (prev_node_index);
          }
          door_hit_timer_.start (3.0f);

          // force path recalculation to find alternative route teammate blocking requires immediate re-path, enemy blocking can wait
          repath_timer_.start (is_teammate_block ? 0.1f : 0.5f);
        }
      }
      else {
        // door opened or we moved away - reset blocking state
        if (pev->velocity.length_sq2d () >= ystl::sqrf (10.0f)) {
          door_block_count_ = 0;
          door_block_type_ = DoorBlockType::None;
          door_blocked_timer_.invalidate ();
        }
      }
    }
  }

  float desired_distance_sq = ystl::sqrf (48.0f);
  const float node_distance_sq = pev->origin.distance_sq (path_origin_);

  // initialize the radius for a special node type, where the node is considered to be reached
  if (has_flag (path_flags_, NodeFlag::Lift)) {
    desired_distance_sq = ystl::sqrf (90.0f);
  }
  else if (IsDucking () || has_flag (path_flags_, NodeFlag::Goal)) {
    desired_distance_sq = ystl::sqrf (25.0f);

    // on cs_ maps goals are usually hostages, so increase reachability distance for them, they (hostages) picked anyway
    if (game.MapIs (MapFlags::HostageRescue) && has_flag (path_flags_, NodeFlag::Goal)) {
      desired_distance_sq = ystl::sqrf (96.0f);
    }
  }
  else if (has_flag (path_flags_, NodeFlag::Ladder)) {
    desired_distance_sq = ComputeLadderDesiredDistance ();
  }
  else if (HasJumpTravelFlag ()) {
    desired_distance_sq = 0.0f;

    if (pev->velocity.z > 16.0f) {
      desired_distance_sq = ystl::sqrf (8.0f);
    }
  }
  else if (has_flag (path_flags_, NodeFlag::Crouch)) {
    desired_distance_sq = ystl::sqrf (6.0f);
  }
  else if (path_->number == cv_debug_goal.As<int> ()) {
    desired_distance_sq = 0.0f;
  }
  else {
    desired_distance_sq = ystl::max (ystl::sqrf (path_->radius), desired_distance_sq);
  }
  bool path_has_flags = false;

  // check if node has a special travel flags, so they need to be reached more precisely
  for (const auto &link : path_->links) {
    if (link.flags != 0) {
      desired_distance_sq = 0.0f;
      path_has_flags = true;

      break;
    }
  }

  // make sure reach exactly, if just lost on path
  if (!lost_reachable_node_timer_.elapsed ()) {
    desired_distance_sq = 0.0f;
  }

  // if just recalculated path, assume reached current node
  if (!repath_timer_.elapsed () && !path_has_flags) {
    desired_distance_sq = ystl::sqrf (48.0f);
  }

  // needs precise placement - check if we get past the point
  if (desired_distance_sq < ystl::sqrf (16.0f) && node_distance_sq < ystl::sqrf (30.0f)) {
    const auto predict_range_sq = path_origin_.distance_sq (pev->origin + pev->velocity * frame_interval_);

    if (predict_range_sq >= node_distance_sq || predict_range_sq <= desired_distance_sq) {
      desired_distance_sq = node_distance_sq + 1.0f;
    }
  }

  // reroute when the final route node is already occupied
  if (path_walk_.HasNext ()) {
    const int32_t next_node = path_walk_.Next ();
    const int32_t last_node = path_walk_.Last ();

    if (next_node == last_node && IsOccupiedNode (next_node, path_has_flags)) {
      FindValidNode ();
      return true;
    }
  }

  if (node_distance_sq < desired_distance_sq) {
    // did we reach a destination node?
    if (Task ()->data == current_node_index_) {
      // cts searching a planted bomb count a reached site as searched (keeps the search moving)
      if (game_state.IsBombPlanted () && team_ == Team::CT && GetTaskId () != TaskId::EscapeFromBomb && !graph.IsVisited (current_node_index_)) {
        if (rg.chance (50)) {
          PushRadioChat (RadioChat::SectorClear);
        }
        MarkBombSiteVisited (current_node_index_);
      }

      if (chosen_goal_index_ != kInvalidNodeIndex) {
        constexpr int kMaxGoalValue = PracticeLimit::kGoal;

        // add goal values
        int goal_value = practice.GetValue (team_, chosen_goal_index_, current_node_index_);
        const int added_value = static_cast<int> (health_value_ * 0.5f + goal_value_ * 0.5f);

        goal_value = ystl::clamp (goal_value + added_value, -kMaxGoalValue, kMaxGoalValue);

        // update the practice for team
        practice.SetValue (team_, chosen_goal_index_, current_node_index_, goal_value);

        // ignore collision
        IgnoreCollision ();
      }
      return true;
    }
    else if (path_walk_.Empty ()) {
      return false;
    }
    const int task_target = Task ()->data;

    if (game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted () && team_ == Team::CT && GetTaskId () != TaskId::EscapeFromBomb &&
        task_target != kInvalidNodeIndex) {

      const ystl::Vector bomb_origin = IsBombAudible ();

      // audio correction: bomb is hearable, but the goal we're heading to is clearly not around it
      if (!bomb_origin.empty ()) {
        const float distance_sq = bomb_origin.distance_sq (graph[task_target].origin);

        if (distance_sq > ystl::sqrf (512.0f)) {
          if (rg.chance (50) && !graph.IsVisited (task_target)) {
            PushRadioChat (RadioChat::SectorClear);
          }
          MarkBombSiteVisited (task_target); // doesn't hear so not a good goal
        }
      }
    }
    AdvanceMovement (); // do the actual movement checking

    // update dest origin after advancing to ensure smooth direction transition
    dest_origin_ = path_origin_;
  }
  return false;
}

bool Bot::UpdateLiftHandling () {
  bool lift_closed_door_exists = false;

  // update node time set
  nav_timer_.start ();

  Trace::Result tr {};

  constexpr auto kLiftHeightTolerance = 70.0f;
  constexpr auto kLiftButtonTimeout = 7.0f;
  constexpr auto kLiftEnterTimeout = 5.0f;
  constexpr auto kLiftTravelTimeout = 14.0f;
  constexpr auto kLiftWaitingTimeout = 15.0f;
  constexpr auto kLiftTeammateTimeout = 8.0f;
  constexpr auto kLiftButtonInsideTimeout = 10.0f;
  constexpr auto kLiftWaitDistanceNear = 22.0f;
  constexpr auto kLiftWaitDistanceFar = 64.0f;
  constexpr auto kLiftFallThreshold = 50.0f;
  constexpr auto kLiftButtonWaitTime = 8.0f;
  constexpr auto kLiftLeavingTimeout = 7.0f;

  // validate lift entity is still valid before any operations
  if (!game.IsNullEntity (lift_entity_)) {
    // check if entity is still valid in the engine (not just non-null)
    if (lift_entity_->free || (lift_entity_->v.flags & FL_KILLME)) {
      lift_entity_ = nullptr;
      lift_state_ = LiftState::None;
      lift_usage_timer_.invalidate ();
    }
  }

  // wait for something about for lift
  auto wait = [&] () {
    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;
    check_terrain_ = false;

    nav_timer_.start ();
    aim_flags_ |= AimFlags::Nav;

    pev->button &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);

    IgnoreCollision ();
  };

  // need to wait?
  auto check_need_to_wait = [&] (float limit_sq = 22.0f) {
    if (pev->origin.distance_sq (dest_origin_) < ystl::sqrf (limit_sq)) {
      wait ();
    }
  };

  // trace line to door
  trace.Line (pev->origin, path_origin_, TraceIgnore::Everything, Ent (), &tr);

  if (tr.fraction < 1.0f && game.IsDoorEntity (tr.hit) &&
      (lift_state_ == LiftState::None || lift_state_ == LiftState::WaitingFor || lift_state_ == LiftState::LookingButtonOutside) &&
      pev->groundentity != tr.hit) {

    if (lift_state_ == LiftState::None) {
      lift_state_ = LiftState::LookingButtonOutside;
      lift_usage_timer_.start (kLiftButtonTimeout);
    }
    lift_closed_door_exists = true;
  }

  // helper
  auto is_func = [] (ystl::StringRef cls) -> bool {
    return cls.starts_with ("func_door") || cls == "func_plat" || cls == "func_train";
  };

  // trace line down
  trace.Line (path_->origin, path_origin_ + ystl::Vector (0.0f, 0.0f, -50.0f), TraceIgnore::Everything, Ent (), &tr);

  // if trace result shows us that it is a lift
  if (!game.IsNullEntity (tr.hit) && !path_walk_.Empty () && is_func (tr.hit->v.classname.str ()) && !lift_closed_door_exists) {

    // allow entry to moving lifts (removed velocity check)
    if (lift_state_ == LiftState::None || lift_state_ == LiftState::WaitingFor || lift_state_ == LiftState::LookingButtonOutside) {

      if (ystl::abs (pev->origin.z - tr.end_pos.z) < kLiftHeightTolerance) {

        // check if lift is stationary or moving slowly enough to board
        const bool lift_stationary = ystl::fzero (tr.hit->v.velocity.z);
        const bool lift_moving_slow = ystl::abs (tr.hit->v.velocity.z) < 50.0f;

        if (lift_stationary || lift_moving_slow) {
          lift_entity_ = tr.hit;
          lift_state_ = LiftState::EnteringIn;
          lift_travel_pos_ = path_origin_;
          lift_usage_timer_.start (kLiftEnterTimeout);
        }
      }
    }
    else if (lift_state_ == LiftState::TravelingBy) {
      lift_state_ = LiftState::Leaving;
      lift_usage_timer_.start (kLiftLeavingTimeout);
    }
  }
  else if (!path_walk_.Empty ()) { // no lift found at node
    if ((lift_state_ == LiftState::None || lift_state_ == LiftState::WaitingFor) && path_walk_.HasNext ()) {
      const auto next_node = path_walk_.Next ();

      if (graph.Exists (next_node) && has_flag (graph[next_node].flags, NodeFlag::Lift)) {
        trace.Line (path_->origin, graph[next_node].origin, TraceIgnore::Everything, Ent (), &tr);

        if (!game.IsNullEntity (tr.hit) && is_func (tr.hit->v.classname.str ())) {
          lift_entity_ = tr.hit;
        }
      }
      lift_state_ = LiftState::LookingButtonOutside;
      lift_usage_timer_.start (kLiftWaitingTimeout);
    }
  }

  // bot is going to enter the lift
  if (lift_state_ == LiftState::EnteringIn) {
    dest_origin_ = lift_travel_pos_;

    // check if we enough to destination
    if (pev->origin.distance_sq (dest_origin_) < ystl::sqrf (kLiftWaitDistanceNear)) {
      wait ();

      // need to wait our following teammate ?
      bool need_wait_for_teammate = false;

      // if some bot is following a bot going into lift - he should take the same lift to go
      for (auto &bot : bots) {
        if (!bot.is_alive_ || bot.team_ != team_ || bot.target_entity_ != Ent () || bot.GetTaskId () != TaskId::FollowUser) {
          continue;
        }

        // fix #7: set flag when teammate is already on lift
        if (bot.pev->groundentity == lift_entity_ && bot.IsOnFloor ()) {
          need_wait_for_teammate = true;
          break;
        }

        bot.lift_entity_ = lift_entity_;
        bot.lift_state_ = LiftState::EnteringIn;
        bot.lift_travel_pos_ = lift_travel_pos_;

        need_wait_for_teammate = true;
      }

      if (need_wait_for_teammate) {
        lift_state_ = LiftState::WaitingForTeammates;
        lift_usage_timer_.start (kLiftTeammateTimeout);
      }
      else {
        lift_state_ = LiftState::LookingButtonInside;
        lift_usage_timer_.start (kLiftButtonInsideTimeout);
      }
    }
  }

  // bot is waiting for his teammates
  if (lift_state_ == LiftState::WaitingForTeammates) {
    // need to wait our following teammate ?
    bool need_wait_for_teammate = false;

    for (auto &bot : bots) {
      if (!bot.is_alive_ || bot.team_ != team_ || bot.target_entity_ != Ent () || bot.GetTaskId () != TaskId::FollowUser ||
          bot.lift_entity_ != lift_entity_) {

        continue;
      }

      if (bot.pev->groundentity == lift_entity_ || !bot.IsOnFloor ()) {
        need_wait_for_teammate = true;
        break;
      }
    }

    // need to wait for teammate
    if (need_wait_for_teammate) {
      dest_origin_ = lift_travel_pos_;
      check_need_to_wait ();
    }

    // else we need to look for button
    if (!need_wait_for_teammate || lift_usage_timer_.elapsed ()) {
      lift_state_ = LiftState::LookingButtonInside;
      lift_usage_timer_.start (kLiftButtonInsideTimeout);
    }
  }

  // bot is trying to find button inside a lift
  if (lift_state_ == LiftState::LookingButtonInside) {
    check_terrain_ = false;

    if (game.IsNullEntity (lift_entity_)) {
      lift_state_ = LiftState::None;
      lift_usage_timer_.invalidate ();
      return true;
    }
    auto button = LookupButton (lift_entity_->v.targetname.str (), true);

    // got a valid button entity ?
    if (!game.IsNullEntity (button) && !game.IsNullEntity (lift_entity_) && pev->groundentity == lift_entity_ &&
        button_push_timer_.remaining_time () < -1.0f && ystl::fzero (lift_entity_->v.velocity.z) && IsOnFloor ()) {

      auto button_with_line_of_sight = LookupButton (lift_entity_->v.targetname.str (), false);

      if (!game.IsNullEntity (button_with_line_of_sight)) {
        pickup_item_ = button_with_line_of_sight;
        pickup_type_ = Pickup::Button;
      }
      else {
        MDLL_Use (button, Ent ());
      }
      const auto prev_node = previous_nodes_[0];

      if (graph.Exists (prev_node) && pev->origin.distance_sq2d (graph[prev_node].origin) < ystl::sqrf (72.0f)) {
        wait ();
      }
    }
    IgnoreCollision ();
  }

  // is lift activated and bot is standing on it and lift is moving ?
  if (lift_state_ == LiftState::LookingButtonInside || lift_state_ == LiftState::EnteringIn || lift_state_ == LiftState::WaitingForTeammates ||
      lift_state_ == LiftState::WaitingFor) {

    const auto prev_node = previous_nodes_[0];

    if (!game.IsNullEntity (lift_entity_) && pev->groundentity == lift_entity_ && !ystl::fzero (lift_entity_->v.velocity.z) && IsOnFloor () &&
        ((graph.Exists (prev_node) && has_flag (graph[prev_node].flags, NodeFlag::Lift)) || !game.IsNullEntity (target_entity_))) {

      lift_state_ = LiftState::TravelingBy;
      lift_usage_timer_.start (kLiftTravelTimeout);

      // fix #9: ensure collision is ignored while traveling on lift
      IgnoreCollision ();
      check_need_to_wait ();
    }
  }

  // bots is currently moving on lift
  if (lift_state_ == LiftState::TravelingBy) {
    dest_origin_ = ystl::Vector (lift_travel_pos_.x, lift_travel_pos_.y, pev->origin.z);

    check_need_to_wait ();
  }

  // need to find a button outside the lift
  if (lift_state_ == LiftState::LookingButtonOutside) {

    // verify button press success - check if lift is responding
    if (button_push_timer_.remaining_time () >= -kLiftButtonWaitTime) {
      if (graph.Exists (previous_nodes_[0])) {
        dest_origin_ = graph[previous_nodes_[0]].origin;
      }
      else {
        dest_origin_ = pev->origin;
      }
      check_need_to_wait (kLiftWaitDistanceFar);
    }
    else if (!game.IsNullEntity (lift_entity_)) {
      auto button = LookupButton (lift_entity_->v.targetname.str (), true);

      // if we got a valid button entity
      if (!game.IsNullEntity (button)) {

        // check if lift is used or reserved by another bot
        bool lift_used = false;

        // iterate though clients, and find if lift already used or reserved
        for (const auto &client : clients) {
          if (!client.IsTeammate (team_, Ent ())) {
            continue;
          }

          // check if physically on the lift
          if (!game.IsNullEntity (client.ent->v.groundentity) && client.ent->v.groundentity == lift_entity_) {
            lift_used = true;
            break;
          }

          // also check if another bot has reserved this lift (entering/waiting states)
          auto other_bot = bots[client.ent];

          if (other_bot != nullptr && other_bot->lift_entity_ == lift_entity_) {
            // bot is in process of using this lift
            if (other_bot->lift_state_ == LiftState::EnteringIn || other_bot->lift_state_ == LiftState::WaitingFor ||
                other_bot->lift_state_ == LiftState::LookingButtonOutside) {

              lift_used = true;
              break;
            }
          }
        }

        // lift is currently used
        if (lift_used) {
          if (graph.Exists (previous_nodes_[0])) {
            dest_origin_ = graph[previous_nodes_[0]].origin;
          }
          else {
            dest_origin_ = button->v.origin;
          }
          check_need_to_wait (64.0f);
        }
        else {
          pickup_item_ = button;
          pickup_type_ = Pickup::Button;

          lift_state_ = LiftState::WaitingFor;

          nav_timer_.start ();
          lift_usage_timer_.start (20.0f);
        }
      }
      else {
        lift_state_ = LiftState::WaitingFor;
        lift_usage_timer_.start (15.0f);
      }
    }
  }

  // bot is waiting for lift
  if (lift_state_ == LiftState::WaitingFor) {
    if (graph.Exists (previous_nodes_[0])) {
      if (!has_flag (graph[previous_nodes_[0]].flags, NodeFlag::Lift)) {
        dest_origin_ = graph[previous_nodes_[0]].origin;
      }
      else if (graph.Exists (previous_nodes_[1])) {
        dest_origin_ = graph[previous_nodes_[1]].origin;
      }
    }
    check_need_to_wait (64.0f);
  }

  // if bot is waiting for lift, or going to it
  if (lift_state_ == LiftState::WaitingFor || lift_state_ == LiftState::EnteringIn) {
    if (pev->groundentity != lift_entity_ && graph.Exists (previous_nodes_[0])) {
      const bool is_actually_falling = pev->velocity.z < -100.0f; // check if bot is falling
      const bool significant_height_diff =
        (path_->origin.z - pev->origin.z) > kLiftFallThreshold && (graph[previous_nodes_[0]].origin.z - pev->origin.z) > kLiftFallThreshold;

      if (has_flag (graph[previous_nodes_[0]].flags, NodeFlag::Lift) && is_actually_falling && significant_height_diff) {

        lift_state_ = LiftState::None;
        lift_entity_ = nullptr;
        lift_usage_timer_.invalidate ();

        ClearSearchNodes ();
        FindNextBestNode ();

        if (graph.Exists (previous_nodes_[2])) {
          FindPath (current_node_index_, previous_nodes_[2], path_type_);
        }
        return false;
      }
    }
  }
  return true;
}

bool Bot::UpdateLiftStates () {
  constexpr auto kLiftTimeoutExtension = 5.0f;
  constexpr auto kLiftLeavingTimeout = 7.0f;

  if (!game.IsNullEntity (lift_entity_) && !has_flag (path_flags_, NodeFlag::Lift)) {
    if (lift_state_ == LiftState::TravelingBy) {
      lift_state_ = LiftState::Leaving;
      lift_usage_timer_.start (kLiftLeavingTimeout);
    }
    if (lift_state_ == LiftState::Leaving && lift_usage_timer_.elapsed () && pev->groundentity != lift_entity_) {
      lift_state_ = LiftState::None;
      lift_usage_timer_.invalidate ();

      lift_entity_ = nullptr;
      check_terrain_ = true;
    }
  }

  if (lift_usage_timer_.elapsed () && lift_usage_timer_.started ()) {

    // don't timeout if we're actively traveling on the lift
    if (lift_state_ == LiftState::TravelingBy && !game.IsNullEntity (lift_entity_) && pev->groundentity == lift_entity_) {

      // extend timeout while still on moving lift
      lift_usage_timer_.start (kLiftTimeoutExtension);
      return true;
    }

    // we're not on the lift or in critical state
    lift_entity_ = nullptr;
    lift_state_ = LiftState::None;
    lift_usage_timer_.invalidate ();
    check_terrain_ = true;

    ClearSearchNodes ();

    if (graph.Exists (previous_nodes_[0])) {
      if (!has_flag (graph[previous_nodes_[0]].flags, NodeFlag::Lift)) {
        ChangeNodeIndex (previous_nodes_[0]);
      }
      else {
        FindNextBestNode ();
      }
    }
    else {
      FindNextBestNode ();
    }
    return false;
  }
  return true;
}

void Bot::ClearSearchNodes () {
  path_walk_.Clear ();
  chosen_goal_index_ = kInvalidNodeIndex;
}

void Bot::MarkBombSiteVisited (const int node) {
  // a checked goal marks the whole bombsite, not a single node, so cts don't
  // run back and forth between the goal points of the same site
  if (!graph.Exists (node)) {
    return;
  }
  const auto &origin = graph[node].origin;
  const float site_radius_sq = ystl::sqrf (kBombSiteGoalRadius);

  for (const int goal_index : graph.GetPoints (PointType::Goal)) {
    if (graph.Exists (goal_index) && graph[goal_index].origin.distance_sq (origin) <= site_radius_sq) {
      graph.SetVisited (goal_index);
    }
  }
}

bool Bot::FindNextBestNode () {
  // find nearest node when the bot lost its path

  const ystl::Vector origin = pev->origin + ystl::Vector { pev->velocity.x, pev->velocity.y, 0.0f } * frame_interval_;
  const auto *bucket = graph.GetNodesInBucket (origin);

  // maximum number of nodes to recheck without buckets
  constexpr auto kNearestRecheckThreshold = 1200;

  // try to search in buckets first
  if (bucket != nullptr && FindNextBestNodeEx (*bucket, graph.Length () < kNearestRecheckThreshold ? true : false)) {
    return true;
  }

  // fallback to nearest search instead
  return FindNextBestNodeEx (graph.GetNodeNumbers (), false);
}

bool Bot::FindNextBestNodeEx (const ystl::Array<int32_t> &data, bool handle_fails) {
  // finds the nearest valid node when the bot has lost its path or needs to restart pathfinding

  constexpr auto kTopCandidates = 3;
  constexpr auto kLowNodeDensityThreshold = 512;

  struct Candidate {
    int index { kInvalidNodeIndex };
    float distance_sq { kInfiniteDistance };
  };
  Candidate candidates[kTopCandidates] {};

  // skip up to 2 recent nodes to avoid oscillation
  const int num_to_skip = (graph.Length () < kLowNodeDensityThreshold || IsStuckState () || handle_fails) ? 0 : rg (0, 2);

  for (const auto node_index : data) {
    const auto &node = graph[node_index];

    // validate node exists
    if (!graph.Exists (node.number)) {
      continue;
    }

    // skip nodes sealed inside a closed mode wall
    if (mode_walls.IsNodeBlocked (node.origin)) {
      continue;
    }

    // skip current node
    if (node.number == current_node_index_) {
      continue;
    }

    // skip recent previous nodes to prevent backtracking
    bool is_recent = false;

    for (int j = 0; j < num_to_skip; ++j) {
      if (!graph.Exists (previous_nodes_[j])) {
        break; // stop if we hit an invalid previous node
      }
      if (node.number == previous_nodes_[j]) {
        is_recent = true;
        break;
      }
    }

    if (is_recent) {
      continue;
    }

    // ct with hostages should avoid nodes marked as nohostage
    if (game.MapIs (MapFlags::HostageRescue) && team_ == Team::CT && has_flag (node.flags, NodeFlag::NoHostage) && has_hostage_) {
      continue;
    }

    // require connectivity from current node if we have one
    if (current_node_index_ != kInvalidNodeIndex && !graph.IsConnected (current_node_index_, node.number)) {
      continue;
    }

    // skip unreachable nodes (visibility/height checks)
    if (!IsReachableNode (node.number)) {
      continue;
    }

    // calculate distance to candidate
    const float distance_sq = pev->origin.distance_sq (node.origin);

    // insert into top candidates if better than worst
    int worst_idx = 0;

    for (int i = 1; i < kTopCandidates; ++i) {
      if (candidates[i].distance_sq > candidates[worst_idx].distance_sq) {
        worst_idx = i;
      }
    }

    if (distance_sq < candidates[worst_idx].distance_sq) {
      candidates[worst_idx] = { node.number, distance_sq };
    }
  }

  // select random candidate from valid ones
  int selected = kInvalidNodeIndex;
  int valid_count = 0;

  for (const auto &cand : candidates) {
    if (cand.index != kInvalidNodeIndex) {
      ++valid_count;
    }
  }

  if (valid_count > 0) {
    const int pick_idx = (valid_count == 1) ? 0 : rg (0, valid_count - 1);

    int found = 0;
    for (const auto &cand : candidates) {
      if (cand.index != kInvalidNodeIndex) {
        if (found == pick_idx) {
          selected = cand.index;
          break;
        }
        ++found;
      }
    }
  }

  // fallback: use findnearestnode if no candidates found
  if (selected == kInvalidNodeIndex) {
    if (handle_fails) {
      return false;
    }
    selected = FindNearestNode ();
  }

  // nothing usable found
  if (selected == kInvalidNodeIndex || !graph.Exists (selected)) {
    return false;
  }

  // start timer based on estimated travel time to the selected node
  const float travel_time = pev->origin.distance_sq (graph[selected].origin) / ystl::sqrf (pev->maxspeed) * 4.0f;
  lost_reachable_node_timer_.start (travel_time);

  ChangeNodeIndex (selected);
  return true;
}

float Bot::GetEstimatedNodeReachTime () {
  const bool long_term_reachability =
    has_flag (path_flags_, NodeFlag::Crouch) || has_flag (path_flags_, NodeFlag::Ladder) || ((pev->button | pev->oldbuttons) & IN_DUCK);

  float estimated_time = long_term_reachability ? 8.5f : 3.5f;

  // if just fired at enemy, increase reachability
  if (shoot_time_ + 0.25f > game.Time ()) {
    return estimated_time;
  }

  if (last_damage_timestamp_ < game.Time () && !ystl::fzero (last_damage_timestamp_) && !IsStuckState () && is_creature_) {
    return estimated_time;
  }

  // calculate 'real' time that we need to get from one node to another
  if (graph.Exists (current_node_index_) && graph.Exists (previous_nodes_[0])) {
    const float distance_sq = graph[previous_nodes_[0]].origin.distance_sq (graph[current_node_index_].origin);

    // calculate estimated time
    estimated_time = 5.0f * (distance_sq / ystl::sqrf (move_speed_ + 1.0f));

    // check for special nodes, that can slowdown our movement
    if (long_term_reachability) {
      estimated_time *= 2.0f;
    }
    estimated_time = ystl::clamp (estimated_time, 3.0f, long_term_reachability ? 8.0f : 3.5f);
  }
  return estimated_time += last_damage_timestamp_ >= game.Time () ? 1.0f : 0.0f;
}

void Bot::FindValidNode () {
  // checks if the last node the bot was heading for is still valid

  auto try_select_new_goal = [&] () {
    if (rechoice_goal_count_ <= 1) {
      ClearSearchNodes ();

      if (FindNextBestNode ()) {
        ++rechoice_goal_count_;
        return;
      }
      rechoice_goal_count_ = 2; // local recovery failed, escalate to a far goal below
    }
    {
      const int new_goal = FindBestGoal ();

      prev_goal_index_ = new_goal;
      chosen_goal_index_ = new_goal;
      Task ()->data = new_goal;

      // do path finding if it's not the current node
      if (new_goal != current_node_index_) {
        FindPath (current_node_index_, new_goal, path_type_);
      }
      rechoice_goal_count_ = 0;
    }
  };

  // if bot hasn't got a node we need a new one anyway or if time to get there expired get new one as well
  if (current_node_index_ == kInvalidNodeIndex) {
    try_select_new_goal ();
  }
  else if (nav_timer_.elapsed_time () > GetEstimatedNodeReachTime ()) {
    constexpr int kMaxDamageValue = PracticeLimit::kDamage;

    // increase danger for both teams
    for (auto team = Team::Terrorist; team < Team::Num; ++team) {
      int damage_value = practice.GetDamage (team, current_node_index_, current_node_index_);
      damage_value = ystl::clamp (damage_value + 100, 0, kMaxDamageValue);

      // affect nearby connected with victim nodes
      for (const auto &neighbour : path_->links) {
        if (graph.Exists (neighbour.index)) {
          int neighbour_value = practice.GetDamage (team, neighbour.index, neighbour.index);
          neighbour_value = ystl::clamp (neighbour_value + 100, 0, kMaxDamageValue);

          practice.SetDamage (team, neighbour.index, neighbour.index, neighbour_value);
        }
      }
      practice.SetDamage (team, current_node_index_, current_node_index_, damage_value);
    }
    try_select_new_goal ();
  }
}

int Bot::ChangeNodeIndex (int index) {
  if (index == kInvalidNodeIndex) {
    return kInvalidNodeIndex;
  }
  ystl::copy (previous_nodes_.begin () + 1, previous_nodes_.begin (), 4);

  previous_nodes_[0] = current_node_index_;
  current_node_index_ = index;

  nav_timer_.start ();

  path_ = &graph[current_node_index_];
  path_origin_ = path_->origin;
  path_flags_ = static_cast<NodeFlag> (path_->flags);

  return current_node_index_; // to satisfy static-code analyzers
}

int Bot::FindNearestNode () {
  // finds the nearest visible and reachable node to the bot

  constexpr auto kTopCandidates = 3;

  constexpr float kMaxDistance = 1024.0f;
  constexpr float kMaxDistanceSq = ystl::sqrf (kMaxDistance);

  const auto predicted_origin = pev->origin + ystl::Vector { pev->velocity.x, pev->velocity.y, 0.0f } * frame_interval_;

  struct Candidate {
    int index { kInvalidNodeIndex };
    float distance_sq { kInfiniteDistance };
  };
  Candidate candidates[kTopCandidates] {};

  // phase 1: search in spatial bucket (fast path)
  const auto *bucket = graph.GetNodesInBucket (predicted_origin);

  for (size_t bi = 0; bucket != nullptr && bi < bucket->size (); ++bi) {
    const int node_index = (*bucket)[bi];

    if (!graph.Exists (node_index)) {
      continue;
    }
    const float distance_sq = graph[node_index].origin.distance_sq (predicted_origin);

    if (distance_sq >= kMaxDistanceSq) {
      continue;
    }

    // skip current node
    if (node_index == current_node_index_) {
      continue;
    }

    if (!IsReachableNode (node_index)) {
      continue;
    }

    // insert into top candidates if better than worst
    int worst_idx = 0;
    for (int i = 1; i < kTopCandidates; ++i) {
      if (candidates[i].distance_sq > candidates[worst_idx].distance_sq) {
        worst_idx = i;
      }
    }

    if (distance_sq < candidates[worst_idx].distance_sq) {
      candidates[worst_idx] = { node_index, distance_sq };
    }
  }

  // select random candidate from valid ones
  int selected = kInvalidNodeIndex;
  int valid_count = 0;

  for (const auto &cand : candidates) {
    if (cand.index != kInvalidNodeIndex) {
      ++valid_count;
    }
  }

  if (valid_count > 0) {
    const int pick_idx = (valid_count == 1) ? 0 : rg (0, valid_count - 1);

    int found = 0;
    for (const auto &cand : candidates) {
      if (cand.index != kInvalidNodeIndex) {
        if (found == pick_idx) {
          selected = cand.index;
          break;
        }
        ++found;
      }
    }
  }

  if (graph.Exists (selected)) {
    return selected;
  }

  // full graph search with visibility check (fallback)
  for (const int node_index : graph.GetNodeNumbers ()) {
    if (!graph.Exists (node_index)) {
      continue;
    }
    const float distance_sq = graph[node_index].origin.distance_sq (predicted_origin);

    if (distance_sq >= kMaxDistanceSq) {
      continue;
    }

    // skip current node
    if (node_index == current_node_index_) {
      continue;
    }

    // same bar as the main pass, no camping bots as a last resort either
    if (IsOccupiedNode (node_index, true)) {
      continue;
    }

    Trace::Result tr {};
    trace.Line (GetEyesPos (), graph[node_index].origin, TraceIgnore::Monsters, Ent (), &tr);

    if (tr.fraction >= 1.0f && !tr.start_solid) {
      // insert into top candidates if better than worst
      int worst_idx = 0;

      for (int i = 1; i < kTopCandidates; ++i) {
        if (candidates[i].distance_sq > candidates[worst_idx].distance_sq) {
          worst_idx = i;
        }
      }

      if (distance_sq < candidates[worst_idx].distance_sq) {
        candidates[worst_idx] = { node_index, distance_sq };
      }
    }
  }

  // select random candidate from valid ones
  selected = kInvalidNodeIndex;
  valid_count = 0;

  for (const auto &cand : candidates) {
    if (cand.index != kInvalidNodeIndex) {
      ++valid_count;
    }
  }

  if (valid_count > 0) {
    const int pick_idx = (valid_count == 1) ? 0 : rg (0, valid_count - 1);

    int found = 0;
    for (const auto &cand : candidates) {
      if (cand.index != kInvalidNodeIndex) {
        if (found == pick_idx) {
          selected = cand.index;
          break;
        }
        ++found;
      }
    }
  }

  if (graph.Exists (selected)) {
    return selected;
  }

  // worst case - use raw nearest without buckets
  return graph.GetNearestNoBuckets (predicted_origin);
}

int Bot::FindRandomNode (PointType type) {
  int picked = kInvalidNodeIndex;

  for (int tries = 0; tries < 8; ++tries) {
    const int candidate = (type == PointType::Count) ? graph.Random () : graph.GetRandomPoint (type);

    if (!graph.Exists (candidate)) {
      continue;
    }
    picked = candidate;

    if (!mode_walls.IsNodeBlocked (graph[candidate].origin)) {
      break;
    }
  }
  return picked;
}

int Bot::FindFarestNode (const ystl::Vector &origin, const float range) {
  if (!mode_walls.HasWalls ()) {
    return graph.GetFarest (origin, range);
  }
  int index = kInvalidNodeIndex;
  auto max_distance_sq = ystl::sqrf (range);

  for (const auto &path : graph) {
    const float distance_sq = path.origin.distance_sq (origin);

    if (distance_sq > max_distance_sq && !mode_walls.IsSegmentBlocked (origin, path.origin)) {
      index = path.number;
      max_distance_sq = distance_sq;
    }
  }
  return index != kInvalidNodeIndex ? index : FindRandomNode ();
}

int Bot::FindBombNode () {
  // finds the best goal node for cts when searching for a planted bomb
  // (intentionally imperfect: cts may check the wrong site first and search on)

  constexpr auto kTopCandidates = 3;

  struct Candidate {
    int index { kInvalidNodeIndex };
    float distance_sq { kInfiniteDistance };
  };
  Candidate candidates[kTopCandidates] {};

  const auto &bomb_origin = game_state.GetBombOrigin ();
  const ystl::Vector audible_origin = IsBombAudible ();

  // bot is very close to bomb - search for nearby node
  if (pev->origin.distance_sq (bomb_origin) < ystl::sqrf (96.0f)) {
    const int node = graph.GetNearest (bomb_origin, 420.0f);

    if (node != kInvalidNodeIndex) {
      bomb_search_overridden_ = true;
      return node;
    }
  }

  // bomb is audible - navigate to sound source
  if (!audible_origin.empty ()) {
    const int node = graph.GetNearest (audible_origin, 240.0f);

    if (node != kInvalidNodeIndex) {
      bomb_search_overridden_ = true;
      return node;
    }
  }

  // fallback: use goal points
  const auto &goals = graph.GetPoints (PointType::Goal);

  if (goals.empty ()) {
    // no goals available - return nearest node to bomb
    bomb_search_overridden_ = true;
    return graph.GetNearest (bomb_origin, 512.0f);
  }

  // collect unvisited bombsites, nearest to the bot first (may be the wrong one, that's intended)
  for (const int goal_index : goals) {
    if (!graph.Exists (goal_index) || graph.IsVisited (goal_index)) {
      continue;
    }
    const float distance_sq = pev->origin.distance_sq (graph[goal_index].origin);

    // insert into top candidates if better than worst
    int worst_idx = 0;

    for (int i = 1; i < kTopCandidates; ++i) {
      if (candidates[i].distance_sq > candidates[worst_idx].distance_sq) {
        worst_idx = i;
      }
    }

    if (distance_sq < candidates[worst_idx].distance_sq) {
      candidates[worst_idx] = { goal_index, distance_sq };
    }
  }

  // select random candidate from valid ones
  int selected = kInvalidNodeIndex;
  int valid_count = 0;

  for (const auto &cand : candidates) {
    if (cand.index != kInvalidNodeIndex) {
      ++valid_count;
    }
  }

  if (valid_count > 0) {
    const int pick_idx = (valid_count == 1) ? 0 : rg (0, valid_count - 1);

    int found = 0;
    for (const auto &cand : candidates) {
      if (cand.index != kInvalidNodeIndex) {
        if (found == pick_idx) {
          selected = cand.index;
          break;
        }
        ++found;
      }
    }
  }

  // if closest goal is marked as visited, try to find an unvisited alternative
  if (graph.IsVisited (selected) && goals.size () > 1) {
    for (const int goal_index : goals) {
      if (!graph.Exists (goal_index) || goal_index == selected) {
        continue;
      }
      if (!graph.IsVisited (goal_index)) {
        return goal_index;
      }
    }
  }
  return selected != kInvalidNodeIndex ? selected : goals.random ();
}

int Bot::FindDefendNode (const ystl::Vector &origin) {
  // find defend node with line of sight to given position

  EnsureCurrentNodeIndex ();
  Trace::Result tr {};

  struct NodeDistance {
    int index {};
    float distance {};
  };
  NodeDistance nodes[kMaxNodeLinks] {};

  // initialize nodes array with defaults
  for (auto &node : nodes) {
    node.index = kInvalidNodeIndex;
    node.distance = 128.0f;
  }

  const int pos_index = graph.GetNearest (origin);
  int src_index = current_node_index_;

  // max search distance
  const auto max_distance = ystl::clamp (static_cast<float> (148 * bots.GetBotCount ()), 256.0f, 1024.0f);

  // some of points not found, return random one
  if (src_index == kInvalidNodeIndex || pos_index == kInvalidNodeIndex) {
    return FindRandomNode ();
  }

  // single wall-aware pass for all distances (one dijkstra / one floyd row instead of a search per node)
  DistanceTable all_distances {};
  const bool have_distances = planner.DistAll (src_index, all_distances, 0);

  auto get_distance = [&] (int index) -> float {
    if (have_distances && index >= 0 && index < all_distances.Length ()) {
      return static_cast<float> (all_distances.At (index));
    }
    return planner.Dist (src_index, index); // fallback, normally unreachable
  };

  // find the best node now
  for (const auto &path : graph) {
    // exclude ladder & current nodes
    if (has_flag (path.flags, NodeFlag::Ladder) || path.number == src_index || !vistab.Visible (path.number, pos_index)) {
      continue;
    }

    // skip parts sealed off by mode walls (vistable refreshes incrementally and may be stale)
    if (mode_walls.IsSegmentBlocked (pev->origin, path.origin)) {
      continue;
    }

    // use the 'real' path finding distances
    auto distance = get_distance (path.number);

    // skip nodes too far
    if (distance > max_distance) {
      continue;
    }

    // skip occupied points
    if (IsOccupiedNode (path.number)) {
      continue;
    }
    trace.Line (path.origin, graph[pos_index].origin, TraceIgnore::Glass, Ent (), &tr);

    // check if line not hit anything
    if (!ystl::fequal (tr.fraction, 1.0f)) {
      continue;
    }

    // find the first node slot where this distance is greater
    for (auto &node : nodes) {
      if (distance > node.distance) {
        node.index = path.number;
        node.distance = distance;
        break;
      }
    }
  }

  // use statistic if we have them - update distances based on practice damage
  for (auto &node : nodes) {
    if (node.index != kInvalidNodeIndex) {
      int practice_damage = practice.GetDamage (team_, node.index, node.index);
      practice_damage = (practice_damage * 100) / practice.GetTeamDamage (team_);

      node.distance = static_cast<float> ((practice_damage * 100) / 8192);
      node.distance += static_cast<float> (practice_damage);
    }
  }

  ystl::bubble_sort (nodes, kMaxNodeLinks, [] (const NodeDistance &a, const NodeDistance &b) {
    return a.distance < b.distance;
  });

  if (nodes[0].index == kInvalidNodeIndex) {
    ystl::SmallArray<int32_t> found {};

    for (const auto &path : graph) {
      if (origin.distance_sq (path.origin) < ystl::sqrf (max_distance) && vistab.Visible (path.number, pos_index) &&
          !IsOccupiedNode (path.number)) {

        found.push (path.number);
      }
    }

    if (found.empty ()) {
      return FindRandomNode (); // most worst case, since there a evil error in nodes
    }
    return found.random ();
  }

  // count valid nodes (those with valid index)
  int valid_count = 0;

  for (const auto &node : nodes) {
    if (node.index == kInvalidNodeIndex) {
      break;
    }
    ++valid_count;
  }

  // return random node from the top half of valid nodes
  if (valid_count > 0) {
    return nodes[rg (0, (valid_count - 1) / 2)].index;
  }
  return FindRandomNode ();
}

int Bot::FindCoverNode (float max_distance) {
  // this function tries to find a good cover node if bot wants to hide

  const float enemy_max_distance = last_enemy_origin_.distance (pev->origin);

  // do not move to a position near to the enemy
  if (max_distance > enemy_max_distance) {
    max_distance = enemy_max_distance;
  }

  if (max_distance < 300.0f) {
    max_distance = 300.0f;
  }

  const int src_index = current_node_index_;
  const int enemy_index = graph.GetNearest (last_enemy_origin_);

  struct NodeDistance {
    int index {};
    float distance {};
  };
  NodeDistance nodes[kMaxNodeLinks] {};

  // initialize nodes array with defaults
  for (auto &node : nodes) {
    node.index = kInvalidNodeIndex;
    node.distance = max_distance;
  }

  if (enemy_index == kInvalidNodeIndex) {
    return kInvalidNodeIndex;
  }
  ystl::SmallArray<int32_t> enemies {};

  // now get enemies neighbouring points
  for (const auto &link : graph[enemy_index].links) {
    if (link.index != kInvalidNodeIndex) {
      enemies.push (link.index);
    }
  }

  // ensure we're on valid point
  ChangeNodeIndex (src_index);

  // single wall-aware pass per origin (one dijkstra / one floyd row instead of two searches per node)
  DistanceTable src_distances {}, enemy_distances {};
  const bool have_src_distances = planner.DistAll (src_index, src_distances, 0);
  const bool have_enemy_distances = planner.DistAll (enemy_index, enemy_distances, 1);

  auto get_src_distance = [&] (int index) -> float {
    if (have_src_distances && index >= 0 && index < src_distances.Length ()) {
      return static_cast<float> (src_distances.At (index));
    }
    return planner.Dist (src_index, index); // fallback, normally unreachable
  };

  auto get_enemy_distance = [&] (int index) -> float {
    if (have_enemy_distances && index >= 0 && index < enemy_distances.Length ()) {
      return static_cast<float> (enemy_distances.At (index));
    }
    return planner.Dist (enemy_index, index); // fallback, normally unreachable
  };

  // find the best node now
  for (const auto &path : graph) {
    // exclude ladder, current node, occupied and nodes seen by the enemy
    if (has_flag (path.flags, NodeFlag::Ladder) || path.number == src_index || IsOccupiedNode (path.number) ||
        vistab.Visible (enemy_index, path.number)) {
      continue;
    }

    // skip cover spots the bot itself cannot reach (vistable may be stale, enemy-side only here)
    if (mode_walls.IsSegmentBlocked (pev->origin, path.origin)) {
      continue;
    }
    bool neighbour_visible = false; // now check neighbour nodes for visibility

    for (const auto &enemy : enemies) {
      if (vistab.Visible (enemy, path.number)) {
        neighbour_visible = true;
        break;
      }
    }

    // skip visible points
    if (neighbour_visible) {
      continue;
    }

    // use the 'real' pathfinding distances
    const float distance = get_src_distance (path.number);
    const float enemy_distance = get_enemy_distance (path.number);

    if (distance >= enemy_distance) {
      continue;
    }

    // find the first node slot where this distance is smaller
    for (auto &node : nodes) {
      if (distance < node.distance) {
        node.index = path.number;
        node.distance = distance;
        break;
      }
    }
  }

  // use statistic if we have them - update distances based on practice damage
  for (auto &node : nodes) {
    if (node.index != kInvalidNodeIndex) {
      int practice_damage = practice.GetDamage (team_, node.index, node.index);
      practice_damage = (practice_damage * 100) / practice.GetTeamDamage (team_);

      node.distance = static_cast<float> ((practice_damage * 100) / 8192);
      node.distance += static_cast<float> (practice_damage);
    }
  }

  ystl::bubble_sort (nodes, kMaxNodeLinks, [] (const NodeDistance &a, const NodeDistance &b) {
    return a.distance < b.distance;
  });

  Trace::Result tr {};

  // take the first one which isn't spotted by the enemy
  for (const auto &node : nodes) {
    if (node.index != kInvalidNodeIndex) {
      trace.Line (last_enemy_origin_ + ystl::Vector (0.0f, 0.0f, 36.0f), graph[node.index].origin, TraceIgnore::Everything, Ent (), &tr);

      if (tr.fraction < 1.0f) {
        return node.index;
      }
    }
  }

  // if all are seen by the enemy, take the first one
  if (nodes[0].index != kInvalidNodeIndex) {
    return nodes[0].index;
  }
  return kInvalidNodeIndex; // do not use random points
}

bool Bot::SelectBestNextNode () {
  // post-process pathfinder nodes to avoid occupied nodes

  const auto next_node_index = path_walk_.Next ();
  const auto current_node_index = path_walk_.First ();
  const auto prev_node_index = current_node_index_;

  // if on ladder or current node is not occupied, no alternative needed
  if (IsOnLadder () || !IsOccupiedNode (current_node_index)) {
    return false;
  }

  // validate prevnodeindex before accessing
  if (!graph.Exists (prev_node_index)) {
    return false;
  }

  // check the links
  for (const auto &link : graph[prev_node_index].links) {

    // skip invalid links, or links that points to itself
    if (!graph.Exists (link.index) || current_node_index == link.index) {
      continue;
    }

    // skip isn't connected links
    if (!graph.IsConnected (link.index, next_node_index) || !graph.IsConnected (link.index, prev_node_index)) {
      continue;
    }

    // skip isn't visible nodes (check both prevnode->link and link->nextnode)
    if (!vistab.Visible (prev_node_index, link.index) || !vistab.Visible (link.index, next_node_index)) {
      continue;
    }

    // if is placed higher or lower than the bot's origin
    if (ystl::abs (graph[link.index].origin.z - pev->origin.z) > 60.0f) {
      continue;
    }

    // don't use ladder/camp nodes or jump links as alternative
    if ((has_flag (graph[link.index].flags, NodeFlag::Ladder | NodeFlag::Camp)) || has_flag (link.flags, PathFlag::Jump)) {
      continue;
    }

    // ct with hostage cannot take no-hostage alternative links
    if (game.MapIs (MapFlags::HostageRescue) && has_hostage_ && has_flag (graph[link.index].flags, NodeFlag::NoHostage)) {
      continue;
    }

    // if not occupied, just set advance
    if (!IsOccupiedNode (link.index)) {
      path_walk_.First () = link.index;
      return true;
    }
  }
  return false;
}

bool Bot::AdvanceMovement () {
  // advances in our pathfinding list and sets the appropriate destination origins for this bot

  FindValidNode (); // check if old nodes is still reliable

  // no nodes from pathfinding?
  if (path_walk_.Empty ()) {
    return false;
  }

  // check if next node is occupied by a camping bot - if so, try to find alternative
  if (path_walk_.HasNext ()) {
    const int next_node_index = path_walk_.Next ();

    if (graph.Exists (next_node_index)) {
      // check if a camping bot is blocking this node
      for (const auto &client : clients) {
        if (!client.IsTeammate (team_, Ent ())) {
          continue;
        }

        auto other_bot = bots[client.ent];

        if (other_bot != nullptr && other_bot->is_alive_ && other_bot != this) {
          const auto other_task_id = other_bot->GetTaskId ();

          // check if other bot is camping/hiding/pausing on the next node
          if ((other_task_id == TaskId::Camp || other_task_id == TaskId::Hide || other_task_id == TaskId::Pause) &&
              other_bot->current_node_index_ == next_node_index) {

            other_bot->CompleteTask ();
            other_bot->FindValidNode ();
          }
        }
      }
    }
  }

  path_walk_.Shift (); // advance in list
  current_travel_flags_ = PathFlag::None; // reset travel flags (jumping etc)

  // we're not at the end of the list?
  if (!path_walk_.Empty ()) {
    const int32_t first_node = path_walk_.First ();
    const int32_t last_node = path_walk_.Last ();

    // if in between a route, postprocess the node (find better alternatives)
    if (path_walk_.HasNext () && first_node != last_node) {
      SelectBestNextNode ();
      min_speed_ = pev->maxspeed;

      const auto tid = GetTaskId ();

      // only if we in normal task and bomb is not planted
      if (tid == TaskId::Normal && game_state.GetRoundMidTime () + 5.0f < game.Time () && time_camping_ + 5.0f < game.Time () &&
          !game_state.IsBombPlanted () && personality_ != Personality::Rusher && !has_c4_ && !is_vip_ &&
          loosed_bomb_node_index_ == kInvalidNodeIndex && !has_hostage_ && !is_creature_ && !ShouldRushEndgameTime ()) {

        camp_buttons_ = 0;

        // do not pause right before a jump connection
        bool jump_ahead = false;
        const auto next_node = path_walk_.First ();

        if (graph.Exists (current_node_index_) && graph.Exists (next_node)) {
          for (const auto &link : path_->links) {
            if (link.index == next_node && has_flag (link.flags, PathFlag::Jump)) {
              jump_ahead = true;
              break;
            }
          }
        }

        const auto next_index = path_walk_.Next ();
        const auto team_damage = static_cast<float> (practice.GetTeamDamage (team_));
        const auto node_damage = static_cast<float> (practice.GetDamage (team_, next_index, next_index));

        // normalize node damage against overall team damage (danger ratio)
        auto danger = 0.0f;

        if (team_damage > 0.0f) {
          danger = (node_damage * 100.0f) / team_damage / 100.0f;

          switch (personality_) {
          case Personality::Normal:
            danger /= 3.0f;
            break;

          default:
            danger /= 2.0f;
            break;
          }
        }

        // if damage done higher than one
        if (node_damage > 1.0f && game_state.GetRoundMidTime () > game.Time () && team_damage > 0.0f) {
          const float task_time = game.Time () + (fear_level_ * (game_state.GetRoundMidTime () - game.Time ()) * 0.5f);
          const int index = FindDefendNode (graph[next_index].origin);

          if (base_agression_level_ < danger && HasPrimaryWeapon ()) {
            StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, task_time, true);
            StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, 0.0f, true);
          }
        }
        else if (bots.EnemySpotted () && !IsOnLadder () && !IsInWater () && !jump_ahead && IsOnFloor ()) {
          // danger on the node is comparable with bot's aggression - duck for a while
          if (danger >= base_agression_level_ * 0.5f) {
            camp_buttons_ |= IN_DUCK;
          }
          else if (rg.chance (Skill ())) {
            min_speed_ = GetShiftSpeed ();
          }
        }
      }
    }

    if (!path_walk_.Empty ()) {
      jump_sequence_ = false;

      const auto dest_index = path_walk_.First ();
      bool is_current_jump = false;

      // find out about connection flags
      if (graph.Exists (dest_index) && graph.Exists (current_node_index_)) {
        for (const auto &link : path_->links) {
          if (link.index == dest_index) {
            current_travel_flags_ = static_cast<PathFlag> (link.flags);
            desired_velocity_ = link.velocity;
            jump_finished_ = false;

            // check if this is already a jump link
            if (has_flag (link.flags, PathFlag::Jump)) {
              is_current_jump = true;
            }

            // if graph is analyzed, check for slope-based jumps
            if ((graph.IsAnalyzed () || analyzer.IsAnalyzed ()) && !has_flag (path_->flags, NodeFlag::Ladder)) {
              const float diff = ystl::abs (path_->origin.z - graph[dest_index].origin.z);

              // if height difference is enough, consider this link as jump link
              if (graph[dest_index].origin.z > path_->origin.z && diff > cv_graph_slope_height.As<float> () &&
                  !IsWalkableAscent (path_->origin, graph[dest_index].origin)) {
                current_travel_flags_ |= PathFlag::Jump;
                desired_velocity_.clear (); // make bot compute jump velocity
                jump_finished_ = false; // force-mark this path as jump

                is_current_jump = true;
              }
            }
            break;
          }
        }

        // hostage to be rescued - never commit a move the hostage cannot follow (window jumps, no-hostage zones)
        if (game.MapIs (MapFlags::HostageRescue) && has_hostage_ &&
            (has_flag (current_travel_flags_, PathFlag::Jump) || has_flag (graph[dest_index].flags, NodeFlag::NoHostage))) {
          prev_goal_index_ = kInvalidNodeIndex;
          Task ()->data = kInvalidNodeIndex;

          const int new_goal = FindBestGoal ();

          prev_goal_index_ = new_goal;
          chosen_goal_index_ = new_goal;
          Task ()->data = new_goal;

          if (graph.Exists (new_goal) && new_goal != current_node_index_) {
            FindPath (current_node_index_, new_goal, path_type_);
          }
          return true;
        }

        // check if bot is going to jump
        bool will_jump = false;
        float jump_distance_sq = 0.0f;

        ystl::Vector src {}, dst {};

        // try to find out about future connection flags
        if (path_walk_.HasNext ()) {
          auto next_index = path_walk_.Next ();

          const auto &path = graph[dest_index];
          const auto &next = graph[next_index];

          for (const auto &link : path.links) {
            if (link.index == next_index && has_flag (link.flags, PathFlag::Jump)) {
              src = path.origin;
              dst = next.origin;

              jump_distance_sq = src.distance_sq (dst);
              will_jump = true;

              break;
            }
          }
        }

        // mark as jump sequence, if the current and next paths are jumps
        if (is_current_jump) {
          jump_sequence_ = will_jump && jump_distance_sq > ystl::sqrf (96.0f);
        }

        // is there a jump node right ahead? then maybe draw the knife for extra speed
        if (will_jump) {
          DrawKnifeForJump (jump_distance_sq, dst.z - src.z);
        }

        // bot not already on ladder but will be soon?
        if (has_flag (graph[dest_index].flags, NodeFlag::Ladder) && !IsOnLadder ()) {

          // get ladder nodes used by other (first moving) bots
          for (const auto &other : bots) {

            // if another bot uses this ladder, wait 3 secs
            if (&other != this && other.is_alive_ && other.current_node_index_ == dest_index && other.IsOnLadder ()) {
              StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + 3.0f, false);
              return true;
            }
          }
        }
      }
      ChangeNodeIndex (dest_index);
    }
  }
  SetPathOrigin ();
  nav_timer_.start ();

  return true;
}

void Bot::SetPathOrigin () {
  constexpr int kMaxAlternatives = 5;

  auto set_non_zero_path_origin = [&] () -> void {
    path_origin_ +=
      ystl::Vector { pev->angles.x, ystl::wrap_angle (pev->angles.y + rg (-90.0f, 90.0f)), 0.0f }.forward () * rg (0.0f, path_->radius);
  };

  // special handling for ladder nodes only (not all nodes when bot is on ladder)
  if (IsOnLadder ()) {
    edict_t *ladder = nullptr;

    game.SearchEntities (path_origin_, 96.0f, [&] (edict_t *e) {
      if (e->v.classname.str () == "func_ladder") {
        ladder = e;
        return EntitySearchResult::Break;
      }
      return EntitySearchResult::Continue;
    });

    if (!game.IsNullEntity (ladder)) {
      Trace::Result tr {};
      trace.Model (path_origin_, game.GetEntityOrigin (ladder), point_hull, ladder, &tr);

      if (path_origin_.z >= pev->origin.z) {
        ladder_dir_ = LadderDir::Up;
        path_origin_ = path_->origin - tr.plane_normal + ystl::Vector { 0.0f, 0.0f, 29.0f };
      }
      else if (path_origin_.z < pev->origin.z) {
        ladder_dir_ = LadderDir::Down;
        path_origin_ = path_->origin + tr.plane_normal;
      }
    }
    return;
  }

  auto validate_position = [&] (const ystl::Vector &pos) -> bool {
    // validate position is walkable and not inside walls
    Trace::Result tr {};
    trace.Hull (pos + ystl::Vector { 0.0f, 0.0f, 36.0f }, pos, TraceIgnore::Monsters, head_hull, Ent (), &tr);

    return tr.fraction >= 1.0f && !tr.start_solid && !tr.all_solid;
  };

  // if node radius non zero vary origin a bit depending on the body angles
  if (path_->radius > 16.0f && !IsInNarrowPlace ()) {
    int nearest_index = kInvalidNodeIndex;

    if (!path_walk_.Empty () && path_walk_.HasNext ()) {
      ystl::Vector orgs[kMaxAlternatives] {};

      // use circular distribution instead of square to respect calculated radius
      for (int i = 0; i < kMaxAlternatives; ++i) {
        const float angle = ystl::rg (0.0f, 360.0f) * ystl::kDegreeToRadians;
        const float dist = rg (0.0f, path_->radius);

        orgs[i] = path_origin_ + ystl::Vector (ystl::cosf (angle) * dist, ystl::sinf (angle) * dist, 0.0f);

        // validate position - fallback to center if invalid
        if (!validate_position (orgs[i])) {
          orgs[i] = path_origin_;
        }
      }
      float nearest_distance_sq = kInfiniteDistance;

      for (int i = 0; i < kMaxAlternatives; ++i) {
        const float distance_sq = pev->origin.distance_sq (orgs[i]);

        if (distance_sq < nearest_distance_sq) {
          nearest_index = i;
          nearest_distance_sq = distance_sq;
        }
      }

      // set the origin if found alternative
      if (nearest_index != kInvalidNodeIndex) {
        path_origin_ = orgs[nearest_index];
      }
    }

    if (nearest_index == kInvalidNodeIndex) {
      set_non_zero_path_origin ();
    }
  }
  else if (path_->radius > 0.0f) {
    set_non_zero_path_origin ();
  }
}

edict_t *Bot::IsBlockedForward (const ystl::Vector &normal) {
  // checks if bot is blocked in its movement direction (excluding doors and teammates/hostages)

  constexpr float kTraceDistance = 24.0f;
  constexpr float kSlopeThreshold = 0.9f;

  // calculate slope-adjusted forward direction
  ystl::Vector forward_dir = normal;

  if (IsOnFloor () && !game.IsNullEntity (pev->groundentity)) {
    Trace::Result ground_tr {};

    const ystl::Vector ground_src = pev->origin + ystl::Vector (0.0f, 0.0f, 2.0f);
    const ystl::Vector ground_dst = pev->origin + ystl::Vector (0.0f, 0.0f, -4.0f);

    trace.Line (ground_src, ground_dst, TraceIgnore::Monsters, Ent (), &ground_tr);

    if (ground_tr.fraction < 1.0f && !ystl::fzero (ground_tr.plane_normal.z) && ground_tr.plane_normal.z < kSlopeThreshold) {
      const auto &n = ground_tr.plane_normal;
      forward_dir = (forward_dir - (forward_dir | n) * n).normalize ();
    }
  }

  // check if entity is a real blockage (not door, not hostage)
  auto is_blocked = [this] (const Trace::Result &result) -> bool {
    return result.fraction < 1.0f && !(game.MapIs (MapFlags::HasDoors) && game.IsDoorEntity (result.hit)) &&
           !(team_ == Team::CT && game.IsHostageEntity (result.hit));
  };

  // single hull trace for body check
  const ystl::Vector right = ystl::Vector (0.0f, pev->angles.y, 0.0f).right ();

  const ystl::Vector hull_src = pev->origin + ystl::Vector (0.0f, 0.0f, 8.0f);
  const ystl::Vector hull_dst = hull_src + forward_dir * kTraceDistance;

  Trace::Result tr {};
  trace.Hull (hull_src, hull_dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

  if (is_blocked (tr)) {
    const ystl::Vector eye_src = GetEyesPos ();
    const ystl::Vector eye_dst = eye_src + forward_dir * kTraceDistance;

    trace.Line (eye_src, eye_dst, TraceIgnore::Monsters, Ent (), &tr);

    if (is_blocked (tr)) {
      return tr.hit;
    }
  }

  // check left and right shoulder positions
  constexpr float kShoulderOffsetZ = 20.0f;
  constexpr float kBodyHalfWidth = 16.0f;

  const ystl::Vector shoulder_src = GetEyesPos () + ystl::Vector (0.0f, 0.0f, -kShoulderOffsetZ);
  const ystl::Vector shoulder_left = shoulder_src - right * kBodyHalfWidth;
  const ystl::Vector shoulder_right = shoulder_src + right * kBodyHalfWidth;

  trace.Line (shoulder_left, shoulder_left + forward_dir * kTraceDistance, TraceIgnore::Monsters, Ent (), &tr);
  if (is_blocked (tr)) {
    return tr.hit;
  }

  trace.Line (shoulder_right, shoulder_right + forward_dir * kTraceDistance, TraceIgnore::Monsters, Ent (), &tr);
  if (is_blocked (tr)) {
    return tr.hit;
  }

  if (IsDucking ()) {
    trace.Line (pev->origin, pev->origin + forward_dir * kTraceDistance, TraceIgnore::Monsters, Ent (), &tr);
    if (is_blocked (tr)) {
      return tr.hit;
    }
  }

  return nullptr;
}

bool Bot::CanStrafeLeft (Trace::Result *tr) {
  // this function checks if bot can move sideways (2 tracelines)

  ystl::Vector left {}, forward {};
  pev->angles.angle_vectors (&forward, &left, nullptr);

  const ystl::Vector &src = pev->origin;
  const ystl::Vector strafe_dest = src - left * 40.0f;

  // trace from the bot's waist straight left
  trace.Line (src, strafe_dest, TraceIgnore::Monsters, Ent (), tr);

  if (tr->fraction < 1.0f && !game.IsDoorEntity (tr->hit)) {
    return false;
  }

  // trace from the strafe position straight forward
  const ystl::Vector forward_dest = strafe_dest + forward * 40.0f;
  trace.Line (strafe_dest, forward_dest, TraceIgnore::Monsters, Ent (), tr);

  return tr->fraction >= 1.0f || game.IsDoorEntity (tr->hit);
}

bool Bot::CanStrafeRight (Trace::Result *tr) {
  // this function checks if bot can move sideways (2 tracelines)

  ystl::Vector right {}, forward {};
  pev->angles.angle_vectors (&forward, &right, nullptr);

  const ystl::Vector &src = pev->origin;
  const ystl::Vector strafe_dest = src + right * 40.0f;

  // trace from the bot's waist straight right
  trace.Line (src, strafe_dest, TraceIgnore::Monsters, Ent (), tr);

  if (tr->fraction < 1.0f && !game.IsDoorEntity (tr->hit)) {
    return false;
  }

  // trace from the strafe position straight forward
  const ystl::Vector forward_dest = strafe_dest + forward * 40.0f;
  trace.Line (strafe_dest, forward_dest, TraceIgnore::Monsters, Ent (), tr);

  return tr->fraction >= 1.0f || game.IsDoorEntity (tr->hit);
}

bool Bot::CanJumpUp (const ystl::Vector &normal) {
  // this function checks if bot can jump over some obstacle

  // can't jump if not on ground and not on ladder/swimming
  if (!IsOnFloor () && (IsOnLadder () || !IsInWater ())) {
    return false;
  }

  // helper lambda to check clearance at a given height offset and position
  auto check_jump_clearance = [this] (
                                const ystl::Vector &offset, const ystl::Vector &surface_normal, float forward_dist, float up_dist) -> bool {
    Trace::Result tr {};

    // trace forward at jump height
    ystl::Vector src = pev->origin + offset;
    ystl::Vector dest = src + surface_normal * forward_dist;

    trace.Line (src, dest, TraceIgnore::Monsters, Ent (), &tr);

    if (tr.fraction < 1.0f) {
      return false;
    }

    // trace upward to check for overhead obstructions
    src = dest;
    dest.z = dest.z + up_dist;

    trace.Line (src, dest, TraceIgnore::Monsters, Ent (), &tr);
    return tr.fraction >= 1.0f;
  };

  // jump height constants
  constexpr float kJumpForwardDist = 32.0f;
  constexpr float kJumpUpDist = 37.0f;
  constexpr float kNormalJumpHeight = -36.0f + 45.0f; // 9.0f - waist height + normal jump
  constexpr float kDuckJumpHeight = -36.0f + 63.0f; // 27.0f - waist height + duck jump
  constexpr float kSideOffset = 16.0f;

  // check center position at normal jump height
  if (!check_jump_clearance (ystl::Vector (0.0f, 0.0f, kNormalJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
    // try duck jump height as fallback
    if (!check_jump_clearance (ystl::Vector (0.0f, 0.0f, kDuckJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
      return false;
    }
  }

  // check right side at normal jump height
  if (!check_jump_clearance (ystl::Vector (kSideOffset, 0.0f, kNormalJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
    // try duck jump height as fallback
    if (!check_jump_clearance (ystl::Vector (kSideOffset, 0.0f, kDuckJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
      return false;
    }
  }

  // check left side at normal jump height
  if (!check_jump_clearance (ystl::Vector (-kSideOffset, 0.0f, kNormalJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
    // try duck jump height as fallback
    if (!check_jump_clearance (ystl::Vector (-kSideOffset, 0.0f, kDuckJumpHeight), normal, kJumpForwardDist, kJumpUpDist)) {
      return false;
    }
  }

  return true;
}

bool Bot::CanDuckUnder (const ystl::Vector &normal) {
  // this function checks if bot can duck under obstacle

  // helper lambda to check clearance at duck height
  auto check_duck_clearance = [this] (const ystl::Vector &offset, const ystl::Vector &surface_normal, float forward_dist) -> bool {
    Trace::Result tr {};

    ystl::Vector base_height = IsDucking () ? pev->origin + ystl::Vector (0.0f, 0.0f, -17.0f) : pev->origin;
    ystl::Vector src = base_height + offset;
    ystl::Vector dest = src + surface_normal * forward_dist;

    trace.Line (src, dest, TraceIgnore::Monsters, Ent (), &tr);
    return tr.fraction >= 1.0f;
  };

  constexpr float kDuckForwardDist = 32.0f;
  constexpr float kSideOffset = 16.0f;

  // check center position
  if (!check_duck_clearance (ystl::Vector (0.0f, 0.0f, 0.0f), normal, kDuckForwardDist)) {
    return false;
  }

  // check right side
  if (!check_duck_clearance (ystl::Vector (kSideOffset, 0.0f, 0.0f), normal, kDuckForwardDist)) {
    return false;
  }

  // check left side
  return check_duck_clearance (ystl::Vector (-kSideOffset, 0.0f, 0.0f), normal, kDuckForwardDist);
}

bool Bot::IsBlockedLeft () {
  Trace::Result tr {};

  ystl::Vector left {}, forward {};
  pev->angles.angle_vectors (&forward, &left, nullptr);

  const ystl::Vector test_dir = move_speed_ > 0.0f ? forward : -forward;
  constexpr float kBlockDistance = 48.0f;

  const ystl::Vector src = pev->origin - left * kBlockDistance;
  const ystl::Vector dst = src + test_dir * kBlockDistance;

  trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

  return tr.fraction < 1.0f && !game.IsDoorEntity (tr.hit);
}

bool Bot::IsBlockedRight () {
  Trace::Result tr {};

  ystl::Vector right {}, forward {};
  pev->angles.angle_vectors (&forward, &right, nullptr);

  const ystl::Vector test_dir = move_speed_ > 0.0f ? forward : -forward;
  constexpr float kBlockDistance = 48.0f;

  const ystl::Vector src = pev->origin + right * kBlockDistance;
  const ystl::Vector dst = src + test_dir * kBlockDistance;

  trace.Hull (src, dst, TraceIgnore::Monsters, head_hull, Ent (), &tr);

  return tr.fraction < 1.0f && !game.IsDoorEntity (tr.hit);
}

bool Bot::CheckWallOnLeft (float distance) {
  Trace::Result tr {};
  trace.Line (pev->origin, pev->origin - pev->angles.right () * distance, TraceIgnore::Monsters, Ent (), &tr);

  // check if the trace hit something
  return tr.fraction < 1.0f;
}

bool Bot::CheckWallOnRight (float distance) {
  Trace::Result tr {};

  // do a trace to the right
  trace.Line (pev->origin, pev->origin + pev->angles.right () * distance, TraceIgnore::Monsters, Ent (), &tr);

  // check if the trace hit something
  return tr.fraction < 1.0f;
}

bool Bot::CheckWallOnBehind (float distance) {
  Trace::Result tr {};

  // do a trace to the right
  trace.Line (pev->origin, pev->origin - pev->angles.forward () * distance, TraceIgnore::Monsters, Ent (), &tr);

  // check if the trace hit something
  return tr.fraction < 1.0f;
}

bool Bot::IsDeadlyMove (const ystl::Vector &to) {
  // this function checks if the path to the given location would hurt the bot with falling damage

  Trace::Result tr {};

  constexpr auto kUnitsDown = 1000.0f;
  constexpr auto kFallLimit = 160.0f;
  constexpr auto kStepSize = 16.0f;

  // helper lambda to get ground height at a position
  auto get_ground_height = [&] (const ystl::Vector &pos) -> float {
    Trace::Result ground {};
    ystl::Vector down = pos;
    down.z -= kUnitsDown;

    trace.Line (pos, down, TraceIgnore::Monsters, Ent (), &ground);
    return ground.fraction * kUnitsDown;
  };

  // check for wall blocking at destination
  trace.Line (to, to + ystl::Vector { 0.0f, 0.0f, -kUnitsDown }, TraceIgnore::Monsters, Ent (), &tr);

  if (tr.start_solid) {
    return false; // wall blocking, not a valid move anyway
  }

  float last_height = tr.fraction * kUnitsDown;
  float distance_sq = to.distance_sq (pev->origin);

  // if destination itself has a deadly drop, return true
  if (distance_sq <= ystl::sqrf (16.0f) && last_height > kFallLimit) {
    return true;
  }

  // trace back from destination to bot position, checking for sudden drops
  const ystl::Vector move_dir = (to - pev->origin).normalize ();
  ystl::Vector check_pos = to;

  while (distance_sq > ystl::sqrf (16.0f)) {
    ystl::Vector check = check_pos - move_dir * kStepSize;
    float height = get_ground_height (check);

    // check if there's a sudden drop (ground falls away)
    if (height > last_height + kFallLimit) {
      return true;
    }

    last_height = height;
    distance_sq = check.distance_sq (pev->origin);
    check_pos = check;
  }

  return false;
}

bool Bot::IsNotSafeToMove (const ystl::Vector &to) {
  // simplified version of isdeadlymove() just for combat movement checking

  constexpr auto kUnitsDown = 1000.0f;
  constexpr auto kFallLimit = 160.0f;

  Trace::Result tr {};
  trace.Line (to, to + ystl::Vector { 0.0f, 0.0f, -kUnitsDown }, TraceIgnore::Monsters, Ent (), &tr);

  return tr.start_solid || tr.fraction * kUnitsDown > kFallLimit;
}

int Bot::GetRandomCampDir () {
  // find a good node to look at when camping

  EnsureCurrentNodeIndex ();

  constexpr auto kMaxCandidates = 5;
  constexpr auto kMinCampDistance = 192.0f;

  struct CampCandidate {
    int index { kInvalidNodeIndex };
    float distance_sq { 0.0f };
    uint16_t visibility { 0 };
  };

  ystl::SmallArray<CampCandidate> candidates {};
  candidates.reserve (kMaxCandidates);

  // collect candidate nodes with visibility and distance scoring
  for (const auto &path : graph) {
    // skip invalid candidates
    if (path.number == current_node_index_ || !vistab.Visible (current_node_index_, path.number)) {
      continue;
    }
    const float distance_sq = pev->origin.distance_sq (path.origin);
    if (distance_sq < ystl::sqrf (kMinCampDistance)) {
      continue;
    }

    const uint16_t visibility = path.vis.crouch + path.vis.stand;

    // fill empty slots first
    if (candidates.size<int> () < kMaxCandidates) {
      candidates.emplace (CampCandidate { path.number, distance_sq, visibility });
    }
    // replace worse candidates when we find better ones
    else {
      for (auto &candidate : candidates) {
        if (visibility >= candidate.visibility && distance_sq > candidate.distance_sq) {
          candidate = { path.number, distance_sq, visibility };
          break;
        }
      }
    }
  }

  // pick random candidate if we have any
  if (!candidates.empty ()) {
    return candidates[rg (0, candidates.size<int> () - 1)].index;
  }

  // fallback: use last enemy origin for prediction
  if (!last_enemy_origin_.empty ()) {
    int path_length = 0;
    const int predict_node = FindAimingNode (last_enemy_origin_, path_length);

    if (IsNodeValidForPredict (predict_node) && path_length > 1 && vistab.Visible (predict_node, current_node_index_)) {
      return predict_node;
    }
  }

  // worst case: return any random node
  return graph.Random ();
}

int Bot::FindAimingNode (const ystl::Vector &to, int &path_length) {
  // return the most distant node which is seen from the bot to the target and is within count
  EnsureCurrentNodeIndex ();

  const int dest_index = graph.GetNearest (to);

  if (dest_index == kInvalidNodeIndex) {
    return kInvalidNodeIndex;
  }

  // context is captured by a single pointer, so the lambda stays within ystl::lambda inline storage
  struct AimContext {
    int path_length {};
    int best_index {};
    int from_index {};
  } ctx { 0, current_node_index_, current_node_index_ };

  auto result = planner.Find (dest_index, current_node_index_, [&ctx] (int index) {
    ++ctx.path_length;

    if (vistab.Visible (ctx.from_index, index)) {
      ctx.best_index = index;
      return false;
    }
    return true;
  });
  path_length = ctx.path_length;

  if (result && ctx.best_index == current_node_index_) {
    return kInvalidNodeIndex;
  }
  return ctx.best_index;
}

void Bot::SetStrafeSpeedRaw (float strafe_speed) {
  constexpr float kWallCheckDistance = 64.0f;

  if (ystl::fequal (strafe_speed, 0.0f)) {
    return;
  }
  const bool want_right = strafe_speed > 0.0f;
  float final_speed = 0.0f;

  if (want_right) {
    if (!CheckWallOnRight (kWallCheckDistance)) {
      final_speed = strafe_speed;
    }
    else if (!CheckWallOnLeft (kWallCheckDistance)) {
      final_speed = -strafe_speed;
    }
  }
  else {
    if (!CheckWallOnLeft (kWallCheckDistance)) {
      final_speed = strafe_speed;
    }
    else if (!CheckWallOnRight (kWallCheckDistance)) {
      final_speed = -strafe_speed;
    }
  }

  if (ystl::fequal (final_speed, 0.0f)) {
    return;
  }

  auto right = pev->angles.right ();
  auto dest = pev->origin + 30.0f * right * ((final_speed > 0.0f) ? 1.0f : -1.0f);

  if (!IsNotSafeToMove (dest)) {
    strafe_speed_ = final_speed;
  }
}

void Bot::SetStrafeSpeed (const ystl::Vector &normal, float strafe_speed) {
  auto los = (normal - pev->origin).normalize2d ();
  float dot = los | pev->angles.forward ().get2d ();

  if (dot > 0.0f && !CheckWallOnRight ()) {
    strafe_speed_ = strafe_speed;
  }
  else if (!CheckWallOnLeft ()) {
    strafe_speed_ = -strafe_speed;
  }
}

int Bot::GetNearestToPlantedBomb () {
  // this function tries to find planted c4 on the defuse scenario map and returns nearest to it node

  if (!game.MapIs (MapFlags::Demolition)) {
    return kInvalidNodeIndex; // don't search for bomb if the player is ct, or it's not defusing bomb
  }

  auto bomb_model = conf.GetBombModelName ();
  auto result = kInvalidNodeIndex;

  // search the bomb on the map
  game.SearchEntities ("classname", "grenade", [&] (edict_t *ent) {
    if (game.IsEntityModelMatches (ent, bomb_model)) {
      result = graph.GetNearest (game.GetEntityOrigin (ent));

      if (graph.Exists (result)) {

        // if bomb entity is bot's ignore list, clear ignore list
        if (IsIgnoredItem (ent)) {
          ignored_items_.clear ();
        }
        return EntitySearchResult::Break;
      }
    }
    return EntitySearchResult::Continue;
  });
  return result;
}

bool Bot::IsOccupiedNode (int index, bool need_zero_velocity) {
  if (!graph.Exists (index)) {
    return true;
  }

  if (pev->solid == SOLID_NOT) {
    return false;
  }

  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }

    // do not check clients far away from us
    if (client.IsOutsideRadius (pev->origin, ystl::sqrf (320.0f))) {
      continue;
    }

    if (need_zero_velocity && client.ent->v.velocity.length2d () > 0.0f) {
      continue;
    }
    const auto distance_sq = client.origin.distance_sq (graph[index].origin);

    // special handling for zero-radius nodes (ladders, goals, vents) - use tight check
    const float occupation_radius = graph[index].radius > 0.0f ? ystl::clamp (graph[index].radius * 2.0f, 64.0f, 120.0f) : 32.0f;

    if (distance_sq < ystl::sqrf (occupation_radius)) {
      return true;
    }
    auto bot = bots[client.ent];

    if (bot == nullptr || bot == this || !bot->is_alive_) {
      continue;
    }
    if (bot->current_node_index_ == index || bot->previous_nodes_[0] == index) {
      return true;
    }
  }
  return false;
}

edict_t *Bot::LookupButton (ystl::StringRef target, bool blind_test) {
  // find nearest button for given target and return its entity

  if (target.empty ()) {
    return nullptr;
  }
  float nearest_distance_sq = kInfiniteDistance;
  edict_t *result = nullptr;

  Trace::Result tr {};

  // find the nearest button which can open our target
  game.SearchEntities ("target", target, [&] (edict_t *ent) {
    const ystl::Vector pos = game.GetEntityOrigin (ent);

    if (!blind_test) {
      trace.Line (pev->origin, pos, TraceIgnore::Monsters, this->Ent (), &tr);
    }

    // check if this place safe
    if (blind_test || (tr.hit == ent || tr.fraction > 0.95f)) {
      const float distance_sq = pev->origin.distance_sq (pos);

      // check if we got more close button
      if (distance_sq < nearest_distance_sq) {
        nearest_distance_sq = distance_sq;
        result = ent;
      }
    }
    return EntitySearchResult::Continue;
  });
  return result;
}

bool Bot::IsReachableNode (int index) {
  // this function return whether bot able to reach index node or not, depending on several factors

  if (!graph.Exists (index)) {
    return false;
  }
  const auto &src = pev->origin;
  const auto &dst = graph[index].origin;

  // recovery scan radius, matches the fallback search range
  constexpr auto kReachableNodeDistance = 1024.0f;

  // is the destination close enough?
  if (dst.distance_sq (src) > ystl::sqrf (kReachableNodeDistance)) {
    return false;
  }

  // it's should be not a problem to reach node inside water
  if (pev->waterlevel == 2 || pev->waterlevel == 3) {
    return true;
  }
  const float distance_sq2d = dst.distance_sq2d (src);

  // check for ladder
  const bool non_ladder = !has_flag (graph[index].flags, NodeFlag::Ladder) || distance_sq2d > ystl::sqrf (16.0f);

  // is dest node higher than src? (62 is max jump height)
  if (non_ladder && dst.z > src.z + 62.0f) {
    return false; // can't reach this one
  }

  // is dest node lower than src?
  if (non_ladder && dst.z < src.z - 100.0f) {
    return false; // can't reach this one
  }

  // some one seems to camp at this node
  if (IsOccupiedNode (index, true)) {
    return false; // can't reach this one
  }

  Trace::Result tr {};
  trace.Line (src, dst, TraceIgnore::Monsters, Ent (), &tr);

  // if node is visible from current position (even behind head)
  return tr.fraction >= 1.0f;
}

bool Bot::IsPreviousLadder () const {
  const auto prev_node_index = previous_nodes_[0];

  if (!graph.Exists (prev_node_index)) {
    return false;
  }
  return (graph[prev_node_index].flags & NodeFlag::Ladder) != 0;
}

void Bot::FindShortestPath (int src_index, int dest_index, PathWalk &staging, PathMeta &meta) {
  // find shortest source to destination path (builds into staging)

  // stale bots shouldn't do pathfinding
  if (is_stale_) {
    return;
  }
  staging.ResetForBuild ();

  meta.chosen_goal_index = src_index;
  meta.goal_value = 0.0f;

  // reject precomputed paths crossing a closed mode wall
  int prev_node = kInvalidNodeIndex;
  bool wall_blocked = false;

  const bool success = planner.Find (src_index, dest_index, [&staging, &prev_node, &wall_blocked] (int index) {
    if (prev_node != kInvalidNodeIndex && !wall_blocked) {
      wall_blocked = mode_walls.IsSegmentBlocked (graph[prev_node].origin, graph[index].origin);
    }
    prev_node = index;
    staging.Add (index);

    return true;
  });

  if (success && !wall_blocked) {
    meta.has_path = true;
    meta.invalidate_prev_goal = false;
    meta.invalidate_goal_task = false;
    meta.update_goal_task = false;
  }
  else {
    meta.has_path = false;
    meta.invalidate_prev_goal = true;
    meta.prev_goal_index = kInvalidNodeIndex;
    meta.invalidate_goal_task = true;
  }
}

void Bot::ApplyPathResult (const PathWalk &staging, const PathMeta &meta) {
  // applies a freshly built path result to the bot

  if (meta.has_path) {
    path_walk_.CopyFrom (staging);
  }
  chosen_goal_index_ = meta.chosen_goal_index;
  goal_value_ = meta.goal_value;

  if (meta.invalidate_prev_goal) {
    prev_goal_index_ = meta.prev_goal_index;
  }
  if (meta.update_goal_task && graph.Exists (meta.goal_task_index)) {
    Task ()->data = meta.goal_task_index;

    prev_goal_index_ = meta.goal_task_index;
    chosen_goal_index_ = meta.goal_task_index;
  }
  if (meta.invalidate_goal_task) {
    Task ()->data = kInvalidNodeIndex;
  }
  if (meta.repath_delay > 0.0f) {
    repath_timer_.start (meta.repath_delay);
  }
}

void Bot::FindPath (int src_index, int dest_index, FindPathType path_type) {
  // this function finds a path from srcindex to destindex;

  // stale bots shouldn't do pathfinding
  if (is_stale_) {
    return;
  }

  // throttle repeated goal changes so the path is not rebuilt every think
  if (!path_enqueue_timer_.elapsed ()) {
    return;
  }
  path_enqueue_timer_.start (0.1f);

  // drop the old walk up front, so a failed/aborted search leaves no stale path
  path_walk_.Clear ();

  // reuse persistent path staging between searches to avoid allocations
  PathWalk &staging = build_path_;

  if (staging.Capacity () != path_walk_.Capacity ()) {
    staging.Init (path_walk_.Capacity ());
  }
  staging.ResetForBuild ();

  PathMeta meta {};

  if (!graph.Exists (src_index)) {
    src_index = ChangeNodeIndex (graph.GetNearestNoBuckets (pev->origin, 256.0f));

    if (!graph.Exists (src_index)) {
      ystl::logger.error ("%s source path index not valid (%d).", __func__, src_index);
      return;
    }
  }
  else if (!graph.Exists (dest_index) || dest_index == src_index) {
    dest_index = graph.GetNearestNoBuckets (pev->origin, kInfiniteDistance, NodeFlag::Goal);

    if (!graph.Exists (dest_index) || src_index == dest_index) {
      dest_index = FindRandomNode ();

      if (!graph.Exists (dest_index)) {
        ystl::logger.error ("%s dest path index not valid (%d).", __func__, dest_index);
        return;
      }
    }
  }

  // do not process if src points to dst
  if (src_index == dest_index) {
    ystl::logger.error ("%s source path is same as dest (%d).", __func__, dest_index);
    return;
  }

  // always use shortest-path algorithm when failed sanity checks within load
  if (planner.IsPathsCheckFailed ()) {
    FindShortestPath (src_index, dest_index, staging, meta);
    ApplyPathResult (staging, meta);
    return;
  }

  // pick the heuristic strategy for the requested path type
  if (path_type == FindPathType::Diversity) {
    planner_->SetHeuristic (&plannerHeuristics::diversity);
  }
  else if (path_type == FindPathType::Optimal) {
    if (game.MapIs (MapFlags::HostageRescue) && has_hostage_) {
      planner_->SetHeuristic (&plannerHeuristics::optimal_hostage);
    }
    else {
      planner_->SetHeuristic (&plannerHeuristics::optimal);
    }
  }
  else if (path_type == FindPathType::Safe) {
    if (game.MapIs (MapFlags::HostageRescue) && has_hostage_) {
      planner_->SetHeuristic (&plannerHeuristics::safe_hostage);
    }
    else {
      planner_->SetHeuristic (&plannerHeuristics::safe);
    }
  }
  else {
    if (game.MapIs (MapFlags::HostageRescue) && has_hostage_) {
      planner_->SetHeuristic (&plannerHeuristics::fast_hostage);
    }
    else {
      planner_->SetHeuristic (&plannerHeuristics::fast);
    }
  }
  staging.ResetForBuild ();

  meta.has_path = true;
  meta.invalidate_prev_goal = false;
  meta.invalidate_goal_task = false;
  meta.chosen_goal_index = src_index;
  meta.goal_value = 0.0f;
  meta.repath_delay = 0.5f;

  // walls may seal the destination node itself; retarget to a free node up-front
  // instead of running a* towards a point that can never be reached
  int effective_dest = dest_index;
  bool dest_swapped = false;

  if (mode_walls.HasWalls () && graph.Exists (effective_dest) && mode_walls.IsNodeBlocked (graph[effective_dest].origin)) {
    for (int tries = 0; tries < 8; ++tries) {
      const int alt = FindRandomNode ();

      if (!graph.Exists (alt) || alt == src_index) {
        continue;
      }

      if (mode_walls.IsNodeBlocked (graph[alt].origin)) {
        continue;
      }
      effective_dest = alt;
      dest_swapped = true;
      break;
    }

    // every probe is walled in: don't spin a* in a tight loop, back off
    if (!dest_swapped) {
      meta.has_path = false;
      meta.invalidate_prev_goal = true;
      meta.prev_goal_index = kInvalidNodeIndex;
      meta.invalidate_goal_task = true;
      meta.repath_delay = 2.0f;

      ApplyPathResult (staging, meta);
      return;
    }
  }

  const auto result = planner_->Find (team_, src_index, effective_dest, [&staging] (int index) {
    staging.Add (index);
    return true;
  });

  // view the results
  switch (result) {
  case AStarResult::Success:
    staging.Reverse (); // reverse path for path follower

    if (dest_swapped) {
      meta.update_goal_task = true;
      meta.goal_task_index = effective_dest;
      meta.chosen_goal_index = effective_dest;
    }
    break;

  case AStarResult::InternalError:
    kick_me_from_server_ = true; // bot should be kicked within main thread, not here

    // do not publish broken path, bot keeps its old path until kicked
    ystl::logger.error ("A* Search for bot \"%s\" failed with internal pathfinder error. Seems to be graph is broken. Bot removed (re-added).",
      pev->netname.chars ());
    return;

  case AStarResult::Failed: {
    FindShortestPath (src_index, effective_dest, staging, meta); // a* found no path, try shortest-path algorithm instead

    if (meta.has_path) {
      if (dest_swapped) {
        meta.update_goal_task = true;
        meta.goal_task_index = effective_dest;
        meta.chosen_goal_index = effective_dest;
      }
      break;
    }

    if (mode_walls.HasWalls ()) {
      bool salvaged = false;

      for (int tries = 0; tries < 8 && !salvaged; ++tries) {
        const int alt = FindRandomNode ();

        if (!graph.Exists (alt) || alt == src_index || alt == effective_dest) {
          continue;
        }

        if (mode_walls.IsNodeBlocked (graph[alt].origin)) {
          continue;
        }

        // wall-aware reachability probe (dijkstra while walls are up)
        if (planner.Dist (src_index, alt) >= static_cast<float> (kInfiniteDistanceLong)) {
          continue;
        }
        FindShortestPath (src_index, alt, staging, meta);

        if (meta.has_path) {
          meta.update_goal_task = true;
          meta.goal_task_index = alt;
          meta.chosen_goal_index = alt;
          salvaged = true;
        }
      }

      if (salvaged) {
        break;
      }
      meta.has_path = false;
      meta.invalidate_prev_goal = true;
      meta.prev_goal_index = kInvalidNodeIndex;
      meta.invalidate_goal_task = true;
      meta.repath_delay = 2.0f;

      if (ctrl.IsDebug ()) {
        ystl::logger.error (
          "A* Search for bot \"%s\" has failed: goal is sealed off by mode walls, will pick a new goal.", pev->netname.chars ());
      }
      break;
    }

    if (ctrl.IsDebug ()) {
      ystl::logger.error (
        "A* Search for bot \"%s\" has failed. Falling back to shortest-path algorithm. Seems to be graph is broken.", pev->netname.chars ());
    }
    break;
  }
  }

  ApplyPathResult (staging, meta);
}

ystl::Vector Bot::CalcJumpVelocity (const ystl::Vector &start, const ystl::Vector &stop) {
  const float gravity = sv_gravity.As<float> ();

  if (ystl::fzero (gravity)) {
    return nullptr;
  }
  const ystl::Vector delta = stop - start;

  // airtime from vertical motion, larger root lands on target height
  const float disc = kPlayerJumpTakeoff * kPlayerJumpTakeoff - 2.0f * gravity * delta.z;

  if (disc < 0.0f) {
    return nullptr; // target too high for a jump
  }
  const float airtime = (kPlayerJumpTakeoff + ystl::sqrtf (disc)) / gravity;

  if (airtime <= 0.0f) {
    return nullptr;
  }
  ystl::Vector velocity = delta * (1.0f / airtime);
  velocity.z = 0.0f; // takeoff comes from the jump button, engine owns it

  // no speed clamp, airborne velocity persists and the arc was solved for it
  return velocity;
}

} // namespace bot
