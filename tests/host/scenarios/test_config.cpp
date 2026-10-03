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
    HOST_REQUIRE (parser.parse ("AK47 { price = 1234 }\nBogus { price = 1 }\n"));

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
    HOST_REQUIRE (parser.parse ("Noisy { flags = BogusFlag }\n"));
    ConfigHook::ApplySounds (&parser.document ());
    CHECK (ConfigHook::ClassifySound ("zzcfgtest-1.wav") == nullptr);
  }

  {
    ystl::ConfParser parser {};
    HOST_REQUIRE (parser.parse ("zzcfgtest { flags = WeaponFire\n radius = 100.0\n duration = 0.5 }\n"));
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

  auto bot = ystl::make_unique<Bot> (ent, Difficulty::Normal, Personality::Normal, Team::CT, 0);
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

} // namespace bot
