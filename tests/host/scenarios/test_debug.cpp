//
// YaPB test host: unit/debug_panel.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for debug.cpp through the public panel API only:
// guards, reset/render/update legs on a live bot.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootDebug (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
   graph.PopulateNodes ();
   planner.Init ();

   bots.InitQuota ();
   cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/debug_panel") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};

   BootDebug (engine, cs);

   // ...null guards never touch the panel...
   botdebug.Update (nullptr);
   botdebug.Render (nullptr, "nobody");
   botdebug.Reset ();

   // ...out-of-range rows are dropped, plain strings land...
   botdebug.Set (DebugChannel::Eyes, -1, "oob");
   botdebug.Set (DebugChannel::Eyes, 99, "oob");
   botdebug.SetString (DebugChannel::Ears, -1, "oob");
   botdebug.SetString (DebugChannel::Ears, 99, "oob");
   botdebug.Set (DebugChannel::Body, 0, "Body %d", 1);
   botdebug.SetString (DebugChannel::Legs, 1, "legs");

   // ...a live bot fills every channel from default state...
   bots.Addbot ("Dbg", Difficulty::Normal, Personality::Normal, Team::CT, 1, true);
   HOST_REQUIRE (testhost::PumpBots (engine, 1));

   Bot *bot = nullptr;

   bots.ForEach ([&] (Bot *candidate) {
      bot = candidate;
      return true;
   });
   HOST_REQUIRE (bot != nullptr);
   bot->team_ = Team::CT;

   botdebug.Update (bot);
   botdebug.Render (bot->Ent (), "spectator");
   botdebug.Reset ();

   // ...bad overlay colors fall back to white instead of crashing...
   cv_debug_overlay_color.Set ("bogus");
   botdebug.Update (bot);
   botdebug.Render (bot->Ent (), "spectator");
   cv_debug_overlay_color.Revert ();

   CHECK (true);

   bots.Destroy ();
}

} // namespace bot
