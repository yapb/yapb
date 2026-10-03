//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

bool Bot::IsWeaponRestricted (Weapon wid) const {
  // this function checks for weapon restrictions

  auto val = cv_restricted_weapons.As<ystl::StringRef> ();

  if (val.empty ()) {
    return IsWeaponRestrictedAmx (wid); // no banned weapons
  }
  const auto &banned_weapons = val.split<ystl::String> (";");
  const auto &alias = util.WeaponIdToAlias (wid);

  for (const auto &ban : banned_weapons) {
    // check is this weapon is banned
    if (ban == alias) {
      return true;
    }
  }
  return IsWeaponRestrictedAmx (wid);
}

bool Bot::IsWeaponRestrictedAmx (Weapon wid) const {
  // this function checks restriction set by amx mod, this function code is courtesy of kwo

  if (!game.Is (GameFlags::Metamod)) {
    return false;
  }

  auto check_restriction = [&wid] (ystl::StringRef cvar, const int *data) -> bool {
    auto restricted_weapons = game.FindCvar (cvar);

    if (restricted_weapons.empty ()) {
      return false;
    }
    // find the weapon index
    const auto index = data[ystl::to_underlying (wid) - 1];

    // validate index range
    if (index < 0 || index >= static_cast<int> (restricted_weapons.size ())) {
      return false;
    }
    return restricted_weapons[static_cast<size_t> (index)] != '0';
  };

  // check for weapon restrictions
  if (has_flag (ystl::bit (wid), kPrimaryWeaponMask | kSecondaryWeaponMask | Weapon::Shield)) {
    constexpr int kIds[] = { 4, 25, 20, -1, 8, -1, 12, 19, -1, 5, 6, 13, 23, 17, 18, 1, 2, 21, 9, 24, 7, 16, 10, 22, -1, 3, 15, 14, 0, 11 };

    // verify restrictions
    return check_restriction ("amx_restrweapons", kIds);
  }

  // check for equipment restrictions
  else {
    constexpr int kIds[] = { -1, -1, -1, 3, -1, -1, -1, -1, 4, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 2, -1, -1, -1, -1, -1,
      0, 1, 5 };

    // verify restrictions
    return check_restriction ("amx_restrequipammo", kIds);
  }
}

bool Bot::CanReplaceWeapon () {
  // check if owned primary weapon can be replaced with current money

  const auto &ak47 = conf.GetWeapon (Weapon::AK47);

  // if bot is not rich enough or weapon mode disabled, return false
  if (!ak47 || ak47->team_standard == WeaponTeam::None || money_amount_ < 4000) {
    return false;
  }

  if (current_weapon_ == Weapon::Scout && money_amount_ > 5000) {
    return true;
  }
  else if (current_weapon_ == Weapon::MP5 && money_amount_ > 6000) {
    return true;
  }
  else if (UsesShotgun () && money_amount_ > 4000) {
    return true;
  }
  return IsWeaponRestricted (current_weapon_);
}

int Bot::PickBestWeapon (ystl::SmallArray<int> &vec, int money_save) const {
  // this function picks best available weapon from random choice with money save

  if (vec.size () < 2) {
    return vec.first ();
  }
  const bool need_more_random_weapon = (personality_ == Personality::Careful) || (rg.chance (25) && personality_ == Personality::Normal);

  if (need_more_random_weapon) {
    auto buy_factor =
      (static_cast<float> (money_amount_) - static_cast<float> (money_save)) / (16000.0f - static_cast<float> (money_save)) * 3.0f;

    if (buy_factor < 1.0f) {
      buy_factor = 1.0f;
    }
    // swap array values
    vec.reverse ();

    return vec[static_cast<int> (
      static_cast<float> (vec.size<int32_t> () - 1) * ystl::log10f (rg (1.0f, ystl::powf (10.0f, buy_factor))) / buy_factor + 0.5f)];
  }
  int chance = 95;

  // high skilled bots almost always prefer best weapon
  if (difficulty_ < Difficulty::Expert) {
    if (personality_ == Personality::Normal) {
      chance = 50;
    }
    else if (personality_ == Personality::Careful) {
      chance = 75;
    }
  }
  const auto &tab = conf.GetWeapons ();

  for (const auto &w : vec) {
    const auto &weapon = tab[w];

    // if we have enough money for weapon, buy it
    if (weapon.price + money_save < money_amount_ + rg (50, 200) && rg.chance (chance)) {
      return w;
    }
  }
  return vec.random ();
}

bool Bot::IsWeaponEligibleForPurchase (const WeaponInfo *weapon, Team team) const {
  // checks if a weapon is eligible for purchase based on various criteria

  // weapon available for every team?
  if (game.MapIs (MapFlags::Assassination) && weapon->team_as != WeaponTeam::Both &&
      weapon->team_as != static_cast<WeaponTeam> (ystl::to_underlying (team))) {
    return false;
  }

  // ignore weapon if this weapon not supported by currently running cs version
  if (has_flag (buy_context_, BuyContext::IsLegacy) && weapon->buy_select == -1) {
    return false;
  }

  // ignore weapon if this weapon is not targeted to out team
  if (weapon->team_standard != WeaponTeam::Both && weapon->team_standard != static_cast<WeaponTeam> (ystl::to_underlying (team))) {
    return false;
  }

  // ignore weapon if this weapon is restricted
  if (IsWeaponRestricted (weapon->id)) {
    return false;
  }
  return true;
}

bool Bot::PassesEconomicsCheck (const WeaponInfo *weapon, int prostock, int disrespect_economics_pct) const {
  // checks if weapon passes economics constraints

  bool ignore_weapon = false;

  if (team_ == Team::CT) {
    switch (weapon->id) {
    case Weapon::TMP:
    case Weapon::UMP45:
    case Weapon::P90:
    case Weapon::MP5:
      if (money_amount_ > conf.GetEconLimit (EcoLimit::SmgCTGreater) + prostock && rg.chance (disrespect_economics_pct)) {
        ignore_weapon = true;
      }
      break;

    default:
      break;
    }

    if (weapon->id == Weapon::Shield && money_amount_ > conf.GetEconLimit (EcoLimit::ShieldGreater) && rg.chance (disrespect_economics_pct)) {
      ignore_weapon = true;
    }
  }
  else if (team_ == Team::Terrorist) {
    switch (weapon->id) {
    case Weapon::UMP45:
    case Weapon::MAC10:
    case Weapon::P90:
    case Weapon::MP5:
    case Weapon::Scout:
      if (money_amount_ > conf.GetEconLimit (EcoLimit::SmgTEGreater) + prostock && rg.chance (disrespect_economics_pct)) {
        ignore_weapon = true;
      }
      break;

    default:
      break;
    }
  }

  switch (weapon->id) {
  case Weapon::XM1014:
  case Weapon::M3:
    if (money_amount_ < conf.GetEconLimit (EcoLimit::ShotgunLess) && rg.chance (disrespect_economics_pct)) {
      ignore_weapon = true;
    }

    if (money_amount_ >= conf.GetEconLimit (EcoLimit::ShotgunGreater)) {
      ignore_weapon = false;
    }
    break;

  default:
    break;
  }

  switch (weapon->id) {
  case Weapon::SG550:
  case Weapon::G3SG1:
  case Weapon::AWP:
  case Weapon::M249:
    if (money_amount_ < conf.GetEconLimit (EcoLimit::HeavyLess) && rg.chance (85)) {
      ignore_weapon = true;
    }

    if (money_amount_ >= conf.GetEconLimit (EcoLimit::HeavyGreater)) {
      ignore_weapon = false;
    }
    break;

  default:
    break;
  }
  return !ignore_weapon;
}

int Bot::GetProstockLimit () const {
  // returns prostock limit based on personality

  switch (personality_) {
  case Personality::Rusher:
    return conf.GetEconLimit (EcoLimit::ProstockRusher);

  case Personality::Careful:
    return conf.GetEconLimit (EcoLimit::ProstockCareful);

  case Personality::Normal:
  default:
    return conf.GetEconLimit (EcoLimit::ProstockNormal);
  }
}

WeaponInfo *Bot::SelectBuyPrimary (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab, int money_save) {
  // selects a primary weapon based on preferences and economics

  ystl::SmallArray<int32_t> choices {};

  const auto prostock = GetProstockLimit ();
  const auto disrespect_economics_pct = 100 - cv_economics_disrespect_percent.As<int> ();

  for (int i = kNumWeapons - 1; i >= 0 && choices.size () < 4; --i) {
    const auto selected_weapon = &tab[pref[i]];

    if (selected_weapon->buy_group == 1) {
      continue;
    }

    // check weapon eligibility
    if (!IsWeaponEligibleForPurchase (selected_weapon, team_)) {
      continue;
    }

    // check economics
    if (!PassesEconomicsCheck (selected_weapon, prostock, disrespect_economics_pct)) {
      const auto ak47 = conf.GetWeapon (Weapon::AK47);
      if (ak47 && ak47->team_standard != WeaponTeam::None && cv_economics_rounds) {
        continue;
      }
    }

    // check if bot can afford the weapon
    if (selected_weapon->price <= (money_amount_ - money_save)) {
      choices.emplace (pref[i]);
    }
  };

  // found a desired weapon?
  if (!choices.empty ()) {
    return const_cast<WeaponInfo *> (&tab[PickBestWeapon (choices, money_save)]);
  }
  return nullptr;
}

WeaponInfo *Bot::SelectBuySecondary (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
  // selects a secondary weapon based on preferences

  ystl::SmallArray<int32_t> choices {};

  for (int i = kNumWeapons - 1; i >= 0 && choices.size () < 4; --i) {
    const auto selected_weapon = &tab[pref[i]];

    if (selected_weapon->buy_group != 1) {
      continue;
    }

    // check weapon eligibility
    if (!IsWeaponEligibleForPurchase (selected_weapon, team_)) {
      continue;
    }

    // check if bot can afford the weapon
    if (selected_weapon->price <= (money_amount_ - rg (100, 200))) {
      choices.emplace (pref[i]);
    }
  }

  // found a desired weapon?
  if (!choices.empty ()) {
    return const_cast<WeaponInfo *> (&tab[PickBestWeapon (choices, rg (100, 200))]);
  }
  return nullptr;
}

void Bot::IssueBuyCommand (const WeaponInfo *weapon) {
  // issues the buy command for a weapon

  const bool is_legacy = has_flag (buy_context_, BuyContext::IsLegacy);

  if (weapon == nullptr) {
    return;
  }
  IssueCommand ("buy;menuselect %d", weapon->buy_group);

  if (is_legacy) {
    IssueCommand ("menuselect %d", weapon->buy_select);
  }
  else {
    if (team_ == Team::Terrorist) {
      IssueCommand ("menuselect %d", weapon->buy_select_t);
    }
    else {
      IssueCommand ("menuselect %d", weapon->buy_select_ct);
    }
  }
}

void Bot::BuyPrimaryWeapon (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
  // buys primary weapon if needed and affordable

  const bool has_primary = has_flag (buy_context_, BuyContext::HasPrimaryWeapon);
  const bool has_good_economics = has_flag (buy_context_, BuyContext::HasGoodEconomics);
  const bool has_shield_val = HasShield ();

  // if no primary weapon and bot has some money, buy a primary weapon
  if ((!has_shield_val && !has_primary && has_good_economics) || (has_good_economics && CanReplaceWeapon ())) {
    int money_save = 0;

    // save money for grenade for example?
    money_save = rg (500, 1000);

    if (bots.GetLastWinner () == team_) {
      money_save = 0;
    }
    auto selected_weapon = SelectBuyPrimary (pref, tab, money_save);

    if (selected_weapon != nullptr) {
      IssueBuyCommand (selected_weapon);
    }
  }
  else if (has_primary && !has_shield_val) {
    reload_data_.state = Reload::Primary;
  }
  else if ((HasSecondaryWeapon () && !has_shield_val) || has_shield_val) {
    reload_data_.state = Reload::Secondary;
  }
}

void Bot::BuyArmor () {
  // buys armor if damaged and affordable

  const bool is_pistol_round = has_flag (buy_context_, BuyContext::IsPistolRound);
  const bool has_primary = has_flag (buy_context_, BuyContext::HasPrimaryWeapon);
  const bool has_good_economics = has_flag (buy_context_, BuyContext::HasGoodEconomics);

  if (pev->armorvalue < rg (50.0f, 80.0f) && has_good_economics && (is_pistol_round || (has_good_economics && has_primary))) {

    // if bot is rich, buy kevlar + helmet, else buy a single kevlar
    if (money_amount_ > 1500 && !IsWeaponRestricted (Weapon::ArmorHelm)) {
      IssueCommand ("buyequip;menuselect 2");
    }
    else if (!IsWeaponRestricted (Weapon::Armor)) {
      IssueCommand ("buyequip;menuselect 1");
    }
  }
}

void Bot::BuySecondaryWeapon (const ystl::SmallArray<int32_t> &pref, const ystl::SmallArray<WeaponInfo> &tab) {
  // buys secondary weapon if affordable

  const bool is_pistol_round = has_flag (buy_context_, BuyContext::IsPistolRound);
  const bool is_first_round = has_flag (buy_context_, BuyContext::IsFirstRound);
  const bool has_default_pistols = has_flag (buy_context_, BuyContext::HasDefaultPistols);
  const bool has_primary = has_flag (buy_context_, BuyContext::HasPrimaryWeapon);

  const bool team_won_last_round = bots.GetLastWinner () == team_;

  // check conditions for buying secondary weapon
  bool should_buy_secondary = false;

  if (is_pistol_round) {
    should_buy_secondary = true;
  }
  else if (is_first_round && has_default_pistols && rg.chance (60)) {
    should_buy_secondary = true;
  }
  else if (has_default_pistols && team_won_last_round && money_amount_ > rg (2000, 3000)) {
    should_buy_secondary = true;
  }
  else if (has_primary && has_default_pistols && money_amount_ > rg (7500, 9000)) {
    should_buy_secondary = true;
  }

  if (!should_buy_secondary) {
    return;
  }
  auto selected_weapon = SelectBuySecondary (pref, tab);

  if (selected_weapon != nullptr) {
    IssueBuyCommand (selected_weapon);
  }
}

void Bot::BuyAmmo () {
  // buys ammo for weapons

  if (!ignore_buy_delay_) {
    for (int i = 0; i < 7; ++i) {
      IssueCommand ("buyammo%d", rg (1, 2)); // simulate human
    }
  }

  // buy enough ammo
  if (HasPrimaryWeapon ()) {
    IssueCommand ("buy;menuselect 6");
  }

  // buy enough ammo for secondary
  if (HasSecondaryWeapon ()) {
    IssueCommand ("buy;menuselect 7");
  }

  // try to reload secondary weapon
  if (reload_data_.state != Reload::Primary) {
    reload_data_.state = Reload::Secondary;
  }
}

void Bot::BuyGrenades () {
  // buys grenades if affordable and allowed

  if (!has_flag (buy_context_, BuyContext::HasGoodEconomics)) {
    return;
  }

  // buy a he grenade
  if (conf.ChanceToBuyGrenade (0) && money_amount_ >= 400 && !IsWeaponRestricted (Weapon::Explosive)) {
    IssueCommand ("buyequip");
    IssueCommand ("menuselect 4");
  }

  // buy a concussion grenade, i.e., 'flashbang'
  if (conf.ChanceToBuyGrenade (1) && money_amount_ >= 300 && !IsWeaponRestricted (Weapon::Flashbang)) {
    IssueCommand ("buyequip");
    IssueCommand ("menuselect 3");
  }

  // buy a smoke grenade
  if (conf.ChanceToBuyGrenade (2) && money_amount_ >= 400 && !IsWeaponRestricted (Weapon::Smoke)) {
    IssueCommand ("buyequip");
    IssueCommand ("menuselect 5");
  }
}

void Bot::BuyDefusalKit () {
  // buys defusal kit on bomb maps for ct

  const bool is_legacy = has_flag (buy_context_, BuyContext::IsLegacy);

  if (game.MapIs (MapFlags::Demolition) && team_ == Team::CT && rg.chance (80) && money_amount_ > 200 && !IsWeaponRestricted (Weapon::Defuser)) {

    if (is_legacy) {
      IssueCommand ("buyequip;menuselect 6");
    }
    else {
      IssueCommand ("defuser"); // use alias in steamcs
    }
  }
}

void Bot::BuyNightVision () {
  // buys night vision goggles if needed

  const bool is_bot_economics_good = has_flag (buy_context_, BuyContext::HasGoodEconomics);
  const bool is_legacy = has_flag (buy_context_, BuyContext::IsLegacy);

  if (is_bot_economics_good && money_amount_ > 2500 && !has_nvg_ && rg.chance (30) && path_) {
    const float sky_color = illum.GetSkyColor ();
    const float light_level = path_->light;

    // if it's somewhat dark, do buy nightvision goggles
    if ((sky_color >= 50.0f && light_level <= 15.0f) || (sky_color < 50.0f && light_level < 40.0f)) {
      if (is_legacy) {
        IssueCommand ("buyequip;menuselect 7");
      }
      else {
        IssueCommand ("nvgs"); // use alias in steamcs
      }
    }
  }
}

void Bot::BuyWeapons () {
  // this function does all the work in selecting correct buy menus for most weapons/items

  if (!ignore_buy_delay_) {
    next_buy_timer_.start (rg (0.3f, 0.5f));
  }
  else {
    next_buy_timer_.start (0.0f);
  }

  // select the priority tab for this personality
  const auto &pref = conf.GetWeaponPrefs (personality_);
  const auto &tab = conf.GetWeapons ();

  // build buy context from individual flags
  BuyContext ctx = BuyContext::None;

  // game version flag
  if (game.Is (GameFlags::Legacy)) {
    ctx |= BuyContext::IsLegacy;
  }

  // economics flag
  if (bots.GetTeamEconomics (team_)) {
    ctx |= BuyContext::HasGoodEconomics;
  }

  // round type flags
  const auto ak47 = conf.GetWeapon (Weapon::AK47);
  const auto deagle = conf.GetWeapon (Weapon::Deagle);

  if (ak47 && deagle && ak47->team_standard == WeaponTeam::None && deagle->team_standard == WeaponTeam::CT) {
    ctx |= BuyContext::IsPistolRound;
  }

  // first round of the game
  if (money_amount_ == mp_startmoney.As<int> ()) {
    ctx |= BuyContext::IsFirstRound;
  }

  // bot has glock18 and usp only
  if (has_flag (pev->weapons, ystl::bit (Weapon::USP) | ystl::bit (Weapon::Glock18))) {
    ctx |= BuyContext::HasDefaultPistols;
  }

  // has primary weapon
  if (HasPrimaryWeapon ()) {
    ctx |= BuyContext::HasPrimaryWeapon;
  }
  buy_context_ = ctx;

  switch (buy_state_) {
  case BuyState::PrimaryWeapon:
    BuyPrimaryWeapon (pref, tab);
    break;

  case BuyState::ArmorVestHelm:
    BuyArmor ();
    break;

  case BuyState::SecondaryWeapon:
    BuySecondaryWeapon (pref, tab);
    break;

  case BuyState::Ammo:
    BuyAmmo ();
    break;

  case BuyState::Grenades:
    BuyGrenades ();
    break;

  case BuyState::DefusalKit:
    BuyDefusalKit ();
    break;

  case BuyState::NightVision:
    BuyNightVision ();
    break;

  default:
    break;
  }

  ++buy_state_;
  PushMsgQueue (Msg::Buy);
}

void Bot::EnteredBuyZone (BuyState buy_state) {
  // this function is gets called when bot enters a buyzone, to allow bot to buy some stuff

  if (is_creature_ || has_hostage_) {
    return; // creatures can't buy anything
  }

  // if bot is in buy zone, try to buy ammo for this weapon
  if (see_enemy_timer_.greater_than (12.0f) && last_equip_timer_.greater_than (30.0f) && in_buy_zone_ &&
      (game_state.GetRoundStartTime () + rg (10.0f, 20.0f) + mp_buytime.As<float> () < game.Time ()) && !game_state.IsBombPlanted () &&
      money_amount_ > conf.GetEconLimit (EcoLimit::PrimaryGreater)) {

    ignore_buy_delay_ = true;
    buying_finished_ = false;
    buy_state_ = buy_state;

    // push buy message
    PushMsgQueue (Msg::Buy);

    next_buy_timer_.start (0.0f);
    last_equip_timer_.start ();
  }
}

} // namespace bot
