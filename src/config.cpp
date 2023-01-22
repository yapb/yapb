//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2023 YaPB Project <yapb@jeefo.net>.
//
// SPDX-License-Identifier: MIT
//

#include <yapb.h>

ConVar cv_bind_menu_key ("yb_bind_menu_key", "=", "Binds specified key for opening bots menu.", false);
ConVar cv_ignore_cvars_on_changelevel ("yb_ignore_cvars_on_changelevel", "yb_quota,yb_autovacate", "Specifies comma separated list of bot cvars, that will not be overriten by config on changelevel.", false);

BotConfig::BotConfig () {
   m_chat.resize (Chat::Count);
   m_chatter.resize (Chatter::Count);

   m_weaponProps.resize (kMaxWeapons);
}

void BotConfig::loadConfigs () {
   setupMemoryFiles ();

   loadNamesConfig ();
   loadChatConfig ();
   loadChatterConfig ();
   loadWeaponsConfig ();
   loadLanguageConfig ();
   loadLogosConfig ();
   loadAvatarsConfig ();
   loadDifficultyConfig ();
   loadCustomConfig ();
}

void BotConfig::loadMainConfig (bool isFirstLoad) {
   if (game.is (GameFlags::Legacy) && !game.is (GameFlags::Xash3D)) {
      util.setNeedForWelcome (true);
   }
   setupMemoryFiles ();

   auto needsToIgnoreVar = [](StringArray &list, const char *needle) {
      for (const auto &var : list) {
         if (var == needle) {
            return true;
         }
      }
      return false;
   };
   String line;
   MemFile file;

   // this is does the same as exec of engine, but not overwriting values of cvars spcified in cv_ignore_cvars_on_changelevel
   if (util.openConfig (strings.format ("%s.cfg", product.folder), "Bot main config file is not found.", &file, false)) {
      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }

         if (isFirstLoad) {
            game.serverCommand (line.chars ());
            continue;
         }
         auto keyval = line.split (" ");

         if (keyval.length () > 1) {
            auto ignore = String (cv_ignore_cvars_on_changelevel.str ()).split (",");

            auto key = keyval[0].trim ().chars ();
            auto cvar = engfuncs.pfnCVarGetPointer (key);

            if (cvar != nullptr) {
               auto value = const_cast <char *> (keyval[1].trim ().trim ("\"").trim ().chars ());

               if (needsToIgnoreVar (ignore, key) && !strings.matches (value, cvar->string)) {

                  // preserve quota number if it's zero
                  if (strings.matches (cvar->name, "yb_quota") && cv_quota.int_ () <= 0) {
                     engfuncs.pfnCvar_DirectSet (cvar, value);
                     continue;
                  }
                  ctrl.msg ("Bot CVAR '%s' differs from the stored in the config (%s/%s). Ignoring.", cvar->name, cvar->string, value);

                  // ensure cvar will have old value
                  engfuncs.pfnCvar_DirectSet (cvar, cvar->string);
               }
               else {
                  engfuncs.pfnCvar_DirectSet (cvar, value);
               }
            }
            else {
               game.serverCommand (line.chars ());
            }
         }
      }
      file.close ();
   }

   // android is abit hard to play, lower the difficulty by default
   if (plat.android && cv_difficulty.int_ () > 3) {
      cv_difficulty.set (3);
   }

   // bind the correct menu key for bot menu...
   if (!game.isDedicated () && !strings.isEmpty (cv_bind_menu_key.str ())) {
      game.serverCommand ("bind \"%s\" \"yb menu\"", cv_bind_menu_key.str ());
   }
}

void BotConfig::loadNamesConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // naming initialization
   if (util.openConfig ("names.cfg", "Name configuration file not found.", &file, true)) {
      m_botNames.clear ();

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         // max botname is 32 characters
         if (line.length () > 32) {
            line[32] = kNullChar;
         }
         m_botNames.emplace (line, -1);
      }
      file.close ();
   }
}

void BotConfig::loadWeaponsConfig () {
   setupMemoryFiles ();

   auto addWeaponEntries = [](SmallArray <WeaponInfo> &weapons, bool as, StringRef name, const StringArray &data) {

      // we're have null terminator element in weapons array...
      if (data.length () + 1 != weapons.length ()) {
         logger.error ("%s entry in weapons config is not valid or malformed (%d/%d).", name, data.length (), weapons.length ());

         return;
      }

      for (size_t i = 0; i < data.length (); ++i) {
         if (as) {
            weapons[i].teamAS = data[i].int_ ();
         }
         else {
            weapons[i].teamStandard = data[i].int_ ();
         }
      }
   };

   auto addIntEntries = [](SmallArray <int32> &to, StringRef name, const StringArray &data) {
      if (data.length () != to.length ()) {
         logger.error ("%s entry in weapons config is not valid or malformed (%d/%d).", name, data.length (), to.length ());
         return;
      }

      for (size_t i = 0; i < to.length (); ++i) {
         to[i] = data[i].int_ ();
      }
   };
   String line;
   MemFile file;

   // weapon data initialization
   if (util.openConfig ("weapon.cfg", "Weapon configuration file not found. Loading defaults.", &file)) {
      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         auto pair = line.split ("=");

         if (pair.length () != 2) {
            continue;
         }

         for (auto &trim : pair) {
            trim.trim ();
         }
         auto splitted = pair[1].split (",");

         if (pair[0].startsWith ("MapStandard")) {
            addWeaponEntries (m_weapons, false, pair[0], splitted);
         }
         else if (pair[0].startsWith ("MapAS")) {
            addWeaponEntries (m_weapons, true, pair[0], splitted);
         }

         else if (pair[0].startsWith ("GrenadePercent")) {
            addIntEntries (m_grenadeBuyPrecent, pair[0], splitted);
         }
         else if (pair[0].startsWith ("Economics")) {
            addIntEntries (m_botBuyEconomyTable, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityNormal")) {
            addIntEntries (m_normalWeaponPrefs, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityRusher")) {
            addIntEntries (m_rusherWeaponPrefs, pair[0], splitted);
         }
         else if (pair[0].startsWith ("PersonalityCareful")) {
            addIntEntries (m_carefulWeaponPrefs, pair[0], splitted);
         }
      }
      file.close ();
   }
}

void BotConfig::loadChatterConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // chatter initialization
   if (game.is (GameFlags::HasBotVoice) && cv_radio_mode.int_ () == 2 && util.openConfig ("chatter.cfg", "Couldn't open chatter system configuration", &file)) {
      m_chatter.clear ();

      struct EventMap {
         String str;
         int code;
         float repeat;
      } chatterEventMap[] = {
         { "Radio_CoverMe", Radio::CoverMe, kMaxChatterRepeatInteval },
         { "Radio_YouTakePoint", Radio::YouTakeThePoint, kMaxChatterRepeatInteval },
         { "Radio_HoldPosition", Radio::HoldThisPosition, 10.0f },
         { "Radio_RegroupTeam", Radio::RegroupTeam, 10.0f },
         { "Radio_FollowMe", Radio::FollowMe, 15.0f },
         { "Radio_TakingFire", Radio::TakingFireNeedAssistance, 5.0f },
         { "Radio_GoGoGo", Radio::GoGoGo, kMaxChatterRepeatInteval },
         { "Radio_Fallback", Radio::TeamFallback, kMaxChatterRepeatInteval },
         { "Radio_StickTogether", Radio::StickTogetherTeam, kMaxChatterRepeatInteval },
         { "Radio_GetInPosition", Radio::GetInPositionAndWaitForGo, kMaxChatterRepeatInteval },
         { "Radio_StormTheFront", Radio::StormTheFront, kMaxChatterRepeatInteval },
         { "Radio_ReportTeam", Radio::ReportInTeam, kMaxChatterRepeatInteval },
         { "Radio_Affirmative", Radio::RogerThat, kMaxChatterRepeatInteval },
         { "Radio_EnemySpotted", Radio::EnemySpotted, 4.0f },
         { "Radio_NeedBackup", Radio::NeedBackup, 5.0f },
         { "Radio_SectorClear", Radio::SectorClear, 10.0f },
         { "Radio_InPosition", Radio::ImInPosition, 10.0f },
         { "Radio_ReportingIn", Radio::ReportingIn, 3.0f },
         { "Radio_ShesGonnaBlow", Radio::ShesGonnaBlow, kMaxChatterRepeatInteval },
         { "Radio_Negative", Radio::Negative, kMaxChatterRepeatInteval },
         { "Radio_EnemyDown", Radio::EnemyDown, 10.0f },
         { "Chatter_DiePain", Chatter::DiePain, kMaxChatterRepeatInteval },
         { "Chatter_GoingToPlantBomb", Chatter::GoingToPlantBomb, 5.0f },
         { "Chatter_GoingToGuardVIPSafety", Chatter::GoingToGuardVIPSafety, kMaxChatterRepeatInteval },
         { "Chatter_RescuingHostages", Chatter::RescuingHostages, kMaxChatterRepeatInteval },
         { "Chatter_TeamKill", Chatter::FriendlyFire, kMaxChatterRepeatInteval },
         { "Chatter_GuardingVipSafety", Chatter::GuardingVIPSafety, kMaxChatterRepeatInteval },
         { "Chatter_PlantingC4", Chatter::PlantingBomb, 10.0f },
         { "Chatter_InCombat", Chatter::InCombat,  kMaxChatterRepeatInteval },
         { "Chatter_SeeksEnemy", Chatter::SeekingEnemies, kMaxChatterRepeatInteval },
         { "Chatter_Nothing", Chatter::Nothing,  kMaxChatterRepeatInteval },
         { "Chatter_EnemyDown", Chatter::EnemyDown, 10.0f },
         { "Chatter_UseHostage", Chatter::UsingHostages, kMaxChatterRepeatInteval },
         { "Chatter_WonTheRound", Chatter::WonTheRound, kMaxChatterRepeatInteval },
         { "Chatter_QuicklyWonTheRound", Chatter::QuickWonRound, kMaxChatterRepeatInteval },
         { "Chatter_NoEnemiesLeft", Chatter::NoEnemiesLeft, kMaxChatterRepeatInteval },
         { "Chatter_FoundBombPlace", Chatter::FoundC4Plant, 15.0f },
         { "Chatter_WhereIsTheBomb", Chatter::WhereIsTheC4, kMaxChatterRepeatInteval },
         { "Chatter_DefendingBombSite", Chatter::DefendingBombsite, kMaxChatterRepeatInteval },
         { "Chatter_BarelyDefused", Chatter::BarelyDefused, kMaxChatterRepeatInteval },
         { "Chatter_NiceshotCommander", Chatter::NiceShotCommander, 10.0f },
         { "Chatter_ReportingIn", Chatter::ReportingIn, 10.0f },
         { "Chatter_SpotTheBomber", Chatter::SpotTheBomber, 4.3f },
         { "Chatter_VIPSpotted", Chatter::VIPSpotted, 5.3f },
         { "Chatter_FriendlyFire", Chatter::FriendlyFire, 2.1f },
         { "Chatter_GotBlinded", Chatter::Blind, 12.0f },
         { "Chatter_GuardDroppedC4", Chatter::GuardingDroppedC4, 3.0f },
         { "Chatter_DefusingC4", Chatter::DefusingBomb, 3.0f },
         { "Chatter_FoundC4", Chatter::FoundC4, 5.5f },
         { "Chatter_ScaredEmotion", Chatter::ScaredEmotion, 6.1f },
         { "Chatter_HeardEnemy", Chatter::ScaredEmotion, 12.8f },
         { "Chatter_SniperWarning", Chatter::SniperWarning, 14.3f },
         { "Chatter_SniperKilled", Chatter::SniperKilled, 12.1f },
         { "Chatter_OneEnemyLeft", Chatter::OneEnemyLeft, 12.5f },
         { "Chatter_TwoEnemiesLeft", Chatter::TwoEnemiesLeft, 12.5f },
         { "Chatter_ThreeEnemiesLeft", Chatter::ThreeEnemiesLeft, 12.5f },
         { "Chatter_NiceshotPall", Chatter::NiceShotPall, 2.0f },
         { "Chatter_GoingToGuardHostages", Chatter::GoingToGuardHostages, 3.0f },
         { "Chatter_GoingToGuardDoppedBomb", Chatter::GoingToGuardDroppedC4, 6.0f },
         { "Chatter_OnMyWay", Chatter::OnMyWay, 1.5f },
         { "Chatter_LeadOnSir", Chatter::LeadOnSir, 5.0f },
         { "Chatter_Pinned_Down", Chatter::PinnedDown, 5.0f },
         { "Chatter_GottaFindTheBomb", Chatter::GottaFindC4, 3.0f },
         { "Chatter_You_Heard_The_Man", Chatter::YouHeardTheMan, 3.0f },
         { "Chatter_Lost_The_Commander", Chatter::LostCommander, 4.5f },
         { "Chatter_NewRound", Chatter::NewRound, 3.5f },
         { "Chatter_CoverMe", Chatter::CoverMe, 3.5f },
         { "Chatter_BehindSmoke", Chatter::BehindSmoke, 3.5f },
         { "Chatter_BombSiteSecured", Chatter::BombsiteSecured, 3.5f },
         { "Chatter_GoingToCamp", Chatter::GoingToCamp, 30.0f },
         { "Chatter_Camp", Chatter::Camping, 10.0f },
      };

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }

         StringRef rewriteKey = "RewritePath";
         StringRef eventKey = "Event";

         if (line.startsWith (rewriteKey)) {
            cv_chatter_path.set (line.substr (rewriteKey.length ()).trim ().chars ());
         }
         else if (line.startsWith (eventKey)) {
            auto items = line.substr (eventKey.length ()).split ("=");

            if (items.length () != 2) {
               logger.error ("Error in chatter config file syntax... Please correct all errors.");
               continue;
            }

            for (auto &item : items) {
               item.trim ();
            }
            items[1].trim ("(;)");

            for (const auto &event : chatterEventMap) {
               if (event.str == items[0]) {
                  // this does common work of parsing comma-separated chatter line
                  auto sounds = items[1].split (",");

                  for (auto &sound : sounds) {
                     sound.trim ().trim ("\"");
                     auto duration = game.getWaveLen (sound.chars ());

                     if (duration > 0.0f) {
                        m_chatter[event.code].emplace (cr::move (sound), event.repeat, duration);
                     }
                  }
                  sounds.clear ();
               }
            }
         }
      }
      file.close ();
   }
   else {
      cv_radio_mode.set (1);
      logger.message ("Bots chatter communication disabled.");
   }
}

void BotConfig::loadChatConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // chat config initialization
   if (util.openConfig ("chat.cfg", "Chat file not found.", &file, true)) {
      StringArray *chat = nullptr;

      StringArray keywords {};
      StringArray replies {};

      // clear all the stuff before loading new one
      for (auto &item : m_chat) {
         item.clear ();
      }
      m_replies.clear ();

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }

         if (line.startsWith ("[KILLED]")) {
            chat = &m_chat[Chat::Kill];
            continue;
         }
         else if (line.startsWith ("[BOMBPLANT]")) {
            chat = &m_chat[Chat::Plant];
            continue;
         }
         else if (line.startsWith ("[DEADCHAT]")) {
            chat = &m_chat[Chat::Dead];
            continue;
         }
         else if (line.startsWith ("[REPLIES]")) {
            chat = nullptr;
            continue;
         }
         else if (line.startsWith ("[UNKNOWN]")) {
            chat = &m_chat[Chat::NoKeyword];
            continue;
         }
         else if (line.startsWith ("[TEAMATTACK]")) {
            chat = &m_chat[Chat::TeamAttack];
            continue;
         }
         else if (line.startsWith ("[WELCOME]")) {
            chat = &m_chat[Chat::Hello];
            continue;
         }
         else if (line.startsWith ("[TEAMKILL]")) {
            chat = &m_chat[Chat::TeamKill];
            continue;
         }

         if (chat != nullptr) {
            chat->push (line);

         }
         else {
            if (line.startsWith ("@KEY")) {
               if (!keywords.empty () && !replies.empty ()) {
                  m_replies.emplace (keywords, replies);

                  keywords.clear ();
                  replies.clear ();
               }
               keywords.clear ();

               for (const auto &key : line.substr (4).split (",")) {
                  keywords.emplace (utf8tools.strToUpper (key));
               }

               for (auto &keyword : keywords) {
                  keyword.trim ().trim ("\"");
               }
            }
            else if (!keywords.empty () && !line.empty ()) {
               replies.push (line);
            }
         }
      }

      // shuffle chat a bit
      for (auto &item : m_chat) {
         item.shuffle ();
         item.shuffle ();
      }
      file.close ();
   }
   else {
      cv_chat.set (0);
   }
}

void BotConfig::loadLanguageConfig () {
   setupMemoryFiles ();

   if (game.is (GameFlags::Legacy)) {
      logger.message ("Bots multilingual system disabled.");
      return; // dedicated server will use only english translation
   }
   String line;
   MemFile file;

   // localizer inititalization
   if (util.openConfig ("lang.cfg", "Specified language not found.", &file, true)) {
      String temp;
      Twin <String, String> lang;

      // clear all the translations before new load
      m_language.clear ();

      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }

         if (line.startsWith ("[ORIGINAL]")) {
            if (!temp.empty ()) {
               lang.second = cr::move (temp);
            }

            if (!lang.second.empty () && !lang.first.empty ()) {
               m_language[hashLangString (lang.first.trim ().chars ())] = lang.second.trim ();
            }
         }
         else if (line.startsWith ("[TRANSLATED]") && !temp.empty ()) {
            lang.first = cr::move (temp);
         }
         else {
            temp += line;
         }
      }
      file.close ();
   }
   else if (strcmp (cv_language.str (), "en") != 0) {
      logger.error ("Couldn't load language configuration");
   }
}

void BotConfig::loadAvatarsConfig () {
   setupMemoryFiles ();

   if (game.is (GameFlags::Legacy)) {
      return;
   }

   String line;
   MemFile file;

   // avatars inititalization
   if (util.openConfig ("avatars.cfg", "Avatars config file not found. Avatars will not be displayed.", &file)) {
      m_avatars.clear ();

      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }
         m_avatars.push (cr::move (line.trim ()));
      }
   }
}

void BotConfig::loadDifficultyConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // initialize defaults
   m_difficulty[Difficulty::Noob] = {
      { 0.8f, 1.0f }, 5, 0, 0
   };

   m_difficulty[Difficulty::Easy] = {
      { 0.6f, 0.8f }, 30, 10, 10
   };

   m_difficulty[Difficulty::Normal] = {
      { 0.4f, 0.6f }, 50, 30, 40
   };

   m_difficulty[Difficulty::Hard] = {
      { 0.2f, 0.4f }, 75, 60, 70
   };

   m_difficulty[Difficulty::Expert] = {
      {  0.1f, 0.2f }, 100, 90, 90
   };

   // currently, mindelay, maxdelay, headprob, seenthruprob, heardthruprob
   constexpr uint32 kMaxDifficultyValues = 5;

   // helper for parsing each level
   auto parseLevel = [&] (int32 level, StringRef data) {
      auto values = data.split <String> (",");

      if (values.length () != kMaxDifficultyValues) {
         logger.error ("Bad value for difficulty level #%d.", level);
         return;
      }
      auto diff = &m_difficulty[level];

      diff->reaction[0] = values[0].float_ ();
      diff->reaction[1] = values[1].float_ ();
      diff->headshotPct = values[2].int_ ();
      diff->seenThruPct = values[3].int_ ();
      diff->hearThruPct = values[4].int_ ();
   };

   // avatars inititalization
   if (util.openConfig ("difficulty.cfg", "Difficulty config file not found. Loading defaults.", &file)) {

      while (file.getLine (line)) {
         if (isCommentLine (line) || line.length () < 3) {
            continue;
         }
         auto items = line.split ("=");

         if (items.length () != 2) {
            logger.error ("Error in difficulty config file syntax... Please correct all errors.");
            continue;
         }
         const auto &key = items[0].trim ();

         // get our keys
         if (key == "Noob") {
            parseLevel (Difficulty::Noob, items[1]);
         }
         else if (key == "Easy") {
            parseLevel (Difficulty::Easy, items[1]);
         }
         else if (key == "Normal") {
            parseLevel (Difficulty::Normal, items[1]);
         }
         else if (key == "Hard") {
            parseLevel (Difficulty::Hard, items[1]);
         }
         else if (key == "Expert") {
            parseLevel (Difficulty::Expert, items[1]);
         }
      }
   }
}

void BotConfig::loadMapSpecificConfig () {
   auto mapSpecificConfig = strings.format ("addons/%s/conf/maps/%s.cfg", product.folder, game.getMapName ());

   // check existence of file
   if (File::exists (strings.format ("%s/%s", game.getRunningModName (), mapSpecificConfig))) {
      game.serverCommand ("exec %s", mapSpecificConfig);

      ctrl.msg ("Executed map-specific config: %s", mapSpecificConfig);
   }
}

void BotConfig::loadCustomConfig () {
   String line;
   MemFile file;

   m_custom["C4ModelName"] = "c4.mdl";
   m_custom["AMXParachuteCvar"] = "sv_parachute";

   // custom inititalization
   if (util.openConfig ("custom.cfg", "Custom config file not found. Loading defaults.", &file)) {
      m_custom.clear ();

      while (file.getLine (line)) {
         line.trim ();

         if (isCommentLine (line)) {
            continue;
         }
         auto values = line.split ("=");

         if (values.length () != 2) {
            logger.error ("Bad configuration for custom.cfg");
            return;
         }
         auto kv = Twin <String, String> (values[0].trim (), values[1].trim ());

         if (!kv.first.empty () && !kv.second.empty ()) {
            m_custom[kv.first] = kv.second;
         }
      }
   }
}

void BotConfig::loadLogosConfig () {
   setupMemoryFiles ();

   String line;
   MemFile file;

   // logos inititalization
   if (util.openConfig ("logos.cfg", "Logos config file not found. Loading defaults.", &file)) {
      m_logos.clear ();

      while (file.getLine (line)) {
         if (isCommentLine (line)) {
            continue;
         }
         m_logos.push (cr::move (line.trim ()));
      }
   }
   else {
      m_logos = cr::move (String { "{biohaz;{graf003;{graf004;{graf005;{lambda06;{target;{hand1;{spit2;{bloodhand6;{foot_l;{foot_r" }.split (";"));
   }
}

void BotConfig::setupMemoryFiles () {
   static bool setMemoryPointers = true;

   auto wrapLoadFile = [] (const char *filename, int *length) {
      return engfuncs.pfnLoadFileForMe (filename, length);
   };

   auto wrapFreeFile = [] (void *buffer) {
      engfuncs.pfnFreeFile (buffer);
   };

   if (setMemoryPointers) {
      MemFileStorage::instance ().initizalize (wrapLoadFile, wrapFreeFile);
      setMemoryPointers = false;
   }
}

BotName *BotConfig::pickBotName () {
   if (m_botNames.empty ()) {
      return nullptr;
   }

   for (size_t i = 0; i < m_botNames.length () * 2; ++i) {
      auto botName = &m_botNames.random ();

      if (botName->name.length () < 3 || botName->usedBy != -1) {
         continue;
      }
      return botName;
   }
   return nullptr;
}

void BotConfig::clearUsedName (Bot *bot) {
   for (auto &name : m_botNames) {
      if (name.usedBy == bot->index ()) {
         name.usedBy = -1;
         break;
      }
   }
}

void BotConfig::initWeapons () {
   m_weapons.clear ();

   // fill array with available weapons
   m_weapons.emplace (Weapon::Knife, "weapon_knife", "knife.mdl", 0, 0, -1, -1, 0, 0, 0, 0, 0, 0, WeaponType::Melee, true);
   m_weapons.emplace (Weapon::USP, "weapon_usp", "usp.mdl", 500, 1, -1, -1, 1, 1, 2, 2, 0, 12, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::Glock18, "weapon_glock18", "glock18.mdl", 400, 1, -1, -1, 1, 2, 1, 1, 0, 20, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::Deagle, "weapon_deagle", "deagle.mdl", 650, 1, 2, 2, 1, 3, 4, 4, 2, 7, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::P228, "weapon_p228", "p228.mdl", 600, 1, 2, 2, 1, 4, 3, 3, 0, 13, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::Elite, "weapon_elite", "elite.mdl", 800, 1, 0, 0, 1, 5, 5, 5, 0, 30, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::FiveSeven, "weapon_fiveseven", "fiveseven.mdl", 750, 1, 1, 1, 1, 6, 5, 5, 0, 20, WeaponType::Pistol, false);
   m_weapons.emplace (Weapon::M3, "weapon_m3", "m3.mdl", 1700, 1, 2, -1, 2, 1, 1, 1, 0, 8, WeaponType::Shotgun, false);
   m_weapons.emplace (Weapon::XM1014, "weapon_xm1014", "xm1014.mdl", 3000, 1, 2, -1, 2, 2, 2, 2, 0, 7, WeaponType::Shotgun, false);
   m_weapons.emplace (Weapon::MP5, "weapon_mp5navy", "mp5.mdl", 1500, 1, 2, 1, 3, 1, 2, 2, 0, 30, WeaponType::SMG, true);
   m_weapons.emplace (Weapon::TMP, "weapon_tmp", "tmp.mdl", 1250, 1, 1, 1, 3, 2, 1, 1, 0, 30, WeaponType::SMG, true);
   m_weapons.emplace (Weapon::P90, "weapon_p90", "p90.mdl", 2350, 1, 2, 1, 3, 3, 4, 4, 0, 50, WeaponType::SMG, true);
   m_weapons.emplace (Weapon::MAC10, "weapon_mac10", "mac10.mdl", 1400, 1, 0, 0, 3, 4, 1, 1, 0, 30, WeaponType::SMG, true);
   m_weapons.emplace (Weapon::UMP45, "weapon_ump45", "ump45.mdl", 1700, 1, 2, 2, 3, 5, 3, 3, 0, 25, WeaponType::SMG, true);
   m_weapons.emplace (Weapon::AK47, "weapon_ak47", "ak47.mdl", 2500, 1, 0, 0, 4, 1, 2, 2, 2, 30, WeaponType::Rifle, true);
   m_weapons.emplace (Weapon::SG552, "weapon_sg552", "sg552.mdl", 3500, 1, 0, -1, 4, 2, 4, 4, 2, 30, WeaponType::ZoomRifle, true);
   m_weapons.emplace (Weapon::M4A1, "weapon_m4a1", "m4a1.mdl", 3100, 1, 1, 1, 4, 3, 3, 3, 2, 30, WeaponType::Rifle, true);
   m_weapons.emplace (Weapon::Galil, "weapon_galil", "galil.mdl", 2000, 1, 0, 0, 4, -1, 1, 1, 2, 35, WeaponType::Rifle, true);
   m_weapons.emplace (Weapon::Famas, "weapon_famas", "famas.mdl", 2250, 1, 1, 1, 4, -1, 1, 1, 2, 25, WeaponType::Rifle, true);
   m_weapons.emplace (Weapon::AUG, "weapon_aug", "aug.mdl", 3500, 1, 1, 1, 4, 4, 4, 4, 2, 30, WeaponType::ZoomRifle, true);
   m_weapons.emplace (Weapon::Scout, "weapon_scout", "scout.mdl", 2750, 1, 2, 0, 4, 5, 3, 2, 3, 10, WeaponType::Sniper, false);
   m_weapons.emplace (Weapon::AWP, "weapon_awp", "awp.mdl", 4750, 1, 2, 0, 4, 6, 5, 6, 3, 10, WeaponType::Sniper, false);
   m_weapons.emplace (Weapon::G3SG1, "weapon_g3sg1", "g3sg1.mdl", 5000, 1, 0, 2, 4, 7, 6, 6, 3, 20, WeaponType::Sniper, false);
   m_weapons.emplace (Weapon::SG550, "weapon_sg550", "sg550.mdl", 4200, 1, 1, 1, 4, 8, 5, 5, 3, 30, WeaponType::Sniper, false);
   m_weapons.emplace (Weapon::M249, "weapon_m249", "m249.mdl", 5750, 1, 2, 1, 5, 1, 1, 1, 2, 100, WeaponType::Heavy, true);
   m_weapons.emplace (Weapon::Shield, "weapon_shield", "shield.mdl", 2200, 0, 1, 1, 8, -1, 8, 8, 0, 0, WeaponType::Pistol, false);

   // not needed actually, but cause too much refactoring for now. todo
   m_weapons.emplace (0, "", "", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, WeaponType::None, false);
}

void BotConfig::adjustWeaponPrices () {
   // elite price is 1000$ on older versions of cs...
   if (!(game.is (GameFlags::Legacy))) {
      return;
   }

   for (auto &weapon : m_weapons) {
      if (weapon.id == Weapon::Elite) {
         weapon.price = 1000;
         break;
      }
   }
}

WeaponInfo &BotConfig::findWeaponById (const int id) {
   for (auto &weapon : m_weapons) {
      if (weapon.id == id) {
         return weapon;
      }
   }
   return m_weapons.at (0);
}

const char *BotConfig::translate (StringRef input) {
   // this function translate input string into needed language

   if (ctrl.ignoreTranslate ()) {
      return input.chars ();
   }
   auto hash = hashLangString (input.chars ());

   if (m_language.has (hash)) {
      return m_language[hash].chars ();
   }
   return input.chars (); // nothing found
}

void BotConfig::showCustomValues () {
   game.print ("Current values for custom config items:");

   m_custom.foreach ([&](const String &key, const String &val) {
      game.print ("  %s = %s", key, val);
   });
}

uint32 BotConfig::hashLangString (StringRef str) {
   auto test = [] (const char ch) {
      return  (ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z');
   };
   String res;

   for (const auto &ch : str) {
      if (!test (ch)) {
         continue;
      }
      res += ch;
   }
   return res.empty () ? 0 : res.hash ();
}
