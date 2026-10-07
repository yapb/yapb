//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void Bot::FilterTasks () {
  // initialize & calculate the desire for all actions based on distances, emotions and other stuff
  Task ();

  // per-call desire candidates, arbitrated against each other below
  struct Candidate {
    TaskId id;
    float desire;
  };

  Candidate attack { TaskId::Attack, 0.0f };
  Candidate pickup { TaskId::PickupItem, 0.0f };
  Candidate seek_cover { TaskId::SeekCover, 0.0f };
  Candidate hunt { TaskId::Hunt, 0.0f };
  Candidate hide { TaskId::Hide, 0.0f };
  Candidate blind { TaskId::Blind, 0.0f };

  float temp_fear = fear_level_;
  float temp_agression = agression_level_;

  // decrease fear if players near
  int friendly_num = 0;

  if (!last_enemy_origin_.empty ()) {
    friendly_num = NumFriendsNear (pev->origin, 500.0f) - NumEnemiesNear (last_enemy_origin_, 500.0f);
  }

  if (friendly_num > 0) {
    temp_fear = temp_fear * 0.5f;
  }

  // increase/decrease fear/aggression if bot uses a sniping weapon to be more careful
  if (UsesSniper ()) {
    temp_fear = temp_fear * 1.5f;
    temp_agression = temp_agression * 0.5f;
  }

  // bot found some item to use?
  if (!game.IsNullEntity (pickup_item_) && GetTaskId () != TaskId::EscapeFromBomb) {
    states_ |= Sense::PickupItem;

    if (pickup_type_ == Pickup::Button) {
      pickup.desire = 50.0f; // always pickup button
    }
    else {
      pickup.desire = ystl::max (50.0f, 500.0f - pev->origin.distance (game.GetEntityOrigin (pickup_item_)) * 0.2f);
    }
  }
  else {
    states_ &= ~Sense::PickupItem;
    pickup.desire = 0.0f;
  }

  // calculate desire to attack
  if (has_flag (states_, Sense::SeeingEnemy) && ReactOnEnemy ()) {
    attack.desire = TaskPri::kAttack;
  }
  else {
    attack.desire = 0.0f;
  }
  float &seek_cover_desire = seek_cover.desire;
  float &hunt_enemy_desire = hunt.desire;
  float &blinded_desire = blind.desire;

  // calculate desires to seek cover or hunt
  if (game.IsPlayerEntity (last_enemy_) && !last_enemy_origin_.empty () && !has_c4_) {
    const float retreat_level = (pev->max_health - health_value_) * temp_fear; // retreat level depends on bot health

    if (is_creature_ || (num_friends_left_ > 0 && num_enemies_left_ > num_friends_left_ / 2)) {
      float time_seen = -see_enemy_timer_.elapsed_time ();
      float time_heard = -heard_sound_timer_.elapsed_time ();
      float ratio = 0.0f;

      if (time_seen > time_heard) {
        time_seen += 10.0f;
        ratio = time_seen * 0.1f;
      }
      else {
        time_heard += 10.0f;
        ratio = time_heard * 0.1f;
      }
      const bool low_ammo = IsLowOnAmmo (current_weapon_, 0.18f);
      const bool sniping = !sniper_stop_timer_.elapsed () && low_ammo;

      if (is_creature_) {
        ratio = 0.0f;
      }
      if (game_state.IsBombPlanted () || IsStuckState () || UsesKnife ()) {
        ratio /= 3.0f; // reduce the seek cover desire if bomb is planted
      }
      else if (is_vip_ || reload_data_.is_reloading || (sniping && UsesSniper ())) {
        ratio *= 3.0f; // triple the seek cover desire if bot is vip or reloading
      }
      else if (infected_enemy_team_) {
        ratio *= 3.0f;
      }
      else if (game.Is (GameFlags::CSDM)) {
        ratio = 0.0f;
      }
      else {
        ratio /= 2.0f; // reduce seek cover otherwise
      }
      seek_cover_desire = retreat_level * ratio;
    }
    else {
      seek_cover_desire = 0.0f;
    }

    // if half of the round is over, allow hunting (creatures hunt all the time, as soon as they know where to run)
    if (GetTaskId () != TaskId::EscapeFromBomb && game.IsNullEntity (enemy_) && !is_vip_ &&
        (is_creature_ || game_state.GetRoundMidTime () < game.Time ()) && !has_hostage_ && !is_using_grenade_ &&
        current_node_index_ != graph.GetNearest (last_enemy_origin_) && (is_creature_ || personality_ != Personality::Careful) &&
        !cv_ignore_enemies) {

      float desire_level = 4096.0f - ((1.0f - temp_agression) * last_enemy_origin_.distance (pev->origin));

      desire_level = (100.0f * desire_level) / 4096.0f;

      // creatures never fear death, they don't have any ranged weapon to retreat with
      if (!is_creature_) {
        desire_level -= retreat_level;
      }

      desire_level = ystl::clamp (desire_level, 0.0f, 89.0f);
      hunt_enemy_desire = desire_level;
    }
    else {
      hunt_enemy_desire = 0.0f;
    }
  }
  else {
    hunt_enemy_desire = 0.0f;
    seek_cover_desire = 0.0f;
  }

  // zombie bots has more hunt desire
  if (is_creature_ && hunt_enemy_desire > 16.0f) {
    hunt_enemy_desire = TaskPri::kAttack;
    seek_cover_desire = 0.0f;
  }

  // don't spam cover search, it's already been done recently
  if (!cover_search_timer_.elapsed ()) {
    seek_cover_desire = 0.0f;
  }

  // blinded behavior
  blinded_desire = !blind_timer_.elapsed () ? TaskPri::kBlind : 0.0f;

  // desires are set, now filter all actions against each other
  // most values were tuned by trial and error, so expect roughness

  // this function returns the behavior having the higher activation level
  auto max_desire = [] (Candidate *first, Candidate *second) {
    if (first->desire > second->desire) {
      return first;
    }
    return second;
  };

  // this function returns the first behavior if its activation level is anything higher than zero
  auto subsume_desire = [] (Candidate *first, Candidate *second) {
    if (first->desire > 0) {
      return first;
    }
    return second;
  };

  // this function returns the input behavior if it's activation level exceeds the threshold, or some default behavior otherwise
  auto threshold_desire = [] (Candidate *first, float threshold, float desire) {
    if (first->desire < threshold) {
      first->desire = desire;
    }
    return first;
  };

  // this function clamp the inputs to be the last known value outside the [min, max] range
  auto hysteresis_desire = [] (float cur, float min, float max, float old) {
    if (cur <= min || cur >= max) {
      old = cur;
    }
    return old;
  };

  old_combat_desire_ = hysteresis_desire (attack.desire, 40.0f, 90.0f, old_combat_desire_);
  attack.desire = old_combat_desire_;

  auto *offensive = &attack;

  // calc survive (cover/hide)
  auto *survive = threshold_desire (&seek_cover, 40.0f, 0.0f);
  survive = subsume_desire (&hide, survive);

  auto *def = threshold_desire (&hunt, 60.0f, 0.0f); // don't allow hunting if desires 60<
  offensive = subsume_desire (offensive, &pickup); // if offensive task, don't allow picking up stuff

  auto *sub = max_desire (offensive, def); // default normal & careful tasks against offensive actions
  auto *final_task = subsume_desire (&blind, max_desire (survive, sub)); // reason about fleeing instead

  if (!tasks_.Empty ()) {
    auto *current = Task ();

    // steady state, keep the running goal
    if (final_task->desire <= current->desire) {
      return;
    }
    // submit the final behavior with highest desire, keep the running goal
    StartTask (final_task->id, final_task->desire, kInvalidNodeIndex, 0.0f, TaskResumable (final_task->id), true);
  }
}

void Bot::ClearTasks () {
  // this function resets bot tasks stack, by removing all entries from the stack

  tasks_.Clear ();
}

void Bot::StartTask (TaskId id, float desire, int data, float time, bool resume, bool preserve_goal) {

  // check entire stack for duplicate task (original behavior)
  auto *existing = tasks_.Find (id);

  if (existing) {
    // skip update when nothing changed
    if (ystl::fequal (existing->desire, desire) && existing->data == data && ystl::fequal (existing->time, time) && existing->resume == resume) {
      return;
    }

    // update fields of existing task (no stack mutation happens here, so pointer is safe)
    if (!ystl::fequal (existing->desire, desire)) {
      existing->desire = desire;
    }

    // filter-driven refreshes must not clobber the goal/expiry of a running task
    if (!preserve_goal) {
      existing->data = data;
      existing->time = time;
    }
    existing->resume = resume;

    // check priorities and select max desire task
    CheckTaskPriorities ();
  }
  else {
    // push new task to stack
    tasks_.Emplace (TaskHandler (id), id, desire, data, time, resume);

    // check if this new task should become current based on priority
    CheckTaskPriorities ();
    IgnoreCollision ();
  }
  const auto tid = GetTaskId ();

  // leader bot?
  if (is_leader_ && tid == TaskId::SeekCover) {
    UpdateTeamCommands (); // reorganize team if fleeing
  }

  if (tid == TaskId::Camp) {
    SelectBestWeapon ();
  }

  // this is best place to handle some chatter commands report team some info
  if (cv_radio_mode.As<int> () > 1) {
    HandleChatterTaskChange (tid);
  }

  if (cv_debug_goal.As<int> () != kInvalidNodeIndex) {
    chosen_goal_index_ = cv_debug_goal.As<int> ();
  }
  else {
    chosen_goal_index_ = Task ()->data;
  }
}

void Bot::CheckTaskPriorities () {
  // prune interrupted tasks; drop the cached path when the active task changed
  if (tasks_.Promote ()) {
    ClearSearchNodes ();
  }
}

Task *Bot::Task () {
  if (tasks_.Empty ()) [[unlikely]] {

    // push the base task directly, without the starttask () side effects
    tasks_.Emplace (TaskHandler (TaskId::Normal), TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);
    IgnoreCollision ();
  }
  return &tasks_.Current ();
}

void Bot::ClearTask (TaskId id) {
  // this function removes one task from the bot task stack

  if (tasks_.Empty () || id == TaskId::Normal) {
    return; // since normal task can be only once on the stack, don't remove it
  }

  if (GetTaskId () == id) {
    ClearSearchNodes ();
    IgnoreCollision ();

    tasks_.Pop ();
    return;
  }

  for (auto &task : tasks_) {
    if (task.id == id) {
      tasks_.Remove (task);
      break;
    }
  }
  CheckTaskPriorities ();
  IgnoreCollision ();
}

void Bot::CompleteTask () {
  // this function called whenever a task is completed

  IgnoreCollision ();

  if (tasks_.Empty ()) {
    return;
  }

  // pop the completed task, and any non-resumable tasks it reveals
  tasks_.Complete ();

  ClearSearchNodes ();
}

void Bot::TaskNormal () {
  aim_flags_ |= AimFlags::Nav;

  const int debug_goal = cv_debug_goal.As<int> ();

  // user forced a node as a goal?
  if (graph.Exists (debug_goal)) {
    if (Task ()->data != debug_goal) {
      ClearSearchNodes ();

      Task ()->data = debug_goal;
      chosen_goal_index_ = debug_goal;
    }
    const auto &debug_origin = graph[debug_goal].origin;
    const auto distance_to_debug_origin_sq = debug_origin.distance_sq (pev->origin);

    if (!IsDucking () && distance_to_debug_origin_sq < ystl::sqrf (172.0f)) {
      move_speed_ = pev->maxspeed * 0.4f;
    }

    // stop the bot if precisely reached debug goal
    if (current_node_index_ == debug_goal) {
      if (distance_to_debug_origin_sq < ystl::sqrf (22.0f) && util.IsVisible (debug_origin, Ent ())) {

        move_to_goal_ = false;
        check_terrain_ = false;

        move_speed_ = 0.0f;
        strafe_speed_ = 0.0f;

        return;
      }
    }
  }

  // round time running out: hostage carriers stop passive camping and deliver at full speed
  if (game.MapIs (MapFlags::HostageRescue) && team_ == Team::CT && has_hostage_ && ShouldRushEndgameTime ()) {
    if (const auto tid = GetTaskId (); tid == TaskId::Camp || tid == TaskId::Pause) {
      CompleteTask ();
      ClearSearchNodes ();

      prev_goal_index_ = kInvalidNodeIndex;
      Task ()->data = kInvalidNodeIndex;
    }
    min_speed_ = pev->maxspeed;
  }

  // bots rushing with knife, when have no enemy (thanks for idea to nicebot project)
  if (cv_random_knife_attacks && UsesKnife () && !game.IsAliveEntity (last_enemy_) && game.IsNullEntity (enemy_) &&
      knife_attack_timer_.elapsed () && !has_hostage_ && !HasShield () && NumFriendsNear (pev->origin, 96.0f) == 0) {

    if (rg.chance (40)) {
      pev->button |= IN_ATTACK;
    }
    else {
      pev->button |= IN_ATTACK2;
    }
    knife_attack_timer_.start (rg (2.5f, 6.0f));
  }
  const auto &prop = conf.GetWeaponProp (current_weapon_);

  if (reload_data_.state == Reload::None && GetAmmo () != 0 && GetAmmoInClip () < 5 && prop.ammo1 != -1) {
    reload_data_.state = Reload::Primary;
  }

  // if bomb planted and it's a ct calculate new path to bomb point if he's not already heading for
  if (!bomb_search_overridden_ && game_state.IsBombPlanted () && team_ == Team::CT && graph.Exists (Task ()->data) &&
      !has_flag (graph[Task ()->data].flags, NodeFlag::Goal) && GetTaskId () != TaskId::EscapeFromBomb) {

    ClearSearchNodes ();
    Task ()->data = kInvalidNodeIndex;
  }

  // reached the destination (goal) node?
  if (UpdateNavigation ()) {
    // if we're reached the goal, and there is not enemies, notify the team
    if (!game_state.IsBombPlanted () && current_node_index_ != kInvalidNodeIndex && has_flag (path_flags_, NodeFlag::Goal) && rg.chance (15) &&
        NumEnemiesNear (pev->origin, 650.0f) == 0) {

      PushRadioChat (RadioChat::SectorClear);
    }

    CompleteTask ();
    prev_goal_index_ = kInvalidNodeIndex;

    // spray logo sometimes if allowed to do so
    if (!has_flag (states_, Sense::SeeingEnemy | Sense::SuspectEnemy) && see_enemy_timer_.greater_than (5.0f) &&
        reload_data_.state == Reload::None && logo_spray_timer_.elapsed () && rg (1, 100) < cv_spraypaints.As<int> () &&
        pev->groundentity == game.GetStartEntity () && move_speed_ >= GetShiftSpeed () && game.IsNullEntity (pickup_item_)) {

      if (!(game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted () && team_ == Team::CT)) {
        StartTask (TaskId::Spraypaint, TaskPri::kSpraypaint, kInvalidNodeIndex, game.Time () + 1.0f, false);
      }
    }

    // reached node is a camp node
    if (has_flag (path_flags_, NodeFlag::Camp) && !game.Is (GameFlags::CSDM) && cv_camping_allowed && !IsKnifeMode ()) {
      const bool allowed_camp_weapon =
        HasPrimaryWeapon () || HasShield () || (HasSecondaryWeapon () && !HasPrimaryWeapon () && num_friends_left_ > game.MaxClients () / 6);

      // check if bot has got a primary weapon and hasn't camped before
      if (allowed_camp_weapon && time_camping_ + 10.0f < game.Time () && !has_hostage_ && !ShouldRushEndgameTime ()) {
        bool camping_allowed = true;

        // check if it's not allowed for this team to camp here
        if (team_ == Team::Terrorist) {
          if (has_flag (path_flags_, NodeFlag::CTOnly)) {
            camping_allowed = false;
          }
        }
        else {
          if (has_flag (path_flags_, NodeFlag::TerroristOnly)) {
            camping_allowed = false;
          }
        }

        // don't allow vip on as_ maps to camp + don't allow terrorist carrying c4 to camp
        if (is_vip_ || (game.MapIs (MapFlags::Demolition) && team_ == Team::Terrorist && !game_state.IsBombPlanted () && has_c4_)) {
          camping_allowed = false;
        }

        // check if another bot is already camping here
        if (IsOccupiedNode (current_node_index_)) {
          camping_allowed = false;
        }

        // skip sniper node if we don't have sniper weapon
        if (!UsesSniper () && has_flag (path_flags_, NodeFlag::Sniper)) {
          camping_allowed = false;
        }

        if (camping_allowed) {
          // crouched camping here?
          if (has_flag (path_flags_, NodeFlag::Crouch)) {
            camp_buttons_ = IN_DUCK;
          }
          else {
            camp_buttons_ = 0;
          }
          SelectBestWeapon ();

          if (!has_flag (states_, Sense::SeeingEnemy | Sense::HearingEnemy) && reload_data_.state == Reload::None) {
            reload_data_.state = Reload::Primary;
          }
          time_camping_ = game.Time () + rg (cv_camping_time_min.As<float> (), cv_camping_time_max.As<float> ());
          StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, time_camping_, true);

          look_at_safe_ = path_origin_ + path_->start.forward () * 500.0f;
          aim_flags_ |= AimFlags::Camp;
          camp_direction_ = 0;

          // tell the world we're camping
          if (rg.chance (25)) {
            PushRadioChat (RadioChat::ImInPosition);
          }
          move_to_goal_ = false;
          check_terrain_ = false;

          move_speed_ = 0.0f;
          strafe_speed_ = 0.0f;
        }
      }
    }
    else {
      // some goal nodes are map dependent so check it out
      if (game.MapIs (MapFlags::HostageRescue)) {
        // ct bot has some hostages following?
        if (team_ == Team::CT && has_hostage_) {

          // and reached a rescue point?
          if (in_rescue_zone_ && has_flag (path_flags_, NodeFlag::Rescue)) {
            hostages_.clear ();
          }
        }
        else if (team_ == Team::Terrorist && rg.chance (75) && !game.MapIs (MapFlags::Demolition)) {
          const int index = FindDefendNode (path_->origin);

          StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + rg (60.0f, 120.0f), true); // add/update camp task
          StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + rg (5.0f, 10.0f), true); // add/update move task

          // decide to duck or not to duck
          SelectCampButtons (index);
        }
      }

      // was elseif here but brokes csde_ scenario
      if (game.MapIs (MapFlags::Demolition) && has_flag (path_flags_, NodeFlag::Goal) && in_bomb_zone_) {
        // is it a terrorist carrying the bomb?
        if (has_c4_) {
          if (has_flag (states_, Sense::SeeingEnemy) && NumFriendsNear (pev->origin, 768.0f) == 0) {
            // request an help also
            PushRadioChat (RadioChat::NeedBackup);
            PushRadioChat (RadioChat::ScaredEmotion);

            StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + rg (4.0f, 8.0f), true);
          }
          else {
            StartTask (TaskId::PlantBomb, TaskPri::kPlantBomb, kInvalidNodeIndex, 0.0f, true);
          }
        }
        else if (team_ == Team::CT) {
          if (!game_state.IsBombPlanted () && NumFriendsNear (pev->origin, 210.0f) < 4) {
            const int index = FindDefendNode (path_->origin);
            float camp_time = rg (25.0f, 40.0f);

            // rusher bots don't like to camp too much
            if (personality_ == Personality::Rusher) {
              camp_time *= 0.5f;
            }
            StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + camp_time, true); // add/update camp task
            StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + rg (5.0f, 11.0f), true); // add/update move task

            // decide to duck or not to duck
            SelectCampButtons (index);

            PushRadioChat (RadioChat::DefendingBombsite); // play info about that
          }
        }
      }
    }
  }
  // no more nodes to follow - search new ones (or we have a bomb)
  else if (!HasActiveGoal ()) {
    IgnoreCollision ();

    // did we already decide about a goal before?
    const auto curr_index = Task ()->data;

    // respect debug goal even for c4/vip bots
    auto dest_index =
      graph.Exists (debug_goal) ? debug_goal : (graph.Exists (curr_index) && !has_c4_ && !is_vip_ ? curr_index : FindBestGoal ());

    // check for existence (this is fail over, for i.e
    if (!graph.Exists (dest_index)) {
      dest_index = FindFarestNode (pev->origin, 1024.0f);
    }
    prev_goal_index_ = dest_index;

    // remember index
    Task ()->data = dest_index;

    auto pst = path_type_;

    // override with fast path
    if (game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted ()) {
      pst = rg.chance (80) ? FindPathType::Fast : FindPathType::Optimal;
    }
    EnsureCurrentNodeIndex ();

    // do pathfinding if it's not the current
    if (dest_index != current_node_index_) {
      FindPath (current_node_index_, dest_index, pst);
    }
  }
  else {
    if (!ShouldRushEndgameTime () && !IsDucking () && !UsesKnife () && !ystl::fequal (min_speed_, pev->maxspeed) && min_speed_ > 1.0f) {
      move_speed_ = min_speed_;
    }
  }
  const float shift_speed = GetShiftSpeed ();

  if ((!ystl::fzero (move_speed_) && move_speed_ > shift_speed) && (cv_walking_allowed && mp_footsteps) &&
      rg.chance (ystl::max (25, Skill ())) && (heard_sound_timer_.less_than (6.0f) || has_flag (states_, Sense::HearingEnemy)) &&
      NumEnemiesNear (pev->origin, 1024.0f) >= 1 && !IsKnifeMode () && !game_state.IsBombPlanted ()) {

    move_speed_ = shift_speed;
  }

  // bot hasn't seen anything in a long time and is asking his teammates to report in
  if (cv_radio_mode.As<int> () > 1 && bots.GetLastRadio (team_) != RadioChat::ReportInTeam &&
      game_state.GetRoundStartTime () + 20.0f < game.Time () && ask_check_timer_.elapsed () && rg.chance (15) &&
      see_enemy_timer_.greater_than (rg (45.0f, 80.0f)) && NumFriendsNear (pev->origin, 1024.0f) == 0) {

    PushRadioChat (RadioChat::ReportInTeam);

    ask_check_timer_.start (rg (45.0f, 80.0f));

    // make sure everyone else will not ask next few moments
    for (auto &bot : bots) {
      if (bot.is_alive_) {
        bot.ask_check_timer_.start (rg (5.0f, 30.0f));
      }
    }
  }
}

void Bot::TaskSpraypaint () {
  aim_flags_ |= AimFlags::Entity;

  // bot didn't spray this round?
  if (logo_spray_timer_.elapsed () && Task ()->time > game.Time ()) {
    const ystl::Vector forward = pev->v_angle.forward ();
    ystl::Vector spray_origin = GetEyesPos () + forward * 128.0f;

    Trace::Result tr {};
    trace.Line (GetEyesPos (), spray_origin, TraceIgnore::Monsters, Ent (), &tr);

    // no wall in front?
    if (tr.fraction >= 1.0f) {
      spray_origin.z -= 128.0f;
    }
    entity_ = spray_origin;

    if (Task ()->time - 0.5f < game.Time ()) {
      // emit spray can sound
      engfuncs.pfnEmitSound (Ent (), CHAN_VOICE, "player/sprayer.wav", 1.0f, ATTN_NORM, 0, 100);

      trace.Line (GetEyesPos (), GetEyesPos () + forward * 128.0f, TraceIgnore::Monsters, Ent (), &tr);

      // paint the actual logo decal
      util.DecalTrace (&tr, logo_decal_index_);
      logo_spray_timer_.start (rg (60.0f, 90.0f));
    }
  }
  else {
    CompleteTask ();
  }
  move_to_goal_ = false;
  check_terrain_ = false;

  nav_timer_.start ();
  move_speed_ = 0.0f;
  strafe_speed_ = 0.0f;

  IgnoreCollision ();
}

void Bot::TaskHunt () {
  aim_flags_ |= AimFlags::Nav;

  // if we've got new enemy
  if (!game.IsNullEntity (enemy_) || game.IsNullEntity (last_enemy_)) {

    // forget about it
    ClearTask (TaskId::Hunt);
    prev_goal_index_ = kInvalidNodeIndex;
  }
  else if (game.GetPlayerTeam (last_enemy_) == team_) {

    // don't hunt down our teammate
    ClearTask (TaskId::Hunt);

    prev_goal_index_ = kInvalidNodeIndex;
    last_enemy_ = nullptr;
  }
  else if (UpdateNavigation ()) // reached last enemy pos?
  {
    // forget about it
    CompleteTask ();

    prev_goal_index_ = kInvalidNodeIndex;
    last_enemy_origin_.clear ();
  }

  // do we need to calculate a new path?
  else if (!HasActiveGoal ()) {
    int dest_index = kInvalidNodeIndex;
    const int goal = Task ()->data;

    // is there a remembered index?
    if (graph.Exists (goal)) {
      dest_index = goal;
    }

    // find new one instead
    else {
      dest_index = graph.GetNearest (last_enemy_origin_);
    }

    // remember index
    prev_goal_index_ = dest_index;
    Task ()->data = dest_index;

    if (dest_index != current_node_index_) {
      FindPath (current_node_index_, dest_index, FindPathType::Fast);
    }
  }

  // bots skill higher than 60?
  if (cv_walking_allowed && mp_footsteps && rg.chance (ystl::max (25, Skill ()))) {

    // then make him move slow if near enemy
    if (current_node_index_ != kInvalidNodeIndex && !has_flag (current_travel_flags_, PathFlag::Jump)) {
      if (path_->radius < 32.0f && !IsOnLadder () && !IsInWater () && see_enemy_timer_.less_than (4.0f)) {
        move_speed_ = GetShiftSpeed ();
      }
    }
  }
}

void Bot::TaskSeekCover () {
  aim_flags_ |= AimFlags::Nav;

  if (!game.IsAliveEntity (last_enemy_)) {
    cover_search_timer_.start (rg (3.0f, 6.0f));

    CompleteTask ();
    prev_goal_index_ = kInvalidNodeIndex;
  }

  // reached final node?
  else if (UpdateNavigation ()) {
    cover_search_timer_.start (rg (3.0f, 6.0f));

    // yep. activate hide behavior
    CompleteTask ();
    prev_goal_index_ = kInvalidNodeIndex;

    // start hide task
    StartTask (TaskId::Hide, TaskPri::kHide, kInvalidNodeIndex, game.Time () + rg (3.0f, 12.0f), false);

    // get a valid look direction
    const ystl::Vector dest = GetCampDirection (last_enemy_origin_);

    aim_flags_ |= AimFlags::Camp;
    look_at_safe_ = dest;
    camp_direction_ = 0;

    // chosen node is a camp node?
    if (has_flag (path_flags_, NodeFlag::Camp)) {
      // use the existing camp node prefs
      if (has_flag (path_flags_, NodeFlag::Crouch)) {
        camp_buttons_ = IN_DUCK;
      }
      else {
        camp_buttons_ = 0;
      }
    }
    else {
      // choose a crouch or stand pos
      if (path_->vis.crouch <= path_->vis.stand) {
        camp_buttons_ = IN_DUCK;
      }
      else {
        camp_buttons_ = 0;
      }

      // enter look direction from previously calculated positions
      if (!dest.empty ()) {
        look_at_safe_ = dest;
      }
    }

    if (reload_data_.state == Reload::None && GetAmmoInClip () < 5 && GetAmmo () != 0) {
      reload_data_.state = Reload::Primary;
    }
    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;

    move_to_goal_ = false;
    check_terrain_ = false;
  }
  else if (!HasActiveGoal ()) {
    int dest_index = kInvalidNodeIndex;

    if (Task ()->data != kInvalidNodeIndex) {
      dest_index = Task ()->data;
    }
    else {
      dest_index = FindCoverNode (infected_enemy_team_ ? 2048.0f : 1024.0f);

      if (dest_index == kInvalidNodeIndex) {
        cover_search_timer_.start (rg (3.0f, 6.0f));

        prev_goal_index_ = kInvalidNodeIndex;

        CompleteTask ();
        return;
      }
    }
    camp_direction_ = 0;

    prev_goal_index_ = dest_index;
    Task ()->data = dest_index;

    EnsureCurrentNodeIndex ();

    if (dest_index != current_node_index_) {
      FindPath (current_node_index_, dest_index, FindPathType::Fast);
    }
  }
}

void Bot::TaskAttack () {
  move_to_goal_ = false;
  check_terrain_ = false;

  // always ignore collision checks in this task
  IgnoreCollision ();

  if (!game.IsNullEntity (enemy_)) {
    AttackMovement ();

    if (UsesKnife () && !enemy_origin_.empty ()) {
      dest_origin_ = enemy_origin_;
    }
  }
  else {
    CompleteTask ();
    FindNextBestNode ();

    if (!last_enemy_origin_.empty ()) {
      dest_origin_ = last_enemy_origin_;
    }
  }
  nav_timer_.start ();
}

void Bot::TaskPause () {
  move_to_goal_ = false;
  check_terrain_ = false;

  nav_timer_.start ();
  move_speed_ = 0.0f;
  strafe_speed_ = 0.0f;

  aim_flags_ |= AimFlags::Nav;

  // is bot blinded and skilled enough to spray back?
  if (view_distance_ < 500.0f && rg.chance (ystl::max (25, Skill ()))) {
    // go mad!
    move_speed_ = -ystl::abs ((view_distance_ - 500.0f) * 0.5f);

    if (move_speed_ < -pev->maxspeed) {
      move_speed_ = -pev->maxspeed;
    }
    look_at_safe_ = GetEyesPos () + pev->v_angle.forward () * 500.0f;

    aim_flags_ |= AimFlags::Override;
    wants_to_fire_ = true;
  }
  else {
    pev->button |= camp_buttons_;
  }

  // stop camping if time over or gets hurt by something else than bullets
  if (Task ()->time < game.Time () || last_damage_type_ > 0) {
    CompleteTask ();
  }
}

void Bot::TaskBlind () {
  move_to_goal_ = false;
  check_terrain_ = false;
  nav_timer_.start ();

  // if bot remembers last enemy position
  if (rg.chance (Skill ()) && !last_enemy_origin_.empty () && game.IsPlayerEntity (last_enemy_) && !UsesSniper ()) {

    auto error = kSprayDistance * last_enemy_origin_.distance (pev->origin) / 2048.0f;
    auto origin = last_enemy_origin_;

    origin.x = origin.x + rg (-error, error);
    origin.y = origin.y + rg (-error, error);

    look_at_ = origin; // face last enemy
    wants_to_fire_ = true; // and shoot it
  }

  if (graph.Exists (blind_node_index_) && rg.chance (50 + Skill () / 2)) {
    if (UpdateNavigation ()) {
      prev_goal_index_ = kInvalidNodeIndex;
      blind_node_index_ = kInvalidNodeIndex;

      blind_move_speed_ = 0.0f;
      blind_side_move_speed_ = 0.0f;
      blind_button_ = 0;

      states_ |= Sense::SuspectEnemy;
      CompleteTask ();
    }
    else if (!HasActiveGoal ()) {
      EnsureCurrentNodeIndex ();

      prev_goal_index_ = blind_node_index_;
      Task ()->data = blind_node_index_;

      FindPath (current_node_index_, blind_node_index_, FindPathType::Fast);
    }
  }
  else {
    move_speed_ = blind_move_speed_;
    strafe_speed_ = blind_side_move_speed_;
    pev->button |= blind_button_;

    states_ |= Sense::SuspectEnemy;
  }

  if (blind_timer_.elapsed ()) {
    CompleteTask ();
  }
}

void Bot::TaskCamp () {
  if (!cv_camping_allowed || IsKnifeMode ()) {
    CompleteTask ();
    return;
  }

  aim_flags_ |= AimFlags::Camp;
  check_terrain_ = false;
  move_to_goal_ = false;

  if (team_ == Team::CT && game_state.IsBombPlanted () && !IsBombDefusing (game_state.GetBombOrigin ())) {
    const bool bomb_far_away = pev->origin.distance_sq (game_state.GetBombOrigin ()) > ystl::sqrf (kBombHearDistance);

    // stop holding the spot if we were defending, or if the bomb is ticking elsewhere and must be searched
    if (bomb_far_away || (defended_bomb_ && !IsOutOfBombTimer ())) {
      defended_bomb_ = false;
      CompleteTask ();
      return;
    }
  }
  IgnoreCollision ();

  // half the reaction time if camping because you're more aware of enemies if camping
  SetIdealReactionTimers ();
  ideal_reaction_time_ *= 0.5f;

  nav_timer_.start ();
  time_camping_ = game.Time ();

  move_speed_ = 0.0f;
  strafe_speed_ = 0.0f;

  FindValidNode ();

  // random camp dir, or prediction
  auto use_random_camp_dir_or_predict_enemy = [&] () {
    if (!last_enemy_origin_.empty () && game.IsAliveEntity (last_enemy_)) {
      auto path_length = last_predict_length_;
      auto predict_node = last_predict_index_;

      if (IsNodeValidForPredict (predict_node) && path_length > 1 && vistab.Visible (predict_node, current_node_index_)) {

        look_at_safe_ = graph[predict_node].origin + pev->view_ofs;
      }
    }
    else {
      look_at_safe_ = graph[GetRandomCampDir ()].origin + pev->view_ofs;
    }
  };

  if (next_camp_dir_timer_.elapsed ()) {
    if (has_flag (path_flags_, NodeFlag::Camp)) {
      ystl::Vector dest {};

      // switch from 1 direction to the other
      if (camp_direction_ < 1) {
        dest = path_->start;
        camp_direction_ = 1;
      }
      else {
        dest = path_->end;
        camp_direction_ = 0;
      }
      dest.z = 0.0f;

      // check if after the conversion camp start and camp end are broken, and bot will look into the wall
      Trace::Result tr {};

      // and use real angles to check it
      const ystl::Vector to = path_origin_ + dest.forward () * 500.0f;

      // let's check the destination
      trace.Line (GetEyesPos (), to, TraceIgnore::Monsters, Ent (), &tr);

      // we're probably facing the wall, so ignore the flags provided by graph, and use our own
      if (tr.fraction < 0.5f) {
        use_random_camp_dir_or_predict_enemy ();
      }
      else {
        look_at_safe_ = to;
      }
    }
    else {
      use_random_camp_dir_or_predict_enemy ();
    }
    next_camp_dir_timer_.start (rg (1.0f, 4.0f));
  }
  // press remembered crouch button
  pev->button |= camp_buttons_;

  // stop camping if time over or gets hurt by something else than bullets
  if (Task ()->time < game.Time () || last_damage_type_ > 0) {
    CompleteTask ();
  }
}

void Bot::TaskHide () {
  if (is_creature_) {
    CompleteTask ();
    return;
  }

  aim_flags_ |= AimFlags::Camp;
  check_terrain_ = false;
  move_to_goal_ = false;

  // half the reaction time if camping
  SetIdealReactionTimers ();
  ideal_reaction_time_ *= 0.5f;

  nav_timer_.start ();
  move_speed_ = 0.0f;
  strafe_speed_ = 0.0f;

  if (HasShield () && !reload_data_.is_reloading) {
    if (!IsShieldDrawn ()) {
      pev->button |= IN_ATTACK2; // draw the shield!
    }
    else {
      pev->button |= IN_DUCK; // duck under if the shield is already drawn
    }
  }

  // if we see an enemy and aren't at a good camping point leave the spot
  if (has_flag (states_, Sense::SeeingEnemy) || in_bomb_zone_) {
    if (!has_flag (path_flags_, NodeFlag::Camp)) {
      CompleteTask ();

      camp_buttons_ = 0;
      prev_goal_index_ = kInvalidNodeIndex;

      return;
    }
  }

  // if we don't have an enemy we're also free to leave
  else if (last_enemy_origin_.empty ()) {
    CompleteTask ();

    camp_buttons_ = 0;
    prev_goal_index_ = kInvalidNodeIndex;

    return;
  }

  pev->button |= camp_buttons_;
  nav_timer_.start ();

  if (!reload_data_.is_reloading) {
    CheckReload ();
  }

  // stop camping if time over or gets hurt by something else than bullets
  if (Task ()->time < game.Time () || last_damage_type_ > 0) {
    CompleteTask ();
  }
}

void Bot::TaskMoveTo () {
  aim_flags_ |= AimFlags::Nav;

  if (IsShieldDrawn ()) {
    pev->button |= IN_ATTACK2;
  }

  auto ensure_dest_index_ok = [&] (int &index) {
    if (!position_.empty () && IsOccupiedNode (index)) {
      index = FindDefendNode (position_);
    }
  };

  // reached destination?
  if (UpdateNavigation ()) {
    CompleteTask (); // we're done

    prev_goal_index_ = kInvalidNodeIndex;
    position_.clear ();
  }
  // didn't choose goal node yet?
  else if (!HasActiveGoal ()) {
    int dest_index = kInvalidNodeIndex;
    const int goal = Task ()->data;

    if (graph.Exists (goal)) {
      dest_index = goal;

      // check if we're ok
      ensure_dest_index_ok (dest_index);
    }
    else {
      dest_index = graph.GetNearest (position_);

      // check if we're ok
      ensure_dest_index_ok (dest_index);
    }

    if (graph.Exists (dest_index)) {
      prev_goal_index_ = dest_index;
      Task ()->data = dest_index;

      EnsureCurrentNodeIndex ();
      FindPath (current_node_index_, dest_index, is_creature_ ? FindPathType::Fast : path_type_);
    }
    else {
      CompleteTask ();
    }
  }
}

void Bot::TaskPlantBomb () {
  aim_flags_ |= AimFlags::Camp;

  // we're still got the c4?
  if (has_c4_ && !IsKnifeMode ()) {
    if (current_weapon_ != Weapon::C4) {
      SelectWeaponById (Weapon::C4);
    }

    if (game.IsAliveEntity (enemy_) || !in_bomb_zone_) {
      if (!has_progress_bar_) {
        CompleteTask ();
      }
    }
    else {
      move_to_goal_ = false;
      check_terrain_ = false;
      nav_timer_.start ();

      if (has_flag (path_flags_, NodeFlag::Crouch)) {
        pev->button |= (IN_ATTACK | IN_DUCK);
      }
      else {
        pev->button |= IN_ATTACK;
      }
      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;
    }
    IgnoreCollision ();
  }

  // done with planting
  else {
    CompleteTask ();

    // tell teammates to move over here
    if (NumFriendsNear (pev->origin, 1200.0f) > 0) {
      PushRadioChat (RadioChat::NeedBackup);
    }
    const auto index = FindDefendNode (pev->origin);
    const auto guard_time = mp_c4timer.As<float> () * 0.5f + mp_c4timer.As<float> () * 0.25f;

    // add/update camp task
    StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + guard_time, true);

    // add/update move task
    StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + guard_time, true);

    // decide to duck or not to duck
    SelectCampButtons (index);
  }
}

void Bot::TaskDefuseBomb () {
  const float full_defuse_time = has_defuser_ ? 7.0f : 12.0f;
  const float time_to_blow_up = game_state.GetBombTimeLeft ();

  float defuse_remaining_time = full_defuse_time;

  if (has_progress_bar_) {
    if (ystl::fzero (Task ()->time)) {
      Task ()->time = game.Time ();
    }
    defuse_remaining_time = full_defuse_time - (game.Time () - Task ()->time);
  }

  const auto &bomb_pos = game_state.GetBombOrigin ();
  bool defuse_error = false;

  // exception: bomb has been defused
  if (bomb_pos.empty ()) {
    defuse_watch_timer_.invalidate ();
    CompleteTask ();

    // defused bomb is gone: drop the stale pursuit so it can't keep pulling the bot back
    entity_.clear ();
    pickup_item_ = nullptr;
    pickup_type_ = Pickup::None;

    for (auto &bot : bots) {
      if (&bot == this || bot.team_ != team_ || !bot.is_alive_) {
        continue;
      }
      auto defend_point = bot.FindFarestNode (bot.pev->origin);

      bot.StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + rg (30.0f, 60.0f), true); // add/update camp task
      bot.StartTask (TaskId::MoveTo, TaskPri::kMoveTo, defend_point, game.Time () + rg (3.0f, 6.0f), true); // add/update move task
    }
    game_state.SetBombOrigin (true);

    if (num_friends_left_ != 0 && rg.chance (50)) {
      if (time_to_blow_up <= 3.0f) {
        if (cv_radio_mode.As<int> () == 2) {
          PushRadioChat (RadioChat::BarelyDefused);
        }
        else if (cv_radio_mode.As<int> () == 1) {
          PushRadioChat (RadioChat::SectorClear);
        }
      }
      else {
        PushRadioChat (RadioChat::SectorClear);
      }
    }
    return;
  }
  else if (defuse_remaining_time > time_to_blow_up) {
    defuse_error = true;
  }
  else if (has_flag (states_, Sense::SeeingEnemy)) {
    const int friends = NumFriendsNear (pev->origin, 768.0f);

    if (friends < 2 && defuse_remaining_time < time_to_blow_up) {
      defuse_error = true;

      if (defuse_remaining_time + 2.0f > time_to_blow_up) {
        defuse_error = false;
      }

      if (num_enemies_left_ > 0 && num_friends_left_ > friends) {
        PushRadioChat (RadioChat::NeedBackup);
      }
    }
  }

  // one of exceptions is thrown. finish task
  if (defuse_error) {
    defuse_watch_timer_.invalidate ();

    entity_.clear ();

    pickup_item_ = nullptr;
    pickup_type_ = Pickup::None;

    SelectBestWeapon ();
    ResetCollision ();

    CompleteTask ();

    return;
  }

  // if the defuse hasn't actually started within a reasonable time (i.e
  if (!has_progress_bar_) {
    if (!defuse_watch_timer_.started ()) {
      defuse_watch_timer_.start (rg (4.0f, 6.0f));
    }
    else if (defuse_watch_timer_.elapsed ()) {
      defuse_watch_timer_.invalidate ();

      entity_.clear ();

      pickup_item_ = nullptr;
      pickup_type_ = Pickup::None;

      SelectBestWeapon ();
      ResetCollision ();

      CompleteTask ();

      return;
    }
  }
  else {
    defuse_watch_timer_.invalidate ();
  }

  // to revert from pause after reload  ting && just to be sure
  move_to_goal_ = false;

  // keep terrain checks until the defuse actually starts
  check_terrain_ = !has_progress_bar_;

  move_speed_ = pev->maxspeed;
  strafe_speed_ = 0.0f;

  // bot is reloading and we close enough to start defusing
  if (reload_data_.is_reloading && bomb_pos.distance_sq2d (pev->origin) < ystl::sqrf (80.0f)) {
    if (num_enemies_left_ == 0 || time_to_blow_up < full_defuse_time + 7.0f ||
        ((GetAmmoInClip () > 8 && reload_data_.state == Reload::Primary) || (GetAmmoInClip () > 5 && reload_data_.state == Reload::Secondary))) {

      const int weapon_index = GetBestOwnedWeaponIndex ();

      // just select knife and then select weapon
      SelectWeaponById (Weapon::Knife);

      if (weapon_index > 0 && weapon_index < kNumWeapons) {
        SelectWeaponByIndex (weapon_index);
      }
      reload_data_.is_reloading = false;
    }
    else {
      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;
    }
  }

  // head to bomb and press use button
  aim_flags_ |= AimFlags::Entity;

  dest_origin_ = bomb_pos;
  entity_ = bomb_pos;

  // prefer the carried bomb, fall back to the tracked bomb entity
  auto bomb_entity = pickup_item_;

  if (game.IsNullEntity (bomb_entity)) {
    bomb_entity = game_state.GetBombEntity ();
  }

  const bool bomb_close = !game.IsNullEntity (bomb_entity) && game.GetEntityOrigin (bomb_entity).distance_sq (pev->origin) < ystl::sqrf (96.0f);

  pev->button |= IN_USE;

  // if defusing is not already started, maybe crouch before
  if (!has_progress_bar_ && duck_defuse_check_timer_.elapsed ()) {
    ystl::Vector bot_duck_origin {}, bot_stand_origin {};

    if (pev->button & IN_DUCK) {
      bot_duck_origin = pev->origin;
      bot_stand_origin = pev->origin + ystl::Vector (0.0f, 0.0f, 18.0f);
    }
    else {
      bot_duck_origin = pev->origin - ystl::Vector (0.0f, 0.0f, 18.0f);
      bot_stand_origin = pev->origin;
    }

    const float duck_distance_sq = entity_.distance_sq (bot_duck_origin);
    const float stand_distance_sq = entity_.distance_sq (bot_stand_origin);

    if (duck_distance_sq > ystl::sqrf (75.0f) || stand_distance_sq > ystl::sqrf (75.0f)) {
      if (stand_distance_sq < duck_distance_sq) {
        duck_defuse_ = false; // stand
      }
      else {
        duck_defuse_ = num_enemies_left_ != 0 && rg.chance (Skill ()); // duck
      }
    }
    duck_defuse_check_timer_.start (5.0f);
  }

  // press duck button
  if (duck_defuse_ || (old_buttons_ & IN_DUCK)) {
    pev->button |= IN_DUCK;
  }
  else {
    pev->button &= ~IN_DUCK;
  }

  // we are defusing bomb
  if (has_progress_bar_ || (old_buttons_ & IN_USE) || !game.IsNullEntity (pickup_item_) || bomb_close) {
    pev->button |= IN_USE;

    if (bomb_close) {
      MDLL_Use (bomb_entity, Ent ());
    }

    // defusing cancels any reload intent
    reload_data_.state = Reload::None;
    nav_timer_.start ();

    // don't move when defusing
    move_to_goal_ = false;
    check_terrain_ = false;

    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;

    // notify team
    if (num_friends_left_ > 0) {
      PushRadioChat (RadioChat::DefusingBomb);

      if (num_enemies_left_ > 0 && NumFriendsNear (pev->origin, 512.0f) < 2) {
        PushRadioChat (RadioChat::NeedBackup);
      }
    }
  }
  else {
    CompleteTask ();
  }
}

void Bot::TaskFollowUser () {
  if (game.IsNullEntity (target_entity_) || !game.IsAliveEntity (target_entity_)) {
    target_entity_ = nullptr;
    CompleteTask ();

    return;
  }

  if (target_entity_->v.button & IN_ATTACK) {
    Trace::Result tr {};

    const ystl::Vector eyes = target_entity_->v.origin + target_entity_->v.view_ofs;
    trace.Line (eyes, eyes + target_entity_->v.v_angle.forward () * 500.0f, TraceIgnore::Everything, Ent (), &tr);

    if (!game.IsNullEntity (tr.hit) && game.IsPlayerEntity (tr.hit) && game.GetPlayerTeam (tr.hit) != team_) {
      target_entity_ = nullptr;
      last_enemy_ = tr.hit;
      last_enemy_origin_ = tr.hit->v.origin;

      CompleteTask ();
      return;
    }
  }

  if (!ystl::fzero (target_entity_->v.maxspeed) && target_entity_->v.maxspeed < pev->maxspeed) {
    move_speed_ = target_entity_->v.maxspeed;

    ResetCollision ();
  }

  if (reload_data_.state == Reload::None && GetAmmo () != 0) {
    reload_data_.state = Reload::Primary;
  }

  if (target_entity_->v.origin.distance_sq (pev->origin) > ystl::sqrf (130.0f)) {
    follow_wait_timer_.invalidate ();
  }
  else {
    move_speed_ = 0.0f;

    if (!follow_wait_timer_.started ()) {
      follow_wait_timer_.start (3.0f);
    }
    else {
      if (follow_wait_timer_.elapsed ()) {
        // stop following if we have been waiting too long
        target_entity_ = nullptr;

        PushRadioChat (RadioChat::YouTakeThePoint);
        CompleteTask ();

        return;
      }
    }
  }
  aim_flags_ |= AimFlags::Nav;

  if (cv_walking_allowed && target_entity_->v.maxspeed < move_speed_ && !IsKnifeMode ()) {
    move_speed_ = GetShiftSpeed ();
  }

  if (IsShieldDrawn ()) {
    pev->button |= IN_ATTACK2;
  }

  // reached destination?
  if (UpdateNavigation ()) {
    Task ()->data = kInvalidNodeIndex;
  }

  // didn't choose goal node yet?
  if (!HasActiveGoal ()) {
    int dest_index = graph.GetNearest (target_entity_->v.origin);
    auto points = graph.GetNearestInRadius (200.0f, target_entity_->v.origin);

    for (const auto &new_index : points) {
      // if node not yet used, assign it as dest
      if (new_index != current_node_index_ && !IsOccupiedNode (new_index)) {
        dest_index = new_index;
      }
    }

    if (graph.Exists (dest_index) && graph.Exists (current_node_index_)) {
      prev_goal_index_ = dest_index;
      Task ()->data = dest_index;

      // always take the shortest path
      FindPath (current_node_index_, dest_index, FindPathType::Fast);
    }
    else {
      target_entity_ = nullptr;
      CompleteTask ();
    }
  }
}

void Bot::TaskThrowExplosive () {
  ystl::Vector dest = throw_;

  if (!has_flag (states_, Sense::SeeingEnemy)) {
    if (!cv_move_during_throw) {
      strafe_speed_ = 0.0f;
      move_speed_ = 0.0f;
      move_to_goal_ = false;
    }
  }
  else if (!has_flag (states_, Sense::SuspectEnemy) && !game.IsNullEntity (enemy_)) {
    dest = enemy_->v.origin + enemy_->v.velocity.get2d ();
  }
  is_using_grenade_ = true;
  check_terrain_ = false;

  IgnoreCollision ();

  if (!IsGrenadeWar () && pev->origin.distance_sq (dest) < ystl::sqrf (kGrenadeDamageRadius)) {
    // heck, i don't wanna blow up myself
    grenade_check_timer_.start (kGrenadeCheckTime * 2.0f);

    SelectBestWeapon ();
    CompleteTask ();

    return;
  }
  grenade_ = CalcThrow (GetEyesPos (), dest);

  if (grenade_.length_sq () < 100.0f) {
    grenade_ = CalcToss (pev->origin, dest);
  }

  if (!IsGrenadeWar () && grenade_.length_sq () <= 100.0f) {
    grenade_check_timer_.start (kGrenadeCheckTime * 2.0f);

    SelectBestWeapon ();
    CompleteTask ();
  }
  else {
    aim_flags_ |= AimFlags::Grenade;

    auto grenade = SetCorrectGrenadeVelocity (kExplosiveModelName);

    if (game.IsNullEntity (grenade)) {
      if (current_weapon_ != Weapon::Explosive) {
        if (has_flag (pev->weapons, ystl::bit (Weapon::Explosive))) {
          SelectWeaponById (Weapon::Explosive);
        }
        else {
          SelectBestWeapon ();
          CompleteTask ();

          return;
        }
      }
      else if (!(old_buttons_ & IN_ATTACK)) {
        pev->button |= IN_ATTACK;
      }
    }
  }
  if (!cv_move_during_throw) {
    pev->button |= camp_buttons_;
  }
}

void Bot::TaskThrowFlashbang () {
  ystl::Vector dest = throw_;

  if (!has_flag (states_, Sense::SeeingEnemy)) {
    if (!cv_move_during_throw) {
      strafe_speed_ = 0.0f;
      move_speed_ = 0.0f;
      move_to_goal_ = false;
    }
  }
  else if (!has_flag (states_, Sense::SuspectEnemy) && !game.IsNullEntity (enemy_)) {
    dest = enemy_->v.origin + enemy_->v.velocity.get2d ();
  }

  is_using_grenade_ = true;
  check_terrain_ = false;

  IgnoreCollision ();

  if (pev->origin.distance_sq (dest) < ystl::sqrf (kGrenadeDamageRadius)) {
    grenade_check_timer_.start (kGrenadeCheckTime * 2.0f); // heck, i don't wanna blow up myself

    SelectBestWeapon ();
    CompleteTask ();

    return;
  }
  grenade_ = CalcThrow (GetEyesPos (), dest);

  if (grenade_.length_sq () < 100.0f) {
    grenade_ = CalcToss (pev->origin, dest);
  }

  if (grenade_.length_sq () <= 100.0f) {
    grenade_check_timer_.start (kGrenadeCheckTime * 2.0f);

    SelectBestWeapon ();
    CompleteTask ();
  }
  else {
    aim_flags_ |= AimFlags::Grenade;

    auto grenade = SetCorrectGrenadeVelocity (kFlashbangModelName);

    if (game.IsNullEntity (grenade)) {
      if (current_weapon_ != Weapon::Flashbang) {
        if (has_flag (pev->weapons, ystl::bit (Weapon::Flashbang))) {
          SelectWeaponById (Weapon::Flashbang);
        }
        else {
          SelectBestWeapon ();
          CompleteTask ();

          return;
        }
      }
      else if (!(old_buttons_ & IN_ATTACK)) {
        pev->button |= IN_ATTACK;
      }
    }
  }
  if (!cv_move_during_throw) {
    pev->button |= camp_buttons_;
  }
}

void Bot::TaskThrowSmoke () {
  if (!has_flag (states_, Sense::SeeingEnemy) && !cv_move_during_throw) {
    strafe_speed_ = 0.0f;
    move_speed_ = 0.0f;
    move_to_goal_ = false;
  }

  check_terrain_ = false;
  is_using_grenade_ = true;

  IgnoreCollision ();

  // use the tactical smoke position already stored in m_throw
  ystl::Vector dest = throw_;

  grenade_ = CalcThrow (GetEyesPos (), dest);

  if (grenade_.length_sq () < 100.0f) {
    grenade_ = CalcToss (pev->origin, dest);
  }

  if (grenade_.length_sq () <= 100.0f) {
    grenade_check_timer_.start (kGrenadeCheckTime * 2.0f);

    SelectBestWeapon ();
    CompleteTask ();

    return;
  }

  if (Task ()->time < game.Time ()) {
    CompleteTask ();
    return;
  }
  aim_flags_ |= AimFlags::Grenade;

  auto grenade = SetCorrectGrenadeVelocity (kSmokeModelName);

  if (game.IsNullEntity (grenade)) {
    if (current_weapon_ != Weapon::Smoke) {
      if (has_flag (pev->weapons, ystl::bit (Weapon::Smoke))) {
        SelectWeaponById (Weapon::Smoke);
      }
      else {
        SelectBestWeapon ();
        CompleteTask ();

        return;
      }
    }
    else if (!(old_buttons_ & IN_ATTACK)) {
      pev->button |= IN_ATTACK;
    }
  }
  if (!cv_move_during_throw) {
    pev->button |= camp_buttons_;
  }
}

void Bot::TaskDoubleJump () {
  if (!game.IsAliveEntity (double_jump_entity_) || has_flag (aim_flags_, AimFlags::Enemy) ||
      (travel_start_index_ != kInvalidNodeIndex &&
        Task ()->time + (graph.CalculateTravelTime (pev->maxspeed, graph[travel_start_index_].origin, double_jump_origin_) + 11.0f) <
          game.Time ())) {
    ResetDoubleJump ();
    return;
  }
  aim_flags_ |= AimFlags::Nav;

  if (jump_ready_) {
    move_to_goal_ = false;
    check_terrain_ = false;

    nav_timer_.start ();
    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;

    bool in_jump = (double_jump_entity_->v.button & IN_JUMP) || (double_jump_entity_->v.oldbuttons & IN_JUMP);

    if (duck_for_jump_ < game.Time ()) {
      pev->button |= IN_DUCK;
    }
    else if (in_jump && !(old_buttons_ & IN_JUMP)) {
      pev->button |= IN_JUMP;
    }

    const ystl::Vector src = pev->origin + ystl::Vector (0.0f, 0.0f, 45.0f);
    const ystl::Vector dest = src + ystl::Vector (0.0f, pev->angles.y, 0.0f).upward () * 256.0f;

    Trace::Result tr {};
    trace.Line (src, dest, TraceIgnore::None, Ent (), &tr);

    if (tr.fraction < 1.0f && tr.hit == double_jump_entity_ && in_jump) {
      duck_for_jump_ = game.Time () + rg (3.0f, 5.0f);
      Task ()->time = game.Time ();
    }
    return;
  }

  if (current_node_index_ == prev_goal_index_) {
    path_origin_ = double_jump_origin_;
    dest_origin_ = double_jump_origin_;
  }

  if (UpdateNavigation ()) {
    Task ()->data = kInvalidNodeIndex;
  }

  // didn't choose goal node yet?
  if (!HasActiveGoal ()) {
    int dest_index = graph.GetNearest (double_jump_origin_);

    if (graph.Exists (dest_index)) {
      prev_goal_index_ = dest_index;
      travel_start_index_ = current_node_index_;

      Task ()->data = dest_index;

      // always take the shortest path
      FindPath (current_node_index_, dest_index, FindPathType::Fast);

      if (current_node_index_ == dest_index) {
        jump_ready_ = true;
      }
    }
    else {
      ResetDoubleJump ();
    }
  }
}

void Bot::TaskEscapeFromBomb () {
  aim_flags_ |= AimFlags::Nav;

  // once committed to escaping, stay committed until the round ends
  escaped_from_bomb_ = true;

  if (!game_state.IsBombPlanted ()) {
    CompleteTask ();
    return;
  }

  if (IsShieldDrawn ()) {
    pev->button |= IN_ATTACK2;
  }

  if (!UsesKnife () && game.IsNullEntity (enemy_) && !game.IsAliveEntity (last_enemy_)) {
    SelectWeaponById (Weapon::Knife);
  }

  // reached destination?
  if (UpdateNavigation ()) {
    CompleteTask (); // we're done

    // press duck button if we still have some enemies
    if (num_enemies_left_ > 0) {
      camp_buttons_ = IN_DUCK;
    }

    // we're reached destination point so just sit down and camp
    StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 10.0f, true);
  }

  // didn't choose goal node yet?
  else if (!HasActiveGoal ()) {
    int best_index = kInvalidNodeIndex;

    const float safe_radius = rg (1513.0f, 2048.0f);
    float nearest_distance_sq = kInfiniteDistance;

    for (const auto &path : graph) {
      if (path.origin.distance_sq (game_state.GetBombOrigin ()) < ystl::sqrf (safe_radius) || IsOccupiedNode (path.number)) {
        continue;
      }
      const float distance_sq = pev->origin.distance_sq (path.origin);

      if (nearest_distance_sq > distance_sq) {
        nearest_distance_sq = distance_sq;
        best_index = path.number;
      }
    }

    if (best_index < 0) {
      best_index = FindFarestNode (pev->origin, safe_radius);
    }

    // still no luck?
    if (best_index < 0) {
      CompleteTask (); // we're done

      // we have no destination point, so just sit down and camp
      StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + 10.0f, true);
      return;
    }
    prev_goal_index_ = best_index;
    Task ()->data = best_index;

    FindPath (current_node_index_, best_index, FindPathType::Fast);
  }
}

void Bot::TaskShootBreakable () {
  // yield only to a visible enemy, not a suspect thru-wall one
  const bool has_visible_enemy = !game.IsNullEntity (enemy_) && has_flag (states_, Sense::SeeingEnemy);

  // breakable destroyed or gave up on it?
  if (has_visible_enemy || !game.IsBreakableEntity (breakable_entity_) || IsIgnoredBreakable (breakable_entity_)) {
    CompleteTask ();
    return;
  }

  // initialize shoot timer on first entry
  if (!breakable_shoot_timer_.started ()) {
    breakable_shoot_timer_.start (3.5f);
  }
  else if (breakable_shoot_timer_.elapsed ()) {
    ignored_breakable_.push (breakable_entity_);

    breakable_entity_ = nullptr;
    breakable_origin_.clear ();
    breakable_shoot_timer_.invalidate ();

    CompleteTask ();
    return;
  }

  {
    Trace::Result tr {};
    trace.Line (pev->origin, breakable_origin_, TraceIgnore::Monsters, Ent (), &tr);

    if (tr.hit != breakable_entity_ && !ystl::fequal (tr.fraction, 1.0f)) {
      if (game.IsBreakableEntity (tr.hit)) {
        ignored_breakable_.push (tr.hit);
      }

      breakable_entity_ = nullptr;
      breakable_origin_.clear ();
      breakable_shoot_timer_.invalidate ();

      CompleteTask ();
      return;
    }
  }
  aim_flags_ |= AimFlags::Override;
  pev->button |= camp_buttons_;

  check_terrain_ = false;
  move_to_goal_ = false;

  nav_timer_.start ();
  look_at_safe_ = breakable_origin_;

  // is bot facing the breakable?
  if (util.ViewDot (Ent (), look_at_safe_) >= 0.90f) {
    move_speed_ = 0.0f;
    strafe_speed_ = 0.0f;

    wants_to_fire_ = true;
    shoot_time_ = game.Time ();

    // enforce shooting
    if (!UsesKnife () && !reload_data_.is_reloading && !(pev->button & IN_RELOAD) && GetAmmoInClip () > 0) {
      wants_to_fire_ = true;
    }

    // out of ammo, give up on this breakable
    if (!HasAnyAmmoInClip ()) {
      if (UsesKnife ()) {
        const float dist_to_obstacle = pev->origin.distance_sq (look_at_safe_);

        if (dist_to_obstacle > ystl::sqrf (32.0f)) {
          breakable_shoot_timer_.invalidate ();
          CompleteTask ();
        }
      }
      else {
        breakable_shoot_timer_.invalidate ();
        CompleteTask ();
      }
    }
  }
  else {
    check_terrain_ = true;
    move_to_goal_ = true;
  }
}

void Bot::TaskPickupItem () {
  if (game.IsNullEntity (pickup_item_)) {
    pickup_item_ = nullptr;

    // after pickup, advance path walk to the closest upcoming node
    if (!path_walk_.Empty ()) {
      float best_dist_sq = pev->origin.distance_sq (graph[path_walk_.First ()].origin);

      while (path_walk_.HasNext ()) {
        const float next_dist_sq = pev->origin.distance_sq (graph[path_walk_.Next ()].origin);

        if (next_dist_sq > best_dist_sq) {
          break;
        }
        best_dist_sq = next_dist_sq;
        path_walk_.Shift ();
      }
      ChangeNodeIndex (path_walk_.First ());
    }
    CompleteTask ();

    return;
  }
  const ystl::Vector dest = game.GetEntityOrigin (pickup_item_);

  dest_origin_ = dest;
  entity_ = dest;

  // find the distance to the item
  const float item_distance_sq = dest.distance_sq (pev->origin);

  switch (pickup_type_) {
  case Pickup::DroppedC4:
  case Pickup::None:
  case Pickup::Items:
    break;

  case Pickup::Weapon:
  case Pickup::AmmoAndKits:
    aim_flags_ |= AimFlags::Nav;

    // near to weapon?
    if (item_distance_sq < ystl::sqrf (50.0f)) {
      int index = 0;
      auto &tab = conf.GetWeapons ();

      for (index = 0; index < kPrimaryWeaponMinIndex; ++index) {
        if (pickup_item_->v.model.str (9) == tab[index].model) {
          break;
        }
      }

      if (index < kPrimaryWeaponMinIndex) {
        // secondary weapon. i.e., pistol
        int weapon_index = 0;

        for (index = 0; index < kPrimaryWeaponMinIndex; ++index) {
          if (has_flag (pev->weapons, ystl::bit (tab[index].id))) {
            weapon_index = index;
          }
        }

        if (weapon_index > 0) {
          const auto old_id = tab[weapon_index].id;

          // remember dry swaps so the dropped gun is never re-picked
          if (ammo_in_clip_[old_id] <= 0 && GetAmmo (old_id) <= 0) {
            dropped_dry_weapons_mask_ |= static_cast<uint32_t> (ystl::bit (old_id));
          }
          SelectWeaponByIndex (weapon_index);
          DropCurrentWeapon ();

          if (HasShield ()) {
            DropCurrentWeapon (); // discard both shield and pistol
          }
        }
        EnteredBuyZone (BuyState::PrimaryWeapon);
      }
      else {
        // primary weapon
        const int weapon_index = GetBestOwnedWeaponIndex ();
        const bool nice_weapon = RateGroundWeapon (pickup_item_);

        if ((weapon_index >= kPrimaryWeaponMinIndex || tab[weapon_index].id == Weapon::Shield || HasShield ()) && nice_weapon) {
          const auto old_id = tab[weapon_index].id;

          // remember dry swaps so the dropped gun is never re-picked
          if (old_id != Weapon::Shield && ammo_in_clip_[old_id] <= 0 && GetAmmo (old_id) <= 0) {
            dropped_dry_weapons_mask_ |= static_cast<uint32_t> (ystl::bit (old_id));
          }
          SelectWeaponByIndex (weapon_index);
          DropCurrentWeapon ();
        }

        if (!weapon_index || !nice_weapon) {
          ignored_items_.push (pickup_item_);

          pickup_item_ = nullptr;
          pickup_type_ = Pickup::None;

          break;
        }
        EnteredBuyZone (BuyState::PrimaryWeapon);
      }
      CheckSilencer (); // check the silencer
    }
    break;

  case Pickup::Shield:
    aim_flags_ |= AimFlags::Nav;

    if (HasShield ()) {
      pickup_item_ = nullptr;
      break;
    }

    // near to shield?
    else if (item_distance_sq < ystl::sqrf (50.0f)) {
      // get current best weapon to check if it's a primary in need to be dropped
      int weapon_index = GetBestOwnedWeaponIndex ();

      if (weapon_index > 6) {
        SelectWeaponByIndex (weapon_index);
        DropCurrentWeapon ();
      }
    }
    break;

  case Pickup::PlantedC4:
    aim_flags_ |= AimFlags::Entity;

    if (team_ == Team::CT && item_distance_sq < ystl::sqrf (80.0f)) {
      PushRadioChat (RadioChat::DefusingBomb);

      // notify team of defusing
      if (num_enemies_left_ > 0 && num_friends_left_ < 3 && rg.chance (90)) {
        PushRadioChat (RadioChat::NeedBackup);
      }
      move_to_goal_ = false;
      check_terrain_ = false;

      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;

      StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
    }
    break;

  case Pickup::Hostage:
    aim_flags_ |= AimFlags::Entity;

    if (!game.IsAliveEntity (pickup_item_)) {
      // don't pickup dead hostages
      pickup_item_ = nullptr;
      CompleteTask ();

      break;
    }

    if (item_distance_sq < ystl::sqrf (50.0f)) {
      const float angle_to_entity = IsInFov (dest - GetEyesPos ());

      // bot faces hostage?
      if (angle_to_entity <= 10.0f) {
        // use game dll function to make sure the hostage is correctly 'used'
        MDLL_Use (pickup_item_, Ent ());

        if (rg.chance (80)) {
          PushRadioChat (RadioChat::UsingHostages);
        }
        hostages_.push (pickup_item_);
        pickup_item_ = nullptr;

        CompleteTask ();

        float nearest_distance_sq = kInfiniteDistance;
        int nearest_hostage_node_index = kInvalidNodeIndex;

        // find the nearest 'unused' hostage within the area
        game.SearchEntities (pev->origin, 1024.0f, [&] (edict_t *ent) {
          if (!game.IsHostageEntity (ent)) {
            return EntitySearchResult::Continue;
          }

          // check if hostage is dead
          if (game.IsNullEntity (ent) || ent->v.health <= 0) {
            return EntitySearchResult::Continue;
          }

          // check if hostage is with a bot
          for (const auto &other : bots) {
            if (other.is_alive_) {
              for (const auto &hostage : other.hostages_) {
                if (hostage == ent) {
                  return EntitySearchResult::Continue;
                }
              }
            }
          }

          // check if hostage is with a human teammate (hack)
          for (const auto &client : clients) {
            if (client.IsUsedAndAlive () && client.IsHuman () && client.team == team_ &&
                client.IsInRadius (ent->v.origin, ystl::sqrf (240.0f))) {

              return EntitySearchResult::Continue;
            }
          }
          const int hostage_node_index = graph.GetNearest (ent->v.origin);

          if (graph.Exists (hostage_node_index)) {
            const float distance_sq = graph[hostage_node_index].origin.distance_sq (pev->origin);

            if (distance_sq < nearest_distance_sq) {
              nearest_distance_sq = distance_sq;
              nearest_hostage_node_index = hostage_node_index;
            }
          }

          return EntitySearchResult::Continue;
        });

        if (nearest_hostage_node_index != kInvalidNodeIndex) {
          ClearTask (TaskId::MoveTo); // remove any move tasks
          StartTask (TaskId::MoveTo, TaskPri::kMoveTo, nearest_hostage_node_index, 0.0f, true);
        }
      }
      IgnoreCollision (); // also don't consider being stuck
    }
    break;

  case Pickup::DefusalKit:
    aim_flags_ |= AimFlags::Nav;

    if (has_defuser_) {
      pickup_item_ = nullptr;
      pickup_type_ = Pickup::None;
    }
    break;

  case Pickup::Button:
    aim_flags_ |= AimFlags::Entity;

    if (game.IsNullEntity (pickup_item_)) {
      CompleteTask ();
      pickup_type_ = Pickup::None;

      break;
    }
    float distance_to_button_sq = ystl::sqrf (90.0f);

    // reduce on lifts
    if (!game.IsNullEntity (lift_entity_)) {
      distance_to_button_sq = ystl::sqrf (24.0f);
    }

    // near to the button?
    if (item_distance_sq < distance_to_button_sq) {
      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;
      move_to_goal_ = false;
      check_terrain_ = false;

      // find angles from bot origin to entity
      const float angle_to_entity = IsInFov (dest - GetEyesPos ());

      // facing it directly?
      if (angle_to_entity <= 10.0f) {
        MDLL_Use (pickup_item_, Ent ());

        pickup_item_ = nullptr;
        pickup_type_ = Pickup::None;
        button_push_timer_.start (3.0f);
        door_hit_timer_.start (1.5f); // let the door open before moving

        CompleteTask ();
      }
    }
    break;
  }

#if 0
   // navigate to the item if we're not close enough
   if (GetTaskId () == TaskId::PickupItem && !game.IsNullEntity (pickup_item_)) {
      int dest_index = graph.GetNearest (dest);

      if (graph.Exists (dest_index) && dest_index != current_node_index_) {
         if (UpdateNavigation ()) {
            Task ()->data = kInvalidNodeIndex;
         }

         if (!HasActiveGoal () || Task ()->data != dest_index) {
            if (graph.Exists (current_node_index_)) {
               prev_goal_index_ = dest_index;
               Task ()->data = dest_index;

               FindPath (current_node_index_, dest_index, path_type_);
            }
         }
      }
      else {
         dest_origin_ = dest;
      }
   }
#endif
}

const ystl::Tuple<Task::Function, bool, ystl::StringRef> &Bot::TaskInfo (TaskId id) {
  // indexed by Task; keep in sync with the enum above
  static constexpr ystl::Tuple<Task::Function, bool, ystl::StringRef> kTasks[] {
    { &Bot::TaskNormal,         true,  "Normal"         },
    { &Bot::TaskPause,          false, "Pause"          },
    { &Bot::TaskMoveTo,         true,  "MoveTo"         },
    { &Bot::TaskFollowUser,     true,  "FollowUser"     },
    { &Bot::TaskPickupItem,     true,  "PickupItem"     },
    { &Bot::TaskCamp,           true,  "Camp"           },
    { &Bot::TaskPlantBomb,      false, "PlantBomb"      },
    { &Bot::TaskDefuseBomb,     false, "DefuseBomb"     },
    { &Bot::TaskAttack,         false, "Attack"         },
    { &Bot::TaskHunt,           false, "Hunt"           },
    { &Bot::TaskSeekCover,      true,  "SeekCover"      },
    { &Bot::TaskThrowExplosive, false, "ThrowExplosive" },
    { &Bot::TaskThrowFlashbang, false, "ThrowFlashbang" },
    { &Bot::TaskThrowSmoke,     false, "ThrowSmoke"     },
    { &Bot::TaskDoubleJump,     false, "DoubleJump"     },
    { &Bot::TaskEscapeFromBomb, false, "EscapeFromBomb" },
    { &Bot::TaskShootBreakable, false, "ShootBreakable" },
    { &Bot::TaskHide,           false, "Hide"           },
    { &Bot::TaskBlind,          false, "Blind"          },
    { &Bot::TaskSpraypaint,     false, "Spraypaint"     }
  };
  static_assert (sizeof (kTasks) / sizeof (kTasks[0]) == ystl::to_underlying (TaskId::Num));

  return kTasks[ystl::to_underlying (id)];
}

Task::Function Bot::TaskHandler (TaskId id) {
  return ystl::get<0> (TaskInfo (id));
}

bool Bot::TaskResumable (TaskId id) {
  return ystl::get<1> (TaskInfo (id));
}

ystl::StringRef Bot::TaskName (TaskId id) {
  return ystl::get<2> (TaskInfo (id));
}

} // namespace bot
