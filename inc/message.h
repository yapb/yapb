//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2021 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#pragma once

// netmessage functions enum
CR_DECLARE_SCOPED_ENUM (NetMsg,
   None = -1,
   VGUIMenu = 1,
   ShowMenu = 2,
   WeaponList = 3,
   CurWeapon = 4,
   AmmoX = 5,
   AmmoPickup = 6,
   Damage = 7,
   Money = 8,
   StatusIcon = 9,
   DeathMsg = 10,
   ScreenFade = 11,
   HLTV = 12,
   TextMsg = 13,
   TeamInfo = 14,
   BarTime = 15,
   SendAudio = 17,
   BotVoice = 18,
   NVGToggle = 19,
   FlashBat = 20,
   Fashlight = 21,
   ItemStatus = 22,
   ScoreInfo = 23
)

// vgui menus (since latest steam updates is obsolete, but left for old cs)
CR_DECLARE_SCOPED_ENUM (GuiMenu,
   TeamSelect = 2, // menu select team
   TerroristSelect = 26, // terrorist select menu
   CTSelect = 27  // ct select menu
)

// cache flags for TextMsg message
CR_DECLARE_SCOPED_ENUM (TextMsgCache,
   NeedHandle = cr::bit (0),
   TerroristWin = cr::bit (1),
   CounterWin = cr::bit (2),
   Commencing = cr::bit (3),
   BombPlanted = cr::bit (4),
   RestartRound = cr::bit (5),
   BurstOn = cr::bit (6),
   BurstOff = cr::bit (7)
)

// cache flags for StatusIcon message
CR_DECLARE_SCOPED_ENUM (StatusIconCache,
   NeedHandle = cr::bit (0),
   BuyZone = cr::bit (1),
   VipSafety = cr::bit (2),
   C4 = cr::bit (3)
)

class MessageDispatcher final : public Singleton <MessageDispatcher> {
private:
   using MsgFunc = void (MessageDispatcher::*) ();
   using MsgHash = Hash <int32>;

private:
   struct Args {
      union {
         float float_;
         int32 long_;
         const char *chars_;
      };

   public:
      Args (float value) : float_ (value) { }
      Args (int32 value) : long_ (value) { }
      Args (const char *value) : chars_ (value) { }
   };

private:
   HashMap <String, int32> m_textMsgCache; // cache strings for faster access for textmsg
   HashMap <String, int32> m_showMenuCache; // cache for the showmenu message
   HashMap <String, int32> m_statusIconCache; // cache for status icon message
   HashMap <String, int32> m_teamInfoCache; // cache for teaminfo message

private:
   Bot *m_bot {}; // owner of a message
   NetMsg m_current {}; // ongoing message id

   SmallArray <Args> m_args; // args collected from write* functions

   HashMap <String, NetMsg> m_wanted; // wanted messages
   HashMap <int32, NetMsg> m_reverseMap; // maps engine message id to our message id

   HashMap <NetMsg, int32, MsgHash> m_maps; // maps our message to id to engine message id
   HashMap <NetMsg, MsgFunc, MsgHash> m_handlers; // maps our message id to handler function

private:
   void netMsgTextMsg ();
   void netMsgVGUIMenu ();
   void netMsgShowMenu ();
   void netMsgWeaponList ();
   void netMsgCurWeapon ();
   void netMsgAmmoX ();
   void netMsgAmmoPickup ();
   void netMsgDamage ();
   void netMsgMoney ();
   void netMsgStatusIcon ();
   void netMsgDeathMsg ();
   void netMsgScreenFade ();
   void netMsgHLTV ();
   void netMsgTeamInfo ();
   void netMsgBarTime ();
   void netMsgItemStatus ();
   void netMsgNVGToggle ();
   void netMsgFlashBat ();
   void netMsgScoreInfo ();

public:
   MessageDispatcher ();
   ~MessageDispatcher () = default;

public:
   int32 add (StringRef name, int32 id);
   int32 id (NetMsg msg);

   void start (edict_t *ent, int32 type);
   void stop ();
   void ensureMessages ();

public:
   template <typename T> void collect (const T &value) {
      if (m_current == NetMsg::None) {
         return;
      }
      m_args.emplace (value);
   }

   void stopCollection () {
      m_current = NetMsg::None;
   }

private:
   void reset () {
      m_current = NetMsg::None;
      m_bot = nullptr;
   }
};

CR_EXPOSE_GLOBAL_SINGLETON (MessageDispatcher, msgs);
