//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// botname structure definition
namespace bot {

struct Name {
  ystl::String name {};
  int used_by = -1;

public:
  Name () = default;
  Name (ystl::StringRef name, int used_by) : name (name), used_by (used_by) {}
};

// voice config structure definition
struct ChatterItem {
  ystl::String name {};
  float repeat {};
  float duration {};

public:
  ChatterItem (ystl::StringRef name, float repeat, float duration) : name (name), repeat (repeat), duration (duration) {}
};

// result of reading a config file, with change tracking against the last successfully applied load
enum class ConfigRead : uint8_t {
  Missing, // file not found or rejected, run the fallback path
  Unchanged, // byte-identical to the last applied load, caller may skip re-parsing
  Ready // fresh content in outtext, parse and apply, then markconfigapplied()
};

// content fingerprint of the last successfully applied config load
struct ConfigSnapshot {
  ystl::String name {};
  uint32_t hash {};
  size_t size {};
};

// slot count for fire delay tables indexed by skill bucket
constexpr auto kFireDelaySlots = 6;

// semi-auto trigger delays, overridden by weapon.cfg firedelay block
struct FireDelayData {
  float pistol_base = 0.05f; // base delay added to every pistol shot
  float other_base = 0.10f; // base delay for other semi-auto weapons
  float deagle_extra = 0.08f; // extra delay for deagle's heavier cycle
  float pistol_min[kFireDelaySlots] = { 0.02f, 0.04f, 0.06f, 0.08f, 0.12f, 0.16f };
  float pistol_max[kFireDelaySlots] = { 0.08f, 0.10f, 0.13f, 0.16f, 0.20f, 0.26f };
  float other_min[kFireDelaySlots] = { 0.0f, 0.1f, 0.2f, 0.3f, 0.4f, 0.6f };
  float other_max[kFireDelaySlots] = { 0.1f, 0.2f, 0.3f, 0.4f, 0.5f, 0.7f };
};

// mostly config stuff, and some stuff dealing with menus
class Config final : public ystl::Singleton<Config> {
public:
  friend struct ConfigHook;

private:
  // fingerprints of applied loads to skip re-parsing unchanged configs
  ystl::Array<ConfigSnapshot> config_snapshots_ {};

  ystl::SmallArray<ystl::Array<ystl::String>> chat_ {};
  ystl::SmallArray<ystl::SmallArray<size_t>> chat_order_ {}; // shuffled index cycle per bank
  ystl::SmallArray<size_t> chat_pos_ {}; // next position in cycle per bank
  ystl::Array<ystl::Array<ChatterItem>> chatter_ {};

  ystl::Array<Name> bot_names_ {};
  ystl::Array<ChatKeywords> replies_ {};
  ystl::HashMap<uint32_t, ystl::SmallArray<size_t>> keyword_index_ {}; // keyword hash -> reply indices for fast lookup
  ystl::SmallArray<WeaponInfo> weapons_ {};
  ystl::SmallArray<WeaponProp> weapon_props_ {};

  ystl::Array<int32_t> logos_indices_ {};
  ystl::Array<ystl::String> avatars_ {};

  ystl::HashMap<uint32_t, ystl::String, ystl::Hash<int32_t>> language_ {};
  ystl::HashMap<uint32_t, ystl::String, ystl::Hash<int32_t>> missing_translations_ {}; // untranslated strings, collected at runtime
  ystl::HashMap<Difficulty, DifficultyData> difficulty_ {};
  ystl::HashMap<ystl::String, ystl::String> custom_ {};

  // parsed gamedef.cfg document; keeps string storage alive for weapon info and sound templates
  ystl::ConfNode gamedef_ {};

  // default tables for personality weapon preferences, overridden by weapon.cfg
  ystl::SmallArray<int32_t> normal_weapon_prefs_ = { 0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 20, 21, 9, 19, 15, 17, 18, 14,
    16 };
  ystl::SmallArray<int32_t> rusher_weapon_prefs_ = { 0, 2, 4, 5, 1, 6, 3, 24, 25, 22, 23, 20, 10, 12, 13, 7, 8, 21, 11, 9, 15, 19, 17, 18, 16,
    14 };
  ystl::SmallArray<int32_t> careful_weapon_prefs_ = { 0, 2, 1, 4, 5, 6, 3, 7, 8, 12, 10, 13, 11, 9, 18, 17, 15, 19, 16, 14, 20, 22, 25, 23, 24,
    21 };
  ystl::SmallArray<int32_t> bot_buy_economy_table_ = { 1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000 };
  ystl::SmallArray<int32_t> grenade_buy_precent_ = { 95, 85, 60 };
  FireDelayData fire_delay_ {};

  // gamedef.cfg / weapon.cfg: weapon id names (section keys in weapons block, names in weapon.cfg)
  struct WeaponIdName {
    ystl::StringRef name {};
    Weapon id {};
  };

  static constexpr WeaponIdName kWeaponIdNames[] = {
    { "Knife",     Weapon::Knife     },
    { "USP",       Weapon::USP       },
    { "Glock18",   Weapon::Glock18   },
    { "Deagle",    Weapon::Deagle    },
    { "P228",      Weapon::P228      },
    { "Elite",     Weapon::Elite     },
    { "FiveSeven", Weapon::FiveSeven },
    { "M3",        Weapon::M3        },
    { "XM1014",    Weapon::XM1014    },
    { "MP5",       Weapon::MP5       },
    { "TMP",       Weapon::TMP       },
    { "P90",       Weapon::P90       },
    { "MAC10",     Weapon::MAC10     },
    { "UMP45",     Weapon::UMP45     },
    { "AK47",      Weapon::AK47      },
    { "SG552",     Weapon::SG552     },
    { "M4A1",      Weapon::M4A1      },
    { "Galil",     Weapon::Galil     },
    { "Famas",     Weapon::Famas     },
    { "AUG",       Weapon::AUG       },
    { "Scout",     Weapon::Scout     },
    { "AWP",       Weapon::AWP       },
    { "G3SG1",     Weapon::G3SG1     },
    { "SG550",     Weapon::SG550     },
    { "M249",      Weapon::M249      },
    { "Shield",    Weapon::Shield    }
  };

public:
  Config ();
  ~Config () = default;

public:
  // load the configuration files
  void LoadConfigs ();

  // loads main config file
  void LoadMainConfig (bool is_first_load = false);

  // loads bot names
  void LoadNamesConfig ();

  // loads weapons config
  void LoadWeaponsConfig ();

  // loads chatter config
  void LoadChatterConfig ();

  // loads chat config
  void LoadChatConfig ();

  // builds keyword index for fast chat reply lookup
  void BuildKeywordIndex ();

  // loads language config
  void LoadLanguageConfig ();

  // load bots logos config
  void LoadLogosConfig ();

  // load bots avatars config
  void LoadAvatarsConfig ();

  // load bots difficulty config
  void LoadDifficultyConfig ();

  // loads gamedef.cfg (weapon definitions + sound templates)
  void LoadGameDefConfig ();

  // loads bots map-specific config
  void LoadMapSpecificConfig ();

  // loads custom config
  void LoadCustomConfig ();

  // sets memfile to use engine functions
  void SetupMemoryFiles ();

  // picks random bot name
  Name *PickBotName ();

  // remove bot name from used list
  void ClearUsedName (Bot *bot);

  // set the bot names as used
  void SetBotNameUsed (const int index, ystl::StringRef name);

  // initialize weapon info
  void InitWeapons ();

  // fix weapon prices (ie for elite)
  void AdjustWeaponPrices ();

  // find weapon info by weapon id
  WeaponInfo &FindWeaponById (Weapon id);

  // find weapon table index by weapon id
  int32_t FindWeaponIndexById (Weapon id);

  // translates bot message into needed language
  const char *Translate (ystl::StringRef input);

  // strings requested for translation, but missing from the language file (see 'yb debug translate')
  const ystl::HashMap<uint32_t, ystl::String, ystl::Hash<int32_t>> &MissingTranslations () const {
    return missing_translations_;
  }

  void ResetMissingTranslations () {
    missing_translations_.clear ();
  }

  // appends missing translations to the language file and counts them
  int WriteMissingTranslations ();

  // display current custom values
  void ShowCustomValues ();

  // opens config helper
  bool OpenConfig (ystl::StringRef file_name, ystl::StringRef error_if_not_exists, ystl::MemFile *out_file, bool language_dependant = false);

private:
  bool IsCommentLine (ystl::StringRef line) const {
    if (line.empty ()) {
      return true;
    }
    return line.substr (0, 1).find_first_of ("#/;") != ystl::String::InvalidIndex;
  };

  // reads whole config file into a text buffer (for the universal parser)
  ConfigRead ReadConfigText (
    ystl::StringRef file_name, ystl::StringRef error_if_not_exists, ystl::String &out_text, bool language_dependant = false);

  // records a successfully applied config load, so the next map can skip re-parsing it
  void MarkConfigApplied (ystl::StringRef file_name, ystl::StringRef text);

  // drops the snapshot (missing file falls back every load, like on first boot)
  void ForgetConfigSnapshot (ystl::StringRef file_name);

  // rejects configs saved by outdated bot versions, before they reach the parser
  bool CheckConfigVersion (ystl::StringRef file_name, ystl::StringRef text);

  // gamedef.cfg name lookups
  static Weapon LookupWeaponId (ystl::StringRef name);
  static ystl::StringRef LookupWeaponIdName (Weapon id);
  static WeaponType LookupWeaponType (ystl::StringRef name, WeaponType def);
  static WeaponTeam LookupWeaponTeam (ystl::StringRef name, WeaponTeam def);
  static Noise LookupNoiseFlags (ystl::StringRef value);

  // applies gamedef.cfg weapon definitions onto defaults
  void ApplyWeaponDefs (const ystl::ConfNode *root);

  // applies gamedef.cfg sound templates onto defaults
  void ApplySoundDefs (const ystl::ConfNode *root);

  // hash the language string, only the letters
  uint32_t HashLangString (ystl::StringRef str);

  // derives a camelcase label from the original text, falling back to the hash
  static ystl::String MakeTranslateLabel (ystl::StringRef original, uint32_t hash);

public:
  // checks whether chat banks contains messages
  bool HasChatBank (Chat chat_type) const {
    return !chat_[chat_type].empty ();
  }

  // checks whether chatter banks contains messages
  bool HasChatterBank (RadioChat type) const {
    return !chatter_[type].empty ();
  }

  // pick random phrase from chat bank (shuffled cycle, no repeats until exhausted)
  ystl::StringRef PickRandomFromChatBank (Chat chat_type);

  // pick random phrase from chatter bank
  const ChatterItem &PickRandomFromChatterBank (RadioChat type) {
    return chatter_[type].random ();
  }

  // gets chatter repeat-interval
  float GetChatterMessageRepeatInterval (RadioChat type) const {
    return chatter_[type][0].repeat;
  }

  // get's the replies array
  ystl::Array<ChatKeywords> &GetReplies () {
    return replies_;
  }

  // get's the keyword index for fast lookup
  ystl::HashMap<uint32_t, ystl::SmallArray<size_t>> &GetKeywordIndex () {
    return keyword_index_;
  }

  // get's the weapon info data
  ystl::SmallArray<WeaponInfo> &GetWeapons () {
    return weapons_;
  }

  // get's the weapons prop
  WeaponProp &GetWeaponProp (Weapon id) {
    return weapon_props_[id];
  }

  // get's weapon info by id
  WeaponInfo *GetWeapon (Weapon id) {
    for (auto &weapon : weapons_) {
      if (weapon.id == id) {
        return &weapon;
      }
    }
    return nullptr;
  }

  // get's weapon info by id (const)
  const WeaponInfo *GetWeapon (Weapon id) const {
    for (const auto &weapon : weapons_) {
      if (weapon.id == id) {
        return &weapon;
      }
    }
    return nullptr;
  }

  // get's weapons type by id
  WeaponType GetWeaponType (Weapon id) const {
    for (const auto &weapon : weapons_) {
      if (weapon.id == id) {
        return weapon.type;
      }
    }
    return WeaponType::None;
  }

  // get's weapon preferences for personality
  const ystl::SmallArray<int32_t> &GetWeaponPrefs (Personality personality) const {
    switch (personality) {
    case Personality::Normal:
    default:
      return normal_weapon_prefs_;

    case Personality::Rusher:
      return rusher_weapon_prefs_;

    case Personality::Careful:
      return careful_weapon_prefs_;
    }
  }

  // get's the difficulty level tweaks
  DifficultyData *GetDifficultyTweaks (Difficulty level) {
    return &difficulty_[level];
  }

  // get economics value
  int32_t GetEconLimit (EcoLimit id) {
    return bot_buy_economy_table_[id];
  }

  // get's grenade buy percents
  bool ChanceToBuyGrenade (int grenade_type) const {
    return ystl::rg.chance (grenade_buy_precent_[grenade_type]);
  }

  // get's semi-auto fire delays
  const FireDelayData &GetFireDelay () const {
    return fire_delay_;
  }

  // get's random avatar for player (if any)
  ystl::StringRef GetRandomAvatar () const {
    if (!avatars_.empty ()) {
      return avatars_.random ();
    }
    return "";
  }

  // get's random logo index
  int32_t GetRandomLogoDecalIndex () const {
    return static_cast<int32_t> (logos_indices_.random ());
  }

  // get custom value
  ystl::StringRef FetchCustom (ystl::StringRef name) {
    // linear scan instead of map lookup, so no string key gets constructed on every call
    for (const auto &[key, val] : custom_) {
      if (name == key) {
        return val;
      }
    }
    ystl::logger.error ("Trying to fetch unknown custom variable: %s", name);

    return "";
  }

  // simple accessors to c4 model name
  ystl::StringRef GetBombModelName () {
    return FetchCustom ("C4ModelName");
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (Config, conf);

} // namespace bot
