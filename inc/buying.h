//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// buy states
namespace bot {

enum class BuyState : int32_t {
  PrimaryWeapon = 0,
  ArmorVestHelm,
  SecondaryWeapon,
  Ammo,
  DefusalKit,
  Grenades,
  NightVision,
  Done
};
YSTL_ENABLE_ENUM_ARITHMETIC (BuyState);

// buy context state
enum class BuyContext : int32_t {
  IsLegacy = (1 << 0),
  IsPistolRound = (1 << 1),
  IsFirstRound = (1 << 2),
  HasDefaultPistols = (1 << 3),
  HasPrimaryWeapon = (1 << 4),
  HasGoodEconomics = (1 << 5),
  None = 0,
};
YSTL_ENABLE_ENUM_FLAGS (BuyContext);

// economics limits
enum class EcoLimit : int32_t {
  PrimaryGreater = 0,
  SmgCTGreater,
  SmgTEGreater,
  ShotgunGreater,
  ShotgunLess,
  HeavyGreater,
  HeavyLess,
  ProstockNormal,
  ProstockRusher,
  ProstockCareful,
  ShieldGreater
};

} // namespace bot
