//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#include <yapb.h>

namespace bot {

ChatManager::ChatManager () {
  clan_tags_ = {
    { "[[", "]]" },
    { "-=", "=-" },
    { "-[", "]-" },
    { "-]", "[-" },
    { "-}", "{-" },
    { "-{", "}-" },
    { "<[", "]>" },
    { "<]", "[>" },
    { "[-", "-]" },
    { "]-", "-[" },
    { "{-", "-}" },
    { "}-", "-{" },

    { "[",  "]"  },
    { "{",  "}"  },
    { "<",  "["  },
    { ">",  "<"  },
    { ")",  "("  },
    { "-",  "-"  },
    { "|",  "|"  },
    { "=",  "="  },
    { "+",  "+"  },
    { "(",  ")"  },
  };
}

void ChatManager::StripTags (ystl::String &line) {
  if (line.empty ()) {
    return;
  }

  for (const auto &tag : clan_tags_) {
    const size_t start = line.find (tag.first, 0);

    if (start != ystl::String::InvalidIndex) {
      const size_t end = line.find (tag.second, start);

      if (end != ystl::String::InvalidIndex && end > start) {
        const size_t diff = end - start;

        if (diff < 32 && diff > 1) {
          line.erase (start, diff + tag.second.size ());
          continue;
        }
      }
    }
  }
}

void ChatManager::HumanizePlayerName (ystl::String &player_name) {
  if (player_name.empty ()) {
    return;
  }

  // drop tag marks, 80 percent of time
  if (ystl::rg.chance (80)) {
    StripTags (player_name);
  }
  else {
    player_name.trim ();
  }

  // sometimes switch name to lower characters, only valid for the english languge
  if (ystl::rg.chance (8) && cv_language.As<ystl::StringRef> () == "en") {
    player_name.lowercase ();
  }
}

void ChatManager::AddChatErrors (ystl::String &line) {
  // sometimes switch name to lower characters, only valid for the english languge
  if (ystl::rg.chance (8) && cv_language.As<ystl::StringRef> () == "en") {
    line.lowercase ();
  }
  const auto length = static_cast<int32_t> (line.size ());

  if (length > 15) {
    const auto percentile = length / 2;

    // prevent garbled text in chinese chat messages
    if (cv_language.As<ystl::StringRef> () == "chs" || cv_language.As<ystl::StringRef> () == "cht") {

      // "length / 2" percent of time drop a character
      if (ystl::rg.chance (percentile)) {

        // try several times
        for (int i = 0; i < 6; ++i) {
          auto pos = ystl::rg (length / 8, length - length / 8);
          auto ch = static_cast<uint8_t> (line[static_cast<size_t> (pos)]);

          if (ch < 0x80 && isalnum (ch)) { // apply for alphas and nums only
            line.erase (static_cast<size_t> (pos));
            break;
          }
        }
      }

      // "length" / 4 precent of time swap character
      const auto swap_length = static_cast<int32_t> (line.size ());

      if (swap_length > 3 && ystl::rg.chance (percentile / 2)) {

        // try several times
        for (int i = 0; i < 6; ++i) {

          // choose random position in string
          auto pos = static_cast<size_t> (ystl::rg (swap_length / 8, 3 * swap_length / 8));

          auto ch1 = static_cast<uint8_t> (line[pos]);
          auto ch2 = static_cast<uint8_t> (line[pos + 1]);

          if (ch1 < 0x80 && ch2 < 0x80 && isalnum (ch1) && isalnum (ch2)) {
            ystl::swap (line[pos], line[pos + 1]);
            break;
          }
        }
      }
    }
    else {
      // "length / 2" percent of time drop a character
      if (ystl::rg.chance (percentile)) {
        auto pos = ystl::rg (length / 8, length - length / 8);
        line.erase (static_cast<size_t> (pos));
      }

      // "length" / 4 precent of time swap character
      const auto swap_length = static_cast<int32_t> (line.size ());

      if (swap_length > 3 && ystl::rg.chance (percentile / 2)) {
        auto pos = static_cast<size_t> (ystl::rg (swap_length / 8, 3 * swap_length / 8)); // choose random position in string
        ystl::swap (line[pos], line[pos + 1]);
      }
    }
  }
}

bool ChatManager::CheckKeywords (ystl::StringRef line, ystl::String &reply, bool allow_generic) {
  // this function checks if string contains keyword, and generates reply to it

  if (!cv_chat || line.empty ()) {
    return false;
  }

  auto &keyword_index = conf.GetKeywordIndex ();
  auto &replies = conf.GetReplies ();

  // helper to select a reply from a factory
  auto select_reply = [] (ChatKeywords &factory, ystl::String &output) -> bool {
    if (factory.replies.empty ()) {
      return false;
    }

    // reset only after full cycle, so replies don't repeat early
    if (factory.UsedReplyCount () >= factory.replies.size ()) {
      factory.ClearUsedReplies ();
    }

    // try to find an unused reply
    ystl::StringRef choosen_reply {};
    bool found_unused = false;

    // try multiple times to find an unused reply
    for (size_t attempts = 0; attempts < factory.replies.size () * 2; ++attempts) {
      choosen_reply = factory.replies.random ();

      if (!factory.IsReplyUsed (choosen_reply)) {
        found_unused = true;
        break;
      }
    }

    // if we couldn't find an unused reply, just use a random one anyway
    if (!found_unused) {
      choosen_reply = factory.replies.random ();
    }

    // assign straight from the view
    output.assign (choosen_reply.chars (), choosen_reply.size ());
    factory.MarkReplyUsed (choosen_reply);

    return true;
  };

  // fast exact word matching using hashmap index
  if (!keyword_index.empty ()) {
    const auto view = ystl::StringRef (line.chars (), line.size ());
    size_t start = 0;

    while (start <= view.size ()) {
      auto end = view.find (' ', start);

      if (end == ystl::String::InvalidIndex) {
        end = view.size ();
      }

      // trim the word bounds in place
      while (start < end && ystl::Tokenizer::is_space (view[start])) {
        ++start;
      }
      while (end > start && ystl::Tokenizer::is_space (view[end - 1])) {
        --end;
      }

      // strip punctuation, so "noob!" matches "NOOB"
      while (end > start) {
        const auto lead = static_cast<uint8_t> (view[start]);
        const auto trail = static_cast<uint8_t> (view[end - 1]);

        if (lead < 0x80 && !isalnum (lead)) {
          ++start;
          continue;
        }

        if (trail < 0x80 && !isalnum (trail)) {
          --end;
          continue;
        }
        break;
      }

      if (end > start) {
        const auto word = ystl::StringRef (view.chars () + start, end - start);

        // check if this word matches any indexed keyword
        const auto word_hash = ystl::detail::fnv1a32_n (word.chars (), word.size ());

        if (keyword_index.exists (word_hash)) {
          const auto &indices = keyword_index[word_hash];

          // try each reply factory that has this keyword
          for (const auto index : indices) {
            if (index < replies.size ()) {
              if (select_reply (replies[index], reply)) {
                return true;
              }
            }
          }
        }
      }

      if (end == view.size ()) {
        break;
      }
      start = end + 1;
    }
  }

  // fallback to substring matching for patterns like "aimbot" in "aimbotting"
  for (auto &factory : replies) {
    for (const auto &keyword : factory.keywords) {

      // check if keyword occurs as substring in message
      if (line.find (keyword) != ystl::String::InvalidIndex) {
        if (select_reply (factory, reply)) {
          return true;
        }
      }
    }
  }

  // didn't find a keyword? 70% of the time use some universal reply
  if (allow_generic && ystl::rg.chance (70) && conf.HasChatBank (Chat::NoKeyword)) {
    reply.assign (conf.PickRandomFromChatBank (Chat::NoKeyword));
    return true;
  }
  return false;
}

void Bot::PrepareChatMessage (ystl::StringRef message) {
  // this function parses messages from the botchat, replaces keywords and converts names into a more human style

  if (!cv_chat || message.empty ()) {
    return;
  }

  size_t pos = message.find ('%');

  // nothing found, bail out
  if (pos == ystl::String::InvalidIndex || pos >= message.size ()) {
    chat_buffer_.assign (message.chars (), message.size ());

    if (!chat_buffer_.empty ()) {
      chatlib.AddChatErrors (chat_buffer_);
    }
    return;
  }

  // get the humanized name out of client
  auto humanized_name = [] (int index) -> ystl::String {
    auto ent = game.PlayerOfIndex (index);

    if (!game.IsPlayerEntity (ent)) {
      return "unknown";
    }
    ystl::String player_name = ent->v.netname.chars ();
    chatlib.HumanizePlayerName (player_name);

    return player_name;
  };

  // find highfrag player
  auto get_highfrag_player = [&] () -> ystl::String {
    int highest_frags = -1;
    int index = 0;

    for (int i = 0; i < game.MaxClients (); ++i) {
      const auto &client = clients[i];

      if (!client.IsUsedAndNot (Ent ())) {
        continue;
      }
      const auto frags = static_cast<int> (client.ent->v.frags);

      if (frags > highest_frags) {
        highest_frags = frags;
        index = i;
      }
    }
    return humanized_name (index);
  };

  // get roundtime
  auto get_round_time = [] () -> ystl::String {
    const auto round_time_secs = static_cast<int> (game_state.GetRoundEndTime () - game.Time ());

    ystl::String round_time {};
    round_time.assignf ("%02d:%02d", ystl::clamp (round_time_secs / 60, 0, 59), ystl::clamp (ystl::abs (round_time_secs % 60), 0, 59));

    return round_time;
  };

  // get bot's victim (may be missing before the first kill)
  auto get_my_victim = [&] () -> ystl::String {
    if (game.IsNullEntity (last_victim_)) {
      return "unknown";
    }
    return humanized_name (game.IndexOfPlayer (last_victim_));
  };

  // get enemy or teammate alive
  auto get_player_alive = [&] (bool needs_enemy) -> ystl::String {
    for (const auto &client : clients) {
      if (!client.IsUsedAndAlive () || client.ent == Ent ()) {
        continue;
      }
      const auto player_index = game.IndexOfPlayer (client.ent);

      if (needs_enemy && team_ != client.team) {
        return humanized_name (player_index);
      }
      else if (!needs_enemy && team_ == client.team) {
        if (game.IsPlayerEntity (pev->dmg_inflictor) && game.GetRealPlayerTeam (pev->dmg_inflictor) == team_) {

          return humanized_name (game.IndexOfPlayer (pev->dmg_inflictor));
        }
        return humanized_name (player_index);
      }
    }
    return get_highfrag_player ();
  };

  // scan original message and build result directly
  chat_buffer_.clear ();

  size_t read_pos = 0;
  size_t replace_counter = 0;

  while (replace_counter < 6 && (pos = message.find ('%', read_pos)) != ystl::String::InvalidIndex) {
    const auto replace_position = pos + 1;

    if (replace_position >= message.size ()) {
      break;
    }

    // copy literal text before this marker
    if (pos > read_pos) {
      chat_buffer_.append (message.chars () + read_pos, pos - read_pos);
    }

    // resolve and append replacement
    switch (message[replace_position]) {

      // the highest frag player
    case 'f':
      chat_buffer_.append (get_highfrag_player ());
      break;

      // current map name
    case 'm':
      chat_buffer_.append (game.GetMapName ());
      break;
      // round time
    case 'r':
      chat_buffer_.append (get_round_time ());
      break;

      // chat reply
    case 's': {
      ystl::String name = say_text_buffer_.entity_index != -1 ? humanized_name (say_text_buffer_.entity_index) : get_highfrag_player ();
      chat_buffer_.append (name);
      break;
    }
      // last bot victim
    case 'v':
      chat_buffer_.append (get_my_victim ());
      break;

      // game name
    case 'd':
      chat_buffer_.append (conf.GetGameName ());
      break;

      // teammate alive
    case 't':
      chat_buffer_.append (get_player_alive (false));
      break;

      // enemy alive
    case 'e':
      chat_buffer_.append (get_player_alive (true));
      break;

    case 'g': {
      auto author = graph.GetAuthor ();
      chat_buffer_.append (author.chars (), author.size ());
      break;
    }
    default:
      // unrecognized marker, keep as-is
      chat_buffer_.append (message.chars () + pos, 2);
      break;
    }
    read_pos = replace_position + 1;
    ++replace_counter;
  }

  // append remaining text after last marker
  if (read_pos < message.size ()) {
    chat_buffer_.append (message.chars () + read_pos, message.size () - read_pos);
  }

  if (!chat_buffer_.empty ()) {
    chatlib.AddChatErrors (chat_buffer_);
  }
}

bool Bot::CheckChatKeywords (ystl::StringRef chat_text, ystl::String &reply) {
  // this function parse chat buffer, and prepare buffer to keyword searching

  return chatlib.CheckKeywordsUpper (chat_text, reply);
}

bool Bot::IsReplyingToChat () {
  // this function sends reply to a player

  if (say_text_buffer_.entity_index == -1) {
    return false;
  }

  // resolve chat text from sender bot (if bot) or stored copy (if human)
  ystl::StringRef chat_text {};
  auto sender = bots[say_text_buffer_.entity_index];

  if (sender != nullptr) {
    chat_text = sender->chat_buffer_;
  }
  else {
    chat_text = say_text_buffer_.say_text;
  }

  if (chat_text.empty ()) {
    say_text_buffer_.entity_index = -1;
    say_text_buffer_.say_text.clear ();
    return false;
  }

  // check is time to chat is good
  if (say_text_buffer_.time_next_chat < game.Time () + rg (say_text_buffer_.chat_delay / 2, say_text_buffer_.chat_delay)) {
    // reuse the persistent reply buffer, so picking a reply doesn't allocate per message
    auto &reply_text = reply_buffer_;

    // keywords react almost always, generic chatter stays rare
    bool want_reply = false;

    if (chatlib.CheckKeywordsUpper (chat_text, reply_text, false)) {
      want_reply = rg.chance (say_text_buffer_.chat_probability + 60);
    }
    else if (rg.chance (25) && conf.HasChatBank (Chat::NoKeyword)) {
      reply_text.assign (conf.PickRandomFromChatBank (Chat::NoKeyword));
      want_reply = true;
    }

    if (want_reply) {
      PrepareChatMessage (reply_text);
      PushMsgQueue (Msg::Say);

      say_text_buffer_.entity_index = -1;
      say_text_buffer_.time_next_chat = game.Time () + say_text_buffer_.chat_delay;
      say_text_buffer_.say_text.clear ();

      return true;
    }
    say_text_buffer_.entity_index = -1;
    say_text_buffer_.say_text.clear ();
  }
  return false;
}

void Bot::PushChatMessage (Chat type, bool is_team_say) {
  if (!conf.HasChatBank (type) || !cv_chat) {
    return;
  }

  PrepareChatMessage (conf.PickRandomFromChatBank (type));
  PushMsgQueue (is_team_say ? Msg::SayTeam : Msg::Say);
}

void Bot::CheckForChat () {
  // say a text every now and then

  if (is_alive_ || !cv_chat || game.Is (GameFlags::CSDM)) {
    return;
  }

  // bot chatting turned on?
  if (rg.chance (cv_chat_percent.As<int> ()) && last_chat_timer_.greater_than (rg (6.0f, 10.0f)) &&
      bots.GetLastChatElapsedTime () > rg (2.5f, 5.0f) && !IsReplyingToChat ()) {

    if (conf.HasChatBank (Chat::Dead)) {
      ystl::StringRef phrase = conf.PickRandomFromChatBank (Chat::Dead);
      bool say_buffer_exists = false;

      // search for last messages, sayed
      for (auto &sentence : say_text_buffer_.last_used_sentences) {
        if (phrase.starts_with (sentence)) {
          say_buffer_exists = true;
          break;
        }
      }

      if (!say_buffer_exists) {
        PrepareChatMessage (phrase);
        PushMsgQueue (Msg::Say);

        last_chat_timer_.start ();
        bots.MarkLastChatTime ();

        // add to ignore list
        say_text_buffer_.last_used_sentences.push (phrase);
      }
    }

    // clear the used line buffer every now and then
    if (static_cast<int> (say_text_buffer_.last_used_sentences.size ()) > rg (4, 6)) {
      say_text_buffer_.last_used_sentences.clear ();
    }
  }
}

void Bot::SendToChat (ystl::StringRef message, bool team_only) {
  // this function prints saytext message to all players

  if (is_creature_ || message.empty () || !cv_chat) {
    return;
  }

  // special handling for legacy games
  if (game.Is (GameFlags::Legacy)) {
    SendToChatLegacy (message, team_only);
  }
  else {
    IssueCommand ("%s \"%s\"", team_only ? "say_team" : "say", message);
  }
}

void Bot::SendToChatLegacy (ystl::StringRef message, bool team_only) {
  // this function prints saytext message to all players for legacy games (< cs 1.6)

  // regular say overruns legacy hlds, so mimic gamedll host_say here

  bool dedicated_send = false;

  auto send_chat_msg = [&] (const Client &client, ystl::StringRef chat_msg) {
    if (game.IsDedicatedServer () && !dedicated_send) {
      dedicated_send = true;

      // trim the message bounds for the console output only, network messages keep the trailing newline
      const auto trimmed = ystl::Tokenizer::trim (chat_msg, "\r\n\t ");

      game.Print ("%.*s", static_cast<int> (trimmed.size ()), trimmed.chars ());
    }
    auto rcv = bots[client.ent];

    if (rcv != nullptr) {
      rcv->say_text_buffer_.entity_index = index_;

      rcv->say_text_buffer_.say_text.assign (message.chars (), message.size ());
      rcv->say_text_buffer_.time_next_chat = game.Time () + rcv->say_text_buffer_.chat_delay;
    }

    if (is_alive_ || !has_flag (client.flags, ClientFlags::Alive)) {
      MessageWriter (MSG_ONE, msgs.Id (NetMsg::SayText), nullptr, client.ent).WriteByte (index_).WriteString (chat_msg.chars ());
    }
  };

  // the chat message is identical for every recipient, so it's built only once, into the reused scratch buffer
  ystl::String &chat_msg = chatlib.ChatScratch ();
  chat_msg.clear ();

  if (team_only) {
    ystl::StringRef team_name {};

    if (team_ == Team::Terrorist) {
      team_name = "(Terrorist)";
    }
    else if (team_ == Team::CT) {
      team_name = "(Counter-Terrorist)";
    }

    if (is_alive_) {
      chat_msg.appendf ("%c%s %c%s%c :  %s\n", 0x01, team_name, 0x03, pev->netname.chars (), 0x01, message);
    }
    else {
      chat_msg.appendf ("%c*DEAD*%s %c%s%c :  %s\n", 0x01, team_name, 0x03, pev->netname.chars (), 0x01, message);
    }

    for (const auto &client : clients) {
      if (!client.IsTeammate2 (team_, Ent ())) {
        continue;
      }
      send_chat_msg (client, chat_msg);
    }
    return;
  }

  if (is_alive_) {
    chat_msg.appendf ("%c%s :  %s\n", 0x02, pev->netname.chars (), message);
  }
  else {
    chat_msg.appendf ("%c*DEAD* %c%s%c :  %s\n", 0x01, 0x03, pev->netname.chars (), 0x01, message);
  }

  for (const auto &client : clients) {
    if (!client.IsUsedAndNot (Ent ())) {
      continue;
    }
    send_chat_msg (client, chat_msg);
  }
}

} // namespace bot
