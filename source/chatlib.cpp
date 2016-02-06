//
// Yet Another POD-Bot, based on PODBot by Markus Klinge ("CountFloyd").
// Copyright (c) YaPB Development Team.
//
// This software is licensed under the BSD-style license.
// Additional exceptions apply. For full license details, see LICENSE.txt or visit:
//     http://yapb.jeefo.net/license
//

#include <core.h>

ConVar yb_chat ("yb_chat", "1");

void StripTags (char *buffer)
{
   // this function strips 'clan' tags specified below in given string buffer

   const char *tagOpen[] = {"-=", "-[", "-]", "-}", "-{", "<[", "<]", "[-", "]-", "{-", "}-", "[[", "[", "{", "]", "}", "<", ">", "-", "|", "=", "+", "(", ")"};
   const char *tagClose[] = {"=-", "]-", "[-", "{-", "}-", "]>", "[>", "-]", "-[", "-}", "-{", "]]", "]", "}", "[", "{", ">", "<", "-", "|", "=", "+", ")", "("};

   int index, fieldStart, fieldStop, i;
   int length = strlen (buffer); // get length of string

   // foreach known tag...
   for (index = 0; index < ARRAYSIZE_HLSDK (tagOpen); index++)
   {
      fieldStart = strstr (buffer, tagOpen[index]) - buffer; // look for a tag start

      // have we found a tag start?
      if (fieldStart >= 0 && fieldStart < 32)
      {
         fieldStop = strstr (buffer, tagClose[index]) - buffer; // look for a tag stop

         // have we found a tag stop?
         if (fieldStop > fieldStart && fieldStop < 32)
         {
            int tagLength = strlen (tagClose[index]);

            for (i = fieldStart; i < length - (fieldStop + tagLength - fieldStart); i++)
               buffer[i] = buffer[i + (fieldStop + tagLength - fieldStart)]; // overwrite the buffer with the stripped string
            
            buffer[i] = 0x0; // terminate the string
         }
      }
   }

   // have we stripped too much (all the stuff)?
   if (buffer[0] != '\0')
   {
      strtrim (buffer); // if so, string is just a tag

      int tagLength = 0;

      // strip just the tag part...
      for (index = 0; index < ARRAYSIZE_HLSDK (tagOpen); index++)
      {
         fieldStart = strstr (buffer, tagOpen[index]) - buffer; // look for a tag start

         // have we found a tag start?
         if (fieldStart >= 0 && fieldStart < 32)
         {
            tagLength = strlen (tagOpen[index]);

            for (i = fieldStart; i < length - tagLength; i++)
               buffer[i] = buffer[i + tagLength]; // overwrite the buffer with the stripped string

            buffer[i] = 0x0; // terminate the string

            fieldStart = strstr (buffer, tagClose[index]) - buffer; // look for a tag stop

            // have we found a tag stop ?
            if (fieldStart >= 0 && fieldStart < 32)
            {
               tagLength = strlen (tagClose[index]);

               for (i = fieldStart; i < length - tagLength; i++)
                  buffer[i] = buffer[i + tagLength]; // overwrite the buffer with the stripped string

               buffer[i] = 0; // terminate the string
            }
         }
      }
   }
   strtrim (buffer); // to finish, strip eventual blanks after and before the tag marks
}

char *HumanizeName (char *name)
{
   // this function humanize player name (i.e. trim clan and switch to lower case (sometimes))

   static char outputName[64]; // create return name buffer
   strncpy (outputName, name, SIZEOF_CHAR (outputName)); // copy name to new buffer

   // drop tag marks, 80 percent of time
   if (Random.Long (1, 100) < 80)
      StripTags (outputName);
   else
      strtrim (outputName);

   // sometimes switch name to lower characters
   // note: since we're using russian names written in english, we reduce this shit to 6 percent
   if (Random.Long (1, 100) <= 6)
   {
      for (int i = 0; i < static_cast <int> (strlen (outputName)); i++)
         outputName[i] = tolower (outputName[i]); // to lower case
   }
   return &outputName[0]; // return terminated string
}

void HumanizeChat (char *buffer)
{
   // this function humanize chat string to be more handwritten

   int length = strlen (buffer); // get length of string
   int i = 0;

   // sometimes switch text to lowercase
   // note: since we're using russian chat written in english, we reduce this shit to 4 percent
   if (Random.Long (1, 100) <= 4)
   {
      for (i = 0; i < length; i++)
         buffer[i] = tolower (buffer[i]); // switch to lowercase
   }

   if (length > 15)
   {
      // "length / 2" percent of time drop a character
      if (Random.Long (1, 100) < (length / 2))
      {
         int pos = Random.Long ((length / 8), length - (length / 8)); // chose random position in string

         for (i = pos; i < length - 1; i++)
            buffer[i] = buffer[i + 1]; // overwrite the buffer with stripped string

         buffer[i] = 0x0; // terminate string;
         length--; // update new string length
      }

      // "length" / 4 precent of time swap character
      if (Random.Long (1, 100) < (length / 4))
      {
         int pos = Random.Long ((length / 8), ((3 * length) / 8)); // choose random position in string
         char ch = buffer[pos]; // swap characters

         buffer[pos] = buffer[pos + 1];
         buffer[pos + 1] = ch;
      }
   }
   buffer[length] = 0; // terminate string
}

void Bot::PrepareChatMessage (char *text)
{
   // this function parses messages from the botchat, replaces keywords and converts names into a more human style

   if (!yb_chat.GetBool () || IsNullString (text))
      return;

   #define ASSIGN_TALK_ENTITY() if (!IsEntityNull (talkEntity)) strncat (m_tempStrings, HumanizeName (const_cast <char *> (STRING (talkEntity->v.netname))), SIZEOF_CHAR (m_tempStrings))

   memset (&m_tempStrings, 0, sizeof (m_tempStrings));

   char *textStart = text;
   char *pattern = text;

   edict_t *talkEntity = NULL;

   while (pattern != NULL)
   {
      // all replacement placeholders start with a %
      pattern = strstr (textStart, "%");

      if (pattern != NULL)
      {
         int length = pattern - textStart;

         if (length > 0)
            strncpy (m_tempStrings, textStart, length);

         pattern++;

         // player with most frags?
         if (*pattern == 'f')
         {
            int highestFrags = -9000; // just pick some start value
            int index = 0;

            for (int i = 0; i < GetMaxClients (); i++)
            {
               if (!(g_clients[i].flags & CF_USED) || g_clients[i].ent == GetEntity ())
                  continue;

               int frags = static_cast <int> (g_clients[i].ent->v.frags);

               if (frags > highestFrags)
               {
                  highestFrags = frags;
                  index = i;
               }
            }
            talkEntity = g_clients[index].ent;

            ASSIGN_TALK_ENTITY ();
         }
         // mapname?
         else if (*pattern == 'm')
            strncat (m_tempStrings, GetMapName (), SIZEOF_CHAR (m_tempStrings));
         // roundtime?
         else if (*pattern == 'r')
         {
            int time = static_cast <int> (g_timeRoundEnd - GetWorldTime ());
            strncat (m_tempStrings, FormatBuffer ("%02d:%02d", time / 60, time % 60), SIZEOF_CHAR (m_tempStrings));
         }
         // chat reply?
         else if (*pattern == 's')
         {
            talkEntity = EntityOfIndex (m_sayTextBuffer.entityIndex);
            ASSIGN_TALK_ENTITY ();
         }
         // teammate alive?
         else if (*pattern == 't')
         {
            int i;

            for (i = 0; i < GetMaxClients (); i++)
            {
               if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
                  continue;

               break;
            }

            if (i < GetMaxClients ())
            {
               if (!IsEntityNull (pev->dmg_inflictor) && m_team == GetTeam (pev->dmg_inflictor))
                  talkEntity = pev->dmg_inflictor;
               else
                  talkEntity = g_clients[i].ent;

               ASSIGN_TALK_ENTITY ();
            }
            else // no teammates alive...
            {
               for (i = 0; i < GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || g_clients[i].team != m_team || g_clients[i].ent == GetEntity ())
                     continue;

                  break;
               }

               if (i < GetMaxClients ())
               {
                  talkEntity = g_clients[i].ent;

                  ASSIGN_TALK_ENTITY ();
               }
            }
         }
         else if (*pattern == 'e')
         {
            int i;

            for (i = 0; i < GetMaxClients (); i++)
            {
               if (!(g_clients[i].flags & CF_USED) || !(g_clients[i].flags & CF_ALIVE) || g_clients[i].team == m_team || g_clients[i].ent == GetEntity ())
                  continue;
               break;
            }

            if (i < GetMaxClients ())
            {
               talkEntity = g_clients[i].ent;
               ASSIGN_TALK_ENTITY ();
            }
            else // no teammates alive...
            {
               for (i = 0; i < GetMaxClients (); i++)
               {
                  if (!(g_clients[i].flags & CF_USED) || g_clients[i].team == m_team || g_clients[i].ent == GetEntity ())
                     continue;
                  break;
               }
               if (i < GetMaxClients ())
               {
                  talkEntity = g_clients[i].ent;
                  ASSIGN_TALK_ENTITY ();
               }
            }
         }
         else if (*pattern == 'd')
         {
            if (g_gameVersion == CSVERSION_CZERO)
            {
               if (Random.Long (1, 100) < 30)
                  strcat (m_tempStrings, "CZ");
               else
                  strcat (m_tempStrings, "Condition Zero");
            }
            else if (g_gameVersion == CSVERSION_CSTRIKE16 || g_gameVersion == CSVERSION_LEGACY)
            {
               if (Random.Long (1, 100) < 30)
                  strcat (m_tempStrings, "CS");
               else
                  strcat (m_tempStrings, "Counter-Strike");
            }
         }
         else if (*pattern == 'v')
         {
            talkEntity = m_lastVictim;
            ASSIGN_TALK_ENTITY ();
         }
         pattern++;
         textStart = pattern;
      }
   }

   if (textStart != NULL)
   {
      // let the bots make some mistakes...
      char tempString[160];
      strncpy (tempString, textStart, SIZEOF_CHAR (tempString));

      HumanizeChat (tempString);
      strncat (m_tempStrings, tempString, SIZEOF_CHAR (m_tempStrings));
   }
}

bool CheckKeywords (char *tempMessage, char *reply)
{
   // this function checks is string contain keyword, and generates relpy to it

   if (!yb_chat.GetBool () || IsNullString (tempMessage))
      return false;

   FOR_EACH_AE (g_replyFactory, i)
   {
      FOR_EACH_AE (g_replyFactory[i].keywords, j)
      {
         // check is keyword has occurred in message
         if (strstr (tempMessage, g_replyFactory[i].keywords[j].GetBuffer ()) != NULL)
         {
            Array <String> &replies = g_replyFactory[i].usedReplies;

            if (replies.GetElementNumber () >= g_replyFactory[i].replies.GetElementNumber () / 2)
               replies.RemoveAll ();

            bool replyUsed = false;
            const char *generatedReply = g_replyFactory[i].replies.GetRandomElement ();

            // don't say this twice
            FOR_EACH_AE (replies, k)
            {
               if (strstr (replies[k].GetBuffer (), generatedReply) != NULL)
                  replyUsed = true;
            }

            // reply not used, so use it
            if (!replyUsed)
            {
               strcpy (reply, generatedReply); // update final buffer
               replies.Push (generatedReply); //add to ignore list

               return true;
            }
         }
      }
   }

   // didn't find a keyword? 70% of the time use some universal reply
   if (Random.Long (1, 100) < 70 && !g_chatFactory[CHAT_NOKW].IsEmpty ())
   {
      strcpy (reply, g_chatFactory[CHAT_NOKW].GetRandomElement ().GetBuffer ());
      return true;
   }
   return false;
}

bool Bot::ParseChat (char *reply)
{
   // this function parse chat buffer, and prepare buffer to keyword searching

   char tempMessage[512];
   strcpy (tempMessage, m_sayTextBuffer.sayText); // copy to safe place

   // text to uppercase for keyword parsing
   for (int i = 0; i < static_cast <int> (strlen (tempMessage)); i++)
      tempMessage[i] = toupper (tempMessage[i]);

   return CheckKeywords (tempMessage, reply);
}

bool Bot::RepliesToPlayer (void)
{
   // this function sends reply to a player

   if (m_sayTextBuffer.entityIndex != -1 && !IsNullString (m_sayTextBuffer.sayText))
   {
      char text[256];

      // check is time to chat is good
      if (m_sayTextBuffer.timeNextChat < GetWorldTime ())
      {
         if (Random.Long (1, 100) < m_sayTextBuffer.chatProbability + Random.Long (2, 10) && ParseChat (reinterpret_cast <char *> (&text)))
         {
            PrepareChatMessage (text);
            PushMessageQueue (GSM_SAY);

            m_sayTextBuffer.entityIndex = -1;
            m_sayTextBuffer.sayText[0] = 0x0;
            m_sayTextBuffer.timeNextChat = GetWorldTime () + m_sayTextBuffer.chatDelay;

            return true;
         }
         m_sayTextBuffer.entityIndex = -1;
         m_sayTextBuffer.sayText[0] = 0x0;
      }
   }
   return false;
}

void Bot::SayText (const char *text)
{
   // this function prints saytext message to all players

   if (IsNullString (text))
      return;

   FakeClientCommand (GetEntity (), "say \"%s\"", text);
}

void Bot::TeamSayText (const char *text)
{
   // this function prints saytext message only for teammates

   if (IsNullString (text))
      return;

   FakeClientCommand (GetEntity (), "say_team \"%s\"", text);
}
