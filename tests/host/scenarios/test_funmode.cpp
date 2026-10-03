//
// YaPB test host: unit/funmode_modes.
//
// SPDX-License-Identifier: Unlicense
//
// Coverage for funmode.cpp through the public singleton only (no
// production-code changes, no hook needed): keyword tables, transitions
// with gravity save/restore, and every mode application.
//

#include <yapb.h>

#include <ystl/test.h>
#include "fake_engine.h"
#include "test_host.h"

namespace bot {

namespace {

void BootFun (testhost::FakeEngine &engine, testhost::FakeCSApi &cs) {
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

  for (int i = 0; i < 4; ++i) {
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

  HOST_REQUIRE (!graph.HasChanged ());
  HOST_REQUIRE (!analyzer.IsAnalyzing ());

  bots.InitQuota ();
  cv_quota.Set (10);
}

} // namespace

TEST_CASE ("unit/funmode_modes") {
  testhost::FakeEngine engine;
  testhost::FakeCSApi cs {};

  BootFun (engine, cs);

  // keyword table parses case-insensitively, strangers miss
  CHECK (FunMode::ParseMode ("imsober") == FunModeId::Off);
  CHECK (FunMode::ParseMode ("off") == FunModeId::Off);
  CHECK (FunMode::ParseMode ("TRONISBACK") == FunModeId::Tron);
  CHECK (FunMode::ParseMode ("ItsNewYear") == FunModeId::NewYear);
  CHECK (FunMode::ParseMode ("imhaunted") == FunModeId::Haunted);
  CHECK (FunMode::ParseMode ("itstoodark") == FunModeId::Dark);
  CHECK (FunMode::ParseMode ("stonedagain") == FunModeId::Stoned);
  CHECK (FunMode::ParseMode ("imonmars") == FunModeId::Mars);
  CHECK (FunMode::ParseMode ("nope") == FunModeId::Invalid);
  CHECK (FunMode::ParseMode ("") == FunModeId::Invalid);

  // keyword and message tables round-trip every mode
  CHECK (FunMode::KeywordOf (FunModeId::Tron) == "tronisback");
  CHECK (FunMode::KeywordOf (FunModeId::NewYear) == "itsnewyear");
  CHECK (FunMode::KeywordOf (FunModeId::Haunted) == "imhaunted");
  CHECK (FunMode::KeywordOf (FunModeId::Dark) == "itstoodark");
  CHECK (FunMode::KeywordOf (FunModeId::Stoned) == "stonedagain");
  CHECK (FunMode::KeywordOf (FunModeId::Mars) == "imonmars");
  CHECK (FunMode::KeywordOf (FunModeId::Off) == "imsober");
  CHECK (FunMode::KeywordOf (FunModeId::Invalid) == "imsober");

  CHECK (FunMode::MessageOf (FunModeId::Tron) == "from the eighties with Love");
  CHECK (FunMode::MessageOf (FunModeId::NewYear) == "Really ? That soon ?");
  CHECK (FunMode::MessageOf (FunModeId::Haunted) == "and the Ghosts are coming for you");
  CHECK (FunMode::MessageOf (FunModeId::Dark) == "the Bots will light your way");
  CHECK (FunMode::MessageOf (FunModeId::Stoned) == "feeling dizzy now ?");
  CHECK (FunMode::MessageOf (FunModeId::Mars) == "feel that Gravity...");
  CHECK (FunMode::MessageOf (FunModeId::Off) == "Back to Life, back to Reality");

  // setters write keywords, invalid never touches the cvar
  fun_mode.SetMode (FunModeId::Tron);
  CHECK (cv_fun_mode.As<ystl::StringRef> () == "tronisback");
  fun_mode.SetMode (FunModeId::Invalid);
  CHECK (cv_fun_mode.As<ystl::StringRef> () == "tronisback");
  fun_mode.SetMode (FunModeId::Off);

  // tron/stoned work off the client table, ghosts need real bots
  bots.Addbot ("FunT", Difficulty::Normal, Personality::Normal, Team::Terrorist, 1, true);
  HOST_REQUIRE (testhost::PumpBots (engine, 1));

  Bot *bot = nullptr;
  bots.ForEach ([&] (Bot *b) {
    bot = b;
    return true;
  });
  HOST_REQUIRE (bot != nullptr);

  edict_t *t = bot->Ent ();
  edict_t *c = game.CreateFakeClient ("FunC");
  HOST_REQUIRE (!game.IsNullEntity (c));

  t->v.health = 100.0f;
  c->v.health = 100.0f;
  t->v.flags |= FL_ONGROUND;
  c->v.flags |= FL_ONGROUND;
  clients.Update ();
  clients[t].team = Team::Terrorist;
  clients[c].team = Team::CT;

  // tron glows red terrorists and blue cts, skipping the glowing
  cv_fun_mode.Set ("tronisback");
  fun_mode.Update ();
  CHECK (t->v.renderfx == kRenderFxGlowShell);
  CHECK (t->v.rendercolor == ystl::Vector (255.0f, 0.0f, 0.0f));
  CHECK (c->v.rendercolor == ystl::Vector (0.0f, 0.0f, 255.0f));

  t->v.renderamt = 99.0f;
  fun_mode.Update ();
  CHECK (t->v.renderamt == 99.0f); // already glowing, untouched

  // leaving the mode restores every render state
  cv_fun_mode.Set ("off");
  fun_mode.Update ();
  CHECK (t->v.renderfx == kRenderFxNone);
  CHECK (t->v.rendermode == kRenderNormal);
  CHECK (t->v.renderamt == 0.0f);
  CHECK (!(t->v.effects & EF_BRIGHTLIGHT));

  // haunted ghosts and dark lights touch living bots only
  // (c is a raw client without a Bot, bots iterate Bot objects)
  t->v.health = 0.0f;
  t->v.deadflag = DEAD_DEAD;
  cv_fun_mode.Set ("imhaunted");
  fun_mode.Update ();
  CHECK (t->v.rendermode != kRenderTransTexture);
  t->v.health = 100.0f;
  t->v.deadflag = DEAD_NO;
  fun_mode.Update ();
  CHECK (t->v.rendermode == kRenderTransTexture);
  CHECK (t->v.renderamt == 100.0f);

  // dark touches living bots only
  t->v.health = 0.0f;
  t->v.deadflag = DEAD_DEAD;
  cv_fun_mode.Set ("itstoodark");
  fun_mode.Update ();
  CHECK (!(t->v.effects & EF_BRIGHTLIGHT));
  t->v.health = 100.0f;
  t->v.deadflag = DEAD_NO;

  cv_fun_mode.Set ("itstoodark");
  fun_mode.Update ();
  CHECK (!!(t->v.effects & EF_BRIGHTLIGHT));
  cv_fun_mode.Set ("off");
  fun_mode.Update ();
  CHECK (!(t->v.effects & EF_BRIGHTLIGHT));

  // new year sparks only living bots
  t->v.health = 0.0f;
  t->v.deadflag = DEAD_DEAD;
  cv_fun_mode.Set ("itsnewyear");
  fun_mode.Update ();
  t->v.health = 100.0f;
  t->v.deadflag = DEAD_NO;
  fun_mode.Update ();

  // stoned is silent without the screen-shake message...
  cv_fun_mode.Set ("stonedagain");
  fun_mode.Update ();

  // ...then shakes grounded clients, throttled by its own timer
  msgs.Add ("ScreenShake", 61);
  fun_mode.Update ();
  fun_mode.Update (); // too soon: shake timer still running

  // airborne clients are skipped by the shake
  t->v.flags &= ~FL_ONGROUND;
  c->v.flags &= ~FL_ONGROUND;
  engine.AdvanceTime (25.0f);
  fun_mode.Update ();
  t->v.flags |= FL_ONGROUND;
  c->v.flags |= FL_ONGROUND;

  cv_fun_mode.Set ("off");
  fun_mode.Update ();

  // mars lowers gravity and gives it back on exit
  const float base_gravity = sv_gravity.As<float> ();
  cv_fun_mode.Set ("imonmars");
  fun_mode.Update ();
  CHECK (sv_gravity.As<float> () == kFunMarsGravity);

  // out-of-band gravity edits are corrected while held
  sv_gravity.Set (500.0f);
  fun_mode.Update ();
  CHECK (sv_gravity.As<float> () == kFunMarsGravity);

  cv_fun_mode.Set ("off");
  fun_mode.Update ();
  CHECK (sv_gravity.As<float> () == base_gravity);

  // unknown strings park in invalid and run nothing
  t->v.renderfx = kRenderFxNone;
  cv_fun_mode.Set ("bogusmode");
  fun_mode.Update ();
  CHECK (t->v.renderfx == kRenderFxNone);

  bots.Destroy ();
}

} // namespace bot
