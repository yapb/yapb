//
// YaPB test host: unit/module_extra.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for module.cpp: the getters the base api test never
// calls (invalid-entity defaults plus live-bot answers) and the node
// radius/light/reachability helpers.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootModuleExtra (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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

   for (int i = 0; i < 4; ++i) {
      for (auto &slot : graph.paths_[static_cast<size_t> (i)].links) {
         if (slot.index == kInvalidNodeIndex) {
            slot.index = static_cast<int16_t> (i + 1);
            slot.distance = 100;
            break;
         }
      }
      for (auto &slot : graph.paths_[static_cast<size_t> (i + 1)].links) {
         if (slot.index == kInvalidNodeIndex) {
            slot.index = static_cast<int16_t> (i);
            slot.distance = 100;
            break;
         }
      }
   }
   graph.PopulateNodes ();
   planner.Init ();

   bots.InitQuota ();
   cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/module_extra") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};

   BootModuleExtra (engine, cs);

   IBotModule *api = GetBotAPI (kBotModuleVersion);
   HOST_REQUIRE (api != nullptr);

   // ...invalid entities answer with safe defaults, never null derefs...
   CHECK (api->GetBotPing (99) == 0);
   CHECK (api->GetBotAmmo (99, 0) == 0);
   CHECK (api->GetBotAmmo (99, 9999) == 0);
   CHECK (api->GetBotDifficulty (99) == static_cast<int> (Difficulty::Invalid));
   CHECK (api->GetBotPersonality (99) == static_cast<int> (Personality::Invalid));
   CHECK (api->GetBotTaskData (99) == kInvalidNodeIndex);
   CHECK (!api->IsNodeReachable (99, 0));
   CHECK (!api->IsNodeReachable (0, 99));
   CHECK (api->GetNodeRadius (99) == 0.0f);
   CHECK (api->GetNodeLight (99) == 0.0f);

   float nowhere[3] = { 0.0f, 0.0f, 0.0f };
   api->SetBotLookAt (99, nowhere); // no-op, must not crash

   // ...a live bot answers the same getters with live data...
   bots.Addbot ("ModX", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
   HOST_REQUIRE (testhost::PumpBots (engine, 1));

   Bot *bot = nullptr;

   bots.ForEach ([&] (Bot *candidate) {
      bot = candidate;
      return true;
   });
   HOST_REQUIRE (bot != nullptr);
   bot->team_ = Team::CT;

   const int slot = game.IndexOfPlayer (bot->Ent ()) + 1;
   CHECK (api->GetBotPing (slot) >= 0);
   CHECK (api->GetBotAmmo (slot, 0) >= 0);
   CHECK (api->GetBotAmmoInClip (slot) >= 0);
   CHECK (api->GetBotDifficulty (slot) == static_cast<int> (bot->difficulty_));
   CHECK (api->GetBotPersonality (slot) == static_cast<int> (bot->personality_));
   CHECK (api->GetBotTaskData (slot) == bot->Task ()->data);

   // ...node helpers on the live graph...
   CHECK (api->IsNodeReachable (0, 1));
   CHECK (!api->IsNodeReachable (0, 99));
   CHECK (api->GetNodeRadius (0) >= 0.0f);
   CHECK (api->GetNodeLight (0) == kInvalidLightLevel);

   float look[3] = { 300.0f, 0.0f, 0.0f };
   api->SetBotLookAt (slot, look);
   CHECK (bot->ForcedLookAt ().x == 300.0f);

   bots.Destroy ();
}

} // namespace bot
