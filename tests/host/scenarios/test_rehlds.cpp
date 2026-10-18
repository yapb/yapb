//
// YaPB test host: unit/rehlds_{dedicated,listen}.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for rehlds.cpp: probe/disconnect guards without a
// ReHLDS engine present (harness never provides CreateInterface).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootRehlds (testhost::FakeEngine &engine, testhost::FakeCSApi &cs, bool dedicated) {
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
   engine.SetDedicatedServer (dedicated);

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
}

} // namespace

TEST_CASE ("unit/rehlds_dedicated") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};

   BootRehlds (engine, cs, true);
   HOST_REQUIRE (game.IsDedicatedServer ());

   // ...bad indices and empty reasons never touch the api...
   CHECK (!rehlds.DropClient (0, "kick"));
   CHECK (!rehlds.DropClient (-1, "kick"));
   CHECK (!rehlds.DropClient (1, ""));
   CHECK (!rehlds.DropClient (1, ystl::StringRef ()));

   // ...without a rehlds engine behind us the probe stays down...
   CHECK (!rehlds.Probe ());
   CHECK (!rehlds.active ());

   // ...second probe hits the tried_ cache and stays down...
   CHECK (!rehlds.Probe ());

   // ...drops fail closed while the api is down...
   CHECK (!rehlds.DropClient (1, "kick"));
   CHECK (!rehlds.DropClient (33, "kick"));
}

TEST_CASE ("unit/rehlds_listen") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};

   BootRehlds (engine, cs, false);
   HOST_REQUIRE (!game.IsDedicatedServer ());

   // ...listen servers never carry rehlds (dedicated-only fast path)...
   CHECK (!rehlds.Probe ());
   CHECK (!rehlds.active ());
   CHECK (!rehlds.DropClient (1, "kick"));
}

} // namespace bot
