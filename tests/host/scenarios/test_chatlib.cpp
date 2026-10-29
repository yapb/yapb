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
  static bool ProcessPending (Bot &bot) {
    return bot.ProcessPendingChat ();
  }
  static void CheckQueue (Bot &bot) {
    bot.CheckMsgQueue ();
  }
  static ystl::RWrand &Rng (Bot &bot) {
    return bot.rg;
  }
};

struct ChatProbe {
  static int pre_count;
  static int post_count;
  static bool block;
  static BotChatSource source;
  static bool team_only;
  static ystl::String message;

  static void Reset () {
    pre_count = post_count = 0;
    block = false;
    source = BotChatSource::Bot;
    team_only = false;
    message.clear ();
  }

  static int OnEvent (const BotEvent &event) {
    const bool post = event.type == BotEventType::ChatPost;

    if (post) {
      ++post_count;
    }
    else {
      ++pre_count;
    }
    source = event.source;
    team_only = event.team_only;
    message = event.message ? event.message : "";

    return !post && block ? 1 : 0;
  }
};

int ChatProbe::pre_count = 0;
int ChatProbe::post_count = 0;
bool ChatProbe::block = false;
BotChatSource ChatProbe::source = BotChatSource::Bot;
bool ChatProbe::team_only = false;
ystl::String ChatProbe::message {};

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

  bots.Addbot ("ChatB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("ChatC", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("ChatD", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
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

TEST_CASE ("unit/chat_api") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootChat (engine, cs);

  bots.Addbot ("ChatApi", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("ChatApi");
  HOST_REQUIRE (bot != nullptr);

  edict_t *human = game.CreateFakeClient ("ChatApiHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[human].team = Team::Terrorist;
  clients[human].team2 = Team::Terrorist;
  clients.Update ();

  cv_chat.Set (1);
  ChatProbe::Reset ();
  bots.SetEventSink (&ChatProbe::OnEvent);

  // auto chat passes through pre and post once
  CHECK (bot->SetChatMode (BotChatMode::Auto));
  auto commands = engine.ClientCommands ().size ();

  bot->SendToChat ("hello", false);
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 1);
  CHECK (ChatProbe::source == BotChatSource::Bot);
  CHECK (ChatProbe::message == "hello");
  CHECK (!ChatProbe::team_only);

  // a blocked pre suppresses the send and the post event
  ChatProbe::Reset ();
  ChatProbe::block = true;
  commands = engine.ClientCommands ().size ();

  bot->SendToChat ("blocked", false);
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 0);
  CHECK (engine.ClientCommands ().size () == commands);

  // the same guard covers core chat queued by the ai
  ChatProbe::Reset ();
  ChatProbe::block = true;
  ChatHook::ChatBuffer (*bot) = "blocked queue";
  ChatHook::DrainQueue (*bot);
  CHECK (!bot->PushMsgQueue (Msg::Say));
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatHook::MsgQueue (*bot).empty ());

  // a queued say is pre-checked once, then sent without a second pre callback
  ChatProbe::Reset ();
  ChatProbe::block = false;
  ChatHook::DrainQueue (*bot);
  ChatHook::ChatBuffer (*bot) = "queued once";
  CHECK (bot->PushMsgQueue (Msg::Say));
  CHECK (ChatProbe::pre_count == 1);

  ChatHook::CheckQueue (*bot);
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 1);
  CHECK (ChatProbe::message == "queued once");
  CHECK (ChatHook::MsgQueue (*bot).empty ());

  // manual mode silences the ai but lets the plugin send
  ChatProbe::Reset ();
  ChatProbe::block = false;
  CHECK (bot->SetChatMode (BotChatMode::Manual));
  commands = engine.ClientCommands ().size ();

  bot->SendToChat ("auto", false);
  CHECK (ChatProbe::pre_count == 0);
  CHECK (engine.ClientCommands ().size () == commands);

  CHECK (bot->QueueChat ("plugin", false, true));
  CHECK (ChatHook::ProcessPending (*bot));
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 1);
  CHECK (ChatProbe::source == BotChatSource::Plugin);
  CHECK (ChatProbe::message == "plugin");

  // force bypasses cv_chat and sanitizes a one-line say argument
  ChatProbe::Reset ();
  cv_chat.Set (0);
  CHECK (bot->QueueChat ("a\n\"b\"\r", true, true));
  CHECK (ChatHook::ProcessPending (*bot));
  CHECK (ChatProbe::message == "ab");
  CHECK (ChatProbe::team_only);
  CHECK (ChatProbe::source == BotChatSource::Plugin);
  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 1);
  cv_chat.Set (1);

  // off drops both queued and direct messages
  ChatProbe::Reset ();
  CHECK (bot->SetChatMode (BotChatMode::Off));
  commands = engine.ClientCommands ().size ();

  CHECK (!bot->QueueChat ("off", false, true));
  bot->SendToChat ("off", false);
  CHECK (ChatProbe::pre_count == 0);
  CHECK (ChatProbe::post_count == 0);
  CHECK (engine.ClientCommands ().size () == commands);
  CHECK (!bot->SetChatMode (BotChatMode::Invalid));
  CHECK (bot->GetChatMode () == BotChatMode::Off);

  // human say emits a single human event before bot buffers are updated
  CHECK (bot->SetChatMode (BotChatMode::Auto));
  ChatProbe::Reset ();
  bot->is_alive_ = true;
  bot->team_ = Team::Terrorist;
  bot->pev->health = 100.0f;
  bot->pev->deadflag = DEAD_NO;
  clients[bot->Ent ()].team = Team::Terrorist;
  clients[bot->Ent ()].team2 = Team::Terrorist;
  clients.Update ();
  bot->say_text_buffer_.entity_index = -1;
  bot->say_text_buffer_.say_text.clear ();

  ystl::Array<ystl::String> args {};
  args.push ("hello");
  engine.SetCmdArgs (args);
  bots.CaptureChatRadio ("say", "", human);

  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 1);
  CHECK (ChatProbe::source == BotChatSource::Human);
  CHECK (ChatProbe::message == "hello");
  CHECK (!ChatProbe::team_only);
  CHECK (bot->say_text_buffer_.say_text == "hello");

  // a human pre block stops the buffer update and the post event
  ChatProbe::Reset ();
  ChatProbe::block = true;
  bot->say_text_buffer_.entity_index = -1;
  bot->say_text_buffer_.say_text.clear ();
  bots.CaptureChatRadio ("say", "", human);

  CHECK (ChatProbe::pre_count == 1);
  CHECK (ChatProbe::post_count == 0);
  CHECK (bot->say_text_buffer_.entity_index == -1);
  CHECK (bot->say_text_buffer_.say_text.empty ());

  ChatProbe::block = false;
  bots.SetEventSink (nullptr);
  bots.Destroy ();
}
TEST_CASE ("unit/chatlib_banks") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootChat (engine, cs);

  // english unlocks the typo dice and the chs/cht branches stay out
  cv_language.Set ("en");

  // pinned global rolls drive every percent leg deterministically
  ystl::rg.force_chance (true);

  ystl::String tagged = "[TAG]Bob";
  chatlib.HumanizePlayerName (tagged);
  CHECK (!tagged.empty ());

  ystl::String en_err = "0123456789ABCDEF";
  chatlib.AddChatErrors (en_err);

  ystl::rg.force_chance (false);

  ystl::String plain = "[TAG]Bob";
  chatlib.HumanizePlayerName (plain);
  CHECK (plain == "[TAG]Bob"); // trimming only, tags stay without the roll

  ystl::rg.clear_forced ();

  // chinese error legs drop and swap through the same dice
  cv_language.Set ("chs");

  ystl::String chs_err = "0123456789ABCDEF";
  ystl::rg.force_chance (true);
  chatlib.AddChatErrors (chs_err);
  ystl::rg.clear_forced ();

  // the loader reads the current language file, so switch back to english
  cv_language.Set ("en");

  // chat banks plus the keyword index come from one language file
  const ystl::String chat_file =
    ystl::strings.join_path (bstor.GetRunningPathVfs (), folders.config, folders.lang, ystl::String ("en_chat.cfg"));
  ystl::File::make_path (ystl::String (chat_file.substr (0, chat_file.find_last_of (kPathSeparator))).chars ());

  ystl::String chat_text = "raw Killed {\n  kill one\n}\nraw DeadChat {\n  dead one\n}\nraw Unknown {\n  unknown one\n}\n";
  chat_text += "Replies {\n  HelloGroup {\n    key = hello\n    raw {\n      hi there\n    }\n  }\n}\n";
  chat_text += "GameNames {\n  CS = cs_one\n}\n";

  {
    ystl::File file (chat_file, "wb");
    HOST_REQUIRE (!!file);
    file.write (chat_text.chars (), chat_text.size (), 1);
    file.close ();
  }
  conf.LoadChatConfig ();
  cv_chat.Set (1); // loads never enable chat, only disable it on failure
  CHECK (!conf.GetReplies ().empty ());
  CHECK (!conf.GetKeywordIndex ().empty ());
  HOST_REQUIRE (conf.HasChatBank (Chat::Kill));
  HOST_REQUIRE (conf.HasChatBank (Chat::Dead));
  HOST_REQUIRE (conf.HasChatBank (Chat::NoKeyword));

  // indexed lookups resolve through the word loop, punctuation and all
  // (index keys are upper-cased at parse, so query them upper-cased)
  ystl::String reply {};
  CHECK (chatlib.CheckKeywords ("say HELLO now", reply, false));
  CHECK (reply == "hi there");
  CHECK (chatlib.CheckKeywords ("...HELLO!!!", reply, false));
  CHECK (reply == "hi there");

  // dangling indices skip past the reply table
  conf.GetKeywordIndex ()[ystl::detail::fnv1a32_n ("ZZZBAD", 6)].push (9999);
  CHECK (!chatlib.CheckKeywords ("ZZZBAD talk", reply, false));

  // strangers fall back to the universal bank on a pinned roll
  ystl::rg.force_chance (true);
  CHECK (ystl::rg.chance (70));
  CHECK (chatlib.CheckKeywords ("qqqx wwwww", reply, true));
  CHECK (reply == "unknown one");
  ystl::rg.force_chance (false);
  CHECK (!chatlib.CheckKeywords ("qqqx wwwww", reply, true));
  ystl::rg.clear_forced ();

  // back to a typo-free language: the 8% english dice would flake
  // every exact buffer assert below (dice legs are covered above)
  cv_language.Set ("xx");

  bots.Addbot ("BankB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = testhost::FindBot ("BankB");
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::Terrorist;
  clients.Update ();
  clients[bot->Ent ()].team = Team::Terrorist;

  edict_t *foe = game.CreateFakeClient ("BankFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));
  foe->v.flags &= ~FL_FAKECLIENT;
  foe->v.health = 100.0f;
  foe->v.deadflag = DEAD_NO;
  foe->v.origin = ystl::Vector (300.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[foe].team = Team::CT;

  edict_t *human = game.CreateFakeClient ("BankHuman");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.flags &= ~FL_FAKECLIENT;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.origin = ystl::Vector (200.0f, 0.0f, 0.0f);
  clients.Update ();
  clients[human].team = Team::Terrorist;

  // game names ride the %d marker (needs a modern-flagged game)
  game.AddGameFlag (GameFlags::Modern);
  bot->PrepareChatMessage ("%d");
  CHECK (ChatHook::ChatBuffer (*bot) == "cs_one");
  game.ClearGameFlag (GameFlags::Modern);

  // enemies resolve off the client table
  bot->PrepareChatMessage ("%e");
  CHECK (ChatHook::ChatBuffer (*bot) == "BankFoe");

  // team damage routes through the inflictor
  bot->pev->dmg_inflictor = human;
  bot->PrepareChatMessage ("%t");
  CHECK (ChatHook::ChatBuffer (*bot) == "BankHuman");
  bot->pev->dmg_inflictor = nullptr;

  // senders that are bots read straight from their buffers
  bot->say_text_buffer_.entity_index = bot->index_;
  bot->say_text_buffer_.chat_probability = 50;
  ChatHook::ChatBuffer (*bot) = "talk hello now";
  ystl::rg.force_chance (true);
  CHECK (ChatHook::Replying (*bot));
  CHECK (ChatHook::ChatBuffer (*bot) == "hi there");
  CHECK (!ChatHook::MsgQueue (*bot).empty ());
  ystl::rg.clear_forced ();

  // banked replies queue through the message pump
  bot->say_text_buffer_.entity_index = game.IndexOfPlayer (human);
  bot->say_text_buffer_.say_text = "talk hello now";
  bot->say_text_buffer_.time_next_chat = 0.0f;
  bot->say_text_buffer_.chat_probability = 50;
  bot->say_text_buffer_.chat_delay = 5.0f;
  ChatHook::DrainQueue (*bot);
  CHECK (ChatHook::Replying (*bot));

  // dry replies with nowhere to go still reset the sender
  // (negative probability fails every roll without pinning)
  bot->say_text_buffer_.entity_index = game.IndexOfPlayer (human);
  bot->say_text_buffer_.say_text = "talk hello now";
  bot->say_text_buffer_.time_next_chat = 0.0f;
  bot->say_text_buffer_.chat_probability = -100;
  ChatHook::Rng (*bot).force_chance (false);
  ChatHook::DrainQueue (*bot);
  CHECK (!ChatHook::Replying (*bot));
  CHECK (bot->say_text_buffer_.entity_index == -1);
  ChatHook::Rng (*bot).clear_forced ();
  bot->say_text_buffer_.say_text.clear ();

  // banked pushes land in the queue
  ChatHook::DrainQueue (*bot);
  bot->PushChatMessage (Chat::Kill, false);
  CHECK (!ChatHook::MsgQueue (*bot).empty ());

  // dead bots work the dead bank with ignore-list memory
  // (NewRound starts the chat throttle, so age past its 6-10s gate)
  bot->is_alive_ = false;
  bot->pev->health = 0.0f;
  ChatHook::DrainQueue (*bot);
  engine.AdvanceTime (11.0f);
  ChatHook::Rng (*bot).force_chance (true);
  bot->CheckForChat ();
  ChatHook::Rng (*bot).clear_forced ();
  CHECK (!ChatHook::MsgQueue (*bot).empty ());

  for (int i = 0; i < 5; ++i) {
    bot->say_text_buffer_.last_used_sentences.push ("dead one");
  }
  engine.AdvanceTime (11.0f);
  ChatHook::Rng (*bot).force_chance (true);
  ChatHook::Rng (*bot).force_int (4);
  bot->CheckForChat ();
  ChatHook::Rng (*bot).clear_forced ();
  CHECK (bot->say_text_buffer_.last_used_sentences.empty ());

  cv_language.Set ("xx");
  ystl::plat.remove_file (chat_file.chars ());

  bots.Destroy ();
}

} // namespace bot
