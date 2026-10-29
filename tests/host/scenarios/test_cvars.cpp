//
// YaPB test host: unit/cvars_{desc,registry,restart,bounds}.
//
// SPDX-License-Identifier: Unlicense
//
// Covers the engine-driven cvar registry: descriptors, registration views, idempotent registration and restart re-registration.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

ConVarReg *FindReg (ystl::StringRef name) {
  for (auto &var : cvars.Get ()) {
    if (var.name == name) {
      return &var;
    }
  }
  return nullptr;
}

void Boot (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
  engine.Initialise (YAPB_TEST_GAMEDIR "/cstrike");
  HOST_REQUIRE (testhost::ResolveFakeCs (YAPB_TEST_CSDLL, cs));
  cs.reset ();
  GiveFnptrsToDll (&engine.Funcs (), &engine.Globals ());
  HOST_REQUIRE (cs.entered () != 0);
}

} // namespace

TEST_CASE ("unit/cvars_desc_defaults") {
  // descriptor bounded values are explicit now
  ConVar info {
    { .name = "tmp_info_xyz", .value = "1", .info = "info", .bounded = true }
  };
  const auto info_spec = info.Spec ();

  CHECK (strcmp (info_spec.name.chars (), "yb_tmp_info_xyz") == 0);
  CHECK (strcmp (info_spec.value.chars (), "1") == 0);
  CHECK (strcmp (info_spec.info.chars (), "info") == 0);
  CHECK (info_spec.bounded);
  CHECK (info_spec.min == 0.0f);
  CHECK (info_spec.max == 1.0f);
  CHECK (info_spec.type == Var::NoServer);
  CHECK (!info_spec.reg_missing);

  // plain ctor is unbounded and keeps the description empty
  ConVar plain {
    { .name = "tmp_plain_xyz", .value = "2" }
  };
  const auto plain_spec = plain.Spec ();

  CHECK (!plain_spec.bounded);
  CHECK (plain_spec.info.empty ());
  CHECK (plain_spec.type == Var::NoServer);

  // game references keep their raw name and initial fallback
  ConVar ref {
    { .name = "mp_tmp_xyz", .value = nullptr, .type = Var::GameRef, .reg_missing = true, .reg_val = "7" }
  };
  const auto ref_spec = ref.Spec ();

  CHECK (strcmp (ref_spec.name.chars (), "mp_tmp_xyz") == 0);
  CHECK (ref_spec.type == Var::GameRef);
  CHECK (ref_spec.reg_missing);
  CHECK (strcmp (ref_spec.reg_val.chars (), "7") == 0);
}

TEST_CASE ("unit/cvars_registry") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};
  Boot (engine, cs);

  // every global cvar is described once, with a valid self pointer
  CHECK (cvars.Get ().size () >= 100);

  for (size_t i = 0; i < cvars.Get ().size (); ++i) {
    const auto &var = cvars.Get ()[i];
    CHECK (var.self != nullptr);

    for (size_t j = i + 1; j < cvars.Get ().size (); ++j) {
      CHECK (var.name != cvars.Get ()[j].name);
    }
  }

  // descriptor fields survive into the registration entry
  auto *debug = FindReg ("yb_debug");
  HOST_REQUIRE (debug != nullptr);
  CHECK (strcmp (debug->value.chars (), "0") == 0);
  CHECK (debug->bounded);
  CHECK (debug->min == 0.0f);
  CHECK (debug->max == 4.0f);
  CHECK (debug->type == Var::NoServer);
  CHECK (!debug->reg_missing);

  auto *buy = FindReg ("mp_buytime");
  HOST_REQUIRE (buy != nullptr);
  CHECK (buy->type == Var::GameRef);
  CHECK (buy->reg_missing);
  CHECK (strcmp (buy->reg_val.chars (), "1") == 0);

  // registration is idempotent, second pass must not append entries
  const size_t count = cvars.Get ().size ();
  cvars.Register ();
  cvars.Register (true);
  CHECK (cvars.Get ().size () == count);

  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/cvars_restart") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};
  Boot (engine, cs);

  cv_quota.Set (5);
  CHECK (cv_quota.As<int> () == 5);

  // simulate engine restart: Cvar_Shutdown drops every cvar and its strings
  engine.DropCvars ();

  // game cvars a real gamedll would register again
  engine.SetCvar ("mp_timelimit", "0");
  engine.SetCvar ("mp_buytime", "1.5");

  cvars.Register ();
  cvars.Register (true);

  // bot cvars are re-registered with their descriptor defaults
  CHECK (cv_quota.ptr != nullptr);
  CHECK (strcmp (cv_quota.Name ().chars (), "yb_quota") == 0);
  CHECK (cv_quota.As<int> () == 9);

  // game references are re-resolved by name against the new engine state
  CHECK (mp_timelimit.ptr != nullptr);
  CHECK (mp_timelimit.As<int> () == 0);
  CHECK (mp_buytime.ptr != nullptr);
  CHECK (mp_buytime.As<float> () == 1.5f);

  // no duplicate registry entries after the restart
  const size_t count = cvars.Get ().size ();
  cvars.Register ();
  cvars.Register (true);
  CHECK (cvars.Get ().size () == count);

  testhost::CloseFakeCs (cs);
}

TEST_CASE ("unit/cvars_bounds_desc") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};
  Boot (engine, cs);

  // descriptor bounds are enforced
  cv_difficulty.Set (10.0f);
  cvars.CheckBounds ();
  CHECK (cv_difficulty.As<int> () == 3);

  cv_debug.Set (3);
  cvars.CheckBounds ();
  CHECK (cv_debug.As<int> () == 3);

  // revert restores the registered default
  cv_debug.Revert ();
  CHECK (cv_debug.As<int> () == 0);

  // descriptions land in the registry entry
  cvars.SetDescription (cv_debug, "custom description");

  auto *debug = FindReg ("yb_debug");
  HOST_REQUIRE (debug != nullptr);
  CHECK (strcmp (debug->info.chars (), "custom description") == 0);

  testhost::CloseFakeCs (cs);
}

} // namespace bot
