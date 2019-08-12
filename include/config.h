//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#pragma once

// botname structure definition
struct BotName {
   String name = "";
   int usedBy = -1;

public:
   BotName () = default;
   BotName (String &name, int usedBy) : name (cr::move (name)), usedBy (usedBy) { }
};

// voice config structure definition
struct ChatterItem {
   String name;
   float repeat;
   float duration;

public:
   ChatterItem (String name, float repeat, float duration) : name (cr::move (name)), repeat (repeat), duration (duration) { }
};

// language hasher
struct HashLangString {
   uint32 operator () (const String &key) const {
      auto str = reinterpret_cast <uint8 *> (const_cast <char *> (key.chars ()));
      uint32 hash = 0;

      while (*str++) {
         if (!isalnum (*str)) {
            continue;
         }
         hash = ((*str << 5) + hash) + *str;
      }
      return hash;
   }
};

// mostly config stuff, and some stuff dealing with menus
class BotConfig final : public Singleton <BotConfig> {
private:
   Array <StringArray> m_chat;
   Array <Array <ChatterItem>> m_chatter;

   Array <BotName> m_botNames;
   Array <Keywords> m_replies;
   SmallArray <WeaponInfo> m_weapons;
   SmallArray <WeaponProp> m_weaponProps;

   StringArray m_logos;
   StringArray m_avatars;

   Dictionary <String, String, HashLangString> m_language;

   // default tables for personality weapon preferences, overridden by general.cfg
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
   const char *translate (const char *input);

private:
   bool isCommentLine (const String &line) {
      const char ch = line.at (0);
      return ch == '#' || ch == '/' || ch == '\r' || ch == ';' || ch == 0 || ch == ' ';
   };

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
   const String &pickRandomFromChatBank (int chatType) {
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

   // get economics value
   int32 *getEconLimit () {
      return m_botBuyEconomyTable.data ();
   }

   // get's grenade buy percents
   bool chanceToBuyGrenade (int grenadeType) const {
      return rg.chance (m_grenadeBuyPrecent[grenadeType]);
   }

   // get's random avatar for player (if any)
   String getRandomAvatar () const {
      if (!m_avatars.empty ()) {
         return m_avatars.random ();
      }
      return "";
   }

   // get's random logo index
   int getRandomLogoIndex () const {
      return m_logos.index (m_logos.random ());
   }

   // get random name by index
   const String &getRandomLogoName (int index) const {
      return m_logos[index];
   }
};


// explose global
static auto &conf = BotConfig::get ();