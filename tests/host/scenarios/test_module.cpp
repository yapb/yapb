//
// YaPB test host: unit/module_api.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for module.cpp: the versioned module interface
// and every BotModule query/command on a live graph with a bot.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootModule (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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

  graph.Reset ();

  for (int i = 0; i < 5; ++i) {
    Path path {};
    path.origin = ystl::Vector (100.0f * i, 0.0f, 0.0f);
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

  for (int i = 0; i < 4; ++i) {
    link (i, i + 1, 100);
    link (i + 1, i, 100);
  }
  graph.PopulateNodes ();
  planner.Init ();

  bots.InitQuota ();
  cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/module_api") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootModule (engine, cs);

  // version handshake first...
  CHECK (GetBotAPI (999) == nullptr);

  IBotModule *api = GetBotAPI (kBotModuleVersion);
  HOST_REQUIRE (api != nullptr);
  CHECK (api == GetBotAPI (kBotModuleVersion)); // singleton, stable across calls
  CHECK (ystl::StringRef (api->GetBotVersion ()).size () > 0);

  // ...packed api version negotiates by major/minor...
  CHECK (IBotModule::ApiMajor (api->GetApiVersion ()) == kBotModuleApiMajor);
  CHECK (IBotModule::ApiMinor (api->GetApiVersion ()) == kBotModuleApiMinor);
  CHECK (GetBotAPI (kBotModuleVersion + 1) == nullptr); // newer minor rejected
  CHECK (GetBotAPI ((IBotModule::ApiMajor (kBotModuleVersion) + 1) << 16) == nullptr); // newer major rejected

  // ...no bots, no graph answers beyond the static ones...
  CHECK (!api->IsBotsInGame ());
  CHECK (!api->IsBot (1));
  CHECK (!api->IsBot (0));
  CHECK (!api->IsBot (99));
  CHECK (api->HasGraph ());
  CHECK (api->IsNodeValid (2));
  CHECK (!api->IsNodeValid (99));
  CHECK (api->GetNodeFlags (0) == 0);
  CHECK (api->GetNodeCount () == 5);
  CHECK (api->GetNodeLinkCount (0) == 1);
  CHECK (api->GetNodeLink (0, 0) == 1);
  CHECK (api->GetNodeLink (0, 1) == kInvalidNodeIndex);
  CHECK (api->GetNodeLinkFlags (0, 0) == 0);
  CHECK (api->GetGraphAuthor () != nullptr);
  CHECK (api->GetGraphModified () != nullptr);

  float origin[3] = { 150.0f, 0.0f, 0.0f };
  CHECK (api->GetNearestNode (origin) == 1 || api->GetNearestNode (origin) == 2);
  CHECK (api->FindNearestNode (origin, 1000.0f) == 1 || api->FindNearestNode (origin, 1000.0f) == 2);
  CHECK (api->FindFarestNode (origin, 10.0f) >= 0);
  CHECK (api->FindNearestNodeInRadius (origin, 60.0f) == 1 || api->FindNearestNodeInRadius (origin, 60.0f) == 2);
  CHECK (api->FindNearestNodeInRadius (origin, 10.0f) == kInvalidNodeIndex);
  CHECK (api->GetNodeDistance (0, 1) == 100.0f);
  CHECK (api->GetNodeDistance (0, 4) == 400.0f);
  CHECK (api->GetNodeDistance (0, 99) < 0.0f);
  CHECK (api->GetRandomNode (0) == kInvalidNodeIndex);
  CHECK (api->GetRandomNode (99) == kInvalidNodeIndex);

  float *node_origin = api->GetNodeOrigin (2);
  HOST_REQUIRE (node_origin != nullptr);
  CHECK (node_origin[0] == 200.0f);
  CHECK (api->GetNodeOrigin (99) == nullptr);

  // ...a live bot answers identity, node and goal queries...
  bots.Addbot ("ModA", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = nullptr;

  bots.ForEach ([&] (Bot *candidate) {
    bot = candidate;
    return true;
  });
  HOST_REQUIRE (bot != nullptr);
  bot->team_ = Team::CT;

  const int slot = game.IndexOfPlayer (bot->Ent ()) + 1;
  CHECK (api->IsBotsInGame ());
  CHECK (api->IsBot (slot));
  CHECK (api->GetBotCount () == 1);
  CHECK (api->GetCurrentNodeId (slot) == kInvalidNodeIndex);

  CHECK (api->GetBotOrigin (slot) != nullptr);
  CHECK (api->GetBotLookAt (slot) != nullptr);
  CHECK (api->GetBotLastEnemyOrigin (slot) != nullptr);
  CHECK (api->GetBotWeapon (slot) >= 0);
  CHECK (api->GetBotTask (slot) >= 0);
  CHECK (api->SetBotDifficulty (slot, static_cast<int> (Difficulty::Hard)));
  CHECK (!api->SetBotDifficulty (slot, 99));
  CHECK (!api->SetBotDifficulty (99, static_cast<int> (Difficulty::Hard)));

  api->SetBotMovement (slot, false);
  CHECK (!api->IsBotMovement (slot));
  api->SetBotMovement (slot, true);
  CHECK (api->IsBotMovement (slot));
  CHECK (!api->IsBotMovement (99));
  CHECK (api->GetBotPathNode (99, 0) == kInvalidNodeIndex);
  CHECK (api->GetBotAmmoInClip (slot) >= 0);
  CHECK (api->GetBotPathLength (slot) >= 0);
  CHECK (api->GetBotStuckTime (slot) >= 0.0f);
  CHECK (!api->IsBotStuck (slot));
  CHECK (!api->IsBotCamping (slot));
  CHECK (api->GetBotEnemy (slot) == 0);
  CHECK (api->GetBotLastEnemy (slot) == 0);
  CHECK (api->GetBotLastVictim (slot) == 0);
  CHECK (!api->IsBotEnemyReachable (slot));
  CHECK (api->GetBotOrigin (99) == nullptr);
  CHECK (api->GetBotEnemy (99) == 0);

  api->SetBotGoal (slot, 3);
  CHECK (api->GetBotGoal (slot) != kInvalidNodeIndex);

  float goal[3] = { 300.0f, 0.0f, 0.0f };
  api->SetBotGoalOrigin (slot, goal);

  // ...bad indices never reach the bots...
  api->SetBotGoal (99, 3);
  api->SetBotGoal (slot, 99);
  api->SetBotGoalOrigin (99, goal);
  CHECK (api->GetCurrentNodeId (99) == kInvalidNodeIndex);
  CHECK (api->GetBotGoal (99) == kInvalidNodeIndex);

  // ...an empty graph answers empty...
  graph.Reset ();
  CHECK (!api->HasGraph ());
  CHECK (api->GetNearestNode (origin) == kInvalidNodeIndex);
  CHECK (api->GetNodeFlags (0) == 0);

  // ...adding a bot queues the request...
  CHECK (api->AddBot ("", static_cast<int> (Difficulty::Normal), static_cast<int> (Personality::Normal), 1));

  bots.Destroy ();
}

} // namespace bot
