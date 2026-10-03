//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// netmessage functions enum
namespace bot {

enum class NetMsg : int32_t {
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
  ScoreInfo = 23,
  ScoreAttrib = 24,
  SayText = 25,
  ResetHUD = 26,
  ScreenShake = 27
};
YSTL_ENABLE_ENUM_HASH (NetMsg);

// vgui menus (since latest steam updates is obsolete, but left for old cs)
enum class GuiMenu : int32_t {
  TeamSelect = 2, // menu select team
  TerroristSelect = 26, // terrorist select menu
  CTSelect = 27 // ct select menu
};

// cache flags for textmsg message
enum class TextMsgCache : int32_t {
  TerroristWin = ystl::bit (0),
  CounterWin = ystl::bit (1),
  Commencing = ystl::bit (2),
  BombPlanted = ystl::bit (3),
  RestartRound = ystl::bit (4),
  BurstOn = ystl::bit (5),
  BurstOff = ystl::bit (6)
};
YSTL_ENABLE_ENUM_FLAGS (TextMsgCache);

// cache flags for statusicon message
enum class StatusIconCache : int32_t {
  BuyZone = ystl::bit (0),
  Escape = ystl::bit (1),
  Rescue = ystl::bit (2),
  VipSafety = ystl::bit (3),
  C4 = ystl::bit (4),
  Defuser = ystl::bit (5)
};
YSTL_ENABLE_ENUM_FLAGS (StatusIconCache);

// item status for statusicon message
enum class ItemStatus : int32_t {
  Nightvision = ystl::bit (0),
  DefusalKit = ystl::bit (1)
};
YSTL_ENABLE_ENUM_FLAGS (ItemStatus);

class MessageDispatch final : public ystl::Singleton<MessageDispatch> {
private:
  using MsgFunc = void (MessageDispatch::*) ();

private:
  struct Args {
    union {
      float float_;
      int32_t long_;
      const char *chars_;
    };

  public:
    Args (float value) : float_ (value) {}
    Args (int32_t value) : long_ (value) {}
    Args (const char *value) : chars_ (value) {}
  };

  struct MessageEntry {
    static constexpr int32_t none = -1;

    NetMsg id {};
    MsgFunc handler {};
    int32_t engine_id = none;

    bool HasHandler () const {
      return handler != nullptr;
    }

    bool HasEngineId () const {
      return engine_id != none;
    }
  };

  template <typename T> struct StringKeyPair {
    ystl::StringRef key {};
    T value {};
  };

private:
  ystl::FixedArray<StringKeyPair<TextMsgCache>, 21> text_msg_cache_ {};
  ystl::FixedArray<StringKeyPair<Msg>, 8> show_menu_cache_ {};
  ystl::FixedArray<StringKeyPair<StatusIconCache>, 6> status_icon_cache_ {};
  ystl::FixedArray<StringKeyPair<Team>, 4> team_info_cache_ {};

private:
  Bot *bot_ {}; // owner of a message
  NetMsg current_ {}; // ongoing message id

  ystl::SmallArray<Args> args_ {}; // args collected from write* functions

  ystl::HashMap<ystl::String, MessageEntry> entries_ {}; // name -> message entry (id, handler, engineid)
  ystl::FixedArray<NetMsg, 256> engine_to_net_msg_ {}; // engine_id -> netmsg

private:
  void NetMsgTextMsg ();
  void NetMsgVguiMenu ();
  void NetMsgShowMenu ();
  void NetMsgWeaponList ();
  void NetMsgCurWeapon ();
  void NetMsgAmmoX ();
  void NetMsgAmmoPickup ();
  void NetMsgDamage ();
  void NetMsgMoney ();
  void NetMsgStatusIcon ();
  void NetMsgDeathMsg ();
  void NetMsgScreenFade ();
  void NetMsgHltv ();
  void NetMsgTeamInfo ();
  void NetMsgBarTime ();
  void NetMsgItemStatus ();
  void NetMsgNvgToggle ();
  void NetMsgFlashBat ();
  void NetMsgScoreInfo ();
  void NetMsgScoreAttrib ();
  void NetMsgResetHud ();

private:
  Bot *PickBot (int32_t index);

public:
  MessageDispatch ();
  ~MessageDispatch () = default;

public:
  int32_t Add (ystl::StringRef name, int32_t id);
  int32_t Id (NetMsg msg);

  void Start (edict_t *ent, int32_t type);
  void Stop ();
  void EnsureMessages ();

public:
  template <typename T> void Collect (const T &value) {
    if (current_ == NetMsg::None) {
      return;
    }
    args_.emplace (value);
  }

  void StopCollection () {
    current_ = NetMsg::None;
  }

private:
  template <typename T, size_t N> static const T *FindInCache (const ystl::FixedArray<StringKeyPair<T>, N> &cache, ystl::StringRef key) {
    for (const auto &item : cache) {
      if (item.key == key) {
        return &item.value;
      }
    }
    return nullptr;
  }

private:
  void Reset () {
    StopCollection ();
    bot_ = nullptr;
  }
};

YSTL_EXPOSE_GLOBAL_SINGLETON (MessageDispatch, msgs);

} // namespace bot
