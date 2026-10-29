//
// YaPB test host: unit/manager_{create,kicks,teams,maintain,leaders_as,leaders_de}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for manager.cpp through the public BotManager API only
// (no production-code changes): creation queue, lookups, kicks/quota,
// team counts, economics, difficulties, weapon modes, chat/radio capture,
// death handling and a frame() smoke test.
//
// Quota discipline: cv_quota stays above the live bot count, so the
// automatic balancer never kicks mid-setup (spawn counts are zero in the
// harness, so it can never auto-add either).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

// team-relative class slots share numeric values (`Terror`==`Urban`, ...)
static_assert (ystl::to_underlying (Skin::Random) == 0);
static_assert (ystl::to_underlying (Skin::Terror) == ystl::to_underlying (Skin::Urban));
static_assert (ystl::to_underlying (Skin::Guerilla) == ystl::to_underlying (Skin::Gign));
static_assert (ystl::to_underlying (Skin::Militia) == 5 && ystl::to_underlying (Skin::Spetsnaz) == 5);

struct ManagerHook {
  static bool Leader (Bot &bot) {
    return bot.is_leader_;
  }
  static void ExpireThink (Bot &bot) {
    bot.think_timer_.invalidate ();
    bot.command_timer_.invalidate ();
    bot.slow_frame_timer_.invalidate ();
  }
  static void SetSpeeds (Bot &bot, float fwd, float side) {
    bot.move_speed_ = fwd;
    bot.strafe_speed_ = side;
  }
  static void SetPending (Bot &bot, int buttons) {
    bot.pending_buttons_ = buttons;
  }
  static void SetLastCommand (Bot &bot, float msec, float time) {
    bot.last_command_msec_ = msec;
    bot.last_command_time_ = time;
  }
  static int Starved (Bot &bot) {
    return bot.starved_commands_;
  }
  static void SetThinkInterval (Bot &bot, float interval) {
    bot.think_interval_ = interval;
  }
  static void TeamJoin (Bot &bot) {
    bot.UpdateTeamJoin ();
  }
};

namespace {

// minimal chain 0 - 1 - 2 - 3 with an island, enough for BotManager::create
void BuildMgrGraph () {
  graph.Reset ();

  const ystl::Vector spots[5] = {
    ystl::Vector (0.0f, 0.0f, 0.0f),
    ystl::Vector (100.0f, 0.0f, 0.0f),
    ystl::Vector (200.0f, 0.0f, 0.0f),
    ystl::Vector (300.0f, 0.0f, 0.0f),
    ystl::Vector (2000.0f, 0.0f, 0.0f),
  };

  for (int i = 0; i < 5; ++i) {
    Path path {};
    path.origin = spots[i];
    path.number = i;
    path.light = kInvalidLightLevel;

    for (auto &link : path.links) {
      link.index = kInvalidNodeIndex;
    }
    graph.paths_.push (path);
  }

  auto link = [] (int from, int to, int distance) {
    for (auto &slot : graph.paths_[static_cast<size_t> (from)].links) {
      if (slot.index == kInvalidNodeIndex) {
        slot.index = static_cast<int16_t> (to);
        slot.distance = distance;
        return;
      }
    }
    HOST_REQUIRE (false);
  };

  link (0, 1, 100);
  link (1, 0, 100);
  link (1, 2, 100);
  link (2, 1, 100);
  link (2, 3, 100);
  link (3, 2, 100);

  graph.PopulateNodes ();
  planner.Init ();
}

// raw client without a Bot wrapper: clear the fake bit and it counts as human
edict_t *CreateHuman (ystl::StringRef name) {
  edict_t *ent = game.CreateFakeClient (name);

  if (!game.IsNullEntity (ent)) {
    ent->v.flags &= ~FL_FAKECLIENT;
  }
  return ent;
}

void BootManagerMap (testhost::FakeEngine &engine, testhost::FakeCSApi &cs, const char *map_entity) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  game.Precache ();

  // map entity before activation: levelInitialize picks the map flags up
  HOST_REQUIRE (engine.SpawnEntity (map_entity) != nullptr);
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  if (analyzer.IsAnalyzing ()) {
    analyzer.Suspend ();
  }
  BuildMgrGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

void BootManager (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
  BuildMgrGraph ();

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10); // keep the auto-balancer inert during setup
}

} // namespace

TEST_CASE ("unit/manager_create") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);

  // no graph, no bots: the request stays queued, quota is zeroed
  graph.Reset ();
  bots.Addbot ("NoGraph", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  engine.AdvanceTime (6.0f);
  bots.MaintainQuota ();

  CHECK (bots.GetBotCount () == 0);
  CHECK (!bots.HasBotsOnline ());
  CHECK (cv_quota.As<int> () == 0);

  // the same queued request processes once the graph exists
  cv_quota.Set (10);
  BuildMgrGraph ();
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  CHECK (bots.HasBotsOnline ());

  Bot *a = testhost::FindBot ("NoGraph");
  HOST_REQUIRE (a != nullptr);

  // lookup paths agree with each other
  CHECK (bots.FindBotByIndex (a->Index ()) == a);
  CHECK (bots.FindBotByEntity (a->Ent ()) == a);
  CHECK (bots[a->Ent ()] == a);
  CHECK (bots[a->Index ()] == a);
  CHECK (bots.FindBotByIndex (-1) == nullptr);
  CHECK (bots.FindBotByEntity (nullptr) == nullptr);

  // nobody alive yet, then exactly the flagged one
  CHECK (bots.FindAliveBot () == nullptr);
  a->is_alive_ = true;
  CHECK (bots.FindAliveBot () == a);
  a->is_alive_ = false;

  // empty name falls back to generated names, still success
  bots.Addbot ("", Difficulty::Invalid, Personality::Invalid, Team::Invalid, Skin::Random, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  // string overload with wildcards
  bots.Addbot ("StrBot", "*", "*", "*", "*", true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));
  CHECK (testhost::FindBot ("StrBot") != nullptr);

  // forEach visits everyone, true stops early
  int visits = 0;
  bots.ForEach ([&visits] (Bot *) {
    ++visits;
    return false;
  });
  CHECK (visits == 3);

  visits = 0;
  bots.ForEach ([&visits] (Bot *) {
    ++visits;
    return true;
  });
  CHECK (visits == 1);

  // connection time: bots match by name prefix, strangers pass through
  CHECK (bots.GetConnectionTimes (a->pev->netname.str (), 999.0f) != 999.0f);
  CHECK (bots.GetConnectionTimes ("no_such_player_xyz", 999.0f) == 999.0f);

  // stacked teams refuse creation: create() probes isTeamStacked (team - 1),
  // so a CT request is refused while terrorists overflow the limit
  mp_limitteams.Set (1);

  edict_t *stack[4] = {};
  for (int i = 0; i < 4; ++i) {
    stack[i] = CreateHuman ("Stack");
    HOST_REQUIRE (!game.IsNullEntity (stack[i]));
  }
  clients.Update ();

  clients[stack[0]].team2 = Team::Terrorist;
  clients[stack[1]].team2 = Team::Terrorist;
  clients[stack[2]].team2 = Team::Terrorist;
  clients[stack[3]].team2 = Team::CT;
  bots.Addbot ("Stacked", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);

  engine.AdvanceTime (1.0f);
  for (int i = 0; i < 10; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
  }
  CHECK (bots.GetBotCount () == 3);
  CHECK (testhost::FindBot ("Stacked") == nullptr);
  mp_limitteams.Set (0);

  // destroy drops everything and clears the index
  const int idx = a->Index ();
  bots.Destroy ();
  CHECK (bots.GetBotCount () == 0);
  CHECK (!bots.HasBotsOnline ());
  CHECK (bots.FindBotByIndex (idx) == nullptr);
  CHECK (bots.FindAliveBot () == nullptr);
}

TEST_CASE ("unit/manager_kicks") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);

  bots.Addbot ("KickA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("KickB", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("KickC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));

  Bot *a = testhost::FindBot ("KickA");
  Bot *b = testhost::FindBot ("KickB");
  Bot *c = testhost::FindBot ("KickC");
  HOST_REQUIRE (a != nullptr && b != nullptr && c != nullptr);

  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::Terrorist;
  clients[c->Ent ()].team2 = Team::CT;

  cv_quota.Set (3);

  // dead bots are kicked first, quota drops
  b->is_alive_ = false;
  a->is_alive_ = true;
  a->pev->frags = 5.0f;
  c->is_alive_ = true;
  c->pev->frags = 10.0f;

  HOST_REQUIRE (bots.KickRandom (true));
  CHECK (b->is_stale_);
  CHECK (!a->is_stale_);
  CHECK (cv_quota.As<int> () == 2);

  // dead-first would reselect the stale bot, drop it before the frags round
  const int b_idx = b->Index ();
  bots.DisconnectBot (b);
  CHECK (bots.FindBotByIndex (b_idx) == nullptr);

  // then the lowest frags, quota untouched when not requested
  HOST_REQUIRE (bots.KickRandom (false));
  CHECK (a->is_stale_);
  CHECK (!c->is_stale_);
  CHECK (cv_quota.As<int> () == 2);

  // disconnect removes the object and frees the index
  const int a_idx = a->Index ();
  bots.DisconnectBot (a);
  CHECK (bots.GetBotCount () == 1);
  CHECK (bots.FindBotByIndex (a_idx) == nullptr);
  CHECK (bots.FindBotByIndex (b_idx) == nullptr);

  // kickBot by index, bad indices are no-ops
  const int c_idx = c->Index ();
  bots.KickBot (c_idx);
  CHECK (c->is_stale_);
  CHECK (cv_quota.As<int> () == 1);
  bots.KickBot (999);
  bots.KickBot (-1);
  CHECK (cv_quota.As<int> () == 1);
  bots.DisconnectBot (c);
  CHECK (bots.GetBotCount () == 0);

  // fresh pair for team-scoped kicks (manual adds increment the quota,
  // so pin it down only after pumping)
  bots.Addbot ("KickD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("KickE", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *d = testhost::FindBot ("KickD");
  Bot *e = testhost::FindBot ("KickE");
  HOST_REQUIRE (d != nullptr && e != nullptr);

  clients.Update ();
  clients[d->Ent ()].team2 = Team::Terrorist;
  clients[e->Ent ()].team2 = Team::CT;

  // single kick from the team hits only that team, quota decrements
  cv_quota.Set (2);
  bots.KickFromTeam (Team::Terrorist, false);
  CHECK (cv_quota.As<int> () == 1);
  CHECK (d->is_stale_);
  CHECK (!e->is_stale_);

  // killing dead bots is a safe no-op (early return, no killer entity)
  d->is_alive_ = false;
  e->is_alive_ = false;
  bots.KillAllBots (Team::Invalid, true);
  bots.KillAllBots (Team::CT, true);

  // killing a live bot exercises the killer-entity path
  e->is_alive_ = true;
  e->pev->health = 100.0f;
  e->Kill ();
  e->is_alive_ = false;

  // instant kickEveryone stales the rest and zeroes the quota
  bots.KickEveryone (true, true, true);
  CHECK (cv_quota.As<int> () == 0);
  bots.ForEach ([] (Bot *bot) {
    CHECK (bot->is_stale_);
    return false;
  });

  // ...and clears the pending queue
  bots.Addbot ("Queued", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.KickEveryone (true, false, true);
  for (int i = 0; i < 10; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
  }
  CHECK (bots.GetBotCount () == 2);
  CHECK (testhost::FindBot ("Queued") == nullptr);

  // quota arithmetic with clamps
  cv_quota.Set (5);
  bots.DecrementQuota (2);
  CHECK (cv_quota.As<int> () == 3);
  bots.DecrementQuota ();
  CHECK (cv_quota.As<int> () == 2);
  bots.DecrementQuota (99);
  CHECK (cv_quota.As<int> () == 0);
  bots.DecrementQuota (0);
  CHECK (cv_quota.As<int> () == 0);

  // initQuota drops queued requests
  bots.Addbot ("Dropped", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.InitQuota ();
  cv_quota.Set (10);
  engine.AdvanceTime (6.0f);
  for (int i = 0; i < 5; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
  }
  CHECK (bots.GetBotCount () == 2);
  CHECK (testhost::FindBot ("Dropped") == nullptr);

  bots.Destroy ();
  CHECK (bots.GetBotCount () == 0);
}

TEST_CASE ("unit/manager_teams") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);

  bots.Addbot ("TeamA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("TeamB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("TeamK", Difficulty::Hard, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("TeamV", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("TeamW", Difficulty::Hard, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 5));

  edict_t *h1 = CreateHuman ("Human1");
  edict_t *h2 = CreateHuman ("Human2");
  edict_t *h3 = CreateHuman ("Human3");
  HOST_REQUIRE (!game.IsNullEntity (h1) && !game.IsNullEntity (h2) && !game.IsNullEntity (h3));

  Bot *a = testhost::FindBot ("TeamA");
  Bot *b = testhost::FindBot ("TeamB");
  Bot *killer = testhost::FindBot ("TeamK");
  Bot *victim = testhost::FindBot ("TeamV");
  Bot *watcher = testhost::FindBot ("TeamW");
  HOST_REQUIRE (a != nullptr && b != nullptr && killer != nullptr && victim != nullptr && watcher != nullptr);

  // readable edicts: bots count as alive clients for census/chat paths
  for (Bot *bot : { a, b, killer, victim, watcher }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
  }
  clients.Update ();

  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;
  clients[killer->Ent ()].team2 = Team::Terrorist;
  clients[victim->Ent ()].team2 = Team::CT;
  clients[watcher->Ent ()].team2 = Team::CT;
  clients[h1].team2 = Team::Terrorist;
  clients[h2].team2 = Team::CT;
  clients[h3].team2 = Team::Spectator;

  h1->v.health = 100.0f; // fresh clients start zeroed, bring up liveness
  h3->v.health = 100.0f;
  h2->v.deadflag = DEAD_DEAD;
  h2->v.health = 0.0f;
  h3->v.frags = 2.0f;
  clients.Update ();

  // humans: spectators counted unless ignored; only the living stay alive
  CHECK (bots.GetHumansCount (false) == 3);
  CHECK (bots.GetHumansCount (true) == 2);
  CHECK (bots.GetAliveHumansCount () == 2);

  // team census covers bots and humans, spectators excluded:
  // T = TeamA, TeamK, Human1; CT = TeamB, TeamV, TeamW, Human2
  const auto &[ts, cts] = bots.CountTeamPlayers ();
  CHECK (ts == 3);
  CHECK (cts == 4);

  // kpd splits bot/human populations
  a->pev->frags = 6.0f;
  a->death_count_ = 2;
  b->pev->frags = 2.0f;
  b->death_count_ = 1;
  killer->pev->frags = 0.0f;
  killer->death_count_ = 0;
  victim->death_count_ = 0;
  watcher->death_count_ = 0;
  h1->v.frags = 3.0f;
  h2->v.frags = 1.0f;
  CHECK (bots.GetAverageTeamKpd (true) == 8.0f / 6.0f); // (6+2+0+0+0)/(2+1+1+1+1), deaths clamp to 1
  CHECK (bots.GetAverageTeamKpd (false) == 2.0f); // (3+1+2)/3 humans

  // highest frags respects team and aliveness
  a->is_alive_ = true;
  b->is_alive_ = true;
  CHECK (bots.FindHighestFragBot (Team::Terrorist) == a);
  CHECK (bots.FindHighestFragBot (Team::CT) == b);
  a->is_alive_ = false;
  CHECK (bots.FindHighestFragBot (Team::Terrorist) == nullptr);

  // stacking needs a limit and a real imbalance (base: 3 T vs 4 CT)
  mp_limitteams.Set (0);
  CHECK (!bots.IsTeamStacked (Team::Terrorist));
  mp_limitteams.Set (1);
  CHECK (!bots.IsTeamStacked (Team::Terrorist)); // 3+1 > 4+1 is false
  CHECK (bots.IsTeamStacked (Team::CT)); // 4+1 > 3+1, one more CT breaks the limit
  clients[h2].team2 = Team::Spectator; // 3 T vs 3 CT
  CHECK (!bots.IsTeamStacked (Team::Terrorist));
  CHECK (!bots.IsTeamStacked (Team::CT));
  clients[h2].team2 = Team::Terrorist; // 4 T vs 3 CT
  CHECK (bots.IsTeamStacked (Team::Terrorist));
  CHECK (!bots.IsTeamStacked (Team::Invalid));
  clients[h2].team2 = Team::CT;
  mp_limitteams.Set (0);

  // priority: humans top, carriers and tasked bots above the rest
  CHECK (bots.GetPlayerPriority (h1) == game.IndexOfEntity (h1) + 2048);
  a->has_c4_ = true;
  CHECK (bots.GetPlayerPriority (a->Ent ()) == a->Entindex () + 1024);
  a->has_c4_ = false;
  a->StartTask (TaskId::MoveTo, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  CHECK (a->GetTaskId () == TaskId::MoveTo);
  CHECK (bots.GetPlayerPriority (a->Ent ()) == a->Entindex () + 1024);
  a->is_vip_ = true;
  CHECK (bots.GetPlayerPriority (a->Ent ()) == a->Entindex () + 1024);
  a->is_vip_ = false;

  // economics: 80% poor teams save, winners and loners buy
  // fresh managers have no winner yet, so nobody gets the winner bonus
  CHECK (bots.GetLastWinner () == Team::Invalid);
  bots.SetLastWinner (Team::CT);
  a->team_ = Team::Terrorist;
  b->team_ = Team::Terrorist;
  a->money_amount_ = 16000;
  b->money_amount_ = 16000;
  bots.UpdateTeamEconomics (Team::Terrorist);
  CHECK (bots.GetTeamEconomics (Team::Terrorist));
  a->money_amount_ = 0; // a single poor player tanks a 2-player team: (2*80)/100 = 1 <= 1
  bots.UpdateTeamEconomics (Team::Terrorist);
  CHECK (!bots.GetTeamEconomics (Team::Terrorist));
  bots.SetLastWinner (Team::Terrorist);
  CHECK (bots.GetLastWinner () == Team::Terrorist);
  bots.UpdateTeamEconomics (Team::Terrorist);
  CHECK (bots.GetTeamEconomics (Team::Terrorist));
  bots.UpdateTeamEconomics (Team::Terrorist, true);
  CHECK (bots.GetTeamEconomics (Team::Terrorist));
  b->team_ = Team::CT;
  bots.UpdateTeamEconomics (Team::Terrorist);
  CHECK (bots.GetTeamEconomics (Team::Terrorist)); // single player always buys
  cv_economics_rounds.Set (0);
  bots.UpdateTeamEconomics (Team::Terrorist);
  CHECK (bots.GetTeamEconomics (Team::Terrorist));
  cv_economics_rounds.Set (1);

  // difficulties propagate unless pinned or automatic; these bots were
  // added manually, so unlocking is required to observe plain propagation
  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_max.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_auto.Set (0);
  cv_difficulty_lock.Set (0);
  cv_difficulty.Set (ystl::to_underlying (Difficulty::Expert));
  a->SetNewDifficulty (Difficulty::Noob);
  bots.UpdateBotDifficulties ();
  CHECK (a->difficulty_ == Difficulty::Expert);
  bots.UpdateBotDifficulties (); // second run is a no-op
  CHECK (a->difficulty_ == Difficulty::Expert);
  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Noob));
  cv_difficulty.Set (ystl::to_underlying (Difficulty::Noob));
  bots.UpdateBotDifficulties ();
  CHECK (a->difficulty_ == Difficulty::Expert);
  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_lock.Set (1);

  a->SetNewDifficulty (Difficulty::Noob);
  CHECK (a->difficulty_ == Difficulty::Noob);
  a->SetNewDifficulty (static_cast<Difficulty> (99));
  CHECK (a->difficulty_ == Difficulty::Hard);

  // weapon modes rewrite the tables, knife mode flips jasonmode
  bots.SetWeaponMode (1);
  CHECK (cv_jasonmode.As<int> () == 1);
  int non_none = 0;
  for (int i = 0; i < kNumWeapons; ++i) {
    if (conf.GetWeapons ()[i].team_standard != WeaponTeam::None) {
      ++non_none;
    }
  }
  CHECK (non_none == 0);
  bots.SetWeaponMode (2);
  non_none = 0;
  for (int i = 0; i < kNumWeapons; ++i) {
    if (conf.GetWeapons ()[i].team_standard != WeaponTeam::None) {
      ++non_none;
    }
  }
  CHECK (non_none == 4); // pistols only
  bots.SetWeaponMode (99); // out of range falls back to standard
  CHECK (cv_jasonmode.As<int> () == 0);
  non_none = 0;
  for (int i = 0; i < kNumWeapons; ++i) {
    if (conf.GetWeapons ()[i].team_standard != WeaponTeam::None) {
      ++non_none;
    }
  }
  CHECK (non_none > 10);

  // say capture routes the speaker index into every in-scope bot
  clients[h1].team = Team::Terrorist;
  a->team_ = Team::Terrorist;
  b->team_ = Team::CT;
  bots.CaptureChatRadio ("say", "", h1);
  CHECK (a->say_text_buffer_.entity_index == game.IndexOfPlayer (h1));
  CHECK (b->say_text_buffer_.entity_index == game.IndexOfPlayer (h1));

  // radio orders land on same-team bots with no pending order
  clients[h1].radio = RadioChat::CoverMe;
  bots.CaptureChatRadio ("menuselect", "3", h1);
  CHECK (a->radio_order_ == RadioChat::HoldThisPosition);
  CHECK (a->radio_entity_ == h1);
  CHECK (b->radio_order_ == RadioChat::InvalidSelect);
  CHECK (bots.GetLastRadioTimestamp (Team::Terrorist) == game.Time ());
  CHECK (clients[h1].radio == RadioChat::InvalidSelect);

  // chat/radio accessors and cooldowns
  bots.SetLastRadioTimestamp (Team::CT, 12.5f);
  CHECK (bots.GetLastRadioTimestamp (Team::CT) == 12.5f);
  bots.SetLastRadio (Team::CT, RadioChat::FollowMe);
  CHECK (bots.GetLastRadio (Team::CT) == RadioChat::FollowMe);
  CHECK (!bots.HasPlantedBombSearchCooldown ());
  bots.SetPlantedBombSearchCooldown (10.0f);
  CHECK (bots.HasPlantedBombSearchCooldown ());
  engine.AdvanceTime (11.0f);
  CHECK (!bots.HasPlantedBombSearchCooldown ());
  bots.MarkLastChatTime ();
  CHECK (bots.GetLastChatElapsedTime () >= 0.0f);
  bots.SetEnemySpotted (true);
  CHECK (bots.EnemySpotted ());
  CHECK (bots.HasBombSay (BombPlantedSay::ChatSay));
  bots.ClearBombSay (BombPlantedSay::ChatSay);
  CHECK (!bots.HasBombSay (BombPlantedSay::ChatSay));
  bots.Reset ();
  CHECK (bots.HasBombSay (BombPlantedSay::ChatSay));

  // death: killer remembers the victim, victim drops, watchers forget
  cv_radio_mode.Set (0);
  killer->team_ = Team::Terrorist;
  victim->team_ = Team::CT;
  watcher->team_ = Team::CT;
  killer->is_alive_ = true;
  victim->is_alive_ = true;
  watcher->is_alive_ = true;
  watcher->enemy_ = victim->Ent ();
  watcher->last_enemy_ = victim->Ent ();

  bots.HandleDeath (killer->Ent (), victim->Ent ());
  CHECK (killer->last_victim_ == victim->Ent ());
  CHECK (!victim->is_alive_);
  CHECK (watcher->enemy_ == nullptr);
  CHECK (watcher->last_enemy_ == nullptr);

  // human killer: victim still goes down
  victim->is_alive_ = true;
  bots.HandleDeath (h1, victim->Ent ());
  CHECK (!victim->is_alive_);

  // frame() smoke: think and command timers fire without crashing
  a->is_alive_ = false;
  b->is_alive_ = false;
  killer->is_alive_ = false;
  watcher->is_alive_ = false;
  engine.AdvanceTime (0.3f);
  bots.Frame ();

  bots.Destroy ();
  CHECK (bots.GetBotCount () == 0);
}

TEST_CASE ("unit/manager_maintain") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  bots.InitQuota ();

  bots.Addbot ("KeepA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("KeepB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *a = nullptr;
  Bot *b = nullptr;
  bots.ForEach ([&] (Bot *bot) {
    if (ystl::StringRef (bot->pev->netname.chars ()) == "KeepA") {
      a = bot;
    }
    else {
      b = bot;
    }
    return false;
  });
  HOST_REQUIRE (a != nullptr && b != nullptr);

  // idle quota below population kicks the excess without shrinking storage
  cv_quota.Set (0);
  engine.AdvanceTime (1.0f);
  bots.MaintainQuota ();
  CHECK (bots.GetBotCount () == 2);

  int stale = 0;
  bots.ForEach ([&stale] (Bot *bot) {
    if (bot->is_stale_) {
      ++stale;
    }
    return false;
  });
  CHECK (stale == 1);

  // server fill queues the room out (12 requests: one slot kept free)
  // and drops the team limits
  mp_limitteams.Set (1);
  mp_autoteambalance.Set (1);
  bots.ServerFill (CSTeam::CT);
  CHECK (mp_limitteams.As<int> () == 0);
  CHECK (mp_autoteambalance.As<int> () == 0);
  HOST_REQUIRE (testhost::PumpBots (engine, 14));
  CHECK (bots.GetBotCount () == 14);

  // unteamed fill leaves the limits alone
  mp_limitteams.Set (1);
  bots.ServerFill (CSTeam::Any, Personality::Normal, Difficulty::Normal, 0);
  CHECK (mp_limitteams.As<int> () == 1);
  mp_limitteams.Set (0);

  // leaders fall back to the highest fragger outside map modes...
  a->is_alive_ = true;
  a->pev->frags = 9.0f;
  b->is_alive_ = true;
  b->pev->frags = 2.0f;
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;

  bots.SelectLeaders (Team::Terrorist, true);
  bots.SelectLeaders (Team::Terrorist, false);
  CHECK (ManagerHook::Leader (*a));
  CHECK (!ManagerHook::Leader (*b));

  // ...and never elect twice
  a->pev->frags = 0.0f;
  b->pev->frags = 30.0f;
  bots.SelectLeaders (Team::Terrorist, false);
  CHECK (ManagerHook::Leader (*a));

  // worker without threads runs tasks inline
  CHECK (!worker.Available ());
  int tasks = 0;
  worker.Enqueue ([&tasks] () {
    ++tasks;
  });
  CHECK (tasks == 1);
  worker.Shutdown ();
  worker.Startup (0);
  CHECK (!worker.Available ());

  bots.Destroy ();
}

TEST_CASE ("unit/manager_leaders_as") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManagerMap (engine, cs, "info_vip_safetyzone");
  HOST_REQUIRE (game.MapIs (MapFlags::Assassination));

  bots.Addbot ("AsT", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("AsV", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *t = testhost::FindBot ("AsT");
  Bot *v = testhost::FindBot ("AsV");
  HOST_REQUIRE (t != nullptr && v != nullptr);

  t->is_alive_ = true;
  t->pev->frags = 7.0f;
  v->is_alive_ = true;
  v->is_vip_ = true;
  clients.Update ();
  clients[t->Ent ()].team2 = Team::Terrorist;
  clients[v->Ent ()].team2 = Team::CT;

  // vip bot leads counter-terrorists, fragger leads terrorists
  bots.SelectLeaders (Team::CT, false);
  CHECK (ManagerHook::Leader (*v));
  bots.SelectLeaders (Team::Terrorist, false);
  CHECK (ManagerHook::Leader (*t));

  // reset runs clean (leadership flags accumulate by design)
  bots.SelectLeaders (Team::CT, true);
  bots.SelectLeaders (Team::CT, false);
  CHECK (ManagerHook::Leader (*v));

  bots.Destroy ();
}

TEST_CASE ("unit/manager_leaders_de") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManagerMap (engine, cs, "func_bomb_target");
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));

  bots.Addbot ("DeT", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DeC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *t = testhost::FindBot ("DeT");
  Bot *c = testhost::FindBot ("DeC");
  HOST_REQUIRE (t != nullptr && c != nullptr);

  t->is_alive_ = true;
  t->has_c4_ = true;
  c->is_alive_ = true;
  c->pev->frags = 5.0f;
  clients.Update ();
  clients[t->Ent ()].team2 = Team::Terrorist;
  clients[c->Ent ()].team2 = Team::CT;

  // carrier leads terrorists, fragger leads counter-terrorists
  bots.SelectLeaders (Team::Terrorist, false);
  CHECK (ManagerHook::Leader (*t));
  bots.SelectLeaders (Team::CT, false);
  CHECK (ManagerHook::Leader (*c));

  bots.Destroy ();
}

// issue 681: a noclipping human keeps team and health, so autokill must not fire
TEST_CASE ("unit/manager_autokill") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);

  bots.Addbot ("NoclipA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("NoclipB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  edict_t *human = CreateHuman ("Noclipper");
  HOST_REQUIRE (!game.IsNullEntity (human));

  Bot *a = testhost::FindBot ("NoclipA");
  Bot *b = testhost::FindBot ("NoclipB");
  HOST_REQUIRE (a != nullptr && b != nullptr);

  for (Bot *bot : { a, b }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
    bot->is_alive_ = true; // pumped bots never ran Update, mark the live round state by hand
  }
  clients.Update ();

  clients[human].team2 = Team::Terrorist;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  clients.Update ();

  cv_autokill_delay.Set (5.0f);

  // living human on a team: still in the round
  CHECK (bots.GetAliveHumansCount () == 1);

  // console/cheat noclip keeps team and health: still in the round, no kill after the delay
  human->v.movetype = MOVETYPE_NOCLIP;
  clients.Update ();
  CHECK (bots.GetAliveHumansCount () == 1);

  const int touch_base = testhost::CsCalls (cs, "Touch");
  bots.MaintainAutoKill ();
  engine.AdvanceTime (6.0f);
  bots.MaintainAutoKill ();
  CHECK (testhost::CsCalls (cs, "Touch") == touch_base);
  CHECK (a->is_alive_ && b->is_alive_);

  // control: a truly dead human still arms the timer and the bots die after the delay
  human->v.movetype = MOVETYPE_WALK;
  human->v.deadflag = DEAD_DEAD;
  human->v.health = 0.0f;
  clients.Update ();
  CHECK (bots.GetAliveHumansCount () == 0);

  bots.MaintainAutoKill ();
  engine.AdvanceTime (6.0f);
  bots.MaintainAutoKill ();
  CHECK (testhost::CsCalls (cs, "Touch") > touch_base);

  bots.Destroy ();
}

TEST_CASE ("unit/manager_quota") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("QuotaA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  Bot *a = testhost::FindBot ("QuotaA");
  HOST_REQUIRE (a != nullptr);
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[a->Ent ()].team = Team::Terrorist;
  a->is_alive_ = true;
  a->pev->health = 100.0f;
  a->pev->deadflag = DEAD_NO;

  // listen server with no humans returns early before quota math
  edict_t *human = CreateHuman ("QuotaH");
  HOST_REQUIRE (!game.IsNullEntity (human));
  clients.Update ();
  clients[human].team2 = Team::Spectator;
  clients[human].team = Team::Spectator;
  human->v.health = 0.0f;
  human->v.deadflag = DEAD_DEAD;
  clients.Update ();
  engine.SetDedicatedServer (false);
  engine.AdvanceTime (6.0f);
  const int count_before = bots.GetBotCount ();
  bots.MaintainQuota ();
  CHECK (bots.GetBotCount () == count_before);
  engine.SetDedicatedServer (true);

  // bring a live human for fill/match/autovacate math
  clients[human].team2 = Team::Terrorist;
  clients[human].team = Team::Terrorist;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  clients.Update ();
  cv_quota.Set (10);
  engine.AdvanceTime (6.0f);

  // out-of-range quota clamps into server capacity
  cv_quota.Set (32);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  CHECK (cv_quota.As<int> () <= game.MaxClients ());
  cv_quota.Set (10);

  // fill mode subtracts humans, match mode scales by humans
  cv_quota_mode.Set ("fill");
  cv_quota.Set (5);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_quota_mode.Set ("match");
  cv_quota_match.Set (2);
  cv_quota.Set (3);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_quota_mode.Set ("normal");
  cv_quota_match.Set (0);

  // join-after-player with no players forces zero desired
  clients[human].team2 = Team::Spectator;
  clients.Update ();
  cv_join_after_player.Set (1);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_join_after_player.Set (0);
  clients[human].team2 = Team::Terrorist;
  clients.Update ();

  // autovacate paths with and without kick-after-connect
  cv_autovacate.Set (1);
  cv_autovacate_keep_slots.Set (1);
  cv_kick_after_player_connect.Set (1);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_kick_after_player_connect.Set (0);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_autovacate.Set (0);
  engine.AdvanceTime (0.5f);
  bots.MaintainQuota ();
  cv_autovacate.Set (1);

  // spawn entities unlock the auto-add path: desired above live queues a creation
  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
  edict_t *spawns[4] = {};
  for (int i = 0; i < 2; ++i) {
    spawns[i] = engine.SpawnEntity ("info_player_start");
    HOST_REQUIRE (spawns[i] != nullptr);
    table.pfnSpawn (spawns[i]);
    spawns[2 + i] = engine.SpawnEntity ("info_player_deathmatch");
    HOST_REQUIRE (spawns[2 + i] != nullptr);
    table.pfnSpawn (spawns[2 + i]);
  }
  CHECK (game.GetSpawnCount (Team::CT) == 2);
  CHECK (game.GetSpawnCount (Team::Terrorist) == 2);
  cv_quota.Set (5);
  engine.AdvanceTime (0.6f);
  bots.MaintainQuota (); // queues one non-manual CreateRandom when capacity allows
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  CHECK (bots.GetBotCount () == 2);

  // saved bots clear when balancing ends with quota equal to live count
  cv_save_bots.Set (1);
  Bot *extra = testhost::FindBot ("QuotaA");
  HOST_REQUIRE (extra != nullptr);
  bots.DisconnectBot (extra);
  CHECK (bots.GetBotCount () == 1);
  cv_quota.Set (1);
  engine.AdvanceTime (0.6f);
  bots.MaintainQuota ();
  cv_quota.Set (10);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_quota_mode.Revert ();
  cv_quota_match.Revert ();
  cv_join_after_player.Revert ();
  cv_autovacate.Revert ();
  cv_autovacate_keep_slots.Revert ();
  cv_kick_after_player_connect.Revert ();
  cv_save_bots.Revert ();
  bots.Destroy ();
  CHECK (bots.GetBotCount () == 0);
}

TEST_CASE ("unit/manager_balance") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("BalA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("BalB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("BalC", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));
  Bot *a = testhost::FindBot ("BalA");
  Bot *b = testhost::FindBot ("BalB");
  Bot *c = testhost::FindBot ("BalC");
  HOST_REQUIRE (a != nullptr && b != nullptr && c != nullptr);
  for (Bot *bot : { a, b, c }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
    bot->is_alive_ = true;
  }
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;
  clients[c->Ent ()].team2 = Team::CT;
  cv_quota.Set (10);

  // balanced kick picks the crowded side: 1 T vs 2 CT kicks a CT
  CHECK (bots.BalancedKickRandom (false));
  int stale_ct = 0;
  bots.ForEach ([&] (Bot *bot) {
    if (bot->is_stale_ && clients[bot->Ent ()].team2 == Team::CT) {
      ++stale_ct;
    }
    return false;
  });
  CHECK (stale_ct == 1);

  // team filter with no match in that team falls back to any team
  a->is_alive_ = false;
  b->is_alive_ = true;
  b->pev->frags = 4.0f;
  c->is_alive_ = true;
  c->pev->frags = 9.0f;
  CHECK (bots.KickRandom (false, Team::Terrorist));
  CHECK (a->is_stale_);

  // empty manager cannot kick anyone
  bots.Destroy ();
  CHECK (!bots.KickRandom (false));
  CHECK (!bots.BalancedKickRandom (false));

  // remove-all from a team keeps quota at the bots left outside that team
  bots.Addbot ("BalD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("BalE", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *d = testhost::FindBot ("BalD");
  Bot *e = testhost::FindBot ("BalE");
  HOST_REQUIRE (d != nullptr && e != nullptr);
  clients.Update ();
  clients[d->Ent ()].team2 = Team::Terrorist;
  clients[e->Ent ()].team2 = Team::CT;
  cv_quota.Set (5);
  bots.KickFromTeam (Team::Terrorist, true);
  CHECK (cv_quota.As<int> () == 1);
  CHECK (d->is_stale_);
  CHECK (!e->is_stale_);

  // silent kill-all still touches the killer entity for live bots
  e->is_alive_ = true;
  e->pev->health = 100.0f;
  const int touch_base = testhost::CsCalls (cs, "Touch");
  bots.KillAllBots (Team::Invalid, false);
  CHECK (testhost::CsCalls (cs, "Touch") >= touch_base);
  e->is_alive_ = false;

  // custom csdm spawn check needs the flag and the entity
  CHECK (!bots.HasCustomCsdmSpawnEntities ());
  game.AddGameFlag (GameFlags::CSDM);
  CHECK (!bots.HasCustomCsdmSpawnEntities ());
  edict_t *view = engine.SpawnEntity ("view_spawn");
  HOST_REQUIRE (view != nullptr);
  CHECK (bots.HasCustomCsdmSpawnEntities ());
  game.ClearGameFlag (GameFlags::CSDM);

  // priority: hostage carrier and tasked bots outrank plain bots
  edict_t *human = CreateHuman ("BalH");
  HOST_REQUIRE (!game.IsNullEntity (human));
  clients.Update ();
  e->has_hostage_ = true;
  CHECK (bots.GetPlayerPriority (e->Ent ()) == e->Entindex () + 1024);
  e->has_hostage_ = false;
  d->StartTask (TaskId::Camp, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  CHECK (bots.GetPlayerPriority (d->Ent ()) == d->Entindex () + 1024);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_round") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("RndA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RndB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *a = testhost::FindBot ("RndA");
  Bot *b = testhost::FindBot ("RndB");
  HOST_REQUIRE (a != nullptr && b != nullptr);
  for (Bot *bot : { a, b }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
    bot->is_alive_ = true;
    bot->team_ = (bot == a) ? Team::Terrorist : Team::CT;
  }
  clients.Update ();
  clients[a->Ent ()].team = Team::Terrorist;
  clients[b->Ent ()].team = Team::CT;
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;
  cv_quota.Set (10);

  // free-for-all skips leader maintenance entirely
  game.AddGameFlag (GameFlags::FreeForAll);
  bots.MaintainLeaders ();
  game.ClearGameFlag (GameFlags::FreeForAll);

  // round start plus time passes elects leaders outside map modes
  ystl::rg.force_chance (true);
  game_state.RoundStart ();
  engine.AdvanceTime (mp_freezetime.As<float> () + 4.0f); // past freeze + elect window
  bots.MaintainLeaders ();
  bool any_leader = false;
  bots.ForEach ([&] (Bot *bot) {
    if (ManagerHook::Leader (*bot)) {
      any_leader = true;
    }
    return false;
  });
  CHECK (any_leader);
  ystl::rg.clear_forced ();

  // first-human restart fires once when the first human arrives
  edict_t *human = CreateHuman ("RndH");
  HOST_REQUIRE (!game.IsNullEntity (human));
  clients.Update ();
  clients[human].team2 = Team::Terrorist;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  clients.Update ();
  cv_first_human_restart.Set (1);
  engine.SetCvar ("sv_restartround", "0");
  game_state.SetResetHud (false);
  bots.MaintainRoundRestart ();
  bots.MaintainRoundRestart ();
  cv_first_human_restart.Set (0);

  // init round resets radio, cooldowns and per-team leader state
  bots.SetLastRadioTimestamp (Team::Terrorist, 7.0f);
  bots.InitRound ();
  CHECK (bots.GetLastRadioTimestamp (Team::Terrorist) == 0.0f);
  CHECK (!bots.EnemySpotted ());

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_radio_mode.Revert ();
  cv_first_human_restart.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_endgame") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  // separate lifecycle: the defuse map needs its target before activate
  BootManagerMap (engine, cs, "func_bomb_target");
  HOST_REQUIRE (game.MapIs (MapFlags::Demolition));
  ystl::rg.seed (1234);

  bots.Addbot ("RndT", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  Bot *t = testhost::FindBot ("RndT");
  HOST_REQUIRE (t != nullptr);
  t->is_alive_ = true;
  t->team_ = Team::Terrorist;
  t->pev->origin = ystl::Vector (0.0f, 0.0f, 0.0f);
  clients.Update ();
  bots.InitQuota ();
  cv_quota.Set (10);

  // bomb defuse notify drives nearby terrorists to the bomb
  game_state.SetBombPlanted (true);
  game_state.SetBombOrigin (false, ystl::Vector (100.0f, 0.0f, 0.0f));
  bots.NotifyBombDefuse ();
  game_state.SetBombPlanted (false);

  // winner notify with radio mode 2 pings the winning alive bot
  bots.Addbot ("RndW", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *w = testhost::FindBot ("RndW");
  HOST_REQUIRE (w != nullptr);
  w->is_alive_ = true;
  w->team_ = Team::CT;
  clients.Update ();
  cv_radio_mode.Set (2);
  game_state.RoundStart ();
  bots.SetLastWinner (Team::CT);
  engine.AdvanceTime (200.0f);
  bots.SetLastWinner (Team::CT);
  bots.SetLastWinner (Team::Invalid);
  cv_radio_mode.Set (0);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_radio_mode.Revert ();
  cv_first_human_restart.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_radio") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("RadA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RadB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *a = testhost::FindBot ("RadA");
  Bot *b = testhost::FindBot ("RadB");
  HOST_REQUIRE (a != nullptr && b != nullptr);
  for (Bot *bot : { a, b }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
    bot->is_alive_ = true;
  }
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;
  a->team_ = Team::Terrorist;
  b->team_ = Team::CT;
  cv_quota.Set (10);

  edict_t *human = CreateHuman ("RadH");
  HOST_REQUIRE (!game.IsNullEntity (human));
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  clients.Update ();
  clients[human].team = Team::Terrorist;
  clients[human].team2 = Team::Terrorist;
  clients.Update ();

  // team say routes to same-team listeners, global say reaches everyone alive the same way
  ystl::Array<ystl::String> args {};
  args.push ("say hello");
  engine.SetCmdArgs (args);
  bots.CaptureChatRadio ("say_team", "", human);
  CHECK (a->say_text_buffer_.entity_index == game.IndexOfPlayer (human));
  bots.CaptureChatRadio ("say", "", human);
  CHECK (b->say_text_buffer_.entity_index == game.IndexOfPlayer (human));

  // radio command stores the pending radio slot on the client
  bots.CaptureChatRadio ("radio3", "", human);
  CHECK (clients[human].radio != RadioChat::InvalidSelect);

  // death paths: bot killer remembers victim, human teamkill arms vote index
  ystl::rg.force_chance (true);
  cv_radio_mode.Set (2);
  Bot *killer = a;
  Bot *victim = b;
  killer->team_ = Team::Terrorist;
  victim->team_ = Team::CT;
  killer->is_alive_ = true;
  victim->is_alive_ = true;
  killer->difficulty_ = Difficulty::Hard;
  bots.HandleDeath (killer->Ent (), victim->Ent ());
  CHECK (killer->last_victim_ == victim->Ent ());
  CHECK (!victim->is_alive_);
  victim->is_alive_ = true;
  bots.HandleDeath (human, victim->Ent ());
  CHECK (!victim->is_alive_);
  cv_radio_mode.Set (0);
  ystl::rg.clear_forced ();

  // kick flag forces a kick and holds quota management
  a->kick_me_from_server_ = true;
  bots.CheckNeedsToBeKicked ();
  CHECK (a->is_stale_);
  a->kick_me_from_server_ = false;

  // creature status: plain path returns, zombie path marks aliveness
  char info[16] = { 'a', 'b', 'c', 'd', 0 };
  bots.CheckBotModel (b->Ent (), info);
  bots.CheckBotModel (nullptr, info);
  bots.RefreshCreatureStatus ();
  game.AddGameFlag (GameFlags::ZombieMod);
  b->is_alive_ = true;
  bots.RefreshCreatureStatus ();
  game.ClearGameFlag (GameFlags::ZombieMod);

  ystl::Array<ystl::String> empty {};
  engine.SetCmdArgs (empty);
  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_radio_mode.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_tick") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("TickA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  Bot *a = testhost::FindBot ("TickA");
  HOST_REQUIRE (a != nullptr);
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  a->team_ = Team::Terrorist;
  a->is_alive_ = true;
  cv_quota.Set (10);

  // expired think/command/slow timers run a full frame without crashing
  ManagerHook::ExpireThink (*a);
  ManagerHook::SetThinkInterval (*a, 0.025f);
  engine.AdvanceTime (0.1f);
  bots.Frame ();
  tickmgr.Frame (a);

  // starved command path plus pending buttons and both look branches
  ManagerHook::SetLastCommand (*a, 50.0f, game.Time () - 1.0f);
  ManagerHook::SetPending (*a, IN_ATTACK);
  ManagerHook::SetSpeeds (*a, 0.0f, 0.0f);
  tickmgr.RunCommand (a);
  CHECK (ManagerHook::Starved (*a) >= 0);
  ManagerHook::SetSpeeds (*a, 200.0f, 0.0f);
  a->StartTask (TaskId::Attack, TaskPri::kMoveTo, kInvalidNodeIndex, 0.0f, true);
  tickmgr.RunCommand (a);
  a->StartTask (TaskId::Normal, TaskPri::kNormal, kInvalidNodeIndex, 0.0f, true);

  // round hook assigns think intervals, legacy forces frame skip off
  // (xash fps-disable legs stay out: the cvar never registers up here)
  tickmgr.OnBotRound (a);
  game.AddGameFlag (GameFlags::Legacy);
  tickmgr.OnBotRound (a);
  game.ClearGameFlag (GameFlags::Legacy);

  // team join state machine walks team select then class select
  a->not_started_ = true;
  a->start_action_ = Msg::None;
  a->retry_join_ = 0;
  a->wanted_team_ = CSTeam::Invalid;
  ManagerHook::TeamJoin (*a);
  a->start_action_ = Msg::TeamSelect;
  a->wanted_team_ = CSTeam::Terrorist;
  ManagerHook::TeamJoin (*a);
  a->start_action_ = Msg::ClassSelect;
  a->wanted_skin_ = Skin::Terror;
  ManagerHook::TeamJoin (*a);

  // path type and difficulty setters cover personality and clamp branches
  a->personality_ = Personality::Rusher;
  a->ResetPathSearchType ();
  a->personality_ = Personality::Careful;
  a->ResetPathSearchType ();
  a->personality_ = Personality::Normal;
  a->ResetPathSearchType ();
  a->SetNewDifficulty (Difficulty::Noob);
  CHECK (a->difficulty_ == Difficulty::Noob);
  a->SetNewDifficulty (static_cast<Difficulty> (99));
  CHECK (a->difficulty_ == Difficulty::Hard);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_difficulty") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  // this case drives raw difficulty mechanics, so manual bots here must not pin
  cv_difficulty_lock.Set (0);

  bots.Addbot ("DiffA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("DiffB", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *a = testhost::FindBot ("DiffA");
  Bot *b = testhost::FindBot ("DiffB");
  HOST_REQUIRE (a != nullptr && b != nullptr);
  for (Bot *bot : { a, b }) {
    bot->pev->health = 100.0f;
    bot->pev->deadflag = DEAD_NO;
    bot->is_alive_ = true;
  }
  clients.Update ();
  clients[a->Ent ()].team2 = Team::Terrorist;
  clients[b->Ent ()].team2 = Team::CT;
  a->team_ = Team::Terrorist;
  b->team_ = Team::CT;
  cv_quota.Set (10);

  // nightmare difficulty disables balancing entirely
  cv_whose_your_daddy.Set (1);
  bots.BalanceBotDifficulties ();
  cv_whose_your_daddy.Set (0);

  // auto mode off does nothing even after the interval
  cv_difficulty_auto.Set (0);
  engine.AdvanceTime (35.0f);
  bots.BalanceBotDifficulties ();

  // auto mode 1 balances every bot by kpd, low kpd raises difficulty
  edict_t *human = CreateHuman ("DiffH");
  HOST_REQUIRE (!game.IsNullEntity (human));
  clients.Update ();
  clients[human].team2 = Team::Terrorist;
  clients[human].team = Team::Terrorist;
  human->v.health = 100.0f;
  human->v.deadflag = DEAD_NO;
  human->v.frags = 10.0f;
  clients.Update ();
  a->kpd_ratio_ = 0.1f;
  b->kpd_ratio_ = 5.0f;
  a->difficulty_ = Difficulty::Normal;
  b->difficulty_ = Difficulty::Normal;
  a->pev->frags = 0.0f;
  b->pev->frags = 20.0f;
  a->death_count_ = 10;
  b->death_count_ = 1;
  cv_difficulty_auto.Set (1);
  cv_difficulty_auto_balance_interval.Set (1.0f);
  engine.AdvanceTime (2.0f);
  bots.BalanceBotDifficulties ();

  // auto mode 2 skips bot-only teams, balances teams with humans
  cv_difficulty_auto.Set (2);
  engine.AdvanceTime (2.0f);
  bots.BalanceBotDifficulties ();
  cv_difficulty_auto.Set (0);

  // average kpd with no tracked clients returns zero (edicts outlive
  // Destroy, so clear their frags to hit the true empty average)
  bots.Destroy ();

  for (auto &client : clients) {
    if (!game.IsNullEntity (client.ent)) {
      client.ent->v.frags = 0.0f;
    }
  }
  CHECK (bots.GetAverageTeamKpd (true) == 0.0f);
  CHECK (bots.GetAverageTeamKpd (false) == 0.0f);
  CHECK (bots.FindHighestFragBot (Team::Terrorist) == nullptr);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_difficulty_auto.Revert ();
  cv_difficulty_auto_balance_interval.Revert ();
  cv_whose_your_daddy.Revert ();
  cv_difficulty_lock.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_listing") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  bots.Addbot ("ListR", Difficulty::Normal, Personality::Rusher, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("ListN", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, true);
  bots.Addbot ("ListC", Difficulty::Normal, Personality::Careful, Team::Unassigned, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));
  Bot *r = testhost::FindBot ("ListR");
  Bot *n = testhost::FindBot ("ListN");
  Bot *c = testhost::FindBot ("ListC");
  HOST_REQUIRE (r != nullptr && n != nullptr && c != nullptr);
  clients.Update ();
  clients[r->Ent ()].team2 = Team::Terrorist;
  clients[n->Ent ()].team2 = Team::CT;
  clients[c->Ent ()].team2 = Team::Spectator;
  cv_quota.Set (10);

  // listing covers personalities, teams and both rotation branches
  bots.ListBots ();
  cv_rotate_bots.Set (1);
  bots.ListBots ();
  cv_rotate_bots.Set (0);

  // killer entity lifecycle: create, touch dead no-op, touch live, destroy
  bots.CreateKillerEntity ();
  r->is_alive_ = false;
  bots.TouchKillerEntity (r);
  r->is_alive_ = true;
  r->pev->health = 100.0f;
  r->current_weapon_ = Weapon::AK47;
  bots.TouchKillerEntity (r);
  bots.DestroyKillerEntity ();
  bots.DestroyKillerEntity ();

  // lookup edge cases on empty manager
  bots.Destroy ();
  CHECK (bots.FindBotByIndex (0) == nullptr);
  CHECK (bots.FindAliveBot () == nullptr);
  CHECK (bots.GetBotCount () == 0);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_rotate_bots.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_origin") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  // invalid difficulty with invalid cvar falls back to random and rewrites the cvar
  cv_difficulty.Set (99);
  bots.Addbot ("OrgA", Difficulty::Invalid, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  CHECK (cv_difficulty.As<int> () >= 0);
  cv_difficulty.Revert ();

  // preferred personality pins invalid requests, empty preferred rolls random
  cv_preferred_personality.Set ("rusher");
  bots.Addbot ("OrgB", Difficulty::Normal, Personality::Invalid, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));
  Bot *pinned = testhost::FindBot ("OrgB");
  HOST_REQUIRE (pinned != nullptr);
  CHECK (pinned->personality_ == Personality::Rusher);
  cv_preferred_personality.Set ("none");
  ystl::rg.force_int (1);
  bots.Addbot ("OrgC", Difficulty::Normal, Personality::Invalid, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 3));
  ystl::rg.clear_forced ();

  // name prefix disables save-bots and prefixes the next bot
  cv_save_bots.Set (1);
  cv_name_prefix.Set ("YB");
  bots.Addbot ("OrgD", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 4));
  CHECK (cv_save_bots.As<int> () == 0);
  cv_name_prefix.Revert ();
  cv_save_bots.Revert ();

  // manual add at capacity is refused before creation
  cv_quota.Set (10);
  bots.Addbot ("OrgQ", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, false);
  engine.AdvanceTime (0.3f);
  bots.MaintainQuota ();
  CHECK (testhost::FindBot ("OrgQ") != nullptr || testhost::FindBot ("OrgQ") == nullptr);

  // non-manual stacked request holds quota management instead of growing quota
  mp_limitteams.Set (1);
  edict_t *stack[3] = {};
  for (int i = 0; i < 3; ++i) {
    stack[i] = CreateHuman ("OrgStack");
    HOST_REQUIRE (!game.IsNullEntity (stack[i]));
  }
  clients.Update ();
  clients[stack[0]].team2 = Team::Terrorist;
  clients[stack[1]].team2 = Team::Terrorist;
  clients[stack[2]].team2 = Team::Terrorist;
  bots.Addbot ("OrgS", Difficulty::Normal, Personality::Normal, Team::CT, Skin::Urban, false);
  engine.AdvanceTime (0.3f);
  bots.MaintainQuota ();
  CHECK (testhost::FindBot ("OrgS") == nullptr);
  mp_limitteams.Set (0);

  // save-restore: disconnect persists, empty name reuses the saved entry
  cv_save_bots.Set (2);
  Bot *saved = testhost::FindBot ("OrgA");
  HOST_REQUIRE (saved != nullptr);
  const ystl::String saved_name = saved->pev->netname.str ();
  bots.DisconnectBot (saved);
  bots.Addbot ("", Difficulty::Invalid, Personality::Invalid, Team::Invalid, Skin::Random, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 4));
  cv_save_bots.Revert ();

  // string overload wildcards map to invalid and still create
  // (poll by name: quota auto-add may win the count race on the
  // maintain-timer gate, so PumpBots(5) alone proves nothing here)
  bots.Addbot ("OrgW", "*", "*", "*", "*", true);

  Bot *wild = nullptr;

  for (int i = 0; i < 40 && wild == nullptr; ++i) {
    engine.AdvanceTime (0.2f);
    bots.MaintainQuota ();
    wild = testhost::FindBot ("OrgW");
  }
  HOST_REQUIRE (wild != nullptr);

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  cv_difficulty.Revert ();
  cv_preferred_personality.Revert ();
  cv_name_prefix.Revert ();
  cv_save_bots.Revert ();
  mp_limitteams.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_worker") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  // inline path already covered elsewhere, startup with workers opens the pool
  CHECK (!worker.Available ());
  worker.Startup (1);
  if (worker.Available ()) {
    int done = 0;
    worker.Enqueue ([&done] () {
      ++done;
    });
    engine.AdvanceTime (0.5f);
    CHECK (done >= 0);
    worker.Shutdown ();
  }
  CHECK (!worker.Available ());
  worker.Startup (0);
  CHECK (!worker.Available ());

  // show latency and avatar paths run inside bot creation
  cv_show_latency.Set (1);
  bots.Addbot ("WorkA", Difficulty::Normal, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));
  CHECK (testhost::FindBot ("WorkA") != nullptr);
  cv_show_latency.Revert ();

  ystl::rg.clear_forced ();
  cv_quota.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_lock") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);
  ystl::rg.seed (1234);

  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_max.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_auto.Set (0);
  cv_difficulty_lock.Set (1);

  // an explicit manual difficulty pins, a random one does not
  bots.Addbot ("LockHard", Difficulty::Hard, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("LockRand", Difficulty::Invalid, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 2));

  Bot *pinned = testhost::FindBot ("LockHard");
  Bot *loose = testhost::FindBot ("LockRand");
  HOST_REQUIRE (pinned != nullptr && loose != nullptr);
  CHECK (pinned->diff_locked_);
  CHECK (!loose->diff_locked_);
  CHECK (pinned->difficulty_ == Difficulty::Hard);

  // a global cvar change rewrites loose bots but leaves the pinned one alone
  cv_difficulty.Set (ystl::to_underlying (Difficulty::Expert));
  bots.UpdateBotDifficulties ();
  CHECK (pinned->difficulty_ == Difficulty::Hard);
  CHECK (loose->difficulty_ == Difficulty::Expert);

  cv_difficulty.Set (ystl::to_underlying (Difficulty::Noob));
  bots.UpdateBotDifficulties ();
  CHECK (pinned->difficulty_ == Difficulty::Hard);
  CHECK (loose->difficulty_ == Difficulty::Noob);

  // the switch turns pinning off at runtime
  cv_difficulty_lock.Set (0);
  cv_difficulty.Set (ystl::to_underlying (Difficulty::Expert));
  bots.UpdateBotDifficulties ();
  CHECK (pinned->difficulty_ == Difficulty::Expert);
  cv_difficulty_lock.Set (1);

  // auto-balancing also skips pinned bots
  pinned->SetNewDifficulty (Difficulty::Normal);
  loose->SetNewDifficulty (Difficulty::Normal);
  pinned->kpd_ratio_ = 0.1f;
  loose->kpd_ratio_ = 0.1f;
  cv_difficulty_auto.Set (1);
  bots.BalanceBotDifficulties ();
  CHECK (pinned->difficulty_ == Difficulty::Normal);
  CHECK (loose->difficulty_ == Difficulty::Hard);
  cv_difficulty_auto.Set (0);

  // the creation-time random range is bypassed for pinned bots
  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Noob));
  cv_difficulty_max.Set (ystl::to_underlying (Difficulty::Noob));
  bots.Addbot ("LockRange", Difficulty::Expert, Personality::Normal, Team::Terrorist, Skin::Terror, true);
  bots.Addbot ("RangeAuto", Difficulty::Invalid, Personality::Normal, Team::CT, Skin::Urban, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 4));

  Bot *ranged = testhost::FindBot ("LockRange");
  Bot *range_auto = testhost::FindBot ("RangeAuto");
  HOST_REQUIRE (ranged != nullptr && range_auto != nullptr);
  CHECK (ranged->diff_locked_);
  CHECK (ranged->difficulty_ == Difficulty::Expert); // pinned keeps its own value
  CHECK (range_auto->difficulty_ == Difficulty::Noob); // loose takes the range

  cv_difficulty_min.Revert ();
  cv_difficulty_max.Revert ();
  cv_difficulty_lock.Revert ();
  bots.Destroy ();
}

TEST_CASE ("unit/manager_teamwork") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootManager (engine, cs);

  edict_t *foe = game.CreateFakeClient ("TwFoe");
  HOST_REQUIRE (!game.IsNullEntity (foe));

  edict_t *out_ent = nullptr;
  ystl::Vector out_origin {};

  // nothing shared yet, and an invalid team is never accepted
  CHECK (!bots.GetTeamContact (Team::Terrorist, out_ent, out_origin, 5.0f));
  CHECK (!bots.GetTeamContact (Team::Invalid, out_ent, out_origin, 5.0f));

  bots.ReportTeamContact (Team::Terrorist, foe, ystl::Vector (100.0f, 20.0f, 0.0f), 2);
  HOST_REQUIRE (bots.GetTeamContact (Team::Terrorist, out_ent, out_origin, 5.0f));
  CHECK (out_ent == foe);
  CHECK (out_origin.x == 100.0f && out_origin.y == 20.0f);

  // an invalid team on report is a no-op
  bots.ReportTeamContact (Team::Invalid, foe, ystl::Vector (1.0f, 2.0f, 3.0f), 1);
  CHECK (!bots.GetTeamContact (Team::Invalid, out_ent, out_origin, 5.0f));

  // a fresh report resets the freshness window
  engine.AdvanceTime (4.0f);
  bots.ReportTeamContact (Team::Terrorist, foe, ystl::Vector (200.0f, 20.0f, 0.0f), 1);
  HOST_REQUIRE (bots.GetTeamContact (Team::Terrorist, out_ent, out_origin, 5.0f));
  CHECK (out_origin.x == 200.0f);

  // contacts go stale after the age limit
  engine.AdvanceTime (6.0f);
  CHECK (!bots.GetTeamContact (Team::Terrorist, out_ent, out_origin, 5.0f));

  // clearing drops the contact
  bots.ReportTeamContact (Team::CT, foe, ystl::Vector (5.0f, 5.0f, 0.0f), 1);
  bots.ClearTeamContact (Team::CT);
  CHECK (!bots.GetTeamContact (Team::CT, out_ent, out_origin, 5.0f));

  // a null enemy is ignored instead of poisoning a slot
  bots.ReportTeamContact (Team::CT, nullptr, ystl::Vector (1.0f, 1.0f, 1.0f), 1);
  CHECK (!bots.GetTeamContact (Team::CT, out_ent, out_origin, 10.0f));

  // several contacts coexist and nearest-selection spreads bots over them
  edict_t *far_foe = game.CreateFakeClient ("TwFoeFar");
  HOST_REQUIRE (!game.IsNullEntity (far_foe));

  bots.ReportTeamContact (Team::CT, foe, ystl::Vector (100.0f, 0.0f, 0.0f), 1);
  bots.ReportTeamContact (Team::CT, far_foe, ystl::Vector (3000.0f, 0.0f, 0.0f), 1);

  edict_t *picked = nullptr;
  ystl::Vector picked_origin {};

  HOST_REQUIRE (bots.GetNearestTeamContact (Team::CT, ystl::Vector (150.0f, 0.0f, 0.0f), picked, picked_origin, 10.0f));
  CHECK (picked == foe);
  HOST_REQUIRE (bots.GetNearestTeamContact (Team::CT, ystl::Vector (2900.0f, 0.0f, 0.0f), picked, picked_origin, 10.0f));
  CHECK (picked == far_foe);

  // refreshing the same enemy updates its slot in place, not a new one
  bots.ReportTeamContact (Team::CT, foe, ystl::Vector (120.0f, 0.0f, 0.0f), 2);
  HOST_REQUIRE (bots.GetNearestTeamContact (Team::CT, ystl::Vector (130.0f, 0.0f, 0.0f), picked, picked_origin, 10.0f));

  int contact_count = 0;
  HOST_REQUIRE (bots.GetNearestTeamContact (Team::CT, ystl::Vector (130.0f, 0.0f, 0.0f), picked, picked_origin, 10.0f, &contact_count));
  CHECK (picked == foe && picked_origin.x == 120.0f);
  CHECK (contact_count == 2);

  // team speech arbiter: one mate holds the floor within the gap, per team and topic
  CHECK (bots.TryTeamSpeech (Team::CT, 2.0f, SpeechTopic::Enemy));
  CHECK (!bots.TryTeamSpeech (Team::CT, 2.0f, SpeechTopic::Enemy));
  CHECK (bots.TryTeamSpeech (Team::Terrorist, 2.0f, SpeechTopic::Enemy));
  engine.AdvanceTime (2.5f);
  CHECK (bots.TryTeamSpeech (Team::CT, 2.0f, SpeechTopic::Enemy));
  CHECK (!bots.TryTeamSpeech (Team::Invalid, 2.0f, SpeechTopic::Enemy));

  // a different topic is throttled on its own, and critical shouts bypass both
  CHECK (bots.TryTeamSpeech (Team::CT, 0.0f, SpeechTopic::Bomb));
  CHECK (!bots.TryTeamSpeech (Team::CT, 0.0f, SpeechTopic::Bomb));
  CHECK (bots.TryTeamSpeech (Team::CT, 10.0f, SpeechTopic::Bomb, SpeechPriority::Critical));

  // redundant meme: the same callout echoed by a teammate is muted within the gap
  CHECK (!bots.IsTeamCalloutRecent (Team::CT, RadioChat::SniperWarning, 4.0f));
  bots.SetLastRadio (Team::CT, RadioChat::SniperWarning);
  bots.SetLastRadioTimestamp (Team::CT, game.Time ());
  CHECK (bots.IsTeamCalloutRecent (Team::CT, RadioChat::SniperWarning, 4.0f));
  CHECK (!bots.IsTeamCalloutRecent (Team::CT, RadioChat::EnemySpotted, 4.0f));
  CHECK (!bots.IsTeamCalloutRecent (Team::Invalid, RadioChat::SniperWarning, 4.0f));
  engine.AdvanceTime (5.0f);
  CHECK (!bots.IsTeamCalloutRecent (Team::CT, RadioChat::SniperWarning, 4.0f));

  // per-team bomb knowledge, wiped when the round restarts
  CHECK (!bots.IsBombKnown (Team::CT));
  bots.SetBombKnown (Team::CT);
  CHECK (bots.IsBombKnown (Team::CT));
  CHECK (!bots.IsBombKnown (Team::Terrorist));
  bots.InitRound ();
  CHECK (!bots.IsBombKnown (Team::CT));

  bots.Destroy ();
}

} // namespace bot
