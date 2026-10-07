//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// defines for pickup items
namespace bot {

enum class Pickup : int32_t {
  None = 0,
  Weapon,
  DroppedC4,
  PlantedC4,
  Hostage,
  Button,
  Shield,
  DefusalKit,
  Items,
  AmmoAndKits
};

// famas/glock burst mode status + m4a1/usp silencer
enum class BurstMode : int32_t {
  On = ystl::bit (0),
  Off = ystl::bit (1)
};

// counter-strike weapon id's
enum class Weapon : int32_t {
  P228 = 1,
  Shield = 2,
  Scout = 3,
  Explosive = 4,
  XM1014 = 5,
  C4 = 6,
  MAC10 = 7,
  AUG = 8,
  Smoke = 9,
  Elite = 10,
  FiveSeven = 11,
  UMP45 = 12,
  SG550 = 13,
  Galil = 14,
  Famas = 15,
  USP = 16,
  Glock18 = 17,
  AWP = 18,
  MP5 = 19,
  M249 = 20,
  M3 = 21,
  M4A1 = 22,
  TMP = 23,
  G3SG1 = 24,
  Flashbang = 25,
  Deagle = 26,
  SG552 = 27,
  AK47 = 28,
  Knife = 29,
  P90 = 30,
  Armor = 31,
  ArmorHelm = 32,
  Defuser = 33,
  Invalid = 0
};
YSTL_ENABLE_ENUM_FLAGS (Weapon);

// counter strike weapon classes (types)
enum class WeaponType : int32_t {
  None,
  Melee,
  Pistol,
  Shotgun,
  ZoomRifle,
  Rifle,
  SMG,
  Sniper,
  Heavy
};

// reload state
enum class Reload : int32_t {
  None = 0, // no reload state currently
  Primary, // primary weapon reload state
  Secondary // secondary weapon reload state
};
YSTL_ENABLE_ENUM_ARITHMETIC (Reload);

// weapon team availability (for weapon mode settings)
enum class WeaponTeam : int32_t {
  None = -1, // not available for any team
  Terrorist = 0, // available for terrorist team only
  CT = 1, // available for ct team only
  Both = 2 // available for both teams
};

// weapon masks
constexpr auto kPrimaryWeaponMask =
  (ystl::bit (Weapon::XM1014) | ystl::bit (Weapon::M3) | ystl::bit (Weapon::MAC10) | ystl::bit (Weapon::UMP45) | ystl::bit (Weapon::MP5) |
    ystl::bit (Weapon::TMP) | ystl::bit (Weapon::P90) | ystl::bit (Weapon::AUG) | ystl::bit (Weapon::M4A1) | ystl::bit (Weapon::SG552) |
    ystl::bit (Weapon::AK47) | ystl::bit (Weapon::Scout) | ystl::bit (Weapon::SG550) | ystl::bit (Weapon::AWP) | ystl::bit (Weapon::G3SG1) |
    ystl::bit (Weapon::M249) | ystl::bit (Weapon::Famas) | ystl::bit (Weapon::Galil));

constexpr auto kSecondaryWeaponMask = (ystl::bit (Weapon::P228) | ystl::bit (Weapon::Elite) | ystl::bit (Weapon::USP) |
                                       ystl::bit (Weapon::Glock18) | ystl::bit (Weapon::Deagle) | ystl::bit (Weapon::FiveSeven));

constexpr auto kSniperWeaponMask = (ystl::bit (Weapon::Scout) | ystl::bit (Weapon::SG550) | ystl::bit (Weapon::AWP) | ystl::bit (Weapon::G3SG1));

// weapons < 7 are secondary
constexpr auto kPrimaryWeaponMinIndex = 7;

// grenade model names
inline constexpr ystl::StringRef kExplosiveModelName = "hegrenade.mdl";
inline constexpr ystl::StringRef kFlashbangModelName = "flashbang.mdl";
inline constexpr ystl::StringRef kSmokeModelName = "smokegrenade.mdl";

// weapon properties structure
struct WeaponProp {
  ystl::String classname {};
  int ammo1 {}; // ammo index for primary ammo
  int ammo1_max {}; // max primary ammo
  int slot {}; // hud slot (0 based)
  int pos {}; // slot position
  Weapon id {}; // weapon id
  int flags {}; // flags???
};

// weapon info structure
struct WeaponInfo {
  Weapon id {}; // the weapon id value
  ystl::StringRef name {}; // name of the weapon when selecting it
  ystl::StringRef model {}; // model name to separate cs weapons
  ystl::StringRef alias {}; // buy-menu alias of the weapon (ie. "usp", used by weapon restrictions)
  ystl::StringRef full_name {}; // full weapon name for display (ie. "hk usp .45 tactical")
  int price {}; // price when buying
  int min_primary_ammo {}; // minimum primary ammo
  WeaponTeam team_standard {}; // used by team (number) (standard map)
  WeaponTeam team_as {}; // used by team (as map)
  int buy_group {}; // group in buy menu (standard map)
  int buy_select {}; // select item in buy menu (standard map)
  int buy_select_t {}; // for counter-strike v1.6
  int buy_select_ct {}; // for counter-strike v1.6
  int penetrate_power {}; // penetrate power
  int max_clip {}; // max ammo in clip
  WeaponType type {}; // weapon class
  bool primary_fire_hold {}; // hold down primary fire button to use?
};

// bot reload data
struct ReloadData {
  Reload state {}; // current reload state
  bool is_reloading {}; // bot is reloading a gun
  ystl::CountdownTimer check_timer {}; // cooldown before next reload check

  // reset reload state
  void Reset () {
    Clear ();
    check_timer.invalidate ();
  }

  // clear state and flag
  void Clear () {
    state = Reload::None;
    is_reloading = false;
  }
};

// bot weapons data mixin
class WeaponsData {
  friend struct TestHook;
  friend struct BuyHook;
  friend struct CombatHook;
  friend struct VisionHook;
  friend struct TasksHook;
  friend struct BehaviorHook;
  friend struct WeaponsHook;

protected:
  // reload
  ReloadData reload_data_ {};

  // firing (except timelastfired)
  bool wants_to_fire_ {}; // bot needs consider firing
  float fire_pause_ {}; // time to pause firing
  float shoot_time_ {}; // time to shoot

  // weapon switching
  bool check_knife_switch_ {}; // is time to check switch to knife
  bool check_weapon_switch_ {}; // is time to check weapon switch
  ystl::IntervalTimer last_weapon_switch_timer_ {}; // time of last weapon switch to prevent rapid switching

  // zoom/shield
  ystl::CountdownTimer zoom_check_timer_ {}; // time to check zoom
  ystl::CountdownTimer shield_check_timer_ {}; // time to check shield drawing
  ystl::CountdownTimer sniper_stop_timer_ {}; // bot switched to other weapon

  // grenade
  ystl::CountdownTimer grenade_check_timer_ {}; // time to check grenade usage
  bool is_using_grenade_ {}; // bot currently using grenade
  ystl::Vector grenade_ {}; // calculated vector for grenades
  ystl::Vector throw_ {}; // origin of node to throw grenades

  // ammo pickup
  ystl::CountdownTimer no_ammo_pickup_timer_ {}; // cooldown to prevent weapon repick loop

public:
  // fields accessed from external code (message.cpp, control.cpp) weapon state
  Weapon current_weapon_ {}; // current weapon for each bot
  WeaponType weapon_type_ {}; // current weapon type
  BurstMode weapon_burst_mode_ {}; // burst mode (famas/glock18, silencer)

  Pickup pickup_type_ {}; // type of entity which needs to be used/picked up
  edict_t *pickup_item_ {}; // pointer to entity of item to use/pickup
  ystl::Array<edict_t *> ignored_items_ {}; // list of pointers to entity to ignore for pickup
  uint32_t dropped_dry_weapons_mask_ {}; // dry weapons swapped away, never worth re-picking

  // ammo
  ystl::FixedArray<int32_t, kMaxWeapons> ammo_in_clip_ {}; // ammo in clip for each weapons
  ystl::FixedArray<int32_t, MAX_AMMO_SLOTS> ammo_ {}; // total ammo amounts

  // firing
  ystl::IntervalTimer last_fired_timer_ {}; // time to last firing

  // equipment
  bool has_defuser_ {}; // does bot has defuser
  bool has_nvg_ {}; // does bot has nightvision goggles
  bool uses_nvg_ {}; // does nightvision goggles turned on
  bool has_c4_ {}; // does bot has c4 bomb
};

} // namespace bot
