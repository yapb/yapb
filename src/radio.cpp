//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void Bot::ShowChatterIcon (bool show, bool disconnect) const {
  // this function depending on show boolean, shows/remove chatter, icon, on the head of bot

  if (!game.Is (GameFlags::HasBotVoice) || cv_radio_mode.As<int> () != 2) {
    return;
  }

  auto send_bot_voice = [] (bool on, edict_t *ent, int own_id) {
    MessageWriter (MSG_ONE, msgs.Id (NetMsg::BotVoice), nullptr, ent) // begin message
      .WriteByte (on) // switch on/off
      .WriteByte (own_id);
  };
  const int own_index = Index ();

  // do not respect timers while disconnecting bot
  for (auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }

    // dormants not receiving messages
    if (client.ent->v.flags & FL_DORMANT) {
      continue;
    }

    // hide from all viewers but show only to teammates and spectators
    bool should_process = false;

    if (!show) {
      // hide icon from anyone who has it visible
      should_process = (client.icon[own_index].flags & ClientIcon::Visible) != 0;
    }
    else {
      // check if client is teammate or spectating someone on bot's team
      const bool is_teammate = client.team == team_;
      bool is_spectating_team = false;

      // check if spectator is watching someone on bot's team
      if (client.ent->v.iuser2 != 0) {
        edict_t *spectated = game.EntityOfIndex (client.ent->v.iuser2);

        if (!game.IsNullEntity (spectated)) {
          is_spectating_team = game.GetPlayerTeam (spectated) == team_;
        }
      }
      should_process = is_teammate || is_spectating_team || disconnect;
    }

    if (!should_process) {
      continue;
    }

    // do not respect timers while disconnecting bot
    if (!show && (disconnect || client.icon[own_index].timestamp < game.Time ())) {
      send_bot_voice (false, client.ent, Entindex ());

      client.icon[own_index].timestamp = 0.0f;
      client.icon[own_index].flags &= ~ClientIcon::Visible;
    }
    else if (show && !has_flag (client.icon[own_index].flags, ClientIcon::Visible)) {
      send_bot_voice (true, client.ent, Entindex ());
    }
  }
}

void Bot::InstantChatter (RadioChat type) const {
  // this function sends instant chatter messages
  if (!game.Is (GameFlags::HasBotVoice) || cv_radio_mode.As<int> () != 2 || !conf.HasChatterBank (type) ||
      !conf.HasChatterBank (RadioChat::DiePain)) {

    return;
  }

  const auto &playback_sound = conf.PickRandomFromChatterBank (type);
  const auto &pain_sound = conf.PickRandomFromChatterBank (RadioChat::DiePain);

  if (is_alive_) {
    ShowChatterIcon (true);
  }
  MessageWriter msg {};
  const int own_index = Index ();

  auto write_chatter_sound = [&msg] (ChatterItem item) {
    msg.WriteString (ystl::strings.format ("%s%s%s.wav", cv_chatter_path.As<ystl::StringRef> (), kPathSeparator, item.name));
  };

  for (auto &client : clients) {
    if (!client.IsUsed () || client.IsBot ()) {
      continue;
    }

    // check if client is teammate or spectating someone on bot's team
    const bool is_teammate = client.team == team_;
    bool is_spectating_team = false;

    // check if spectator is watching someone on bot's team
    if (client.ent->v.iuser2 != 0) {
      edict_t *spectated = game.EntityOfIndex (client.ent->v.iuser2);

      if (!game.IsNullEntity (spectated)) {
        is_spectating_team = game.GetPlayerTeam (spectated) == team_;
      }
    }

    if (!is_teammate && !is_spectating_team) {
      continue;
    }
    msg.Start (MSG_ONE, msgs.Id (NetMsg::SendAudio), nullptr, client.ent); // begin message
    msg.WriteByte (own_index);

    if (pev->deadflag == DEAD_DYING) {
      client.icon[own_index].timestamp = game.Time () + pain_sound.duration;
      write_chatter_sound (pain_sound);
    }
    else if (is_alive_) {
      client.icon[own_index].timestamp = game.Time () + playback_sound.duration;
      write_chatter_sound (playback_sound);
    }
    msg.WriteShort (voice_pitch_).end ();
    client.icon[own_index].flags |= ClientIcon::Visible;
  }
}

void Bot::CheckRadioQueue () {
  // this function handling radio and reacting to it

  // don't allow bot listen you if bot is busy
  if (radio_order_ != RadioChat::ReportInTeam &&
      (GetTaskId () == TaskId::DefuseBomb || GetTaskId () == TaskId::PlantBomb || has_hostage_ || has_c4_ || is_creature_)) {

    radio_order_ = RadioChat::InvalidSelect;
    return;
  }
  float distance_sq = radio_entity_->v.origin.distance_sq (pev->origin);

  switch (radio_order_) {
  case RadioChat::CoverMe:
  case RadioChat::FollowMe:
  case RadioChat::StickTogetherTeam:
  case RadioChat::GoingToPlantBomb:
    // check if line of sight to object is not blocked (i.e. visible)
    if (SeesEntity (radio_entity_->v.origin) || radio_order_ == RadioChat::StickTogetherTeam) {
      if (game.IsNullEntity (target_entity_) && game.IsNullEntity (enemy_) && rg.chance (radio_percent_)) {

        int num_followers = 0;

        // check if no more followers are allowed
        for (const auto &bot : bots) {
          if (bot.is_alive_) {
            if (bot.target_entity_ == radio_entity_) {
              ++num_followers;
            }
          }
        }
        int allowed_followers = cv_user_max_followers.As<int> ();

        if (has_flag (radio_entity_->v.weapons, ystl::bit (Weapon::C4))) {
          allowed_followers = 1;
        }

        if (num_followers < allowed_followers) {
          PushRadioChat (RadioChat::RogerThat);
          target_entity_ = radio_entity_;

          // don't pause/camp/follow anymore
          const auto tid = GetTaskId ();

          if (tid == TaskId::Pause || tid == TaskId::Camp) {
            Task ()->time = game.Time ();
          }
          StartTask (TaskId::FollowUser, TaskPri::kFollowUser, kInvalidNodeIndex, 0.0f, true);
        }
        else if (num_followers > allowed_followers) {
          for (int i = 0; (i < game.MaxClients () && num_followers > allowed_followers); ++i) {
            auto bot = bots[i];

            if (bot != nullptr) {
              if (bot->is_alive_) {
                if (bot->target_entity_ == radio_entity_) {
                  bot->target_entity_ = nullptr;
                  num_followers--;
                }
              }
            }
          }
        }
        else if (radio_order_ != RadioChat::GoingToPlantBomb && rg.chance (radio_percent_)) {
          PushRadioChat (RadioChat::Negative);
        }
      }
      else if (radio_order_ != RadioChat::GoingToPlantBomb && rg.chance (radio_percent_)) {
        PushRadioChat (RadioChat::Negative);
      }
    }
    break;

  case RadioChat::HoldThisPosition:
    if (!game.IsNullEntity (target_entity_)) {
      if (target_entity_ == radio_entity_) {
        target_entity_ = nullptr;
        PushRadioChat (RadioChat::RogerThat);

        camp_buttons_ = 0;
        StartTask (TaskId::Pause, TaskPri::kPause, kInvalidNodeIndex, game.Time () + rg (30.0f, 60.0f), false);
      }
    }
    break;

  case RadioChat::NewRound:
    PushRadioChat (RadioChat::YouHeardTheMan);
    break;

  case RadioChat::TakingFireNeedAssistance:
    if (game.IsNullEntity (target_entity_)) {
      if (game.IsNullEntity (enemy_) && see_enemy_timer_.greater_than (4.0f)) {
        // decrease fear levels to lower probability of bot seeking cover again
        fear_level_ -= 0.2f;

        if (fear_level_ < 0.0f) {
          fear_level_ = 0.0f;
        }

        if (rg.chance (radio_percent_) && cv_radio_mode.As<int> () == 2) {
          PushRadioChat (RadioChat::OnMyWay);
        }
        else if (radio_order_ == RadioChat::NeedBackup && cv_radio_mode.As<int> () != 2) {
          PushRadioChat (RadioChat::RogerThat);
        }
        TryHeadTowardRadioMessage ();
      }
      else if (rg.chance (radio_percent_)) {
        PushRadioChat (RadioChat::Negative);
      }
    }
    break;

  case RadioChat::YouTakeThePoint:
    if (SeesEntity (radio_entity_->v.origin) && is_leader_) {
      PushRadioChat (RadioChat::RogerThat);
    }
    break;

  case RadioChat::EnemySpotted:
  case RadioChat::NeedBackup:
  case RadioChat::SpottedOneEnemy:
  case RadioChat::SpottedTwoEnemies:
  case RadioChat::SpottedThreeEnemies:
  case RadioChat::TooManyEnemies:
  case RadioChat::ScaredEmotion:
  case RadioChat::PinnedDown:
    if (((game.IsNullEntity (enemy_) && SeesEntity (radio_entity_->v.origin)) || distance_sq < ystl::sqrf (2048.0f) || !move_to_c4_) &&
        rg.chance (radio_percent_) && see_enemy_timer_.greater_than (4.0f)) {

      fear_level_ -= 0.1f;

      if (fear_level_ < 0.0f) {
        fear_level_ = 0.0f;
      }

      if (rg.chance (radio_percent_) && cv_radio_mode.As<int> () == 2) {
        PushRadioChat (RadioChat::OnMyWay);
      }
      else if (radio_order_ == RadioChat::NeedBackup && cv_radio_mode.As<int> () != 2 && rg.chance (radio_percent_)) {
        PushRadioChat (RadioChat::RogerThat);
      }
      TryHeadTowardRadioMessage ();
    }
    else if (rg.chance (radio_percent_) && radio_order_ == RadioChat::NeedBackup) {
      PushRadioChat (RadioChat::Negative);
    }
    break;

  case RadioChat::GoGoGo:
    if (radio_entity_ == target_entity_) {
      if (rg.chance (radio_percent_) && cv_radio_mode.As<int> () == 2) {
        PushRadioChat (RadioChat::RogerThat);
      }
      else if (radio_order_ == RadioChat::NeedBackup && cv_radio_mode.As<int> () != 2) {
        PushRadioChat (RadioChat::RogerThat);
      }

      target_entity_ = nullptr;
      fear_level_ -= 0.2f;

      if (fear_level_ < 0.0f) {
        fear_level_ = 0.0f;
      }
    }
    else if ((game.IsNullEntity (enemy_) && SeesEntity (radio_entity_->v.origin)) || distance_sq < ystl::sqrf (2048.0f)) {
      const auto tid = GetTaskId ();

      if (tid == TaskId::Pause || tid == TaskId::Camp) {
        fear_level_ -= 0.2f;

        if (fear_level_ < 0.0f) {
          fear_level_ = 0.0f;
        }

        PushRadioChat (RadioChat::RogerThat);
        // don't pause/camp anymore
        Task ()->time = game.Time ();

        target_entity_ = nullptr;
        position_ = radio_entity_->v.origin + radio_entity_->v.v_angle.forward () * rg (1024.0f, 2048.0f);

        ClearSearchNodes ();
        StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
      }
    }
    else if (!game.IsNullEntity (double_jump_entity_)) {
      PushRadioChat (RadioChat::RogerThat);
      ResetDoubleJump ();
    }
    else if (rg.chance (radio_percent_)) {
      PushRadioChat (RadioChat::Negative);
    }
    break;

  case RadioChat::ShesGonnaBlow:
    if (game.IsNullEntity (enemy_) && distance_sq < ystl::sqrf (2048.0f) && game_state.IsBombPlanted () && team_ == Team::Terrorist) {
      PushRadioChat (RadioChat::RogerThat);

      if (GetTaskId () == TaskId::Camp) {
        ClearTask (TaskId::Camp);
      }
      target_entity_ = nullptr;
      StartTask (TaskId::EscapeFromBomb, TaskPri::kEscapeFromBomb, kInvalidNodeIndex, 0.0f, true);
    }
    else if (rg.chance (radio_percent_)) {
      PushRadioChat (RadioChat::Negative);
    }
    break;

  case RadioChat::RegroupTeam:
    // if no more enemies found and bomb planted, switch to knife to get to bombplace faster
    if (team_ == Team::CT && !UsesKnife () && num_enemies_left_ == 0 && game_state.IsBombPlanted () && GetTaskId () != TaskId::DefuseBomb) {
      SelectWeaponById (Weapon::Knife);
      ClearSearchNodes ();

      position_ = game_state.GetBombOrigin ();
      StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);

      PushRadioChat (RadioChat::RogerThat);
    }
    break;

  case RadioChat::StormTheFront:
    if (((game.IsNullEntity (enemy_) && SeesEntity (radio_entity_->v.origin)) || distance_sq < ystl::sqrf (1024.0f)) &&
        rg.chance (radio_percent_)) {
      PushRadioChat (RadioChat::RogerThat);

      // don't pause/camp anymore
      const auto tid = GetTaskId ();

      if (tid == TaskId::Pause || tid == TaskId::Camp) {
        Task ()->time = game.Time ();
      }
      target_entity_ = nullptr;
      position_ = radio_entity_->v.origin + radio_entity_->v.v_angle.forward () * rg (1024.0f, 2048.0f);

      ClearSearchNodes ();
      StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);

      fear_level_ -= 0.3f;

      if (fear_level_ < 0.0f) {
        fear_level_ = 0.0f;
      }
      agression_level_ += 0.3f;

      if (agression_level_ > 1.0f) {
        agression_level_ = 1.0f;
      }
    }
    break;

  case RadioChat::TeamFallback:
    if ((game.IsNullEntity (enemy_) && SeesEntity (radio_entity_->v.origin)) || distance_sq < ystl::sqrf (1024.0f)) {
      fear_level_ += 0.5f;

      if (fear_level_ > 1.0f) {
        fear_level_ = 1.0f;
      }
      agression_level_ -= 0.5f;

      if (agression_level_ < 0.0f) {
        agression_level_ = 0.0f;
      }
      if (GetTaskId () == TaskId::Camp) {
        Task ()->time += rg (10.0f, 15.0f);
      }
      else {
        // don't pause/camp anymore
        const auto tid = GetTaskId ();

        if (tid == TaskId::Pause) {
          Task ()->time = game.Time ();
        }
        target_entity_ = nullptr;
        see_enemy_timer_.start ();

        // if bot has no enemy
        if (last_enemy_origin_.empty ()) {
          float nearest_distance_sq = kInfiniteDistance;

          // take nearest enemy to ordering player
          for (const auto &client : clients) {
            if (!client.IsUsedAndAlive () || client.IsSameTeam (team_)) {
              continue;
            }

            auto enemy = client.ent;
            const float current_distance_sq = radio_entity_->v.origin.distance_sq (enemy->v.origin);

            if (current_distance_sq < nearest_distance_sq) {
              nearest_distance_sq = current_distance_sq;

              last_enemy_ = enemy;
              last_enemy_origin_ = enemy->v.origin;
            }
          }
        }
        ClearSearchNodes ();
      }
    }
    break;

  case RadioChat::ReportInTeam:
    switch (GetTaskId ()) {
    case TaskId::Normal:
      if (Task ()->data != kInvalidNodeIndex && rg.chance (radio_percent_)) {
        const auto &path = graph[Task ()->data];

        if (has_flag (path.flags, NodeFlag::Goal)) {
          if (has_c4_) {
            PushRadioChat (RadioChat::GoingToPlantBomb);
          }
          else {
            PushRadioChat (RadioChat::Nothing);
          }
        }
        else if (has_hostage_) {
          PushRadioChat (RadioChat::RescuingHostages);
        }
        else if (has_flag (path.flags, NodeFlag::Camp) && rg.chance (radio_percent_)) {
          PushRadioChat (RadioChat::GoingToCamp);
        }
        else if (has_flag (states_, Sense::HearingEnemy)) {
          PushRadioChat (RadioChat::HeardTheEnemy);
        }
      }
      else if (rg.chance (radio_percent_)) {
        PushRadioChat (RadioChat::ReportingIn);
      }
      break;

    case TaskId::MoveTo:
      if (rg.chance (2)) {
        PushRadioChat (RadioChat::GoingToCamp);
      }
      break;

    case TaskId::Camp:
      if (rg.chance (radio_percent_)) {
        if (game_state.IsBombPlanted () && team_ == Team::Terrorist) {
          PushRadioChat (RadioChat::GuardingPlantedC4);
        }
        else if (in_escape_zone_ && team_ == Team::CT) {
          PushRadioChat (RadioChat::GuardingEscapeZone);
        }
        else if (in_vip_zone_ && team_ == Team::Terrorist) {
          PushRadioChat (RadioChat::GuardingVIPSafety);
        }
        else {
          PushRadioChat (RadioChat::Camping);
        }
      }
      break;

    case TaskId::PlantBomb:
      PushRadioChat (RadioChat::PlantingBomb);
      break;

    case TaskId::DefuseBomb:
      PushRadioChat (RadioChat::DefusingBomb);
      break;

    case TaskId::Attack:
      if (rg.chance (50)) {
        PushRadioChat (RadioChat::InCombat);
      }
      else {
        if (cv_radio_mode.As<int> () == 2) {
          switch (NumEnemiesNear (pev->origin, 384.0f)) {
          case 1:
            PushRadioChat (RadioChat::SpottedOneEnemy);
            break;
          case 2:
            PushRadioChat (RadioChat::SpottedTwoEnemies);
            break;
          case 3:
            PushRadioChat (RadioChat::SpottedThreeEnemies);
            break;
          default:
            PushRadioChat (RadioChat::TooManyEnemies);
            break;
          }
        }
        else if (cv_radio_mode.As<int> () == 1) {
          PushRadioChat (RadioChat::EnemySpotted);
        }
      }
      break;

    case TaskId::Hide:
    case TaskId::SeekCover:
      PushRadioChat (RadioChat::SeekingEnemies);
      break;

    default:
      if (rg.chance (radio_percent_)) {
        PushRadioChat (RadioChat::Nothing);
      }
      break;
    }
    break;

  case RadioChat::SectorClear:
    // is bomb planted and it's a ct
    if (!game_state.IsBombPlanted ()) {
      break;
    }

    // check if it's a ct command
    if (game.GetPlayerTeam (radio_entity_) == Team::CT && team_ == Team::CT && game.IsFakeClientEntity (radio_entity_) &&
        !bots.HasPlantedBombSearchCooldown ()) {

      float nearest_distance_sq = kInfiniteDistance;
      int bomb_point = kInvalidNodeIndex;

      // find nearest bomb node to player
      for (const auto &point : graph.GetPoints (PointType::Goal)) {
        distance_sq = graph[point].origin.distance_sq (radio_entity_->v.origin);

        if (distance_sq < nearest_distance_sq) {
          nearest_distance_sq = distance_sq;
          bomb_point = point;
        }
      }

      // mark this node as restricted point
      if (bomb_point != kInvalidNodeIndex && !graph.IsVisited (bomb_point)) {
        // does this bot want to defuse?
        if (GetTaskId () == TaskId::Normal && graph.Exists (Task ()->data)) {
          // is he approaching the reported site (not just the exact node)?
          if (Task ()->data == bomb_point ||
              graph[Task ()->data].origin.distance_sq (graph[bomb_point].origin) < ystl::sqrf (kBombSiteGoalRadius)) {
            Task ()->data = kInvalidNodeIndex;
            PushRadioChat (RadioChat::RogerThat);
          }
        }
        MarkBombSiteVisited (bomb_point);
      }
      bots.SetPlantedBombSearchCooldown (0.5f);
    }
    break;

  case RadioChat::GetInPositionAndWaitForGo:
    if (!is_creature_ && ((game.IsNullEntity (enemy_) && SeesEntity (radio_entity_->v.origin)) || distance_sq < ystl::sqrf (1024.0f))) {
      PushRadioChat (RadioChat::RogerThat);

      if (GetTaskId () == TaskId::Camp) {
        Task ()->time = game.Time () + rg (30.0f, 60.0f);
      }
      else {
        // don't pause anymore
        const auto tid = GetTaskId ();

        if (tid == TaskId::Pause) {
          Task ()->time = game.Time ();
        }

        target_entity_ = nullptr;
        see_enemy_timer_.start ();

        // if bot has no enemy
        if (last_enemy_origin_.empty ()) {
          float nearest_distance_sq = kInfiniteDistance;

          // take nearest enemy to ordering player
          for (const auto &client : clients) {
            if (!client.IsUsedAndAlive () || client.IsSameTeam (team_)) {
              continue;
            }

            auto enemy = client.ent;
            const float enemy_distance_sq = radio_entity_->v.origin.distance_sq (enemy->v.origin);

            if (enemy_distance_sq < nearest_distance_sq) {
              nearest_distance_sq = enemy_distance_sq;

              last_enemy_ = enemy;
              last_enemy_origin_ = enemy->v.origin;
            }
          }
        }
        ClearSearchNodes ();

        const int index = FindDefendNode (radio_entity_->v.origin);

        // add/update camp task
        StartTask (TaskId::Camp, TaskPri::kCamp, kInvalidNodeIndex, game.Time () + rg (30.0f, 60.0f), true);

        // add/update move task
        StartTask (TaskId::MoveTo, TaskPri::kMoveTo, index, game.Time () + rg (30.0f, 60.0f), true);

        // decide to duck or not to duck
        SelectCampButtons (index);
      }
    }
    break;

  default:
    break;
  }
  radio_order_ = RadioChat::InvalidSelect; // radio command has been handled, reset
}

void Bot::HandleChatterTaskChange (TaskId tid) {
  if (rg.chance (90)) {
    if (tid == TaskId::Blind) {
      PushRadioChat (RadioChat::Blind);
    }
    else if (tid == TaskId::PlantBomb) {
      PushRadioChat (RadioChat::PlantingBomb);
    }
  }

  if (rg.chance (25) && tid == TaskId::Camp) {
    if (game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted ()) {
      PushRadioChat (RadioChat::GuardingPlantedC4);
    }
    else if (rg.chance (radio_percent_)) {
      PushRadioChat (RadioChat::GoingToCamp);
    }
  }

  if (rg.chance (75) && tid == TaskId::Camp && team_ == Team::CT && in_escape_zone_) {
    PushRadioChat (RadioChat::GoingToGuardEscapeZone);
  }

  if (rg.chance (75) && tid == TaskId::Camp && team_ == Team::Terrorist && in_rescue_zone_) {
    PushRadioChat (RadioChat::GoingToGuardRescueZone);
  }

  if (rg.chance (75) && tid == TaskId::Camp && team_ == Team::Terrorist && in_vip_zone_) {
    PushRadioChat (RadioChat::GoingToGuardVIPSafety);
  }
}

void Bot::HandleChatterOnPlayerKill (bool is_team_kill) {
  if (is_team_kill) {
    PushChatMessage (Chat::TeamKill, true);
    PushRadioChat (RadioChat::FriendlyFire);

    return;
  }
  radio_percent_ = ystl::clamp (radio_percent_ - rg (1, 5), 0, 15);

  if (rg.chance (10)) {
    PushChatMessage (Chat::Kill);
  }

  if (rg.chance (10)) {
    PushRadioChat (RadioChat::EnemyDown);
  }
  else if (rg.chance (80)) {
    if (!game.IsNullEntity (last_victim_) && has_flag (last_victim_->v.weapons, kSniperWeaponMask)) {
      PushRadioChat (RadioChat::SniperKilled);
    }
    else {
      switch (NumEnemiesNear (pev->origin, kInfiniteDistance)) {
      case 0:
        if (rg.chance (50)) {
          PushRadioChat (RadioChat::NoEnemiesLeft);
        }
        else {
          PushRadioChat (RadioChat::EnemyDown);
        }
        break;

      case 1:
        PushRadioChat (RadioChat::OneEnemyLeft);
        break;

      case 2:
        PushRadioChat (RadioChat::TwoEnemiesLeft);
        break;

      case 3:
        PushRadioChat (RadioChat::ThreeEnemiesLeft);
        break;

      default:
        PushRadioChat (RadioChat::EnemyDown);
      }
    }
  }
  else {
    kills_interval_ = last_victim_timer_.elapsed_time ();

    if (kills_interval_ <= 5.0f) {
      ++kills_count_;

      if (kills_count_ > 2) {
        PushRadioChat (RadioChat::OnARoll);
      }
    }
    else {
      kills_count_ = 0;
    }
  }

  // if no more enemies found and bomb planted, switch to knife to get to bomb place faster
  if (team_ == Team::CT && !UsesKnife () && num_enemies_left_ == 0 && game_state.IsBombPlanted ()) {
    SelectWeaponById (Weapon::Knife);
    planted_bomb_node_index_ = GetNearestToPlantedBomb ();

    if (IsOccupiedNode (planted_bomb_node_index_)) {
      PushRadioChat (RadioChat::BombsiteSecured);
    }
  }
}

void Bot::HandleChatterEnemyDown () {
  switch (NumEnemiesNear (pev->origin, 1024.0f)) {
  case 1:
    PushRadioChat (RadioChat::SpottedOneEnemy);
    break;

  case 2:
    PushRadioChat (RadioChat::SpottedTwoEnemies);
    break;

  case 3:
    PushRadioChat (RadioChat::SpottedThreeEnemies);
    break;

  default:
    PushRadioChat (RadioChat::TooManyEnemies);
    break;
  }
}

void Bot::ExecuteChatterFrameEvents () {
  if (cv_radio_mode.As<int> () != 2) {
    return;
  }

  // some stuff required by by chatter engine
  if (has_flag (states_, Sense::SeeingEnemy) && !game.IsNullEntity (enemy_)) {
    int has_friend_nearby = NumFriendsNear (pev->origin, 512.0f);

    if (!has_friend_nearby && rg.chance (45) && has_flag (enemy_->v.weapons, ystl::bit (Weapon::C4))) {
      PushRadioChat (RadioChat::SpotTheBomber);
    }
    else if (!has_friend_nearby && rg.chance (45) && team_ == Team::Terrorist && game.IsPlayerVip (enemy_)) {
      PushRadioChat (RadioChat::VIPSpotted);
    }
    else if (!has_friend_nearby && rg.chance (50) && game.GetPlayerTeam (enemy_) != team_ && IsGroupOfEnemies (enemy_->v.origin)) {
      PushRadioChat (RadioChat::ScaredEmotion);
    }
    else if (!has_friend_nearby && rg.chance (40) && has_flag (enemy_->v.weapons, kSniperWeaponMask)) {
      PushRadioChat (RadioChat::SniperWarning);
    }

    // if bot is trapped under shield yell for help !
    if (GetTaskId () == TaskId::Camp && HasShield () && IsShieldDrawn () && has_friend_nearby >= 2) {
      PushRadioChat (RadioChat::PinnedDown);
    }
  }

  // if bomb planted warn players !
  if (bots.HasBombSay (BombPlantedSay::Chatter) && game_state.IsBombPlanted () && team_ == Team::CT) {
    PushRadioChat (RadioChat::GottaFindC4);
    bots.ClearBombSay (BombPlantedSay::Chatter);
  }
}

void Bot::TryHeadTowardRadioMessage () {
  const auto tid = GetTaskId ();

  if (tid == TaskId::MoveTo || headed_timer_.greater_than (15.0f) || !game.IsAliveEntity (radio_entity_) || has_c4_) {
    return;
  }

  if ((game.IsFakeClientEntity (radio_entity_) && rg.chance (radio_percent_) && personality_ == Personality::Normal) ||
      !(radio_entity_->v.flags & FL_FAKECLIENT)) {

    if (tid == TaskId::Pause || tid == TaskId::Camp) {
      Task ()->time = game.Time ();
    }
    headed_timer_.start ();
    position_ = radio_entity_->v.origin;

    ClearSearchNodes ();
    StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  }
}

void Bot::PushRadioChat (RadioChat message) {
  // this function inserts the radio/voice message into the message queue

  // common early-exit conditions
  if (is_creature_ || num_friends_left_ == 0) {
    return;
  }

  // radio mode (cv_radio_mode != 2) or no bot voice support - use radio
  if (cv_radio_mode.As<int> () != 2 || !game.Is (GameFlags::HasBotVoice) || !conf.HasChatterBank (message)) {
    if (cv_radio_mode.As<int> () == 0) {
      return;
    }
    force_radio_ = true;
    radio_select_ = message;

    PushMsgQueue (Msg::Radio);
    return;
  }

  // voice/chatter mode (cv_radio_mode == 2) with bot voice support
  bool send_message = false;

  const auto message_repeat = conf.GetChatterMessageRepeatInterval (message);
  auto &message_timer = chatter_times_[message];

  if (message_timer < game.Time () || ystl::fequal (message_timer, kMaxChatterRepeatInterval)) {
    if (!ystl::fequal (message_repeat, kMaxChatterRepeatInterval)) {
      message_timer = game.Time () + message_repeat;
    }
    else {
      message_timer = game.Time () + 1.0f; // prevent spamming for messages with max repeat interval
    }
    send_message = true;
  }

  if (!send_message) {
    radio_select_ = RadioChat::Invalid;
    return;
  }
  radio_select_ = message;
  PushMsgQueue (Msg::Radio);
}

} // namespace bot
