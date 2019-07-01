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

void BotUtils::stripTags (String &line) {
   if (line.empty ()) {
      return;
   }

   for (const auto &tag : m_tags) {
      const size_t start = line.find (tag.first, 0);

      if (start != String::INVALID_INDEX) {
         const size_t end = line.find (tag.second, start);
         const size_t diff = end - start;

         if (end != String::INVALID_INDEX && end > start && diff < 32 && diff > 4) {
            line.erase (start, diff + tag.second.length ());
            break;
         }
      }
   }
}

void BotUtils::humanizePlayerName (String &playerName) {
   if (playerName.empty ()) {
      return;
   }

   // drop tag marks, 80 percent of time
   if (rng.chance (80)) {
      stripTags (playerName);
   }
   else {
      playerName.trim ();
   }

   // sometimes switch name to lower characters, only valid for the english languge
   if (rng.chance (15) && strcmp (yb_language.str (), "en") == 0) {
      playerName.lowercase ();
   }
}

void BotUtils::addChatErrors (String &line) {
   // sometimes switch name to lower characters, only valid for the english languge
   if (rng.chance (15) && strcmp (yb_language.str (), "en") == 0) {
      line.lowercase ();
   }
   auto length = line.length ();

   if (length > 15) {
      size_t percentile = line.length () / 2;

      // "length / 2" percent of time drop a character
      if (rng.chance (percentile)) {
         line.erase (rng.getInt (length / 8, length - length / 8));
      }

      // "length" / 4 precent of time swap character
      if (rng.chance (percentile / 2)) {
         size_t pos = rng.getInt (length / 8, 3 * length / 8); // choose random position in string
         cr::swap (line[pos], line[pos + 1]);
      }
   }
}

bool BotUtils::checkKeywords (const String &line, String &reply) {
   // this function checks is string contain keyword, and generates reply to it

   if (!yb_chat.boolean () || line.empty ()) {
      return false;
   }

   for (auto &factory : conf.getReplies ()) {
      for (auto &keyword : factory.keywords) {

         // check is keyword has occurred in message
         if (line.find (keyword, 0) != String::INVALID_INDEX) {
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
                  reply.assign (choosenReply); // update final buffer
                  usedReplies.push (choosenReply); // add to ignore list

                  return true;
               }
            }
         }
      }
   }
   auto &chat = conf.getChat ();

   // didn't find a keyword? 70% of the time use some universal reply
   if (rng.chance (70) && !chat[CHAT_NOKW].empty ()) {
      reply.assign (chat[CHAT_NOKW].random ());
      return true;
   }
   return false;
}

void Bot::prepareChatMessage (const String &message) {
   // this function parses messages from the botchat, replaces keywords and converts names into a more human style

   if (!yb_chat.boolean () || message.empty ()) {
      return;
   }
   m_chatBuffer = message;

   // must be called before return or on the end
   auto finishPreparation = [&] (void) {
      if (!m_chatBuffer.empty ()) {
         util.addChatErrors (m_chatBuffer);
      }
   };

   // need to check if we're have special symbols
   size_t pos = message.find ('%', 0);

   // nothing found, bail out
   if (pos == String::INVALID_INDEX || pos >= message.length ()) {
      finishPreparation ();
      return;
   }

   // get the humanized name out of client
   auto humanizedName = [] (const Client &client) -> String {
      if (!util.isPlayer (client.ent)) {
         return cr::move (String ("unknown"));
      }
      String playerName = STRING (client.ent->v.netname);
      util.humanizePlayerName (playerName);

      return cr::move (playerName);
   };

   // find highfrag player
   auto getHighfragPlayer = [&] (void) -> String {
      int highestFrags = -1;
      int index = 0;

      for (int i = 0; i < game.maxClients (); i++) {
         const Client &client = util.getClient (i);

         if (!(client.flags & CF_USED) || client.ent == ent ()) {
            continue;
         }
         int frags = static_cast <int> (client.ent->v.frags);

         if (frags > highestFrags) {
            highestFrags = frags;
            index = i;
         }
      }
      return humanizedName (util.getClient (index));
   };

   // get roundtime
   auto getRoundTime = [] (void) -> String {
      int roundTimeSecs = static_cast <int> (bots.getRoundEndTime () - game.timebase ());
      
      String roundTime;
      roundTime.assign ("%02d:%02d", roundTimeSecs / 60, cr::abs (roundTimeSecs) % 60);

      return cr::move (roundTime);
   };

   // get bot's victim
   auto getMyVictim = [&] (void) -> String {
      for (const Client &client : util.getClients ()) {
         if (client.ent == m_lastVictim) {
            return humanizedName (client);
         }
      }
      return cr::move (String ("unknown"));
   };

   // get the game name alias
   auto getGameName = [] (void) -> String {
      String gameName;

      if (game.is (GAME_CZERO)) {
         if (rng.chance (30)) {
            gameName = "CZ";
         }
         else {
            gameName = "Condition Zero";
         }
      }
      else if (game.is (GAME_CSTRIKE16) || game.is (GAME_LEGACY)) {
         if (rng.chance (30)) {
            gameName = "CS";
         }
         else {
            gameName = "Counter-Strike";
         }
      }
      return cr::move (gameName);
   };

   // get enemy or teammate alive
   auto getPlayerAlive = [&] (bool needsEnemy) -> String {
      int index;

      for (index = 0; index < game.maxClients (); index++) {
         const Client &client = util.getClient (index);

         if (!(client.flags & CF_USED) || !(client.flags & CF_ALIVE) || client.ent == ent ()) {
            continue;
         }

         if ((needsEnemy && m_team == client.team) || (!needsEnemy && m_team != client.team)) {
            continue;
         }
         break;
      }

      if (index < game.maxClients ()) {
         if (!needsEnemy && util.isPlayer (pev->dmg_inflictor) && m_team == game.getTeam (pev->dmg_inflictor)) {
            return humanizedName (util.getClient (game.indexOfEntity (pev->dmg_inflictor) - 1));
         }
         else {
            return humanizedName (util.getClient (index));
         }
      }
      else {
         for (index = 0; index < game.maxClients (); index++) {
            const Client &client = util.getClient (index);

            if (!(client.flags & CF_USED) || client.team != m_team || client.ent == ent ()) {
               continue;
            }

            if ((needsEnemy && m_team != client.team) || (!needsEnemy && m_team == client.team)) {
               continue;
            }
            break;
         }

         if (index < game.maxClients ()) {
            return humanizedName (util.getClient (index));
         }
      }
      return cr::move (String ("unknown"));
   };

   // found one, let's do replace
   switch (message[pos + 1]) {

      // the highest frag player
   case 'f':
      m_chatBuffer.replace ("%f", getHighfragPlayer ());
      break;

      // current map name
   case 'm':
      m_chatBuffer.replace ("%m", game.getMapName ());
      break;

      // round time
   case 'r':
      m_chatBuffer.replace ("%r", getRoundTime ());
      break;

      // chat reply
   case 's':
      if (m_sayTextBuffer.entityIndex != -1) {
         m_chatBuffer.replace ("%s", humanizedName (util.getClient (m_sayTextBuffer.entityIndex)));
      }
      else {
         m_chatBuffer.replace ("%s", getHighfragPlayer ());
      }
      break;

      // last bot victim
   case 'v':
      m_chatBuffer.replace ("%v", getMyVictim ());
      break;

      // game name
   case 'd':
      m_chatBuffer.replace ("%d", getGameName ());
      break;

      // teammate alive
   case 't':
      m_chatBuffer.replace ("%t", getPlayerAlive (false));
      break;

      // enemy alive
   case 'e':
      m_chatBuffer.replace ("%e", getPlayerAlive (true));
      break;

   };
   finishPreparation ();
}

bool Bot::checkChatKeywords (String &reply) {
   // this function parse chat buffer, and prepare buffer to keyword searching

   String message = m_sayTextBuffer.sayText;
   return util.checkKeywords (message.uppercase (), reply);
}

bool Bot::isReplyingToChat (void) {
   // this function sends reply to a player

   if (m_sayTextBuffer.entityIndex != -1 && !m_sayTextBuffer.sayText.empty ()) {
      String message;

      // check is time to chat is good
      if (m_sayTextBuffer.timeNextChat < game.timebase ()) {
         if (rng.chance (m_sayTextBuffer.chatProbability + rng.getInt (25, 45)) && checkChatKeywords (message)) {
            prepareChatMessage (message);
            pushMsgQueue (GAME_MSG_SAY_CMD);
  
            m_sayTextBuffer.entityIndex = -1;
            m_sayTextBuffer.timeNextChat = game.timebase () + m_sayTextBuffer.chatDelay;
            m_sayTextBuffer.sayText.clear ();

            return true;
         }
         m_sayTextBuffer.entityIndex = -1;
         m_sayTextBuffer.sayText.clear ();
      }
   }
   return false;
}

void Bot::checkForChat (void) {


   // bot chatting turned on?
   if (!m_notKilled && yb_chat.boolean () && m_lastChatTime + 10.0 < game.timebase () && bots.getLastChatTimestamp () + 5.0f < game.timebase () && !isReplyingToChat ()) {
      auto &chat = conf.getChat ();

      // say a text every now and then
      if (rng.chance (50)) {
         m_lastChatTime = game.timebase ();
         bots.setLastChatTimestamp (game.timebase ());

         if (!chat[CHAT_DEAD].empty ()) {
            const String &phrase = chat[CHAT_DEAD].random ();
            bool sayBufferExists = false;

            // search for last messages, sayed
            for (auto &sentence : m_sayTextBuffer.lastUsedSentences) {
               if (strncmp (sentence.chars (), phrase.chars (), sentence.length ()) == 0) {
                  sayBufferExists = true;
                  break;
               }
            }

            if (!sayBufferExists) {
               prepareChatMessage (phrase);
               pushMsgQueue (GAME_MSG_SAY_CMD);

               // add to ignore list
               m_sayTextBuffer.lastUsedSentences.push (phrase);
            }
         }

         // clear the used line buffer every now and then
         if (static_cast <int> (m_sayTextBuffer.lastUsedSentences.length ()) > rng.getInt (4, 6)) {
            m_sayTextBuffer.lastUsedSentences.clear ();
         }
      }
   }
}

void Bot::say (const char *text) {
   // this function prints saytext message to all players

   if (util.isEmptyStr (text) || !yb_chat.boolean ()) {
      return;
   }
   game.execBotCmd (ent (), "say \"%s\"", text);
}

void Bot::sayTeam (const char *text) {
   // this function prints saytext message only for teammates

   if (util.isEmptyStr (text) || !yb_chat.boolean ()) {
      return;
   }
   game.execBotCmd (ent (), "say_team \"%s\"", text);
}
