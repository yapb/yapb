//
// YaPB test host: unit/hooks_query.
//
// SPDX-License-Identifier: Unlicense
//
// Per-file coverage for hooks.cpp: the QueryBuffer byte mechanics and
// the server-query rewrite paths (players/info/rules/passthrough),
// plus the safe init/bypass legs. Sends go to a dead socket, so only
// parsing is exercised, never the network.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootHooks (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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
}

// raw player-list query: header, count, number, name, score, time
ystl::SmallArray<uint8_t> PlayerPacket (ystl::StringRef name, float time) {
  ystl::SmallArray<uint8_t> packet { 0xff, 0xff, 0xff, 0xff, 'D', 1, 7 };

  for (size_t i = 0; i < name.size (); ++i) {
    packet.push (static_cast<uint8_t> (name[i]));
  }
  packet.push (0);

  const int32_t score = 5;
  const auto score_bytes = reinterpret_cast<const uint8_t *> (&score);

  for (size_t i = 0; i < sizeof (score); ++i) {
    packet.push (score_bytes[i]);
  }

  const auto time_bytes = reinterpret_cast<const uint8_t *> (&time);

  for (size_t i = 0; i < sizeof (time); ++i) {
    packet.push (time_bytes[i]);
  }
  return packet;
}

// raw info query: header, protocol, four strings, app id, players/max/bots
ystl::SmallArray<uint8_t> InfoPacket () {
  ystl::SmallArray<uint8_t> packet { 0xff, 0xff, 0xff, 0xff, 'I', 48 };

  for (const auto *text : { "srv", "mod", "map", "game" }) {
    for (const char *ch = text; *ch != '\0'; ++ch) {
      packet.push (static_cast<uint8_t> (*ch));
    }
    packet.push (0);
  }
  packet.push (10);
  packet.push (0);
  packet.push (3);
  packet.push (16);
  packet.push (2);
  return packet;
}

int32_t SendQuery (const ystl::SmallArray<uint8_t> &packet) {
  return ServerQueryHook::SendTo (-1, packet.data (), packet.size (), 0, nullptr, 0);
}

} // namespace

TEST_CASE ("unit/hooks_query") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootHooks (engine, cs);

  // buffer primitives: reads advance, writes land on the last read...
  {
    const uint8_t raw[] = { 10, 20, 30 };
    QueryBuffer buffer { raw, sizeof (raw), 0 };

    CHECK (buffer.Read<uint8_t> () == 10);
    buffer.Write<uint8_t> (11); // lands on the just-read slot

    auto [data, length] = buffer.Data ();
    CHECK (length == 3);
    CHECK (data[0] == 11);
    CHECK (data[1] == 20);
    CHECK (data[2] == 30);
  }

  // ...overreads return zero, overskips hold still...
  {
    const uint8_t raw[] = { 7 };
    QueryBuffer buffer { raw, sizeof (raw), 0 };

    CHECK (buffer.Read<uint8_t> () == 7);
    CHECK (buffer.Read<uint8_t> () == 0);
    CHECK (buffer.Read<int32_t> () == 0);
    buffer.Skip<int32_t> ();
    CHECK (buffer.Read<uint8_t> () == 0);
  }

  // ...strings stop at nulls, skips hop over them...
  {
    const uint8_t raw[] = { 'a', 'b', 0, 'c', 0 };
    QueryBuffer buffer { raw, sizeof (raw), 0 };

    CHECK (buffer.ReadString () == "ab");
    CHECK (buffer.Read<uint8_t> () == 'c');
  }

  {
    const uint8_t raw[] = { 'a', 0, 'b', 0 };
    QueryBuffer buffer { raw, sizeof (raw), 0 };

    buffer.SkipString ();
    CHECK (buffer.ReadString () == "b");
    CHECK (buffer.ReadString () == "");
  }

  // ...shifting to the end turns the next write into a tail patch...
  {
    const uint8_t raw[] = { 1, 2, 9 };
    QueryBuffer buffer { raw, sizeof (raw), 0 };

    buffer.ShiftToEnd ();
    buffer.Write<uint8_t> (0);

    auto [data, length] = buffer.Data ();
    CHECK (length == 3);
    CHECK (data[2] == 0);
  }

  // ...ctor shift skips the header...
  {
    const uint8_t raw[] = { 0xff, 0xff, 'D', 5 };
    QueryBuffer buffer { raw, sizeof (raw), 3 };

    CHECK (buffer.Read<uint8_t> () == 5);
  }

  // ...player queries rewrite times (no bots: echo), info zeroes bots,
  // rules patch the tail, shorts and strangers pass through...
  CHECK (SendQuery (PlayerPacket ("Nobody", 1.5f)) == -1);
  CHECK (SendQuery (InfoPacket ()) == -1);

  const ystl::SmallArray<uint8_t> rules { 0xff, 0xff, 0xff, 0xff, 'm', 1, 2, 3 };
  CHECK (SendQuery (rules) == -1);

  const ystl::SmallArray<uint8_t> shorty { 0xff, 0xff };
  CHECK (SendQuery (shorty) == -1);

  const ystl::SmallArray<uint8_t> strange { 0xff, 0xff, 0xff, 0xff, 'X', 1, 2 };
  CHECK (SendQuery (strange) == -1);

  const ystl::SmallArray<uint8_t> empty {};
  CHECK (SendQuery (empty) == -1);

  // ...init stays out with the hook disabled, static link needs no bypass...
  cv_enable_query_hook.Set (0);
  fakequeries.Init ();
  CHECK (!entlink.NeedsBypass ());
  entlink.Initialize ();

  // ...unresolvable player factories report failure instead of calling...
  edict_t *nobody = game.CreateFakeClient ("HookNobody");
  HOST_REQUIRE (!game.IsNullEntity (nobody));
  CHECK (!entlink.CallPlayerFunction (nobody));
}

} // namespace bot
