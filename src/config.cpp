//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

Config::Config () {
  constexpr auto chat_size = ystl::to_underlying (Chat::Count);

  chat_.resize (chat_size);
  chat_order_.resize (chat_size);
  chat_pos_.resize (chat_size);
  chatter_.resize (ystl::to_underlying (RadioChat::Count));

  weapon_props_.resize (kMaxWeapons);
}

ystl::StringRef Config::PickRandomFromChatBank (Chat chat_type) {
  const auto id = ystl::to_underlying (chat_type);
  auto &bank = chat_[id];

  if (bank.empty ()) {
    return "";
  }
  auto &order = chat_order_[id];
  auto &pos = chat_pos_[id];

  if (order.size () != bank.size () || pos >= order.size ()) {
    order.clear (); // rebuild shuffled cycle

    for (size_t i = 0; i < bank.size (); ++i) {
      order.push (i);
    }
    order.shuffle ();
    pos = 0;
  }
  return bank[order[pos++]];
}

ystl::StringRef Config::GetGameName () const {
  const bool cz = game.Is (GameFlags::ConditionZero);

  if (!cz && !game.Is (GameFlags::Modern) && !game.Is (GameFlags::Legacy)) {
    return "";
  }
  const auto &bank = cz ? game_names_.cz : game_names_.cs;

  // configs without the section behave exactly like before
  if (bank.empty ()) {
    if (cz) {
      return ystl::rg.chance (30) ? "CZ" : "Condition Zero";
    }
    return ystl::rg.chance (30) ? "CS" : "Counter-Strike";
  }

  if (bank.size () == 1 || ystl::rg.chance (30)) {
    return bank[0];
  }
  return bank[ystl::rg.get (1, static_cast<int32_t> (bank.size ()) - 1)];
}

void Config::LoadConfigs () {
  SetupMemoryFiles ();

  LoadCustomConfig ();
  LoadNamesConfig ();
  LoadChatConfig ();
  LoadChatterConfig ();
  LoadLanguageConfig ();
  LoadLogosConfig ();
  LoadAvatarsConfig ();
  LoadDifficultyConfig ();
}

void Config::LoadMainConfig (bool is_first_load) {
  if (game.Is (GameFlags::Legacy)) {
    util.SetNeedForWelcome (true);
  }
  SetupMemoryFiles ();

  auto needs_to_ignore_var = [] (ystl::Array<ystl::String> &list, const char *needle) {
    for (const auto &var : list) {
      if (var == needle) {
        return true;
      }
    }
    return false;
  };

  auto store_var_value = [] (cvar_t *c, ystl::StringRef value) {
    auto &cvars = game.GetCvars ();

    for (auto &var : cvars) {
      if (var.name == c->name) {
        var.init = value;
        engfuncs.pfnCvar_DirectSet (c, value.chars ());

        break;
      }
    }
  };

  ystl::String line {};
  ystl::MemFile file {};

  auto ignore = ystl::String (cv_ignore_cvars_on_changelevel.As<ystl::StringRef> ()).split (",");

  // this is does the same as exec of engine, but not overwriting values of cvars specified in cv_ignore_cvars_on_changelevel
  if (OpenConfig (product.name_lower, "Bot main config file is not found.", &file, false)) {
    while (file.get_line (line)) {
      line.trim ();

      if (IsCommentLine (line)) {
        continue;
      }

      if (is_first_load) {
        game.ServerCommand (line.chars ());
        continue;
      }
      auto keyval = line.split (" ");

      if (keyval.size () > 1) {
        auto key = keyval[0].trim ().chars ();
        auto cvar = engfuncs.pfnCVarGetPointer (key);

        if (cvar != nullptr) {
          auto value = const_cast<char *> (keyval[1].trim ().trim ("\"").trim ().chars ());

          if (needs_to_ignore_var (ignore, key) && !ystl::strings.matches (value, cvar->string)) {

            // preserve quota number if it's zero
            if (cv_quota.Name () == cvar->name && cv_quota.As<int> () <= 0) {
              store_var_value (cvar, value);
              continue;
            }
            ctrl.Msg ("Bot CVAR '%s' differs from the stored in the config (%s/%s). Ignoring.", cvar->name, cvar->string, value);

            // ensure cvar will have old value
            store_var_value (cvar, cvar->string);
          }
          else {
            store_var_value (cvar, value);
          }
        }
        else {
          game.ServerCommand (line.chars ());
        }
      }
    }
    file.close ();
  }
  else {
    game.ServerCommand (ystl::strings.format ("%s cvars save", product.cmd_pri));
  }

  // android is a bit hard to play, lower the difficulty by default
  if (ystl::plat.android && cv_difficulty.As<int> () > 3) {
    cv_difficulty.Set (3);
  }

  // preload custom config
  conf.LoadCustomConfig ();

  // startup the sockets on windows and check if our host is available
  if (is_first_load) {
    ystl::http.startup (
      conf.FetchCustom ("CheckConnectivityHost"), "Bot is unable to check network availability. Networking features are disabled.");
  }

  // bind the correct menu key for bot menu
  if (!game.IsDedicatedServer ()) {
    auto val = cv_bind_menu_key.As<ystl::StringRef> ();

    if (!val.empty ()) {
      game.ServerCommand ("bind \"%s\" \"%s menu\"", val, product.cmd_pri);
    }
  }
  static const bool disable_log_write = conf.FetchCustom ("DisableLogFile").starts_with ("yes");

  // disable logger if requested
  ystl::logger.set_log_write_enabled (!disable_log_write);

  if (disable_log_write) {
    game.Print ("Bot logging is disabled.");
  }
}

void Config::LoadNamesConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  constexpr auto kMaxNameLen = 32;
  constexpr auto kInvalidBotName = -1;

  // refresh unchanged configs without reallocating, only reset usage and reshuffle
  const auto refresh_names = [&] () {
    for (auto &name : bot_names_) {
      name.used_by = -1;
    }
    bot_names_.shuffle ();
  };
  const auto rc = ReadConfigText ("names", "Name configuration file not found. Falling back to built-in names.", text);

  if (rc == ConfigRead::Unchanged) {
    refresh_names ();
    return;
  }

  // naming initialization (shared across languages)
  if (rc == ConfigRead::Ready) {
    ystl::ConfParser parser;

    if (!parser.parse (text)) {
      ystl::logger.error ("names.%s: %s", kConfigExtension, parser.error ().chars ());
    }
    else if (const auto *names = parser.document ().find ("Names")) {
      bot_names_.clear ();

      for (const auto &node : names->children ()) {
        if (!node->is_item ()) {
          continue;
        }
        auto name = node->value ();

        // max botname is 32 characters
        if (name.size () > kMaxNameLen - 1) {
          name = name.substr (0, kMaxNameLen - 1);
        }
        bot_names_.emplace (name, kInvalidBotName);
      }
    }
  }

  // built-in bot names, used as a fallback when the names config is missing, empty, or comments-only
  static constexpr ystl::StringRef kDefaultBotNames[] = { ".e7`~Silent", "007", "AttAckA", "BDM-WhiteViper", "Deadlynome", "DrEdGe FuEgO",
    "FatManMGS2", "Firestorm", "Gilroy", "Helios", "Magnus", "RAGE OF THE BOY", "Shadow-Death", "Tator", "Texas", "The BiG SMUT", "Zachalicious",
    "alldritc", "bjaren", "bunkerking111", "hoop", "iamblazed", "intangible", "joem91606", "kirby", "simslater", "sk8_sam_101", "thunder",
    "waeldreegia", "wiggles", "wind", "xUTxJackFrost", "$$$KH9I3b_FOBOS$$$", "4epalno", "Aracurd", "BigMann", "DEZ'0", "Danger", "DeadlyHunter",
    "Diman", "EDDY", "FXtrt", "Inf3ktRo", "KBAC", "MaximAnohin", "N4K3d", "Nicolay.Myasnikov", "No_Entry", "Put", "SPorT", "SpiseeCoics",
    "Vanes", "Yo", "ZiGNoff", "aka.", "buredos", "demiurg", "lkfun", "radist", "shalatonchik", "slava", "trouble", "zeddd", "~ASUS~" };

  // built-in fallback when names config is missing, empty, or comments-only
  if (bot_names_.empty ()) {
    for (const auto &name : kDefaultBotNames) {
      bot_names_.emplace (name, kInvalidBotName);
    }
  }
  refresh_names ();
  MarkConfigApplied ("names", text);
}

void Config::LoadWeaponsConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  if (ReadConfigText ("weapon", "Weapon configuration file not found. Loading defaults.", text) != ConfigRead::Ready) {
    return;
  }
  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ystl::logger.error ("weapon.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }
  const auto &doc = parser.document ();

  // per-weapon team availability for a map type
  auto load_team_availability = [&] (ystl::StringRef section, const bool is_as_map) {
    const auto *block = doc.find (section);

    if (!block) {
      return;
    }

    for (const auto &entry : block->children ()) {
      const auto id = LookupWeaponId (entry->name ());

      if (id == Weapon::Invalid) {
        ystl::logger.error (
          "weapon.%s: unknown weapon '%s' in '%s'. Entry ignored.", kConfigExtension, entry->name ().chars (), section.chars ());

        continue;
      }
      auto &weapon = FindWeaponById (id);

      if (is_as_map) {
        weapon.team_as = LookupWeaponTeam (entry->value (), weapon.team_as);
      }
      else {
        weapon.team_standard = LookupWeaponTeam (entry->value (), weapon.team_standard);
      }
    }
  };

  // weapon.cfg loads after gamedef.cfg, so it's able to override team availability
  load_team_availability ("Standard", false);
  load_team_availability ("AS", true);

  // grenade buying percentages
  if (const auto *block = doc.find ("Grenades")) {
    static constexpr struct GrenadeName {
      ystl::StringRef name {};
      int index {};
    } kGrenades[] = {
      { "HE",        0 },
      { "Flash",     1 },
      { "Flashbang", 1 },
      { "Smoke",     2 }
    };

    for (const auto &entry : block->children ()) {
      bool matched = false;

      for (const auto &grenade : kGrenades) {
        if (ystl::strings.matches (entry->name ().chars (), grenade.name.chars ())) {
          grenade_buy_precent_[grenade.index] = entry->as_int ();
          matched = true;

          break;
        }
      }

      if (!matched) {
        ystl::logger.error ("weapon.%s: unknown grenade type '%s'. Entry ignored.", kConfigExtension, entry->name ().chars ());
      }
    }
  }

  // bot economics thresholds
  if (const auto *block = doc.find ("Economy")) {
    static constexpr struct EconomyName {
      ystl::StringRef name {};
      EcoLimit limit {};
    } kLimits[] = {
      { "Primary",         EcoLimit::PrimaryGreater  },
      { "SmgCT",           EcoLimit::SmgCTGreater    },
      { "SmgT",            EcoLimit::SmgTEGreater    },
      { "Shotgun",         EcoLimit::ShotgunGreater  },
      { "ShotgunMax",      EcoLimit::ShotgunLess     },
      { "Heavy",           EcoLimit::HeavyGreater    },
      { "HeavyMax",        EcoLimit::HeavyLess       },
      { "ProstockNormal",  EcoLimit::ProstockNormal  },
      { "ProstockRusher",  EcoLimit::ProstockRusher  },
      { "ProstockCareful", EcoLimit::ProstockCareful },
      { "Shield",          EcoLimit::ShieldGreater   }
    };

    for (const auto &entry : block->children ()) {
      bool matched = false;

      for (const auto &limit : kLimits) {
        if (ystl::strings.matches (entry->name ().chars (), limit.name.chars ())) {
          bot_buy_economy_table_[ystl::to_underlying (limit.limit)] = entry->as_int ();
          matched = true;

          break;
        }
      }

      if (!matched) {
        ystl::logger.error ("weapon.%s: unknown economy key '%s'. Entry ignored.", kConfigExtension, entry->name ().chars ());
      }
    }
  }

  // semi-auto fire delays; final delay = base + random (min[slot], max[slot]),
  if (const auto *block = doc.find ("FireDelay")) {
    auto load_delay_list = [&] (ystl::StringRef key, float *out) {
      const auto *node = block->find (key);

      if (!node) {
        return;
      }
      const auto list = node->as_list ();

      if (list.size () != kFireDelaySlots) {
        ystl::logger.error ("weapon.%s: '%s' in 'FireDelay' must contain six values. Entry ignored.", kConfigExtension, key.chars ());
        return;
      }

      for (int i = 0; i < kFireDelaySlots; ++i) {
        out[i] = ystl::max (0.0f, list[i].as<float> ());
      }
    };

    fire_delay_.pistol_base = ystl::max (0.0f, block->get_float ("PistolBase", fire_delay_.pistol_base));
    fire_delay_.other_base = ystl::max (0.0f, block->get_float ("OtherBase", fire_delay_.other_base));
    fire_delay_.deagle_extra = ystl::max (0.0f, block->get_float ("DeagleExtra", fire_delay_.deagle_extra));

    load_delay_list ("PistolMin", fire_delay_.pistol_min);
    load_delay_list ("PistolMax", fire_delay_.pistol_max);
    load_delay_list ("OtherMin", fire_delay_.other_min);
    load_delay_list ("OtherMax", fire_delay_.other_max);
  }

  // weapon priorities per personality; weapons listed best-first, every weapon must be present exactly once
  if (const auto *block = doc.find ("Preferences")) {
    auto load_prefs = [&] (ystl::StringRef name, ystl::SmallArray<int32_t> &prefs) {
      const auto *section = block->find (name);

      if (!section) {
        return;
      }
      ystl::SmallArray<Weapon> ranked {};

      auto collect_names = [&] (const auto &node) {
        for (const auto &token : node->as_list ()) {
          const auto id = LookupWeaponId (token);

          if (id == Weapon::Invalid) {
            ystl::logger.error ("weapon.%s: unknown weapon '%s' in '%s' preferences.", kConfigExtension, token.chars (), name.chars ());

            return false;
          }
          ranked.emplace (id);
        }
        return true;
      };

      if (section->is_scalar ()) {
        if (!collect_names (section)) {
          return;
        }
      }
      else {
        for (const auto &entry : section->children ()) {
          if (!collect_names (entry)) {
            return;
          }
        }
      }
      ystl::SmallArray<bool> seen {};

      // indexed by counter-strike weapon id, which isn't compact (goes up to 30)
      seen.resize (kMaxWeapons);

      for (const auto &id : ranked) {
        if (seen[ystl::to_underlying (id)]) {
          ystl::logger.error (
            "weapon.%s: weapon '%s' listed twice in '%s' preferences.", kConfigExtension, LookupWeaponIdName (id).chars (), name.chars ());

          return;
        }
        seen[ystl::to_underlying (id)] = true;
      }
      ystl::String missing {};

      for (const auto &entry : kWeaponIdNames) {
        if (!seen[ystl::to_underlying (entry.id)]) {
          if (!missing.empty ()) {
            missing += ", ";
          }
          missing += entry.name;
        }
      }

      if (!missing.empty ()) {
        ystl::logger.error ("weapon.%s: missing weapons in '%s' preferences: %s.", kConfigExtension, name.chars (), missing.chars ());

        return;
      }

      // best-first in config -> priority index in memory (highest index is checked first when buying);
      for (size_t i = 0; i < ranked.size (); ++i) {
        prefs[kNumWeapons - 1 - i] = FindWeaponIndexById (ranked[i]);
      }
    };

    load_prefs ("Normal", normal_weapon_prefs_);
    load_prefs ("Rusher", rusher_weapon_prefs_);
    load_prefs ("Careful", careful_weapon_prefs_);
  }

  // warn about unknown entries (including leftovers from the old positional format)
  for (const auto &node : doc.children ()) {
    const auto name = node->name ();

    if (!ystl::strings.matches (name.chars (), "Standard") && !ystl::strings.matches (name.chars (), "AS") &&
        !ystl::strings.matches (name.chars (), "Grenades") && !ystl::strings.matches (name.chars (), "Economy") &&
        !ystl::strings.matches (name.chars (), "FireDelay") && !ystl::strings.matches (name.chars (), "Preferences")) {

      ystl::logger.error ("weapon.%s: unknown entry '%s'. Entry ignored.", kConfigExtension, name.chars ());
    }
  }
}

void Config::LoadChatterConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  // chatter initialization (banks stay valid across maps, no per-map refresh needed)
  const auto radio_ready = game.Is (GameFlags::HasBotVoice) && cv_radio_mode.As<int> () == 2;
  const auto rc = radio_ready ? ReadConfigText ("chatter", "Couldn't open chatter configuration.", text) : ConfigRead::Missing;

  if (rc == ConfigRead::Unchanged) {
    return;
  }

  if (radio_ready && rc == ConfigRead::Ready) {

    chatter_.clear ();

    // chatter event names; sounds of every event go into the bank of its radiochat code
    static constexpr struct EventMap {
      ystl::StringRef name {};
      RadioChat code {};
      float repeat {};
    } kChatterEventMap[] = {
      { "CoverMe",                 RadioChat::CoverMe,                   kMaxChatterRepeatInterval },
      { "YouTakePoint",            RadioChat::YouTakeThePoint,           kMaxChatterRepeatInterval },
      { "HoldPosition",            RadioChat::HoldThisPosition,          10.0f                     },
      { "RegroupTeam",             RadioChat::RegroupTeam,               10.0f                     },
      { "FollowMe",                RadioChat::FollowMe,                  15.0f                     },
      { "TakingFire",              RadioChat::TakingFireNeedAssistance,  5.0f                      },
      { "GoGoGo",                  RadioChat::GoGoGo,                    kMaxChatterRepeatInterval },
      { "Fallback",                RadioChat::TeamFallback,              kMaxChatterRepeatInterval },
      { "StickTogether",           RadioChat::StickTogetherTeam,         kMaxChatterRepeatInterval },
      { "GetInPosition",           RadioChat::GetInPositionAndWaitForGo, kMaxChatterRepeatInterval },
      { "StormTheFront",           RadioChat::StormTheFront,             kMaxChatterRepeatInterval },
      { "ReportTeam",              RadioChat::ReportInTeam,              kMaxChatterRepeatInterval },
      { "Affirmative",             RadioChat::RogerThat,                 kMaxChatterRepeatInterval },
      { "EnemySpotted",            RadioChat::EnemySpotted,              4.0f                      },
      { "NeedBackup",              RadioChat::NeedBackup,                5.0f                      },
      { "SectorClear",             RadioChat::SectorClear,               10.0f                     },
      { "InPosition",              RadioChat::ImInPosition,              10.0f                     },
      { "ReportingIn",             RadioChat::ReportingIn,               3.0f                      },
      { "ShesGonnaBlow",           RadioChat::ShesGonnaBlow,             kMaxChatterRepeatInterval },
      { "Negative",                RadioChat::Negative,                  kMaxChatterRepeatInterval },
      { "EnemyDown",               RadioChat::EnemyDown,                 10.0f                     },
      { "DiePain",                 RadioChat::DiePain,                   kMaxChatterRepeatInterval },
      { "GoingToPlantBomb",        RadioChat::GoingToPlantBomb,          5.0f                      },
      { "GoingToGuardEscapeZone",  RadioChat::GoingToGuardEscapeZone,    kMaxChatterRepeatInterval },
      { "GoingToGuardRescueZone",  RadioChat::GoingToGuardRescueZone,    kMaxChatterRepeatInterval },
      { "GoingToGuardVIPSafety",   RadioChat::GoingToGuardVIPSafety,     kMaxChatterRepeatInterval },
      { "RescuingHostages",        RadioChat::RescuingHostages,          kMaxChatterRepeatInterval },
      { "TeamKill",                RadioChat::TeamKill,                  kMaxChatterRepeatInterval },
      { "GuardingEscapeZone",      RadioChat::GuardingEscapeZone,        kMaxChatterRepeatInterval },
      { "GuardingVipSafety",       RadioChat::GuardingVIPSafety,         kMaxChatterRepeatInterval },
      { "PlantingC4",              RadioChat::PlantingBomb,              10.0f                     },
      { "InCombat",                RadioChat::InCombat,                  kMaxChatterRepeatInterval },
      { "SeeksEnemy",              RadioChat::SeekingEnemies,            kMaxChatterRepeatInterval },
      { "Nothing",                 RadioChat::Nothing,                   kMaxChatterRepeatInterval },
      { "UseHostage",              RadioChat::UsingHostages,             kMaxChatterRepeatInterval },
      { "WonTheRound",             RadioChat::WonTheRound,               kMaxChatterRepeatInterval },
      { "QuicklyWonTheRound",      RadioChat::QuickWonRound,             kMaxChatterRepeatInterval },
      { "NoEnemiesLeft",           RadioChat::NoEnemiesLeft,             kMaxChatterRepeatInterval },
      { "FoundBombPlace",          RadioChat::FoundC4Plant,              15.0f                     },
      { "WhereIsTheBomb",          RadioChat::WhereIsTheC4,              kMaxChatterRepeatInterval },
      { "DefendingBombSite",       RadioChat::DefendingBombsite,         kMaxChatterRepeatInterval },
      { "BarelyDefused",           RadioChat::BarelyDefused,             kMaxChatterRepeatInterval },
      { "NiceShotCommander",       RadioChat::NiceShotCommander,         10.0f                     },
      { "SpotTheBomber",           RadioChat::SpotTheBomber,             4.3f                      },
      { "VIPSpotted",              RadioChat::VIPSpotted,                5.3f                      },
      { "FriendlyFire",            RadioChat::FriendlyFire,              2.1f                      },
      { "GotBlinded",              RadioChat::Blind,                     12.0f                     },
      { "GuardingPlantedC4",       RadioChat::GuardingPlantedC4,         3.0f                      },
      { "DefusingC4",              RadioChat::DefusingBomb,              3.0f                      },
      { "FoundC4",                 RadioChat::FoundC4,                   5.5f                      },
      { "ScaredEmotion",           RadioChat::ScaredEmotion,             6.1f                      },
      { "HeardEnemy",              RadioChat::HeardTheEnemy,             12.8f                     },
      { "SpottedOneEnemy",         RadioChat::SpottedOneEnemy,           4.0f                      },
      { "SpottedTwoEnemies",       RadioChat::SpottedTwoEnemies,         4.0f                      },
      { "SpottedThreeEnemies",     RadioChat::SpottedThreeEnemies,       4.0f                      },
      { "TooManyEnemies",          RadioChat::TooManyEnemies,            4.0f                      },
      { "SniperWarning",           RadioChat::SniperWarning,             14.3f                     },
      { "SniperKilled",            RadioChat::SniperKilled,              12.1f                     },
      { "OneEnemyLeft",            RadioChat::OneEnemyLeft,              12.5f                     },
      { "TwoEnemiesLeft",          RadioChat::TwoEnemiesLeft,            12.5f                     },
      { "ThreeEnemiesLeft",        RadioChat::ThreeEnemiesLeft,          12.5f                     },
      { "NiceShotPall",            RadioChat::NiceShotPall,              2.0f                      },
      { "GoingToGuardHostages",    RadioChat::GoingToGuardHostages,      3.0f                      },
      { "GoingToGuardDroppedBomb", RadioChat::GoingToGuardDroppedC4,     6.0f                      },
      { "OnMyWay",                 RadioChat::OnMyWay,                   21.5f                     },
      { "LeadOnSir",               RadioChat::LeadOnSir,                 5.0f                      },
      { "PinnedDown",              RadioChat::PinnedDown,                5.0f                      },
      { "GottaFindTheBomb",        RadioChat::GottaFindC4,               3.0f                      },
      { "YouHeardTheMan",          RadioChat::YouHeardTheMan,            3.0f                      },
      { "LostCommander",           RadioChat::LostCommander,             4.5f                      },
      { "NewRound",                RadioChat::NewRound,                  3.5f                      },
      { "BehindSmoke",             RadioChat::BehindSmoke,               3.5f                      },
      { "BombSiteSecured",         RadioChat::BombsiteSecured,           3.5f                      },
      { "GoingToCamp",             RadioChat::GoingToCamp,               30.0f                     },
      { "Camp",                    RadioChat::Camping,                   10.0f                     },
      { "OnARoll",                 RadioChat::OnARoll,                   kMaxChatterRepeatInterval },
    };

    ystl::Array<ystl::String> missing_waves {};
    ystl::ConfParser parser {};

    if (!parser.parse (text)) {
      ystl::logger.error ("chatter.%s: %s", kConfigExtension, parser.error ().chars ());
      cv_radio_mode.Set (1);
      return;
    }
    const auto &doc = parser.document ();
    const auto *rewrite = doc.find ("RewritePath");

    if (!rewrite) {
      rewrite = doc.get ("Chatter.RewritePath");
    }

    if (rewrite && !rewrite->value ().empty ()) {
      // own the value: the parser view must not outlive this call into the engine
      const ystl::String path = rewrite->as_string ();
      cv_chatter_path.Set (path.chars ());
    }
    const auto *chatter_root = doc.find ("Chatter");

    if (!chatter_root) {
      chatter_root = &doc;
    }
    {
      for (const auto &node : chatter_root->children ()) {
        // rewritepath is handled separately
        if (node->name () == "RewritePath") {
          continue;
        }
        const EventMap *event_def = nullptr;

        for (const auto &event : kChatterEventMap) {
          if (event.name == node->name ()) {
            event_def = &event;
            break;
          }
        }

        if (!event_def) {
          ystl::logger.error ("chatter.%s: unknown chatter event '%s'. Entry ignored.", kConfigExtension, node->name ().chars ());
          continue;
        }

        // sound names: bare items inside a block, or a comma-separated scalar value
        ystl::Array<ystl::String> sentences {};

        if (node->is_block ()) {
          for (const auto &item : node->children ()) {
            if (item->is_scalar () && !item->value ().empty ()) {
              sentences.push (ystl::String (item->value ()));
            }
          }
        }
        else if (node->is_scalar ()) {
          sentences = node->as_list ();
        }

        if (sentences.empty ()) {
          continue;
        }
        sentences.shuffle ();

        for (auto &sound : sentences) {
          sound.trim ().trim ("\"");
          const auto duration = util.GetWaveFileDuration (sound.chars ());

          if (duration > 0.0f) {
            chatter_[event_def->code].emplace (ystl::move (sound), event_def->repeat, duration);
          }
          else {
            missing_waves.push (sound);
          }
        }
      }
    }

    if (!missing_waves.empty ()) {
      constexpr auto kMaxErroredWaves = 10;

      // too much erros bail out
      if (missing_waves.size () > kMaxErroredWaves) {
        cv_radio_mode.Set (1);

        missing_waves.resize (kMaxErroredWaves);
      }
      game.Print ("Warning: Couldn't get duration of next chatter sounds: %s ...", ystl::String::join (missing_waves, ","));
    }
    MarkConfigApplied ("chatter", text);
  }
  else {
    cv_radio_mode.Set (1);

    // only notify if has bot voice, but failed to open file
    if (game.Is (GameFlags::HasBotVoice)) {
      game.Print ("Bots chatter communication disabled.");
    }
  }
}

void Config::LoadChatConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  const auto reset_cycles = [&] () {
    for (size_t i = 0; i < chat_.size (); ++i) {
      chat_order_[i].clear (); // force rebuild of shuffled cycle on next pick
      chat_pos_[i] = 0;
    }
  };
  const auto rc = ReadConfigText ("chat", "Couldn't open chat configuration.", text, true);

  if (rc == ConfigRead::Missing) {
    cv_chat.Set (0);

    return;
  }

  if (rc == ConfigRead::Unchanged) {
    return; // keep current cycles, so map change doesn't reset variety
  }
  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ystl::logger.error ("chat.%s: %s", kConfigExtension, parser.error ().chars ());
    cv_chat.Set (0);

    return;
  }
  const auto *chat = parser.document ().find ("Chat");

  if (!chat) {
    chat = &parser.document ();
  }

  // clear all the stuff before loading new one
  for (auto &item : chat_) {
    item.clear ();
  }
  game_names_.clear ();
  replies_.clear ();
  keyword_index_.clear ();

  // chat section names (matches block names in chat.cfg)
  static constexpr struct ChatSection {
    ystl::StringRef name {};
    Chat id {};
  } kSections[] = {
    { "Killed",     Chat::Kill       },
    { "BombPlant",  Chat::Plant      },
    { "DeadChat",   Chat::Dead       },
    { "Welcome",    Chat::Hello      },
    { "TeamAttack", Chat::TeamAttack },
    { "TeamKill",   Chat::TeamKill   },
    { "Unknown",    Chat::NoKeyword  }
  };

  for (const auto &node : chat->children ()) {
    if (!node->is_block ()) {
      ystl::logger.error ("chat.%s: unexpected entry '%s'. Entry ignored.", kConfigExtension, node->name ().chars ());

      continue;
    }

    if (ystl::strings.matches (node->name ().chars (), "Replies")) {
      // reply groups: 'key' scalars for keywords, anonymous raw block for verbatim replies
      for (const auto &group : node->children ()) {
        if (!group->is_block ()) {
          ystl::logger.error ("chat.%s: unexpected entry '%s' in Replies. Entry ignored.", kConfigExtension, group->name ().chars ());

          continue;
        }
        ystl::Array<ystl::String> keywords {};
        ystl::Array<ystl::String> replies {};

        for (const auto &entry : group->children ()) {
          if (entry->is_block ()) {
            for (const auto &item : entry->children ()) {
              replies.emplace (item->value ());
            }

            if (!keywords.empty () && !replies.empty ()) {
              replies_.emplace (keywords, replies);
            }
            keywords.clear ();
            replies.clear ();
          }
          else if (ystl::strings.matches (entry->name ().chars (), "key") && !entry->value ().empty ()) {
            keywords.emplace (ystl::utf8tools.str_to_upper (entry->value ()));
          }
          else if (!ystl::strings.matches (entry->name ().chars (), "key")) {
            ystl::logger.error ("chat.%s: unexpected entry '%s' in reply group. Entry ignored.", kConfigExtension, entry->name ().chars ());
          }
        }
      }
      continue;
    }

    if (ystl::strings.matches (node->name ().chars (), "GameNames")) {
      for (const auto &entry : node->children ()) {
        if (ystl::strings.matches (entry->name ().chars (), "CZ")) {
          game_names_.cz = entry->as_list ();
        }
        else if (ystl::strings.matches (entry->name ().chars (), "CS")) {
          game_names_.cs = entry->as_list ();
        }
        else {
          ystl::logger.error ("chat.%s: unexpected entry '%s' in GameNames. Entry ignored.", kConfigExtension, entry->name ().chars ());
        }
      }
      continue;
    }

    for (const auto &section : kSections) {
      if (!ystl::strings.matches (node->name ().chars (), section.name.chars ())) {
        continue;
      }
      auto *chat_array = &chat_[ystl::to_underlying (section.id)];

      for (const auto &item : node->children ()) {
        chat_array->push (item->value ());
      }
      break;
    }
  }

  reset_cycles ();

  // build keyword index for fast lookup
  BuildKeywordIndex ();

  MarkConfigApplied ("chat", text);
}

void Config::BuildKeywordIndex () {
  // map each keyword hash to reply indices for fast lookup

  keyword_index_.clear ();

  for (size_t i = 0; i < replies_.size (); ++i) {
    const auto &reply = replies_[i];

    for (const auto &keyword : reply.keywords) {
      size_t start = 0, end = keyword.size ();

      while (start < end && keyword[start] == ' ') { // trim padding, so " BOT " indexes as "BOT"
        ++start;
      }
      while (end > start && keyword[end - 1] == ' ') {
        --end;
      }

      if (end <= start) {
        continue;
      }
      const auto hash = ystl::detail::fnv1a32_n (keyword.chars () + start, end - start);

      if (!keyword_index_.exists (hash)) {
        keyword_index_[hash] = ystl::SmallArray<size_t> {};
      }
      keyword_index_[hash].push (i);
    }
  }
}

void Config::LoadLanguageConfig () {
  SetupMemoryFiles ();
  missing_translations_.clear (); // new language, fresh missing strings list

  if (game.Is (GameFlags::Legacy)) {
    return; // legacy versions will use only english translation
  }
  ystl::String text {};

  const auto rc = ReadConfigText ("lang", "Specified language not found.", text, true);

  if (rc == ConfigRead::Missing) {
    if (cv_language.As<ystl::StringRef> () != "en") {
      ystl::logger.error ("Couldn't load language configuration.");
    }
    return;
  }

  if (rc == ConfigRead::Unchanged) {
    return; // translations intact, missing list already cleared above
  }
  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ystl::logger.error ("lang.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }
  const auto *lang = parser.document ().find ("Lang");

  if (!lang) {
    lang = &parser.document ();
  }

  // joins raw block lines back into the multi-line string
  auto join_lines = [] (const ystl::ConfNode *block) {
    ystl::String result {};

    for (const auto &item : block->children ()) {
      if (!result.empty ()) {
        result += '\n';
      }
      result += item->value ();
    }
    return result;
  };

  // clear all the translations before new load
  language_.clear ();

  for (const auto &entry : lang->children ()) {
    if (!entry->is_block ()) {
      ystl::logger.error ("lang.%s: unexpected entry '%s'. Entry ignored.", kConfigExtension, entry->name ().chars ());

      continue;
    }
    const auto *original = entry->find ("Original");
    const auto *translated = entry->find ("Translated");

    if (!original || !translated || original->size () == 0 || translated->size () == 0) {
      continue;
    }
    auto key = join_lines (original);

    language_[HashLangString (key.chars ())] = join_lines (translated);
  }
  MarkConfigApplied ("lang", text);
}

void Config::LoadAvatarsConfig () {
  SetupMemoryFiles ();

  if (game.Is (GameFlags::Legacy) || game.Is (GameFlags::Xash3D)) {
    return;
  }
  ystl::String text {};

  const auto rc = ReadConfigText ("avatars", "Avatars config file not found. Avatars will not be displayed.", text);

  if (rc == ConfigRead::Unchanged) {
    return; // avatars intact
  }
  avatars_.clear ();

  if (rc == ConfigRead::Missing) {
    return;
  }
  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ystl::logger.error ("avatars.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }

  if (auto *root = parser.document ().find ("Avatars")) {
    for (const auto &node : root->children ()) {
      if (node->is_scalar () && !node->value ().empty ()) {
        avatars_.push (ystl::String (node->value ()));
      }
    }
  }
  MarkConfigApplied ("avatars", text);
}

void Config::LoadDifficultyConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  const auto rc = ReadConfigText ("difficulty", "Difficulty config file not found. Loading defaults.", text);

  if (rc == ConfigRead::Unchanged) {
    return; // tweaks intact
  }

  // initialize defaults
  difficulty_[Difficulty::Noob] = {
    { 1.0f, 1.4f },
    20, 10, 10, 34, { 12.0f, 12.0f, 20.0f }
  };
  difficulty_[Difficulty::Easy] = {
    { 0.7f, 1.0f },
    35, 25, 25, 30, { 8.0f, 8.0f, 14.0f }
  };
  difficulty_[Difficulty::Normal] = {
    { 0.5f, 0.7f },
    50, 40, 40, 27, { 5.0f, 5.0f, 10.0f }
  };
  difficulty_[Difficulty::Hard] = {
    { 0.3f, 0.45f },
    65, 60, 60, 24, { 2.0f, 2.0f, 4.0f }
  };
  difficulty_[Difficulty::Expert] = {
    { 0.15f, 0.3f },
    80, 75, 75, 21, { 0.0f, 0.0f, 0.0f }
  };

  if (rc == ConfigRead::Missing) {
    return;
  }
  ystl::ConfParser parser;

  if (!parser.parse (text)) {
    ystl::logger.error ("difficulty.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }

  // each root block is a difficulty level with named tweak values
  for (const auto &level : parser.document ().children ()) {
    if (!level->is_block ()) {
      continue;
    }
    Difficulty id = Difficulty::Invalid;

    if (level->name () == "Noob") {
      id = Difficulty::Noob;
    }
    else if (level->name () == "Easy") {
      id = Difficulty::Easy;
    }
    else if (level->name () == "Normal") {
      id = Difficulty::Normal;
    }
    else if (level->name () == "Hard") {
      id = Difficulty::Hard;
    }
    else if (level->name () == "Expert") {
      id = Difficulty::Expert;
    }
    else {
      ystl::logger.error ("difficulty.%s: unknown difficulty level '%s'. Entry ignored.", kConfigExtension, level->name ().chars ());
      continue;
    }
    auto diff = &difficulty_[id];

    if (auto *value = level->find ("Reaction")) {
      const auto reaction = value->as_list ();

      if (reaction.size () != 2) {
        ystl::logger.error (
          "difficulty.%s: 'Reaction' of '%s' must contain two values. Entry ignored.", kConfigExtension, level->name ().chars ());
      }
      else {
        diff->reaction[0] = reaction[0].as<float> ();
        diff->reaction[1] = reaction[1].as<float> ();
      }
    }
    diff->headshot_pct = level->get_int ("HeadshotChance", diff->headshot_pct);
    diff->seen_thru_pct = level->get_int ("SeenThruWallChance", diff->seen_thru_pct);
    diff->hear_thru_pct = level->get_int ("HeardThruWallChance", diff->hear_thru_pct);
    diff->max_recoil = level->get_int ("MaxRecoil", diff->max_recoil);

    if (auto *value = level->find ("AimError")) {
      const auto aim_error = value->as_list ();

      if (aim_error.size () != 3) {
        ystl::logger.error (
          "difficulty.%s: 'AimError' of '%s' must contain three values. Entry ignored.", kConfigExtension, level->name ().chars ());
      }
      else {
        diff->aim_error.x = aim_error[0].as<float> ();
        diff->aim_error.y = aim_error[1].as<float> ();
        diff->aim_error.z = aim_error[2].as<float> ();
      }
    }
  }
  MarkConfigApplied ("difficulty", text);
}

void Config::LoadMapSpecificConfig () {
  auto map_specific_config =
    ystl::strings.join_path (folders.config, "maps", ystl::strings.format ("%s.%s", game.GetMapName (), kConfigExtension));

  // check existence of file
  if (ystl::plat.file_exists (ystl::strings.join_path (bstor.GetRunningPath (), map_specific_config).chars ())) {

    auto map_specific_config_for_exec = ystl::strings.join_path (bstor.GetRunningPathVfs (), map_specific_config);
    map_specific_config_for_exec.replace ("\\", "/");

    game.ServerCommand ("exec %s", map_specific_config_for_exec);
    ctrl.Msg ("Executed map-specific config: %s", map_specific_config_for_exec);
  }
}

void Config::LoadCustomConfig () {
  ystl::String text {};

  const auto rc = ReadConfigText ("custom", "Custom config file not found. Loading defaults.", text);

  if (rc == ConfigRead::Unchanged) {
    return; // custom values intact
  }

  // refill in place so the buckets survive across map loads
  auto set_defaults = [&] () {
    static constexpr struct CustomDefault {
      ystl::StringRef key {};
      ystl::StringRef value {};
    } kDefaults[] = {
      { "C4ModelName",           "c4.mdl"                             },
      { "AMXParachuteCvar",      "sv_parachute"                       },
      { "CustomCSDMSpawnPoint",  "view_spawn"                         },
      { "CSDMDetectCvar",        "csdm_active"                        },
      { "ZMDetectCvar",          "zp_delay,zp_on"                     },
      { "ZMDelayCvar",           "zp_delay"                           },
      { "ZMInfectedTeam",        "T"                                  },
      { "ModeWallClassname",     "test_effect"                        },
      { "EnableFakeBotFeatures", "no"                                 },
      { "DisableLogFile",        "no"                                 },
      { "DisableSpawnControl",   "no"                                 },
      { "UnlockThinkFPS",        "no"                                 },
      { "CheckConnectivityHost", GraphUrlResolver::kConnectivityHost  },
      { "GraphGithubDownload",   GraphUrlResolver::kGithubDownloadUrl },
      { "GraphRussiaDownload",   GraphUrlResolver::kRussiaDownloadUrl },
      { "GraphWorkersUpload",    GraphUrlResolver::kWorkersBaseUrl    },
      { "GraphRussiaWorker",     GraphUrlResolver::kRussiaWorkerUrl   },
      { "GraphLegacyBase",       GraphUrlResolver::kLegacyBaseUrl     },
      { "GraphLegacyUpload",     GraphUrlResolver::kLegacyUploadUrl   }
    };

    custom_.clear ();

    for (const auto &def : kDefaults) {
      custom_[ystl::String (def.key)] = ystl::String (def.value);
    }
  };
  set_defaults ();

  if (rc == ConfigRead::Missing) {
    return;
  }
  ystl::ConfParser parser {};

  if (!parser.parse (text)) {
    ystl::logger.error ("custom.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }

  for (const auto &node : parser.document ().children ()) {
    if (node->is_scalar ()) {
      if (!node->name ().empty ()) {
        custom_[ystl::String (node->name ())] = ystl::String (node->value ());
      }
      continue;
    }

    for (const auto &entry : node->children ()) {
      if (entry->is_scalar () && !entry->name ().empty ()) {
        custom_[ystl::String (entry->name ())] = ystl::String (entry->value ());
      }
    }
  }
  MarkConfigApplied ("custom", text);
}

void Config::LoadLogosConfig () {
  SetupMemoryFiles ();

  auto add_logo_index = [&] (ystl::StringRef logo) {
    const auto index = engfuncs.pfnDecalIndex (logo.chars ());

    if (index > 0) {
      logos_indices_.push (index);
    }
  };
  logos_indices_.clear ();

  ystl::String text {};

  if (ReadConfigText ("logos", "Logos config file not found. Loading defaults.", text) != ConfigRead::Missing) {
    ystl::ConfParser parser {};

    if (!parser.parse (text)) {
      ystl::logger.error ("logos.%s: %s", kConfigExtension, parser.error ().chars ());
    }
    else if (auto *root = parser.document ().find ("Logos")) {
      for (const auto &node : root->children ()) {
        if (node->is_scalar () && !node->value ().empty ()) {
          add_logo_index (node->value ());
        }
      }
    }
  }

  // use defaults
  if (logos_indices_.empty ()) {
    auto defaults =
      ystl::String { "{biohaz;{graf003;{graf004;{graf005;{lambda06;{target;{hand1;{spit2;{bloodhand6;{foot_l;{foot_r" }.split (";");

    for (const auto &logo : defaults) {
      add_logo_index (logo);
    }
  }
}

void Config::LoadMaterialTypes () {
  // mirrors TEXTURETYPE_Init: type char, then texture name, comments skipped
  if (material_types_loaded_) {
    return;
  }
  material_types_loaded_ = true;

  ystl::MemFile file ("sound/materials.txt");

  if (!file) {
    return; // missing file means everything counts as concrete, like the game
  }
  ystl::String line {};

  while (file.get_line (line)) {
    line.trim ();

    if (line.empty () || IsCommentLine (line)) {
      continue;
    }
    // single alpha char type, then the texture name up to the next whitespace
    const auto type = line.substr (0, 1);
    auto name = line.substr (1);

    name.trim ();

    const auto name_end = name.find_first_of (" \t");

    if (name_end != ystl::String::InvalidIndex) {
      name = name.substr (0, name_end);
    }

    if (name.empty ()) {
      continue;
    }
    char type_char = type[0];

    if (type_char >= 'a' && type_char <= 'z') {
      type_char = static_cast<char> (type_char - ('a' - 'A')); // game uppercases the type
    }
    else if (!(type_char >= 'A' && type_char <= 'Z')) {
      continue;
    }
    material_types_.push (MaterialEntry { type_char, name });
  }
}

char Config::GetMaterialType (const char *texture) {
  LoadMaterialTypes ();

  if (texture == nullptr) {
    return 'C';
  }
  // strip engine prefixes exactly like the game does
  const char *name = texture;

  if ((*name == '-' || *name == '+') && name[1] != '\0') {
    name += 2;
  }

  if ((*name == '{' || *name == '!' || *name == '~' || *name == ' ') && name[1] != '\0') {
    name += 1;
  }

  for (const auto &entry : material_types_) {
    if (ystl::strings.matches (entry.name.chars (), name)) {
      return entry.type;
    }
  }
  return 'C'; // unknown textures count as concrete, like the game
}

void Config::SetupMemoryFiles () {
  static bool set_memory_pointers = true;

  auto wrap_load_file = [] (const char *filename, int *length) {
    return engfuncs.pfnLoadFileForMe (filename, length);
  };

  auto wrap_free_file = [] (void *buffer) {
    engfuncs.pfnFreeFile (buffer);
  };

  if (set_memory_pointers) {
    ystl::FileLoader::instance ().initialize (ystl::move (wrap_load_file), ystl::move (wrap_free_file));
    set_memory_pointers = false;
  }
}

Name *Config::PickBotName () {
  if (bot_names_.empty ()) {
    return nullptr;
  }

  for (size_t i = 0; i < bot_names_.size () * 2; ++i) {
    auto bn = &bot_names_.random ();

    if (bn->name.empty () || bn->used_by != -1) {
      continue;
    }
    return bn;
  }
  return nullptr;
}

void Config::ClearUsedName (Bot *bot) {
  for (auto &bn : bot_names_) {
    if (bn.used_by == bot->Index ()) {
      bn.used_by = -1;
      break;
    }
  }
}

void Config::SetBotNameUsed (const int index, ystl::StringRef name) {
  for (auto &bn : bot_names_) {
    if (bn.name == name) {
      bn.used_by = index;
      break;
    }
  }
}

void Config::InitWeapons () {
  // weapon defaults in fixed order, indexed by weapon preferences

  // clang-format off
   static constexpr WeaponInfo kWeaponTable[] = {
      {
         .id = Weapon::Knife,
         .name = "weapon_knife",
         .model = "knife.mdl",
         .alias = "knife",
         .full_name = "Knife",
         .team_standard = WeaponTeam::None,
         .team_as = WeaponTeam::None,
         .type = WeaponType::Melee,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::USP,
         .name = "weapon_usp",
         .model = "usp.mdl",
         .alias = "usp",
         .full_name = "HK USP .45 Tactical",
         .price = 500,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::None,
         .team_as = WeaponTeam::None,
         .buy_group = 1,
         .buy_select = 1,
         .buy_select_t = 2,
         .buy_select_ct = 2,
         .max_clip = 12,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::Glock18,
         .name = "weapon_glock18",
         .model = "glock18.mdl",
         .alias = "glock",
         .full_name = "Glock18 Select Fire",
         .price = 400,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::None,
         .team_as = WeaponTeam::None,
         .buy_group = 1,
         .buy_select = 2,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .max_clip = 20,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::Deagle,
         .name = "weapon_deagle",
         .model = "deagle.mdl",
         .alias = "deagle",
         .full_name = "Desert Eagle .50AE",
         .price = 650,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 1,
         .buy_select = 3,
         .buy_select_t = 4,
         .buy_select_ct = 4,
         .penetrate_power = 2,
         .max_clip = 7,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::P228,
         .name = "weapon_p228",
         .model = "p228.mdl",
         .alias = "p228",
         .full_name = "SIG P228",
         .price = 600,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 1,
         .buy_select = 4,
         .buy_select_t = 3,
         .buy_select_ct = 3,
         .max_clip = 13,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::Elite,
         .name = "weapon_elite",
         .model = "elite.mdl",
         .alias = "elite",
         .full_name = "Dual Beretta 96G Elite",
         .price = 800,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 1,
         .buy_select = 5,
         .buy_select_t = 5,
         .buy_select_ct = 5,
         .max_clip = 30,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::FiveSeven,
         .name = "weapon_fiveseven",
         .model = "fiveseven.mdl",
         .alias = "fn57",
         .full_name = "FN Five-Seven",
         .price = 750,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 1,
         .buy_select = 6,
         .buy_select_t = 5,
         .buy_select_ct = 5,
         .max_clip = 20,
         .type = WeaponType::Pistol
      },
      {
         .id = Weapon::M3,
         .name = "weapon_m3",
         .model = "m3.mdl",
         .alias = "m3",
         .full_name = "Benelli M3 Super90",
         .price = 1700,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::None,
         .buy_group = 2,
         .buy_select = 1,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .max_clip = 8,
         .type = WeaponType::Shotgun
      },
      {
         .id = Weapon::XM1014,
         .name = "weapon_xm1014",
         .model = "xm1014.mdl",
         .alias = "xm1014",
         .full_name = "Benelli XM1014",
         .price = 3000,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::None,
         .buy_group = 2,
         .buy_select = 2,
         .buy_select_t = 2,
         .buy_select_ct = 2,
         .max_clip = 7,
         .type = WeaponType::Shotgun
      },
      {
         .id = Weapon::MP5,
         .name = "weapon_mp5navy",
         .model = "mp5.mdl",
         .alias = "mp5",
         .full_name = "HK MP5-Navy",
         .price = 1500,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 3,
         .buy_select = 1,
         .buy_select_t = 2,
         .buy_select_ct = 2,
         .max_clip = 30,
         .type = WeaponType::SMG,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::TMP,
         .name = "weapon_tmp",
         .model = "tmp.mdl",
         .alias = "tmp",
         .full_name = "Steyr Tactical Machine Pistol",
         .price = 1250,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 3,
         .buy_select = 2,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .max_clip = 30,
         .type = WeaponType::SMG,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::P90,
         .name = "weapon_p90",
         .model = "p90.mdl",
         .alias = "p90",
         .full_name = "FN P90",
         .price = 2350,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 3,
         .buy_select = 3,
         .buy_select_t = 4,
         .buy_select_ct = 4,
         .max_clip = 50,
         .type = WeaponType::SMG,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::MAC10,
         .name = "weapon_mac10",
         .model = "mac10.mdl",
         .alias = "mac10",
         .full_name = "Ingram MAC-10",
         .price = 1400,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 3,
         .buy_select = 4,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .max_clip = 30,
         .type = WeaponType::SMG,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::UMP45,
         .name = "weapon_ump45",
         .model = "ump45.mdl",
         .alias = "ump45",
         .full_name = "HK UMP45",
         .price = 1700,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 3,
         .buy_select = 5,
         .buy_select_t = 3,
         .buy_select_ct = 3,
         .max_clip = 25,
         .type = WeaponType::SMG,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::AK47,
         .name = "weapon_ak47",
         .model = "ak47.mdl",
         .alias = "ak47",
         .full_name = "Automat Kalashnikov AK-47",
         .price = 2500,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 4,
         .buy_select = 1,
         .buy_select_t = 2,
         .buy_select_ct = 2,
         .penetrate_power = 2,
         .max_clip = 30,
         .type = WeaponType::Rifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::SG552,
         .name = "weapon_sg552",
         .model = "sg552.mdl",
         .alias = "sg552",
         .full_name = "Sig SG-552 Commando",
         .price = 3500,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::None,
         .buy_group = 4,
         .buy_select = 2,
         .buy_select_t = 4,
         .buy_select_ct = 4,
         .penetrate_power = 2,
         .max_clip = 30,
         .type = WeaponType::ZoomRifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::M4A1,
         .name = "weapon_m4a1",
         .model = "m4a1.mdl",
         .alias = "m4a1",
         .full_name = "Colt M4A1 Carbine",
         .price = 3100,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 4,
         .buy_select = 3,
         .buy_select_t = 3,
         .buy_select_ct = 3,
         .penetrate_power = 2,
         .max_clip = 30,
         .type = WeaponType::Rifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::Galil,
         .name = "weapon_galil",
         .model = "galil.mdl",
         .alias = "galil",
         .full_name = "IMI Galil",
         .price = 2000,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 4,
         .buy_select = -1,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .penetrate_power = 2,
         .max_clip = 35,
         .type = WeaponType::Rifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::Famas,
         .name = "weapon_famas",
         .model = "famas.mdl",
         .alias = "famas",
         .full_name = "GIAT FAMAS",
         .price = 2250,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 4,
         .buy_select = -1,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .penetrate_power = 2,
         .max_clip = 25,
         .type = WeaponType::Rifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::AUG,
         .name = "weapon_aug",
         .model = "aug.mdl",
         .alias = "aug",
         .full_name = "Steyr Aug",
         .price = 3500,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 4,
         .buy_select = 4,
         .buy_select_t = 4,
         .buy_select_ct = 4,
         .penetrate_power = 2,
         .max_clip = 30,
         .type = WeaponType::ZoomRifle,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::Scout,
         .name = "weapon_scout",
         .model = "scout.mdl",
         .alias = "scout",
         .full_name = "Steyr Scout",
         .price = 2750,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 4,
         .buy_select = 5,
         .buy_select_t = 3,
         .buy_select_ct = 2,
         .penetrate_power = 3,
         .max_clip = 10,
         .type = WeaponType::Sniper
      },
      {
         .id = Weapon::AWP,
         .name = "weapon_awp",
         .model = "awp.mdl",
         .alias = "awp",
         .full_name = "AI Arctic Warfare/Magnum",
         .price = 4750,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 4,
         .buy_select = 6,
         .buy_select_t = 5,
         .buy_select_ct = 6,
         .penetrate_power = 3,
         .max_clip = 10,
         .type = WeaponType::Sniper
      },
      {
         .id = Weapon::G3SG1,
         .name = "weapon_g3sg1",
         .model = "g3sg1.mdl",
         .alias = "g3sg1",
         .full_name = "HK G3/SG-1 Sniper Rifle",
         .price = 5000,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::Terrorist,
         .team_as = WeaponTeam::CT,
         .buy_group = 4,
         .buy_select = 7,
         .buy_select_t = 6,
         .buy_select_ct = 6,
         .penetrate_power = 3,
         .max_clip = 20,
         .type = WeaponType::Sniper
      },
      {
         .id = Weapon::SG550,
         .name = "weapon_sg550",
         .model = "sg550.mdl",
         .alias = "sg550",
         .full_name = "Sig SG-550 Sniper",
         .price = 4200,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 4,
         .buy_select = 8,
         .buy_select_t = 5,
         .buy_select_ct = 5,
         .penetrate_power = 3,
         .max_clip = 30,
         .type = WeaponType::Sniper
      },
      {
         .id = Weapon::M249,
         .name = "weapon_m249",
         .model = "m249.mdl",
         .alias = "m249",
         .full_name = "FN M249 Para",
         .price = 5750,
         .min_primary_ammo = 1,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::Terrorist,
         .buy_group = 5,
         .buy_select = 1,
         .buy_select_t = 1,
         .buy_select_ct = 1,
         .penetrate_power = 2,
         .max_clip = 100,
         .type = WeaponType::Heavy,
         .primary_fire_hold = true
      },
      {
         .id = Weapon::Shield,
         .name = "weapon_shield",
         .model = "shield.mdl",
         .alias = "shield",
         .full_name = "Tactical Shield",
         .price = 2200,
         .team_standard = WeaponTeam::CT,
         .team_as = WeaponTeam::CT,
         .buy_group = 8,
         .buy_select = -1,
         .buy_select_t = 8,
         .buy_select_ct = 8,
         .type = WeaponType::Pistol
      }
   };
  // clang-format on

  weapons_.clear ();

  for (const auto &weapon : kWeaponTable) {
    weapons_.emplace (weapon);
  }

  // override defaults with gamedef.cfg definitions (weapons + sound templates)
  LoadGameDefConfig ();

  // weapon.cfg goes last, so it's able to override gamedef.cfg team availability
  LoadWeaponsConfig ();
}

void Config::AdjustWeaponPrices () {
  // elite price is 1000$ on older versions of cs
  if (!game.Is (GameFlags::Legacy)) {
    return;
  }

  for (auto &weapon : weapons_) {
    if (weapon.id == Weapon::Elite && weapon.price == 800) {
      weapon.price = 1000;
      break;
    }
  }
}

WeaponInfo &Config::FindWeaponById (const Weapon id) {
  for (auto &weapon : weapons_) {
    if (weapon.id == id) {
      return weapon;
    }
  }
  return weapons_.at (0);
}

int32_t Config::FindWeaponIndexById (const Weapon id) {
  for (size_t i = 0; i < weapons_.size (); ++i) {
    if (weapons_[i].id == id) {
      return static_cast<int32_t> (i);
    }
  }
  return 0;
}

Weapon Config::LookupWeaponId (ystl::StringRef name) {
  for (const auto &entry : kWeaponIdNames) {
    if (ystl::strings.matches (name.chars (), entry.name.chars ())) {
      return entry.id;
    }
  }
  return Weapon::Invalid;
}

ystl::StringRef Config::LookupWeaponIdName (const Weapon id) {
  for (const auto &entry : kWeaponIdNames) {
    if (entry.id == id) {
      return entry.name;
    }
  }
  return "Unknown";
}

WeaponType Config::LookupWeaponType (ystl::StringRef name, WeaponType def) {
  static constexpr struct {
    ystl::StringRef name {};
    WeaponType type {};
  } kNames[] = {
    { "Melee",     WeaponType::Melee     },
    { "Pistol",    WeaponType::Pistol    },
    { "Shotgun",   WeaponType::Shotgun   },
    { "ZoomRifle", WeaponType::ZoomRifle },
    { "Rifle",     WeaponType::Rifle     },
    { "SMG",       WeaponType::SMG       },
    { "Sniper",    WeaponType::Sniper    },
    { "Heavy",     WeaponType::Heavy     },
    { "None",      WeaponType::None      }
  };

  for (const auto &entry : kNames) {
    if (ystl::strings.matches (name.chars (), entry.name.chars ())) {
      return entry.type;
    }
  }
  return def;
}

WeaponTeam Config::LookupWeaponTeam (ystl::StringRef name, WeaponTeam def) {
  static constexpr struct {
    ystl::StringRef name {};
    WeaponTeam team {};
  } kNames[] = {
    { "Banned",    WeaponTeam::None      },
    { "None",      WeaponTeam::None      },
    { "Terrorist", WeaponTeam::Terrorist },
    { "T",         WeaponTeam::Terrorist },
    { "CT",        WeaponTeam::CT        },
    { "Both",      WeaponTeam::Both      }
  };

  for (const auto &entry : kNames) {
    if (ystl::strings.matches (name.chars (), entry.name.chars ())) {
      return entry.team;
    }
  }
  return def;
}

Noise Config::LookupNoiseFlags (ystl::StringRef value) {
  // noise classification names
  static constexpr struct {
    ystl::StringRef name {};
    Noise flag {};
  } kNames[] = {
    { "HitFall",    Noise::HitFall    },
    { "Pickup",     Noise::Pickup     },
    { "Zoom",       Noise::Zoom       },
    { "Ammo",       Noise::Ammo       },
    { "Hostage",    Noise::Hostage    },
    { "Broke",      Noise::Broke      },
    { "Door",       Noise::Door       },
    { "Defuse",     Noise::Defuse     },
    { "SGDetonate", Noise::SGDetonate },
    { "WeaponFire", Noise::WeaponFire },
    { "Footstep",   Noise::Footstep   },
    { "Explosion",  Noise::Explosion  },
    { "Ricochet",   Noise::Ricochet   },
    { "Misc",       Noise::Misc       }
  };

  Noise result {};
  const auto has_comma = value.find (",") != ystl::String::InvalidIndex;

  for (auto &token : value.split<ystl::String> (has_comma ? "," : " ")) {
    token.trim ();

    if (token.empty ()) {
      continue;
    }
    bool matched = false;

    for (const auto &entry : kNames) {
      if (ystl::strings.matches (token.chars (), entry.name.chars ())) {
        result |= entry.flag;
        matched = true;

        break;
      }
    }

    if (!matched) {
      ystl::logger.error ("gamedef.%s: unknown noise flag '%s'.", kConfigExtension, token.chars ());
    }
  }
  return result;
}

ConfigRead Config::ReadConfigText (
  ystl::StringRef file_name, ystl::StringRef error_if_not_exists, ystl::String &out_text, bool language_dependant) {
  ystl::MemFile file {};

  if (!OpenConfig (file_name, error_if_not_exists, &file, language_dependant)) {

    // gone files must re-run their fallback every load (and reparse if they reappear)
    ForgetConfigSnapshot (file_name);
    return ConfigRead::Missing;
  }

  // file is already fully loaded in memory (pfnloadfileforme), hand it over in a single copy
  out_text.assign (file.view ());

  if (!CheckConfigVersion (file_name, out_text)) {
    ForgetConfigSnapshot (file_name);
    return ConfigRead::Missing;
  }

  const auto hash = out_text.hash ();
  const auto size = out_text.size ();

  for (const auto &snap : config_snapshots_) {
    if (snap.name == file_name) {
      return (snap.hash == hash && snap.size == size) ? ConfigRead::Unchanged : ConfigRead::Ready;
    }
  }
  return ConfigRead::Ready;
}

void Config::MarkConfigApplied (ystl::StringRef file_name, ystl::StringRef text) {
  const auto hash = text.hash ();
  const auto size = text.size ();

  for (auto &snap : config_snapshots_) {
    if (snap.name == file_name) {
      snap.hash = hash;
      snap.size = size;

      return;
    }
  }
  ConfigSnapshot snap {};

  snap.name = file_name;
  snap.hash = hash;
  snap.size = size;

  config_snapshots_.push (ystl::move (snap));
}

void Config::ForgetConfigSnapshot (ystl::StringRef file_name) {
  config_snapshots_.erase_if ([&file_name] (const ConfigSnapshot &snap) {
    return snap.name == file_name;
  });
}

bool Config::CheckConfigVersion (ystl::StringRef file_name, ystl::StringRef text) {
  // rejects configs saved by outdated bot versions, before they reach the parser

  const auto marker = text.find ("@version");

  if (marker == ystl::String::InvalidIndex) {
    return true; // no version marker, assume a hand-made config
  }

  // only scan a small window after the marker, config headers are short
  const auto length = text.size ();
  const auto limit = ystl::min (marker + 64, length);

  auto major = 0, minor = 0;
  auto ch = marker + 8; // length of "@version"

  while (ch < limit && (text[ch] < '0' || text[ch] > '9')) {
    ++ch;
  }

  while (ch < limit && text[ch] >= '0' && text[ch] <= '9') {
    major = major * 10 + (text[ch] - '0');
    ++ch;
  }

  if (ch < limit && text[ch] == '.') {
    ++ch;

    while (ch < limit && text[ch] >= '0' && text[ch] <= '9') {
      minor = minor * 10 + (text[ch] - '0');
      ++ch;
    }
  }

  if (major < kMinimalConfigVersionMajor || (major == kMinimalConfigVersionMajor && minor < kMinimalConfigVersionMinor)) {
    ystl::logger.error (
      "%s.%s: configuration is from an outdated bot version (found %d.%d, minimum required %d.%d). Please remove or update your "
      "bot configs, so they're regenerated.",
      file_name.chars (), kConfigExtension, major, minor, kMinimalConfigVersionMajor, kMinimalConfigVersionMinor);
    return false;
  }
  return true;
}

void Config::LoadGameDefConfig () {
  SetupMemoryFiles ();

  ystl::String text {};

  if (ReadConfigText ("gamedef", "Game definition config file not found. Loading built-in defaults.", text) != ConfigRead::Ready) {
    return;
  }
  ystl::ConfParser parser;

  if (!parser.parse (text)) {
    ystl::logger.error ("gamedef.%s: %s", kConfigExtension, parser.error ().chars ());
    return;
  }

  // keep the document alive: weapon names/models and sound prefixes reference its strings
  gamedef_ = parser.take_document ();

  ApplyWeaponDefs (gamedef_.find ("Weapons"));
  ApplySoundDefs (gamedef_.find ("HearableSounds"));

  util.ApplySentenceDefs (gamedef_.find ("Sentences"));
}

void Config::ApplyWeaponDefs (const ystl::ConfNode *root) {
  if (!root) {
    return;
  }
  for (const auto &entry : root->children ()) {
    if (!entry->is_block ()) {
      continue;
    }
    const auto id = LookupWeaponId (entry->name ());

    if (id == Weapon::Invalid) {
      ystl::logger.error ("gamedef.%s: unknown weapon '%s'. Entry ignored.", kConfigExtension, entry->name ().chars ());
      continue;
    }
    auto &weapon = FindWeaponById (id);

    weapon.name = entry->get_string ("Name", weapon.name);
    weapon.model = entry->get_string ("Model", weapon.model);
    weapon.alias = entry->get_string ("Alias", weapon.alias);
    weapon.full_name = entry->get_string ("FullName", weapon.full_name);
    weapon.price = entry->get_int ("Price", weapon.price);
    weapon.min_primary_ammo = entry->get_int ("MinPrimaryAmmo", weapon.min_primary_ammo);
    weapon.buy_group = entry->get_int ("BuyGroup", weapon.buy_group);
    weapon.buy_select = entry->get_int ("BuySelect", weapon.buy_select);
    weapon.buy_select_t = entry->get_int ("BuySelectT", weapon.buy_select_t);
    weapon.buy_select_ct = entry->get_int ("BuySelectCT", weapon.buy_select_ct);
    weapon.penetrate_power = entry->get_int ("PenetratePower", weapon.penetrate_power);
    weapon.max_clip = entry->get_int ("MaxClip", weapon.max_clip);
    weapon.primary_fire_hold = entry->get_bool ("PrimaryFireHold", weapon.primary_fire_hold);

    if (auto *value = entry->find ("TeamStandard")) {
      weapon.team_standard = LookupWeaponTeam (value->value (), weapon.team_standard);
    }
    if (auto *value = entry->find ("TeamAS")) {
      weapon.team_as = LookupWeaponTeam (value->value (), weapon.team_as);
    }
    if (auto *value = entry->find ("Type")) {
      weapon.type = LookupWeaponType (value->value (), weapon.type);
    }
  }
}

void Config::ApplySoundDefs (const ystl::ConfNode *root) {
  if (!root) {
    return;
  }
  ystl::Array<SoundTemplate> templates {};

  for (const auto &entry : root->children ()) {
    if (!entry->is_block ()) {
      continue;
    }
    SoundTemplate tmpl {};

    // section name is the sound prefix; string storage is owned by m_gamedef
    tmpl.prefix = entry->name ().chars ();
    tmpl.flags = LookupNoiseFlags (entry->get_string ("Flags"));

    if (tmpl.flags == Noise (0)) {
      ystl::logger.error ("gamedef.%s: sound template '%s' has no valid flags. Entry ignored.", kConfigExtension, entry->name ().chars ());
      continue;
    }
    tmpl.base_radius = entry->get_float ("Radius", 2048.0f);
    tmpl.duration = entry->get_float ("Duration", 2.0f);

    templates.push (tmpl);
  }

  // replace built-in templates only when the section has valid entries
  if (!templates.empty ()) {
    sounds.ApplyTemplates (ystl::move (templates));
  }
}

const char *Config::Translate (ystl::StringRef input) {
  // this function translate input string into needed language

  if (ctrl.IgnoreTranslate ()) {
    return input.chars ();
  }

  // english is the source language and has no language file, so there is nothing to translate or collect
  if (language_.empty ()) {
    return input.chars ();
  }
  auto hash = HashLangString (input.chars ());

  if (language_.exists (hash)) {
    return language_[hash].chars ();
  }

  // remember untranslated strings, so they can be listed with 'yb debug translate'
  if (!missing_translations_.exists (hash)) {
    missing_translations_[hash] = input.chars ();
  }
  return input.chars (); // nothing found
}

void Config::ShowCustomValues () {
  ctrl.Msg ("Current values for custom config items:");

  for (const auto &[key, val] : custom_) {
    ctrl.Msg ("  %s = %s", key, val);
  }
}

uint32_t Config::HashLangString (ystl::StringRef str) {
  auto test = [] (const char ch) {
    return (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
  };

  auto hash = 0x811c9dc5u;
  auto count = size_t (0);

  for (const auto &ch : str) {
    if (!test (ch)) {
      continue;
    }
    hash = ystl::detail::fnv1a32_step (hash, ch);
    ++count;
  }
  return count > 0 ? hash : 0;
}

ystl::String Config::MakeTranslateLabel (ystl::StringRef original, uint32_t hash) {
  ystl::String label {};

  for (const auto &word : original.split (" ")) {
    ystl::String clean {};
    auto uppercase = true;

    for (const auto &ch : word) {
      if (ch < '0' || (ch > '9' && ch < 'A') || (ch > 'Z' && ch < 'a') || ch > 'z') {
        continue;
      }
      if (uppercase && ch >= 'a' && ch <= 'z') {
        clean += static_cast<char> (ch - 'a' + 'A');
      }
      else {
        clean += ch;
      }
      uppercase = false;
    }
    if (clean.empty ()) {
      continue;
    }
    label += clean;

    if (label.size () >= 24) {
      break;
    }
  }
  if (label.empty ()) {
    label.assignf ("Entry%08x", hash);
  }
  return label;
}

int Config::WriteMissingTranslations () {
  if (missing_translations_.empty ()) {
    return 0;
  }
  // locate the language file, mirroring openconfig paths
  const auto lang_path = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, folders.lang,
    ystl::strings.format ("%s_%s.%s", cv_language.As<ystl::StringRef> (), "lang", kConfigExtension));

  ystl::String text {};
  ystl::MemFile file {};

  if (file.open (lang_path)) {
    text.assign (file.view ());
    file.close ();
  }
  ystl::ConfParser parser {};

  if (!text.empty () && !parser.parse (text)) {
    ystl::logger.error ("lang.%s: %s", kConfigExtension, parser.error ().chars ());
    return -1;
  }
  auto doc = parser.take_document ();

  // flat layout: entries live at the document root; legacy files keep appending inside Lang block
  auto *lang = doc.find ("Lang");

  if (!lang) {
    lang = &doc;
  }
  auto count = 0;

  for (const auto &[hash, original] : missing_translations_) {
    auto *entry = &lang->add_block (MakeTranslateLabel (original, hash));

    // original acts as the translation until the file gets translated and reloaded
    auto *source = &entry->add_raw_block ("Original");

    for (const auto &line : original.split ("\n")) {
      if (!line.empty ()) {
        source->add_item (line);
      }
    }
    auto *translated = &entry->add_raw_block ("Translated");

    for (const auto &line : original.split ("\n")) {
      if (!line.empty ()) {
        translated->add_item (line);
      }
    }
    ++count;
  }

  // brand-new file has no header yet; writer drops root comments, so attach them to the first entry
  if (text.empty () && !doc.children ().empty ()) {
    // major.minor like in the shipped config headers, but never below the version the bot accepts
    const auto parts = product.version.split (".");
    const auto major = !parts.empty () ? parts[0].as<int> () : 0;
    const auto minor = parts.size () > 1 ? parts[1].as<int> () : 0;

    ystl::String version = ystl::strings.format ("%d.%d", major, minor);

    if (major < kMinimalConfigVersionMajor || (major == kMinimalConfigVersionMajor && minor < kMinimalConfigVersionMinor)) {
      version.assignf ("%d.%d", kMinimalConfigVersionMajor, kMinimalConfigVersionMinor);
    }
    auto *first = doc.children ()[0];

    first->add_comment (ystl::strings.format ("@package: %s", product.name.chars ()));
    first->add_comment (ystl::strings.format ("@version: %s", version.chars ()));
    first->add_comment (ystl::strings.format ("@author: %s", product.author.chars ()));
  }
  const auto serialized = ystl::ConfWriter::write (doc);

  // keep a backup of the previous file before overwriting
  if (!text.empty ()) {
    ystl::File backup {};

    if (backup.open (ystl::strings.format ("%s.bak", lang_path.chars ()), "wb")) {
      backup.write (text.chars (), text.size (), 1);
      backup.close ();
    }
  }
  ystl::File output {};

  if (!output.open (lang_path, "wb")) {
    ystl::logger.error ("Couldn't write language configuration file.");
    return -1;
  }
  output.write (serialized.chars (), serialized.size (), 1);
  output.close ();

  // originals act as translations until the file gets translated and reloaded
  for (const auto &[hash, original] : missing_translations_) {
    language_[hash] = original;
  }
  missing_translations_.clear ();

  return count;
}

bool Config::OpenConfig (
  ystl::StringRef file_name, ystl::StringRef error_if_not_exists, ystl::MemFile *out_file, bool language_dependant /*= false*/) {
  if (*out_file) {
    out_file->close ();
  }

  // save config dir
  auto config_dir = ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config);

  if (language_dependant) {
    if (file_name.starts_with ("lang") && cv_language.As<ystl::StringRef> () == "en") {
      return false;
    }
    auto lang_config = ystl::strings.join_path (
      config_dir, folders.lang, ystl::strings.format ("%s_%s.%s", cv_language.As<ystl::StringRef> (), file_name, kConfigExtension));

    // check is file is exists for this language
    if (!out_file->open (lang_config)) {
      out_file->open (ystl::strings.join_path (config_dir, folders.lang, ystl::strings.format ("en_%s.%s", file_name, kConfigExtension)));
    }
  }
  else {
    out_file->open (ystl::strings.join_path (config_dir, ystl::strings.format ("%s.%s", file_name, kConfigExtension)));
  }

  if (!*out_file) {
    ystl::logger.error (error_if_not_exists.chars ());
    return false;
  }
  return true;
}

} // namespace bot
