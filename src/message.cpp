//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

void MessageDispatch::NetMsgTextMsg () {
  enum args {
    msg = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }

  // lookup cached message, skip unknown messages
  const auto cached_item = FindInCache (text_msg_cache_, args_[msg].chars_);

  if (!cached_item) {
    return;
  }
  const auto cached = *cached_item;

  // reset bomb position for all the bots
  const auto reset_bomb_position = [] () -> void {
    if (game.MapIs (MapFlags::Demolition)) {
      game_state.SetBombOrigin (true);
    }
  };

  if (has_flag (cached, TextMsgCache::Commencing)) {
    util.SetNeedForWelcome (true);
  }
  else if (has_flag (cached, TextMsgCache::CounterWin)) {
    bots.SetLastWinner (Team::CT); // update last winner for economics
    reset_bomb_position ();
  }
  else if (has_flag (cached, TextMsgCache::RestartRound)) {
    bots.UpdateTeamEconomics (Team::CT, true);
    bots.UpdateTeamEconomics (Team::Terrorist, true);

    // set balance for all players
    bots.ForEach ([] (Bot *bot) {
      bot->money_amount_ = mp_startmoney.As<int> ();
      return false;
    });

    reset_bomb_position ();
  }
  else if (has_flag (cached, TextMsgCache::TerroristWin)) {
    bots.SetLastWinner (Team::Terrorist); // update last winner for economics
    reset_bomb_position ();
  }
  else if (has_flag (cached, TextMsgCache::BombPlanted) && !game_state.IsBombPlanted ()) {
    game_state.SetBombPlanted (true);

    for (auto &notify : bots) {
      if (notify.is_alive_) {
        notify.ClearSearchNodes ();

        // clear only camp tasks
        notify.ClearTask (TaskId::Camp);

        if (cv_radio_mode.As<int> () == 2 && ystl::rg.chance (55) && notify.team_ == Team::CT) {
          notify.PushRadioChat (RadioChat::WhereIsTheC4);
        }
      }
    }
    game_state.SetBombOrigin ();
  }

  // check for burst fire message
  if (bot_) {
    if (has_flag (cached, TextMsgCache::BurstOn)) {
      bot_->weapon_burst_mode_ = BurstMode::On;
    }
    else if (has_flag (cached, TextMsgCache::BurstOff)) {
      bot_->weapon_burst_mode_ = BurstMode::Off;
    }
  }
}

void MessageDispatch::NetMsgVguiMenu () {
  // this message is sent when a vgui menu is displayed

  enum args {
    MenuId = 0,
    min = 1
  };

  // check the minimum states or existence of bot
  if (args_.size () < min || !bot_) {
    return;
  }
  const auto gui_menu = static_cast<GuiMenu> (args_[MenuId].long_);

  switch (gui_menu) {
  case GuiMenu::TeamSelect:
    bot_->start_action_ = Msg::TeamSelect;
    break;

  case GuiMenu::TerroristSelect:
  case GuiMenu::CTSelect:
    bot_->start_action_ = Msg::ClassSelect;
    break;
  }
}

void MessageDispatch::NetMsgShowMenu () {
  // this message is sent when a text menu is displayed

  enum args {
    MenuId = 3,
    min = 4
  };

  // check the minimum states or existence of bot
  if (args_.size () < min || !bot_) {
    return;
  }
  const auto cached_item = FindInCache (show_menu_cache_, args_[MenuId].chars_);

  if (!cached_item) {
    return;
  }
  bot_->start_action_ = *cached_item;
}

void MessageDispatch::NetMsgWeaponList () {
  // this message is sent when a client joins the game. all of the weapons are sent with the weapon id and information about what ammo is used

  enum args {
    classname = 0,
    ammo_index_1 = 1,
    max_ammo_1 = 2,
    slot = 5,
    slot_pos = 6,
    weapon_id = 7,
    flags = 8,
    min = 9
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }

  // validate weapon id before indexing
  if (args_[weapon_id].long_ < 0 || args_[weapon_id].long_ >= kMaxWeapons) {
    return;
  }

  // store away this weapon with it's ammo information
  auto &prop = conf.GetWeaponProp (static_cast<Weapon> (args_[weapon_id].long_));

  prop.id = static_cast<Weapon> (args_[weapon_id].long_);
  prop.classname = args_[classname].chars_;
  prop.ammo1 = args_[ammo_index_1].long_;
  prop.ammo1_max = args_[max_ammo_1].long_;
  prop.slot = args_[slot].long_;
  prop.pos = args_[slot_pos].long_;
  prop.flags = args_[flags].long_;
}

void MessageDispatch::NetMsgCurWeapon () {
  // this message is sent when a weapon is selected

  enum args {
    state = 0,
    weapon_id = 1,
    clip = 2,
    min = 3
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }

  if (args_[weapon_id].long_ >= 0 && args_[weapon_id].long_ < kMaxWeapons) {
    if (args_[state].long_ != 0) {
      const auto weapon = static_cast<Weapon> (args_[weapon_id].long_);

      bot_->current_weapon_ = weapon;
      bot_->weapon_type_ = conf.GetWeaponType (weapon);
    }

    // ammo amount decreased ? must have fired a bullet
    if (args_[weapon_id].long_ == bot_->current_weapon_ && bot_->ammo_in_clip_[args_[weapon_id].long_] > args_[clip].long_) {
      bot_->last_fired_timer_.start (); // remember the last bullet time
    }
    bot_->ammo_in_clip_[args_[weapon_id].long_] = args_[clip].long_;
  }
}

void MessageDispatch::NetMsgAmmoX () {
  // this message is sent whenever ammo amounts are adjusted (up or down). note: logging reveals that cs uses it very unreliable!

#if 1
  NetMsgAmmoPickup ();
#else
  enum args {
    arg_index = 0,
    value = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  bot_->ammo_[args_[arg_index].long_] = args_[value].long_; // store it away
#endif
}

void MessageDispatch::NetMsgAmmoPickup () {
  // this message is sent when the bot picks up some ammo

  enum args {
    arg_index = 0,
    value = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  const auto ammo_index = args_[arg_index].long_;

  if (ammo_index >= 0 && ammo_index < MAX_AMMO_SLOTS) {
    bot_->ammo_[ammo_index] = args_[value].long_; // store it away
  }
}

void MessageDispatch::NetMsgDamage () {
  // this message gets sent when the bots are getting damaged

  enum args {
    armor = 0,
    health = 1,
    bits = 2,
    min = 3
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }

  // handle damage if any
  if (args_[armor].long_ > 0 || args_[health].long_ > 0) {
    bot_->TakeDamage (bot_->pev->dmg_inflictor, args_[health].long_, args_[armor].long_, args_[bits].long_);
  }
}

void MessageDispatch::NetMsgMoney () {
  // this message gets sent when the bots money amount changes

  enum args {
    money = 0,
    min = 1
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  auto amount = args_[money].long_;

  if (amount < 0) {
    amount = 800;
  }
  else if (amount > mp_maxmoney.As<int> ()) {
    amount = mp_maxmoney.As<int> ();
  }
  bot_->money_amount_ = amount;
}

void MessageDispatch::NetMsgStatusIcon () {
  enum args {
    enabled = 0,
    icon = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }

  // lookup cached icon, skip unknown icons
  const auto cached_item = FindInCache (status_icon_cache_, args_[icon].chars_);

  if (!cached_item) {
    return;
  }
  const auto cached = *cached_item;

  // handle cases
  if (has_flag (cached, StatusIconCache::BuyZone)) {
    bot_->in_buy_zone_ = (args_[enabled].long_ != 0);

    // try to equip in buyzone
    bot_->EnteredBuyZone (BuyState::PrimaryWeapon);
  }
  else if (has_flag (cached, StatusIconCache::Escape)) {
    bot_->in_escape_zone_ = (args_[enabled].long_ != 0);
  }
  else if (has_flag (cached, StatusIconCache::Rescue)) {
    bot_->in_rescue_zone_ = (args_[enabled].long_ != 0);
  }
  else if (has_flag (cached, StatusIconCache::VipSafety)) {
    bot_->in_vip_zone_ = (args_[enabled].long_ != 0);
  }
  else if (has_flag (cached, StatusIconCache::C4)) {
    bot_->in_bomb_zone_ = (args_[enabled].long_ == 2);
  }
  else if (has_flag (cached, StatusIconCache::Defuser)) {
    bot_->has_defuser_ = (args_[enabled].long_ != 0);
  }
}

void MessageDispatch::NetMsgDeathMsg () {
  // this message gets sent when player kills player

  enum args {
    killer = 0,
    victim = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }

  auto killer_entity = game.EntityOfIndex (args_[killer].long_);
  auto victim_entity = game.EntityOfIndex (args_[victim].long_);

  if (game.IsNullEntity (killer_entity) || game.IsNullEntity (victim_entity) || victim_entity == killer_entity) {
    return;
  }
  bots.HandleDeath (killer_entity, victim_entity);
}

void MessageDispatch::NetMsgScreenFade () {
  // this message gets sent when the screen fades (flashbang)

  enum args {
    r = 3,
    g = 4,
    b = 5,
    alpha = 6,
    min = 7
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }

  // screen completely faded ? (flash color is typically white, allow near-white for mod compatibility)
  if (args_[r].long_ > 200 && args_[g].long_ > 200 && args_[b].long_ > 200 && args_[alpha].long_ > 180) {
    bot_->TakeBlind (args_[alpha].long_);
  }
}

void MessageDispatch::NetMsgHltv () {
  // this message gets sent when new round is started in modern cs versions

  enum args {
    players = 0,
    fov = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }

  // need to start new round ? (we're tracking fov reset message)
  if (args_[players].long_ == 0 && args_[fov].long_ == 0) {
    game_state.RoundStart ();
  }
}

void MessageDispatch::NetMsgTeamInfo () {
  // this message gets sent when player team index is changed

  enum args {
    arg_index = 0,
    team = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }
  const auto client_index = args_[arg_index].long_ - 1;

  if (client_index < 0 || client_index >= game.MaxClients ()) {
    return;
  }
  auto &client = clients[client_index];

  // update player team, skip unknown team strings
  const auto cached_item = FindInCache (team_info_cache_, args_[team].chars_);

  if (!cached_item) {
    return;
  }

  client.team2 = *cached_item; // update real team
  client.team = game.Is (GameFlags::FreeForAll) ? static_cast<Team> (args_[arg_index].long_) : client.team2;
}

void MessageDispatch::NetMsgScoreInfo () {
  // this message gets sent when scoreboard info is update, we're use it to track k-d ratio

  enum args {
    arg_index = 0,
    score = 1,
    deaths = 2,
    class_id = 3,
    team_id = 4,
    min = 5
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }
  auto bot = PickBot (arg_index);

  // if we're have bot, set the kd ratio
  if (bot != nullptr) {
    bot->kpd_ratio_ = bot->pev->frags / ystl::max (static_cast<float> (args_[deaths].long_), 1.0f);
    bot->death_count_ = args_[deaths].long_;
  }
}

void MessageDispatch::NetMsgScoreAttrib () {
  // this message updates the scoreboard attribute for the specified player

  enum args {
    arg_index = 0,
    flags = 1,
    min = 2
  };

  // check the minimum states
  if (args_.size () < min) {
    return;
  }
  auto bot = PickBot (arg_index);

  // if we're have bot, set the vip state
  if (bot != nullptr) {
    constexpr int32_t kPlayerIsVIP = ystl::bit (2);

    bot->is_vip_ = !!(args_[flags].long_ & kPlayerIsVIP);
  }
}

void MessageDispatch::NetMsgBarTime () {
  enum args {
    enabled = 0,
    min = 1
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }

  // check if has progress bar
  if (args_[enabled].long_ > 0) {
    bot_->has_progress_bar_ = true; // the progress bar on a hud

    // notify bots about defusing has started
    if (game.MapIs (MapFlags::Demolition) && game_state.IsBombPlanted () && bot_->team_ == Team::CT) {
      bots.NotifyBombDefuse ();
    }
  }
  else {
    bot_->has_progress_bar_ = false; // no progress bar or disappeared
  }
}

void MessageDispatch::NetMsgItemStatus () {
  enum args {
    value = 0,
    min = 1
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  const auto mask = args_[value].long_;

  bot_->has_nvg_ = has_flag (mask, ItemStatus::Nightvision);
  bot_->has_defuser_ = has_flag (mask, ItemStatus::DefusalKit);
}

void MessageDispatch::NetMsgNvgToggle () {
  enum args {
    value = 0,
    min = 1
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  bot_->uses_nvg_ = args_[value].long_ > 0;
}

void MessageDispatch::NetMsgFlashBat () {
  enum args {
    value = 0,
    min = 1
  };

  // check the minimum states
  if (args_.size () < min || !bot_) {
    return;
  }
  bot_->flash_level_ = args_[value].long_;
}

void MessageDispatch::NetMsgResetHud () {
  if (bot_) {
    bot_->Spawned ();
  }
  game_state.SetResetHud (true);
}

MessageDispatch::MessageDispatch () {
  Reset ();

  // initialize engine lookup array with none
  engine_to_net_msg_.fill (NetMsg::None);

  // keep registration dynamic, msvc x64 fails library load on a local static array
  auto register_entry = [&] (ystl::StringRef name, NetMsg id, MsgFunc handler) -> void {
    entries_[name] = { id, handler, MessageEntry::none };
  };

  // we want to handle next messages
  register_entry ("TextMsg", NetMsg::TextMsg, &MessageDispatch::NetMsgTextMsg);
  register_entry ("VGUIMenu", NetMsg::VGUIMenu, &MessageDispatch::NetMsgVguiMenu);
  register_entry ("ShowMenu", NetMsg::ShowMenu, &MessageDispatch::NetMsgShowMenu);
  register_entry ("WeaponList", NetMsg::WeaponList, &MessageDispatch::NetMsgWeaponList);
  register_entry ("CurWeapon", NetMsg::CurWeapon, &MessageDispatch::NetMsgCurWeapon);
  register_entry ("AmmoX", NetMsg::AmmoX, &MessageDispatch::NetMsgAmmoX);
  register_entry ("AmmoPickup", NetMsg::AmmoPickup, &MessageDispatch::NetMsgAmmoPickup);
  register_entry ("Damage", NetMsg::Damage, &MessageDispatch::NetMsgDamage);
  register_entry ("Money", NetMsg::Money, &MessageDispatch::NetMsgMoney);
  register_entry ("StatusIcon", NetMsg::StatusIcon, &MessageDispatch::NetMsgStatusIcon);
  register_entry ("DeathMsg", NetMsg::DeathMsg, &MessageDispatch::NetMsgDeathMsg);
  register_entry ("ScreenFade", NetMsg::ScreenFade, &MessageDispatch::NetMsgScreenFade);
  register_entry ("HLTV", NetMsg::HLTV, &MessageDispatch::NetMsgHltv);
  register_entry ("TeamInfo", NetMsg::TeamInfo, &MessageDispatch::NetMsgTeamInfo);
  register_entry ("BarTime", NetMsg::BarTime, &MessageDispatch::NetMsgBarTime);
  register_entry ("ItemStatus", NetMsg::ItemStatus, &MessageDispatch::NetMsgItemStatus);
  register_entry ("NVGToggle", NetMsg::NVGToggle, &MessageDispatch::NetMsgNvgToggle);
  register_entry ("FlashBat", NetMsg::FlashBat, &MessageDispatch::NetMsgFlashBat);
  register_entry ("ScoreInfo", NetMsg::ScoreInfo, &MessageDispatch::NetMsgScoreInfo);
  register_entry ("ScoreAttrib", NetMsg::ScoreAttrib, &MessageDispatch::NetMsgScoreAttrib);
  register_entry ("ResetHUD", NetMsg::ResetHUD, &MessageDispatch::NetMsgResetHud);

  // we're need next messages ids but we're won't handle them, so they will be removed from wanted list as soon as they get engine ids
  register_entry ("BotVoice", NetMsg::BotVoice, nullptr);
  register_entry ("SendAudio", NetMsg::SendAudio, nullptr);
  register_entry ("SayText", NetMsg::SayText, nullptr);
  register_entry ("ScreenShake", NetMsg::ScreenShake, nullptr);

  // register text msg cache (fixed array, no heap allocation)
  // clang-format off
   text_msg_cache_ = { {
      { "#CTs_Win", TextMsgCache::CounterWin },
      { "#Bomb_Defused", TextMsgCache::CounterWin },
      { "#Bomb_Planted", TextMsgCache::BombPlanted },
      { "#Terrorists_Win", TextMsgCache::TerroristWin },
      { "#Round_Draw", TextMsgCache::RestartRound },
      { "#All_Hostages_Rescued", TextMsgCache::CounterWin },
      { "#Target_Saved", TextMsgCache::CounterWin },
      { "#Hostages_Not_Rescued", TextMsgCache::TerroristWin },
      { "#Terrorists_Not_Escaped", TextMsgCache::CounterWin },
      { "#VIP_Not_Escaped", TextMsgCache::TerroristWin },
      { "#Escaping_Terrorists_Neutralized", TextMsgCache::CounterWin },
      { "#VIP_Assassinated", TextMsgCache::TerroristWin },
      { "#VIP_Escaped", TextMsgCache::CounterWin },
      { "#Terrorists_Escaped", TextMsgCache::TerroristWin },
      { "#CTs_PreventEscape", TextMsgCache::CounterWin },
      { "#Target_Bombed", TextMsgCache::TerroristWin },
      { "#Game_Commencing", TextMsgCache::Commencing },
      { "#Game_will_restart_in", TextMsgCache::RestartRound },
      { "#Switch_To_BurstFire", TextMsgCache::BurstOn },
      { "#Switch_To_SemiAuto", TextMsgCache::BurstOff },
      { "#Switch_To_FullAuto", TextMsgCache::BurstOff }
   } };

   // register show menu cache
   show_menu_cache_ = { {
      { "#Team_Select", Msg::TeamSelect },
      { "#Team_Select_Spect", Msg::TeamSelect },
      { "#IG_Team_Select_Spect", Msg::TeamSelect },
      { "#IG_Team_Select", Msg::TeamSelect },
      { "#IG_VIP_Team_Select", Msg::TeamSelect },
      { "#IG_VIP_Team_Select_Spect", Msg::TeamSelect },
      { "#Terrorist_Select", Msg::ClassSelect },
      { "#CT_Select", Msg::ClassSelect }
   } };

   // register status icon cache
   status_icon_cache_ = { {
      { "buyzone", StatusIconCache::BuyZone },
      { "escape", StatusIconCache::Escape },
      { "rescue", StatusIconCache::Rescue },
      { "vipsafety", StatusIconCache::VipSafety },
      { "c4", StatusIconCache::C4 },
      { "defuser", StatusIconCache::Defuser }
   } };

   // register team info cache
   team_info_cache_ = { {
      { "TERRORIST", Team::Terrorist },
      { "UNASSIGNED", Team::Unassigned },
      { "SPECTATOR", Team::Spectator },
      { "CT", Team::CT }
   } };
  // clang-format on
}

int32_t MessageDispatch::Add (ystl::StringRef name, int32_t id) {
  auto entry = entries_.find (name);

  if (!entry) {
    return id;
  }

  // store engine message id in the entry
  entry->engine_id = id;

  // map engine id to our message id for fast lookup during message processing
  if (id >= 0 && id < 256) {
    engine_to_net_msg_[id] = entry->id;
  }
  return id;
}

void MessageDispatch::Start (edict_t *ent, int32_t type) {
  Reset ();

  if (game.Is (GameFlags::Metamod)) {
    EnsureMessages ();
  }

  // search if we need to handle this message using fast array lookup
  if (type >= 0 && type < 256) {
    const auto msg = engine_to_net_msg_[type];

    if (msg != NetMsg::None) {
      // find entry by iterating (small fixed set, fast enough)
      for (const auto &item : entries_) {
        if (item.second.id == msg && item.second.HasHandler ()) {
          current_ = msg;
          break;
        }
      }
    }
  }

  // no message no processing
  if (current_ == NetMsg::None) {
    return;
  }

  // message for bot bot?
  if (!game.IsNullEntity (ent) && !(ent->v.flags & FL_DORMANT)) {
    bot_ = bots[ent];

    if (!bot_) {
      StopCollection ();
      return;
    }
  }
  args_.clear (); // clear previous args
}

void MessageDispatch::Stop () {
  if (current_ == NetMsg::None) {
    return;
  }

  // find handler by iterating entries
  for (const auto &item : entries_) {
    if (item.second.id == current_ && item.second.HasHandler ()) {
      (this->*item.second.handler) ();
      break;
    }
  }

  StopCollection ();
}

void MessageDispatch::EnsureMessages () {
  // refresh metamod message ids lost on unload and reload

  // check if we have engine ids registered
  bool has_ids = false;

  for (const auto &item : entries_) {
    if (item.second.HasEngineId ()) {
      has_ids = true;
      break;
    }
  }

  if (has_ids) {
    return;
  }

  // re-register our message
  for (const auto &item : entries_) {
    Add (item.first, MUTIL_GetUserMsgID (PLID, item.first.chars (), nullptr));
  }
}

int32_t MessageDispatch::Id (NetMsg msg) {
  // find engine id for this message
  for (const auto &item : entries_) {
    if (item.second.id == msg) {
      return item.second.engine_id;
    }
  }
  return MessageEntry::none;
}

Bot *MessageDispatch::PickBot (int32_t index) {
  const auto client_index = args_[index].long_ - 1;

  if (client_index < 0 || client_index >= game.MaxClients ()) {
    return nullptr;
  }
  const auto &client = clients[client_index];

  // get the bot in this message
  return bots[client.ent];
}

} // namespace bot
