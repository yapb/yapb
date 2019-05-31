//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <yapb.h>

ConVar yb_chat ("yb_chat", "1");

void stripClanTags (char *buffer) {
   // this function strips 'clan' tags specified below in given string buffer

   if (isEmptyStr (buffer)) {
      return;
   }
   size_t nameLength = strlen (buffer);

   if (nameLength < 4) {
      return;
   }

   constexpr const char *open[] = { "-=", "-[", "-]", "-}", "-{", "<[", "<]", "[-", "]-", "{-", "}-", "[[", "[", "{", "]", "}", "<", ">", "-", "|", "=", "+", "(", ")" };
   constexpr const char *close[] = { "=-", "]-", "[-", "{-", "}-", "]>", "[>", "-]", "-[", "-}", "-{", "]]", "]", "}", "[", "{", ">", "<", "-", "|", "=", "+", ")", "(" };

   // for each known tag...
   for (size_t i = 0; i < cr::arrsize (open); i++) {
      size_t start = strstr (buffer, open[i]) - buffer; // look for a tag start

      // have we found a tag start?
      if (start < 32) {
         size_t stop = strstr (buffer, close[i]) - buffer; // look for a tag stop

         // have we found a tag stop?
         if (stop > start && stop < 32) {
            size_t tag = strlen (close[i]);
            size_t j = start;

            for (; j < nameLength - (stop + tag - start); j++) {
               buffer[j] = buffer[j + (stop + tag - start)]; // overwrite the buffer with the stripped string
            }
            buffer[j] = '\0'; // terminate the string
         }
      }
   }

   // have we stripped too much (all the stuff)?
   if (isEmptyStr (buffer)) {
      String::trimChars (buffer);
      return;
   }
   String::trimChars (buffer); // if so, string is just a tag

   // strip just the tag part...
   for (size_t i = 0; i < cr::arrsize (open); i++) {
      size_t start = strstr (buffer, open[i]) - buffer; // look for a tag start

      // have we found a tag start?
      if (start < 32) {
         size_t tag = strlen (open[i]);
         size_t j = start;

         for (; j < nameLength - tag; j++) {
            buffer[j] = buffer[j + tag]; // overwrite the buffer with the stripped string
         }
         buffer[j] = '\0'; // terminate the string
         start = strstr (buffer, close[i]) - buffer; // look for a tag stop

         // have we found a tag stop ?
         if (start < 32) {
            tag = strlen (close[i]);
            j = start;

            for (; j < nameLength - tag; j++) {
               buffer[j] = buffer[j + tag]; // overwrite the buffer with the stripped string
            }
            buffer[j] = 0; // terminate the string
         }
      }
   }
   String::trimChars (buffer); // to finish, strip eventual blanks after and before the tag marks
}

char *humanizeName (char *name) {
   // this function humanize player name (i.e. trim clan and switch to lower case (sometimes))

   static char outputName[64]; // create return name buffer
   strncpy (outputName, name, cr::bufsize (outputName)); // copy name to new buffer

   // drop tag marks, 80 percent of time
   if (rng.chance (80)) {
      stripClanTags (outputName);
   }
   else {
      String::trimChars (outputName);
   }

   // sometimes switch name to lower characters
   // note: since we're using russian names written in english, we reduce this shit to 6 percent
   if (rng.chance (7)) {
      for (size_t i = 0; i < strlen (outputName); i++) {
         outputName[i] = static_cast <char> (tolower (static_cast <int> (outputName[i]))); // to lower case
      }
   }
   return &outputName[0]; // return terminated string
}

void addChatErrors (char *buffer) {
   // this function humanize chat string to be more handwritten

   size_t length = strlen (buffer); // get length of string
   size_t i = 0;

   // sometimes switch text to lowercase
   // note: since we're using russian chat written in English, we reduce this shit to 4 percent
   if (rng.chance (5)) {
      for (i = 0; i < length; i++) {
         buffer[i] = static_cast <char> (tolower (static_cast <int> (buffer[i]))); // switch to lowercase
      }
   }

   if (length > 15) {
      size_t percentile = length / 2;

      // "length / 2" percent of time drop a character
      if (rng.getInt (1u, 100u) < percentile) {
         size_t pos = rng.getInt (length / 8, length - length / 8); // chose random position in string

         for (i = pos; i < length - 1; i++) {
            buffer[i] = buffer[i + 1]; // overwrite the buffer with stripped string
         }
         buffer[i] = '\0'; // terminate string;
         length--; // update new string length
      }

      // "length" / 4 precent of time swap character
      if (rng.getInt (1u, 100u) < percentile / 2) {
         size_t pos = rng.getInt (length / 8, 3 * length / 8); // choose random position in string
         char ch = buffer[pos]; // swap characters

         buffer[pos] = buffer[pos + 1];
         buffer[pos + 1] = ch;
      }
   }
   buffer[length] = 0; // terminate string
}

void Bot::prepareChatMessage (char *text) {
   // this function parses messages from the botchat, replaces keywords and converts names into a more human style

   if (!yb_chat.boolean () || isEmptyStr (text)) {
      return;
   }
   m_tempStrings.clear ();

   char *textStart = text;
   char *pattern = text;

   edict_t *talkEntity = nullptr;

   auto getHumanizedName = [] (edict_t *ent) {
      if (!engine.isNullEntity (ent)) {
         return const_cast <const char *> (humanizeName (const_cast <char *> (STRING (ent->v.netname))));
      }
      return "unknown";
   };

   while (pattern != nullptr) {
      // all replacement placeholders start with a %
      pattern = strchr (textStart, '%');

      if (pattern != nullptr) {
         size_t length = pattern - textStart;

         if (length > 0) {
            m_tempStrings = String (textStart, length);
         }
         pattern++;

         // player with most frags?
         if (*pattern == 'f') {
            int highestFrags = -9000; // just pick some start value
            int index = 0;

            for (int i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || client.ent == ent ()) {
                  continue;
               }
               int frags = static_cast <int> (client.ent->v.frags);

               if (frags > highestFrags) {
                  highestFrags = frags;
                  index = i;
               }
            }
            talkEntity = g_clients[index].ent;
            m_tempStrings += getHumanizedName (talkEntity);
         }
         // mapname?
         else if (*pattern == 'm') {
            m_tempStrings += engine.getMapName ();
         }
         // roundtime?
         else if (*pattern == 'r') {
            int time = static_cast <int> (g_timeRoundEnd - engine.timebase ());
            m_tempStrings.formatAppend ("%02d:%02d", time / 60, time % 60);
         }
         // chat reply?
         else if (*pattern == 's') {
            talkEntity = engine.entityOfIndex (m_sayTextBuffer.entityIndex);
            m_tempStrings += getHumanizedName (talkEntity);
         }
         // teammate alive?
         else if (*pattern == 't') {
            int i;

            for (i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ()) {
                  continue;
               }
               break;
            }

            if (i < engine.maxClients ()) {
               if (isPlayer (pev->dmg_inflictor) && m_team == engine.getTeam (pev->dmg_inflictor)) {
                  talkEntity = pev->dmg_inflictor;
               }
               else {
                  talkEntity = g_clients[i].ent;
               }
               m_tempStrings += getHumanizedName (talkEntity);
            }
            else // no teammates alive...
            {
               for (i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || client.team != m_team || client.ent == ent ()) {
                     continue;
                  }
                  break;
               }

               if (i < engine.maxClients ()) {
                  talkEntity = g_clients[i].ent;
                  m_tempStrings += getHumanizedName (talkEntity);
               }
            }
         }
         else if (*pattern == 'e') {
            int i;

            for (i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team || client.ent == ent ()) {
                  continue;
               }
               break;
            }

            if (i < engine.maxClients ()) {
               talkEntity = g_clients[i].ent;
               m_tempStrings += getHumanizedName (talkEntity);
            }

            // no teammates alive ?
            else {
               for (i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || client.team == m_team || client.ent == ent ()) {
                     continue;
                  }
                  break;
               }
               if (i < engine.maxClients ()) {
                  talkEntity = g_clients[i].ent;
                  m_tempStrings += getHumanizedName (talkEntity);
               }
            }
         }
         else if (*pattern == 'd') {
            if (g_gameFlags & GAME_CZERO) {
               if (rng.chance (30)) {
                  m_tempStrings += "CZ";
               }
               else {
                  m_tempStrings += "Condition Zero";
               }
            }
            else if ((g_gameFlags & GAME_CSTRIKE16) || (g_gameFlags & GAME_LEGACY)) {
               if (rng.chance (30)) {
                  m_tempStrings += "CS";
               }
               else {
                  m_tempStrings += "Counter-Strike";
               }
            }
         }
         else if (*pattern == 'v') {
            talkEntity = m_lastVictim;
            m_tempStrings += getHumanizedName (talkEntity);
         }
         pattern++;
         textStart = pattern;
      }
   }

   if (textStart != nullptr) {
      // let the bots make some mistakes...
      char tempString[160];
      strncpy (tempString, textStart, cr::bufsize (tempString));

      addChatErrors (tempString);
      m_tempStrings += tempString;
   }
}

bool checkForKeywords (char *message, char *reply) {
   // this function checks is string contain keyword, and generates reply to it

   if (!yb_chat.boolean () || isEmptyStr (message)) {
      return false;
   }

   for (auto &factory : g_replyFactory) {
      for (auto &keyword : factory.keywords) {

         // check is keyword has occurred in message
         if (strstr (message, keyword.chars ()) != nullptr) {
            StringArray &usedReplies = factory.usedReplies;
            
            if (usedReplies.length () >= factory.replies.length () / 2) {
               usedReplies.clear ();
            }
            
            if (!factory.replies.empty ()) {
               bool replyUsed = false;
               const String &choosenReply = factory.replies.random ();

               // don't say this twice
               for (auto &used : usedReplies) {
                  if (used.contains (choosenReply)) {
                     replyUsed = true;
                     break;
                  }
               }

               // reply not used, so use it
               if (!replyUsed) {
                  strcpy (reply, choosenReply.chars ()); // update final buffer
                  usedReplies.push (choosenReply); // add to ignore list

                  return true;
               }
            }
         }
      }
   }

   // didn't find a keyword? 70% of the time use some universal reply
   if (rng.chance (70) && !g_chatFactory[CHAT_NOKW].empty ()) {
      strcpy (reply, g_chatFactory[CHAT_NOKW].random ().chars ());
      return true;
   }
   return false;
}

bool Bot::processChatKeywords (char *reply) {
   // this function parse chat buffer, and prepare buffer to keyword searching

   char tempMessage[512];
   size_t maxLength = cr::bufsize (tempMessage);

   strncpy (tempMessage, m_sayTextBuffer.sayText.chars (), maxLength); // copy to safe place

   // text to uppercase for keyword parsing
   for (size_t i = 0; i < maxLength; i++) {
      tempMessage[i] = static_cast <char> (toupper (static_cast <int> (tempMessage[i])));
   }
   return checkForKeywords (tempMessage, reply);
}

bool Bot::isReplyingToChat (void) {
   // this function sends reply to a player

   if (m_sayTextBuffer.entityIndex != -1 && !m_sayTextBuffer.sayText.empty ()) {
      char text[256];

      // check is time to chat is good
      if (m_sayTextBuffer.timeNextChat < engine.timebase ()) {
         if (rng.chance (m_sayTextBuffer.chatProbability + rng.getInt (15, 35)) && processChatKeywords (reinterpret_cast <char *> (&text))) {
            prepareChatMessage (text);
            pushMsgQueue (GAME_MSG_SAY_CMD);

  
            m_sayTextBuffer.entityIndex = -1;
            m_sayTextBuffer.timeNextChat = engine.timebase () + m_sayTextBuffer.chatDelay;
            m_sayTextBuffer.sayText.clear ();

            return true;
         }
         m_sayTextBuffer.entityIndex = -1;
         m_sayTextBuffer.sayText.clear ();
      }
   }
   return false;
}

void Bot::say (const char *text) {
   // this function prints saytext message to all players

   if (isEmptyStr (text)) {
      return;
   }
   engine.execBotCmd (ent (), "say \"%s\"", text);
}

void Bot::sayTeam (const char *text) {
   // this function prints saytext message only for teammates

   if (isEmptyStr (text)) {
      return;
   }
   engine.execBotCmd (ent (), "say_team \"%s\"", text);
}
