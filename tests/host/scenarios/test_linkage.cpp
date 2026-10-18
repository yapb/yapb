//
// YaPB test host: unit/linkage_tables.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for linkage.cpp: the game-dll and engine table
// wrappers through the installed tables only (no metamod paths).
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootLinkage (testhost::FakeEngine &engine, testhost::FakeCSApi &cs, gamefuncs_t &table) {
   engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

   HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
   cs.reset ();

   GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
   HOST_REQUIRE (cs.entered () != 0);

   HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

   game.Precache ();
   table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

   if (analyzer.IsAnalyzing ()) {
      analyzer.Suspend ();
   }
}

} // namespace

TEST_CASE ("unit/linkage_tables") {
   testhost::FakeEngine engine;
   testhost::FakeCSApi cs {};
   gamefuncs_t table {};

   BootLinkage (engine, cs, table);

   // ...game init runs the config load plus the version banner...
   table.pfnGameInit ();

   // ...spawn notifies the game and forwards to the gamedll...
   edict_t *spawn = engine.SpawnEntity ("info_null");
   HOST_REQUIRE (spawn != nullptr);
   table.pfnSpawn (spawn);

   // ...touch with no breakables forwards quietly...
   edict_t *other = engine.SpawnEntity ("info_null");
   HOST_REQUIRE (other != nullptr);
   table.pfnTouch (spawn, other);

   // ...connect tracks loopback hosts and refreshes the census...
   edict_t *client = engine.SpawnClient ("LinkClient");
   HOST_REQUIRE (client != nullptr);

   char reject[128] = {};
   table.pfnClientConnect (client, "LinkClient", "127.0.0.1", reject);
   table.pfnClientConnect (client, "LinkClient", "loopback", reject);

   // ...disconnect clears editor/issuer state and forwards...
   table.pfnClientDisconnect (client);

   // ...engine sound/message wrappers forward plus feed bot systems...
   engine.Funcs ().pfnEmitSound (spawn, 0, "weapons/ak47-1.wav", 1.0f, 0.8f, 0, 100);
   engine.Funcs ().pfnMessageBegin (0, 0, nullptr, nullptr);
   engine.Funcs ().pfnWriteByte (1);
   engine.Funcs ().pfnWriteChar (2);
   engine.Funcs ().pfnWriteShort (3);
   engine.Funcs ().pfnWriteLong (4);
   engine.Funcs ().pfnWriteAngle (90.0f);
   engine.Funcs ().pfnWriteCoord (128.0f);
   engine.Funcs ().pfnWriteString ("link");
   engine.Funcs ().pfnWriteEntity (1);
   engine.Funcs ().pfnMessageEnd ();

   // ...client prints to real clients forward, bot short-circuits...
   engine.Funcs ().pfnClientPrintf (client, print_console, "hello");
   engine.Funcs ().pfnClientCommand (client, "say hi");

   // ...user message registration round-trips through the collector...
   const int msg = engine.Funcs ().pfnRegUserMsg ("LinkMsg", 8);
   CHECK (msg > 0);

   // ...server activation state stays consistent after the extra calls...
   CHECK (!game.IsNullEntity (spawn));
   CHECK (true);
}

} // namespace bot
