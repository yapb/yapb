//
// YaPB test host: unit/buying_{select,flow}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for buying.cpp through a test-only hook (friend, no
// production behavior changes): restriction/eligibility/economics gates,
// weapon selectors on crafted tables, exact buy-command strings, and the
// full buy state machine. Randomness is pinned via extreme chances and
// single-candidate tables, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct BuyHook {
  static bool Restricted (Bot &bot, Weapon id) {
    return bot.IsWeaponRestricted (id);
  }
  static bool RestrictedAmx (Bot &bot, Weapon id) {
    return bot.IsWeaponRestrictedAmx (id);
  }
  static bool CanReplace (Bot &bot) {
    return bot.CanReplaceWeapon ();
  }
  static int PickBest (Bot &bot, ystl::SmallArray<int> &vec, int money_save) {
    return bot.PickBestWeapon (vec, money_save);
  }
  static bool Eligible (Bot &bot, const WeaponInfo *weapon, Team team) {
    return bot.IsWeaponEligibleForPurchase (weapon, team);
  }
  static bool Economics (Bot &bot, const WeaponInfo *weapon, int prostock, int pct) {
    return bot.PassesEconomicsCheck (weapon, prostock, pct);
  }
  static int Prostock (Bot &bot) {
    return bot.GetProstockLimit ();
  }
  static WeaponInfo *SelectPrimary (Bot &bot, const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab, int money_save) {
    return bot.SelectBuyPrimary (pref, tab, money_save);
  }
  static WeaponInfo *SelectSecondary (Bot &bot, const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
    return bot.SelectBuySecondary (pref, tab);
  }
  static void BuyCommand (Bot &bot, const WeaponInfo *weapon) {
    bot.IssueBuyCommand (weapon);
  }
  static void BuyPrimary (Bot &bot, const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
    bot.BuyPrimaryWeapon (pref, tab);
  }
  static void BuySecondary (Bot &bot, const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
    bot.BuySecondaryWeapon (pref, tab);
  }
  static void BuyArmor (Bot &bot) {
    bot.BuyArmor ();
  }
  static void BuyAmmo (Bot &bot) {
    bot.BuyAmmo ();
  }
  static void BuyGrenades (Bot &bot) {
    bot.BuyGrenades ();
  }
  static void BuyDefusal (Bot &bot) {
    bot.BuyDefusalKit ();
  }
  static void BuyNvg (Bot &bot) {
    bot.BuyNightVision ();
  }
  static void BuyWeapons (Bot &bot) {
    bot.BuyWeapons ();
  }
  static void EnterZone (Bot &bot, BuyState state) {
    bot.EnteredBuyZone (state);
  }
  static Reload ReloadState (Bot &bot) {
    return bot.reload_data_.state;
  }
  static size_t QueueLength (Bot &bot) {
    return bot.msg_queue_.size ();
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
};

namespace {

// four-node chain, dense numbering matches indices
void BuildBuyGraph () {
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
  graph.PopulateNodes ();
  planner.Init ();
}

void BootBuy (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildBuyGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);

  // economics chances at zero: every chance branch takes the deterministic leg
  cv_economics_disrespect_percent.Set (100);
  cv_restricted_weapons.Set ("");
}

// crafted table: exactly one affordable primary (slot 3) and one
// affordable secondary (slot 5), everything else priced out
struct BuyFixture {
  ystl::SmallArray<int32_t> pref {};
  ystl::SmallArray<WeaponInfo> tab {};

  BuyFixture () {
    for (int i = 0; i < kNumWeapons; ++i) {
      pref.push (i);

      WeaponInfo info {};
      info.id = static_cast<Weapon> (i);
      info.price = 99999;
      info.team_standard = WeaponTeam::Both;
      info.team_as = WeaponTeam::Both;
      info.buy_group = 4;
      info.buy_select = 3;
      info.buy_select_t = 2;
      info.buy_select_ct = 3;
      tab.push (info);
    }

    auto &rifle = tab[3];
    rifle.id = Weapon::AK47;
    rifle.price = 2500;
    rifle.buy_group = 4;

    auto &pistol = tab[5];
    pistol.id = Weapon::Deagle;
    pistol.price = 650;
    pistol.buy_group = 1;
  }
};

// every ';'-separated part of issueCommand lands in the gamedll as one
// ClientCommand (menu handlers skip fake clients), so dispatch counts pin
// the command structure: "buy;menuselect N" is two dispatches
} // namespace

TEST_CASE ("unit/buying_select") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBuy (engine, cs);

  bots.Addbot ("BuyS", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("BuyS");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;

  // no bans, no metamod: nothing is restricted...
  CHECK (!BuyHook::Restricted (*bot, Weapon::AK47));
  CHECK (!BuyHook::RestrictedAmx (*bot, Weapon::AK47));

  // ...a listed alias bans exactly that weapon
  const ystl::String ak_alias = util.WeaponIdToAlias (Weapon::AK47);
  cv_restricted_weapons.Set (ak_alias.chars ());
  CHECK (BuyHook::Restricted (*bot, Weapon::AK47));
  CHECK (!BuyHook::Restricted (*bot, Weapon::AWP));
  cv_restricted_weapons.Set ("deagle;awp");
  CHECK (BuyHook::Restricted (*bot, Weapon::Deagle));
  CHECK (!BuyHook::Restricted (*bot, Weapon::AK47));
  cv_restricted_weapons.Set ("");

  // replacement needs money first, then a downgrade worth swapping
  bot->money_amount_ = 3000;
  bot->current_weapon_ = Weapon::AK47;
  CHECK (!BuyHook::CanReplace (*bot));
  bot->money_amount_ = 10000;
  bot->current_weapon_ = Weapon::Scout;
  CHECK (BuyHook::CanReplace (*bot));
  bot->current_weapon_ = Weapon::MP5;
  bot->money_amount_ = 7000;
  CHECK (BuyHook::CanReplace (*bot));
  bot->current_weapon_ = Weapon::M3;
  bot->weapon_type_ = WeaponType::Shotgun;
  bot->money_amount_ = 5000;
  CHECK (BuyHook::CanReplace (*bot));
  bot->current_weapon_ = Weapon::AK47;
  bot->weapon_type_ = WeaponType::Rifle;
  bot->money_amount_ = 10000;
  CHECK (!BuyHook::CanReplace (*bot));

  // single-candidate picks are exact, no rolls involved
  ystl::SmallArray<int> solo {};
  solo.push (7);
  CHECK (BuyHook::PickBest (*bot, solo, 0) == 7);

  // eligibility is pure: team, legacy select and bans decide
  BuyFixture fx {};
  CHECK (BuyHook::Eligible (*bot, &fx.tab[3], Team::Terrorist));
  fx.tab[3].team_standard = WeaponTeam::CT;
  CHECK (!BuyHook::Eligible (*bot, &fx.tab[3], Team::Terrorist));
  CHECK (BuyHook::Eligible (*bot, &fx.tab[3], Team::CT));
  fx.tab[3].team_standard = WeaponTeam::Both;

  bot->buy_context_ = BuyContext::IsLegacy;
  fx.tab[3].buy_select = -1;
  CHECK (!BuyHook::Eligible (*bot, &fx.tab[3], Team::Terrorist));
  fx.tab[3].buy_select = 3;
  CHECK (BuyHook::Eligible (*bot, &fx.tab[3], Team::Terrorist));
  bot->buy_context_ = BuyContext::None;

  cv_restricted_weapons.Set (ak_alias.chars ());
  CHECK (!BuyHook::Eligible (*bot, &fx.tab[3], Team::Terrorist));
  cv_restricted_weapons.Set ("");

  // economics with zeroed chances: smg/shotgun/heavy money lines decide
  WeaponInfo tmp {};
  tmp.id = Weapon::TMP;
  bot->team_ = Team::CT;
  bot->money_amount_ = conf.GetEconLimit (EcoLimit::SmgCTGreater) + 5000;
  CHECK (BuyHook::Economics (*bot, &tmp, 0, 0));
  bot->team_ = Team::Terrorist;

  WeaponInfo m3 {};
  m3.id = Weapon::M3;
  bot->money_amount_ = conf.GetEconLimit (EcoLimit::ShotgunLess) - 100;
  CHECK (BuyHook::Economics (*bot, &m3, 0, 0));
  bot->money_amount_ = conf.GetEconLimit (EcoLimit::ShotgunGreater);
  CHECK (BuyHook::Economics (*bot, &m3, 0, 0));

  WeaponInfo awp {};
  awp.id = Weapon::AWP;
  bot->money_amount_ = conf.GetEconLimit (EcoLimit::HeavyGreater);
  CHECK (BuyHook::Economics (*bot, &awp, 0, 0));

  // full disrespect flips the smg branch
  bot->team_ = Team::CT;
  bot->money_amount_ = conf.GetEconLimit (EcoLimit::SmgCTGreater) + 5000;
  CHECK (!BuyHook::Economics (*bot, &tmp, 0, 100));
  bot->team_ = Team::Terrorist;

  // prostock follows the personality switch
  bot->personality_ = Personality::Rusher;
  CHECK (BuyHook::Prostock (*bot) == conf.GetEconLimit (EcoLimit::ProstockRusher));
  bot->personality_ = Personality::Careful;
  CHECK (BuyHook::Prostock (*bot) == conf.GetEconLimit (EcoLimit::ProstockCareful));
  bot->personality_ = Personality::Normal;
  CHECK (BuyHook::Prostock (*bot) == conf.GetEconLimit (EcoLimit::ProstockNormal));

  // selectors resolve the single affordable candidate each
  bot->money_amount_ = 3000;
  WeaponInfo *primary = BuyHook::SelectPrimary (*bot, fx.pref, fx.tab, 0);
  HOST_REQUIRE (primary != nullptr);
  CHECK (primary->id == Weapon::AK47);

  bot->money_amount_ = 100;
  CHECK (BuyHook::SelectPrimary (*bot, fx.pref, fx.tab, 0) == nullptr);

  bot->money_amount_ = 16000;
  WeaponInfo *secondary = BuyHook::SelectSecondary (*bot, fx.pref, fx.tab);
  HOST_REQUIRE (secondary != nullptr);
  CHECK (secondary->id == Weapon::Deagle);

  bot->money_amount_ = 0;
  CHECK (BuyHook::SelectSecondary (*bot, fx.pref, fx.tab) == nullptr);

  bots.Destroy ();
}

TEST_CASE ("unit/buying_flow") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootBuy (engine, cs);

  bots.Addbot ("BuyF", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("BuyF2", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *bot = testhost::FindBot ("BuyF");
  Bot *wing = testhost::FindBot ("BuyF2");
  HOST_REQUIRE (bot != nullptr && wing != nullptr);
  bot->team_ = Team::Terrorist;
  wing->team_ = Team::Terrorist;

  BuyFixture fx {};
  const int base = testhost::CsCalls (cs, "ClientCommand");

  // null weapons buy nothing, real ones dispatch buy plus menuselect
  BuyHook::BuyCommand (*bot, nullptr);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base);

  // buy plus the follow-up menuselect: three dispatches per weapon
  BuyHook::BuyCommand (*bot, &fx.tab[3]);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 3);

  bot->buy_context_ = BuyContext::IsLegacy;
  BuyHook::BuyCommand (*bot, &fx.tab[3]);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 6);
  bot->buy_context_ = BuyContext::None;

  // primary buy with no history falls into the reload lane instead
  bot->buy_context_ = BuyContext::HasPrimaryWeapon;
  BuyHook::BuyPrimary (*bot, fx.pref, fx.tab);
  CHECK (BuyHook::ReloadState (*bot) == Reload::Primary);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 6);

  // replacement path buys the single affordable rifle
  bot->buy_context_ = BuyContext::HasGoodEconomics;
  bot->money_amount_ = 10000;
  bot->current_weapon_ = Weapon::Scout;
  BuyHook::BuyPrimary (*bot, fx.pref, fx.tab);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 9);

  // armor respects the damage line and the wallet
  bot->pev->armorvalue = 100.0f;
  BuyHook::BuyArmor (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 9);

  bot->pev->armorvalue = 0.0f;
  bot->money_amount_ = 2000;
  bot->buy_context_ = BuyContext::HasGoodEconomics | BuyContext::IsPistolRound;
  BuyHook::BuyArmor (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 11);

  bot->money_amount_ = 1000;
  BuyHook::BuyArmor (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == base + 13);

  // secondary only moves on pistol rounds here
  bot->buy_context_ = BuyContext::None;
  const int armor_cmds = testhost::CsCalls (cs, "ClientCommand");
  BuyHook::BuySecondary (*bot, fx.pref, fx.tab);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == armor_cmds);

  bot->buy_context_ = BuyContext::IsPistolRound;
  bot->money_amount_ = 16000;
  BuyHook::BuySecondary (*bot, fx.pref, fx.tab);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == armor_cmds + 3);

  // secondary lane parks the reload state without spending
  bot->buy_context_ = BuyContext::None;
  bot->money_amount_ = 0;
  bot->pev->weapons = ystl::to_underlying (kSecondaryWeaponMask);
  BuyHook::BuyPrimary (*bot, fx.pref, fx.tab);
  CHECK (BuyHook::ReloadState (*bot) == Reload::Secondary);

  // ammo buys per owned class and keeps the reload lane
  bot->ignore_buy_delay_ = true;
  bot->pev->weapons = 0;
  const int ammo_base = testhost::CsCalls (cs, "ClientCommand");
  BuyHook::BuyAmmo (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base);
  CHECK (BuyHook::ReloadState (*bot) == Reload::Secondary);

  bot->pev->weapons = ystl::to_underlying (kPrimaryWeaponMask) | ystl::to_underlying (kSecondaryWeaponMask);
  BuyHook::BuyAmmo (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base + 4);

  // grenades and kits stay quiet without money or a map for them
  bot->buy_context_ = BuyContext::None;
  BuyHook::BuyGrenades (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base + 4);

  bot->buy_context_ = BuyContext::HasGoodEconomics;
  bot->money_amount_ = 0;
  BuyHook::BuyGrenades (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base + 4);

  BuyHook::BuyDefusal (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base + 4);

  bot->money_amount_ = 1000;
  BuyHook::BuyNvg (*bot);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == ammo_base + 4);

  // broke bots walk the whole machine with no wallet chatter
  bot->ignore_buy_delay_ = false;
  bot->money_amount_ = 0;
  wing->money_amount_ = 0;
  bot->buy_state_ = BuyState::PrimaryWeapon;
  CHECK (bots.GetLastWinner () == Team::Invalid); // no winner by default
  bots.SetLastWinner (Team::CT); // make Terrorists lose, so poor team saves
  bots.UpdateTeamEconomics (Team::Terrorist, false);
  HOST_REQUIRE (!bots.GetTeamEconomics (Team::Terrorist));
  bot->pev->weapons = 0;
  const int flow_base = testhost::CsCalls (cs, "ClientCommand");
  const size_t queue_base = BuyHook::QueueLength (*bot);

  for (int step = 0; step < 7; ++step) {
    BuyHook::BuyWeapons (*bot);
  }
  CHECK (bot->buy_state_ == BuyState::Done);

  // seven blind buyammo rolls plus nothing else affordable
  CHECK (testhost::CsCalls (cs, "ClientCommand") == flow_base + 7);
  CHECK (BuyHook::QueueLength (*bot) == queue_base + 7);
  CHECK (BuyHook::ReloadState (*bot) == Reload::Secondary);

  // buy zones ignore creatures and idle wallets alike
  BuyHook::SetCreature (*bot, true);
  BuyHook::EnterZone (*bot, BuyState::PrimaryWeapon);
  BuyHook::SetCreature (*bot, false);
  bot->has_hostage_ = true;
  BuyHook::EnterZone (*bot, BuyState::PrimaryWeapon);
  bot->has_hostage_ = false;
  CHECK (BuyHook::QueueLength (*bot) == queue_base + 7);
  CHECK (testhost::CsCalls (cs, "ClientCommand") == flow_base + 7);

  bots.Destroy ();
}

} // namespace bot
