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

struct ManagerHook {
  static bool Leader (Bot &bot) {
    return bot.is_leader_;
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
  bots.Addbot ("NoGraph", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);

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
  bots.Addbot ("", Difficulty::Invalid, Personality::Invalid, Team::Invalid, -1, true);
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
  bots.Addbot ("Stacked", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);

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

  bots.Addbot ("KickA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("KickB", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("KickC", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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
  bots.Addbot ("KickD", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("KickE", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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
  bots.Addbot ("Queued", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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
  bots.Addbot ("Dropped", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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

  bots.Addbot ("TeamA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("TeamB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  bots.Addbot ("TeamK", Difficulty::Hard, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("TeamV", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  bots.Addbot ("TeamW", Difficulty::Hard, Personality::Normal, Team::CT, 1, true);
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

  // difficulties propagate unless pinned or automatic
  cv_difficulty_min.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_max.Set (ystl::to_underlying (Difficulty::Invalid));
  cv_difficulty_auto.Set (0);
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

  bots.Addbot ("KeepA", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("KeepB", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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

  bots.Addbot ("AsT", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("AsV", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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

  bots.Addbot ("DeT", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  bots.Addbot ("DeC", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
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

} // namespace bot
