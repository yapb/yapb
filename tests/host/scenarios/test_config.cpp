//
// YaPB test host: unit/config_{lookups,files}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for config.cpp: weapon id/type/team/noise lookups,
// table accessors, price tweaks, def application, snapshot/version
// machinery, names, chat banks, custom values and translations.
// Randomness is pinned via exact modes and extreme
// chances, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct ConfigHook {
  static Weapon LookupId (ystl::StringRef name) {
    return Config::LookupWeaponId (name);
  }
  static ystl::StringRef LookupIdName (Weapon id) {
    return Config::LookupWeaponIdName (id);
  }
  static WeaponType LookupType (ystl::StringRef name, WeaponType def) {
    return Config::LookupWeaponType (name, def);
  }
  static WeaponTeam LookupTeam (ystl::StringRef name, WeaponTeam def) {
    return Config::LookupWeaponTeam (name, def);
  }
  static Noise LookupNoise (ystl::StringRef value) {
    return Config::LookupNoiseFlags (value);
  }
  static void ApplyWeapons (const ystl::ConfNode *root) {
    conf.ApplyWeaponDefs (root);
  }
  static void ApplySounds (const ystl::ConfNode *root) {
    conf.ApplySoundDefs (root);
  }
  static uint32_t HashLang (ystl::StringRef str) {
    return conf.HashLangString (str);
  }
  static ystl::String MakeLabel (ystl::StringRef original, uint32_t hash) {
    return Config::MakeTranslateLabel (original, hash);
  }
  static const SoundTemplate *ClassifySound (ystl::StringRef sample) {
    return sounds.Classify (sample);
  }
  static size_t ChatterBankCount () {
    return conf.chatter_.size ();
  }
  static ConfigRead ReadText (ystl::StringRef file_name, ystl::StringRef error_if_not_exists, ystl::String &out_text, bool language_dependant) {
    return conf.ReadConfigText (file_name, error_if_not_exists, out_text, language_dependant);
  }
  static void ForgetSnapshot (ystl::StringRef file_name) {
    conf.ForgetConfigSnapshot (file_name);
  }
  static void MarkApplied (ystl::StringRef file_name, ystl::StringRef text) {
    conf.MarkConfigApplied (file_name, text);
  }
  static bool CheckVersion (ystl::StringRef file_name, ystl::StringRef text) {
    return conf.CheckConfigVersion (file_name, text);
  }
};

namespace {

void BootConfig (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  conf.InitWeapons ();
}

ystl::String ConfFile (ystl::StringRef name) {
  return ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, ystl::strings.format ("%s.%s", name, kConfigExtension));
}

ystl::String ConfLangFile (ystl::StringRef name) {
  return ystl::strings.join_path (
    bstor.GetRunningPathVfs (), folders.config, folders.lang, ystl::strings.format ("%s.%s", name, kConfigExtension));
}

void WriteConfFile (const ystl::String &path, ystl::StringRef text) {
  ystl::File::make_path (ystl::String (path.substr (0, path.find_last_of (kPathSeparator))).chars ());

  ystl::File file (path, "wb");
  HOST_REQUIRE (!!file);
  file.write (text.chars (), text.size (), 1);
  file.close ();
}

ystl::String MapSpecificFile () {
  return ystl::strings.join_path (bstor.GetRunningPath (), folders.config, "maps", ystl::strings.format ("de_test.%s", kConfigExtension));
}

ystl::String MaterialsFile () {
  return ystl::strings.join_path ("sound", "materials.txt");
}

ystl::String WavePath (ystl::StringRef name) {
  return ystl::strings.join_path (cv_chatter_path.As<ystl::StringRef> (), ystl::strings.format ("%s.wav", name));
}

void WriteWaveFile (const ystl::String &path) {
  testhost::MakeDirs (path.substr (0, path.find_last_of ("/\\")));

  // 8000 Hz 8-bit mono, 8000 frames: exactly one second of detectable audio...
  ystl::String wav {};

  wav += "RIFF";

  auto push_le32 = [&] (uint32_t value) {
    wav += static_cast<char> (value & 0xffu);
    wav += static_cast<char> ((value >> 8) & 0xffu);
    wav += static_cast<char> ((value >> 16) & 0xffu);
    wav += static_cast<char> ((value >> 24) & 0xffu);
  };

  auto push_le16 = [&] (uint32_t value) {
    wav += static_cast<char> (value & 0xffu);
    wav += static_cast<char> ((value >> 8) & 0xffu);
  };

  push_le32 (8036u); // chunk size: header remainder plus 8000 frames
  wav += "WAVE";
  wav += "fmt ";
  push_le32 (16u);
  push_le16 (1u); // pcm
  push_le16 (1u); // mono
  push_le32 (8000u); // sample rate
  push_le32 (8000u); // byte rate
  push_le16 (1u); // block align
  push_le16 (8u); // bits per sample
  wav += "data";
  push_le32 (8000u);

  for (int i = 0; i < 8000; ++i) {
    wav += static_cast<char> (0x80);
  }
  ystl::File file (path, "wb");
  HOST_REQUIRE (!!file);
  file.write (wav.chars (), wav.size (), 1);
  file.close ();
}

} // namespace

TEST_CASE ("unit/config_lookups") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  // tables are up, AK47 resolves both ways...
  HOST_REQUIRE (conf.GetWeapons ().size () > 0);
  CHECK (conf.FindWeaponById (Weapon::AK47).id == Weapon::AK47);
  CHECK (conf.GetWeapons ()[conf.FindWeaponIndexById (Weapon::AK47)].id == Weapon::AK47);
  CHECK (conf.FindWeaponById (Weapon::Invalid).id == Weapon::Knife); // miss falls back to first

  CHECK (ConfigHook::LookupId ("AK47") == Weapon::AK47);
  CHECK (ConfigHook::LookupId ("NoSuchWeapon") == Weapon::Invalid);
  CHECK (ConfigHook::LookupIdName (Weapon::AK47) == "AK47");
  CHECK (ConfigHook::LookupIdName (Weapon::Invalid) == "Unknown");

  // ...types and teams pass defaults through on unknown names...
  CHECK (ConfigHook::LookupType ("Rifle", WeaponType::None) == WeaponType::Rifle);
  CHECK (ConfigHook::LookupType ("Bogus", WeaponType::Sniper) == WeaponType::Sniper);
  CHECK (ConfigHook::LookupTeam ("CT", WeaponTeam::Both) == WeaponTeam::CT);
  CHECK (ConfigHook::LookupTeam ("Bogus", WeaponTeam::Both) == WeaponTeam::Both);
  CHECK (conf.GetWeaponType (Weapon::AK47) == WeaponType::Rifle);

  // ...noise flags combine comma and space lists, unknowns empty out...
  CHECK (ConfigHook::LookupNoise ("WeaponFire") == Noise::WeaponFire);
  CHECK (ConfigHook::LookupNoise ("WeaponFire, Explosion") == (Noise::WeaponFire | Noise::Explosion));
  CHECK (ConfigHook::LookupNoise ("WeaponFire Explosion") == (Noise::WeaponFire | Noise::Explosion));
  CHECK (ConfigHook::LookupNoise ("Bogus") == Noise (0));

  // ...preference tables keep their shape across personalities...
  CHECK (conf.GetWeaponPrefs (Personality::Normal).size () == 26);
  CHECK (conf.GetWeaponPrefs (Personality::Rusher).size () == 26);
  CHECK (conf.GetWeaponPrefs (Personality::Careful).size () == 26);

  // ...custom values fall back to defaults, then to empty...
  conf.LoadCustomConfig ();
  CHECK (conf.FetchCustom ("C4ModelName") == "c4.mdl");
  CHECK (conf.GetBombModelName () == "c4.mdl");
  CHECK (conf.FetchCustom ("NoSuchKey") == "");
  CHECK (conf.GetRandomAvatar () == "");

  // ...elite pricing normalizes to 1000 on legacy builds, frozen elsewhere...
  const int elite_before = conf.FindWeaponById (Weapon::Elite).price;
  conf.AdjustWeaponPrices ();

  if (game.Is (GameFlags::Legacy)) {
    CHECK (conf.FindWeaponById (Weapon::Elite).price == 1000);
  }
  else {
    CHECK (conf.FindWeaponById (Weapon::Elite).price == elite_before);
  }

  // ...defs apply onto the live table and ignore garbage...
  {
    const int ak_before = conf.FindWeaponById (Weapon::AK47).price;

    ystl::ConfParser parser {};
    HOST_REQUIRE (parser.parse ("AK47 { Price = 1234 }\nBogus { Price = 1 }\n"));

    ConfigHook::ApplyWeapons (&parser.document ());
    CHECK (conf.FindWeaponById (Weapon::AK47).price == 1234);

    conf.FindWeaponById (Weapon::AK47).price = ak_before;
    ConfigHook::ApplyWeapons (nullptr);
    CHECK (conf.FindWeaponById (Weapon::AK47).price == ak_before);
  }

  // ...sound templates reject flagless entries, novel prefixes apply...
  CHECK (ConfigHook::ClassifySound ("zzcfgtest-1.wav") == nullptr);

  {
    ystl::ConfParser parser {};
    HOST_REQUIRE (parser.parse ("Noisy { Flags = BogusFlag }\n"));
    ConfigHook::ApplySounds (&parser.document ());
    CHECK (ConfigHook::ClassifySound ("zzcfgtest-1.wav") == nullptr);
  }

  {
    ystl::ConfParser parser {};
    HOST_REQUIRE (parser.parse ("zzcfgtest { Flags = WeaponFire\n Radius = 100.0\n Duration = 0.5 }\n"));
    ConfigHook::ApplySounds (&parser.document ());

    const SoundTemplate *tmpl = ConfigHook::ClassifySound ("zzcfgtest-1.wav");
    HOST_REQUIRE (tmpl != nullptr);
    CHECK (tmpl->base_radius == 100.0f);
    CHECK (tmpl->duration == 0.5f);
    ConfigHook::ApplySounds (nullptr);
  }
}

TEST_CASE ("unit/config_difficulty") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);
  conf.LoadDifficultyConfig ();

  // curve never inverts: higher level aims faster and hits harder...
  const DifficultyData *prev = nullptr;

  for (int i = ystl::to_underlying (Difficulty::Noob); i <= ystl::to_underlying (Difficulty::Expert); ++i) {
    const auto *cur = conf.GetDifficultyTweaks (static_cast<Difficulty> (i));
    HOST_REQUIRE (cur != nullptr);

    if (prev != nullptr) {
      CHECK (cur->reaction[1] <= prev->reaction[1]);
      CHECK (cur->headshot_pct >= prev->headshot_pct);
      CHECK (cur->seen_thru_pct >= prev->seen_thru_pct);
      CHECK (cur->hear_thru_pct >= prev->hear_thru_pct);
      CHECK (cur->max_recoil <= prev->max_recoil);
      CHECK (cur->aim_error.length () <= prev->aim_error.length ());
    }
    prev = cur;
  }

  // ...low tiers still fight back: sub-1.5s reactions, nonzero wall awareness...
  const auto *noob = conf.GetDifficultyTweaks (Difficulty::Noob);
  HOST_REQUIRE (noob != nullptr);
  CHECK (noob->reaction[1] <= 1.5f);
  CHECK (noob->seen_thru_pct > 0);
  CHECK (noob->hear_thru_pct > 0);

  // ...top tier stays crisp: zero aim error, sub-0.35s reactions...
  const auto *expert = conf.GetDifficultyTweaks (Difficulty::Expert);
  HOST_REQUIRE (expert != nullptr);
  CHECK (expert->reaction[1] <= 0.35f);
  CHECK (expert->aim_error.length () < 0.01f);
}

TEST_CASE ("unit/config_files") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  // missing files never open...
  ystl::MemFile missing {};
  CHECK (!conf.OpenConfig ("nosuchconfig", "gone", &missing, false));

  // ...snapshots flip Ready/Unchanged on content, versions gate entry...
  WriteConfFile (ConfFile ("testcfg"), "hello test\n");

  ystl::String text {};
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);

  // snapshots are recorded by loaders, not reads...
  ConfigHook::MarkApplied ("testcfg", text);
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Unchanged);

  WriteConfFile (ConfFile ("testcfg"), "hello changed\n");
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);

  ConfigHook::ForgetSnapshot ("testcfg");
  ConfigHook::MarkApplied ("testcfg", text);
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Unchanged);
  ConfigHook::ForgetSnapshot ("testcfg");
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);

  CHECK (ConfigHook::CheckVersion ("testcfg", "plain handmade config"));
  CHECK (!ConfigHook::CheckVersion ("testcfg", "; @version 0.1\nold = 1\n"));
  CHECK (ConfigHook::CheckVersion ("testcfg", "; @version 99.99\nnew = 1\n"));
  ystl::plat.remove_file (ConfFile ("testcfg").chars ());
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Missing);

  // ...a single-name roster is fully deterministic...
  WriteConfFile (ConfFile ("names"), "raw Names {\n   Solo\n}\n");
  conf.LoadNamesConfig ();

  Name *first = conf.PickBotName ();
  HOST_REQUIRE (first != nullptr && !first->name.empty ());
  CHECK (first->name == "Solo");

  conf.SetBotNameUsed (0, first->name);
  CHECK (conf.PickBotName () == nullptr); // the only name is taken

  // direct bot for the index-keyed release (manager-less, like navigate)
  graph.Reset ();

  for (int i = 0; i < 5; ++i) {
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

  edict_t *ent = game.CreateFakeClient ("CfgBot");
  HOST_REQUIRE (!game.IsNullEntity (ent));

  auto bot = ystl::make_unique<Bot> (ent, Difficulty::Normal, Personality::Normal, Team::CT, Skin::Random, false);
  HOST_REQUIRE (bot.get () != nullptr && bot->Index () == 0);

  conf.ClearUsedName (bot.get ());
  Name *freed = conf.PickBotName ();
  HOST_REQUIRE (freed != nullptr);
  CHECK (freed->name == "Solo");
  ystl::plat.remove_file (ConfFile ("names").chars ());

  // ...chat banks cycle their lines, empty banks stay quiet...
  CHECK (!conf.HasChatBank (Chat::Kill));
  CHECK (conf.PickRandomFromChatBank (Chat::Kill) == "");

  WriteConfFile (ConfLangFile ("en_chat"), "raw Killed {\n   phi one\n   phi two\n}\n");
  conf.LoadChatConfig ();
  CHECK (conf.HasChatBank (Chat::Kill));
  CHECK (!conf.HasChatBank (Chat::Dead));

  bool saw_one = false, saw_two = false;

  for (int i = 0; i < 2; ++i) {
    const auto line = conf.PickRandomFromChatBank (Chat::Kill);
    saw_one = saw_one || line == "phi one";
    saw_two = saw_two || line == "phi two";
  }
  CHECK (saw_one && saw_two);
  ystl::plat.remove_file (ConfLangFile ("en_chat").chars ());

  // ...custom values override defaults from a fixture...
  WriteConfFile (ConfFile ("custom"), "TestSection {\n   TestKey = TestValue\n}\n");
  conf.LoadCustomConfig ();
  CHECK (conf.FetchCustom ("TestKey") == "TestValue");
  CHECK (conf.FetchCustom ("C4ModelName") == "c4.mdl");
  ystl::plat.remove_file (ConfFile ("custom").chars ());
  conf.LoadCustomConfig ();

  // ...legacy builds skip language loading entirely (english only)...
  HOST_REQUIRE (game.Is (GameFlags::Legacy));
  conf.LoadLanguageConfig ();
  CHECK (ystl::StringRef (conf.Translate ("Hello test")) == "Hello test");
  CHECK (conf.MissingTranslations ().empty ());
  CHECK (conf.WriteMissingTranslations () == 0);
  conf.ResetMissingTranslations ();

  CHECK (ConfigHook::HashLang ("abc") == ConfigHook::HashLang ("abc"));
  CHECK (ConfigHook::MakeLabel ("hello world", 1) == "HelloWorld");

  // ...everything else loads (or skips) without a crash...
  conf.LoadWeaponsConfig ();
  conf.LoadChatterConfig ();
  conf.LoadDifficultyConfig ();
  conf.LoadAvatarsConfig ();
  conf.LoadLogosConfig ();
  conf.LoadMapSpecificConfig ();
  conf.LoadMainConfig (false);
  conf.LoadGameDefConfig ();
  HOST_REQUIRE (conf.GetWeapons ().size () > 0);

  // ...full load falls back to builtins with no files around...
  conf.LoadConfigs ();
  HOST_REQUIRE (conf.PickBotName () != nullptr);
}

TEST_CASE ("unit/config_listen") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // listen server: the first load binds the bot menu key
  engine.SetDedicatedServer (false);
  engine.SetCvar ("bind_menu_key", "g");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  // pfnGameInit is where the first main-config load runs
  table.pfnGameInit ();

  CHECK (!game.IsDedicatedServer ());

  bool saw_bind = false;

  for (const auto &cmd : engine.ServerCommands ()) {
    if (cmd.find ("bind") != ystl::String::InvalidIndex && cmd.find ("menu") != ystl::String::InvalidIndex) {
      saw_bind = true;
    }
  }
  CHECK (saw_bind);

  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/config_mainstay") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  // listen boot: the dedicated flag is cached on first query, unlock before anything asks...
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  engine.SetDedicatedServer (false);

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // fixtures next: the server activation runs the first main load, which samples the disable flag...
  WriteConfFile (ConfFile ("custom"), "DisableLogFile = yes\n");
  WriteConfFile (ConfFile ("yapb"), "; main config fixture\n\nyb_debug 1\nyb_quota 5\nyb_autovacate 0\nyb_no_such_cvar hello\n");

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  conf.InitWeapons ();

  // pin the ignore list and quota: re-apply deterministically on top of whatever the boot left...
  const ystl::String ignore_before = cv_ignore_cvars_on_changelevel.As<ystl::StringRef> ();
  cv_ignore_cvars_on_changelevel.Set ("yb_quota,yb_autovacate");
  cv_quota.Set (9);

  // the boot load already applied the fixture: stored values stick, ignored ones keep theirs...
  conf.LoadMainConfig (false);

  // ...a stored debug value applies, ignored cvars keep their live values...
  CHECK (cv_debug.As<int> () == 1);
  CHECK (cv_quota.As<int> () == 9);
  CHECK (cv_autovacate.As<int> () == 1);

  bool saw_bind = false;

  for (const auto &cmd : engine.ServerCommands ()) {
    if (cmd.find ("bind") != ystl::String::InvalidIndex && cmd.find ("menu") != ystl::String::InvalidIndex) {
      saw_bind = true;
    }
  }
  CHECK (saw_bind);

  // ...zero quota preserves the incoming value instead of the live one...
  cv_quota.Set (0);
  WriteConfFile (ConfFile ("yapb"), "; quota fixture\nyb_quota 5\n");
  conf.LoadMainConfig (false);
  CHECK (cv_quota.As<int> () == 5);
  cv_quota.Set (9);

  // ...the first load executes every line verbatim...
  WriteConfFile (ConfFile ("yapb"), "; first load fixture\nyb_debug 0\n");
  conf.LoadMainConfig (true);

  bool saw_debug = false;

  for (const auto &cmd : engine.ServerCommands ()) {
    if (cmd.find ("yb_debug 0") != ystl::String::InvalidIndex) {
      saw_debug = true;
    }
  }
  CHECK (saw_debug);

  // ...a missing file falls back to saving cvars...
  ystl::plat.remove_file (ConfFile ("yapb").chars ());
  conf.LoadMainConfig (false);

  ystl::plat.remove_file (ConfFile ("custom").chars ());
  conf.LoadCustomConfig ();

  ystl::logger.set_log_write_enabled (true);
  cv_debug.Set (0);
  cv_quota.Set (9);
  cv_ignore_cvars_on_changelevel.Set (ignore_before.chars ());
  engine.SetDedicatedServer (true);

  bots.Destroy ();
}

TEST_CASE ("unit/config_armory") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  ystl::String order = "Knife USP Glock18 Deagle P228 Elite FiveSeven M3 XM1014 MP5 TMP P90 ";
  order += "MAC10 UMP45 AK47 SG552 M4A1 Galil Famas AUG Scout AWP G3SG1 SG550 M249 Shield";

  // teams, grenades, economy, delays and preferences in one pass...
  ystl::String weapon_v1 = "Standard {\n  AK47 = Both\n  BogusGun = CT\n}\nAS {\n  M4A1 = BogusTeam\n}\n";
  weapon_v1 += "Grenades {\n  HE = 90\n  Flash = 80\n  Flashbang = 70\n  Smoke = 60\n  BogusNade = 1\n}\n";
  weapon_v1 += "Economy {\n  Primary = 5000\n  Shield = 999\n  BogusKey = 1\n}\n";
  weapon_v1 += "FireDelay {\n  PistolBase = 0.06\n  OtherBase = 0.11\n  DeagleExtra = 0.09\n";
  weapon_v1 += "  PistolMin = 0.02 0.04 0.06\n  PistolMax = 0.02 0.04 0.06 0.08 0.12 0.16\n";
  weapon_v1 += "  OtherMin = 0.0 0.1 0.2 0.3 0.4 0.6\n  OtherMax = 0.1 0.2 0.3 0.4 0.5 0.7\n}\n";
  weapon_v1 += ystl::strings.format ("Preferences {\n  Normal = %s\n}\nBogusTopLevel = 1\n", order);

  WriteConfFile (ConfFile ("weapon"), weapon_v1);
  conf.LoadWeaponsConfig ();

  CHECK (conf.FindWeaponById (Weapon::AK47).team_standard == WeaponTeam::Both);
  CHECK (conf.FindWeaponById (Weapon::M4A1).team_as == WeaponTeam::CT); // unknown team keeps the default
  CHECK (conf.GetEconLimit (EcoLimit::PrimaryGreater) == 5000);
  CHECK (conf.GetEconLimit (EcoLimit::ShieldGreater) == 999);

  const auto &delay = conf.GetFireDelay ();
  CHECK (delay.pistol_base > 0.05f && delay.pistol_base < 0.07f);
  CHECK (delay.deagle_extra > 0.08f && delay.deagle_extra < 0.10f);

  HOST_REQUIRE (conf.GetWeaponPrefs (Personality::Normal).size () == 26);
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));

  // ...block-form preferences apply per personality...
  ystl::String rows = "Knife USP Glock18 Deagle P228 Elite FiveSeven M3 XM1014 MP5 TMP P90 MAC10";
  ystl::String weapon_v2 = ystl::strings.format ("Preferences {\n  Normal = %s\n  Rusher {\n    row1 = %s\n", order, rows);
  weapon_v2 += "    row2 = UMP45 AK47 SG552 M4A1 Galil Famas AUG Scout AWP G3SG1 SG550 M249 Shield\n  }\n}\n";

  WriteConfFile (ConfFile ("weapon"), weapon_v2);
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Rusher)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));

  // ...all three personalities apply at once...
  ystl::String careful = "AWP Knife USP Glock18 Deagle P228 Elite FiveSeven M3 XM1014 MP5 TMP P90 ";
  careful += "MAC10 UMP45 AK47 SG552 M4A1 Galil Famas AUG Scout G3SG1 SG550 M249 Shield";
  ystl::String rusher = "M4A1 Knife USP Glock18 Deagle P228 Elite FiveSeven M3 XM1014 MP5 TMP P90 ";
  rusher += "MAC10 UMP45 AK47 SG552 Galil Famas AUG Scout AWP G3SG1 SG550 M249 Shield";
  ystl::String weapon_v3 = ystl::strings.format ("Preferences {\n  Normal = %s\n  Rusher = %s\n  Careful = %s\n}\n", order, rusher, careful);

  WriteConfFile (ConfFile ("weapon"), weapon_v3);
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));
  CHECK (conf.GetWeaponPrefs (Personality::Rusher)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::M4A1));
  CHECK (conf.GetWeaponPrefs (Personality::Careful)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::AWP));

  // ...block-form preferences reject unknown weapons the same way...
  ystl::String weapon_v2b = ystl::strings.format ("Preferences {\n  Normal = %s\n  Rusher {\n    row1 = Knife BogusGun2\n  }\n}\n", order);

  WriteConfFile (ConfFile ("weapon"), weapon_v2b);
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));
  CHECK (conf.GetWeaponPrefs (Personality::Rusher)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::M4A1));

  // ...unknown weapons, duplicates and gaps leave the table untouched...
  WriteConfFile (ConfFile ("weapon"), "Preferences {\n  Normal = AK47 BogusGun\n}\n");
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));

  WriteConfFile (ConfFile ("weapon"), "Preferences {\n  Normal = Knife Knife USP\n}\n");
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));

  ystl::String scant = "Knife USP Glock18 Deagle P228 Elite FiveSeven M3 XM1014 MP5 TMP P90 ";
  scant += "MAC10 UMP45 AK47 SG552 M4A1 Galil Famas AUG Scout AWP G3SG1 SG550";

  WriteConfFile (ConfFile ("weapon"), ystl::strings.format ("Preferences {\n  Normal = %s\n}\n", scant));
  conf.LoadWeaponsConfig ();
  CHECK (conf.GetWeaponPrefs (Personality::Normal)[kNumWeapons - 1] == conf.FindWeaponIndexById (Weapon::Knife));

  // ...garbage never parses...
  WriteConfFile (ConfFile ("weapon"), "{{{\n");
  conf.LoadWeaponsConfig ();

  ystl::plat.remove_file (ConfFile ("weapon").chars ());
  conf.LoadWeaponsConfig ();

  bots.Destroy ();
}

TEST_CASE ("unit/config_chatterbox") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  const int radio_before = cv_radio_mode.As<int> ();
  const ystl::String chatter_path_before = cv_chatter_path.As<ystl::StringRef> ();

  game.AddGameFlag (GameFlags::HasBotVoice);
  cv_radio_mode.Set (2);
  WriteWaveFile (WavePath ("wavok1"));
  WriteWaveFile (WavePath ("wavok2"));

  // block plus scalar sentences, one unknown event and one empty event...
  ystl::String chatter_v1 = "RewritePath = sound/radio/bot\nCoverMe {\n  wavok1\n  missingwave1\n  missingwave2\n";
  chatter_v1 += "  missingwave3\n  missingwave4\n  missingwave5\n  missingwave6\n}\n";
  chatter_v1 += "EnemySpotted = wavok2, missingwave7, missingwave8, missingwave9, missingwave10, missingwave11\n";
  chatter_v1 += "BogusEvent = whatever\nHoldPosition = ,\n";

  WriteConfFile (ConfFile ("chatter"), chatter_v1);
  conf.LoadChatterConfig ();

  // ...eleven missing waves trip the breaker, the two real ones still land...
  CHECK (ConfigHook::ChatterBankCount () == ystl::to_underlying (RadioChat::Count));
  CHECK (cv_radio_mode.As<int> () == 1);
  CHECK (conf.HasChatterBank (RadioChat::CoverMe));
  CHECK (conf.HasChatterBank (RadioChat::EnemySpotted));
  CHECK (conf.GetChatterMessageRepeatInterval (RadioChat::CoverMe) == kMaxChatterRepeatInterval);

  // ...nested rewrite paths apply the same way (second ready load replaces the banks)...
  cv_radio_mode.Set (2);
  ystl::String chatter_v2 = "Chatter {\n  RewritePath = sound/radio/bot\n  CoverMe {\n    wavok1\n  }\n}\n";

  WriteConfFile (ConfFile ("chatter"), chatter_v2);
  conf.LoadChatterConfig ();
  CHECK (ConfigHook::ChatterBankCount () == ystl::to_underlying (RadioChat::Count));
  CHECK (conf.HasChatterBank (RadioChat::CoverMe));
  CHECK (!conf.HasChatterBank (RadioChat::EnemySpotted));

  // ...identical content skips the second parse...
  conf.LoadChatterConfig ();
  CHECK (ConfigHook::ChatterBankCount () == ystl::to_underlying (RadioChat::Count));
  CHECK (conf.HasChatterBank (RadioChat::CoverMe));

  // ...garbage silences the radio...
  cv_radio_mode.Set (2);
  WriteConfFile (ConfFile ("chatter"), "{{{\n");
  conf.LoadChatterConfig ();
  CHECK (cv_radio_mode.As<int> () == 1);

  // ...a silenced radio skips the file entirely...
  WriteConfFile (ConfFile ("chatter"), "CoverMe = wavok1\n");
  conf.LoadChatterConfig ();
  CHECK (cv_radio_mode.As<int> () == 1);

  cv_radio_mode.Set (radio_before);
  cv_chatter_path.Set (chatter_path_before.chars ());
  game.ClearGameFlag (GameFlags::HasBotVoice);

  ystl::plat.remove_file (ConfFile ("chatter").chars ());
  ystl::plat.remove_file (WavePath ("wavok1").chars ());
  ystl::plat.remove_file (WavePath ("wavok2").chars ());

  bots.Destroy ();
}

TEST_CASE ("unit/config_dialogue") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  const int chat_before = cv_chat.As<int> ();

  // garbage disables chat...
  WriteConfFile (ConfLangFile ("en_chat"), "{{{\n");
  conf.LoadChatConfig ();
  CHECK (cv_chat.As<int> () == 0);
  cv_chat.Set (1);

  // ...the full matrix loads: every section, replies, game names and junk...
  ystl::String chat_v1 = "TopScalar = oops\nBogusSection {\n  x = 1\n}\n";
  chat_v1 += "raw Killed {\n  kill one\n  kill two\n}\nraw BombPlant {\n  plant one\n}\nraw DeadChat {\n  dead one\n}\n";
  chat_v1 += "raw Welcome {\n  hello one\n}\nraw TeamAttack {\n  attack one\n}\nraw TeamKill {\n  teamkill one\n}\n";
  chat_v1 += "raw Unknown {\n  unknown one\n}\n";
  chat_v1 += "Replies {\n  GoodGroup {\n    key = hello\n    raw {\n      hi there\n      hello back\n    }\n  }\n";
  chat_v1 += "  BadGroup = oops\n  MixedGroup {\n    key = BOT\n    bogusentry = x\n";
  chat_v1 += "    raw {\n      only reply\n    }\n  }\n  SpaceGroup {\n    key = \" BOT \"\n    raw {\n      spaced reply\n    }\n";
  chat_v1 += "  }\n  PadGroup {\n    key = \"   \"\n    raw {\n      padded reply\n    }\n  }\n}\n";
  chat_v1 += "GameNames {\n  CZ = cz_only\n  CS = cs_one, cs_two, cs_three\n  Bogus = x\n}\n";

  WriteConfFile (ConfLangFile ("en_chat"), chat_v1);
  conf.LoadChatConfig ();

  CHECK (conf.HasChatBank (Chat::Kill));
  CHECK (conf.HasChatBank (Chat::Plant));
  CHECK (conf.HasChatBank (Chat::Dead));
  CHECK (conf.HasChatBank (Chat::Hello));
  CHECK (conf.HasChatBank (Chat::TeamAttack));
  CHECK (conf.HasChatBank (Chat::TeamKill));
  CHECK (conf.HasChatBank (Chat::NoKeyword));
  CHECK (!conf.GetReplies ().empty ());
  CHECK (!conf.GetKeywordIndex ().empty ());

  // ...named game banks answer deterministically under a pinned roll...
  ystl::rg.force_chance (true);
  CHECK (conf.GetGameName () == "cs_one");
  ystl::rg.force_chance (false);
  ystl::rg.force_int (2);
  CHECK (conf.GetGameName () == "cs_three");
  ystl::rg.clear_forced ();

  // ...identical content keeps the current cycles...
  conf.LoadChatConfig ();
  CHECK (conf.HasChatBank (Chat::Kill));

  // ...a missing file disables chat...
  ystl::plat.remove_file (ConfLangFile ("en_chat").chars ());
  conf.LoadChatConfig ();
  CHECK (cv_chat.As<int> () == 0);
  cv_chat.Set (chat_before);

  bots.Destroy ();
}

TEST_CASE ("unit/config_tongue") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  const ystl::String language_before = cv_language.As<ystl::StringRef> ();
  cv_language.Set ("xx");

  game.ClearGameFlag (GameFlags::Legacy);

  // menus always translate: showing one clears the boot-time translate bypass...
  ctrl.ShowMenu (MenuId::Main);
  CHECK (!ctrl.IgnoreTranslate ());

  // a missing non-english language only logs...
  conf.LoadLanguageConfig ();
  CHECK (conf.MissingTranslations ().empty ());

  // ...entries load without a Lang wrapper, junk and hollow pairs drop out...
  ystl::String lang_v1 = "Greet {\n  raw Original {\n    Hello test\n    Second line\n  }\n";
  lang_v1 += "  raw Translated {\n    Hola test\n    Segunda linea\n  }\n}\nScalarOops = ignored\n";
  lang_v1 += "NoOrig {\n  raw Translated {\n    x\n  }\n}\n";

  WriteConfFile (ConfLangFile ("xx_lang"), lang_v1);

  ystl::MemFile probe {};
  CHECK (conf.OpenConfig ("lang", "probe-missing", &probe, true));
  probe.close ();

  conf.LoadLanguageConfig ();
  CHECK (ystl::StringRef (conf.Translate ("Hello test\nSecond line")) == "Hola test\nSegunda linea");

  // ...identical content keeps the translations...
  conf.LoadLanguageConfig ();
  CHECK (ystl::StringRef (conf.Translate ("Hello test\nSecond line")) == "Hola test\nSegunda linea");

  // ...a Lang wrapper loads the same way...
  ystl::String lang_v2 = "Lang {\n  Bye {\n    raw Original {\n      Bye now\n    }\n    raw Translated {\n      Adios\n";
  lang_v2 += "    }\n  }\n}\n";

  WriteConfFile (ConfLangFile ("xx_lang"), lang_v2);
  conf.LoadLanguageConfig ();

  // ...garbage never parses, the previous translations survive...
  WriteConfFile (ConfLangFile ("xx_lang"), "{{{\n");
  conf.LoadLanguageConfig ();
  CHECK (ystl::StringRef (conf.Translate ("Bye now")) == "Adios");
  WriteConfFile (ConfLangFile ("xx_lang"), lang_v2);

  // ...unseen strings are collected for the missing list...
  CHECK (ystl::StringRef (conf.Translate ("Unseen phrase xyz")) == "Unseen phrase xyz");
  CHECK (ystl::StringRef (conf.Translate ("!!!")) == "!!!");
  CHECK (conf.MissingTranslations ().size () == 2);

  // ...hashing skips non-letters, labels camelize or fall back to the hash...
  CHECK (ConfigHook::HashLang ("a b!") == ConfigHook::HashLang ("ab"));
  CHECK (ConfigHook::MakeLabel ("!!!", 0x12345678u) == "Entry12345678");
  CHECK (ConfigHook::MakeLabel ("alpha beta gamma delta epsilon zeta", 1) == "AlphaBetaGammaDeltaEpsilon");

  // ...the missing list appends to the existing file with a backup...
  CHECK (conf.WriteMissingTranslations () == 2);
  CHECK (conf.MissingTranslations ().empty ());
  CHECK (ystl::plat.file_exists (ystl::strings.format ("%s.bak", ConfLangFile ("xx_lang").chars ())));

  // ...a corrupt language file fails the write...
  WriteConfFile (ConfLangFile ("xx_lang"), "{{{\n");
  CHECK (ystl::StringRef (conf.Translate ("Garbage miss")) == "Garbage miss");
  CHECK (conf.WriteMissingTranslations () == -1);

  // ...a missing file is created from scratch with a version header...
  ystl::plat.remove_file (ConfLangFile ("xx_lang").chars ());
  CHECK (ystl::StringRef (conf.Translate ("Fresh miss one")) == "Fresh miss one");
  CHECK (conf.WriteMissingTranslations () == 2);
  CHECK (conf.MissingTranslations ().empty ());

  // ...an unwritable file fails the write...
  ystl::plat.remove_file (ConfLangFile ("xx_lang").chars ());
  ystl::plat.create_directory (ConfLangFile ("xx_lang").chars ());
  CHECK (ystl::StringRef (conf.Translate ("Dir miss")) == "Dir miss");
  CHECK (conf.WriteMissingTranslations () == -1);
  ystl::plat.remove_directory (ConfLangFile ("xx_lang").chars ());

  game.AddGameFlag (GameFlags::Legacy);

  conf.ShowCustomValues ();

  cv_language.Set (language_before.chars ());
  ystl::plat.remove_file (ConfLangFile ("xx_lang").chars ());
  ystl::plat.remove_file (ystl::strings.format ("%s.bak", ConfLangFile ("xx_lang").chars ()));

  bots.Destroy ();
}

TEST_CASE ("unit/config_staples") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  // the boot fills the fallback roster, so names are always at hand...
  HOST_REQUIRE (conf.PickBotName () != nullptr);

  // ...a lone name resolves, reloads refresh without reallocating...
  WriteConfFile (ConfFile ("names"), "raw Names {\n  Solo\n  ThisNameIsDefinitelyWayTooLongForTheBotLimitXYZ\n}\n");
  conf.LoadNamesConfig ();
  HOST_REQUIRE (conf.PickBotName () != nullptr);
  conf.LoadNamesConfig ();
  HOST_REQUIRE (conf.PickBotName () != nullptr);

  // ...garbage keeps the previous roster...
  WriteConfFile (ConfFile ("names"), "{{{\n");
  conf.LoadNamesConfig ();
  HOST_REQUIRE (conf.PickBotName () != nullptr);

  // ...named entries are skipped, long names are clipped...
  WriteConfFile (ConfFile ("names"), "Names {\n  SoloTwo\n  NamedOops = ignored\n  ThisNameIsDefinitelyWayTooLongForTheBotLimitXYZ\n}\n");
  conf.LoadNamesConfig ();
  HOST_REQUIRE (conf.PickBotName () != nullptr);

  // ...id lookups miss cleanly, noise lists skip empties...
  CHECK (conf.FindWeaponIndexById (Weapon::Invalid) == 0);
  CHECK (ConfigHook::LookupNoise ("WeaponFire, ,Explosion") == (Noise::WeaponFire | Noise::Explosion));

  // ...avatars need a non-legacy game...
  game.ClearGameFlag (GameFlags::Legacy);
  WriteConfFile (ConfFile ("avatars"), "Avatars {\n  avatar_one\n  avatar_two\n  NamedAva = avatar_three\n}\n");
  conf.LoadAvatarsConfig ();
  CHECK (!conf.GetRandomAvatar ().empty ());
  conf.LoadAvatarsConfig ();
  WriteConfFile (ConfFile ("avatars"), "{{{\n");
  conf.LoadAvatarsConfig ();

  // ...a missing file clears the banks...
  ystl::plat.remove_file (ConfFile ("avatars").chars ());
  conf.LoadAvatarsConfig ();
  CHECK (conf.GetRandomAvatar ().empty ());
  game.AddGameFlag (GameFlags::Legacy);

  // ...difficulty parses every level and rejects the junk...
  ystl::String diff_v1 = "StrayScalar = 1\nNoob {\n  Reaction = 1.0 1.2\n  HeadshotChance = 21\n";
  diff_v1 += "  SeenThruWallChance = 11\n  HeardThruWallChance = 12\n  MaxRecoil = 33\n  AimError = 11.0 11.0 19.0\n}\n";
  diff_v1 += "Easy {\n  Reaction = 0.7\n}\nNormal {\n  HeadshotChance = 55\n  AimError = 1.0 2.0\n}\n";
  diff_v1 += "Hard {\n  Reaction = 0.3 0.45\n  HeadshotChance = 65\n}\nExpert {\n  Reaction = 0.15 0.3\n";
  diff_v1 += "  AimError = 1.0 2.0 3.0\n}\nBogus {\n  HeadshotChance = 5\n}\n";

  WriteConfFile (ConfFile ("difficulty"), diff_v1);
  conf.LoadDifficultyConfig ();

  const auto *noob = conf.GetDifficultyTweaks (Difficulty::Noob);
  HOST_REQUIRE (noob != nullptr);
  CHECK (noob->reaction[1] > 1.19f && noob->reaction[1] < 1.21f);
  CHECK (noob->headshot_pct == 21);

  const auto *normal = conf.GetDifficultyTweaks (Difficulty::Normal);
  HOST_REQUIRE (normal != nullptr);
  CHECK (normal->headshot_pct == 55);

  conf.LoadDifficultyConfig ();
  WriteConfFile (ConfFile ("difficulty"), "{{{\n");
  conf.LoadDifficultyConfig ();

  // ...custom values accept bare and named scalars...
  WriteConfFile (ConfFile ("custom"), "TopScalar = topvalue\nBareTopItem\nTestSection {\n  TestKey = TestValue\n  BareInner\n}\n");
  conf.LoadCustomConfig ();
  CHECK (conf.FetchCustom ("TestKey") == "TestValue");
  CHECK (conf.FetchCustom ("TopScalar") == "topvalue");
  conf.LoadCustomConfig ();

  WriteConfFile (ConfFile ("custom"), "{{{\n");
  conf.LoadCustomConfig ();

  ystl::plat.remove_file (ConfFile ("custom").chars ());
  conf.LoadCustomConfig ();

  // ...logos resolve through the engine decals...
  WriteConfFile (ConfFile ("logos"), "Logos {\n  targetone\n  handone\n  NamedLogo = spittwo\n}\n");
  conf.LoadLogosConfig ();
  CHECK (conf.GetRandomLogoDecalIndex () > 0);
  WriteConfFile (ConfFile ("logos"), "{{{\n");
  conf.LoadLogosConfig ();

  ystl::plat.remove_file (ConfFile ("logos").chars ());
  conf.LoadLogosConfig ();

  ystl::plat.remove_file (ConfFile ("avatars").chars ());
  ystl::plat.remove_file (ConfFile ("difficulty").chars ());
  ystl::plat.remove_file (ConfFile ("names").chars ());

  bots.Destroy ();
}

TEST_CASE ("unit/config_gamedef") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  // material types load once per process, so the fixture must predate the activation load...
  ystl::String materials = "// comment\n; another\n# third\n\nC conc_wall\nc low_wall extra words here\nW weaponwall\n";
  materials += "1 badtype\n{\nC\n{bracewall\n";

  WriteConfFile (MaterialsFile (), materials);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  conf.InitWeapons ();

  // garbage keeps the builtins...
  WriteConfFile (ConfFile ("gamedef"), "{{{\n");
  conf.LoadGameDefConfig ();
  HOST_REQUIRE (conf.GetWeapons ().size () > 0);

  // ...weapon defs override every field, sounds need valid flags...
  ystl::String gamedef_v1 = "Weapons {\n  AK47 {\n    Price = 1234\n    TeamStandard = Both\n    TeamAS = T\n    Type = Rifle\n";
  gamedef_v1 += "  }\n  M4A1 {\n    Name = weapon_m4a1x\n    Model = m4a1x.mdl\n    Alias = m4a1x\n    FullName = X Rifle\n";
  gamedef_v1 += "    Price = 3100\n    MinPrimaryAmmo = 2\n    BuyGroup = 4\n    BuySelect = 9\n    BuySelectT = 9\n";
  gamedef_v1 += "    BuySelectCT = 9\n    PenetratePower = 1\n    MaxClip = 30\n    PrimaryFireHold = false\n";
  gamedef_v1 += "    TeamStandard = CT\n    TeamAS = CT\n    Type = ZoomRifle\n  }\n  BogusGun {\n    Price = 1\n  }\n";
  gamedef_v1 += "  NonBlockEntry = 5\n}\nHearableSounds {\n  zzcfgtest {\n    Flags = WeaponFire, Explosion\n";
  gamedef_v1 += "    Radius = 100.0\n    Duration = 0.5\n  }\n  badsound {\n    Flags = BogusFlag\n  }\n}\n";
  gamedef_v1 += "Sentences {\n  alpha\n  bravo\n}\n";

  WriteConfFile (ConfFile ("gamedef"), gamedef_v1);
  conf.LoadGameDefConfig ();

  CHECK (conf.FindWeaponById (Weapon::AK47).price == 1234);
  CHECK (conf.FindWeaponById (Weapon::AK47).team_standard == WeaponTeam::Both);
  CHECK (conf.GetWeaponType (Weapon::M4A1) == WeaponType::ZoomRifle);

  const SoundTemplate *tmpl = ConfigHook::ClassifySound ("zzcfgtest-1.wav");
  HOST_REQUIRE (tmpl != nullptr);
  CHECK (tmpl->base_radius == 100.0f);
  CHECK (tmpl->duration == 0.5f);

  // ...material types parse once and answer with engine prefixes...
  conf.LoadMaterialTypes ();
  CHECK (conf.HasMaterialTypes ());
  conf.LoadMaterialTypes ();

  CHECK (conf.GetMaterialType (nullptr) == 'C');
  CHECK (conf.GetMaterialType ("weaponwall") == 'W');
  CHECK (conf.GetMaterialType ("-aweaponwall") == 'W');
  CHECK (conf.GetMaterialType ("{weaponwall") == 'W');
  CHECK (conf.GetMaterialType ("!weaponwall") == 'W');
  CHECK (conf.GetMaterialType ("~weaponwall") == 'W');
  CHECK (conf.GetMaterialType (" weaponwall") == 'W');
  CHECK (conf.GetMaterialType ("conc_wall") == 'C');
  CHECK (conf.GetMaterialType ("low_wall") == 'C');
  CHECK (conf.GetMaterialType ("nope_missing_xyz") == 'C');

  ystl::plat.remove_file (ConfFile ("gamedef").chars ());
  conf.LoadGameDefConfig ();
  ystl::plat.remove_file (MaterialsFile ().chars ());

  bots.Destroy ();
}

TEST_CASE ("unit/config_versions") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootConfig (engine, cs);

  // an empty game has no default name bank...
  ystl::rg.force_chance (false);
  CHECK (conf.GetGameName () == "Counter-Strike");
  ystl::rg.force_chance (true);
  CHECK (conf.GetGameName () == "CS");
  ystl::rg.clear_forced ();

  // ...a flagless game has no name at all...
  game.ClearGameFlag (GameFlags::Legacy);
  CHECK (conf.GetGameName () == "");

  // ...prices stay put outside legacy...
  conf.AdjustWeaponPrices ();
  game.AddGameFlag (GameFlags::Legacy);

  // ...a language file never opens for english...
  ystl::MemFile english {};
  CHECK (!conf.OpenConfig ("lang", "gone", &english, true));

  // ...condition zero answers from its own bank...
  game.AddGameFlag (GameFlags::ConditionZero);
  ystl::rg.force_chance (false);
  CHECK (conf.GetGameName () == "Condition Zero");
  ystl::rg.force_chance (true);
  CHECK (conf.GetGameName () == "CZ");
  ystl::rg.clear_forced ();
  game.ClearGameFlag (GameFlags::ConditionZero);

  // ...reads track content until a load snapshots it...
  WriteConfFile (ConfFile ("testcfg"), "hello test\n");

  ystl::String text {};
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);

  ConfigHook::MarkApplied ("testcfg", text);
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Unchanged);

  // ...re-marking with new content updates the stored snapshot...
  ConfigHook::MarkApplied ("testcfg", "hello test changed\n");
  WriteConfFile (ConfFile ("testcfg"), "hello test changed\n");
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Unchanged);

  WriteConfFile (ConfFile ("testcfg"), "hello test other\n");
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Ready);

  // ...outdated versions never reach the parser...
  ConfigHook::ForgetSnapshot ("testcfg");
  WriteConfFile (ConfFile ("testcfg"), "; @version 0.1\nold = 1\n");
  CHECK (ConfigHook::ReadText ("testcfg", "gone", text, false) == ConfigRead::Missing);

  CHECK (ConfigHook::CheckVersion ("testcfg", "plain handmade config"));
  CHECK (!ConfigHook::CheckVersion ("testcfg", "; @version 0.1\nold = 1\n"));
  CHECK (ConfigHook::CheckVersion ("testcfg", "; @version 4.7\nnew = 1\n"));
  CHECK (!ConfigHook::CheckVersion ("testcfg", "; @version 4\nnew = 1\n"));
  CHECK (ConfigHook::CheckVersion ("testcfg", "; @version 99.99\nnew = 1\n"));

  // ...an already open file is closed before reopening...
  WriteConfFile (ConfFile ("testcfg"), "hello test\n");
  ystl::MemFile open_first {};
  HOST_REQUIRE (open_first.open (ConfFile ("testcfg")));
  CHECK (conf.OpenConfig ("testcfg", "gone", &open_first, false));
  open_first.close ();

  // ...a missing translation falls back to english...
  const ystl::String language_before = cv_language.As<ystl::StringRef> ();
  cv_language.Set ("xx");
  WriteConfFile (ConfLangFile ("en_chat"), "raw Killed {\n  hi one\n}\n");
  ystl::MemFile fallback {};
  CHECK (conf.OpenConfig ("chat", "gone", &fallback, true));
  fallback.close ();
  cv_language.Set (language_before.chars ());

  // ...map configs execute when the file exists...
  WriteConfFile (MapSpecificFile (), "yb_quota 9\n");
  conf.LoadMapSpecificConfig ();

  bool saw_exec = false;

  for (const auto &cmd : engine.ServerCommands ()) {
    if (cmd.find ("exec") != ystl::String::InvalidIndex) {
      saw_exec = true;
    }
  }
  CHECK (saw_exec);

  ystl::plat.remove_file (ConfFile ("testcfg").chars ());
  ystl::plat.remove_file (ConfLangFile ("en_chat").chars ());
  ystl::plat.remove_file (MapSpecificFile ().chars ());

  bots.Destroy ();
}

} // namespace bot
