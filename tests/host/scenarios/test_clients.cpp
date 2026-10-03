//
// YaPB test host: unit/clients_track.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for clients.cpp: the client census (used/alive/
// bot/human flags, origin tracking) and the edict accessors.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

TEST_CASE ("unit/clients_track") {
  testhost::FakeEngine engine;
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");

  testhost::FakeCSApi cs {};
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

  // empty server tracks nobody (edict-level check, null slots have no index)...
  clients.Update ();

  bool any_player = false;

  for (int i = 0; i < game.MaxClients (); ++i) {
    any_player = any_player || !game.IsNullEntity (game.PlayerOfIndex (i));
  }
  CHECK (!any_player);

  // ...two fake clients show up as used, alive bots with cached origins...
  edict_t *first = game.CreateFakeClient ("CliA");
  edict_t *second = game.CreateFakeClient ("CliB");
  HOST_REQUIRE (!game.IsNullEntity (first) && !game.IsNullEntity (second));

  // fresh clients start zeroed (correct engine behavior): bring up liveness
  for (auto client : { first, second }) {
    client->v.health = 100.0f;
    client->v.deadflag = DEAD_NO;
    client->v.takedamage = DAMAGE_YES;
    client->v.solid = SOLID_BBOX;
    client->v.movetype = MOVETYPE_WALK;
  }

  const float first_pos[3] = { 100.0f, 0.0f, 0.0f };
  const float second_pos[3] = { 200.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (first, first_pos);
  engine.Funcs ().pfnSetOrigin (second, second_pos);

  clients.Update ();

  CHECK (clients[first].IsUsed ());
  CHECK (clients[first].IsUsedAndAlive ());
  CHECK (clients[first].IsBot ());
  CHECK (!clients[first].IsHuman ());
  CHECK (clients[first].origin == ystl::Vector (100.0f, 0.0f, 0.0f));
  CHECK (clients[first].IsUsedAnd (first));
  CHECK (clients[first].IsUsedAndNot (second));
  CHECK (!clients[first].IsUsedAndNot (first));

  // ...teams are sticky and drive the teammate test...
  clients[first].team = Team::Terrorist;
  clients[second].team = Team::Terrorist;
  CHECK (clients[first].IsTeammate (Team::Terrorist, second));
  CHECK (!clients[first].IsTeammate (Team::CT, second));
  CHECK (!clients[first].IsTeammate (Team::Terrorist, first)); // self never matches
  CHECK (clients[first].IsSameTeam (Team::Terrorist));
  CHECK (clients[first].IsEnemyTeam (Team::CT));

  // ...moved origins refresh on the next update...
  const float moved_pos[3] = { 150.0f, 0.0f, 0.0f };
  engine.Funcs ().pfnSetOrigin (first, moved_pos);
  clients.Update ();
  CHECK (clients[first].origin == ystl::Vector (150.0f, 0.0f, 0.0f));
  CHECK (clients[first].IsInRadius (ystl::Vector (150.0f, 0.0f, 0.0f), 1.0f));
  CHECK (clients[first].IsOutsideRadius (ystl::Vector (0.0f, 0.0f, 0.0f), 1.0f));

  // ...dead clients keep the slot but lose the alive flag...
  first->v.health = 0.0f;
  clients.Update ();
  CHECK (clients[first].IsUsed ());
  CHECK (!clients[first].IsUsedAndAlive ());
  CHECK (!clients[first].IsTeammate (Team::Terrorist, second));

  // ...dormant clients drop out entirely...
  first->v.flags |= FL_DORMANT;
  clients.Update ();
  CHECK (!clients[first].IsUsed ());
  CHECK (!clients[first].IsUsedAndAlive ());
  CHECK (clients[first].ent == nullptr);

  // ...const access reads the survivor...
  const ClientManager &frozen = clients;
  CHECK (frozen[second].IsUsedAndAlive ());
  CHECK (frozen[second].team == Team::Terrorist);
}

} // namespace bot
