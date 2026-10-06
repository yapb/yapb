//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void Bot::PushMsgQueue (Msg message) {
  // this function put a message into the bot message queue

  if (message == Msg::Say) {
    // notify other bots of the spoken text otherwise, bots won't respond to other bots (network messages aren't sent from bots)
    const int entity_index = Index ();

    for (auto &other : bots) {
      if (other.pev != pev) {
        if (is_alive_ == other.is_alive_) {
          other.say_text_buffer_.entity_index = entity_index;
          other.say_text_buffer_.time_next_chat =
            game.Time () + other.say_text_buffer_.chat_delay; // mute only listeners, so alive chatter doesn't block dead replies
        }
      }
    }
  }
  msg_queue_.emplace_last (message);
}

void Bot::AvoidGrenades () {
  // checks if bot 'sees' a grenade, and avoid it

  // the flash aim flag follows the hold timer, drops in combat or when blind
  if (!avoid_flash_timer_.elapsed () && blind_timer_.elapsed () && !has_flag (states_, Sense::SeeingEnemy)) {
    aim_flags_ |= AimFlags::Flash;
  }
  else {
    aim_flags_ &= ~AimFlags::Flash;
  }

  // check if old pointers to grenade is invalid
  if (game.IsNullEntity (avoid_grenade_)) {
    avoid_grenade_ = nullptr;
    need_avoid_grenade_ = 0;
  }
  else if ((avoid_grenade_->v.flags & FL_ONGROUND) || (avoid_grenade_->v.effects & EF_NODRAW)) {
    avoid_grenade_ = nullptr;
    need_avoid_grenade_ = 0;
  }

  if (!game_state.HasActiveGrenades ()) {
    return;
  }
  const auto &active_grenades = game_state.GetActiveGrenades ();

  // find all grenades on the map
  for (const auto &grenade : active_grenades) {
    const auto pent = grenade.ent;

    if (pent->v.effects & EF_NODRAW) {
      continue;
    }

    // check if visible to the bot, floor-hold only fires on a seen flash
    if (grenade.kind == GrenadeKind::Flash) {
      if (!SeesEntity (pent->v.origin)) {
        continue;
      }
    }
    else if (IsInFov (pent->v.origin - GetEyesPos ()) > pev->fov * 0.5f && !SeesEntity (pent->v.origin)) {
      continue;
    }

    // never break the aim while fighting, the enemy is the bigger threat
    if (grenade.kind == GrenadeKind::Flash && blind_timer_.elapsed () && !has_flag (states_, Sense::SeeingEnemy)) {
      const float time_left = pent->v.dmgtime - game.Time ();

      if (time_left > 0.0f) {
        const float window = cv_whose_your_daddy ? 1.0f : 0.2f + 0.45f * (static_cast<float> (Skill ()) / 100.0f);

        if (time_left <= window && pent->v.origin.distance_sq (pev->origin) < ystl::sqrf (1500.0f)) {
          const bool fresh = pent != avoid_flash_ent_ || !ystl::fequal (pent->v.dmgtime, avoid_flash_dmgtime_);

          // even the best never react every time, the cap keeps a human miss rate
          if (fresh && (cv_whose_your_daddy || rg.chance (40 + Skill () / 2))) {
            avoid_flash_timer_.start (time_left + 0.3f);

            // face directly away from the grenade, level pitch, the aim spring turns smoothly
            const auto away = (GetEyesPos () - game.GetEntityOrigin (pent)).normalize2d ();

            avoid_flash_look_ = GetEyesPos () + away * 500.0f;
            aim_flags_ |= AimFlags::Flash;
          }
          avoid_flash_ent_ = pent;
          avoid_flash_dmgtime_ = pent->v.dmgtime;
        }
      }
    }
    else if (game.IsNullEntity (avoid_grenade_) && grenade.kind == GrenadeKind::Explosive) {
      if (game.GetPlayerTeam (pent->v.owner) == team_ || pent->v.owner == Ent ()) {
        continue;
      }

      if (!(pent->v.flags & FL_ONGROUND)) {
        const float distance_sq = pent->v.origin.distance_sq (pev->origin);
        const float distance_moved_sq = pev->origin.distance_sq (pent->v.origin + pent->v.velocity * frame_interval_);

        if (distance_moved_sq < distance_sq && distance_sq < ystl::sqrf (500.0f)) {
          const ystl::Vector dir_to_point = (pev->origin - pent->v.origin).normalize2d ();
          const ystl::Vector right_side = pev->v_angle.right ().normalize2d ();

          // strafe along the escape direction, not against it
          if ((dir_to_point | right_side) > 0.0f) {
            need_avoid_grenade_ = 1;
          }
          else {
            need_avoid_grenade_ = -1;
          }
          avoid_grenade_ = pent;
        }
      }
    }
    else if (cv_smoke_grenade_checks.As<int> () == 1 && (pent->v.flags & FL_ONGROUND) && grenade.kind == GrenadeKind::Smoke) {
      if (SeesEntity (pent->v.origin) && IsInFov (pent->v.origin - GetEyesPos ()) < pev->fov / 3.0f) {
        const ystl::Vector ent_origin = game.GetEntityOrigin (pent);
        const ystl::Vector between_us = pev->v_angle.forward ();
        const ystl::Vector between_nade = (ent_origin - pev->origin).normalize ();
        const ystl::Vector between_result = ((between_nade.get2d () * 150.0f + ent_origin) - pev->origin).normalize ();

        // visibility already proven by the gate above, no second trace
        if ((between_nade | between_us) > (between_nade | between_result)) {
          const float distance = ent_origin.distance (pev->origin);

          // shrink bot's viewing distance to smoke grenade's distance
          if (view_distance_ > distance) {
            view_distance_ = distance;

            if (rg.chance (45)) {
              PushRadioChat (RadioChat::BehindSmoke);
            }
          }
        }
      }
    }
  }
}

void Bot::CheckBreakable (edict_t *touch) {
  if (!game.HasBreakables ()) {
    return;
  }
  const bool has_enemy = !game.IsNullEntity (enemy_);

  // do n ot track for breakables if has some enemies
  if (has_enemy) {
    return;
  }

  if (game.IsNullEntity (touch)) {
    auto breakable = LookupBreakable ();

    breakable_entity_ = breakable;
    if (!game.IsNullEntity (breakable)) {
      breakable_origin_ = game.GetEntityOrigin (breakable);
    }
    else {
      breakable_origin_.clear ();
    }
  }
  else {
    if (breakable_entity_ != touch) {
      breakable_entity_ = touch;
      breakable_origin_ = game.GetEntityOrigin (touch);
    }
  }

  // re-check from previous steps
  if (game.IsNullEntity (breakable_entity_) || breakable_origin_.empty ()) {
    return;
  }
  camp_buttons_ = pev->button & IN_DUCK;
  StartTask (TaskId::ShootBreakable, TaskPri::kShootBreakable, kInvalidNodeIndex, 0.0f, false);
}

void Bot::CheckBreakablesAround () {
  if (!buying_finished_ || !cv_destroy_breakables_around || UsesKnife () || UsesSniper () || IsOnLadder () || rg.chance (25) ||
      !game.HasBreakables () || see_enemy_timer_.less_than (4.0f) || !game.IsNullEntity (enemy_) ||
      has_flag (aim_flags_, AimFlags::PredictPath | AimFlags::Danger) || !HasPrimaryWeapon ()) {
    return;
  }
  const auto radius = cv_object_destroy_radius.As<float> ();

  // check if we're have some breakables in 400 units range
  for (const auto &item : game_state.GetInterestingEntities ()) {
    if (item.kind != EntityKind::Breakable) {
      continue;
    }
    const auto breakable = item.ent;
    bool ignore_breakable = false;

    // check if it's blacklisted
    for (const auto &ignored : ignored_breakable_) {
      if (ignored == breakable) {
        ignore_breakable = true;
        break;
      }
    }

    // keep searching
    if (ignore_breakable) {
      continue;
    }

    if (!game.IsBreakableEntity (breakable)) {
      continue;
    }

    const ystl::Vector origin = game.GetEntityOrigin (breakable);
    const auto distance_to_obstacle_sq = origin.distance_sq2d (pev->origin);

    // too far, skip it
    if (distance_to_obstacle_sq > ystl::sqrf (radius)) {
      continue;
    }

    // too close, skip it
    if (distance_to_obstacle_sq < ystl::sqrf (100.0f)) {
      continue;
    }

    // maybe time to give up?
    if (last_breakable_ == breakable && breakable_timer_.remaining_time () < -1.5f) {
      ignored_breakable_.push (breakable);
      breakable_origin_.clear ();

      last_breakable_ = nullptr;
      breakable_entity_ = nullptr;

      continue;
    }

    if (IsInFov (origin - GetEyesPos ()) < pev->fov && SeesEntity (origin)) {
      if (breakable_entity_ != breakable) {
        breakable_timer_.start (0.0f);
        last_breakable_ = breakable;
      }

      breakable_origin_ = origin;
      breakable_entity_ = breakable;
      camp_buttons_ = pev->button & IN_DUCK;

      StartTask (TaskId::ShootBreakable, TaskPri::kShootBreakable, kInvalidNodeIndex, 0.0f, false);
      break;
    }
  }
}

edict_t *Bot::LookupBreakable () {
  // this function checks if bot is blocked by a shoot able breakable in his moving direction

  // we're got something already, but re-validate team and ignore list
  if (game.IsBreakableEntity (breakable_entity_)) {
    if (breakable_entity_->v.team > 0 && breakable_entity_->v.team != ystl::to_underlying (game.GetPlayerTeamGame (Ent ()))) {
      breakable_entity_ = nullptr;
      breakable_origin_.clear ();
    }
    else {
      for (const auto &br : ignored_breakable_) {
        if (br == breakable_entity_) {
          breakable_entity_ = nullptr;
          breakable_origin_.clear ();
          break;
        }
      }
    }

    if (!game.IsNullEntity (breakable_entity_)) {
      return breakable_entity_;
    }
  }
  const float detect_breakable_distance = (UsesKnife () || IsOnLadder ()) ? 32.0f : 164.0f;

  auto do_lookup = [&] (const ystl::Vector &start, const ystl::Vector &end, const float dist) -> edict_t * {
    Trace::Result tr {};
    trace.Line (start, start + (end - start).normalize () * dist, TraceIgnore::None, Ent (), &tr);

    if (!ystl::fequal (tr.fraction, 1.0f)) {
      auto hit = tr.hit;

      // check if this isn't a triggered (bomb) breakable and if it takes damage. if true, shoot the crap!
      if (game.IsBreakableEntity (hit)) {
        breakable_origin_ = game.GetEntityOrigin (hit);
        breakable_entity_ = hit;

        return hit;
      }
    }
    return nullptr;
  };

  auto is_good_for_us = [&] (edict_t *ent) -> bool {
    if (game.IsNullEntity (ent)) {
      return false;
    }

    // check breakable team, needed for some plugins
    if (ent->v.team > 0 && ent->v.team != ystl::to_underlying (game.GetPlayerTeamGame (this->Ent ()))) {
      return false;
    }

    for (const auto &br : ignored_breakable_) {
      if (br == ent) {
        return false;
      }
    }
    return true;
  };
  auto hit = do_lookup (pev->origin, dest_origin_, detect_breakable_distance);

  if (is_good_for_us (hit)) {
    return hit;
  }
  hit = do_lookup (GetEyesPos (), dest_origin_, detect_breakable_distance);

  if (is_good_for_us (hit)) {
    return hit;
  }
  breakable_entity_ = nullptr;
  breakable_origin_.clear ();

  return nullptr;
}

void Bot::SetIdealReactionTimers (bool actual) {
  if (cv_whose_your_daddy) {
    ideal_reaction_time_ = 0.05f;
    actual_reaction_time_ = 0.095f;

    return; // zero out reaction times for extreme mode
  }

  if (actual) {
    ideal_reaction_time_ = difficulty_data_->reaction[0];
    actual_reaction_time_ = difficulty_data_->reaction[0];

    return;
  }
  ideal_reaction_time_ = rg (difficulty_data_->reaction[0], difficulty_data_->reaction[1]);
}

bool Bot::IsIgnoredItem (edict_t *ent) {
  for (const auto &ignored : ignored_items_) {
    if (ignored == ent) {
      return true;
    }
  }
  return false;
}

ystl::Vector Bot::GetCampDirection (const ystl::Vector &dest) {
  // finds an alternative camping direction when the view to the last enemy position is blocked

  constexpr auto kMaxObstructionDistanceSq = ystl::sqrf (1024.0f);

  const ystl::Vector eye_pos = GetEyesPos ();

  // check if line of sight to destination is blocked
  Trace::Result tr {};
  trace.Line (eye_pos, dest, TraceIgnore::Monsters, Ent (), &tr);

  // if view is obstructed, find an alternative direction
  if (tr.fraction < 1.0f) {
    // ignore obstructions that are too far away
    if (tr.end_pos.distance_sq (eye_pos) > kMaxObstructionDistanceSq) {
      return ystl::Vector {};
    }

    const int enemy_index = graph.GetNearest (dest);
    const int current_index = graph.GetNearest (pev->origin);

    // validate node indices
    if (!graph.Exists (current_index) || !graph.Exists (enemy_index)) {
      return ystl::Vector {};
    }

    // find the connected node with shortest path to enemy
    struct NodeDistance {
      int index { kInvalidNodeIndex };
      float distance { kInfiniteDistance };
    };
    NodeDistance best {};

    for (const auto &link : graph[current_index].links) {
      if (!graph.Exists (link.index)) {
        continue;
      }

      const float distance = planner.Dist (link.index, enemy_index);

      if (distance < best.distance) {
        best = { link.index, distance };
      }
    }

    if (graph.Exists (best.index)) {
      return graph[best.index].origin;
    }
  }

  // fallback: use practice system's danger index for this position
  const int danger_index = practice.GetIndex (team_, current_node_index_, current_node_index_);

  if (graph.Exists (danger_index)) {
    return graph[danger_index].origin;
  }
  return {};
}

void Bot::CheckMsgQueue () {
  // this function checks and executes pending messages

  // no new message?
  if (msg_queue_.empty ()) {
    return;
  }

  // get message from deque
  const auto state = msg_queue_.pop_front ();

  // nothing to do?
  if (state == Msg::None || (state == Msg::Radio && (is_creature_ || game.Is (GameFlags::FreeForAll)))) {
    return;
  }
  float delay_response_time = 0.0f;

  switch (state) {
  case Msg::Buy: // general buy message

    // buy weapon
    if (!next_buy_timer_.elapsed ()) {
      // keep sending message
      PushMsgQueue (Msg::Buy);
      return;
    }

    if (!in_buy_zone_ || game.Is (GameFlags::CSDM) || is_creature_) {
      buy_pending_ = true;
      buying_finished_ = true;

      break;
    }

    buy_pending_ = false;
    next_buy_timer_.start (rg (0.5f, 1.3f));

    // if freezetime is very low do not delay the buy process
    if (mp_freezetime.As<float> () <= 1.0f) {
      next_buy_timer_.start (0.0f);
      ignore_buy_delay_ = true;
    }

    // if bot buying is off then no need to buy
    if (!cv_botbuy) {
      buy_state_ = BuyState::Done;
    }

    // if fun-mode no need to buy
    if (cv_jasonmode) {
      buy_state_ = BuyState::Done;
      SelectWeaponById (Weapon::Knife);
    }

    // prevent vip from buying
    if (is_vip_) {
      buy_state_ = BuyState::Done;
      path_type_ = FindPathType::Fast;
    }

    // prevent terrorists from buying on es maps
    if (game.MapIs (MapFlags::Escape) && team_ == Team::Terrorist && !in_buy_zone_) {
      buy_state_ = BuyState::Done;
    }

    // prevent teams from buying on fun maps
    if (game.MapIs (MapFlags::KnifeArena)) {
      buy_state_ = BuyState::Done;
      cv_jasonmode.Set (1);
    }

    if (buy_state_ >= BuyState::Done) {
      buying_finished_ = true;
      return;
    }

    PushMsgQueue (Msg::None);
    BuyWeapons ();

    break;

  case Msg::Radio:
    delay_response_time = rg (1.0f, 3.0f);

    // if last bot radio command (global) happened some a little time ago, delay response
    if (bots.GetLastRadioTimestamp (team_) + delay_response_time < game.Time ()) {

      // if same message like previous just do a yes/no
      if (radio_select_ != RadioChat::RogerThat && radio_select_ != RadioChat::Negative) {
        if (radio_select_ == bots.GetLastRadio (team_) && bots.GetLastRadioTimestamp (team_) + delay_response_time * 0.5f > game.Time ()) {
          radio_select_ = RadioChat::Invalid;
        }
        else {
          if (radio_select_ != RadioChat::ReportingIn) {
            bots.SetLastRadio (team_, radio_select_);
          }
          else {
            bots.SetLastRadio (team_, RadioChat::Invalid);
          }

          for (auto &bot : bots) {
            if (pev != bot.pev && bot.team_ == team_) {
              bot.radio_order_ = radio_select_;
              bot.radio_entity_ = Ent ();
            }
          }
        }
      }

      if (radio_select_ != RadioChat::Invalid) {
        if ((radio_select_ != RadioChat::ReportingIn && force_radio_) || cv_radio_mode.As<int> () != 2 || !conf.HasChatterBank (radio_select_) ||
            !game.Is (GameFlags::HasBotVoice)) {

          auto radio_slot = radio_select_;

          if (radio_select_ < RadioChat::GoGoGo) {
            IssueCommand ("radio1");
          }
          else if (radio_select_ < RadioChat::RogerThat) {
            radio_slot -= RadioChat::GoGoGo - 1;
            IssueCommand ("radio2");
          }
          else {
            radio_slot -= RadioChat::RogerThat - 1;
            IssueCommand ("radio3");
          }

          // select correct menu item for this radio message
          IssueCommand ("menuselect %d", radio_slot);
        }
        else if (radio_select_ != RadioChat::ReportingIn) {
          InstantChatter (radio_select_);
        }
      }
      force_radio_ = false; // reset radio to voice
      bots.SetLastRadioTimestamp (team_, game.Time ()); // store last radio usage
    }
    else {
      PushMsgQueue (Msg::Radio);
    }
    break;

    // team independent saytext
  case Msg::Say:
    SendToChat (chat_buffer_, false);
    break;

    // team dependent saytext
  case Msg::SayTeam:
    SendToChat (chat_buffer_, true);
    break;

  default:
    return;
  }
}

void Bot::UpdateEmotions () {
  constexpr auto kEmotionUpdateStep = 0.05f;

  if (!emotion_update_timer_.elapsed ()) {
    return;
  }

  if (see_enemy_timer_.less_than (1.0f)) {
    agression_level_ = ystl::min (agression_level_ + kEmotionUpdateStep, 1.0f);
  }
  else if (see_enemy_timer_.greater_than (5.0f)) {
    // smoothly return aggression to base level
    if (agression_level_ > base_agression_level_) {
      agression_level_ = ystl::max (agression_level_ - kEmotionUpdateStep, base_agression_level_);
    }
    else {
      agression_level_ = ystl::min (agression_level_ + kEmotionUpdateStep, base_agression_level_);
    }

    // smoothly return fear to base level
    if (fear_level_ > base_fear_level_) {
      fear_level_ = ystl::max (fear_level_ - kEmotionUpdateStep, base_fear_level_);
    }
    else {
      fear_level_ = ystl::min (fear_level_ + kEmotionUpdateStep, base_fear_level_);
    }
  }
  emotion_update_timer_.start (0.5f);
}

void Bot::OverrideConditions () {
  const auto tid = GetTaskId ();

  // check if we need to escape from bomb
  if ((tid == TaskId::Normal || tid == TaskId::MoveTo) && game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted () && is_alive_ &&
      IsOutOfBombTimer ()) {

    CompleteTask (); // complete current task

    // then start escape from bomb immediate
    StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
  }
  float reach_enemy_knife_distance_sq = ystl::sqrf (128.0f);

  // special handling, if we have a knife in our hands
  if (IsKnifeMode () && (game.IsPlayerEntity (enemy_) || (cv_attack_monsters && game.IsMonsterEntity (enemy_)))) {
    const auto distance_sq2d = pev->origin.distance_sq2d (enemy_->v.origin);
    const auto nearest_to_enemy_point = graph.GetNearest (enemy_->v.origin);

    if (nearest_to_enemy_point != kInvalidNodeIndex && nearest_to_enemy_point != current_node_index_) {
      reach_enemy_knife_distance_sq = graph[nearest_to_enemy_point].origin.distance_sq (enemy_->v.origin);
      reach_enemy_knife_distance_sq += ystl::sqrf (48.0f);
    }

    // do nodes movement if enemy is not reachable with a knife
    if (distance_sq2d > reach_enemy_knife_distance_sq && has_flag (states_, Sense::SeeingEnemy)) {
      if (nearest_to_enemy_point != kInvalidNodeIndex && nearest_to_enemy_point != current_node_index_ &&
          ystl::abs (graph[nearest_to_enemy_point].origin.z - enemy_->v.origin.z) < 16.0f) {

        const float task_time = game.Time () + distance_sq2d / ystl::sqrf (move_speed_) * 2.0f;

        if (tid != TaskId::MoveTo && !ystl::fequal (Task ()->desire, TaskPri::kHide)) {
          StartTask (TaskId::MoveTo, TaskPri::kHide, nearest_to_enemy_point, task_time, true);
        }
        else if (tid == TaskId::MoveTo && Task ()->data != nearest_to_enemy_point) {
          ClearTask (TaskId::MoveTo);
          StartTask (TaskId::MoveTo, TaskPri::kHide, nearest_to_enemy_point, task_time, true);
        }
      }
    }
    else if (!is_creature_ && distance_sq2d <= reach_enemy_knife_distance_sq && has_flag (states_, Sense::SeeingEnemy) &&
             tid == TaskId::MoveTo) {

      ClearTask (TaskId::MoveTo); // remove any move tasks
    }
  }

  // special handling for sniping
  if (UsesSniper () && !reload_data_.is_reloading && has_flag (states_, Sense::SeeingEnemy | Sense::SuspectEnemy) &&
      shoot_time_ - 0.4f <= game.Time () && shoot_time_ + 0.1f > game.Time () && !sniper_stop_timer_.elapsed ()) {

    const float dot = util.ViewDot (Ent (), enemy_origin_);

    if (dot > 0.95f) {
      IgnoreCollision ();

      move_speed_ = 0.0f;
      strafe_speed_ = 0.0f;

      nav_timer_.start ();
    }
  }

  // special handling for reloading
  if (!game_state.IsRoundOver () && tid == TaskId::Normal && reload_data_.state != Reload::None && reload_data_.is_reloading && !IsDucking () &&
      !IsInNarrowPlace ()) {

    const auto max_clip = conf.FindWeaponById (current_weapon_).max_clip;
    const auto cur_clip = GetAmmoInClip ();

    // consider not reloading if full ammo in clip
    if (cur_clip >= max_clip) {
      reload_data_.is_reloading = false;
    }

    if (see_enemy_timer_.greater_than (2.5f) && has_flag (states_, Sense::SuspectEnemy | Sense::HearingEnemy)) {
      move_speed_ = fear_level_ > agression_level_ ? 0.0f : GetShiftSpeed ();
      nav_timer_.start ();
    }
  }
}

void Bot::UpdatePredictedIndex () {
  if (!is_alive_ || last_enemy_origin_.empty () || !vistab.IsReady () || !game.IsAliveEntity (last_enemy_)) {
    return; // do not run task if no last enemy
  }

  // predict cache lives 1.5-6.0 seconds, no point in recalculating it every frame
  if (!predict_enqueue_timer_.elapsed ()) {
    return;
  }
  predict_enqueue_timer_.start (0.2f);

  auto wipe_predict = [this] () {
    last_predict_index_ = kInvalidNodeIndex;
    last_predict_length_ = kInfiniteDistanceLong;
    predict_cache_.is_valid = false;
  };

  const auto &bot_origin = pev->origin;
  const auto &last_enemy_origin = last_enemy_origin_;
  const auto current_node_index = current_node_index_;

  if (last_enemy_origin.empty () || !vistab.IsReady () || !game.IsAliveEntity (last_enemy_)) {
    wipe_predict ();
    return;
  }

  if (predict_cache_.IsStillValid (last_enemy_origin, game.Time ())) {
    last_predict_index_ = predict_cache_.node_index;
    last_predict_length_ = predict_cache_.path_length;
    return;
  }

  // buckets are main-thread state, worker thread must use bucketless variant
  const int dest_index = graph.GetNearestNoBuckets (last_enemy_origin);

  if (!IsNodeValidForPredict (dest_index)) {
    wipe_predict ();
    return;
  }

  float time_since_last_seen = see_enemy_timer_.elapsed_time ();
  float max_predict_distance = ystl::min (time_since_last_seen * 320.0f, 1024.0f);

  // context is captured by a single pointer, so the lambda stays within ystl::lambda inline storage
  struct PredictContext {
    int path_length {};
    int best_index {};
    int current_node_index {};
    float max_predict_distance_sq {};
    const ystl::Vector *bot_origin {};
    const ystl::Vector *last_enemy_origin {};
  } ctx { 0, current_node_index_, current_node_index, ystl::sqrf (max_predict_distance), &bot_origin, &last_enemy_origin };

  planner.Find (dest_index, current_node_index, [&ctx] (int index) {
    ++ctx.path_length;

    const float dist_from_enemy_sq = ctx.last_enemy_origin->distance_sq (graph[index].origin);
    const float dist_to_bot_sq = ctx.bot_origin->distance_sq (graph[index].origin);

    if (dist_from_enemy_sq > ctx.max_predict_distance_sq) {
      return true;
    }

    if (vistab.Visible (ctx.current_node_index, index) && dist_to_bot_sq > ystl::sqrf (128.0f) && dist_to_bot_sq < ystl::sqrf (2048.0f)) {
      ctx.best_index = index;
      return false;
    }
    return true;
  });

  if (IsNodeValidForPredict (ctx.best_index)) {
    last_predict_index_ = ctx.best_index;
    last_predict_length_ = ctx.path_length;

    predict_cache_.node_index = ctx.best_index;
    predict_cache_.path_length = ctx.path_length;
    predict_cache_.position = graph[ctx.best_index].origin;
    predict_cache_.enemy_origin = last_enemy_origin;
    predict_cache_.timestamp = game.Time ();

    // extend validity if predicted position is within bot's view cone
    const auto extended_predict = IsInFov (graph[ctx.best_index].origin - pev->origin) < 60.0f || move_speed_ < 20.0f;
    const auto extended_validity = extended_predict ? rg (3.0f, 6.0f) : rg (1.5f, 2.9f);

    predict_cache_.valid_until = game.Time () + extended_validity;
    predict_cache_.is_valid = true;

    return;
  }
  wipe_predict ();
}

void Bot::RefreshEnemyPredict () {
  if (is_creature_) {
    return;
  }

  if (game.IsNullEntity (enemy_) && !game.IsNullEntity (last_enemy_) && !last_enemy_origin_.empty ()) {
    const auto distance_to_last_enemy_sq = last_enemy_origin_.distance_sq (pev->origin);

    if (distance_to_last_enemy_sq < ystl::sqrf (2048.0f)) {
      aim_flags_ |= AimFlags::PredictPath;
    }
    const bool deny_last_enemy =
      pev->velocity.length_sq2d () > 0.0f && distance_to_last_enemy_sq < ystl::sqrf (256.0f) && shoot_time_ + 1.5f > game.Time ();

    if (!has_flag (aim_flags_, AimFlags::Enemy | AimFlags::PredictPath | AimFlags::Danger) && !deny_last_enemy &&
        SeesEntity (last_enemy_origin_, true)) {
      aim_flags_ |= AimFlags::LastEnemy;
    }
  }

  if (has_flag (aim_flags_, AimFlags::PredictPath)) {
    UpdatePredictedIndex ();
  }
}

void Bot::SetLastVictim (edict_t *ent) {
  last_victim_ = ent;
  last_victim_origin_ = ent->v.origin;
  last_victim_timer_.start ();

  if (last_victim_origin_.distance_sq (pev->origin) > ystl::sqrf (384.0f)) {
    forget_last_victim_timer_.start (rg (1.0f, 2.0f));
  }
}

void Bot::SetConditions () {
  // this function carried out each frame

  aim_flags_ = AimFlags::Invalid;
  UpdateEmotions ();

  // does bot see an enemy?
  TrackEnemies ();

  // did bot just kill an enemy?
  if (!game.IsNullEntity (last_victim_)) {
    if (game.GetPlayerTeam (last_victim_) != team_) {
      // add some aggression because we just killed somebody
      agression_level_ += 0.1f;

      if (agression_level_ > 1.0f) {
        agression_level_ = 1.0f;
      }
    }
    last_victim_ = nullptr;
  }

  num_friends_left_ = NumFriendsNear (pev->origin, kInfiniteDistance);
  num_enemies_left_ = NumEnemiesNear (pev->origin, kInfiniteDistance);

  // check if our current enemy is still valid
  if (!game.IsNullEntity (last_enemy_)) {
    if (!game.IsAliveEntity (last_enemy_) && shoot_at_dead_timer_.elapsed ()) {
      last_enemy_ = nullptr;
    }
  }
  else {
    last_enemy_ = nullptr;
  }

  // dynamic hearing rate: fast during combat, slow when idle
  constexpr float kHearingBaseInterval = 0.05f; // base time between hearing updates (idle)
  constexpr float kHearingFastInterval = 0.025f; // fast time between hearing updates (combat)

  float hearing_interval = kHearingBaseInterval;

  if (heard_sound_timer_.less_than (3.0f)) {
    hearing_interval = kHearingFastInterval; // recent sound -> faster updates
  }
  // don't listen if seeing enemy, just checked for sounds or being blinded (because its inhuman)
  if (sound_update_timer_.elapsed () && blind_timer_.elapsed () && see_enemy_timer_.greater_than (0.5f)) {

    UpdateHearing ();
    sound_update_timer_.start (hearing_interval);
  }
  else if (!sound_update_timer_.elapsed () && heard_sound_timer_.greater_than (10.0f)) {
    states_ &= ~Sense::HearingEnemy;

    // clear the last enemy pointers if time has passed or enemy far away
    if (!last_enemy_origin_.empty ()) {
      const auto distance_sq = pev->origin.distance_sq (last_enemy_origin_);

      if (distance_sq >= ystl::sqrf (2048.0f) || (game.IsNullEntity (enemy_) && see_enemy_timer_.greater_than (10.0f))) {
        last_enemy_origin_.clear ();
        last_enemy_ = nullptr;

        aim_flags_ &= ~AimFlags::LastEnemy;
      }
    }
  }
  RefreshEnemyPredict ();

  // check for grenades depending on difficulty
  if (rg.chance (ystl::max (25, Skill ())) && !is_creature_) {
    CheckGrenadesThrow ();
  }

  // check if there are items needing to be used/collected
  if (item_check_timer_.elapsed () || !game.IsNullEntity (pickup_item_)) {
    UpdatePickups ();
    item_check_timer_.start (0.5f);
  }
  FilterTasks ();
}

void Bot::CheckParachute () {
  static ConVarRef parachute (conf.FetchCustom ("AMXParachuteCvar").chars ());

  // if no cvar or it's not enabled do not bother
  if (parachute.Exists () && parachute.Value () > 0.0f) {
    if (IsOnLadder () || pev->velocity.z > -50.0f || IsOnFloor ()) {
      fall_down_timer_.invalidate ();
    }
    else if (!fall_down_timer_.started ()) {
      fall_down_timer_.start (0.35f);
    }

    // press use anyway
    if (fall_down_timer_.started () && fall_down_timer_.elapsed ()) {
      pev->button |= IN_USE;
    }
  }
}

bool Bot::TryTeleportToSealedBomb () {
  // sometime bots sometimes walk onto a plant sealed by mode walls, so we cheat back:
  // if the planted bomb is sealed off, teleport the bot inside instead of wandering outside

  if (!mode_walls.HasWalls ()) {
    return false;
  }

  if (!game.MapIs (MapFlags::Demolition) || !is_alive_) {
    return false;
  }

  if (!game_state.IsBombPlanted () || has_progress_bar_) {
    return false;
  }

  if (GetTaskId () == TaskId::EscapeFromBomb) {
    return false;
  }

  if (team_ != Team::CT && team_ != Team::Terrorist) {
    return false;
  }
  const auto &bomb_origin = game_state.GetBombOrigin ();

  if (bomb_origin.empty ()) {
    return false;
  }

  // not sealed - normal pathfinding handles it
  if (!mode_walls.IsNodeBlocked (bomb_origin) && !mode_walls.IsSegmentBlocked (pev->origin, bomb_origin)) {
    return false;
  }

  // sealed off, but we're already standing on the plant (e.g. inner partition) - nothing to cheat
  if (pev->origin.distance_sq (bomb_origin) < ystl::sqrf (48.0f)) {
    return false;
  }

  // find a free spot near the bomb (within defuse reach)
  ystl::Vector dest {};
  bool found = false;
  float best_dist_sq = kInfiniteDistance;

  for (const auto &node : graph) {
    if (bomb_origin.distance_sq (node.origin) > ystl::sqrf (256.0f)) {
      continue;
    }

    if (mode_walls.IsNodeBlocked (node.origin)) {
      continue;
    }

    if (mode_walls.IsSegmentBlocked (node.origin, bomb_origin)) {
      continue;
    }
    const float d = bomb_origin.distance_sq (node.origin);

    if (d < best_dist_sq) {
      best_dist_sq = d;
      dest = node.origin;
      found = true;
    }
  }

  // no free graph node nearby - try radial offsets around the bomb
  if (!found) {
    constexpr float radii[] = { 48.0f, 96.0f, 144.0f };

    for (const float r : radii) {
      for (int dx = -1; dx <= 1 && !found; ++dx) {
        for (int dy = -1; dy <= 1 && !found; ++dy) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          ystl::Vector cand = bomb_origin + ystl::Vector (static_cast<float> (dx) * r, static_cast<float> (dy) * r, 16.0f);

          if (mode_walls.IsNodeBlocked (cand)) {
            continue;
          }

          if (mode_walls.IsSegmentBlocked (cand, bomb_origin)) {
            continue;
          }
          dest = cand;
          found = true;
        }
      }

      if (found) {
        break;
      }
    }
  }

  // last resort - bomb itself, if not inside a wall box
  if (!found) {
    if (mode_walls.IsNodeBlocked (bomb_origin)) {
      return false;
    }
    dest = bomb_origin + ystl::Vector (0.0f, 0.0f, 16.0f);
    found = true;
  }

  // jitter so the whole team doesn't stack on a single spot
  const ystl::Vector base = dest;
  dest.x += rg (-24.0f, 24.0f);
  dest.y += rg (-24.0f, 24.0f);

  if (mode_walls.IsNodeBlocked (dest) || mode_walls.IsSegmentBlocked (dest, bomb_origin)) {
    dest = base;
  }

  engfuncs.pfnSetOrigin (Ent (), dest);
  pev->velocity.clear ();

  ClearSearchNodes ();
  path_walk_.Clear ();
  current_node_index_ = graph.GetNearest (dest);
  prev_origin_ = dest;
  dest_origin_ = dest;
  stuck_timer_.invalidate ();
  nav_timer_.reset ();

  // force repath to the bomb on next think instead of keeping the outside fallback goal
  Task ()->data = kInvalidNodeIndex;
  chosen_goal_index_ = kInvalidNodeIndex;
  bomb_search_overridden_ = false;

  DebugMsg ("cheated via teleport to sealed bomb (walls up).");

  return true;
}

void Bot::SlowFrame () {
  TryTeleportToSealedBomb ();

  if (game_state.IsBombPlanted () && team_ == Team::CT && is_alive_) {
    const auto &bomb_position = game_state.GetBombOrigin ();

    if (!has_progress_bar_ && GetTaskId () != TaskId::EscapeFromBomb && pev->origin.distance_sq (bomb_position) < ystl::sqrf (1540.0f) &&
        !IsBombDefusing (bomb_position)) {

      ignored_items_.clear ();
      item_check_timer_.start (0.0f);

      const auto tid = GetTaskId ();

      if (tid == TaskId::Camp) {
        ClearTask (GetTaskId ());
      }

      // try pickup chain first, fall back to defuse task if bot lingers near bomb
      if (tid != TaskId::DefuseBomb) {
        if (bomb_position.distance_sq (pev->origin) >= ystl::sqrf (96.0f)) {
          near_bomb_timer_.invalidate ();
        }
        else if (!near_bomb_timer_.started ()) {
          near_bomb_timer_.start (rg (2.0f, 4.0f));
        }
        else if (near_bomb_timer_.elapsed ()) {
          StartTask (TaskId::DefuseBomb, TaskPri::kDefuseBomb, kInvalidNodeIndex, 0.0f, true);
        }
      }
      else {
        near_bomb_timer_.invalidate ();
      }
    }
  }

  CheckSpawnConditions ();
  CheckForChat ();
  CheckBreakablesAround ();

  if (game.Is (GameFlags::HasBotVoice)) {
    ShowChatterIcon (false); // end voice feedback
  }

  // kick the bot if stay time is over, the quota maintain will add new bot for us later
  if (cv_rotate_bots && stay_timer_.elapsed ()) {
    kicked_by_rotation_ = true; // kicked by rotation, so not save bot name if save bot names is active

    Kick ();
    return;
  }
  slow_frame_timer_.start (0.5f);
}

void Bot::Update () {
  const auto tid = GetTaskId ();

  // remember the think interval, used by all the prediction code; clamp against hitches
  frame_interval_ = ystl::clamp (game.Time () - previous_think_time_, 0.0f, 0.25f);
  previous_think_time_ = game.Time ();

  can_set_aim_direction_ = true;
  is_alive_ = game.IsAliveEntity (Ent ());
  team_ = game.GetPlayerTeam (Ent ());
  health_value_ = ystl::clamp (pev->health, 0.0f, 99999.9f);

  if (team_ == Team::Terrorist && game.MapIs (MapFlags::Demolition)) {
    has_c4_ = has_flag (pev->weapons, ystl::bit (Weapon::C4));

    if (has_c4_ && (cv_ignore_objectives || cv_jasonmode)) {
      if (cv_ignore_objectives) {
        DonateC4ToHuman ();
      }
      has_c4_ = false;
    }
  }
  else if (team_ == Team::CT && game.MapIs (MapFlags::HostageRescue)) {
    has_hostage_ = HasHostage ();
  }
  is_creature_ = IsCreature ();

  // damage victim action
  if (last_damage_timestamp_ < game.Time () && !ystl::fzero (last_damage_timestamp_)) {
    last_damage_timestamp_ = 0.0f;
  }
  pev->flags |= FL_CLIENT | FL_FAKECLIENT; // restore fake client bit, just in case

  // is bot movement enabled
  bot_movement_ = false;

  // for some unknown reason some bots have speed of 1.0 after respawn on csdm
  if (game.Is (GameFlags::CSDM) && ystl::fequal (pev->maxspeed, 1.0f) && tid == TaskId::Normal) {
    static ConVarRef sv_maxspeed ("sv_maxspeed");

    // reset max speed to max value, thus allowing bot movement
    pev->maxspeed = sv_maxspeed.Value ();
  }

  // if the bot hasn't selected stuff to start the game yet, go do that
  if (not_started_) {
    UpdateTeamJoin (); // select team & class
  }
  else if (!is_alive_) {
    // we got a teamkiller? vote him away
    if (vote_kick_index_ != last_vote_kick_ && cv_tkpunish) {
      IssueCommand ("vote %d", vote_kick_index_);
      last_vote_kick_ = vote_kick_index_;

      // if bot tk punishment is enabled slay the tk
      if (cv_tkpunish.As<int> () != 2 || game.IsFakeClientEntity (game.EntityOfIndex (vote_kick_index_))) {
        return;
      }
      auto killer = game.EntityOfIndex (last_vote_kick_);

      if (killer != nullptr) {
        ++killer->v.frags;
        MDLL_ClientKill (killer);
      }
    }

    // host wants us to kick someone
    else if (vote_map_ != 0) {
      IssueCommand ("votemap %d", vote_map_);
      vote_map_ = 0;
    }
  }
  else if (IsMoveAllowed () && buying_finished_ && !(pev->maxspeed < 10.0f && !HasBombTask ()) &&
           !cv_freeze_bots && !graph.HasChanged ()) {

    bot_movement_ = true;
  }
  CheckMsgQueue ();

  if (!is_stale_ && bot_movement_) {
    Logic (); // execute main code
  }
  else if (pev->maxspeed < 10.0f) {
    LogicDuringFreezetime ();
  }
  else if (!bot_movement_) {
    ResetMovement ();
  }
}

void Bot::LogicDuringFreezetime () {
  if (is_stale_) {
    return;
  }
  pev->button &= ~IN_DUCK;

  UpdateLookAngles ();

  if (!change_view_timer_.elapsed ()) {
    return;
  }

  if (rg.chance (15) && jump_time_ < game.Time ()) {
    pev->button |= IN_JUMP;
    jump_time_ = game.Time () + rg (1.0f, 3.0f);
  }
  ystl::SmallArray<edict_t *> players {};

  // search for visible enemies
  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.IsSameTeam (team_) || !SeesEntity (client.origin)) {
      continue;
    }
    players.push (client.ent);
  }

  // use teammates
  if (players.empty ()) {
    for (const auto &client : clients) {
      if (!client.IsTeammate (team_, Ent ()) || !SeesEntity (client.origin)) {
        continue;
      }
      players.push (client.ent);
    }
  }
  else {
    SelectBestWeapon ();
  }

  if (!players.empty ()) {
    auto ent = players.random ();

    if (ent) {
      look_at_ = ent->v.origin + ent->v.view_ofs;

      if (buying_finished_ && game.GetPlayerTeam (ent) != team_) {
        enemy_ = ent;
        enemy_origin_ = ent->v.origin;
      }
    }

    // good time too greet everyone
    if (need_to_send_welcome_chat_) {
      PushChatMessage (Chat::Hello);
      need_to_send_welcome_chat_ = false;
    }
  }
  change_view_timer_.start (rg (1.25f, 3.0f));
}

void Bot::ExecuteTasks () {
  // this is core function that handle task execution

  auto func = Task ()->func;

  // run the current task
  (this->*func) ();
}

void Bot::CheckSpawnConditions () {
  // this function is called instead of ai when buying finished, but freezetime is not yet left

  if (!game.IsNullEntity (enemy_)) {
    return;
  }

  // switch to knife if time to do this
  if (check_knife_switch_ && buying_finished_ && spawn_timer_.greater_than (rg (5.0f, 7.5f))) {
    if (rg (1, 100) < cv_spraypaints.As<int> () && pev->groundentity == game.GetStartEntity ()) {
      StartTask (TaskId::Spraypaint, TaskPri::kSpraypaint, kInvalidNodeIndex, game.Time () + 1.0f, false);
    }

    // keep an active reload, only a pending scan may be interrupted
    if (difficulty_ >= Difficulty::Normal && !reload_data_.is_reloading &&
        game.MapIs (MapFlags::HostageRescue | MapFlags::Demolition | MapFlags::Escape | MapFlags::Assassination)) {

      // 10% chance to switch to he grenade if bot has one
      if (rg.chance (10) && has_flag (pev->weapons, ystl::bit (Weapon::Explosive))) {
        SelectWeaponById (Weapon::Explosive);
      }
      // switch to knife (except scout snipers)
      else if (IsKnifeMode ()) {
        DropCurrentWeapon ();
      }
      else if (current_weapon_ != Weapon::Scout) {
        SelectWeaponById (Weapon::Knife);
      }
      check_knife_switch_ = false;
    }

    if (rg.chance (cv_user_follow_percent.As<int> ()) && game.IsNullEntity (target_entity_) && !is_leader_ && !has_c4_ && rg.chance (50)) {
      DecideFollowUser ();
    }
  }

  // check if we already switched weapon mode
  if (check_weapon_switch_ && buying_finished_ && spawn_timer_.greater_than (rg (3.0f, 4.5f))) {
    if (HasShield () && IsShieldDrawn ()) {
      pev->button |= IN_ATTACK2;
    }
    else {
      switch (current_weapon_) {
      case Weapon::M4A1:
      case Weapon::USP:
        CheckSilencer ();
        break;

      case Weapon::Famas:
      case Weapon::Glock18:
        if (rg.chance (50)) {
          pev->button |= IN_ATTACK2;
        }
        break;

      default:
        break;
      }
    }

    // movement in freezetime is disabled, so queue the press for the next think
    pending_buttons_ |= IN_ATTACK2;
    check_weapon_switch_ = false;
  }
}

void Bot::Logic () {
  // this function gets called each frame and is the core of all bot ai. from here all other subroutines are called

  ResetMovement ();

  // increase reaction time
  actual_reaction_time_ += 0.3f;
  moved_distance_ = kMinMovedDistance + 0.1f; // length of different vector (distance bot moved)

  if (actual_reaction_time_ > ideal_reaction_time_) {
    actual_reaction_time_ = ideal_reaction_time_;
  }

  // bot could be blinded by flashbang or smoke, recover from it
  view_distance_ += 3.0f;

  if (view_distance_ > max_view_distance_) {
    view_distance_ = max_view_distance_;
  }

  if (!blind_timer_.elapsed ()) {
    max_view_distance_ = 4096.0f;
  }
  move_speed_ = pev->maxspeed;

  if (prev_timer_.elapsed ()) {

    // see how far bot has moved since the previous position
    moved_distance_ = prev_origin_.distance_sq (pev->origin);

    // save current position as previous
    prev_origin_ = pev->origin;
    prev_timer_.start (0.2f - frame_interval_);
  }

  // if there's some radio message to respond, check it
  if (radio_order_ != RadioChat::InvalidSelect) {
    CheckRadioQueue ();
  }

  // do all sensing, calculate/filter all actions here
  if (CanRunHeavyWeight ()) {
    SetConditions ();
  }
  else if (!game.IsNullEntity (enemy_)) {
    TrackEnemies ();
  }
  ExecuteChatterFrameEvents ();

  check_terrain_ = true;
  move_to_goal_ = true;
  wants_to_fire_ = false;

  // avoid flyings grenades, if needed
  if (cv_avoid_grenades && !is_creature_) {
    AvoidGrenades ();
  }
  is_using_grenade_ = false;

  ExecuteTasks (); // execute current task
  SetAimDirection (); // choose aim direction
  UpdateLookAngles (); // and turn to chosen aim direction
  DoFireWeapons (); // fire the weapons

  // check for reloading
  if (reload_data_.check_timer.elapsed ()) {
    CheckReload ();
  }

  // set the reaction time (surprise momentum) different each frame according to skill
  SetIdealReactionTimers ();

  // calculate 2 direction vectors, 1 without the up/down component
  const ystl::Vector dir_old = dest_origin_ - (pev->origin + pev->velocity * frame_interval_);
  const ystl::Vector dir_normal = dir_old.normalize2d ();

  move_angles_ = dir_old.angles ();
  move_angles_.clamp_angles ();
  move_angles_.x = -move_angles_.x; // invert for engine

  // clamp movement pitch on the ground: a goal nearly straight below or above
  // collapses the horizontal drive and stalls the bot on the spot
  if (IsOnFloor () && !IsInWater () && !IsOnLadder ()) {
    move_angles_.x = ystl::clamp (move_angles_.x, -45.0f, 45.0f);
  }

  // do some overriding for special cases
  OverrideConditions ();

  // allowed to move to a destination position?
  if (move_to_goal_) {
    MoveToGoal ();
  }

  // are we allowed to check blocking terrain (and react to it)?
  if (check_terrain_) {
    // check for breakables around bots movement direction
    CheckBreakable (nullptr);

    DoPlayerAvoidance (dir_normal);
    CheckTerrain (dir_normal);
  }

  // if we have fallen from the place of move, the nearest point is allowed
  CheckFall ();

  // check the darkness
  if (!is_creature_ && cv_check_darkness) {
    CheckDarkness ();
  }

  // must avoid a grenade?
  if (need_avoid_grenade_ != 0) {
    // don't duck to get away faster
    pev->button &= ~IN_DUCK;

    ystl::Vector right {}, forward {};
    pev->v_angle.angle_vectors (&forward, &right, nullptr);

    const ystl::Vector front = forward * -pev->maxspeed * 0.2f;
    const ystl::Vector side = right * pev->maxspeed * static_cast<float> (need_avoid_grenade_) * 0.2f;
    const ystl::Vector spot = pev->origin + front + side + pev->velocity * frame_interval_;

    if (!IsDeadlyMove (spot)) {
      move_speed_ = -pev->maxspeed;
      strafe_speed_ = pev->maxspeed * static_cast<float> (need_avoid_grenade_);
    }
  }

  // ensure we're not stuck picking something check if elapsed time exceeds expected travel time plus tolerance
  if (move_to_goal_ && move_speed_ > 0.0f && !has_flag (states_, Sense::SeeingEnemy)) {
    const float distance = dest_origin_.distance2d (pev->origin);
    const float speed = ystl::max (1.0f, move_speed_);

    const float expected_travel_time = distance / speed; // time = distance / speed
    const float tolerance = rg (2.5f, 3.5f); // random tolerance to prevent simultaneous bot unstucking

    if (nav_timer_.elapsed_time () > expected_travel_time + tolerance) {
      EnsurePickupEntitiesClear ();
    }
  }

  // check if need to use parachute
  CheckParachute ();

  // display some debugging thingy to host entity
  if (ctrl.IsDebug (1)) {
    ShowDebugOverlay ();
  }

  // save the previous speed (for checking if stuck)
  prev_speed_ = ystl::abs (move_speed_);
  last_damage_type_ = -1; // reset damage
}

void Bot::Spawned () {
  if (game.Is (GameFlags::CSDM | GameFlags::ZombieMod)) {
    NewRound ();
    ClearTasks ();

    buying_finished_ = true;
  }
}

void Bot::ShowDebugOverlay () {
  // early exit if no editor available
  if (!graph.HasEditor ()) {
    return;
  }

  auto overlay_entity = graph.GetEditor ();

  // determine if debug overlay should be displayed
  auto should_display_overlay = [&] () -> bool {
    // check if spectating this bot
    if (overlay_entity->v.iuser2 == Entindex () && overlay_entity->v.origin.distance_sq (pev->origin) < ystl::sqrf (256.0f)) {
      return true;
    }

    // check if this is the nearest bot (when debug level >= 2)
    if (ctrl.IsDebug (2)) {
      if (const auto nearest = util.FindNearestBot ({ .origin = overlay_entity, .distance = 128.0f, .alive = true, .visible = true })) {
        if (*nearest == this) {
          return true;
        }
      }
    }

    return false;
  };

  if (!should_display_overlay ()) {
    return;
  }

  // render hud text information into the multi-panel debug console
  auto render_debug_text = [&] () {
    if (tasks_.Empty ()) {
      return;
    }

    // only update text when timer expires
    if (!debug_update_timer_.elapsed ()) {
      return;
    }

    // pull the live bot state into the shared debug panel and send it out
    botdebug.Update (this);
    botdebug.Render (overlay_entity, pev->netname.str ());
    debug_update_timer_.start (DebugPanel::kRefreshInterval);
  };

  // render visual debug lines
  auto render_debug_lines = [&] () {
    constexpr auto kArrowLifeTime = 1;
    constexpr auto kLineWidth = 10;
    constexpr auto kLineNoise = 0;
    constexpr auto kLineBrightness = 250;
    constexpr auto kLineSpeed = 5;

    const ystl::Vector eye_position = GetEyesPos ();

    // green arrow: destination origin
    game.DrawLine (overlay_entity, eye_position, dest_origin_, kLineWidth, kLineNoise, { 0, 255, 0 }, kLineBrightness, kLineSpeed,
      kArrowLifeTime, DrawLineType::Arrow);

    // blue arrow: ideal angles direction
    game.DrawLine (overlay_entity, eye_position - ystl::Vector (0.0f, 0.0f, 16.0f), eye_position + ideal_angles_.forward () * 300.0f, kLineWidth,
      kLineNoise, { 0, 0, 255 }, kLineBrightness, kLineSpeed, kArrowLifeTime, DrawLineType::Arrow);

    // red arrow: current view angles direction
    game.DrawLine (overlay_entity, eye_position - ystl::Vector (0.0f, 0.0f, 32.0f), eye_position + pev->v_angle.forward () * 300.0f, kLineWidth,
      kLineNoise, { 255, 0, 0 }, kLineBrightness, kLineSpeed, kArrowLifeTime, DrawLineType::Arrow);

    // orange arrows: path walk visualization
    constexpr auto kPathLineWidth = 15;
    constexpr auto kPathLineNoise = 0;
    constexpr auto kPathLineBrightness = 200;
    constexpr auto kPathLineSpeed = 5;

    for (size_t i = 0; i + 1 < path_walk_.Length (); ++i) {
      const ystl::Vector &start_pos = graph[path_walk_.At (i)].origin;
      const ystl::Vector &end_pos = graph[path_walk_.At (i + 1)].origin;

      game.DrawLine (overlay_entity, start_pos, end_pos, kPathLineWidth, kPathLineNoise, { 255, 100, 55 }, kPathLineBrightness, kPathLineSpeed,
        kArrowLifeTime, DrawLineType::Arrow);
    }

    if (cv_debug.As<int> () < 3) {
      return;
    }

    // green box: bot bounding box
    constexpr auto kBoxWidth = 5;
    constexpr auto kBoxSpeed = 0;

    game.DrawBox (overlay_entity, pev->absmin, pev->absmax, kBoxWidth, kLineNoise, { 0, 255, 0 }, kLineBrightness, kBoxSpeed, kArrowLifeTime);

    // white line: eye ray up to the first obstacle
    Trace::Result tr {};

    trace.Line (eye_position, eye_position + pev->v_angle.forward () * 1024.0f, TraceIgnore::Monsters, Ent (), &tr);

    game.DrawLine (
      overlay_entity, eye_position, tr.end_pos, kLineWidth, kLineNoise, { 255, 255, 255 }, kLineBrightness, kLineSpeed, kArrowLifeTime);

    // yellow arrows: recent sound sources still in memory
    constexpr auto kSoundLineBrightness = 200;
    constexpr auto kSoundLineSpeed = 5;

    const auto now = game.Time ();

    for (int i = 0; i < kSoundMemorySize; ++i) {
      const auto &mem = sound_memory_[i];

      if (mem.source == nullptr || mem.time + 5.0f < now) {
        continue;
      }
      // fade the arrow color with sound age, older sounds are dimmer
      const auto age = ystl::clamp (1.0f - (now - mem.time) / 5.0f, 0.0f, 1.0f);
      const auto shade = static_cast<int> (80.0f + 175.0f * age);

      game.DrawLine (overlay_entity, pev->origin, mem.pos, kLineWidth, kLineNoise, { shade, shade, 0 }, kSoundLineBrightness, kSoundLineSpeed,
        kArrowLifeTime, DrawLineType::Arrow);
    }

    // cyan probes: forward / left / right collision checks at feet and head heights
    const ystl::Vector right = pev->v_angle.right ();

    constexpr auto kProbeDistance = 96.0f;
    constexpr auto kFootHeight = 17.0f;
    constexpr auto kHeadHeight = 64.0f;

    const ystl::Vector probe_dirs[3] { pev->v_angle.forward (), right * -1.0f, right };

    for (const auto &dir : probe_dirs) {
      for (const float height : { kFootHeight, kHeadHeight }) {
        const ystl::Vector start = pev->origin + ystl::Vector (0.0f, 0.0f, height);

        trace.Line (start, start + dir * kProbeDistance, TraceIgnore::Monsters, Ent (), &tr);

        game.DrawLine (overlay_entity, start, tr.end_pos, kLineWidth, kLineNoise, { 0, 255, 255 }, kLineBrightness, kLineSpeed, kArrowLifeTime);
      }
    }
  };

  render_debug_text ();
  render_debug_lines ();
}

bool Bot::HasHostage () {
  if (cv_ignore_objectives || !game.MapIs (MapFlags::HostageRescue)) {
    return false;
  }

  for (auto &hostage : hostages_) {
    if (!game.IsNullEntity (hostage)) {

      // don't care about dead hostages
      if (hostage->v.health > 0.0f || pev->origin.distance_sq (hostage->v.origin) < ystl::sqrf (600.0f)) {
        return true;
      }
      else {
        hostages_.remove (hostage);
        hostage = nullptr;
      }
    }
  }
  return false;
}

void Bot::TakeDamage (edict_t *inflictor, int damage, int armor, int bits) {
  // called from net message handler when the bot is hurt by another player

  last_damage_type_ = bits;
  last_damage_timestamp_ = game.Time ();

  if (is_creature_) {
    // creatures should remember the direction of damage, even if attacker is not visible
    if (game.IsPlayerEntity (inflictor) && game.GetRealPlayerTeam (inflictor) != team_ && game.IsNullEntity (enemy_)) {
      if (SeesEnemy (inflictor)) {
        enemy_ = inflictor;
        enemy_origin_ = inflictor->v.origin;
      }
      else {
        last_enemy_ = inflictor;
        last_enemy_origin_ = inflictor->v.origin;
        see_enemy_timer_.start ();
      }
    }
    return;
  }

  if (!game.Is (GameFlags::CSDM)) {
    practice.UpdateValue (this, damage);
  }

  if (game.IsPlayerEntity (inflictor) || (cv_attack_monsters && game.IsMonsterEntity (inflictor))) {
    const auto inflictor_team = game.GetPlayerTeam (inflictor);

    if (!game.IsMonsterEntity (inflictor) && cv_tkpunish && inflictor_team == team_ && !game.IsFakeClientEntity (inflictor)) {
      // alright, die you team killer!!!
      actual_reaction_time_ = 0.0f;
      see_enemy_timer_.start ();
      enemy_ = inflictor;

      last_enemy_ = enemy_;
      last_enemy_origin_ = enemy_->v.origin;
      enemy_origin_ = enemy_->v.origin;

      PushChatMessage (Chat::TeamAttack);
      PushRadioChat (RadioChat::FriendlyFire);
    }
    else {
      // increase radio percent
      radio_percent_ = ystl::max (radio_percent_ + 1, 90);

      // attacked by an enemy
      if (health_value_ > 60.0f) {
        agression_level_ += 0.1f;

        if (agression_level_ > 1.0f) {
          agression_level_ = 1.0f;
        }
      }
      else {
        fear_level_ += 0.03f;

        if (fear_level_ > 1.0f) {
          fear_level_ = 1.0f;
        }
      }
      ClearTask (TaskId::Camp);

      if (game.IsNullEntity (enemy_) && team_ != inflictor_team) {
        last_enemy_ = inflictor;
        last_enemy_origin_ = inflictor->v.origin;

        // fixme - bot doesn't necessary sees this enemy
        see_enemy_timer_.start ();
      }

      if (!game.Is (GameFlags::CSDM)) {
        practice.UpdateDamage (this, inflictor, armor + damage);
      }
    }
  }
  // hurt by unusual damage like drowning or gas
  else {
    // leave the camping/hiding position
    if (!IsReachableNode (graph.GetNearest (dest_origin_))) {
      ClearSearchNodes ();
      FindNextBestNode ();
    }
  }
}

void Bot::TakeBlind (int alpha) {
  // called on screenfade message to blind the bot from grenade flash

  view_distance_ = rg (10.0f, 20.0f);

  // do not take in effect some unique map effects on round start
  if (game_state.GetRoundStartTime () + 5.0f < game.Time ()) {
    view_distance_ = max_view_distance_;
  }
  blind_timer_.start (static_cast<float> (alpha - 180) / 16.0f);

  if (blind_timer_.elapsed ()) {
    return;
  }
  enemy_ = nullptr;

  if (difficulty_ <= Difficulty::Normal) {
    blind_move_speed_ = 0.0f;
    blind_side_move_speed_ = 0.0f;
    blind_button_ = IN_DUCK;

    return;
  }
  blind_node_index_ = FindCoverNode (900.0f);
  blind_move_speed_ = -pev->maxspeed;
  blind_side_move_speed_ = 0.0f;

  if (rg.chance (50)) {
    blind_side_move_speed_ = pev->maxspeed;
  }
  else {
    blind_side_move_speed_ = -pev->maxspeed;
  }

  if (health_value_ < 85.0f) {
    blind_move_speed_ = -pev->maxspeed;
  }
  else if (personality_ == Personality::Careful) {
    blind_move_speed_ = 0.0f;
    blind_button_ = IN_DUCK;
  }
  else {
    blind_move_speed_ = pev->maxspeed;
  }
}

void Bot::DropWeaponForUser (edict_t *user, bool discard_c4) {
  // discard current primary weapon for the user requesting it

  if (game.IsAliveEntity (user) && money_amount_ >= 2000 && HasPrimaryWeapon () &&
      user->v.origin.distance_sq (pev->origin) <= ystl::sqrf (450.0f)) {
    aim_flags_ |= AimFlags::Entity;
    look_at_ = user->v.origin;

    if (discard_c4 && has_c4_) {
      SelectWeaponById (Weapon::C4);
      DropCurrentWeapon ();
    }
    else if (!discard_c4) {
      SelectBestWeapon ();
      DropCurrentWeapon ();
    }

    pickup_item_ = nullptr;
    pickup_type_ = Pickup::None;
    item_check_timer_.start (5.0f);

    if (in_buy_zone_) {
      ignore_buy_delay_ = true;
      buying_finished_ = false;
      buy_state_ = BuyState::PrimaryWeapon;

      PushMsgQueue (Msg::Buy);
      next_buy_timer_.start (0.0f);
    }
  }
}

void Bot::StartDoubleJump (edict_t *ent) {
  ResetDoubleJump ();

  double_jump_origin_ = ent->v.origin;
  double_jump_entity_ = ent;

  StartTask (TaskId::DoubleJump, TaskPri::kDoubleJump, kInvalidNodeIndex, game.Time (), true);
  SendToChat (ystl::strings.format ("Ok %s, i will help you!", ent->v.netname.str ()), true);
}

void Bot::SendBotToOrigin (const ystl::Vector &origin) {
  position_ = origin;
  chosen_goal_index_ = graph.GetNearestNoBuckets (origin);

  Task ()->data = chosen_goal_index_;

  StartTask (TaskId::MoveTo, TaskPri::kHide, chosen_goal_index_, 0.0f, true);
}

void Bot::ResetDoubleJump () {
  CompleteTask ();

  double_jump_entity_ = nullptr;
  duck_for_jump_ = 0.0f;
  double_jump_origin_.clear ();
  travel_start_index_ = kInvalidNodeIndex;
  jump_ready_ = false;
}

void Bot::DebugMsgInternal (ystl::StringRef str) {
  const int level = ctrl.DebugLevel ();

  // debug levels: 0-2 = disabled, 3 = spectator only, 4+ = all
  if (!ctrl.IsDebug (3)) {
    return;
  }

  ystl::String print_buf {};
  print_buf.assignf ("%s: %s", pev->netname.str (), str);

  // determine if message should be displayed to this user
  bool should_display = false;

  if (level >= 4) {
    // level 4+: show to everyone
    should_display = true;
  }
  else if (level == 3) {
    // level 3: show only if user is spectating this bot
    should_display = !game.IsNullEntity (game.GetLocalEntity ()) && game.GetLocalEntity ()->v.iuser2 == Entindex ();
  }

  if (!should_display) {
    return;
  }

  // output to logger, never pass player text as a format string
  ystl::logger.message ("%s", print_buf.chars ());

  // output to chat area for non-dedicated servers, so players see it ingame
  if (!game.IsDedicatedServer ()) {
    SendToChat (print_buf, false);
  }
}

ystl::Vector Bot::IsBombAudible () {
  // fallback bomb detection: used when the c4 beep sound hook hasn't registered

  if (!game_state.IsBombPlanted () || GetTaskId () == TaskId::EscapeFromBomb) {
    return nullptr;
  }
  const auto &bomb_origin = game_state.GetBombOrigin ();

  // check if bot already has a recent sound memory entry from the c4 beep hook
  for (int i = 0; i < kSoundMemorySize; ++i) {
    const auto &mem = sound_memory_[i];

    if (mem.source == nullptr || mem.time + 5.0f < game.Time ()) {
      continue;
    }
    if (has_flag (mem.type, Noise::Defuse) && mem.pos.distance_sq (bomb_origin) < ystl::sqrf (256.0f)) {
      return bomb_origin; // sound hook already providing c4 position
    }
  }

  // fallback ticking check with wider panic radius when blast is near
  float desired_radius = kBombHearDistance;

  const float time_elapsed = ((game.Time () - game_state.GetTimeBombPlanted ()) / mp_c4timer.As<float> ()) * 100.0f;

  if (time_elapsed > 85.0f) {
    desired_radius = kBombPanicHearDistance;
  }

  if (pev->origin.distance_sq2d (bomb_origin) < ystl::sqrf (desired_radius)) {
    return bomb_origin;
  }
  return nullptr;
}

bool Bot::CanRunHeavyWeight () {
  if (heavy_timer_.elapsed ()) {
    heavy_timer_.start (full_think_interval_);

    return true;
  }
  return false;
}

bool Bot::IsOutOfBombTimer () {
  if (!game.MapIs (MapFlags::Demolition)) {
    return false;
  }

  if (current_node_index_ == kInvalidNodeIndex || escaped_from_bomb_ || (has_progress_bar_ || GetTaskId () == TaskId::EscapeFromBomb)) {
    return false; // if ct bot already start defusing, or already escaping, return false
  }

  // calculate left time
  const float time_left = game_state.GetBombTimeLeft ();

  // if time left greater than 13, no need to do other checks
  if (time_left > 13.0f) {
    return false;
  }
  const auto &bomb_origin = game_state.GetBombOrigin ();

  // for terrorist, if timer is lower than 13 seconds, return true
  if (time_left < 13.0f && team_ == Team::Terrorist && bomb_origin.distance_sq (pev->origin) < ystl::sqrf (964.0f)) {
    return true;
  }
  bool has_teammates_with_defuser_kit = false;

  // check if our players has defusal kit
  for (const auto &bot : bots) {
    // search players with defuse kit
    if (&bot != this && bot.team_ == Team::CT && bot.has_defuser_ && bomb_origin.distance_sq (bot.pev->origin) < ystl::sqrf (512.0f)) {
      has_teammates_with_defuser_kit = true;
      break;
    }
  }

  // add reach time to left time
  const float reach_time = graph.CalculateTravelTime (pev->maxspeed, path_origin_, bomb_origin);

  // for counter-terrorist check alos is we have time to reach position plus average defuse time
  if ((time_left < reach_time + 8.0f && !has_defuser_ && !has_teammates_with_defuser_kit) || (time_left < reach_time + 4.0f && has_defuser_)) {
    return true;
  }

  if (has_progress_bar_ && IsOnFloor () && ((has_defuser_ ? 10.0f : 15.0f) > game_state.GetBombTimeLeft ())) {
    return true;
  }
  return false; // return false otherwise
}

void Bot::UpdateHearing () {
  if (game.Is (GameFlags::FreeForAll) || !enemy_ignore_timer_.elapsed () || cv_ignore_enemies) {
    return;
  }
  heard_enemy_ = nullptr;

  float nearest_distance_sq = kInfiniteDistance;
  float best_score = kInfiniteDistance;

  Noise best_sound_type {};

  // do not hear to other enemies if just tracked old one
  if (!UsesKnife () && next_tracking_timer_.elapsed () && last_enemy_ == tracking_edict_ && game.IsAliveEntity (last_enemy_)) {

    heard_enemy_ = last_enemy_;
    last_enemy_origin_ = last_enemy_->v.origin;

    // still register the contact, otherwise the heard state is silently consumed
    heard_sound_timer_.start ();
    states_ |= Sense::HearingEnemy;

    return;
  }

  // setup potential visibility set from engine
  auto set = game.GetVisibilitySet (this, false);

  // loop through all enemy clients to check for hearable stuff
  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.IsSameTeam (team_) || client.noise.last < game.Time ()) {
      continue;
    }

    const float distance_sq = client.noise.pos.distance_sq (pev->origin);
    const float current_range = client.noise.CurrentRange ();

    if (distance_sq > ystl::sqrf (current_range)) {
      continue;
    }

    // ignore invincible, no-target and not potentially visible players
    if (IsEnemyInvincible (client.ent) || IsEnemyNoTarget (client.ent) || !game.CheckVisibility (client.ent, set)) {
      continue;
    }

    // preference scoring: lower is better
    float score = distance_sq;

    if (has_flag (client.noise.type, Noise::WeaponFire)) {
      score *= 0.8f; // weapon fire is most attention-grabbing
    }
    else if (has_flag (client.noise.type, Noise::Defuse)) {
      score *= 0.5f; // defuse is critically important
    }
    else if (has_flag (client.noise.type, Noise::Explosion)) {
      score *= 0.7f;
    }
    else if (has_flag (client.noise.type, Noise::Ricochet)) {
      score *= 0.9f; // bullet impacts - combat indicator
    }

    if (score < best_score) {
      heard_enemy_ = client.ent;
      nearest_distance_sq = distance_sq;
      best_score = score;
      best_sound_type = client.noise.type;

      // store in sound memory ring buffer
      sound_memory_[sound_memory_head_] = { client.noise.pos, game.Time (), client.noise.type, client.ent };
      sound_memory_head_ = (sound_memory_head_ + 1) % kSoundMemorySize;
    }
  }

  // did the bot hear someone ?
  if (game.IsPlayerEntity (heard_enemy_)) {
    // change to best weapon for high-priority sounds (weapon fire, explosions, defuse)
    const bool high_priority_sound =
      has_flag (best_sound_type, Noise::WeaponFire) || has_flag (best_sound_type, Noise::Defuse) || has_flag (best_sound_type, Noise::Explosion);

    // close movement noise means enemy is near, draw the weapon at once
    const bool close_movement_noise = nearest_distance_sq < ystl::sqrf (650.0f) && !has_flag (best_sound_type, Noise::Zoom);

    if (high_priority_sound || close_movement_noise) {
      if (IsOnFloor () && current_weapon_ != Weapon::C4 && current_weapon_ != Weapon::Explosive && current_weapon_ != Weapon::Smoke &&
          current_weapon_ != Weapon::Flashbang && !IsKnifeMode ()) {

        SelectBestWeapon ();
      }
    }

    heard_sound_timer_.start ();
    states_ |= Sense::HearingEnemy;

    if (rg.chance (10) && game.IsNullEntity (enemy_) && game.IsNullEntity (last_enemy_) && see_enemy_timer_.greater_than (7.0f)) {
      PushRadioChat (RadioChat::HeardTheEnemy);
    }

    // positional error: grows with distance, farther sounds are vaguer
    auto get_heard_origin_with_error = [&] () -> ystl::Vector {
      const float error = ystl::min (ystl::sqrtf (nearest_distance_sq) * 0.06f, 180.0f);
      auto origin = heard_enemy_->v.origin;

      origin.x += rg (-error, error);
      origin.y += rg (-error, error);

      return origin;
    };

    // didn't bot already have an enemy ? take this one
    if (last_enemy_origin_.empty () || game.IsNullEntity (last_enemy_)) {
      last_enemy_ = heard_enemy_;
      last_enemy_origin_ = get_heard_origin_with_error ();
    }

    // bot had an enemy, check if it's the heard one
    else {
      if (heard_enemy_ == last_enemy_) {
        // bot sees enemy ? then bail out !
        if (has_flag (states_, Sense::SeeingEnemy)) {
          return;
        }
        last_enemy_origin_ = get_heard_origin_with_error ();
      }
      else if (heard_enemy_ != nullptr) {
        // if bot had an enemy but the heard one is nearer, take it instead
        const float distance_sq = last_enemy_origin_.distance_sq (pev->origin);

        if (distance_sq > heard_enemy_->v.origin.distance_sq (pev->origin) && see_enemy_timer_.greater_than (1.0f)) {
          last_enemy_ = heard_enemy_;
          last_enemy_origin_ = get_heard_origin_with_error ();
        }
        else {
          return;
        }
      }
    }

    // check if heard enemy can be seen
    if (CheckBodyPartsWithOffsets (heard_enemy_)) {
      enemy_ = heard_enemy_;
      last_enemy_ = heard_enemy_;
      last_enemy_origin_ = enemy_origin_;

      states_ |= Sense::SeeingEnemy;
      see_enemy_timer_.start ();
    }

    // check if heard enemy can be shoot through some obstacle
    else {
      if (cv_shoots_thru_walls && last_enemy_ == heard_enemy_ && thru_wall_hold_timer_.elapsed () && thru_wall_reroll_timer_.elapsed () &&
          see_enemy_timer_.less_than (3.0f) && IsPenetrableObstacleCached (heard_enemy_->v.origin)) {

        // roll at most once per backoff interval, so the chance is per-attempt, not per-tick
        thru_wall_reroll_timer_.start (rg (0.75f, 1.5f));

        if (GetThruWallChance (difficulty_data_->hear_thru_pct)) {
          DebugMsg ("thru-wall engage (heard): %s.", heard_enemy_->v.netname.str ());

          thru_wall_hold_timer_.start (rg (1.0f, 1.5f));

          states_ |= Sense::SuspectEnemy;
          aim_flags_ |= AimFlags::LastEnemy;
        }
        else {
          DebugMsg (
            "thru-wall roll failed (heard): %s, retry in %.1fs.", heard_enemy_->v.netname.str (), thru_wall_reroll_timer_.remaining_time ());
        }
      }
    }
  }
}

void Bot::SelectCampButtons (int index) {
  const auto &path = graph[index];

  if (personality_ == Personality::Rusher || pev->health >= 90.0f) {
    if (path.vis.crouch < path.vis.stand && fear_level_ > agression_level_) {
      camp_buttons_ |= IN_DUCK;
    }
    else {
      camp_buttons_ &= ~IN_DUCK;
    }
  }
  else {
    if (path.vis.crouch <= path.vis.stand) {
      camp_buttons_ |= IN_DUCK;
    }
    else {
      camp_buttons_ &= ~IN_DUCK;
    }
  }
}

bool Bot::IsBombDefusing (const ystl::Vector &bomb_origin) const {
  // this function finds if somebody currently defusing the bomb

  constexpr auto kDistanceToBomb = ystl::sqrf (165.0f);
  constexpr auto kApproachDistance = ystl::sqrf (512.0f); // detect approaching bots

  if (!game_state.IsBombPlanted ()) {
    return false;
  }

  for (const auto &client : clients) {
    if (!client.IsUsedAndAlive () || client.ent == Ent ()) {
      continue;
    }

    auto bot = bots[client.ent];
    const auto bomb_distance_sq = client.origin.distance_sq (bomb_origin);

    if (bot && bot->is_alive_ && bot->team_ == team_) {
      const auto task_id = bot->GetTaskId ();

      // already defusing at bomb
      if (bomb_distance_sq < kDistanceToBomb && (task_id == TaskId::DefuseBomb || bot->has_progress_bar_)) {
        return true;
      }

      // approaching to defuse
      if (bomb_distance_sq < kApproachDistance && task_id == TaskId::DefuseBomb) {
        return true;
      }

      // has plantedc4 pickup task
      if (bomb_distance_sq < kApproachDistance && bot->pickup_type_ == Pickup::PlantedC4) {
        return true;
      }

      // has goal near bomb
      if (bomb_distance_sq < kApproachDistance && task_id == TaskId::MoveTo && bot->Task ()->data != kInvalidNodeIndex) {

        const auto &goal_origin = graph[bot->Task ()->data].origin;
        if (goal_origin.distance_sq (bomb_origin) < kApproachDistance) {
          return true;
        }
      }
      continue;
    }

    // check human teammates
    if (!bot && client.team == team_) {
      if (bomb_distance_sq < kDistanceToBomb && ((client.ent->v.button | client.ent->v.oldbuttons) & IN_USE)) {
        return true;
      }
    }
  }
  return false;
}

float Bot::GetShiftSpeed () {
  if (GetTaskId () == TaskId::SeekCover || has_flag (aim_flags_, AimFlags::Enemy) || IsDucking () ||
      ((pev->button | pev->oldbuttons) & IN_DUCK) || has_flag (current_travel_flags_, PathFlag::Jump) ||
      has_flag (path_flags_, NodeFlag::Ladder) || IsOnLadder () || IsInWater () || IsKnifeMode () || IsStuckState () || num_enemies_left_ <= 0 ||
      !lost_reachable_node_timer_.elapsed ()) {

    return pev->maxspeed;
  }
  return pev->maxspeed * 0.4f;
}

void Bot::RefreshCreatureStatus (char *infobuffer) {
  // if bot is on infected team, assume he is a creature
  if (game.Is (GameFlags::ZombieMod)) {
    static ystl::StringRef zm_infected_team (conf.FetchCustom ("ZMInfectedTeam"));

    // by default infected team is terrorists
    Team infected_team = Team::Terrorist;

    if (zm_infected_team == "CT") {
      infected_team = Team::CT;
    }

    // if bot is on infected team, and zombie mode is active, assume bot is a creature/zombie
    is_on_infected_team_ = game.GetRealPlayerTeam (Ent ()) == infected_team;
    infected_enemy_team_ = game.GetRealPlayerTeam (enemy_) == infected_team;

    // do not process next if already infected
    if (is_on_infected_team_ || infected_enemy_team_) {
      return;
    }
  }

  if (infobuffer == nullptr) {
    infobuffer = engfuncs.pfnGetInfoKeyBuffer (Ent ());
  }
  ystl::StringRef model_name = engfuncs.pfnInfoKeyValue (infobuffer, "model");

  // need at least four characters to test model mask
  if (model_name.size () < 4) {
    model_mask_ = 0;
    return;
  }
  union ModelTest {
    char model[4];
    uint32_t mask;
    ModelTest (ystl::StringRef m) : model { m[0], m[1], m[2], m[3] } {}
  } model_test { model_name };

  // assign our model mask (tests against model done every bot update)
  model_mask_ = model_test.mask;
}

bool Bot::IsCreature () const {
  // current creature models are: zombie, chicken
  constexpr auto kModelMaskZombie = (('b' << 24) + ('m' << 16) + ('o' << 8) + 'z');
  constexpr auto kModelMaskChicken = (('c' << 24) + ('i' << 16) + ('h' << 8) + 'c');

  return is_on_infected_team_ || model_mask_ == kModelMaskZombie || model_mask_ == kModelMaskChicken;
}

void Bot::DonateC4ToHuman () {
  edict_t *recipient = nullptr;

  if (!has_c4_) {
    return;
  }
  const float radius_sq = ystl::sqrf (1024.0f);

  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }

    // skip the bots
    if (bots[client.ent]) {
      continue;
    }

    if (client.IsInRadius (pev->origin, radius_sq)) {
      recipient = client.ent;
      break;
    }
  }

  if (game.IsNullEntity (recipient)) {
    return;
  }
  item_check_timer_.start (1.0f);

  // select the bomb
  if (current_weapon_ != Weapon::C4) {
    SelectWeaponById (Weapon::C4);
  }
  DropCurrentWeapon ();

  // bomb on the ground entity
  edict_t *bomb = nullptr;

  // search world for just dropped bomb
  game.SearchEntities ("classname", "weaponbox", [&] (edict_t *ent) {
    if (game.IsEntityModelMatches (ent, "backpack.mdl")) {
      bomb = ent;

      if (!game.IsNullEntity (bomb)) {
        return EntitySearchResult::Break;
      }
    }
    return EntitySearchResult::Continue;
  });

  // got c4 backpack
  if (!game.IsNullEntity (bomb)) {
    bomb->v.flags |= FL_ONGROUND;

    // make recipient friend "pickup" it
    MDLL_Touch (bomb, recipient);
  }
}

void Bot::DecideFollowUser () {
  // this function forces bot to follow user
  ystl::Array<edict_t *> users {};

  // search friends near us
  for (const auto &client : clients) {
    if (!client.IsTeammate (team_, Ent ())) {
      continue;
    }

    if (SeesEntity (client.origin) && client.IsHuman ()) {
      users.push (client.ent);
    }
  }

  if (users.empty ()) {
    return;
  }
  target_entity_ = users.random ();

  PushRadioChat (RadioChat::LeadOnSir);
  StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
}

} // namespace bot
