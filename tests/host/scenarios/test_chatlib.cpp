//
// YaPB test host: unit/chatlib_{text,bot}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for chatlib.cpp: tag stripping, name humanizing,
// chat-error bounds, keyword matching with reply cycling, and the
// Bot-side message preparation/reply/chat paths. Privates go through
// BotChatHook (friend, tests only); randomness is pinned via cv_language
// and single-reply factories, never asserted on.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

struct ChatHook {
  static ystl::String &ChatBuffer (Bot &bot) {
    return bot.chat_buffer_;
  }
  static ystl::Deque<Msg> &MsgQueue (Bot &bot) {
    return bot.msg_queue_;
  }
  static void SetCreature (Bot &bot, bool value) {
    bot.is_creature_ = value;
  }
  static void DrainQueue (Bot &bot) {
    bot.msg_queue_.clear ();
  }
  static bool CheckKeywords (Bot &bot, ystl::StringRef text, ystl::String &reply) {
    return bot.CheckChatKeywords (text, reply);
  }
  static bool Replying (Bot &bot) {
    return bot.IsReplyingToChat ();
  }
};

namespace {

// four-node chain, dense numbering matches indices
void BuildChatGraph () {
  graph.Reset ();

  for (int i = 0; i < 4; ++i) {
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
  planner.Init ();
}

void BootChat (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildChatGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);

  // english typo generators roll an 8% lowercase dice: disable it so
  // short-message asserts stay exact (long ones only assert bounds)
  cv_language.Set ("xx");
}

// single-reply factory: matching outcome is exact, rolls can't matter
void InjectKeyword (const char *keyword, const char *reply) {
  ChatKeywords factory {};
  factory.keywords.push (keyword);
  factory.replies.push (reply);

  conf.GetReplies ().push (ystl::move (factory));
}

} // namespace

TEST_CASE ("unit/chatlib_text") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootChat (engine, cs);

  // clan tags strip pairwise, nothing else moves
  ystl::String tagged = "[TAG]Name";
  chatlib.StripTags (tagged);
  CHECK (tagged == "Name");

  ystl::String braces = "{-x-}test";
  chatlib.StripTags (braces);
  CHECK (braces == "test");

  ystl::String plain = "Plain";
  chatlib.StripTags (plain);
  CHECK (plain == "Plain");

  ystl::String empty {};
  chatlib.StripTags (empty);
  CHECK (empty.empty ());

  // oversized tag content and degenerate pairs survive
  ystl::String big = "[0123456789012345678901234567890123]x";
  chatlib.StripTags (big);
  CHECK (big == "[0123456789012345678901234567890123]x");

  ystl::String degenerate = "[]x";
  chatlib.StripTags (degenerate);
  CHECK (degenerate == "[]x");

  // humanizing never grows the name and tolerates empties
  ystl::String name = "[TAG]Bob";
  chatlib.HumanizePlayerName (name);
  CHECK (!name.empty ());
  CHECK (name.size () <= 8);

  ystl::String no_name {};
  chatlib.HumanizePlayerName (no_name);
  CHECK (no_name.empty ());

  // chat errors: empty stays empty, long lines shrink by at most one
  ystl::String no_err {};
  chatlib.AddChatErrors (no_err);
  CHECK (no_err.empty ());

  ystl::String long_line = "0123456789ABCDEF";
  chatlib.AddChatErrors (long_line);
  CHECK (long_line.size () == 15 || long_line.size () == 16);

  ystl::String short_line = "hello";
  chatlib.AddChatErrors (short_line);
  CHECK (short_line == "hello"); // short + non-english: byte-identical

  // keyword guards: chat off, empty input, unknown without generic
  cv_chat.Set (0);
  ystl::String reply {};
  CHECK (!chatlib.CheckKeywords ("XYZZYQ", reply, false));
  cv_chat.Set (1);
  CHECK (!chatlib.CheckKeywords ("", reply, false));
  CHECK (!chatlib.CheckKeywords ("qqqx wwwww zzzzz", reply, false));
  CHECK (reply.empty ());

  // single-reply factory: exact text, punctuation-tolerant upper path
  InjectKeyword ("XYZZYQ", "Hi there");
  CHECK (chatlib.CheckKeywords ("say XYZZYQ now", reply, false));
  CHECK (reply == "Hi there");
  CHECK (chatlib.CheckKeywordsUpper ("say xyzzYQ now", reply, false));
  CHECK (reply == "Hi there");

  // reply cycle resets instead of starving: always an answer
  for (int i = 0; i < 4; ++i) {
    CHECK (chatlib.CheckKeywords ("XYZZYQ", reply, false));
    CHECK (reply == "Hi there");
  }

  // factory without replies matches nothing and falls through cleanly
  ChatKeywords mute {};
  mute.keywords.push ("QQMUTE");
  conf.GetReplies ().push (ystl::move (mute));
  CHECK (!chatlib.CheckKeywords ("QQMUTE talk", reply, false));

  bots.Destroy ();
}

TEST_CASE ("unit/chatlib_bot") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootChat (engine, cs);
  InjectKeyword ("XYZZYQ", "Hi there");

  bots.Addbot ("ChatB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("ChatC", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("ChatD", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));

  Bot *b = testhost::FindBot ("ChatB");
  Bot *c = testhost::FindBot ("ChatC");
  Bot *d = testhost::FindBot ("ChatD");
  HOST_REQUIRE (b != nullptr && c != nullptr && d != nullptr);

  edict_t *human = game.CreateFakeClient ("Human1");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.frags = 10.0f;
  clients.Update ();

  clients[human].team = Team::Terrorist;
  clients[human].team2 = Team::Terrorist;
  b->team_ = Team::Terrorist;

  // chat off freezes the buffer
  ChatHook::ChatBuffer (*b) = "SENTINEL";
  cv_chat.Set (0);
  b->PrepareChatMessage ("%m");
  CHECK (ChatHook::ChatBuffer (*b) == "SENTINEL");
  cv_chat.Set (1);

  // map marker resolves through the engine
  b->PrepareChatMessage ("%m");
  CHECK (ChatHook::ChatBuffer (*b) == game.GetMapName ());

  // round clock renders zero-padded minutes and seconds
  b->PrepareChatMessage ("%r");
  const ystl::String &clock = ChatHook::ChatBuffer (*b);
  CHECK (clock.size () == 5);
  CHECK (clock[2] == ':');

  // highest fragger by name, deterministic on tagless names
  b->PrepareChatMessage ("%f");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");

  // speaker fallback and explicit speaker agree
  b->say_text_buffer_.entity_index = -1;
  b->PrepareChatMessage ("%s");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");
  b->say_text_buffer_.entity_index = game.IndexOfPlayer (human);
  b->PrepareChatMessage ("%s");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");
  b->say_text_buffer_.entity_index = -1;

  // missing victim renders as unknown instead of crashing (regression:
  // indexOfPlayer(null) used to walk a wild pointer into ent->free)
  b->PrepareChatMessage ("%v");
  CHECK (ChatHook::ChatBuffer (*b) == "unknown");

  // last victim resolves by name once it exists
  b->is_alive_ = true;
  bots.HandleDeath (b->Ent (), human);
  b->PrepareChatMessage ("%v");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");

  // teammate and enemy resolve from the client table
  b->PrepareChatMessage ("%t");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");
  b->PrepareChatMessage ("%e");
  CHECK (ChatHook::ChatBuffer (*b) == "Human1");

  // empty graph has no author, unknown markers pass through
  b->PrepareChatMessage ("%g");
  CHECK (ChatHook::ChatBuffer (*b).empty ());
  b->PrepareChatMessage ("a%xb");
  CHECK (ChatHook::ChatBuffer (*b) == "a%xb");

  // trailing marker and the six-marker cap stay literal (kept short so
  // the typo generator, active past 15 chars, cannot roll)
  b->PrepareChatMessage ("abc%");
  CHECK (ChatHook::ChatBuffer (*b) == "abc%");
  b->PrepareChatMessage ("%x%x%x%x%x%x%x");
  CHECK (ChatHook::ChatBuffer (*b) == "%x%x%x%x%x%x%x");

  // markerless text flows through untouched
  b->PrepareChatMessage ("hello");
  CHECK (ChatHook::ChatBuffer (*b) == "hello");

  // keyword wrapper routes through the upper-cased matcher
  ystl::String bot_reply {};
  CHECK (ChatHook::CheckKeywords (*b, "talk XYZZYQ now", bot_reply));
  CHECK (bot_reply == "Hi there");
  CHECK (!ChatHook::CheckKeywords (*b, "qqqx wwwww", bot_reply));

  // no sender, no reply
  CHECK (!ChatHook::Replying (*b));

  // empty sender text resets and bails
  b->say_text_buffer_.entity_index = game.IndexOfPlayer (human);
  b->say_text_buffer_.say_text.clear ();
  CHECK (!ChatHook::Replying (*b));
  CHECK (b->say_text_buffer_.entity_index == -1);

  // live sender text with a keyword always replies (chance over 100%)
  b->say_text_buffer_.entity_index = game.IndexOfPlayer (human);
  b->say_text_buffer_.say_text = "talk XYZZYQ now";
  b->say_text_buffer_.time_next_chat = 0.0f;
  b->say_text_buffer_.chat_probability = 50;
  b->say_text_buffer_.chat_delay = 5.0f;
  ChatHook::DrainQueue (*b);
  CHECK (ChatHook::Replying (*b));
  CHECK (ChatHook::ChatBuffer (*b) == "Hi there");
  CHECK (b->say_text_buffer_.entity_index == -1);
  CHECK (!ChatHook::MsgQueue (*b).empty ());

  // chat off blocks queueing entirely
  ChatHook::DrainQueue (*b);
  cv_chat.Set (0);
  b->PushChatMessage (Chat::Kill, false);
  CHECK (ChatHook::MsgQueue (*b).empty ());
  cv_chat.Set (1);

  // dead bots stay silent while alive
  b->is_alive_ = true;
  ChatHook::DrainQueue (*b);
  b->CheckForChat ();
  CHECK (ChatHook::MsgQueue (*b).empty ());

  // send guards: creatures, empties and muted chat never emit
  const size_t cmds = engine.ClientCommands ().size ();
  ChatHook::SetCreature (*b, true);
  b->SendToChat ("hi", false);
  ChatHook::SetCreature (*b, false);
  b->SendToChat ("", false);
  cv_chat.Set (0);
  b->SendToChat ("hi", false);
  cv_chat.Set (1);
  CHECK (engine.ClientCommands ().size () == cmds);

  // legacy broadcast reaches teammates and strangers, not enemies
  clients.Update ();
  clients[c->Ent ()].team2 = Team::Terrorist;
  clients[d->Ent ()].team2 = Team::CT;
  b->is_alive_ = true;
  c->say_text_buffer_.entity_index = -1;
  d->say_text_buffer_.entity_index = -1;
  b->SendToChat ("hello team", true);
  CHECK (c->say_text_buffer_.entity_index == b->index_);
  CHECK (ystl::StringRef (c->say_text_buffer_.say_text.chars ()) == "hello team");
  CHECK (d->say_text_buffer_.entity_index == -1);
  CHECK (ystl::StringRef (chatlib.ChatScratch ().chars ()).find ("hello team") != ystl::String::InvalidIndex);

  bots.Destroy ();
}

} // namespace bot
