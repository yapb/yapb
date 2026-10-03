//
// YaPB test host: unit/message_{dispatch,items,round}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for message.cpp through the public MessageDispatcher
// API only (no production-code changes): registration/plumbing, per-bot
// item/state messages, and round-flow messages (wins, plant, scores,
// death, hltv, reset).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

// engine ids used inside the tests (arbitrary, registered per case)
constexpr int32_t kMoney = 11;
constexpr int32_t kCurWeapon = 12;
constexpr int32_t kAmmo = 13;
constexpr int32_t kWeaponList = 14;
constexpr int32_t kDamage = 15;
constexpr int32_t kStatusIcon = 16;
constexpr int32_t kScreenFade = 17;
constexpr int32_t kVGUIMenu = 18;
constexpr int32_t kShowMenu = 19;
constexpr int32_t kBarTime = 20;
constexpr int32_t kItemStatus = 21;
constexpr int32_t kNVGToggle = 22;
constexpr int32_t kFlashBat = 23;
constexpr int32_t kTextMsg = 24;
constexpr int32_t kTeamInfo = 25;
constexpr int32_t kScoreInfo = 26;
constexpr int32_t kScoreAttrib = 27;
constexpr int32_t kDeathMsg = 28;
constexpr int32_t kHLTV = 29;
constexpr int32_t kResetHUD = 30;
constexpr int32_t kSayText = 31; // registered without a handler

// minimal chain, enough for BotManager::create
void BuildMsgGraph () {
  graph.Reset ();

  for (int i = 0; i < 4; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = 100;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  link (0, 1);
  link (1, 0);
  link (1, 2);
  link (2, 1);
  link (2, 3);
  link (3, 2);

  graph.PopulateNodes ();
  planner.Init ();
}

void BootMessages (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildMsgGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

void SendLongs (edict_t *ent, int32_t type, std::initializer_list<int32_t> values) {
  msgs.Start (ent, type);

  for (const auto value : values) {
    msgs.Collect (value);
  }
  msgs.Stop ();
}

void SendText (edict_t *ent, int32_t type, int32_t first, const char *text) {
  msgs.Start (ent, type);
  msgs.Collect (first);
  msgs.Collect (text);
  msgs.Stop ();
}

} // namespace

TEST_CASE ("unit/message_dispatch") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootMessages (engine, cs);

  // cold dispatcher knows nothing
  CHECK (msgs.Id (NetMsg::TextMsg) == -1);
  CHECK (msgs.Id (NetMsg::Money) == -1);

  // stop/collect outside a message are safe no-ops
  msgs.Stop ();
  msgs.Collect<int32_t> (5);

  // registration maps names to engine ids, unknown names pass through
  CHECK (msgs.Add ("TextMsg", kTextMsg) == kTextMsg);
  CHECK (msgs.Add ("Money", kMoney) == kMoney);
  CHECK (msgs.Add ("NoSuchMessage", 77) == 77);
  CHECK (msgs.Id (NetMsg::TextMsg) == kTextMsg);
  CHECK (msgs.Id (NetMsg::Money) == kMoney);

  // metatmod refresh is a no-op once ids exist
  msgs.EnsureMessages ();
  CHECK (msgs.Id (NetMsg::TextMsg) == kTextMsg);

  // no winner by default, so neither team gets the winner bonus
  CHECK (bots.GetLastWinner () == Team::Invalid);

  bots.Addbot ("DispBot", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("DispBot");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("DispHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;

  // messages for strangers are dropped: bot state untouched
  SendLongs (human, kMoney, { 1500 });
  CHECK (bot->money_amount_ == 0);

  // ...even for handler-less messages and unknown engine ids
  CHECK (msgs.Add ("SayText", kSayText) == kSayText);
  SendLongs (human, kSayText, { 1, 2, 3 });
  SendLongs (human, 200, { 1 });
  CHECK (bot->money_amount_ == 0);

  // dormant carriers still dispatch the bot-independent parts
  bots.SetLastWinner (Team::CT);
  human->v.flags |= FL_DORMANT;
  SendText (human, kTextMsg, 2, "#Terrorists_Win");
  CHECK (bots.GetLastWinner () == Team::Terrorist);
  human->v.flags &= ~FL_DORMANT;

  // ...while live strangers drop everything
  SendText (human, kTextMsg, 2, "#CTs_Win");
  CHECK (bots.GetLastWinner () == Team::Terrorist);

  // the cold collect() above polluted nothing: a real message works
  SendLongs (bot->Ent (), kMoney, { 1500 });
  CHECK (bot->money_amount_ == 1500);

  bots.Destroy ();
}

TEST_CASE ("unit/message_items") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootMessages (engine, cs);

  CHECK (msgs.Add ("Money", kMoney) == kMoney);
  CHECK (msgs.Add ("CurWeapon", kCurWeapon) == kCurWeapon);
  CHECK (msgs.Add ("AmmoPickup", kAmmo) == kAmmo);
  CHECK (msgs.Add ("WeaponList", kWeaponList) == kWeaponList);
  CHECK (msgs.Add ("Damage", kDamage) == kDamage);
  CHECK (msgs.Add ("StatusIcon", kStatusIcon) == kStatusIcon);
  CHECK (msgs.Add ("ScreenFade", kScreenFade) == kScreenFade);
  CHECK (msgs.Add ("VGUIMenu", kVGUIMenu) == kVGUIMenu);
  CHECK (msgs.Add ("ShowMenu", kShowMenu) == kShowMenu);
  CHECK (msgs.Add ("BarTime", kBarTime) == kBarTime);
  CHECK (msgs.Add ("ItemStatus", kItemStatus) == kItemStatus);
  CHECK (msgs.Add ("NVGToggle", kNVGToggle) == kNVGToggle);
  CHECK (msgs.Add ("FlashBat", kFlashBat) == kFlashBat);

  bots.Addbot ("ItemBot", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("ItemBot");
  HOST_REQUIRE (bot != nullptr);

  // money with refunds: negative resets, overfull clamps to the max
  const int max_money = mp_maxmoney.As<int> ();
  SendLongs (bot->Ent (), kMoney, { 1500 });
  CHECK (bot->money_amount_ == (1500 > max_money ? max_money : 1500));
  SendLongs (bot->Ent (), kMoney, { -5 });
  CHECK (bot->money_amount_ == 800);
  SendLongs (bot->Ent (), kMoney, { max_money + 9999 });
  CHECK (bot->money_amount_ == max_money);
  SendLongs (bot->Ent (), kMoney, {}); // too short, keeps the old
  CHECK (bot->money_amount_ == max_money);

  // weapon select stores the gun and the clip...
  SendLongs (bot->Ent (), kCurWeapon, { 1, 6, 30 });
  CHECK (bot->current_weapon_ == static_cast<Weapon> (6));
  CHECK (bot->ammo_in_clip_[6] == 30);

  // ...a shrinking clip means a bullet was fired...
  SendLongs (bot->Ent (), kCurWeapon, { 1, 6, 10 });
  CHECK (bot->ammo_in_clip_[6] == 10);
  CHECK (bot->last_fired_timer_.started ());

  // ...holstered updates keep the old gun, bad ids are ignored
  SendLongs (bot->Ent (), kCurWeapon, { 0, 6, 50 });
  CHECK (bot->current_weapon_ == static_cast<Weapon> (6));
  CHECK (bot->ammo_in_clip_[6] == 50);
  SendLongs (bot->Ent (), kCurWeapon, { 1, 999, 10 });
  CHECK (bot->ammo_in_clip_[6] == 50);
  SendLongs (bot->Ent (), kCurWeapon, { 1, 6 });
  CHECK (bot->ammo_in_clip_[6] == 50);

  // ammo pickup stores by slot, out of range is dropped
  SendLongs (bot->Ent (), kAmmo, { 3, 45 });
  CHECK (bot->ammo_[3] == 45);
  SendLongs (bot->Ent (), kAmmo, { -1, 5 });
  SendLongs (bot->Ent (), kAmmo, { MAX_AMMO_SLOTS, 5 });
  CHECK (bot->ammo_[3] == 45);
  SendLongs (bot->Ent (), kAmmo, { 4 });
  CHECK (bot->ammo_[3] == 45);

  // weapon list publishes gun metadata into conf
  msgs.Start (bot->Ent (), kWeaponList);
  msgs.Collect ("weapon_ak47");
  for (const auto value : { int32_t (2), int32_t (90), int32_t (0), int32_t (0), int32_t (1), int32_t (2), int32_t (4), int32_t (8) }) {
    msgs.Collect (value);
  }
  msgs.Stop ();

  const auto &prop = conf.GetWeaponProp (static_cast<Weapon> (4));
  CHECK (ystl::StringRef (prop.classname.chars ()) == "weapon_ak47");
  CHECK (prop.ammo1 == 2);
  CHECK (prop.ammo1_max == 90);
  CHECK (prop.slot == 1);
  CHECK (prop.pos == 2);
  CHECK (prop.flags == 8);

  msgs.Start (bot->Ent (), kWeaponList); // too short, keeps the old
  msgs.Collect ("weapon_awp");
  msgs.Collect (1);
  msgs.Collect (2);
  msgs.Stop ();
  CHECK (ystl::StringRef (conf.GetWeaponProp (static_cast<Weapon> (4)).classname.chars ()) == "weapon_ak47");

  // zero damage never reaches takeDamage
  const float health = bot->pev->health;
  SendLongs (bot->Ent (), kDamage, { 0, 0, 0 });
  CHECK (bot->pev->health == health);

  // live damage routes (armor, health, bits) straight into takeDamage
  edict_t *foe = game.CreateFakeClient ("DamageFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::Terrorist;
  clients[foe].team2 = Team::Terrorist;
  bot->team_ = Team::CT;
  bot->health_value_ = 100.0f;
  bot->agression_level_ = 0.0f;
  bot->pev->dmg_inflictor = foe;

  SendLongs (bot->Ent (), kDamage, { 5, 20, 0 });
  CHECK (bot->last_damage_type_ == 0);
  CHECK (bot->agression_level_ == 0.1f);
  CHECK (bot->last_enemy_ == foe);

  // status icons flip the zone flags, unknown icons are skipped
  SendLongs (bot->Ent (), kStatusIcon, { 1 });
  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("buyzone");
  msgs.Stop ();
  CHECK (bot->in_buy_zone_);

  SendLongs (bot->Ent (), kStatusIcon, { 0 });
  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (0));
  msgs.Collect ("buyzone");
  msgs.Stop ();
  CHECK (!bot->in_buy_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("escape");
  msgs.Stop ();
  CHECK (bot->in_escape_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("rescue");
  msgs.Stop ();
  CHECK (bot->in_rescue_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("vipsafety");
  msgs.Stop ();
  CHECK (bot->in_vip_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (2));
  msgs.Collect ("c4");
  msgs.Stop ();
  CHECK (bot->in_bomb_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("c4");
  msgs.Stop ();
  CHECK (!bot->in_bomb_zone_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("defuser");
  msgs.Stop ();
  CHECK (bot->has_defuser_);

  msgs.Start (bot->Ent (), kStatusIcon);
  msgs.Collect (int32_t (1));
  msgs.Collect ("bogus_icon");
  msgs.Stop ();
  CHECK (!bot->in_buy_zone_); // nothing else flipped

  // full-white fade blinds, tinted one does not
  SendLongs (bot->Ent (), kScreenFade, { 0, 0, 0, 255, 255, 255, 200 });
  CHECK (bot->blind_timer_.started ());
  bot->blind_timer_.invalidate ();
  SendLongs (bot->Ent (), kScreenFade, { 0, 0, 0, 255, 0, 0, 200 });
  CHECK (!bot->blind_timer_.started ());
  SendLongs (bot->Ent (), kScreenFade, { 0, 0 }); // too short
  CHECK (!bot->blind_timer_.started ());

  // vgui menus drive team/class selection
  SendLongs (bot->Ent (), kVGUIMenu, { 2 });
  CHECK (bot->start_action_ == Msg::TeamSelect);
  SendLongs (bot->Ent (), kVGUIMenu, { 26 });
  CHECK (bot->start_action_ == Msg::ClassSelect);
  SendLongs (bot->Ent (), kVGUIMenu, { 27 });
  CHECK (bot->start_action_ == Msg::ClassSelect);
  SendLongs (bot->Ent (), kVGUIMenu, { 99 });
  CHECK (bot->start_action_ == Msg::ClassSelect);
  SendLongs (bot->Ent (), kVGUIMenu, {});
  CHECK (bot->start_action_ == Msg::ClassSelect);

  // text menus resolve through the cache
  msgs.Start (bot->Ent (), kShowMenu);
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect ("#Team_Select");
  msgs.Stop ();
  CHECK (bot->start_action_ == Msg::TeamSelect);

  msgs.Start (bot->Ent (), kShowMenu);
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect ("#CT_Select");
  msgs.Stop ();
  CHECK (bot->start_action_ == Msg::ClassSelect);

  msgs.Start (bot->Ent (), kShowMenu);
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect (int32_t (0));
  msgs.Collect ("#Bogus_Menu");
  msgs.Stop ();
  CHECK (bot->start_action_ == Msg::ClassSelect);

  // progress bar toggles
  SendLongs (bot->Ent (), kBarTime, { 100 });
  CHECK (bot->has_progress_bar_);
  SendLongs (bot->Ent (), kBarTime, { 0 });
  CHECK (!bot->has_progress_bar_);
  SendLongs (bot->Ent (), kBarTime, {});
  CHECK (!bot->has_progress_bar_);

  // item status bits, nightvision toggle, flash battery
  SendLongs (bot->Ent (), kItemStatus, { 3 });
  CHECK (bot->has_nvg_);
  CHECK (bot->has_defuser_);
  SendLongs (bot->Ent (), kItemStatus, { 0 });
  CHECK (!bot->has_nvg_);
  CHECK (!bot->has_defuser_);

  SendLongs (bot->Ent (), kNVGToggle, { 1 });
  CHECK (bot->uses_nvg_);
  SendLongs (bot->Ent (), kNVGToggle, { 0 });
  CHECK (!bot->uses_nvg_);

  SendLongs (bot->Ent (), kFlashBat, { 75 });
  CHECK (bot->flash_level_ == 75);

  bots.Destroy ();
}

TEST_CASE ("unit/message_round") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootMessages (engine, cs);

  CHECK (msgs.Add ("TextMsg", kTextMsg) == kTextMsg);
  CHECK (msgs.Add ("TeamInfo", kTeamInfo) == kTeamInfo);
  CHECK (msgs.Add ("ScoreInfo", kScoreInfo) == kScoreInfo);
  CHECK (msgs.Add ("ScoreAttrib", kScoreAttrib) == kScoreAttrib);
  CHECK (msgs.Add ("DeathMsg", kDeathMsg) == kDeathMsg);
  CHECK (msgs.Add ("HLTV", kHLTV) == kHLTV);
  CHECK (msgs.Add ("ResetHUD", kResetHUD) == kResetHUD);

  // no winner by default
  CHECK (bots.GetLastWinner () == Team::Invalid);

  bots.Addbot ("RoundA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("RoundB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *a = testhost::FindBot ("RoundA");
  Bot *b = testhost::FindBot ("RoundB");
  HOST_REQUIRE (a != nullptr && b != nullptr);

  a->pev->health = 100.0f;
  b->pev->health = 100.0f;
  clients.Update ();

  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;

  // wins update the last winner for economics
  bots.SetLastWinner (Team::CT);
  SendText (nullptr, kTextMsg, 2, "#Terrorists_Win");
  CHECK (bots.GetLastWinner () == Team::Terrorist);
  SendText (nullptr, kTextMsg, 2, "#CTs_Win");
  CHECK (bots.GetLastWinner () == Team::CT);

  // commencing and unknown texts change nothing observable
  SendText (nullptr, kTextMsg, 2, "#Game_Commencing");
  CHECK (bots.GetLastWinner () == Team::CT);
  SendText (nullptr, kTextMsg, 2, "#Bogus_Text");
  CHECK (bots.GetLastWinner () == Team::CT);
  SendText (nullptr, kTextMsg, 0, "#CTs_Win"); // too short
  CHECK (bots.GetLastWinner () == Team::CT);

  // restart levels the money and the economics
  a->money_amount_ = 5000;
  b->money_amount_ = 100;
  SendText (nullptr, kTextMsg, 2, "#Round_Draw");
  CHECK (a->money_amount_ == mp_startmoney.As<int> ());
  CHECK (b->money_amount_ == mp_startmoney.As<int> ());
  CHECK (bots.GetTeamEconomics (Team::Terrorist));
  CHECK (bots.GetTeamEconomics (Team::CT));

  // the plant flips the bomb state and clears camp tasks
  CHECK (!game_state.IsBombPlanted ());
  a->StartTask (TaskId::Camp, TaskPri::camp, kInvalidNodeIndex, 0.0f, true);
  HOST_REQUIRE (a->GetTaskId () == TaskId::Camp);
  a->is_alive_ = true;
  b->is_alive_ = true;
  cv_radio_mode.Set (0); // keep the plant deterministic, no chatter rolls
  SendText (nullptr, kTextMsg, 2, "#Bomb_Planted");
  CHECK (game_state.IsBombPlanted ());
  CHECK (a->GetTaskId () != TaskId::Camp);
  SendText (nullptr, kTextMsg, 2, "#Bomb_Planted"); // already planted, no-op
  CHECK (game_state.IsBombPlanted ());

  // burst selectors ride the same message with a live carrier
  SendText (a->Ent (), kTextMsg, 2, "#Switch_To_BurstFire");
  CHECK (a->weapon_burst_mode_ == BurstMode::On);
  SendText (a->Ent (), kTextMsg, 2, "#Switch_To_SemiAuto");
  CHECK (a->weapon_burst_mode_ == BurstMode::Off);

  // team info tracks real teams, unknown strings are skipped
  const int32_t a_client = game.IndexOfPlayer (a->Ent ()) + 1;
  SendLongs (nullptr, kTeamInfo, { a_client });
  msgs.Start (nullptr, kTeamInfo);
  msgs.Collect (a_client);
  msgs.Collect ("CT");
  msgs.Stop ();
  CHECK (clients[a->Ent ()].team2 == Team::CT);
  CHECK (clients[a->Ent ()].team == Team::CT);

  msgs.Start (nullptr, kTeamInfo);
  msgs.Collect (a_client);
  msgs.Collect ("TERRORIST");
  msgs.Stop ();
  CHECK (clients[a->Ent ()].team2 == Team::Terrorist);

  msgs.Start (nullptr, kTeamInfo);
  msgs.Collect (a_client);
  msgs.Collect ("BOGUS");
  msgs.Stop ();
  CHECK (clients[a->Ent ()].team2 == Team::Terrorist);

  SendLongs (nullptr, kTeamInfo, { 99, 0 }); // out of range
  SendLongs (nullptr, kTeamInfo, { 0, 0 }); // negative index
  CHECK (clients[a->Ent ()].team2 == Team::Terrorist);

  // score info tracks the kd ratio through the client table
  a->pev->frags = 8.0f;
  SendLongs (nullptr, kScoreInfo, { a_client, 8, 4, 0, 0 });
  CHECK (a->kpd_ratio_ == 2.0f);
  CHECK (a->death_count_ == 4);
  SendLongs (nullptr, kScoreInfo, { 99, 8, 4, 0, 0 });
  SendLongs (nullptr, kScoreInfo, { 0, 8, 4, 0, 0 });
  CHECK (a->death_count_ == 4);

  // score attrib flips the vip flag
  SendLongs (nullptr, kScoreAttrib, { a_client, 4 });
  CHECK (a->is_vip_);
  SendLongs (nullptr, kScoreAttrib, { a_client, 0 });
  CHECK (!a->is_vip_);
  SendLongs (nullptr, kScoreAttrib, { 99, 4 });

  // death messages route into the manager
  const int32_t a_idx = game.IndexOfEntity (a->Ent ());
  const int32_t b_idx = game.IndexOfEntity (b->Ent ());
  SendLongs (nullptr, kDeathMsg, { a_idx, a_idx }); // suicide is ignored
  CHECK (a->is_alive_);
  SendLongs (nullptr, kDeathMsg, { a_idx, b_idx });
  CHECK (!b->is_alive_);
  CHECK (a->last_victim_ == b->Ent ());
  SendLongs (nullptr, kDeathMsg, { a_idx }); // too short
  CHECK (a->is_alive_);

  // hltv only restarts the round on the fov reset
  const float round_start = game_state.GetRoundStartTime ();
  SendLongs (nullptr, kHLTV, { 1, 1 });
  CHECK (game_state.GetRoundStartTime () == round_start);
  SendLongs (nullptr, kHLTV, { 0, 0 });
  CHECK (game_state.GetRoundStartTime () != round_start);

  // reset hud respawns the carrier and raises the flag
  a->is_alive_ = false;
  SendLongs (a->Ent (), kResetHUD, {});
  CHECK (game_state.IsResetHud ());
  SendLongs (nullptr, kResetHUD, {});
  CHECK (game_state.IsResetHud ());

  bots.Destroy ();
}

} // namespace bot
