//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     https://yapb.ru/license
//

#include <core.h>

ConVar yb_chat ("yb_chat", "1");

void StripTags (char *buffer) {
   // this function strips 'clan' tags specified below in given string buffer

   if (!buffer)
      return;

   const char *tagOpen[] = {"-=", "-[", "-]", "-}", "-{", "<[", "<]", "[-", "]-", "{-", "}-", "[[", "[", "{", "]", "}", "<", ">", "-", "|", "=", "+", "(", ")"};
   const char *tagClose[] = {"=-", "]-", "[-", "{-", "}-", "]>", "[>", "-]", "-[", "-}", "-{", "]]", "]", "}", "[", "{", ">", "<", "-", "|", "=", "+", ")", "("};

   int index, fieldStart, fieldStop, i;
   int length = strlen (buffer); // get length of string

   // foreach known tag...
   for (index = 0; index < ARRAYSIZE_HLSDK (tagOpen); index++) {
      fieldStart = strstr (buffer, tagOpen[index]) - buffer; // look for a tag start

      // have we found a tag start?
      if (fieldStart >= 0 && fieldStart < 32) {
         fieldStop = strstr (buffer, tagClose[index]) - buffer; // look for a tag stop

         // have we found a tag stop?
         if (fieldStop > fieldStart && fieldStop < 32) {
            int tagLength = strlen (tagClose[index]);

            for (i = fieldStart; i < length - (fieldStop + tagLength - fieldStart); i++)
               buffer[i] = buffer[i + (fieldStop + tagLength - fieldStart)]; // overwrite the buffer with the stripped string

            buffer[i] = 0x0; // terminate the string
         }
      }
   }

   // have we stripped too much (all the stuff)?
   if (buffer[0] != '\0') {
      String::trimChars (buffer); // if so, string is just a tag

      int tagLength = 0;

      // strip just the tag part...
      for (index = 0; index < ARRAYSIZE_HLSDK (tagOpen); index++) {
         fieldStart = strstr (buffer, tagOpen[index]) - buffer; // look for a tag start

         // have we found a tag start?
         if (fieldStart >= 0 && fieldStart < 32) {
            tagLength = strlen (tagOpen[index]);

            for (i = fieldStart; i < length - tagLength; i++)
               buffer[i] = buffer[i + tagLength]; // overwrite the buffer with the stripped string

            buffer[i] = 0x0; // terminate the string

            fieldStart = strstr (buffer, tagClose[index]) - buffer; // look for a tag stop

            // have we found a tag stop ?
            if (fieldStart >= 0 && fieldStart < 32) {
               tagLength = strlen (tagClose[index]);

               for (i = fieldStart; i < length - tagLength; i++)
                  buffer[i] = buffer[i + tagLength]; // overwrite the buffer with the stripped string

               buffer[i] = 0; // terminate the string
            }
         }
      }
   }
   String::trimChars (buffer); // to finish, strip eventual blanks after and before the tag marks
}

char *HumanizeName (char *name) {
   // this function humanize player name (i.e. trim clan and switch to lower case (sometimes))

   static char outputName[64]; // create return name buffer
   strncpy (outputName, name, A_bufsize (outputName)); // copy name to new buffer

   // drop tag marks, 80 percent of time
   if (rng.getInt (1, 100) < 80)
      StripTags (outputName);
   else
      String::trimChars (outputName);

   // sometimes switch name to lower characters
   // note: since we're using russian names written in english, we reduce this shit to 6 percent
   if (rng.getInt (1, 100) <= 6) {
      for (int i = 0; i < static_cast<int> (strlen (outputName)); i++)
         outputName[i] = static_cast<char> (tolower (static_cast<int> (outputName[i]))); // to lower case
   }
   return &outputName[0]; // return terminated string
}

void HumanizeChat (char *buffer) {
   // this function humanize chat string to be more handwritten

   int length = strlen (buffer); // get length of string
   int i = 0;

   // sometimes switch text to lowercase
   // note: since we're using russian chat written in english, we reduce this shit to 4 percent
   if (rng.getInt (1, 100) <= 4) {
      for (i = 0; i < length; i++)
         buffer[i] = static_cast<char> (tolower (static_cast<int> (buffer[i]))); // switch to lowercase
   }

   if (length > 15) {
      // "length / 2" percent of time drop a character
      if (rng.getInt (1, 100) < (length / 2)) {
         int pos = rng.getInt ((length / 8), length - (length / 8)); // chose random position in string

         for (i = pos; i < length - 1; i++)
            buffer[i] = buffer[i + 1]; // overwrite the buffer with stripped string

         buffer[i] = 0x0; // terminate string;
         length--; // update new string length
      }

      // "length" / 4 precent of time swap character
      if (rng.getInt (1, 100) < (length / 4)) {
         int pos = rng.getInt ((length / 8), ((3 * length) / 8)); // choose random position in string
         char ch = buffer[pos]; // swap characters

         buffer[pos] = buffer[pos + 1];
         buffer[pos + 1] = ch;
      }
   }
   buffer[length] = 0; // terminate string
}

void Bot::prepareChatMessage (char *text) {
   // this function parses messages from the botchat, replaces keywords and converts names into a more human style

   if (!yb_chat.boolean () || isEmptyStr (text))
      return;

   memset (&m_tempStrings, 0, sizeof (m_tempStrings));

   char *textStart = text;
   char *pattern = text;

   edict_t *talkEntity = nullptr;

   static auto assignChatTalkEntity = [](edict_t *ent, char *buffer) {
      const char *botName = STRING (ent->v.netname);

      if (!engine.isNullEntity (ent))
         strncat (buffer, HumanizeName (const_cast<char *> (botName)), 159);
   };

   while (pattern != nullptr) {
      // all replacement placeholders start with a %
      pattern = strstr (textStart, "%");

      if (pattern != nullptr) {
         int length = pattern - textStart;

         if (length > 0)
            strncpy (m_tempStrings, textStart, length);

         pattern++;

         // player with most frags?
         if (*pattern == 'f') {
            int highestFrags = -9000; // just pick some start value
            int index = 0;

            for (int i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || client.ent == ent ())
                  continue;

               int frags = static_cast<int> (client.ent->v.frags);

               if (frags > highestFrags) {
                  highestFrags = frags;
                  index = i;
               }
            }
            talkEntity = g_clients[index].ent;

            assignChatTalkEntity (talkEntity, m_tempStrings);
         }
         // mapname?
         else if (*pattern == 'm')
            strncat (m_tempStrings, engine.getMapName (), A_bufsize (m_tempStrings));
         // roundtime?
         else if (*pattern == 'r') {
            int time = static_cast<int> (g_timeRoundEnd - engine.timebase ());
            strncat (m_tempStrings, format ("%02d:%02d", time / 60, time % 60), A_bufsize (m_tempStrings));
         }
         // chat reply?
         else if (*pattern == 's') {
            talkEntity = engine.entityOfIndex (m_sayTextBuffer.entityIndex);
            assignChatTalkEntity (talkEntity, m_tempStrings);
         }
         // teammate alive?
         else if (*pattern == 't') {
            int i;

            for (i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team != m_team || client.ent == ent ())
                  continue;

               break;
            }

            if (i < engine.maxClients ()) {
               if (!engine.isNullEntity (pev->dmg_inflictor) && m_team == engine.getTeam (pev->dmg_inflictor))
                  talkEntity = pev->dmg_inflictor;
               else
                  talkEntity = g_clients[i].ent;

               assignChatTalkEntity (talkEntity, m_tempStrings);
            }
            else // no teammates alive...
            {
               for (i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || client.team != m_team || client.ent == ent ())
                     continue;

                  break;
               }

               if (i < engine.maxClients ()) {
                  talkEntity = g_clients[i].ent;
                  assignChatTalkEntity (talkEntity, m_tempStrings);
               }
            }
         }
         else if (*pattern == 'e') {
            int i;

            for (i = 0; i < engine.maxClients (); i++) {
               const Client &client = g_clients[i];

               if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.team == m_team || client.ent == ent ())
                  continue;

               break;
            }

            if (i < engine.maxClients ()) {
               talkEntity = g_clients[i].ent;
               assignChatTalkEntity (talkEntity, m_tempStrings);
            }
            else // no teammates alive...
            {
               for (i = 0; i < engine.maxClients (); i++) {
                  const Client &client = g_clients[i];

                  if (!(client.flags & CF_USED) || client.team == m_team || client.ent == ent ())
                     continue;

                  break;
               }
               if (i < engine.maxClients ()) {
                  talkEntity = g_clients[i].ent;
                  assignChatTalkEntity (talkEntity, m_tempStrings);
               }
            }
         }
         else if (*pattern == 'd') {
            if (g_gameFlags & GAME_CZERO) {
               if (rng.getInt (1, 100) < 30)
                  strcat (m_tempStrings, "CZ");
               else
                  strcat (m_tempStrings, "Condition Zero");
            }
            else if ((g_gameFlags & GAME_CSTRIKE16) || (g_gameFlags & GAME_LEGACY)) {
               if (rng.getInt (1, 100) < 30)
                  strcat (m_tempStrings, "CS");
               else
                  strcat (m_tempStrings, "Counter-Strike");
            }
         }
         else if (*pattern == 'v') {
            talkEntity = m_lastVictim;
            assignChatTalkEntity (talkEntity, m_tempStrings);
         }
         pattern++;
         textStart = pattern;
      }
   }

   if (textStart != nullptr) {
      // let the bots make some mistakes...
      char tempString[160];
      strncpy (tempString, textStart, A_bufsize (tempString));

      HumanizeChat (tempString);
      strncat (m_tempStrings, tempString, A_bufsize (m_tempStrings));
   }
}

bool CheckKeywords (char *message, char *reply) {
   // this function checks is string contain keyword, and generates reply to it

   if (!yb_chat.boolean () || isEmptyStr (message))
      return false;
  
   for (auto &factory : g_replyFactory) {
      for (auto &keyword : factory.keywords) {

         // check is keyword has occurred in message
         if (strstr (message, keyword.chars ()) != nullptr) {
            StringArray &replies = factory.usedReplies;

            if (replies.length () >= factory.replies.length () / 2) {
               replies.clear ();
            }
            else if (!replies.empty ()) {
               bool replyUsed = false;
               const String &choosenReply = factory.replies.random ();

               // don't say this twice
               for (auto &used : replies) {
                  if (used.contains (choosenReply)) {
                     replyUsed = true;
                     break;
                  }
               }

               // reply not used, so use it
               if (!replyUsed) {
                  strcpy (reply, choosenReply.chars ()); // update final buffer
                  replies.push (choosenReply); // add to ignore list

                  return true;
               }
            }
         }
      }
   }

   // didn't find a keyword? 70% of the time use some universal reply
   if (rng.getInt (1, 100) < 70 && !g_chatFactory[CHAT_NOKW].empty ()) {
      strcpy (reply, g_chatFactory[CHAT_NOKW].random ().chars ());
      return true;
   }
   return false;
}

bool Bot::processChatKeywords (char *reply) {
   // this function parse chat buffer, and prepare buffer to keyword searching

   char tempMessage[512];
   strcpy (tempMessage, m_sayTextBuffer.sayText); // copy to safe place

   // text to uppercase for keyword parsing
   for (int i = 0; i < static_cast<int> (strlen (tempMessage)); i++)
      tempMessage[i] = static_cast<char> (toupper (static_cast<int> (tempMessage[i])));

   return CheckKeywords (tempMessage, reply);
}

bool Bot::isReplyingToChat (void) {
   // this function sends reply to a player

   if (m_sayTextBuffer.entityIndex != -1 && !isEmptyStr (m_sayTextBuffer.sayText)) {
      char text[256];

      // check is time to chat is good
      if (m_sayTextBuffer.timeNextChat < engine.timebase ()) {
         if (rng.getInt (1, 100) < m_sayTextBuffer.chatProbability + rng.getInt (15, 25) && processChatKeywords (reinterpret_cast<char *> (&text))) {
            prepareChatMessage (text);
            pushMsgQueue (GAME_MSG_SAY_CMD);

  
            m_sayTextBuffer.entityIndex = -1;
            m_sayTextBuffer.sayText[0] = 0x0;
            m_sayTextBuffer.timeNextChat = engine.timebase () + m_sayTextBuffer.chatDelay;

            return true;
         }
         m_sayTextBuffer.entityIndex = -1;
         m_sayTextBuffer.sayText[0] = 0x0;
      }
   }
   return false;
}

void Bot::say (const char *text) {
   // this function prints saytext message to all players

   if (isEmptyStr (text))
      return;

   engine.execBotCmd (ent (), "say \"%s\"", text);
}

void Bot::sayTeam (const char *text) {
   // this function prints saytext message only for teammates

   if (isEmptyStr (text))
      return;

   engine.execBotCmd (ent (), "say_team \"%s\"", text);
}
