//
// YaPB - Counter-Strike Bot based on PODBot by Markus Klinge.
// Copyright © 2004-2020 YaPB Development Team <team@yapb.ru>.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//

#pragma once

// botname structure definition
struct BotName {
   String name = "";
   int usedBy = -1;

public:
   BotName () = default;
   BotName (StringRef name, int usedBy) : name (name), usedBy (usedBy) { }
};

// voice config structure definition
struct ChatterItem {
   String name;
   float repeat;
   float duration;

public:
   ChatterItem (StringRef name, float repeat, float duration) : name (name), repeat (repeat), duration (duration) { }
};

// mostly config stuff, and some stuff dealing with menus
class BotConfig final : public Singleton <BotConfig> {
public:
   struct DifficultyData {
      float reaction[2] {};
      int32 headshotPct {};
      int32 seenThruPct {};
      int32 hearThruPct {};
   };

private:
   Array <StringArray> m_chat;
   Array <Array <ChatterItem>> m_chatter;

   Array <BotName> m_botNames;
   Array <Keywords> m_replies;
   SmallArray <WeaponInfo> m_weapons;
   SmallArray <WeaponProp> m_weaponProps;

   StringArray m_logos;
   StringArray m_avatars;

   HashMap <uint32, String, Hash <int32>> m_language { 256 };
   HashMap <int32, DifficultyData> m_difficulty;

   // default tables for personality weapon preferences, overridden by weapon.cfg
   SmallArray <int32> m_normalWeaponPrefs = { 0, 2, 1, 4, 5, 6, 3, 12, 10, 24, 25, 13, 11, 8, 7, 22, 23, 18, 21, 17, 19, 15, 17, 9, 14, 16 };
   SmallArray <int32> m_rusherWeaponPrefs = { 0, 2, 1, 4, 5, 6, 3, 24, 19, 22, 23, 20, 21, 10, 12, 13, 7, 8, 11, 9, 18, 17, 19, 25, 15, 16 };
   SmallArray <int32> m_carefulWeaponPrefs = { 0, 2, 1, 4, 25, 6, 3, 7, 8, 12, 10, 13, 11, 9, 24, 18, 14, 17, 16, 15, 19, 20, 21, 22, 23, 5 };
   SmallArray <int32> m_botBuyEconomyTable = { 1900, 2100, 2100, 4000, 6000, 7000, 16000, 1200, 800, 1000, 3000 };
   SmallArray <int32> m_grenadeBuyPrecent = { 95, 85, 60 };

public:
   BotConfig ();
   ~BotConfig () = default;

public:

   // load the configuration files
   void loadConfigs ();

   // loads main config file
   void loadMainConfig ();

   // loads bot names
   void loadNamesConfig ();

   // loads weapons config
   void loadWeaponsConfig ();

   // loads chatter config
   void loadChatterConfig ();

   // loads chat config
   void loadChatConfig ();

   // loads language config
   void loadLanguageConfig ();

   // load bots logos config
   void loadLogosConfig ();

   // load bots avatars config
   void loadAvatarsConfig ();

   // load bots difficulty config
   void loadDifficultyConfig ();

   // loads bots map-specific config
   void loadMapSpecificConfig ();

   // sets memfile to use engine functions
   void setupMemoryFiles ();

   // picks random bot name
   BotName *pickBotName ();

   // remove bot name from used list
   void clearUsedName (Bot *bot);

   // initialize weapon info
   void initWeapons ();

   // fix weapon prices (ie for elite)
   void adjustWeaponPrices ();

   // find weapon info by weaponi d
   WeaponInfo &findWeaponById (int id);

   // translates bot message into needed language
   const char *translate (StringRef input);

private:
   bool isCommentLine (StringRef line) const {
      if (line.empty ()) {
         return true;
      }
      return line.substr (0, 1).findFirstOf ("#/;") != String::InvalidIndex;
   };

   // hash the lang string, only the letters
   uint32 hashLangString (const char *input);

public:

   // checks whether chat banks contains messages
   bool hasChatBank (int chatType) const {
      return !m_chat[chatType].empty ();
   }

   // checks whether chatter banks contains messages
   bool hasChatterBank (int chatterType) const {
      return !m_chatter[chatterType].empty ();
   }

   // pick random phrase from chat bank
   StringRef pickRandomFromChatBank (int chatType) {
      return m_chat[chatType].random ();
   }

   // pick random phrase from chatter bank
   const ChatterItem &pickRandomFromChatterBank (int chatterType) {
      return m_chatter[chatterType].random ();
   }

   // gets chatter repeat-interval
   float getChatterMessageRepeatInterval (int chatterType) const {
      return m_chatter[chatterType][0].repeat;
   }

   // get's the replies array
   Array <Keywords> &getReplies () {
      return m_replies;
   }

   // get's the weapon info data
   SmallArray <WeaponInfo> &getWeapons () {
      return m_weapons;
   }

   // get's raw weapon info
   WeaponInfo *getRawWeapons () {
      return m_weapons.begin ();
   }

   // get's the weapons prop
   WeaponProp &getWeaponProp (int id) {
      return m_weaponProps[id];
   }

   // get's weapon preferences for personality
   int32 *getWeaponPrefs (int personality) const {
      switch (personality) {
      case Personality::Normal:
      default:
         return m_normalWeaponPrefs.data ();

      case Personality::Rusher:
         return m_rusherWeaponPrefs.data ();

      case Personality::Careful:
         return m_carefulWeaponPrefs.data ();
      }
   }

   // get's the difficulty level tweaks
   DifficultyData *getDifficultyTweaks (int32 level) {
      if (level < Difficulty::Noob || level > Difficulty::Expert) {
         return &m_difficulty[Difficulty::Expert];
      }
      return &m_difficulty[level];
   }

   // get economics value
   int32 *getEconLimit () {
      return m_botBuyEconomyTable.data ();
   }

   // get's grenade buy percents
   bool chanceToBuyGrenade (int grenadeType) const {
      return rg.chance (m_grenadeBuyPrecent[grenadeType]);
   }

   // get's random avatar for player (if any)
   StringRef getRandomAvatar () const {
      if (!m_avatars.empty ()) {
         return m_avatars.random ();
      }
      return "";
   }

   // get's random logo index
   int32 getRandomLogoIndex () const {
      return static_cast <int32> (m_logos.index (m_logos.random ()));
   }

   // get random name by index
   StringRef getRandomLogoName (int index) {
      return m_logos[index];
   }
};

// explose global
CR_EXPOSE_GLOBAL_SINGLETON (BotConfig, conf);
