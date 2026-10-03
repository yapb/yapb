//
// YaPB test host: gamedll detection matrix.
//
// SPDX-License-Identifier: Unlicense
//
// Same boot against three fake gamedll variants (legacy / modern / regame).
// Observables: hook presence in gamefuncs_t (bot overwrites the baseline
// only where its conditions hold) and the version banner text with runtime
// flags printed through pfnServerPrint.
//

#include <cstring>

#include <ystl/ystl.h>
#include <linkage/goldsrc.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

// dllapi baseline is filled by yapb's GetEntityAPI (see goldsrc.h externs)
extern gamefuncs_t dllapi;

namespace bot {

namespace {

struct MatrixExpect {
  const char *version {};
  const char *present[4] {};
  int present_count {};
  const char *absent[4] {};
  int absent_count {};
  bool cmd_start_hook {};
  bool update_client_data_hook {};
};

void RunMatrixVariant (const char *game_dir_, const char *cs_dll, const MatrixExpect &expect) {
  testhost::FakeEngine engine;
  engine.Initialise (game_dir_);

  testhost::FakeCSApi cs {};
  HOST_REQUIRE (testhost::ResolveFakeCs (cs_dll, cs));
  cs.reset ();

  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);

  gamefuncs_t table {};
  HOST_REQUIRE (GetEntityAPI (&table, 140) != 0);

  // bot hooks overwrite the baseline only when their conditions hold:
  // pfnCmdStart needs a non-legacy game, pfnUpdateClientData needs fakepings
  CHECK ((table.pfnCmdStart != dllapi.pfnCmdStart) == expect.cmd_start_hook);
  CHECK ((table.pfnUpdateClientData != dllapi.pfnUpdateClientData) == expect.update_client_data_hook);

  // version banner goes through the print queue, flushed by StartFrame
  table.pfnGameInit ();
  CHECK (cs.HasCall ("GameInit"));

  table.pfnServerActivate (engine.EdictList (), engine.EdictCount (), 16);

  for (int frame = 0; frame < 20; ++frame) {
    engine.AdvanceTime (0.1f);
    table.pfnStartFrame ();
  }

  // sendServerMessage may chunk long prints: concatenate everything
  ystl::String banner {};
  for (size_t i = 0; i < engine.Calls ().size (); ++i) {
    const auto &call = engine.Calls ()[i];

    if (strcmp (call.name.chars (), "ServerPrint") == 0) {
      banner += call.detail;
      banner += '\n';
    }
  }
  CHECK (strstr (banner.chars (), expect.version) != nullptr);

  for (int i = 0; i < expect.present_count; ++i) {
    CHECK (strstr (banner.chars (), expect.present[i]) != nullptr);
  }
  for (int i = 0; i < expect.absent_count; ++i) {
    CHECK (strstr (banner.chars (), expect.absent[i]) == nullptr);
  }

  table.pfnServerDeactivate ();
  testhost::CloseFakeCs (cs);
}

} // namespace

TEST_CASE ("gamedll/matrix_legacy") {
  const MatrixExpect expect {
    "Legacy",
    { nullptr },
    0,
    { "BotVoice", "ReGameDLL", "FakePing" },
    3,
    false,
    false,
  };
  RunMatrixVariant (YAPB_TEST_GAMEDIR "/cstrike", YAPB_TEST_GAMEDIR "/cstrike/dlls/" YAPB_TEST_CSSUFFIX, expect);
}

TEST_CASE ("gamedll/matrix_modern [modern]") {
  const MatrixExpect expect {
    "v1.6",
    { "BotVoice", "FakePing" },
    2,
    { "ReGameDLL" },
    1,
    true,
    true,
  };
  RunMatrixVariant (YAPB_TEST_GAMEDIR "-modern/cstrike", YAPB_TEST_GAMEDIR "-modern/cstrike/dlls/" YAPB_TEST_CSSUFFIX, expect);
}

TEST_CASE ("gamedll/matrix_regame [regame]") {
  const MatrixExpect expect {
    "v1.6",
    { "BotVoice", "FakePing", "ReGameDLL" },
    3,
    { nullptr },
    0,
    true,
    true,
  };
  RunMatrixVariant (YAPB_TEST_GAMEDIR "-regame/cstrike", YAPB_TEST_GAMEDIR "-regame/cstrike/dlls/" YAPB_TEST_CSSUFFIX, expect);
}

} // namespace bot
