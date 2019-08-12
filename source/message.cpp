//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

void MessageDispatcher::netMsgTextMsg () {
   enum args { msg = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min) {
      return;
   }

   // bots chatter notification
   const auto dispatchChatterMessage = [&] () -> void {
      if (yb_radio_mode.int_ () == 2) {
         auto notify = bots.findAliveBot ();

         if (notify && notify->m_notKilled) {
            notify->processChatterMessage (m_args[msg].chars_);

         }
      }
   };

   // lookup cached message
   auto cached = m_textMsgCache[m_args[msg].chars_];

   // check if we're need to handle message
   if (!(cached & TextMsgCache::NeedHandle)) {
      return;
   }

   // reset bomb position
   if (game.mapIs (MapFlags::Demolition)) {
      graph.setBombPos (true);
   }

   if (cached & TextMsgCache::Commencing) {
      util.setNeedForWelcome (true);
   }
   else if (cached & TextMsgCache::CounterWin) {
      bots.setLastWinner (Team::CT); // update last winner for economics
      dispatchChatterMessage ();
   }
   else if (cached & TextMsgCache::RestartRound) {
      bots.updateTeamEconomics (Team::CT, true);
      bots.updateTeamEconomics (Team::Terrorist, true);
   }
   else if (cached & TextMsgCache::TerroristWin) {
      bots.setLastWinner (Team::Terrorist); // update last winner for economics
      dispatchChatterMessage ();
   }
   else if ((cached & TextMsgCache::BombPlanted) && !bots.isBombPlanted ()) {
      bots.setBombPlanted (true);

      for (const auto &notify : bots) {
         if (notify->m_notKilled) {
            notify->clearSearchNodes ();
            notify->clearTasks ();

            if (yb_radio_mode.int_ () == 2 && rg.chance (55) && notify->m_team == Team::CT) {
               notify->pushChatterMessage (Chatter::WhereIsTheC4);
            }
         }
      }
      graph.setBombPos ();
   }

   // check for burst fire message
   if (m_bot) {
      if (cached & TextMsgCache::BurstOn) {
         m_bot->m_weaponBurstMode = BurstMode::On;
      }
      else if (cached & TextMsgCache::BurstOff) {
         m_bot->m_weaponBurstMode = BurstMode::Off;
      }
   }
}

void MessageDispatcher::netMsgVGUIMenu () {
   // this message is sent when a VGUI menu is displayed.

   enum args { menu = 0, min = 1 };

   // check the minimum states or existance of bot
   if (m_args.length () < min || !m_bot) {
      return;
   }

   switch (m_args[menu].long_) {
   case GuiMenu::TeamSelect:
      m_bot->m_startAction = BotMsg::TeamSelect;
      break;

   case GuiMenu::TerroristSelect:
   case GuiMenu::CTSelect:
      m_bot->m_startAction = BotMsg::ClassSelect;
      break;
   }
}

void MessageDispatcher::netMsgShowMenu () {
   // this message is sent when a text menu is displayed.

   enum args { menu = 3, min = 4 };

   // check the minimum states or existance of bot
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_startAction = m_showMenuCache[m_args[menu].chars_];
}

void MessageDispatcher::netMsgWeaponList () {
   // this message is sent when a client joins the game. All of the weapons are sent with the weapon ID and information about what ammo is used.

   enum args { classname = 0, ammo_index_1 = 1, max_ammo_1 = 2, slot = 5, slot_pos = 6, id = 7, flags = 8, min = 9 };

   // check the minimum states
   if (m_args.length () < min) {
      return;
   }

   // register prop
   WeaponProp prop {
      m_args[classname].chars_,
      m_args[ammo_index_1].long_,
      m_args[max_ammo_1].long_,
      m_args[slot].long_,
      m_args[slot_pos].long_,
      m_args[id].long_,
      m_args[flags].long_
   };
   conf.setWeaponProp (cr::move (prop)); // store away this weapon with it's ammo information...
}

void MessageDispatcher::netMsgCurWeapon () {
   // this message is sent when a weapon is selected (either by the bot chosing a weapon or by the server auto assigning the bot a weapon). In CS it's also called when Ammo is increased/decreased

   enum args { state = 0, id = 1, clip = 2, min = 3 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }

   if (m_args[id].long_ < kMaxWeapons) {
      if (m_args[state].long_ != 0) {
         m_bot->m_currentWeapon = m_args[id].long_;
      }

      // ammo amount decreased ? must have fired a bullet...
      if (m_args[id].long_ == m_bot->m_currentWeapon && m_bot->m_ammoInClip[m_args[id].long_] > m_args[clip].long_) {
         m_bot->m_timeLastFired = game.timebase (); // remember the last bullet time
      }
      m_bot->m_ammoInClip[m_args[id].long_] = m_args[clip].long_;
   }
}

void MessageDispatcher::netMsgAmmoX () {
   // this message is sent whenever ammo amounts are adjusted (up or down). NOTE: Logging reveals that CS uses it very unreliable!

   enum args { index = 0, value = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_ammo[m_args[index].long_] = m_args[value].long_; // store it away
}

void MessageDispatcher::netMsgAmmoPickup () {
   // this message is sent when the bot picks up some ammo (AmmoX messages are also sent so this message is probably
   // not really necessary except it allows the HUD to draw pictures of ammo that have been picked up.  The bots
   // don't really need pictures since they don't have any eyes anyway.

   enum args { index = 0, value = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_ammo[m_args[index].long_] = m_args[value].long_; // store it away
}

void MessageDispatcher::netMsgDamage () {
   // this message gets sent when the bots are getting damaged.

   enum args { armor = 0, health = 1, bits = 2, min = 3 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   
   // handle damage if any
   if (m_args[armor].long_ > 0 || m_args[health].long_) {
      m_bot->processDamage (m_bot->pev->dmg_inflictor, m_args[health].long_, m_args[armor].long_, m_args[bits].long_);
   }
}

void MessageDispatcher::netMsgMoney () {
   // this message gets sent when the bots money amount changes

   enum args { money = 0, min = 1 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_moneyAmount = m_args[money].long_;
}

void MessageDispatcher::netMsgStatusIcon () {
   enum args { enabled = 0, icon = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   // lookup cached icon
   auto cached = m_statusIconCache[m_args[icon].chars_];

   // check if we're need to handle message
   if (!(cached & TextMsgCache::NeedHandle)) {
      return;
   }

   // handle cases
   if (cached & StatusIconCache::BuyZone) {
      m_bot->m_inBuyZone = (m_args[enabled].long_ != 0);

      // try to equip in buyzone
      m_bot->processBuyzoneEntering (BuyState::PrimaryWeapon);
   }
   else if (cached & StatusIconCache::VipSafety) {
      m_bot->m_inVIPZone = (m_args[enabled].long_ != 0);
   }
   else if (cached & StatusIconCache::C4) {
      m_bot->m_inBombZone = (m_args[enabled].long_ == 2);
   }
}

void MessageDispatcher::netMsgDeathMsg () {
   // this message gets sent when player kills player

   enum args { killer = 0, victim = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min) {
      return;
   }

   auto killerEntity = game.entityOfIndex (m_args[killer].long_);
   auto victimEntity = game.entityOfIndex (m_args[victim].long_);

   if (game.isNullEntity (killerEntity) || game.isNullEntity (victimEntity) || victimEntity == killerEntity) {
      return;
   }
   bots.handleDeath (killerEntity, victimEntity);
}

void MessageDispatcher::netMsgScreenFade () {
   // this message gets sent when the screen fades (flashbang)

   enum args { r = 3, g = 4, b = 5, alpha = 6, min = 7 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   
   // screen completely faded ?
   if (m_args[r].long_ >= 255 && m_args[g].long_ >= 255 && m_args[b].long_ >= 255 && m_args[alpha].long_ > 170) {
      m_bot->processBlind (m_args[alpha].long_);
   }
}

void MessageDispatcher::netMsgHLTV () {
   // this message gets sent when new round is started in modern cs versions

   enum args { players = 0, fov = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min) {
      return;
   }

   // need to start new round ? (we're tracking FOV reset message)
   if (m_args[players].long_ == 0 && m_args[fov].long_ == 0) {
      bots.initRound ();
   }
}

void MessageDispatcher::netMsgTeamInfo () {
   // this message gets sent when player team index is changed

   enum args { index = 0, team = 1, min = 2 };

   // check the minimum states
   if (m_args.length () < min) {
      return;
   }
   auto &client = util.getClient (m_args[index].long_ - 1);

   // update player team
   client.team2 = m_teamInfoCache[m_args[team].chars_]; // update real team
   client.team = game.is (GameFlags::FreeForAll) ? m_args[index].long_ : client.team2;
}

void MessageDispatcher::netMsgBarTime () {
   enum args { enabled = 0, min = 1 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }

   // check if has progress bar
   if (m_args[enabled].long_ > 0) {
      m_bot->m_hasProgressBar = true; // the progress bar on a hud

      // notify bots about defusing has started
      if (game.mapIs (MapFlags::Demolition) && bots.isBombPlanted () && m_bot->m_team == Team::CT) {
         bots.notifyBombDefuse ();
      }
   }
   else {
      m_bot->m_hasProgressBar = false; // no progress bar or disappeared
   }
}

void MessageDispatcher::netMsgItemStatus () {
   enum args { value = 0, min = 1 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   auto mask = m_args[value].long_;

   m_bot->m_hasNVG = !!(mask & ItemStatus::Nightvision);
   m_bot->m_hasDefuser = !!(mask & ItemStatus::DefusalKit);
}

void MessageDispatcher::netMsgNVGToggle () {
   enum args { value = 0, min = 1 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_usesNVG = m_args[value].long_ > 0;
}

void MessageDispatcher::netMsgFlashBat () {
   enum args { value = 0, min = 1 };

   // check the minimum states
   if (m_args.length () < min || !m_bot) {
      return;
   }
   m_bot->m_flashLevel = m_args[value].float_;
}

MessageDispatcher::MessageDispatcher () {

   // register wanted message
   auto pushWanted = [&] (const String &name, NetMsg id, MsgFunc handler) -> void {
      m_wanted[name] = id;
      m_handlers[id] = handler;
   };

   // we want to handle next messages
   pushWanted ("TextMsg", NetMsg::TextMsg, &MessageDispatcher::netMsgTextMsg);
   pushWanted ("VGUIMenu", NetMsg::VGUIMenu, &MessageDispatcher::netMsgVGUIMenu);
   pushWanted ("ShowMenu", NetMsg::ShowMenu, &MessageDispatcher::netMsgShowMenu);
   pushWanted ("WeaponList", NetMsg::WeaponList, &MessageDispatcher::netMsgWeaponList);
   pushWanted ("CurWeapon", NetMsg::CurWeapon, &MessageDispatcher::netMsgCurWeapon);
   pushWanted ("AmmoX", NetMsg::AmmoX, &MessageDispatcher::netMsgAmmoX);
   pushWanted ("AmmoPickup", NetMsg::AmmoPickup, &MessageDispatcher::netMsgAmmoPickup);
   pushWanted ("Damage", NetMsg::Damage, &MessageDispatcher::netMsgDamage);
   pushWanted ("Money", NetMsg::Money, &MessageDispatcher::netMsgMoney);
   pushWanted ("StatusIcon", NetMsg::StatusIcon, &MessageDispatcher::netMsgStatusIcon);
   pushWanted ("DeathMsg", NetMsg::DeathMsg, &MessageDispatcher::netMsgDeathMsg);
   pushWanted ("ScreenFade", NetMsg::ScreenFade, &MessageDispatcher::netMsgScreenFade);
   pushWanted ("HLTV", NetMsg::HLTV, &MessageDispatcher::netMsgHLTV);
   pushWanted ("TeamInfo", NetMsg::TeamInfo, &MessageDispatcher::netMsgTeamInfo);
   pushWanted ("BarTime", NetMsg::BarTime, &MessageDispatcher::netMsgBarTime);
   pushWanted ("ItemStatus", NetMsg::ItemStatus, &MessageDispatcher::netMsgItemStatus);
   pushWanted ("NVGToggle", NetMsg::NVGToggle, &MessageDispatcher::netMsgNVGToggle);
   pushWanted ("FlashBat", NetMsg::FlashBat, &MessageDispatcher::netMsgFlashBat);

   // we're need next messages IDs but we're won't handle them, so they will be removed from wanted list as soon as they get engine IDs
   pushWanted ("BotVoice", NetMsg::BotVoice, nullptr);
   pushWanted ("SendAudio", NetMsg::SendAudio, nullptr);

   // register text msg cache
   m_textMsgCache["#CTs_Win"] = TextMsgCache::NeedHandle | TextMsgCache::CounterWin;
   m_textMsgCache["#Bomb_Defused"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Bomb_Planted"] = TextMsgCache::NeedHandle | TextMsgCache::BombPlanted;
   m_textMsgCache["#Terrorists_Win"] = TextMsgCache::NeedHandle | TextMsgCache::TerroristWin;
   m_textMsgCache["#Round_Draw"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#All_Hostages_Rescued"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Target_Saved"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Hostages_Not_Rescued"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Terrorists_Not_Escaped"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#VIP_Not_Escaped"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Escaping_Terrorists_Neutralized"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#VIP_Assassinated"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#VIP_Escaped"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Terrorists_Escaped"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#CTs_PreventEscape"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Target_Bombed"] = TextMsgCache::NeedHandle;
   m_textMsgCache["#Game_Commencing"] = TextMsgCache::NeedHandle | TextMsgCache::Commencing;
   m_textMsgCache["#Game_will_restart_in"] = TextMsgCache::NeedHandle | TextMsgCache::RestartRound;
   m_textMsgCache["#Switch_To_BurstFire"] = TextMsgCache::NeedHandle | TextMsgCache::BurstOn;
   m_textMsgCache["#Switch_To_SemiAuto"] = TextMsgCache::NeedHandle | TextMsgCache::BurstOff;

   // register show menu cache
   m_showMenuCache["#Team_Select"] = BotMsg::TeamSelect;
   m_showMenuCache["#Team_Select_Spect"] = BotMsg::TeamSelect;
   m_showMenuCache["#IG_Team_Select_Spect"] = BotMsg::TeamSelect;
   m_showMenuCache["#IG_Team_Select"] = BotMsg::TeamSelect;
   m_showMenuCache["#IG_VIP_Team_Select"] = BotMsg::TeamSelect;
   m_showMenuCache["#IG_VIP_Team_Select_Spect"] = BotMsg::TeamSelect;
   m_showMenuCache["#Terrorist_Select"] = BotMsg::ClassSelect;
   m_showMenuCache["#CT_Select"] = BotMsg::ClassSelect;

   // register status icon cache
   m_statusIconCache["buyzone"] = StatusIconCache::NeedHandle | StatusIconCache::BuyZone;
   m_statusIconCache["vipsafety"] = StatusIconCache::NeedHandle | StatusIconCache::VipSafety;
   m_statusIconCache["c4"] = StatusIconCache::NeedHandle | StatusIconCache::C4;

   // register team info cache
   m_teamInfoCache["TERRORIST"] = Team::Terrorist;
   m_teamInfoCache["UNASSIGNED"] = Team::Unassigned;
   m_teamInfoCache["SPECTATOR"] = Team::Spectator;
   m_teamInfoCache["CT"] = Team::CT;
}

void MessageDispatcher::registerMessage (const String &name, int32 id) {
   if (!m_wanted.exists (name)) {
      return;
   }
   m_maps[m_wanted[name]] = id; // add message from engine RegUserMsg
}

void MessageDispatcher::start (edict_t *ent, int32 dest, int32 type) {
   m_current = NetMsg::None;

   // search if we need to handle this message
   for (const auto &msg : m_maps) {
      if (msg.value == type && m_handlers[msg.key]) {
         m_current = msg.key;
         break;
      }
   }

   // no messagem no processing
   if (m_current == NetMsg::None) {
      return;
   }

   // broadcast message ?
   if (dest == MSG_ALL || dest == MSG_SPEC || dest == MSG_BROADCAST) {
      m_broadcast = true;
   }

   // message for bot bot?
   if (ent && (ent->v.flags & FL_FAKECLIENT)) {
      m_bot = bots[ent];

      if (!m_bot) {
         m_current = NetMsg::None;
         return;
      }
   }
   m_args.clear (); // clear previous args
}

void MessageDispatcher::stop () {
   if (m_current == NetMsg::None) {
      return;
   }
   (this->*m_handlers[m_current]) ();
   m_current = NetMsg::None;
}
