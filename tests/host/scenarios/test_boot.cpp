//
// YaPB test host: boot smoke scenario.
//
// SPDX-License-Identifier: Unlicense
//
// Full standalone cycle against both mocks:
// fake engine -> GiveFnptrsToDll -> GetEntityAPI -> ServerActivate ->
// N x StartFrame -> ServerDeactivate. Asserts the bot library stays alive
// and the baseline gamedll forwarding is intact.
//

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("boot/full_cycle") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  // preload the staged fake gamedll globally so FakeCS_* resolve;
  // yapb loads the same file afterwards (refcounted, same image)
  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  // engine -> bot linkage (runs postload + cs binary load + hook install)
  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);
  CHECK (cs.HasCall ("GiveFnptrsToDll"));

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);
  CHECK (cs.HasCall ("GetEntityAPI"));

  newgamefuncs_t newtable {};
  int new_version = 0;
  HOST_REQUIRE (GetNewDLLFunctions (&newtable, &new_version) != 0);

  // level lifecycle
  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  CHECK (cs.HasCall ("ServerActivate"));

  for (int frame = 0; frame < 100; ++frame) {
    engine.AdvanceTime (0.01f);
    table.pfnStartFrame ();
  }
  CHECK (cs.HasCall ("StartFrame"));

  table.pfnServerDeactivate ();
  CHECK (cs.HasCall ("ServerDeactivate"));

  testhost::CloseFakeCs (cs);
}

} // namespace bot
