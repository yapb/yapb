//
// YaPB test host: gamedll forwarding scenario.
//
// SPDX-License-Identifier: Unlicense
//
// Every bot hook must let the baseline gamedll call through, in order:
// Spawn, ClientConnect/PutInServer/Command/Disconnect, Touch, KeyValue.
//

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("forward/hooks") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  newgamefuncs_t newtable {};
  int new_version = 0;
  HOST_REQUIRE (GetNewDLLFunctions (&newtable, &new_version) != 0);

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);
  HOST_REQUIRE (cs.HasCall ("ServerActivate"));

  // entity spawn forwards (runs precache + spawn notify on the bot side)
  const float wall_origin[3] = { 100.0f, 0.0f, 0.0f };
  edict_t *wall = engine.SpawnEntity ("func_wall", wall_origin);
  HOST_REQUIRE (wall != nullptr);
  table.pfnSpawn (wall);
  CHECK (cs.HasCall ("Spawn"));

  // client lifecycle forwards
  edict_t *client = engine.SpawnClient ("Player1");
  HOST_REQUIRE (client != nullptr);

  char reject[128] = {};
  const int connected = table.pfnClientConnect (client, "Player1", "127.0.0.1", reject);
  CHECK (connected != 0);
  CHECK (cs.HasCall ("ClientConnect Player1"));

  table.pfnClientPutInServer (client);
  CHECK (cs.HasCall ("ClientPutInServer"));

  // plain chat command is not a bot command: captured and forwarded
  ystl::Array<ystl::String> args {};
  args.push ("say");
  args.push ("hello");
  engine.SetCmdArgs (args);
  table.pfnClientCommand (client);
  CHECK (cs.HasCall ("ClientCommand"));

  // touch forwards
  const float other_origin[3] = { 200.0f, 0.0f, 0.0f };
  edict_t *other = engine.SpawnEntity ("func_wall", other_origin);
  HOST_REQUIRE (other != nullptr);
  table.pfnTouch (wall, other);
  CHECK (cs.HasCall ("Touch"));

  // unbreakable glass keyvalue is filtered on the bot side, then forwarded
  const float breakable_origin[3] = { 300.0f, 0.0f, 0.0f };
  edict_t *breakable = engine.SpawnEntity ("func_breakable", breakable_origin);
  HOST_REQUIRE (breakable != nullptr);

  KeyValueData kvd {};
  kvd.szClassName = "func_breakable";
  kvd.szKeyName = "material";
  char material[8] = { '7', 0 };
  kvd.szValue = material;
  table.pfnKeyValue (breakable, &kvd);
  CHECK (cs.HasCall ("KeyValue"));

  // new-dll table carries the baseline through (bot overwrites it with its
  // own hook only on non-legacy games); a call must reach the gamedll
  HOST_REQUIRE (newtable.pfnOnFreeEntPrivateData != nullptr);
  newtable.pfnOnFreeEntPrivateData (breakable);
  CHECK (cs.HasCall ("OnFreeEntPrivateData"));

  // frames + disconnect + shutdown forward
  for (int frame = 0; frame < 10; ++frame) {
    engine.AdvanceTime (0.01f);
    table.pfnStartFrame ();
  }
  CHECK (cs.HasCall ("StartFrame"));

  table.pfnClientDisconnect (client);
  CHECK (cs.HasCall ("ClientDisconnect"));

  table.pfnServerDeactivate ();
  CHECK (cs.HasCall ("ServerDeactivate"));

  // baseline saw everything in engine order
  const int spawn = cs.CallIndex ("Spawn");
  const int connect = cs.CallIndex ("ClientConnect");
  const int put_in_server = cs.CallIndex ("ClientPutInServer");
  const int command = cs.CallIndex ("ClientCommand");
  const int touch = cs.CallIndex ("Touch");
  const int key_value = cs.CallIndex ("KeyValue");
  const int disconnect = cs.CallIndex ("ClientDisconnect");
  const int deactivate = cs.CallIndex ("ServerDeactivate");

  CHECK (spawn >= 0);
  CHECK (connect > spawn);
  CHECK (put_in_server > connect);
  CHECK (command > put_in_server);
  CHECK (touch > command);
  CHECK (key_value > touch);
  CHECK (disconnect > key_value);
  CHECK (deactivate > disconnect);

  testhost::CloseFakeCs (cs);
}

} // namespace bot
