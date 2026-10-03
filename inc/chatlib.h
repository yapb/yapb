//
// YaPB, started from PODBot by Count Floyd
// Maintained by YaPB Team <yapb@jeefo.net>
//
// SPDX-License-Identifier: Unlicense
//

#pragma once

// chat types id's
namespace bot {

enum class Chat : int32_t {
  Kill = 0, // id to kill chat array
  Dead, // id to dead chat array
  Plant, // id to bomb chat array
  TeamAttack, // id to team-attack chat array
  TeamKill, // id to team-kill chat array
  Hello, // id to welcome chat array
  NoKeyword, // id to no keyword chat array
  Count // number for array
};

// links keywords and replies together
struct ChatKeywords {
  ystl::Array<ystl::String> keywords {};
  ystl::Array<ystl::String> replies {};

  ystl::HashSet<int32_t> used_reply_hashes {};

public:
  ChatKeywords () = default;

  ChatKeywords (const ystl::Array<ystl::String> &keywords, const ystl::Array<ystl::String> &replies) {
    this->keywords.clear ();
    this->replies.clear ();
    this->used_reply_hashes.clear ();

    this->keywords.insert (0, keywords);
    this->replies.insert (0, replies);
  }

  // check if a reply has been used
  bool IsReplyUsed (ystl::StringRef reply) const {
    return used_reply_hashes.exists (reply.hash ());
  }

  // mark a reply as used
  void MarkReplyUsed (ystl::StringRef reply) {
    used_reply_hashes.insert (reply.hash ());
  }

  // clear used replies tracking
  void ClearUsedReplies () {
    used_reply_hashes.clear ();
  }

  // get count of used replies
  size_t UsedReplyCount () const {
    return used_reply_hashes.size ();
  }
};

// define chatting collection structure
struct ChatCollection {
  int chat_probability {};
  float chat_delay {};
  float time_next_chat {};
  int entity_index {};
  ystl::String say_text {};
  ystl::Array<ystl::StringRef> last_used_sentences {};
};

// bot's chat manager
class ChatManager : public ystl::Singleton<ChatManager> {
private:
  ystl::SmallArray<ystl::Twin<ystl::String, ystl::String>> clan_tags_ {}; // strippable clan tags
  ystl::String upper_case_line_ {}; // reused buffer for upper-cased keyword matching
  ystl::String chat_scratch_ {}; // reused buffer for building outgoing chat messages

public:
  ChatManager ();
  ~ChatManager () = default;

public:
  // reused buffer for building outgoing chat messages
  ystl::String &ChatScratch () {
    return chat_scratch_;
  }

  // chat helper to strip the clantags out of the string
  void StripTags (ystl::String &line);

  // chat helper to make player name more human-like
  void HumanizePlayerName (ystl::String &player_name);

  // chat helper to add errors to the bot chat string
  void AddChatErrors (ystl::String &line);

  // chat helper to find keywords for given string
  bool CheckKeywords (ystl::StringRef line, ystl::String &reply, bool allow_generic = true);

  // same as checkKeywords, but matches against an upper-cased copy of the line kept
  // in a reused buffer, so case-insensitive matching doesn't allocate per message
  bool CheckKeywordsUpper (ystl::StringRef line, ystl::String &reply, bool allow_generic = true) {
    ystl::utf8tools.str_to_upper (line, upper_case_line_);

    return CheckKeywords (upper_case_line_, reply, allow_generic);
  }
};

// expose global
YSTL_EXPOSE_GLOBAL_SINGLETON (ChatManager, chatlib);

// bot chat data mixin
class ChatData {
  friend class DebugPanel;
  friend struct TestHook;
  friend struct ChatHook;

protected:
  ystl::IntervalTimer last_chat_timer_ {}; // time bot last chatted
  bool need_to_send_welcome_chat_ {}; // bot needs to greet people on server?
  ystl::String chat_buffer_ {}; // space for strings (say text...)
  ystl::String reply_buffer_ {}; // reused buffer for chat keyword replies, so reply picking doesn't allocate

public:
  ChatCollection say_text_buffer_ {}; // holds the index & the actual message of the last unprocessed text message of a player
};

} // namespace bot
